// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BPI_TrafficLightUpdate.h"
#include "SignalInfoComponent.h"
#include "OsiTrafficLightTypes.h"
#include "OsiTrafficLightActor.generated.h"

/**
 * Base actor for OSI traffic lights.
 *
 * Handles all boilerplate: subsystem lookup, delegate binding, and
 * ID filtering. Subclasses (Blueprint or C++) only need to override
 * OnTrafficLightUpdate to implement the visual response.
 *
 * Includes a default USignalInfoComponent whose SignalId is used for
 * delegate filtering. FSignalGenerator populates it automatically;
 * for manual placement, set SignalId in the Details panel.
 *
 * Usage:
 * 1. Create a Blueprint subclass of this actor
 * 2. Override OnTrafficLightUpdate in the Event Graph
 * 3. Place instances in the level and set SignalInfo > SignalId per instance
 */
UCLASS(Blueprintable)
class OPENDRIVE_API AOsiTrafficLightActor : public AActor,
	public IBPI_TrafficLightUpdate
{
	GENERATED_BODY()

public:
	AOsiTrafficLightActor();

	/** Signal metadata (includes SignalId for filtering). Default subobject. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OSI Traffic Light")
	TObjectPtr<USignalInfoComponent> SignalInfo;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/**
	 * Called when this traffic light receives a state update.
	 * Override in Blueprint to update visuals (lights, materials, etc.).
	 * In C++ subclasses, override OnTrafficLightUpdate_Implementation.
	 */
	virtual void OnTrafficLightUpdate_Implementation(
		const FOsiTrafficLightState& NewState) override;

private:
	/** Delegate callback bound to UTrafficLightSubsystem. Filters by SignalInfo->SignalId. */
	UFUNCTION()
	void OnSubsystemStateUpdated(int32 TrafficLightId, const FOsiTrafficLightState& NewState);
};
