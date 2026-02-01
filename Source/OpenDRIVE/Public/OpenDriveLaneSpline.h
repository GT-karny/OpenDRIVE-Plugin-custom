#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineComponent.h"
#include "RoadManager.hpp"
#include "OpenDriveLaneSpline.generated.h"

UCLASS()
class OPENDRIVE_API AOpenDriveLaneSpline : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AOpenDriveLaneSpline();

	/**
	 * Initializes the lane spline
	 * @param road The Roadmanager's road
	 * @param laneSection The Roadmanager's lane section 
	 * @param lane The Roadmanager's lane
	 * @param offset The road offset
	 * @param step The step (the lower it is, the more precise it will be)  
	 */
	void Initialize(roadmanager::Road* road, roadmanager::LaneSection* laneSection, roadmanager::Lane* lane, float offset, float step);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OpenDRIVE")
	int RoadId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OpenDRIVE")
	int JunctionId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OpenDRIVE")
	int LaneId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OpenDRIVE")
	FString LaneType;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USplineComponent* SplineComponent;

private:
	/**
	* Sets a lane's spline point
	* @param position Reference to roadmanager::Position
	* @param ds Distance to move along lane
	* @param offset The road offset
	*/
	void SetLanePoint(roadmanager::Position& position, double s, float offset);

	/**
	* Checks the distance between the last spline point and his predecessor : if the distance is too short, we remove its predecessor.
	* @param step The step used
	*/
	void CheckLastTwoPointsDistance(float step);

	roadmanager::Road* _road;
	roadmanager::LaneSection* _laneSection;
	roadmanager::Lane* _lane;
};
