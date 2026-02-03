#include "Public/EditorMode/OpenDriveEditorMode.h"
#include "OpenDriveLaneSpline.h"
#include "Public/OpenDriveEditor.h"
#include "Toolkits/ToolkitManager.h"
#include "ScopedTransaction.h"
#include "Public/EditorMode/OpenDriveEditorToolkit.h"
#include "RoadManager.hpp"
#include "SignalInfoComponent.h"
#include "SignalTypeMapping.h"
#include "CoordTranslate.h"
#include "GT_esminiRMLib.hpp"

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

void FOpenDRIVEEditorMode::GenerateSignals()
{
	// Clear existing signals first
	ClearGeneratedSignals();

	if (!bGenerateSignals)
	{
		return;
	}

	// Get number of roads
	int NumRoads = GT_RM_GetNumRoads();
	if (NumRoads <= 0)
	{
		UE_LOG(LogClass, Warning, TEXT("GenerateSignals: No roads found or GT_RM not initialized"));
		return;
	}

	// Actor spawn parameters
	FActorSpawnParameters SpawnParams;
	SpawnParams.bHideFromSceneOutliner = false;
	SpawnParams.bTemporaryEditorActor = false;

	int32 TotalSignalsSpawned = 0;

	// Iterate all roads
	for (int RoadIdx = 0; RoadIdx < NumRoads; RoadIdx++)
	{
		uint32_t RoadId = GT_RM_GetRoadIdByIndex(RoadIdx);
		if (RoadId == 0xFFFFFFFF) continue;

		int SignalCount = GT_RM_GetRoadSignalCount(RoadId);
		if (SignalCount <= 0) continue;

		// Iterate all signals on this road
		for (int SignalIdx = 0; SignalIdx < SignalCount; SignalIdx++)
		{
			GT_RM_RoadSignalInfo SignalInfo;
			int Result = GT_RM_GetRoadSignal(RoadId, SignalIdx, &SignalInfo);
			if (Result != 0) continue;

			// Determine actor class to spawn
			TSubclassOf<AActor> ActorClass = nullptr;
			if (SignalTypeMappingAsset)
			{
				ActorClass = SignalTypeMappingAsset->FindActorClassForSignal(
					FString(UTF8_TO_TCHAR(SignalInfo.type)),
					FString(UTF8_TO_TCHAR(SignalInfo.subtype)),
					FString(UTF8_TO_TCHAR(SignalInfo.country))
				);
			}

			if (!ActorClass)
			{
				UE_LOG(LogClass, Warning, TEXT("GenerateSignals: No actor class for signal type=%s subtype=%s (Road %d, Signal %d)"),
					UTF8_TO_TCHAR(SignalInfo.type), UTF8_TO_TCHAR(SignalInfo.subtype), RoadId, SignalInfo.id);
				continue;
			}

			// Convert coordinates: GT_RM provides x,y,z in meters (OpenDRIVE coordinate system)
			// Need to convert to Unreal coordinate system (cm, left-handed)
			FVector Location(
				SignalInfo.x * 100.0,   // meters to cm
				-SignalInfo.y * 100.0,  // Y negated for left-handed coordinate system
				SignalInfo.z * 100.0    // meters to cm
			);

			// Convert rotation: h (heading), p (pitch), r (roll) in radians
			FRotator Rotation(
				FMath::RadiansToDegrees(SignalInfo.p),   // Pitch
				FMath::RadiansToDegrees(-SignalInfo.h),  // Yaw (negated for UE coordinate system)
				FMath::RadiansToDegrees(SignalInfo.r)    // Roll
			);

			FTransform SpawnTransform(Rotation, Location);

			// Spawn the actor
			AActor* SignalActor = GetWorld()->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParams);
			if (!SignalActor) continue;

			// Add SignalInfoComponent and populate it
			USignalInfoComponent* InfoComp = NewObject<USignalInfoComponent>(SignalActor, NAME_None, RF_Transactional);
			InfoComp->SignalId = SignalInfo.id;
			InfoComp->RoadId = static_cast<int32>(RoadId);
			InfoComp->S = SignalInfo.s;
			InfoComp->T = SignalInfo.t;
			InfoComp->Type = FString(UTF8_TO_TCHAR(SignalInfo.type));
			InfoComp->SubType = FString(UTF8_TO_TCHAR(SignalInfo.subtype));
			InfoComp->Country = FString(UTF8_TO_TCHAR(SignalInfo.country));
			InfoComp->Value = SignalInfo.value;
			InfoComp->Unit = FString(UTF8_TO_TCHAR(SignalInfo.unit));
			InfoComp->Text = FString(UTF8_TO_TCHAR(SignalInfo.text));
			InfoComp->bIsDynamic = SignalInfo.isDynamic;
			InfoComp->Height = SignalInfo.height;
			InfoComp->Width = SignalInfo.width;
			InfoComp->RegisterComponent();
			SignalActor->AddInstanceComponent(InfoComp);

#if WITH_EDITOR
			// Organize in editor folder
			FString FolderPath = FString::Printf(TEXT("Signals/Road_%d"), RoadId);
			SignalActor->SetFolderPath(FName(*FolderPath));

			// Set actor label
			FString Label = FString::Printf(TEXT("Signal_%d_%s"), SignalInfo.id, *InfoComp->Type);
			SignalActor->SetActorLabel(Label);
#endif

			GeneratedSignals.Add(SignalActor);
			TotalSignalsSpawned++;
		}
	}

	UE_LOG(LogClass, Log, TEXT("GenerateSignals: Spawned %d signals"), TotalSignalsSpawned);
}

void FOpenDRIVEEditorMode::ClearGeneratedSignals()
{
	for (AActor* Signal : GeneratedSignals)
	{
		if (IsValid(Signal))
		{
			Signal->Destroy();
		}
	}
	GeneratedSignals.Empty();
}
