// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "TestDebugInterface.h"
#include "SpatialFunctionalTestFlowController.h"
#include "Utils/DebugInterface.h"

#include "LoadBalancing/LayeredLBStrategy.h"
#include "LoadBalancing/GridBasedLBStrategy.h"

#include "SpatialGDKFunctionalTests/SpatialGDK/TestActors/ReplicatedTestActorBase.h"
#include "Kismet/GameplayStatics.h"

ATestDebugInterface::ATestDebugInterface()
	: Super()
{
	Author = "Nicolas";
	Description = TEXT("Test Debug interface");
}

namespace
{
	FName GetTestTag()
	{
		static const FName TestTag(TEXT("TestActorToFollow"));
		return TestTag;
	}
}

void ATestDebugInterface::BeginPlay()
{
	Super::BeginPlay();

	AddStep(TEXT("SetupStep"), FWorkerDefinition::AllServers, nullptr, nullptr, [](ASpatialFunctionalTest* NetTest, float DeltaTime)
	{
		ATestDebugInterface* Test = Cast<ATestDebugInterface>(NetTest);
		UWorld* World = Test->GetWorld();
		Test->DelegationStep = 0;
		Test->Workers.Empty();
		Test->bIsOnDefaultLayer = false;

		USpatialGDKDebugInterface::EnableDebugSpatialGDK(World);

		ULayeredLBStrategy* RootStrategy = USpatialGDKDebugInterface::GetLoadBalancingStrategy(World);

		Test->bIsOnDefaultLayer = RootStrategy->CouldHaveAuthority(AReplicatedTestActorBase::StaticClass());
		if (Test->bIsOnDefaultLayer)
		{
			FName LocalLayer = RootStrategy->GetLocalLayerName();
			UAbstractLBStrategy* LocalStrategy = RootStrategy->GetLBStrategyForLayer(LocalLayer);

			Test->AssertTrue(LocalStrategy->IsA<UGridBasedLBStrategy>(), TEXT(""));

			UGridBasedLBStrategy* GridStrategy = Cast<UGridBasedLBStrategy>(LocalStrategy);

			for (auto& WorkerRegion : GridStrategy->GetLBStrategyRegions())
			{
				Test->Workers.Add(WorkerRegion.Key);
			}
			Test->LocalWorker = GridStrategy->GetLocalVirtualWorkerId();

			Test->WorkerEntityPosition = GridStrategy->GetWorkerEntityPosition();
			AReplicatedTestActorBase* Actor = World->SpawnActor<AReplicatedTestActorBase>(Test->WorkerEntityPosition, FRotator());
			USpatialGDKDebugInterface::AddTag(Actor, GetTestTag());
			Test->RegisterAutoDestroyActor(Actor);
		}

		Test->FinishStep();
	});

	AddStep(TEXT("Wait for actor ready and add extra interest"), FWorkerDefinition::AllServers,
		[](ASpatialFunctionalTest* NetTest) -> bool
		{
			ATestDebugInterface* Test = Cast<ATestDebugInterface>(NetTest);
			if (!Test->bIsOnDefaultLayer)
			{
				return true;
			}
			UWorld* World = Test->GetWorld();

			TArray<AActor*> TestActors;
			UGameplayStatics::GetAllActorsOfClass(World, AReplicatedTestActorBase::StaticClass(), TestActors);
			if (!Test->AssertTrue(TestActors.Num() == 1, "We should only see a single actor at this point!!"))
			{
				return false;
			}
			return USpatialGDKDebugInterface::IsActorReady(TestActors[0]);
		},
		[](ASpatialFunctionalTest* NetTest)
		{
			ATestDebugInterface* Test = Cast<ATestDebugInterface>(NetTest);
			if (Test->bIsOnDefaultLayer)
			{
				USpatialGDKDebugInterface::AddInterestOnTag(Test, GetTestTag());
			}

			Test->FinishStep();
		}, nullptr, 5.0f);

	AddStep(TEXT("Wait for extra actors"), FWorkerDefinition::AllServers,
		[](ASpatialFunctionalTest* NetTest) -> bool
		{
			ATestDebugInterface* Test = Cast<ATestDebugInterface>(NetTest);
			if (Test->bIsOnDefaultLayer)
			{
				UWorld* World = Test->GetWorld();

				TArray<AActor*> TestActors;
				UGameplayStatics::GetAllActorsOfClass(World, AReplicatedTestActorBase::StaticClass(), TestActors);
				if (TestActors.Num() == 1)
				{
					return false;
				}
			}
			return true;
		},
		[](ASpatialFunctionalTest* NetTest)
		{
			ATestDebugInterface* Test = Cast<ATestDebugInterface>(NetTest);
			if (Test->bIsOnDefaultLayer)
			{
				UWorld* World = Test->GetWorld();

				TArray<AActor*> TestActors;
				UGameplayStatics::GetAllActorsOfClass(World, AReplicatedTestActorBase::StaticClass(), TestActors);

				Test->AssertTrue(TestActors.Num() == Test->Workers.Num(), TEXT("Not the expected number of actors"));
			}

			Test->FinishStep();
		}, nullptr, 5.0f);

	AddStep(TEXT("Force actor delegation"), FWorkerDefinition::AllServers, nullptr, nullptr,
		[](ASpatialFunctionalTest* NetTest, float DeltaTime)
		{
			ATestDebugInterface* Test = Cast<ATestDebugInterface>(NetTest);
			if (Test->bIsOnDefaultLayer)
			{
				int32 CurWorker = Test->DelegationStep / 2;
				int32 WorkerSubStep = Test->DelegationStep % 2;
				switch (WorkerSubStep)
				{
				case 0:
					USpatialGDKDebugInterface::DelegateTagToWorker(Test, GetTestTag(), Test->Workers[CurWorker]);
					++Test->DelegationStep;
					break;
				case 1:
					bool bExpectedAuth = Test->Workers[CurWorker] == Test->LocalWorker;
					bool bConsistentResult = true;

					TArray<AActor*> TestActors;
					UGameplayStatics::GetAllActorsOfClass(Test->GetWorld(), AReplicatedTestActorBase::StaticClass(), TestActors);
					for (AActor* Actor : TestActors)
					{
						bConsistentResult &= Actor->HasAuthority() == bExpectedAuth;
					}

					if (bConsistentResult)
					{
						++Test->DelegationStep;
					}
					break;
				}
			}
			if (Test->DelegationStep >= Test->Workers.Num() * 2)
			{
				UWorld* World = Test->GetWorld();
				
				AReplicatedTestActorBase* Actor = World->SpawnActor<AReplicatedTestActorBase>(Test->WorkerEntityPosition, FRotator());
				USpatialGDKDebugInterface::AddTag(Actor, GetTestTag());
				Test->RegisterAutoDestroyActor(Actor);

				Test->FinishStep();
			}
		}, 10.0f);

	AddStep(TEXT("Check new actors interest and delegation"), FWorkerDefinition::AllServers,
		[](ASpatialFunctionalTest* NetTest) -> bool
		{
			ATestDebugInterface* Test = Cast<ATestDebugInterface>(NetTest);
			if (Test->bIsOnDefaultLayer)
			{
				UWorld* World = Test->GetWorld();

				TArray<AActor*> TestActors;
				UGameplayStatics::GetAllActorsOfClass(World, AReplicatedTestActorBase::StaticClass(), TestActors);
				if (TestActors.Num() != Test->Workers.Num() * 2)
				{
					return false;
				}
			}
			return true;
		}, nullptr,
		[](ASpatialFunctionalTest* NetTest, float DeltaTime)
	{
		ATestDebugInterface* Test = Cast<ATestDebugInterface>(NetTest);
		if (Test->bIsOnDefaultLayer)
		{
			int32_t CurWorker = Test->Workers.Num() - 1;

			bool bExpectedAuth = Test->Workers[CurWorker] == Test->LocalWorker;
			bool bConsistentResult = true;

			TArray<AActor*> TestActors;
			UGameplayStatics::GetAllActorsOfClass(Test->GetWorld(), AReplicatedTestActorBase::StaticClass(), TestActors);
			for (AActor* Actor : TestActors)
			{
				bConsistentResult &= Actor->HasAuthority() == bExpectedAuth;
			}

			if (bConsistentResult)
			{
				//USpatialGDKDebugInterface::DisableDebugSpatialGDK(Test->GetWorld());
				Test->FinishStep();
			}
		}
	}, 10.0f);

	AddStep(TEXT("Shutdown debugging"), FWorkerDefinition::AllServers, nullptr,
		[](ASpatialFunctionalTest* NetTest)
	{
		ATestDebugInterface* Test = Cast<ATestDebugInterface>(NetTest);
		if (Test->bIsOnDefaultLayer)
		{
			USpatialGDKDebugInterface::DisableDebugSpatialGDK(Test->GetWorld());
			Test->FinishStep();
		}
	}, nullptr);

	AddStep(TEXT("Check state after debug shutdow"), FWorkerDefinition::AllServers, [](ASpatialFunctionalTest* NetTest) -> bool
	{
		ATestDebugInterface* Test = Cast<ATestDebugInterface>(NetTest);
		if (Test->bIsOnDefaultLayer)
		{
			UWorld* World = Test->GetWorld();

			TArray<AActor*> TestActors;
			UGameplayStatics::GetAllActorsOfClass(World, AReplicatedTestActorBase::StaticClass(), TestActors);

			for (AActor* Actor : TestActors)
			{
				if (Actor->IsPendingKill() || Actor->IsActorBeingDestroyed() || Actor->IsPendingKillOrUnreachable() || Actor->IsPendingKillPending())
				{
					check(false);
				}
			}

			if (TestActors.Num() != 2)
			{
				return false;
			}
		}
		return true;
	}, nullptr,
		[](ASpatialFunctionalTest* NetTest, float DeltaTime)
	{
		ATestDebugInterface* Test = Cast<ATestDebugInterface>(NetTest);
		if (Test->bIsOnDefaultLayer)
		{
			bool bConsistentResult = true;

			TArray<AActor*> TestActors;
			UGameplayStatics::GetAllActorsOfClass(Test->GetWorld(), AReplicatedTestActorBase::StaticClass(), TestActors);
			for (AActor* Actor : TestActors)
			{
				bConsistentResult &= Actor->HasAuthority();
			}

			if (bConsistentResult)
			{
				Test->FinishStep();
			}
		}
	}, 10.0f);
}
