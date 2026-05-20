#include "Public/EditorMode/OpenDriveEditorMode.h"
#include "OpenDriveLaneSpline.h"
#include "Public/OpenDriveEditor.h"
#include "Toolkits/ToolkitManager.h"
#include "ScopedTransaction.h"
#include "Public/EditorMode/OpenDriveEditorToolkit.h"
#include "RoadManager.hpp"
#include "CoordTranslate.h"

#include "OpenDrive2Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeLayerInfoObject.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY_STATIC(LogOpenDriveLandscapeIntegration, Log, All);

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
	// Sync offset/step from editor mode (shared with preview) to SplineGenerator
	SplineGenerator.SetOffset(_roadOffset);
	SplineGenerator.SetStep(_step);
	SplineGenerator.GenerateLaneSplines(GetWorld());
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

namespace
{
	// Pop a transient editor notification (top-right) so the user sees the result.
	void Notify(const FString& Msg, bool bSuccess)
	{
		FNotificationInfo Info(FText::FromString(Msg));
		Info.ExpireDuration = 4.f;
		Info.bUseSuccessFailIcons = true;
		TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
		if (Item.IsValid())
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}

	// Load and instantiate the BP utility EUBP_OpenDrive2Landscape so we can call its
	// SculptLandscape (which dispatches the BP-implemented ApplySpline).
	UOpenDrive2Landscape* MakeLandscapeUtilityInstance()
	{
		// Plugin Content mount: /OpenDRIVE/. BP-generated class suffix: _C.
		static const TCHAR* BPClassPath = TEXT("/OpenDRIVE/EUBP_OpenDrive2Landscape.EUBP_OpenDrive2Landscape_C");
		UClass* BPClass = LoadClass<UOpenDrive2Landscape>(nullptr, BPClassPath);
		if (!BPClass)
		{
			UE_LOG(LogOpenDriveLandscapeIntegration, Warning,
				TEXT("Could not load BP class '%s'. Plugin Content may be missing."), BPClassPath);
			return nullptr;
		}
		return NewObject<UOpenDrive2Landscape>(GetTransientPackage(), BPClass);
	}

	// Returns true if at least one Landscape is currently selected in the level editor.
	bool HasLandscapeSelected()
	{
		if (!GEditor) return false;
		if (USelection* Sel = GEditor->GetSelectedActors())
		{
			for (FSelectionIterator It(*Sel); It; ++It)
			{
				if (Cast<ALandscapeProxy>(*It)) return true;
			}
		}
		return false;
	}
}

void FOpenDRIVEEditorMode::SetLandscapePaintLayer(ULandscapeLayerInfoObject* V)
{
	_landscapePaintLayer = V;
}

ULandscapeLayerInfoObject* FOpenDRIVEEditorMode::GetLandscapePaintLayer() const
{
	return _landscapePaintLayer.Get();
}

void FOpenDRIVEEditorMode::LandscapeSculptSelected()
{
	if (!HasLandscapeSelected())
	{
		Notify(TEXT("Select a Landscape actor in the level first."), false);
		return;
	}

	UOpenDrive2Landscape* Util = MakeLandscapeUtilityInstance();
	if (!Util)
	{
		Notify(TEXT("Failed to load EUBP_OpenDrive2Landscape. Is the plugin Content present?"), false);
		return;
	}

	Util->SculptLandscape(_landscapeZOffset, _landscapeFalloff, _landscapePaintLayer.Get(), _landscapeLayerName);
	Notify(TEXT("Sculpted Landscape from OpenDRIVE roads."), true);
}

void FOpenDRIVEEditorMode::LandscapeCreateSplinesOnSelected()
{
	if (!HasLandscapeSelected())
	{
		Notify(TEXT("Select a Landscape actor in the level first."), false);
		return;
	}

	UOpenDrive2Landscape* Util = MakeLandscapeUtilityInstance();
	if (!Util)
	{
		Notify(TEXT("Failed to load EUBP_OpenDrive2Landscape. Is the plugin Content present?"), false);
		return;
	}

	Util->CreateRoadSplines(_landscapeZOffset, _landscapeFalloff, _landscapePaintLayer.Get(), _landscapeLayerName);
	Notify(TEXT("Created Landscape Splines from OpenDRIVE roads."), true);
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

