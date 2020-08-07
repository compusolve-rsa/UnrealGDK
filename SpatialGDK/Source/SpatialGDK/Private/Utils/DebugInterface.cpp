#include "Utils/DebugInterface.h"

#include "EngineClasses/SpatialNetDriver.h"
#include "EngineClasses/SpatialNetDriverDebugContext.h"
#include "EngineClasses/SpatialWorldSettings.h"

#include "LoadBalancing/DebugLBStrategy.h"
#include "LoadBalancing/LayeredLBStrategy.h"

#include "Utils/LayerInfo.h"
#include "Utils/SpatialActorUtils.h"

#include "EngineUtils.h"

namespace
{
	bool CheckDebuggingEnabled(const UWorld* World)
	{
		if (ensureMsgf(World != nullptr, TEXT("World is null")))
		{
			USpatialNetDriver* NetDriver = Cast<USpatialNetDriver>(World->GetNetDriver());
			if (ensureMsgf(NetDriver, TEXT("Using USpatialGDKDebugInterface while the NetDriver is not Spatial")))
			{
				return ensureMsgf(NetDriver->DebugCtx != nullptr, TEXT("Using USpatialGDKDebugInterface while debugging is not enabled"));
			}
		}
		return false;
	}

	UWorld* CheckDebuggingEnabled(UObject* WorldContextObject)
	{
		checkf(WorldContextObject, TEXT("World context object is null"));
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::Assert);
		if (!CheckDebuggingEnabled(World))
		{
			return nullptr;
		}
		return World;
	}
}

void USpatialGDKDebugInterface::EnableDebugSpatialGDK(UWorld* World)
{
	check(World);
	USpatialNetDriver* NetDriver = Cast<USpatialNetDriver>(World->GetNetDriver());
	check(NetDriver);
	
	if (NetDriver->DebugCtx == nullptr)
	{
		if (!ensureMsgf(NetDriver->LoadBalanceStrategy, TEXT("Enabling SpatialGDKDebug too soon")))
		{
			return ;
		}
		NetDriver->DebugCtx = NewObject<USpatialNetDriverDebugContext>();
		NetDriver->DebugCtx->Init(NetDriver);
	}
}

void USpatialGDKDebugInterface::DisableDebugSpatialGDK(UWorld* World)
{
	if (!CheckDebuggingEnabled(World))
	{
		return;
	}

	USpatialNetDriver* NetDriver = Cast<USpatialNetDriver>(World->GetNetDriver());
	
	if (NetDriver->DebugCtx != nullptr)
	{
		NetDriver->DebugCtx->Cleanup();
	}
}

ULayeredLBStrategy* USpatialGDKDebugInterface::GetLoadBalancingStrategy(UWorld* World)
{
	if (World == nullptr || !CheckDebuggingEnabled(World))
	{
		return nullptr;
	}

	USpatialNetDriver* NetDriver = Cast<USpatialNetDriver>(World->GetNetDriver());
	return Cast<ULayeredLBStrategy>(NetDriver->DebugCtx->DebugStrategy->GetWrappedStrategy());
}

bool USpatialGDKDebugInterface::IsActorReady(AActor* Actor)
{
	if (Actor == nullptr || !CheckDebuggingEnabled(Actor))
	{
		return false;
	}

	USpatialNetDriver* NetDriver = Cast<USpatialNetDriver>(Actor->GetWorld()->GetNetDriver());

	return NetDriver->DebugCtx->IsActorReady(Actor);
}

void USpatialGDKDebugInterface::AddTag(AActor* Actor, FName Tag)
{
	if (Actor == nullptr || !CheckDebuggingEnabled(Actor))
	{
		return;
	}

	USpatialNetDriver* NetDriver = Cast<USpatialNetDriver>(Actor->GetWorld()->GetNetDriver());

	NetDriver->DebugCtx->AddActorTag(Actor, Tag);
}

void USpatialGDKDebugInterface::AddInterestOnTag(UObject* WorldContextObject, FName Tag)
{
	UWorld* World = CheckDebuggingEnabled(WorldContextObject);
	if (World == nullptr)
	{
		return;
	}

	USpatialNetDriver* NetDriver = Cast<USpatialNetDriver>(World->GetNetDriver());

	NetDriver->DebugCtx->AddInterestOnTag(Tag);
}

void USpatialGDKDebugInterface::RemoveInterestOnTag(UObject* WorldContextObject, FName Tag)
{
	UWorld* World = CheckDebuggingEnabled(WorldContextObject);
	if (World == nullptr)
	{
		return;
	}

	USpatialNetDriver* NetDriver = Cast<USpatialNetDriver>(World->GetNetDriver());

	//NetDriver->DebugCtx->AddActorTag(Actor, Tag);
}

void USpatialGDKDebugInterface::KeepActorOnCurrentWorker(AActor* Actor)
{
	if (!CheckDebuggingEnabled(Actor))
	{
		return;
	}

	USpatialNetDriver* NetDriver = Cast<USpatialNetDriver>(Actor->GetWorld()->GetNetDriver());
	NetDriver->DebugCtx->KeepActorOnLocalWorker(Actor);
}

void USpatialGDKDebugInterface::DelegateTagToWorker(UObject* WorldContextObject, FName Tag, int32 WorkerId)
{
	UWorld* World = CheckDebuggingEnabled(WorldContextObject);
	if (World == nullptr)
	{
		return;
	}

	USpatialNetDriver* NetDriver = Cast<USpatialNetDriver>(World->GetNetDriver());
	NetDriver->DebugCtx->DelegateTagToWorker(Tag, WorkerId);
}
