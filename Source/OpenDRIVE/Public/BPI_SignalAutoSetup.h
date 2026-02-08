// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BPI_SignalAutoSetup.generated.h"

class USignalInfoComponent;

UINTERFACE(MinimalAPI, BlueprintType, Blueprintable)
class UBPI_SignalAutoSetup : public UInterface
{
	GENERATED_BODY()
};

/**
 * Blueprint Interface for actors that need automatic property setup
 * when placed by the editor's signal generator (FSignalGenerator).
 *
 * Implement this on any actor class used in USignalTypeMapping.
 * After spawning and attaching USignalInfoComponent, the generator
 * calls OnSignalAutoPlaced so the actor can extract what it needs
 * (e.g., set MyTrafficLightId from SignalInfo->SignalId).
 */
class OPENDRIVE_API IBPI_SignalAutoSetup
{
	GENERATED_BODY()

public:
	/**
	 * Called by FSignalGenerator after the actor is spawned and
	 * USignalInfoComponent is attached with all OpenDRIVE metadata.
	 * @param SignalInfo The populated signal info component
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Signal Auto Setup")
	void OnSignalAutoPlaced(const USignalInfoComponent* SignalInfo);
};
