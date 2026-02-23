#pragma once
#include "CoreMinimal.h"
#include "OpenDriveLaneSpline.h"

class FSplineGenerator
{
public:

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

	bool bGenerateOutermostDrivingLaneOnly = false;
	void SetGenerateOutermostDrivingLaneOnly(bool Val) { bGenerateOutermostDrivingLaneOnly = Val; }
	bool GetGenerateOutermostDrivingLaneOnly() const { return bGenerateOutermostDrivingLaneOnly; }

private:

	/** Array of spawned spline actors for cleanup */
	TArray<AOpenDriveLaneSpline*> GeneratedSplines;
};
