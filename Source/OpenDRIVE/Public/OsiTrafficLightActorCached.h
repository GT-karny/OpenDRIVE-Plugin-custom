// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OsiTrafficLightActor.h"
#include "OsiTrafficLightActorCached.generated.h"

/**
 * Traffic light actor that skips duplicate state updates.
 *
 * Caches the last received state and only calls OnTrafficLightStateChanged
 * when the state actually differs. Use this to avoid redundant visual
 * updates (e.g. re-triggering material swaps or light toggling).
 *
 * Usage:
 * 1. Create a Blueprint subclass of this actor
 * 2. Override OnTrafficLightStateChanged (NOT OnTrafficLightUpdate)
 * 3. Place instances in the level and set MyTrafficLightId per instance
 */
UCLASS(Blueprintable)
class OPENDRIVE_API AOsiTrafficLightActorCached : public AOsiTrafficLightActor
{
	GENERATED_BODY()

protected:
	/** Intercepts updates and skips if state is unchanged. */
	virtual void OnTrafficLightUpdate_Implementation(
		const FOsiTrafficLightState& NewState) override;

	/**
	 * Called only when the state actually changes.
	 * Override in Blueprint to update visuals (lights, materials, etc.).
	 * In C++ subclasses, override OnTrafficLightStateChanged_Implementation.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "OSI Traffic Light")
	void OnTrafficLightStateChanged(const FOsiTrafficLightState& NewState);
	virtual void OnTrafficLightStateChanged_Implementation(
		const FOsiTrafficLightState& NewState);

	/** The last received state. Valid after the first update. */
	UPROPERTY(BlueprintReadOnly, Category = "OSI Traffic Light")
	FOsiTrafficLightState CachedState;

private:
	bool bHasReceivedState = false;
};
