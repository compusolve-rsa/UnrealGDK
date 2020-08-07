#pragma once

#include "SpatialCommonTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "DebugInterface.generated.h"

class UWorld;
class AActor;
class ULayeredLBStrategy;

UCLASS()
class SPATIALGDK_API USpatialGDKDebugInterface : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// Enables or disables the debug facilities on the provided world.
	static void EnableDebugSpatialGDK(UWorld* World);
	static void DisableDebugSpatialGDK(UWorld* World);

	static ULayeredLBStrategy* GetLoadBalancingStrategy(UWorld* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "SpatialGDK")
	static void AddTag(AActor* Actor, FName Tag);

	UFUNCTION(BlueprintCallable, Category = "SpatialGDK")
	static bool IsActorReady(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "SpatialGDK", meta = (WorldContext = "WorldContextObject"))
	static void AddInterestOnTag(UObject* WorldContextObject, FName Tag);

	UFUNCTION(BlueprintCallable, Category = "SpatialGDK", meta = (WorldContext = "WorldContextObject"))
	static void RemoveInterestOnTag(UObject* WorldContextObject, FName Tag);

	UFUNCTION(BlueprintCallable, Category = "SpatialGDK")
	static void KeepActorOnCurrentWorker(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "SpatialGDK", meta = (WorldContext = "WorldContextObject"))
	static void DelegateTagToWorker(UObject* WorldContextObject, FName Tag, int32 WorkerId);
};
