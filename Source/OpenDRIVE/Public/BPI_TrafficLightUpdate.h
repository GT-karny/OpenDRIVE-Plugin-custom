// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "OsiTrafficLightTypes.h"
#include "BPI_TrafficLightUpdate.generated.h"

UINTERFACE(MinimalAPI, BlueprintType, Blueprintable)
class UBPI_TrafficLightUpdate : public UInterface
{
	GENERATED_BODY()
};

/**
 * Blueprint Interface for actors that receive OSI traffic light state updates.
 * Implement this on any traffic light actor to receive state information
 * from a TrafficLightHandlerBase.
 */
class OPENDRIVE_API IBPI_TrafficLightUpdate
{
	GENERATED_BODY()

public:
	/**
	 * Called when the traffic light state should be updated.
	 * @param NewState The new OSI traffic light state (color, icon, mode, counter)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "OSI Traffic Light")
	void OnTrafficLightUpdate(const FOsiTrafficLightState& NewState);
};
