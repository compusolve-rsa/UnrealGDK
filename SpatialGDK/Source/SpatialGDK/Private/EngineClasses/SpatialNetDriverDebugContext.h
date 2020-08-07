// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "SpatialCommonTypes.h"
#include "Schema/DebugComponent.h"

#include "SpatialNetDriverDebugContext.generated.h"

class UDebugLBStrategy;
class USpatialNetDriver;

UCLASS()
class USpatialNetDriverDebugContext : public UObject
{
	GENERATED_BODY()
public:

	// Startup / Shutdown
	void Init(USpatialNetDriver* NetDriver);
	void Cleanup();

	// Debug Interface
	void AddActorTag(AActor* Actor, FName Tag);
	void AddInterestOnTag(FName Tag);
	void KeepActorOnLocalWorker(AActor* Actor);
	void DelegateTagToWorker(FName Tag, int32 WorkerId);
	TOptional<VirtualWorkerId> GetActorHierarchyExplicitDelegation(const AActor* Actor);

	// Utility
	bool IsActorReady(AActor* Actor);

	// NetDriver Integration
	void TickServer();
	void OnDebugComponentUpdateReceived(Worker_EntityId);
	void OnDebugComponentAuthLost(Worker_EntityId EntityId);

	bool NeedEntityInterestUpdate()
	{
		return bNeedToUpdateInterest;
	}

	void ClearNeedEntityInterestUpdate()
	{
		bNeedToUpdateInterest = false;
	}

	TArray<Worker_EntityId_Key> GetAdditionalEntityInterest()
	{
		return CachedInterestSet.Array();
	}

	UPROPERTY()
	UDebugLBStrategy* DebugStrategy = nullptr;

protected:

	struct DebugComponentView
	{
		SpatialGDK::DebugComponent Component;
		Worker_EntityId Entity = SpatialConstants::INVALID_ENTITY_ID;
		bool bAdded = false;
		bool bUpdated = false;
	};

	DebugComponentView& GetDebugComponentView(AActor* Actor);

	TOptional<VirtualWorkerId> GetActorExplicitDelegation(const AActor* Actor);
	TOptional<VirtualWorkerId> GetActorHierarchyExplicitDelegation_Traverse(const AActor* Actor);

	void AddEntityToWatch(Worker_EntityId);

	USpatialNetDriver* NetDriver;

	// Collection of tag delegations.
	TMap<FName, VirtualWorkerId> SemanticDelegations;

	// Collection of actor tags we should get interest over.
	TSet<FName> SemanticInterest;

	// Debug info for actors
	TMap<AActor*, DebugComponentView> ActorDebugInfo;

	// Contains a cache of entities computed from the explicit actor interest and semantic interest.
	TSet<Worker_EntityId_Key> CachedInterestSet;

	bool bNeedToUpdateInterest = false;
};
