// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BPI_TrafficLightUpdate.h"
#include "BPI_SignalAutoSetup.h"
#include "OsiTrafficLightTypes.h"
#include "OsiTrafficLightActor.generated.h"

/**
 * Base actor for OSI traffic lights.
 *
 * Handles all boilerplate: subsystem lookup, delegate binding, and
 * ID filtering. Subclasses (Blueprint or C++) only need to override
 * OnTrafficLightUpdate to implement the visual response.
 *
 * Usage:
 * 1. Create a Blueprint subclass of this actor
 * 2. Override OnTrafficLightUpdate in the Event Graph
 * 3. Place instances in the level and set MyTrafficLightId per instance
 */
UCLASS(Blueprintable)
class OPENDRIVE_API AOsiTrafficLightActor : public AActor,
	public IBPI_TrafficLightUpdate,
	public IBPI_SignalAutoSetup
{
	GENERATED_BODY()

public:
	AOsiTrafficLightActor();

	/** Traffic light ID. Set per instance in the Details panel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSI Traffic Light")
	int32 MyTrafficLightId = 0;

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

	/** Called by FSignalGenerator after auto-placement. Sets MyTrafficLightId from SignalInfo. */
	virtual void OnSignalAutoPlaced_Implementation(
		const USignalInfoComponent* SignalInfo) override;

private:
	/** Delegate callback bound to UTrafficLightSubsystem. Filters by ID. */
	UFUNCTION()
	void OnSubsystemStateUpdated(int32 TrafficLightId, const FOsiTrafficLightState& NewState);
};
