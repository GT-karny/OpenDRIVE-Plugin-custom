// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OsiTrafficLightTypes.h"
#include "BPI_TrafficLightUpdate.h"
#include "BPI_TrafficLightHandlerUpdate.h"
#include "TrafficLightHandlerBase.generated.h"

/**
 * Entry mapping an integer ID to a traffic light actor implementing BPI_TrafficLightUpdate.
 */
USTRUCT(BlueprintType)
struct FManagedTrafficLight
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSI Traffic Light|Handler")
	int32 TrafficLightId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSI Traffic Light|Handler")
	AActor* TrafficLightActor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSI Traffic Light|Handler")
	FOsiTrafficLightState CurrentState;
};

/**
 * Base handler actor for managing OSI traffic light actors.
 *
 * Holds references to actors implementing BPI_TrafficLightUpdate, indexed by integer ID.
 * Implements BPI_TrafficLightHandlerUpdate so external systems can push OSI state updates,
 * which the handler propagates to the appropriate managed actors.
 *
 * Usage:
 * 1. Place this actor (or a Blueprint subclass) in the level
 * 2. Populate ManagedTrafficLights in the editor or via RegisterTrafficLight()
 * 3. External systems call UpdateTrafficLightById() / UpdateTrafficLightsBatch()
 *    through the BPI_TrafficLightHandlerUpdate interface
 */
UCLASS(Blueprintable)
class OPENDRIVE_API ATrafficLightHandlerBase : public AActor,
	public IBPI_TrafficLightHandlerUpdate
{
	GENERATED_BODY()

public:
	ATrafficLightHandlerBase();

	/** Array of managed traffic lights with their IDs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSI Traffic Light|Handler")
	TArray<FManagedTrafficLight> ManagedTrafficLights;

	/**
	 * Register a traffic light actor with a given ID.
	 * @param TrafficLightId Unique integer identifier
	 * @param Actor Actor implementing BPI_TrafficLightUpdate
	 * @return true if registered successfully, false if ID already exists or Actor is invalid
	 */
	UFUNCTION(BlueprintCallable, Category = "OSI Traffic Light|Handler")
	bool RegisterTrafficLight(int32 TrafficLightId, AActor* Actor);

	/**
	 * Unregister a traffic light by its ID.
	 * @param TrafficLightId The ID to remove
	 * @return true if found and removed
	 */
	UFUNCTION(BlueprintCallable, Category = "OSI Traffic Light|Handler")
	bool UnregisterTrafficLight(int32 TrafficLightId);

	/**
	 * Get the current cached state of a traffic light by ID.
	 * @param TrafficLightId The ID to query
	 * @param OutState The current state (only valid if return is true)
	 * @return true if found
	 */
	UFUNCTION(BlueprintCallable, Category = "OSI Traffic Light|Handler")
	bool GetTrafficLightState(int32 TrafficLightId, FOsiTrafficLightState& OutState) const;

	// BPI_TrafficLightHandlerUpdate implementation
	virtual void UpdateTrafficLightById_Implementation(int32 TrafficLightId, const FOsiTrafficLightState& NewState) override;
	virtual void UpdateTrafficLightsBatch_Implementation(const TArray<FOsiTrafficLightBatchEntry>& Updates) override;

protected:
	virtual void BeginPlay() override;

	/** Propagate a state to a single managed entry via BPI_TrafficLightUpdate. */
	void PropagateStateToActor(FManagedTrafficLight& Entry, const FOsiTrafficLightState& NewState);

	/** Build the ID-to-index lookup map from ManagedTrafficLights array. */
	void RebuildLookupMap();

private:
	/** Fast lookup: TrafficLightId -> index into ManagedTrafficLights array */
	TMap<int32, int32> IdToIndexMap;
};
