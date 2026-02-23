#include "Public/SplineGenerator.h"
#include "RoadManager.hpp"

void FSplineGenerator::GenerateLaneSplines(UWorld* World)
{
	ClearGeneratedSplines();

	if (!World)
	{
		UE_LOG(LogClass, Warning, TEXT("GenerateLaneSplines: World is null"));
		return;
	}

	// roadmanager params
	roadmanager::OpenDrive* Odr = roadmanager::Position::GetOpenDrive();
	if (!Odr)
	{
		UE_LOG(LogClass, Warning, TEXT("GenerateLaneSplines: OpenDrive not loaded"));
		return;
	}

	roadmanager::Road* road = 0;
	roadmanager::LaneSection* laneSection = 0;
	roadmanager::Lane* lane = 0;
	size_t nrOfRoads = Odr->GetNumOfRoads();

	if (nrOfRoads == 0)
	{
		UE_LOG(LogClass, Warning, TEXT("GenerateLaneSplines: No roads found"));
		return;
	}

	// Actor spawn params
	FActorSpawnParameters spawnParam;
	spawnParam.bHideFromSceneOutliner = false;
	spawnParam.bTemporaryEditorActor = false;

	int32 TotalSplinesSpawned = 0;

	for (int i = 0; i < (int)nrOfRoads; i++)
	{
		road = Odr->GetRoadByIdx(i);
		if (!road) continue;

		for (int j = 0; j < road->GetNumberOfLaneSections(); j++)
		{
			laneSection = road->GetLaneSectionByIdx(j);

			if (!laneSection) continue;

			// Check filtering for Roads vs Junctions
			int32 JunctionId = road->GetJunction();
			bool bIsJunction = (JunctionId != -1);

			if (bIsJunction && !bGenerateJunctions) continue;
			if (!bIsJunction && !bGenerateRoads) continue;

			// Identify Outermost Driving Lanes
			int32 minRightDrivingId = 0;
			int32 maxLeftDrivingId = 0;

			if (bGenerateOutermostDrivingLaneOnly)
			{
				for (int m = 0; m < laneSection->GetNumberOfLanes(); m++)
				{
					roadmanager::Lane* checkLane = laneSection->GetLaneByIdx(m);
					if (!checkLane) continue;
					if (checkLane->GetLaneType() == roadmanager::Lane::LaneType::LANE_TYPE_DRIVING)
					{
						int32 id = checkLane->GetId();
						if (id < 0)
						{
							if (minRightDrivingId == 0 || id < minRightDrivingId) minRightDrivingId = id;
						}
						else if (id > 0)
						{
							if (maxLeftDrivingId == 0 || id > maxLeftDrivingId) maxLeftDrivingId = id;
						}
					}
				}
			}

			for (int k = 0; k < laneSection->GetNumberOfLanes(); k++)
			{
				lane = laneSection->GetLaneByIdx(k);

				if (!lane) continue;

				bool bShouldGenerate = false;
				if (lane->GetId() == 0)
				{
					bShouldGenerate = bGenerateReferenceLane;
				}
				else
				{
					switch (lane->GetLaneType())
					{
					case roadmanager::Lane::LaneType::LANE_TYPE_DRIVING:
						bShouldGenerate = bGenerateDrivingLane;
						if (bShouldGenerate && bGenerateOutermostDrivingLaneOnly)
						{
							int32 id = lane->GetId();
							if (id < 0 && id != minRightDrivingId) bShouldGenerate = false;
							if (id > 0 && id != maxLeftDrivingId) bShouldGenerate = false;
						}
						break;
					case roadmanager::Lane::LaneType::LANE_TYPE_SIDEWALK:
						bShouldGenerate = bGenerateSidewalkLane;
						break;
					case roadmanager::Lane::LaneType::LANE_TYPE_BIKING:
						bShouldGenerate = bGenerateBikingLane;
						break;
					case roadmanager::Lane::LaneType::LANE_TYPE_PARKING:
						bShouldGenerate = bGenerateParkingLane;
						break;
					case roadmanager::Lane::LaneType::LANE_TYPE_SHOULDER:
						bShouldGenerate = bGenerateShoulderLane;
						break;
					case roadmanager::Lane::LaneType::LANE_TYPE_RESTRICTED:
						bShouldGenerate = bGenerateRestrictedLane;
						break;
					case roadmanager::Lane::LaneType::LANE_TYPE_MEDIAN:
						bShouldGenerate = bGenerateMedianLane;
						break;
					default:
						bShouldGenerate = bGenerateOtherLane;
						break;
					}
				}

				if (!bShouldGenerate) continue;

				AOpenDriveLaneSpline* newSpline = World->SpawnActor<AOpenDriveLaneSpline>(FVector::ZeroVector, FRotator::ZeroRotator, spawnParam);
				newSpline->Initialize(road, laneSection, lane, Offset, Step, SplineMode);
#if WITH_EDITOR
				newSpline->SetActorLabel(FString::Printf(TEXT("LaneSpline_Road%d_Lane%d"), road->GetId(), lane->GetId()));
#endif
				GeneratedSplines.Add(newSpline);
				TotalSplinesSpawned++;
			}
		}
	}

	UE_LOG(LogClass, Log, TEXT("GenerateLaneSplines: Spawned %d splines"), TotalSplinesSpawned);
}

void FSplineGenerator::ClearGeneratedSplines()
{
	for (AOpenDriveLaneSpline* Spline : GeneratedSplines)
	{
		if (IsValid(Spline))
		{
			Spline->Destroy();
		}
	}
	GeneratedSplines.Empty();
}
