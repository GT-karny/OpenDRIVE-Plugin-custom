#include "Public/EditorMode/OpenDriveEditorMode.h"
#include "OpenDriveLaneSpline.h"
#include "Public/OpenDriveEditor.h"
#include "Toolkits/ToolkitManager.h"
#include "ScopedTransaction.h"
#include "Public/EditorMode/OpenDriveEditorToolkit.h"
#include "RoadManager.hpp"
#include "CoordTranslate.h"

const FEditorModeID FOpenDRIVEEditorMode::EM_RoadMode(TEXT("EM_RoadMode"));

FOpenDRIVEEditorMode::FOpenDRIVEEditorMode()
{
	UE_LOG(LogClass, Warning, TEXT("Custom editor mode constructor called"));

	FEdMode::FEdMode();
	MapOpenedDelegateHandle = FEditorDelegates::MapChange.AddRaw(this, &FOpenDRIVEEditorMode::OnMapOpenedCallback);
	OnActorSelectedHandle = USelection::SelectObjectEvent.AddRaw(this, &FOpenDRIVEEditorMode::OnActorSelected);
}

void FOpenDRIVEEditorMode::Enter()
{
	UE_LOG(LogClass, Warning, TEXT("Enter"));

	FEdMode::Enter();

	bIsMapOpening = false;

	if (!Toolkit.IsValid())
	{
		Toolkit = MakeShareable(new FOpenDRIVEEditorModeToolkit);
		Toolkit->Init(Owner->GetToolkitHost());
	}
	
	if (bHasBeenLoaded == false	&& (GEditor->IsSimulateInEditorInProgress() == false && GEditor->IsPlaySessionInProgress() == false))
	{
		LoadRoadsNetwork();
	}
	else
	{
		SetRoadsVisibilityInEditor(false);
	}
}

void FOpenDRIVEEditorMode::Exit()
{
	FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
	Toolkit.Reset();
	
	if (bIsMapOpening == false) //prevents the function's call in case of level change 
	{
		SetRoadsVisibilityInEditor(true);
		SetRoadsArrowsVisibilityInEditor(false);
	}

	FEdMode::Exit();
}

void FOpenDRIVEEditorMode::ResetRoadsArray()
{
	for (auto road : roadsArray)
	{
		if (IsValid(road) == true)
		{
			road->Destroy();
		}
	}
	roadsArray.Reset();
	bHasBeenLoaded = false;
}

void FOpenDRIVEEditorMode::Generate()
{
	LoadRoadsNetwork();
}

FOpenDRIVEEditorMode::~FOpenDRIVEEditorMode()
{
	FEditorDelegates::OnMapOpened.Remove(MapOpenedDelegateHandle);
	USelection::SelectObjectEvent.Remove(OnActorSelectedHandle);
}

void FOpenDRIVEEditorMode::OnMapOpenedCallback(uint32 type)
{
	if (type == MapChangeEventFlags::NewMap)
	{
		UE_LOG(LogClass, Warning, TEXT("a new map has been opened"));

		roadsArray.Reset();
		bIsMapOpening = true;
		bHasBeenLoaded = false;
	}
}

void FOpenDRIVEEditorMode::LoadRoadsNetwork()
{
	// empty the array if needed

	if (roadsArray.IsEmpty() == false)
	{
		ResetRoadsArray();
	}
	
	// roadmanager params
	roadmanager::OpenDrive* Odr = roadmanager::Position::GetOpenDrive();
	roadmanager::Road* road = 0;
	roadmanager::LaneSection* laneSection = 0;
	roadmanager::Lane* lane = 0;
	size_t nrOfRoads = Odr->GetNumOfRoads();
	
	// Actor spawn params
	FActorSpawnParameters spawnParam;
	spawnParam.bHideFromSceneOutliner = true;
	spawnParam.bTemporaryEditorActor = true;

	for (int i = 0; i < (int)nrOfRoads; i++)
	{
		road = Odr->GetRoadByIdx(i);
		if (!road) continue;

		for (int j = 0; j < road->GetNumberOfLaneSections(); j++)
		{
			laneSection = road->GetLaneSectionByIdx(j);

			if (!laneSection) continue;

			for (int k = 0; k < laneSection->GetNumberOfLanes(); k++)
			{
				lane = laneSection->GetLaneByIdx(k);

				if (!lane || lane->GetId() == 0) continue;

				AOpenDriveEditorLane* newRoad = GetWorld()->SpawnActor<AOpenDriveEditorLane>(FVector::ZeroVector, FRotator::ZeroRotator, spawnParam);
				newRoad->SetActorHiddenInGame(true);
				newRoad->Initialize(road, laneSection, lane, _roadOffset, _step);
				roadsArray.Add(newRoad);
			}
		}
	}
	bHasBeenLoaded = true;
}

void FOpenDRIVEEditorMode::GenerateLaneSplines()
{
	// roadmanager params
	roadmanager::OpenDrive* Odr = roadmanager::Position::GetOpenDrive();
	roadmanager::Road* road = 0;
	roadmanager::LaneSection* laneSection = 0;
	roadmanager::Lane* lane = 0;
	size_t nrOfRoads = Odr->GetNumOfRoads();

	// Actor spawn params
	FActorSpawnParameters spawnParam;
	spawnParam.bHideFromSceneOutliner = false; // Persistent actors should be visible
	spawnParam.bTemporaryEditorActor = false; // Persistent

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
						if (id < 0) // Right lane (ID is negative, outermost is smallest value e.g. -3)
						{
							if (minRightDrivingId == 0 || id < minRightDrivingId) minRightDrivingId = id;
						}
						else if (id > 0) // Left lane (ID is positive, outermost is largest value e.g. 3)
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
				// Note: We include ID 0 (center lane) as per requirement

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


				AOpenDriveLaneSpline* newSpline = GetWorld()->SpawnActor<AOpenDriveLaneSpline>(FVector::ZeroVector, FRotator::ZeroRotator, spawnParam);
				newSpline->Initialize(road, laneSection, lane, _roadOffset, _step, GetSplineGenerationMode());
#if WITH_EDITOR
				newSpline->SetActorLabel(FString::Printf(TEXT("LaneSpline_Road%d_Lane%d"), road->GetId(), lane->GetId()));
#endif
			}
		}
	}
}

void FOpenDRIVEEditorMode::SetRoadsVisibilityInEditor(bool bIsVisible)
{
	if (roadsArray.IsEmpty() == false)
	{
		for (AOpenDriveEditorLane* road : roadsArray)
		{
			road->SetIsTemporarilyHiddenInEditor(bIsVisible);
		}
	}
}

void FOpenDRIVEEditorMode::SetRoadsArrowsVisibilityInEditor(bool bIsVisible)
{
	if (roadsArray.IsEmpty() == false)
	{
		for (AOpenDriveEditorLane* road : roadsArray)
		{
			road->SetArrowVisibility(bIsVisible);
		}
	}
}

void FOpenDRIVEEditorMode::OnActorSelected(UObject* selectedObject)
{
	AOpenDriveEditorLane* selectedRoad = Cast<AOpenDriveEditorLane>(selectedObject);

	if (IsValid(selectedRoad) == true)
	{
		UE_LOG(LogClass, Warning, TEXT("road selected"));

		TSharedPtr<FOpenDRIVEEditorModeToolkit> openDRIVEEdToolkit = StaticCastSharedPtr<FOpenDRIVEEditorModeToolkit>(Toolkit);

		if (openDRIVEEdToolkit.IsValid())
		{
			TSharedPtr<SOpenDRIVEEditorModeWidget> openDRIVEEdWidget = StaticCastSharedPtr<SOpenDRIVEEditorModeWidget>(openDRIVEEdToolkit->GetInlineContent());

			if (openDRIVEEdWidget.IsValid())
			{
				openDRIVEEdWidget->UpdateLaneInfo(selectedRoad);
			}
		}
	}
}

