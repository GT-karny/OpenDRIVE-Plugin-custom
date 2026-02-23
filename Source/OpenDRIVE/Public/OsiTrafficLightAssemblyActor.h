// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BPI_TrafficLightUpdate.h"
#include "OsiTrafficLightTypes.h"
#include "OsiTrafficLightAssemblyActor.generated.h"

class USignalInfoComponent;

/**
 * Base actor for OSI traffic light assemblies (grouped signals).
 *
 * Unlike AOsiTrafficLightActor (single signal), this actor manages
 * multiple USignalInfoComponents and responds to state updates for
 * ALL constituent signal IDs.
 *
 * Subclasses (Blueprint or C++) override OnTrafficLightAssemblyUpdate
 * to handle per-signal visual updates. The SignalId parameter identifies
 * which signal head received the update.
 *
 * Usage:
 * 1. Create a Blueprint subclass
 * 2. Override OnTrafficLightAssemblyUpdate in the Event Graph
 * 3. Use the SignalId parameter to determine which sub-light to update
 */
UCLASS(Blueprintable)
class OPENDRIVE_API AOsiTrafficLightAssemblyActor : public AActor,
	public IBPI_TrafficLightUpdate
{
	GENERATED_BODY()

public:
	AOsiTrafficLightAssemblyActor();

	/** Get all SignalInfoComponents attached to this assembly */
	UFUNCTION(BlueprintCallable, Category = "OSI Traffic Light|Assembly")
	TArray<USignalInfoComponent*> GetSignalInfoComponents() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** Default no-op for single-signal update (assembly uses OnTrafficLightAssemblyUpdate instead) */
	virtual void OnTrafficLightUpdate_Implementation(
		const FOsiTrafficLightState& NewState) override;

	/**
	 * Called when a constituent signal receives a state update.
	 * Override in Blueprint to update the appropriate signal head visuals.
	 * @param SignalId Identifies which signal head was updated
	 * @param NewState The new OSI traffic light state
	 */
	virtual void OnTrafficLightAssemblyUpdate_Implementation(
		int32 SignalId, const FOsiTrafficLightState& NewState) override;

private:
	/** Delegate callback bound to UTrafficLightSubsystem. Filters by all constituent SignalIds. */
	UFUNCTION()
	void OnSubsystemStateUpdated(int32 TrafficLightId, const FOsiTrafficLightState& NewState);

	/** Cached set of SignalIds for fast lookup during state updates */
	TSet<int32> ManagedSignalIds;
};
