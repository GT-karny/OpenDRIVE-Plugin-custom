// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OsiTrafficLightTypes.h"
#include "BPI_TrafficLightUpdate.h"
#include "BPI_TrafficLightHandlerUpdate.h"
#include "BPI_TrafficLightRegister.h"
#include "TrafficLightHandlerBase.generated.h"

/**
 * Base handler actor for managing OSI traffic light actors.
 *
 * Holds a TMap of integer IDs to actors implementing BPI_TrafficLightUpdate.
 * Implements BPI_TrafficLightHandlerUpdate for receiving state updates from external systems,
 * and BPI_TrafficLightRegister for registering/unregistering traffic light actors.
 *
 * Usage:
 * 1. Place this actor (or a Blueprint subclass) in the level
 * 2. Populate ManagedTrafficLights in the editor or via RegisterTrafficLight()
 * 3. External systems call UpdateTrafficLightById() / UpdateTrafficLightsBatch()
 *    through the BPI_TrafficLightHandlerUpdate interface
 */
UCLASS(Blueprintable)
class OPENDRIVE_API ATrafficLightHandlerBase : public AActor,
	public IBPI_TrafficLightHandlerUpdate,
	public IBPI_TrafficLightRegister
{
	GENERATED_BODY()

public:
	ATrafficLightHandlerBase();

	/** ID -> BPI_TrafficLightUpdate implementing actor map */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSI Traffic Light|Handler")
	TMap<int32, AActor*> ManagedTrafficLights;

	/**
	 * Get the current cached state of a traffic light by ID.
	 * @param TrafficLightId The ID to query
	 * @param OutState The current state (only valid if return is true)
	 * @return true if found
	 */
	UFUNCTION(BlueprintCallable, Category = "OSI Traffic Light|Handler")
	bool GetTrafficLightState(int32 TrafficLightId, FOsiTrafficLightState& OutState) const;

	// --- BPI_TrafficLightRegister implementation ---
	virtual bool RegisterTrafficLight_Implementation(int32 TrafficLightId, AActor* TrafficLightActor) override;
	virtual bool UnregisterTrafficLight_Implementation(int32 TrafficLightId) override;

	// --- BPI_TrafficLightHandlerUpdate implementation ---
	virtual void UpdateTrafficLightById_Implementation(int32 TrafficLightId, const FOsiTrafficLightState& NewState) override;
	virtual void UpdateTrafficLightsBatch_Implementation(const TArray<FOsiTrafficLightBatchEntry>& Updates) override;

protected:
	/** Propagate a state to a single actor via BPI_TrafficLightUpdate. */
	void PropagateStateToActor(int32 TrafficLightId, AActor* Actor, const FOsiTrafficLightState& NewState);

private:
	/** Cached state per traffic light ID (transient, not serialized) */
	TMap<int32, FOsiTrafficLightState> StateCache;
};
