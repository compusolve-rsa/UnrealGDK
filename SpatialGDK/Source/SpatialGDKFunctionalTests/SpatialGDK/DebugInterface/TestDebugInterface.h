// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "SpatialFunctionalTest.h"
#include "SpatialCommonTypes.h"
#include "TestDebugInterface.generated.h"

UCLASS()
class SPATIALGDKFUNCTIONALTESTS_API ATestDebugInterface : public ASpatialFunctionalTest
{
	GENERATED_BODY()
public:
	ATestDebugInterface();

	virtual void BeginPlay() override;

protected:

	TArray<VirtualWorkerId> Workers;
	VirtualWorkerId LocalWorker;
	FVector WorkerEntityPosition;
	bool bIsOnDefaultLayer = false;
	int32 DelegationStep = 0;
};
