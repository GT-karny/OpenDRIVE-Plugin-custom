#pragma once
#include "EditorModes.h"
#include "EdMode.h"
#include "../OpenDriveEditorLane.h"
#include "OpenDriveLaneSpline.h"
#include "../SignalGenerator.h"
#include "../SplineGenerator.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnLaneSelected, AOpenDriveEditorLane* road)

class FOpenDRIVEEditorMode : public FEdMode
{
public :

	const static FEditorModeID EM_RoadMode;

	FOpenDRIVEEditorMode();

	~FOpenDRIVEEditorMode();

	/**
	* Called everytime the editor mode is entered
	*/
	virtual void Enter() override;

	/**
	* Called everytime the editor mode is closed
	*/
	virtual void Exit() override;

	/**
	 * Gets if the roads are drawn or not.
	 * @return true if loaded, false if not
	 */
	inline bool GetHasBeenLoaded() const { return bHasBeenLoaded; };

	/**
	 * Deletes drawn roads.
	 */
	void ResetRoadsArray();

	/*
	 * Generates roads.
	 * It will call Reset() if there's already a generation done.
	 */
	void Generate();

	/**
	 * Generates lane splines (persistent actors).
	 */
	void GenerateLaneSplines();

	/**
	 * Sets the road offset
	 * @param newOffset_ The new offset
	 */
	inline void SetRoadOffset(float newOffset_) { _roadOffset = newOffset_;};

	/**
	 * @return The road offset
	 */
	inline float GetRoadOffset() { return _roadOffset; };

	/**
	* Sets the step for the roads' lanes drawing
	* @param The new step
	*/
	inline void SetStep(float newStep_) { _step = newStep_; };

	/**
	* @return The step
	*/
	inline float GetStep() { return _step; };

	/**
	* Sets roads' arrows visibility
	* @param bIsVisible True for visible false for hidden
	*/
	void SetRoadsArrowsVisibilityInEditor(bool bIsVisible);

	/**
	* Sets the roads visibility in editor only
	* @param bIsVisible True for visible, False for hidden
	*/
	void SetRoadsVisibilityInEditor(bool bIsVisible);

	// General filters (forwarded to SplineGenerator)
	void SetGenerateRoads(bool Val) { SplineGenerator.SetGenerateRoads(Val); }
	bool GetGenerateRoads() const { return SplineGenerator.GetGenerateRoads(); }

	void SetGenerateJunctions(bool Val) { SplineGenerator.SetGenerateJunctions(Val); }
	bool GetGenerateJunctions() const { return SplineGenerator.GetGenerateJunctions(); }

	// Lane generation flags (forwarded to SplineGenerator)
	void SetGenerateDrivingLane(bool bGenerate) { SplineGenerator.SetGenerateDrivingLane(bGenerate); }
	bool GetGenerateDrivingLane() const { return SplineGenerator.GetGenerateDrivingLane(); }

	void SetGenerateSidewalkLane(bool bGenerate) { SplineGenerator.SetGenerateSidewalkLane(bGenerate); }
	bool GetGenerateSidewalkLane() const { return SplineGenerator.GetGenerateSidewalkLane(); }

	void SetGenerateBikingLane(bool bGenerate) { SplineGenerator.SetGenerateBikingLane(bGenerate); }
	bool GetGenerateBikingLane() const { return SplineGenerator.GetGenerateBikingLane(); }

	void SetGenerateParkingLane(bool bGenerate) { SplineGenerator.SetGenerateParkingLane(bGenerate); }
	bool GetGenerateParkingLane() const { return SplineGenerator.GetGenerateParkingLane(); }

	void SetGenerateShoulderLane(bool bGenerate) { SplineGenerator.SetGenerateShoulderLane(bGenerate); }
	bool GetGenerateShoulderLane() const { return SplineGenerator.GetGenerateShoulderLane(); }

	void SetGenerateRestrictedLane(bool bGenerate) { SplineGenerator.SetGenerateRestrictedLane(bGenerate); }
	bool GetGenerateRestrictedLane() const { return SplineGenerator.GetGenerateRestrictedLane(); }

	void SetGenerateMedianLane(bool bGenerate) { SplineGenerator.SetGenerateMedianLane(bGenerate); }
	bool GetGenerateMedianLane() const { return SplineGenerator.GetGenerateMedianLane(); }

	void SetGenerateOtherLane(bool bGenerate) { SplineGenerator.SetGenerateOtherLane(bGenerate); }
	bool GetGenerateOtherLane() const { return SplineGenerator.GetGenerateOtherLane(); }

	void SetGenerateReferenceLane(bool bGenerate) { SplineGenerator.SetGenerateReferenceLane(bGenerate); }
	bool GetGenerateReferenceLane() const { return SplineGenerator.GetGenerateReferenceLane(); }

	void SetGenerateOutermostDrivingLaneOnly(bool bGenerate) { SplineGenerator.SetGenerateOutermostDrivingLaneOnly(bGenerate); }
	bool GetGenerateOutermostDrivingLaneOnly() const { return SplineGenerator.GetGenerateOutermostDrivingLaneOnly(); }

	// Side filters (forwarded to SplineGenerator)
	void SetGenerateLeftLanes(bool Val) { SplineGenerator.SetGenerateLeftLanes(Val); }
	bool GetGenerateLeftLanes() const { return SplineGenerator.GetGenerateLeftLanes(); }

	void SetGenerateRightLanes(bool Val) { SplineGenerator.SetGenerateRightLanes(Val); }
	bool GetGenerateRightLanes() const { return SplineGenerator.GetGenerateRightLanes(); }

	// Lane position filter (forwarded to SplineGenerator)
	void SetLanePositionFilter(FSplineGenerator::ELanePositionFilter Val) { SplineGenerator.SetLanePositionFilter(Val); }
	FSplineGenerator::ELanePositionFilter GetLanePositionFilter() const { return SplineGenerator.GetLanePositionFilter(); }

	void SetSpecificLaneIndex(int32 Val) { SplineGenerator.SetSpecificLaneIndex(Val); }
	int32 GetSpecificLaneIndex() const { return SplineGenerator.GetSpecificLaneIndex(); }

	// Spline generation mode (forwarded to SplineGenerator)
	void SetSplineGenerationMode(AOpenDriveLaneSpline::EOpenDriveLaneSplineMode NewMode) { SplineGenerator.SetSplineMode(NewMode); }
	AOpenDriveLaneSpline::EOpenDriveLaneSplineMode GetSplineGenerationMode() const { return SplineGenerator.GetSplineMode(); }

	// === Signal Generation (delegated to FSignalGenerator) ===

	FSignalGenerator SignalGenerator;

	void GenerateSignals() { SignalGenerator.GenerateSignals(GetWorld()); }
	void ClearGeneratedSplines() { SplineGenerator.ClearGeneratedSplines(); }
	void ClearGeneratedSignals() { SignalGenerator.ClearGeneratedSignals(); }
	FSignalGenerator& GetSignalGenerator() { return SignalGenerator; }

protected :

	/**
	 * Loads roads
	 */
	void LoadRoadsNetwork();

	TArray<AOpenDriveEditorLane*> roadsArray;

private :

	FSplineGenerator SplineGenerator;

	float _roadOffset = 20.0f;
	float _step = 5.f;
	bool bHasBeenLoaded = false;

	FDelegateHandle MapOpenedDelegateHandle;
	/**
	* Called when a new level is opened (or created)
	* @param type MapChangeEventFlags namespace flag
	*/
	void OnMapOpenedCallback(uint32 type);
	bool bIsMapOpening = false;

	FDelegateHandle OnActorSelectedHandle;
	/**
	* Called when an actor is selected in editor
	* @param selectedObject The selected object
	*/
	void OnActorSelected(UObject* _selectedObject);
};
