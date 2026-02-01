#include "OpenDriveLaneSpline.h"
#include "CoordTranslate.h"

// Sets default values
AOpenDriveLaneSpline::AOpenDriveLaneSpline()
{
	PrimaryActorTick.bCanEverTick = false;
	SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
	RootComponent = SplineComponent;
}

void AOpenDriveLaneSpline::Initialize(roadmanager::Road* road, roadmanager::LaneSection* laneSection, roadmanager::Lane* lane, float offset, float step, EOpenDriveLaneSplineMode mode)
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

	if (LaneId == 0)
	{
		SplineComponent->ComponentTags.Add(FName("Reference Line"));
	}


	double laneLength = _laneSection->GetLength();
	double s = _laneSection->GetS();

	SplineComponent->ClearSplinePoints();

	roadmanager::Position position;
	
	// Start point
	position.Init();
	position.SetSnapLaneTypes(roadmanager::Lane::LANE_TYPE_ANY);

	SetLanePoint(position, s, offset, mode);

	// Had a lane spline point every step meters
	s += step;
	for (s; s < _laneSection->GetS() + laneLength; s += step)
	{
		SetLanePoint(position, s, offset, mode);
	}

	// Final point
	SetLanePoint(position, _laneSection->GetS() + laneLength, offset, mode);
	if (laneLength > step)
	{
		CheckLastTwoPointsDistance(step);
	}
}

void AOpenDriveLaneSpline::SetLanePoint(roadmanager::Position& position, double s, float offset, EOpenDriveLaneSplineMode mode)
{
	double t = 0.;
	if (mode != Center)
	{
		// Width calculation requires s, lane ID. _lane is available but width is variable along s.
		// LaneSection::GetLaneWidth(s, laneId) ? Accessing lane directly.
		double width = _laneSection->GetWidth(s, LaneId);

		// Inside: towards center (0). Outside: away from center.
		// LaneId > 0 (Left): t is positive. Inside means t becomes smaller (closer to 0), i.e., -width/2 relative to center.
		// LaneId < 0 (Right): t is negative. Inside means t becomes larger (closer to 0), i.e., +width/2.

		// Wait, SetLanePos takes (RoadId, LaneId, s, relative_t).
		// Wait, SetLanePos implementation: "offset: lateral offset relative to the lane center line"
		// If 0, it's center.
		// Width is the full width. 
		
		// If we want Inside (closest to road center):
		// Left Lane (>0): We want right side of lane. Offset = -Width/2.
		// Right Lane (<0): We want left side of lane. Offset = +Width/2.

		// If we want Outside (furthest from road center):
		// Left Lane (>0): We want left side of lane. Offset = +Width/2.
		// Right Lane (<0): We want right side of lane. Offset = -Width/2.

		if (mode == Inside)
		{
			if (LaneId > 0) t = -width / 2.0;
			else if (LaneId < 0) t = width / 2.0;
		}
		else if (mode == Outside)
		{
			if (LaneId > 0) t = width / 2.0;
			else if (LaneId < 0) t = -width / 2.0;
		}
	}

	position.SetLanePos(RoadId, LaneId, s, t);

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
