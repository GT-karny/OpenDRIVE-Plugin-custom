#pragma once 
#include "EditorModes.h"
#include "EdMode.h"
#include "../OpenDriveEditorLane.h"
#include "OpenDriveLaneSpline.h"

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
	
	// General filters
	bool bGenerateRoads = true;
	bool bGenerateJunctions = true;

	void SetGenerateRoads(bool Val) { bGenerateRoads = Val; }
	bool GetGenerateRoads() const { return bGenerateRoads; }

	void SetGenerateJunctions(bool Val) { bGenerateJunctions = Val; }
	bool GetGenerateJunctions() const { return bGenerateJunctions; }

	// Lane generation flags
	bool bGenerateDrivingLane = true;
	bool bGenerateSidewalkLane = true;
	bool bGenerateBikingLane = true;
	bool bGenerateParkingLane = true;
	bool bGenerateShoulderLane = true;
	bool bGenerateRestrictedLane = true;
	bool bGenerateMedianLane = true;
	bool bGenerateOtherLane = true;

	void SetGenerateDrivingLane(bool bGenerate) { bGenerateDrivingLane = bGenerate; }
	bool GetGenerateDrivingLane() const { return bGenerateDrivingLane; }

	void SetGenerateSidewalkLane(bool bGenerate) { bGenerateSidewalkLane = bGenerate; }
	bool GetGenerateSidewalkLane() const { return bGenerateSidewalkLane; }

	void SetGenerateBikingLane(bool bGenerate) { bGenerateBikingLane = bGenerate; }
	bool GetGenerateBikingLane() const { return bGenerateBikingLane; }

	void SetGenerateParkingLane(bool bGenerate) { bGenerateParkingLane = bGenerate; }
	bool GetGenerateParkingLane() const { return bGenerateParkingLane; }
	
	void SetGenerateShoulderLane(bool bGenerate) { bGenerateShoulderLane = bGenerate; }
	bool GetGenerateShoulderLane() const { return bGenerateShoulderLane; }

	void SetGenerateRestrictedLane(bool bGenerate) { bGenerateRestrictedLane = bGenerate; }
	bool GetGenerateRestrictedLane() const { return bGenerateRestrictedLane; }

	void SetGenerateMedianLane(bool bGenerate) { bGenerateMedianLane = bGenerate; }
	bool GetGenerateMedianLane() const { return bGenerateMedianLane; }

	void SetGenerateOtherLane(bool bGenerate) { bGenerateOtherLane = bGenerate; }
	bool GetGenerateOtherLane() const { return bGenerateOtherLane; }

	bool bGenerateReferenceLane = true;
	void SetGenerateReferenceLane(bool bGenerate) { bGenerateReferenceLane = bGenerate; }
	bool GetGenerateReferenceLane() const { return bGenerateReferenceLane; }

	bool bGenerateOutermostDrivingLaneOnly = false;
	void SetGenerateOutermostDrivingLaneOnly(bool bGenerate) { bGenerateOutermostDrivingLaneOnly = bGenerate; }
	bool GetGenerateOutermostDrivingLaneOnly() const { return bGenerateOutermostDrivingLaneOnly; }


protected :

	/**
	 * Loads roads 
	 */
	void LoadRoadsNetwork();

	TArray<AOpenDriveEditorLane*> roadsArray;

private :

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
	
public:
	// Enum moved to AOpenDriveLaneSpline to allow Runtime access

	void SetSplineGenerationMode(AOpenDriveLaneSpline::EOpenDriveLaneSplineMode NewMode) { SplineGenerationMode = NewMode; }
	AOpenDriveLaneSpline::EOpenDriveLaneSplineMode GetSplineGenerationMode() const { return SplineGenerationMode; }

private:
	AOpenDriveLaneSpline::EOpenDriveLaneSplineMode SplineGenerationMode = AOpenDriveLaneSpline::Center;
};

