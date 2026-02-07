// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BPI_TrafficLightUpdate.h"
#include "BPI_TrafficLightRegister.generated.h"

UINTERFACE(MinimalAPI, BlueprintType, Blueprintable)
class UBPI_TrafficLightRegister : public UInterface
{
	GENERATED_BODY()
};

/**
 * Blueprint Interface for registering/unregistering traffic light actors with a handler.
 * Allows external systems to register BPI_TrafficLightUpdate-implementing actors
 * by ID without knowing the concrete handler class.
 */
class OPENDRIVE_API IBPI_TrafficLightRegister
{
	GENERATED_BODY()

public:
	/**
	 * Register a traffic light actor with a given ID.
	 * @param TrafficLightId Unique integer identifier
	 * @param TrafficLightActor Actor implementing BPI_TrafficLightUpdate
	 * @return true if registered successfully
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "OSI Traffic Light|Handler")
	bool RegisterTrafficLight(int32 TrafficLightId, const TScriptInterface<IBPI_TrafficLightUpdate>& TrafficLightActor);

	/**
	 * Unregister a traffic light by its ID.
	 * @param TrafficLightId The ID to remove
	 * @return true if found and removed
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "OSI Traffic Light|Handler")
	bool UnregisterTrafficLight(int32 TrafficLightId);
};
