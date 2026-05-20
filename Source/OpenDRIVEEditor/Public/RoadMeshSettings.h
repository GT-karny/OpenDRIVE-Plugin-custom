// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInterface.h"
#include "RoadMeshSettings.generated.h"

UENUM(BlueprintType)
enum class ERoadMeshMaterialSlot : uint8
{
	Asphalt   = 0 UMETA(DisplayName = "Asphalt / Driving"),
	Sidewalk  = 1 UMETA(DisplayName = "Sidewalk / Concrete"),
	Border    = 2 UMETA(DisplayName = "Border / Shoulder"),
	Marking   = 3 UMETA(DisplayName = "Road Marking"),
	Misc      = 4 UMETA(DisplayName = "Misc / Other")
};

USTRUCT(BlueprintType)
struct OPENDRIVEEDITOR_API FRoadMeshSettings
{
	GENERATED_BODY()

	/** Maximum step along s in meters between adjacent samples. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling")
	float MaxStepMeters = 2.0f;

	/** Minimum step along s in meters; below this distance samples are coalesced. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling")
	float MinStepMeters = 0.2f;

	/** Z offset in cm added to road surface to avoid Z-fighting with Landscape. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling")
	float ZOffsetCm = 0.0f;

	/** Z offset in cm added to road markings on top of road surface.
	 *  0.05cm (0.5mm) is the practical lower bound for 32-bit float Z without z-fighting near origin.
	 *  For very large worlds (kilometers from origin) you may need 0.1cm.
	 *  Best alternative: use a marking material with Pixel Depth Offset and set this to 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling")
	float MarkingZOffsetCm = 0.05f;

	/** If true, generate road markings (lane boundary lines). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scope")
	bool bGenerateMarkings = true;

	/** If true, generate junction patches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scope")
	bool bGenerateJunctionPatches = true;

	/** Materials assigned to each ERoadMeshMaterialSlot in order. Slots may be left null. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	TArray<UMaterialInterface*> Materials;

	/** Content folder to auto-discover materials from when Materials is empty. Searched recursively.
	 *  Default: "/Game/RoadGeneration/Material". Materials are mapped to slots by name keyword:
	 *  Asphalt/Driving/Road, Sidewalk/Walk, Border/Shoulder/Curb, Mark/Line/Stripe, Misc/Other. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	FString MaterialFolder = TEXT("/Game/RoadGeneration/Material");

	/** Returns the ERoadMeshMaterialSlot for a roadmanager::Lane::LaneType (int cast). */
	static int32 SlotForLaneType(int32 LaneTypeFlag);
};
