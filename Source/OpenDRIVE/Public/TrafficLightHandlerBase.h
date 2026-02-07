// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OsiTrafficLightTypes.h"
#include "BPI_TrafficLightHandlerUpdate.h"
#include "TrafficLightHandlerBase.generated.h"

/**
 * Broadcast when a traffic light state is updated.
 * Traffic light actors bind to this delegate and filter by their own ID.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnOsiTrafficLightStateUpdated,
	int32, TrafficLightId,
	const FOsiTrafficLightState&, NewState);

/**
 * Base handler actor for managing OSI traffic light state.
 *
 * Manages a state cache and broadcasts updates via delegate.
 * Does NOT hold references to traffic light actors — actors bind to
 * OnTrafficLightStateUpdated and filter by their own ID.
 *
 * Usage:
 * 1. Place this actor (or a Blueprint subclass) in the level
 * 2. Traffic light actors bind to OnTrafficLightStateUpdated in BeginPlay
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

	/**
	 * Broadcast when any traffic light state is updated.
	 * Traffic light actors should bind to this in BeginPlay
	 * and filter by TrafficLightId.
	 */
	UPROPERTY(BlueprintAssignable, Category = "OSI Traffic Light|Handler")
	FOnOsiTrafficLightStateUpdated OnTrafficLightStateUpdated;

	/**
	 * Get the current cached state of a traffic light by ID.
	 * @param TrafficLightId The ID to query
	 * @param OutState The current state (only valid if return is true)
	 * @return true if found
	 */
	UFUNCTION(BlueprintCallable, Category = "OSI Traffic Light|Handler")
	bool GetTrafficLightState(int32 TrafficLightId, FOsiTrafficLightState& OutState) const;

	// --- BPI_TrafficLightHandlerUpdate implementation ---
	virtual void UpdateTrafficLightById_Implementation(int32 TrafficLightId, const FOsiTrafficLightState& NewState) override;
	virtual void UpdateTrafficLightsBatch_Implementation(const TArray<FOsiTrafficLightBatchEntry>& Updates) override;

private:
	/** ID -> current state cache */
	UPROPERTY(VisibleAnywhere, Category = "OSI Traffic Light|Handler")
	TMap<int32, FOsiTrafficLightState> StateCache;
};
