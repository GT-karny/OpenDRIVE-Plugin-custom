// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "OsiTrafficLightTypes.h"
#include "BPI_TrafficLightHandlerUpdate.generated.h"

UINTERFACE(MinimalAPI, BlueprintType, Blueprintable)
class UBPI_TrafficLightHandlerUpdate : public UInterface
{
	GENERATED_BODY()
};

/**
 * Blueprint Interface for handlers that receive external OSI traffic light updates.
 * External systems (co-simulation bridges, esmini, scenario runners) call these
 * methods on a TrafficLightHandlerBase to update managed traffic lights.
 */
class OPENDRIVE_API IBPI_TrafficLightHandlerUpdate
{
	GENERATED_BODY()

public:
	/**
	 * Update a single managed traffic light by its ID.
	 * @param TrafficLightId The identifier for the managed traffic light
	 * @param NewState The new OSI state to apply
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "OSI Traffic Light|Handler")
	void UpdateTrafficLightById(int32 TrafficLightId, const FOsiTrafficLightState& NewState);

	/**
	 * Update multiple managed traffic lights at once.
	 * @param Updates Array of ID + State pairs to apply
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "OSI Traffic Light|Handler")
	void UpdateTrafficLightsBatch(const TArray<FOsiTrafficLightBatchEntry>& Updates);
};
