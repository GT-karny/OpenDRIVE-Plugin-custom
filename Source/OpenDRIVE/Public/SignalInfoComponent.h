#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SignalInfoComponent.generated.h"

/**
 * Holds OpenDRIVE signal information for an actor.
 * Attach this component to any actor that represents a road signal/sign.
 */
UCLASS(ClassGroup=(OpenDRIVE), meta=(BlueprintSpawnableComponent))
class OPENDRIVE_API USignalInfoComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USignalInfoComponent();

	/** Signal ID from OpenDRIVE */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Signal Info")
	int32 SignalId;

	/** Road ID where signal is located */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Signal Info")
	int32 RoadId;

	/** S-coordinate along the road (meters) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Signal Info")
	double S;

	/** T-coordinate (lateral offset, meters) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Signal Info")
	double T;

	/** Signal type (e.g., "1000001" for traffic lights in Germany) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Signal Info")
	FString Type;

	/** Signal subtype */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Signal Info")
	FString SubType;

	/** Country code (e.g., "DEU", "JPN") */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Signal Info")
	FString Country;

	/** Signal value (e.g., speed limit value) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Signal Info")
	double Value;

	/** Unit for the value (e.g., "km/h") */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Signal Info")
	FString Unit;

	/** Signal text content */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Signal Info")
	FString Text;

	/** Whether this is a dynamic signal (e.g., variable message sign) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Signal Info")
	bool bIsDynamic;

	/** Physical height of the signal (meters) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Signal Info")
	double Height;

	/** Physical width of the signal (meters) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Signal Info")
	double Width;
};
