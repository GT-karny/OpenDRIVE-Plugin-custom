#include "Public/SplineGenerator.h"
#include "RoadManager.hpp"

void FSplineGenerator::GenerateLaneSplines(UWorld* World)
{
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

		int32 RoadId = road->GetId();
		int32 JunctionId = road->GetJunction();
		bool bIsJunction = (JunctionId != -1);

		if (bIsJunction && !bGenerateJunctions) continue;
		if (!bIsJunction && !bGenerateRoads) continue;

		for (int j = 0; j < road->GetNumberOfLaneSections(); j++)
		{
			laneSection = road->GetLaneSectionByIdx(j);
			if (!laneSection) continue;

			// ============================================================
			// First Pass: Build lane index maps and identify outermost/innermost
			// ============================================================

			// All lanes (excluding reference lane ID=0), sorted by distance from center
			TArray<int32> RightAllLaneIds;   // negative IDs
			TArray<int32> LeftAllLaneIds;    // positive IDs

			// Driving lanes only, sorted by distance from center
			TArray<int32> RightDrivingLaneIds;
			TArray<int32> LeftDrivingLaneIds;

			for (int m = 0; m < laneSection->GetNumberOfLanes(); m++)
			{
				roadmanager::Lane* checkLane = laneSection->GetLaneByIdx(m);
				if (!checkLane) continue;

				int32 id = checkLane->GetId();
				if (id == 0) continue; // skip reference lane

				if (id < 0)
				{
					RightAllLaneIds.Add(id);
					if (checkLane->GetLaneType() == roadmanager::Lane::LaneType::LANE_TYPE_DRIVING)
					{
						RightDrivingLaneIds.Add(id);
					}
				}
				else // id > 0
				{
					LeftAllLaneIds.Add(id);
					if (checkLane->GetLaneType() == roadmanager::Lane::LaneType::LANE_TYPE_DRIVING)
					{
						LeftDrivingLaneIds.Add(id);
					}
				}
			}

			// Sort: closest to center first
			// Right: -1 > -2 > -3 (descending, so -1 is first = closest)
			RightAllLaneIds.Sort([](const int32& A, const int32& B) { return A > B; });
			RightDrivingLaneIds.Sort([](const int32& A, const int32& B) { return A > B; });
			// Left: 1 < 2 < 3 (ascending, so 1 is first = closest)
			LeftAllLaneIds.Sort();
			LeftDrivingLaneIds.Sort();

			// Build LaneIndexMap: LaneId -> Lane_N (1-based, all lane types)
			TMap<int32, int32> LaneIndexMap;
			for (int32 idx = 0; idx < RightAllLaneIds.Num(); idx++)
			{
				LaneIndexMap.Add(RightAllLaneIds[idx], idx + 1);
			}
			for (int32 idx = 0; idx < LeftAllLaneIds.Num(); idx++)
			{
				LaneIndexMap.Add(LeftAllLaneIds[idx], idx + 1);
			}

			// Build DrivingLaneIndexMap: LaneId -> DrivingN (1-based, driving only)
			TMap<int32, int32> DrivingLaneIndexMap;
			for (int32 idx = 0; idx < RightDrivingLaneIds.Num(); idx++)
			{
				DrivingLaneIndexMap.Add(RightDrivingLaneIds[idx], idx + 1);
			}
			for (int32 idx = 0; idx < LeftDrivingLaneIds.Num(); idx++)
			{
				DrivingLaneIndexMap.Add(LeftDrivingLaneIds[idx], idx + 1);
			}

			// Identify outermost/innermost IDs (all lane types)
			int32 OutermostRightLaneId = RightAllLaneIds.Num() > 0 ? RightAllLaneIds.Last() : 0;
			int32 OutermostLeftLaneId = LeftAllLaneIds.Num() > 0 ? LeftAllLaneIds.Last() : 0;
			int32 InnermostRightLaneId = RightAllLaneIds.Num() > 0 ? RightAllLaneIds[0] : 0;
			int32 InnermostLeftLaneId = LeftAllLaneIds.Num() > 0 ? LeftAllLaneIds[0] : 0;

			// Identify outermost/innermost IDs (driving lanes only)
			int32 OutermostRightDrivingId = RightDrivingLaneIds.Num() > 0 ? RightDrivingLaneIds.Last() : 0;
			int32 OutermostLeftDrivingId = LeftDrivingLaneIds.Num() > 0 ? LeftDrivingLaneIds.Last() : 0;
			int32 InnermostRightDrivingId = RightDrivingLaneIds.Num() > 0 ? RightDrivingLaneIds[0] : 0;
			int32 InnermostLeftDrivingId = LeftDrivingLaneIds.Num() > 0 ? LeftDrivingLaneIds[0] : 0;

			// ============================================================
			// Second Pass: Filter, spawn, and tag
			// ============================================================

			for (int k = 0; k < laneSection->GetNumberOfLanes(); k++)
			{
				lane = laneSection->GetLaneByIdx(k);
				if (!lane) continue;

				int32 LaneId = lane->GetId();

				// --- Side filter (skip reference lane from side filtering) ---
				if (LaneId > 0 && !bGenerateLeftLanes) continue;
				if (LaneId < 0 && !bGenerateRightLanes) continue;

				// --- Lane type filter ---
				bool bShouldGenerate = false;
				if (LaneId == 0)
				{
					bShouldGenerate = bGenerateReferenceLane;
				}
				else
				{
					switch (lane->GetLaneType())
					{
					case roadmanager::Lane::LaneType::LANE_TYPE_DRIVING:
						bShouldGenerate = bGenerateDrivingLane;
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

				// --- Lane position filter (for non-reference lanes) ---
				if (LaneId != 0 && LanePositionFilter != ELanePositionFilter::All)
				{
					bool bPassPositionFilter = true;

					switch (LanePositionFilter)
					{
					case ELanePositionFilter::OutermostOnly:
					{
						if (LaneId < 0 && LaneId != OutermostRightLaneId) bPassPositionFilter = false;
						if (LaneId > 0 && LaneId != OutermostLeftLaneId) bPassPositionFilter = false;
						break;
					}
					case ELanePositionFilter::OutermostDrivingOnly:
					{
						// Only applies to driving lanes; non-driving lanes are filtered out
						if (lane->GetLaneType() != roadmanager::Lane::LaneType::LANE_TYPE_DRIVING)
						{
							bPassPositionFilter = false;
						}
						else
						{
							if (LaneId < 0 && LaneId != OutermostRightDrivingId) bPassPositionFilter = false;
							if (LaneId > 0 && LaneId != OutermostLeftDrivingId) bPassPositionFilter = false;
						}
						break;
					}
					case ELanePositionFilter::InnermostOnly:
					{
						if (LaneId < 0 && LaneId != InnermostRightLaneId) bPassPositionFilter = false;
						if (LaneId > 0 && LaneId != InnermostLeftLaneId) bPassPositionFilter = false;
						break;
					}
					case ELanePositionFilter::InnermostDrivingOnly:
					{
						if (lane->GetLaneType() != roadmanager::Lane::LaneType::LANE_TYPE_DRIVING)
						{
							bPassPositionFilter = false;
						}
						else
						{
							if (LaneId < 0 && LaneId != InnermostRightDrivingId) bPassPositionFilter = false;
							if (LaneId > 0 && LaneId != InnermostLeftDrivingId) bPassPositionFilter = false;
						}
						break;
					}
					case ELanePositionFilter::SpecificIndex:
					{
						int32* FoundIndex = LaneIndexMap.Find(LaneId);
						if (!FoundIndex || *FoundIndex != SpecificLaneIndex) bPassPositionFilter = false;
						break;
					}
					default:
						break;
					}

					if (!bPassPositionFilter) continue;
				}

				// --- Spawn the spline actor ---
				AOpenDriveLaneSpline* newSpline = World->SpawnActor<AOpenDriveLaneSpline>(FVector::ZeroVector, FRotator::ZeroRotator, spawnParam);
				newSpline->Initialize(road, laneSection, lane, Offset, Step, SplineMode);

#if WITH_EDITOR
				newSpline->SetActorLabel(FString::Printf(TEXT("LaneSpline_Road%d_Lane%d"), RoadId, LaneId));

				// Folder organization by road
				FString FolderPath = FString::Printf(TEXT("OpenDriveSplines/Road_%d"), RoadId);
				newSpline->SetFolderPath(FName(*FolderPath));
#endif

				// === Auto-tagging ===

				// Road ID tag
				newSpline->Tags.Add(FName(*FString::Printf(TEXT("Road_%d"), RoadId)));

				// Side tag (L/R)
				if (LaneId > 0)
				{
					newSpline->Tags.Add(FName(TEXT("L")));
				}
				else if (LaneId < 0)
				{
					newSpline->Tags.Add(FName(TEXT("R")));
				}

				// Lane index tag (all lane types, 1-based from center)
				if (LaneId != 0)
				{
					int32* FoundLaneIndex = LaneIndexMap.Find(LaneId);
					if (FoundLaneIndex)
					{
						newSpline->Tags.Add(FName(*FString::Printf(TEXT("Lane_%d"), *FoundLaneIndex)));
					}

					// Outermost / Innermost tags (all lane types)
					bool bIsOutermost = (LaneId < 0 && LaneId == OutermostRightLaneId) ||
					                    (LaneId > 0 && LaneId == OutermostLeftLaneId);
					bool bIsInnermost = (LaneId < 0 && LaneId == InnermostRightLaneId) ||
					                    (LaneId > 0 && LaneId == InnermostLeftLaneId);

					if (bIsOutermost)
					{
						newSpline->Tags.Add(FName(TEXT("Outermost")));
					}
					if (bIsInnermost)
					{
						newSpline->Tags.Add(FName(TEXT("Innermost")));
					}
				}

				// Driving lane specific tags
				if (lane->GetLaneType() == roadmanager::Lane::LaneType::LANE_TYPE_DRIVING)
				{
					int32* FoundDrivingIndex = DrivingLaneIndexMap.Find(LaneId);
					if (FoundDrivingIndex)
					{
						newSpline->Tags.Add(FName(*FString::Printf(TEXT("Driving%d"), *FoundDrivingIndex)));

						// OutermostDriving / InnermostDriving tags
						bool bIsOutermostDriving = (LaneId < 0 && LaneId == OutermostRightDrivingId) ||
						                           (LaneId > 0 && LaneId == OutermostLeftDrivingId);
						bool bIsInnermostDriving = (LaneId < 0 && LaneId == InnermostRightDrivingId) ||
						                           (LaneId > 0 && LaneId == InnermostLeftDrivingId);

						if (bIsOutermostDriving)
						{
							newSpline->Tags.Add(FName(TEXT("OutermostDriving")));
						}
						if (bIsInnermostDriving)
						{
							newSpline->Tags.Add(FName(TEXT("InnermostDriving")));
						}
					}
				}

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
