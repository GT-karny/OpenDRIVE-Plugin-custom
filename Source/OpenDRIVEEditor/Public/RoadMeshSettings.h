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
	Misc      = 4 UMETA(DisplayName = "Misc / Other"),
	Deck      = 5 UMETA(DisplayName = "Deck / Structure")  // elevated deck slab / parapet / piers + at-grade road slab
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

	/** If true, generate sidewalk / border / shoulder / other non-driving lane surfaces (by material slot). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scope")
	bool bGenerateNonDrivingLanes = true;

	// ---- Lanes ------------------------------------------------------------------------

	/** Raised-curb height (cm): sidewalk/border/curb lanes are lifted by this much with a vertical
	 *  curb riser face. Set to 0 for a fully flat surface (no step). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lanes", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "40.0"))
	float CurbHeightCm = 15.0f;

	/** Vertical slab thickness (cm) under every at-grade road, giving the drivable surface a real
	 *  volume instead of a paper-thin ribbon. Set 0 for the legacy thin ribbon. Elevated spans use
	 *  the thicker deck slab instead (the two never overlap). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lanes", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "200.0"))
	float RoadThicknessCm = 50.0f;

	// ---- Overpass Deck ----------------------------------------------------------------

	/** Turn the surface of elevated spans into a real bridge: thick deck slab (top + underside +
	 *  fascia), edge parapets, and support piers down to the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overpass Deck")
	bool bGenerateDeckStructure = false;

	/** Surface height (m) above GroundZCm at which a station is considered on an elevated deck. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overpass Deck", meta = (ClampMin = "0.1", UIMin = "0.5", UIMax = "10.0", EditCondition = "bGenerateDeckStructure"))
	float DeckHeightThresholdMeters = 2.0f;

	/** Ground reference Z (cm). Deck height is measured from here and piers descend to it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overpass Deck", meta = (EditCondition = "bGenerateDeckStructure"))
	float GroundZCm = 0.0f;

	/** Deck slab thickness (cm): how far the underside sits below the road surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overpass Deck", meta = (ClampMin = "1.0", UIMin = "20.0", UIMax = "200.0", EditCondition = "bGenerateDeckStructure"))
	float DeckThicknessCm = 80.0f;

	/** Parapet (edge barrier) height (cm). Set 0 to omit parapets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overpass Deck", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "200.0", EditCondition = "bGenerateDeckStructure"))
	float ParapetHeightCm = 90.0f;

	/** Parapet thickness (cm) toward the road centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overpass Deck", meta = (ClampMin = "1.0", UIMin = "5.0", UIMax = "60.0", EditCondition = "bGenerateDeckStructure"))
	float ParapetThicknessCm = 25.0f;

	/** Longitudinal spacing (m) between support piers along a deck span. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overpass Deck", meta = (ClampMin = "2.0", UIMin = "10.0", UIMax = "80.0", EditCondition = "bGenerateDeckStructure"))
	float PierSpacingMeters = 30.0f;

	/** Pier box half-width (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overpass Deck", meta = (ClampMin = "10.0", UIMin = "30.0", UIMax = "300.0", EditCondition = "bGenerateDeckStructure"))
	float PierHalfWidthCm = 90.0f;

	/** Skip a pier when its footprint comes within this distance (m) of a road passing underneath. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overpass Deck", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "10.0", EditCondition = "bGenerateDeckStructure"))
	float PierRoadClearanceMeters = 3.0f;

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
