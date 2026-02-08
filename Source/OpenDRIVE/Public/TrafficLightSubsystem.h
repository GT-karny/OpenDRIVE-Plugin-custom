// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OsiTrafficLightTypes.h"
#include "BPI_TrafficLightHandlerUpdate.h"
#include "TrafficLightSubsystem.generated.h"

/**
 * Broadcast when a traffic light state is updated.
 * Traffic light actors bind to this delegate and filter by their own ID.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnOsiTrafficLightStateUpdated,
	int32, TrafficLightId,
	const FOsiTrafficLightState&, NewState);

/**
 * World subsystem for managing OSI traffic light state.
 *
 * Manages a state cache and broadcasts updates via delegate.
 * Does NOT hold references to traffic light actors — actors bind to
 * OnTrafficLightStateUpdated and filter by their own ID.
 *
 * Guaranteed to be initialized before any Actor's BeginPlay.
 *
 * Usage:
 * 1. Traffic light actors bind to OnTrafficLightStateUpdated in BeginPlay
 *    via GetWorld()->GetSubsystem<UTrafficLightSubsystem>()
 * 2. External systems (receiver actors, co-sim bridges) call
 *    UpdateTrafficLightById() / UpdateTrafficLightsBatch()
 *    through the BPI_TrafficLightHandlerUpdate interface or directly
 */
UCLASS()
class OPENDRIVE_API UTrafficLightSubsystem : public UWorldSubsystem,
	public IBPI_TrafficLightHandlerUpdate
{
	GENERATED_BODY()

public:
	// --- USubsystem interface ---
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

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
	UPROPERTY()
	TMap<int32, FOsiTrafficLightState> StateCache;
};
