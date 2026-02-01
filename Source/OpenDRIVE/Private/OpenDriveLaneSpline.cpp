#include "OpenDriveLaneSpline.h"
#include "CoordTranslate.h"

// Sets default values
AOpenDriveLaneSpline::AOpenDriveLaneSpline()
{
	PrimaryActorTick.bCanEverTick = false;
	SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
	RootComponent = SplineComponent;
}

void AOpenDriveLaneSpline::Initialize(roadmanager::Road* road, roadmanager::LaneSection* laneSection, roadmanager::Lane* lane, float offset, float step)
{
	_road = road;
	_laneSection = laneSection;
	_lane = lane;

	RoadId = _road->GetId();
	JunctionId = _road->GetJunction();
	LaneId = _lane->GetId();
	
	switch (_lane->GetLaneType())
	{
	case(roadmanager::Lane::LaneType::LANE_TYPE_DRIVING):
		LaneType = "Driving road";
		break;
	case(roadmanager::Lane::LaneType::LANE_TYPE_BIKING):
		LaneType = "Bike path";
		break;
	case(roadmanager::Lane::LaneType::LANE_TYPE_SIDEWALK):
		LaneType = "Sidewalk lane";
		break;
	case(roadmanager::Lane::LaneType::LANE_TYPE_PARKING):
		LaneType = "Parking slot(s)";
		break;
	case(roadmanager::Lane::LaneType::LANE_TYPE_BORDER):
		LaneType = "Border";
		break;
	case(roadmanager::Lane::LaneType::LANE_TYPE_RAIL):
		LaneType = "Rail";
		break;
	case(roadmanager::Lane::LaneType::LANE_TYPE_TRAM):
		LaneType = "Tram";
		break;
	case(roadmanager::Lane::LaneType::LANE_TYPE_SHOULDER):
		LaneType = "Shoulder";
		break;
	case(roadmanager::Lane::LaneType::LANE_TYPE_RESTRICTED):
		LaneType = "Restricted lane";
		break;
	case(roadmanager::Lane::LaneType::LANE_TYPE_MEDIAN):
		LaneType = "Median";
		break;
	default:
		LaneType = "None";
		break;
	}
	
	SplineComponent->ComponentTags.Add(FName(*LaneType));
	UE_LOG(LogTemp, Log, TEXT("AOpenDriveLaneSpline::Initialize: RoadId=%d, LaneId=%d, LaneType=%s, TagsNum=%d"), RoadId, LaneId, *LaneType, SplineComponent->ComponentTags.Num());


	double laneLength = _laneSection->GetLength();
	double s = _laneSection->GetS();

	SplineComponent->ClearSplinePoints();

	roadmanager::Position position;
	
	// Start point
	position.Init();
	position.SetSnapLaneTypes(roadmanager::Lane::LANE_TYPE_ANY);
	SetLanePoint(position, s, offset);

	// Had a lane spline point every step meters
	s += step;
	for (s; s < _laneSection->GetS() + laneLength; s += step)
	{
		SetLanePoint(position, s, offset);
	}

	// Final point
	SetLanePoint(position, _laneSection->GetS() + laneLength, offset);
	if (laneLength > step)
	{
		CheckLastTwoPointsDistance(step);
	}
}

void AOpenDriveLaneSpline::SetLanePoint(roadmanager::Position& position, double s, float offset)
{
	position.SetLanePos(RoadId, LaneId, s, 0.);

	FVector sp;
	sp = CoordTranslate::OdrToUe::ToLocation(position);
	sp.Z += offset;
	SplineComponent->AddSplineWorldPoint(sp);
	
	FRotator rot = CoordTranslate::OdrToUe::ToRotation(position);
	SplineComponent->SetRotationAtSplinePoint(SplineComponent->GetNumberOfSplinePoints() - 1, rot, ESplineCoordinateSpace::World);

	// No scaling set as we are not using spline meshes for visualization in this actor
}

void AOpenDriveLaneSpline::CheckLastTwoPointsDistance(float step)
{
	float dist = FVector::Distance(SplineComponent->GetWorldLocationAtSplinePoint(SplineComponent->GetNumberOfSplinePoints() - 2),
		SplineComponent->GetWorldLocationAtSplinePoint(SplineComponent->GetNumberOfSplinePoints() - 1));
	if ((dist / 100) < step / 3)
	{
		SplineComponent->RemoveSplinePoint(SplineComponent->GetNumberOfSplinePoints() - 2);
	}
}
