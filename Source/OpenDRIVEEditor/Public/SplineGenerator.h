#pragma once
#include "CoreMinimal.h"
#include "OpenDriveLaneSpline.h"

class FSplineGenerator
{
public:

	// Lane position filter modes
	enum class ELanePositionFilter : uint8
	{
		All = 0,                 // Generate all lanes
		OutermostOnly = 1,       // Outermost lane per side (all lane types)
		OutermostDrivingOnly = 2,// Outermost driving lane per side
		InnermostOnly = 3,       // Innermost lane per side (all lane types)
		InnermostDrivingOnly = 4,// Innermost driving lane per side
		SpecificIndex = 5        // Specific lane index from center (all lane types)
	};

	/**
	 * Generates lane spline actors from OpenDRIVE road data.
	 * @param World The world to spawn actors in
	 */
	void GenerateLaneSplines(UWorld* World);

	/**
	 * Clears all generated lane spline actors.
	 */
	void ClearGeneratedSplines();

	// --- Settings ---

	float Offset = 20.0f;
	void SetOffset(float Val) { Offset = Val; }
	float GetOffset() const { return Offset; }

	float Step = 5.0f;
	void SetStep(float Val) { Step = Val; }
	float GetStep() const { return Step; }

	AOpenDriveLaneSpline::EOpenDriveLaneSplineMode SplineMode = AOpenDriveLaneSpline::Center;
	void SetSplineMode(AOpenDriveLaneSpline::EOpenDriveLaneSplineMode Val) { SplineMode = Val; }
	AOpenDriveLaneSpline::EOpenDriveLaneSplineMode GetSplineMode() const { return SplineMode; }

	// General filters
	bool bGenerateRoads = true;
	void SetGenerateRoads(bool Val) { bGenerateRoads = Val; }
	bool GetGenerateRoads() const { return bGenerateRoads; }

	bool bGenerateJunctions = true;
	void SetGenerateJunctions(bool Val) { bGenerateJunctions = Val; }
	bool GetGenerateJunctions() const { return bGenerateJunctions; }

	// Side filters
	bool bGenerateLeftLanes = true;
	void SetGenerateLeftLanes(bool Val) { bGenerateLeftLanes = Val; }
	bool GetGenerateLeftLanes() const { return bGenerateLeftLanes; }

	bool bGenerateRightLanes = true;
	void SetGenerateRightLanes(bool Val) { bGenerateRightLanes = Val; }
	bool GetGenerateRightLanes() const { return bGenerateRightLanes; }

	// Lane type filters
	bool bGenerateDrivingLane = true;
	void SetGenerateDrivingLane(bool Val) { bGenerateDrivingLane = Val; }
	bool GetGenerateDrivingLane() const { return bGenerateDrivingLane; }

	bool bGenerateSidewalkLane = true;
	void SetGenerateSidewalkLane(bool Val) { bGenerateSidewalkLane = Val; }
	bool GetGenerateSidewalkLane() const { return bGenerateSidewalkLane; }

	bool bGenerateBikingLane = true;
	void SetGenerateBikingLane(bool Val) { bGenerateBikingLane = Val; }
	bool GetGenerateBikingLane() const { return bGenerateBikingLane; }

	bool bGenerateParkingLane = true;
	void SetGenerateParkingLane(bool Val) { bGenerateParkingLane = Val; }
	bool GetGenerateParkingLane() const { return bGenerateParkingLane; }

	bool bGenerateShoulderLane = true;
	void SetGenerateShoulderLane(bool Val) { bGenerateShoulderLane = Val; }
	bool GetGenerateShoulderLane() const { return bGenerateShoulderLane; }

	bool bGenerateRestrictedLane = true;
	void SetGenerateRestrictedLane(bool Val) { bGenerateRestrictedLane = Val; }
	bool GetGenerateRestrictedLane() const { return bGenerateRestrictedLane; }

	bool bGenerateMedianLane = true;
	void SetGenerateMedianLane(bool Val) { bGenerateMedianLane = Val; }
	bool GetGenerateMedianLane() const { return bGenerateMedianLane; }

	bool bGenerateOtherLane = true;
	void SetGenerateOtherLane(bool Val) { bGenerateOtherLane = Val; }
	bool GetGenerateOtherLane() const { return bGenerateOtherLane; }

	bool bGenerateReferenceLane = true;
	void SetGenerateReferenceLane(bool Val) { bGenerateReferenceLane = Val; }
	bool GetGenerateReferenceLane() const { return bGenerateReferenceLane; }

	// Lane position filter
	ELanePositionFilter LanePositionFilter = ELanePositionFilter::All;
	void SetLanePositionFilter(ELanePositionFilter Val) { LanePositionFilter = Val; }
	ELanePositionFilter GetLanePositionFilter() const { return LanePositionFilter; }

	int32 SpecificLaneIndex = 1;
	void SetSpecificLaneIndex(int32 Val) { SpecificLaneIndex = FMath::Max(1, Val); }
	int32 GetSpecificLaneIndex() const { return SpecificLaneIndex; }

	// Backward-compatible setter (maps to OutermostDrivingOnly)
	bool bGenerateOutermostDrivingLaneOnly = false;
	void SetGenerateOutermostDrivingLaneOnly(bool Val)
	{
		bGenerateOutermostDrivingLaneOnly = Val;
		if (Val)
		{
			LanePositionFilter = ELanePositionFilter::OutermostDrivingOnly;
		}
		else if (LanePositionFilter == ELanePositionFilter::OutermostDrivingOnly)
		{
			LanePositionFilter = ELanePositionFilter::All;
		}
	}
	bool GetGenerateOutermostDrivingLaneOnly() const { return bGenerateOutermostDrivingLaneOnly; }

private:

	/** Array of spawned spline actors for cleanup */
	TArray<AOpenDriveLaneSpline*> GeneratedSplines;
};
