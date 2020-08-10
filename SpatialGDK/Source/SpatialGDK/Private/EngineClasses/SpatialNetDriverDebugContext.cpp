#include "SpatialNetDriverDebugContext.h"

#include "EngineClasses/SpatialNetDriver.h"
#include "EngineClasses/SpatialPackageMapClient.h"
#include "Interop/Connection/SpatialWorkerConnection.h"
#include "Interop/SpatialSender.h"
#include "LoadBalancing/DebugLBStrategy.h"
#include "Utils/SpatialActorUtils.h"


void USpatialNetDriverDebugContext::Init(USpatialNetDriver* InNetDriver)
{
	NetDriver = InNetDriver;
	DebugStrategy = NewObject<UDebugLBStrategy>();
	DebugStrategy->InitDebugStrategy(this, NetDriver->LoadBalanceStrategy);
	NetDriver->LoadBalanceStrategy = DebugStrategy;

	NetDriver->Sender->UpdateServerWorkerEntityInterestAndPosition();
}

void USpatialNetDriverDebugContext::Cleanup()
{
	TArray<Worker_EntityId_Key> EntityIds;
	NetDriver->StaticComponentView->GetEntityIds(EntityIds);

	for (auto Entity : EntityIds)
	{
		if (NetDriver->StaticComponentView->HasAuthority(Entity, SpatialConstants::GDK_DEBUG_COMPONENT_ID))
		{
			NetDriver->Sender->SendRemoveComponents(Entity, { SpatialConstants::GDK_DEBUG_COMPONENT_ID });
		}
	}

	NetDriver->LoadBalanceStrategy = Cast<UDebugLBStrategy>(DebugStrategy)->GetWrappedStrategy();
	NetDriver->DebugCtx = nullptr;
	NetDriver->Sender->UpdateServerWorkerEntityInterestAndPosition();
}

USpatialNetDriverDebugContext::DebugComponentView& USpatialNetDriverDebugContext::GetDebugComponentView(AActor* Actor)
{
	check(Actor && Actor->HasAuthority());
	SpatialGDK::DebugComponent* DbgComp = nullptr;

	Worker_EntityId Entity = NetDriver->PackageMap->GetEntityIdFromObject(Actor);
	if (Entity != SpatialConstants::INVALID_ENTITY_ID)
	{
		DbgComp = NetDriver->StaticComponentView->GetComponentData<SpatialGDK::DebugComponent>(Entity);
	}

	DebugComponentView& Comp = ActorDebugInfo.FindOrAdd(Actor);
	if (DbgComp && Comp.Entity == SpatialConstants::INVALID_ENTITY_ID)
	{
		Comp.Component = *DbgComp;
		Comp.bAdded = true;
	}
	Comp.Entity = Entity;

	return Comp;
}

void USpatialNetDriverDebugContext::AddActorTag(AActor* Actor, FName Tag)
{
	if (Actor->HasAuthority())
	{
		DebugComponentView& Comp = GetDebugComponentView(Actor);
		Comp.Component.ActorTags.Add(Tag);
		if (SemanticInterest.Contains(Tag) && Comp.Entity != SpatialConstants::INVALID_ENTITY_ID)
		{
			AddEntityToWatch(Comp.Entity);
		}
		Comp.bUpdated = true;
	}
}

void USpatialNetDriverDebugContext::OnDebugComponentUpdateReceived(Worker_EntityId Entity)
{
	SpatialGDK::DebugComponent* DbgComp = NetDriver->StaticComponentView->GetComponentData<SpatialGDK::DebugComponent>(Entity);
	check(DbgComp);
	if (DbgComp->ActorTags.Intersect(SemanticInterest).Num() > 0)
	{
		AddEntityToWatch(Entity);
	}
}

void USpatialNetDriverDebugContext::OnDebugComponentAuthLost(Worker_EntityId EntityId)
{
	for (auto Iterator = ActorDebugInfo.CreateIterator(); Iterator; ++Iterator)
	{
		if (Iterator->Value.Entity == EntityId)
		{
			Iterator.RemoveCurrent();
			break;
		}
	}
}

void USpatialNetDriverDebugContext::AddEntityToWatch(Worker_EntityId Entity)
{
	bool bAlreadyWatchingEntity = false;
	CachedInterestSet.Add(Entity, &bAlreadyWatchingEntity);
	bNeedToUpdateInterest |= !bAlreadyWatchingEntity;
}

void USpatialNetDriverDebugContext::AddInterestOnTag(FName Tag)
{
	bool bAlreadyInSet = false;
	SemanticInterest.Add(Tag, &bAlreadyInSet);

	if (!bAlreadyInSet)
	{
		TArray<Worker_EntityId_Key> EntityIds;
		NetDriver->StaticComponentView->GetEntityIds(EntityIds);

		for (auto Entity : EntityIds)
		{
			if (SpatialGDK::DebugComponent* DbgComp = NetDriver->StaticComponentView->GetComponentData<SpatialGDK::DebugComponent>(Entity))
			{
				if (DbgComp->ActorTags.Contains(Tag))
				{
					AddEntityToWatch(Entity);
				}
			}
		}

		for (const auto& Item : ActorDebugInfo)
		{
			if (!Item.Value.bAdded || Item.Value.bUpdated)
			{
				if (Item.Value.Component.ActorTags.Contains(Tag))
				{
					Worker_EntityId Entity = NetDriver->PackageMap->GetEntityIdFromObject(Item.Key);
					if (Entity != SpatialConstants::INVALID_ENTITY_ID)
					{
						AddEntityToWatch(Entity);
					}
				}
			}
		}
	}
}

void USpatialNetDriverDebugContext::KeepActorOnLocalWorker(AActor* Actor)
{
	if (Actor->HasAuthority())
	{
		DebugComponentView& Comp = GetDebugComponentView(Actor);
		Comp.Component.DelegatedWorkerId = DebugStrategy->GetLocalVirtualWorkerId();
		Comp.bUpdated = true;
	}
}

void USpatialNetDriverDebugContext::DelegateTagToWorker(FName Tag, int32 WorkerId)
{
	if (WorkerId < 0)
	{
		SemanticDelegations.Remove(Tag);
	}
	else
	{
		SemanticDelegations.Add(Tag, WorkerId);
	}
}

TOptional<VirtualWorkerId> USpatialNetDriverDebugContext::GetActorHierarchyExplicitDelegation(const AActor* Actor)
{
	const AActor* NetOwner = SpatialGDK::GetHierarchyRoot(Actor);
	return GetActorHierarchyExplicitDelegation_Traverse(NetOwner);
}

TOptional<VirtualWorkerId> USpatialNetDriverDebugContext::GetActorHierarchyExplicitDelegation_Traverse(const AActor* Actor)
{
	TOptional<VirtualWorkerId> CandidateDelegation = GetActorExplicitDelegation(Actor);
	for (const AActor* Child : Actor->Children)
	{
		TOptional<VirtualWorkerId> ChildDelegation = GetActorHierarchyExplicitDelegation_Traverse(Child);
		ensureMsgf(!CandidateDelegation.IsSet() || !ChildDelegation.IsSet() || CandidateDelegation.GetValue() == ChildDelegation.GetValue(),
			TEXT("Inconsistent delegation. Actor %s is delegated to %i but a child is delegated to %i"),
			*Actor->GetName(), CandidateDelegation.GetValue(), ChildDelegation.GetValue());
		CandidateDelegation = ChildDelegation;
	}

	return CandidateDelegation;
}

TOptional<VirtualWorkerId> USpatialNetDriverDebugContext::GetActorExplicitDelegation(const AActor* Actor)
{
	SpatialGDK::DebugComponent* DbgComp = nullptr;
	if (DebugComponentView* DebugInfo = ActorDebugInfo.Find(Actor))
	{
		DbgComp = &DebugInfo->Component;
	}
	else
	{
		Worker_EntityId Entity = NetDriver->PackageMap->GetEntityIdFromObject(Actor);
		if (Entity != SpatialConstants::INVALID_ENTITY_ID)
		{
			DbgComp = NetDriver->StaticComponentView->GetComponentData<SpatialGDK::DebugComponent>(Entity);
		}
	}

	if (!DbgComp)
	{
		return {};
	}

	if (DbgComp->DelegatedWorkerId)
	{
		return *DbgComp->DelegatedWorkerId;
	}

	TOptional<VirtualWorkerId> CandidateDelegation;
	for (auto Tag : DbgComp->ActorTags)
	{
		if (VirtualWorkerId* Worker = SemanticDelegations.Find(Tag))
		{
			ensureMsgf(!CandidateDelegation.IsSet() || *CandidateDelegation == *Worker,
				TEXT("Inconsistent delegation. Actor %s delegated to both %i and %i"),
				*Actor->GetName(), *CandidateDelegation, *Worker);
			CandidateDelegation = *Worker;
		}
	}

	return CandidateDelegation;
}

void USpatialNetDriverDebugContext::TickServer()
{
	for (auto& Entry : ActorDebugInfo)
	{
		if (!Entry.Value.bAdded)
		{
			Worker_EntityId Entity = NetDriver->PackageMap->GetEntityIdFromObject(Entry.Key);
			if (Entity != SpatialConstants::INVALID_ENTITY_ID)
			{
				if (Entry.Value.Component.ActorTags.Intersect(SemanticInterest).Num() > 0)
				{
					AddEntityToWatch(Entity);
				}

				// There is a requirement of readiness before we can use SendAddComponent
				if (IsActorReady(Entry.Key))
				{
					Worker_ComponentData CompData = Entry.Value.Component.CreateDebugComponent();
					NetDriver->Sender->SendAddComponents(Entity, { CompData });
					Entry.Value.Entity = Entity;
					Entry.Value.bAdded = true;
				}
			}
		}
		else if (Entry.Value.bUpdated)
		{
			FWorkerComponentUpdate CompUpdate = Entry.Value.Component.CreateDebugComponentUpdate();
			NetDriver->Connection->SendComponentUpdate(Entry.Value.Entity, &CompUpdate);
			Entry.Value.bUpdated = false;
		}
	}

	if (NeedEntityInterestUpdate())
	{
		NetDriver->Sender->UpdateServerWorkerEntityInterestAndPosition();
	}
}

bool USpatialNetDriverDebugContext::IsActorReady(AActor* Actor)
{
	Worker_EntityId Entity = NetDriver->PackageMap->GetEntityIdFromObject(Actor);
	if (Entity != SpatialConstants::INVALID_ENTITY_ID)
	{
		return NetDriver->StaticComponentView->HasAuthority(Entity, SpatialConstants::POSITION_COMPONENT_ID);
	}
	return false;
}
