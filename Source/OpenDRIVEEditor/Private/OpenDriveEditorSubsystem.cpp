#include "Public/OpenDriveEditorSubsystem.h"
#include "SignalTypeMapping.h"
#include "OpenDriveWorldSettings.h"
#include "OpenDriveAsset.h"

// ==========================================
// OpenDRIVE Asset Setup
// ==========================================

bool UOpenDriveEditorSubsystem::SetOpenDriveAsset(UOpenDriveAsset* Asset)
{
	if (!Asset)
	{
		UE_LOG(LogClass, Warning, TEXT("SetOpenDriveAsset: Asset is null"));
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		UE_LOG(LogClass, Warning, TEXT("SetOpenDriveAsset: No editor world"));
		return false;
	}

	AOpenDriveWorldSettings* WorldSettings = Cast<AOpenDriveWorldSettings>(World->GetWorldSettings());
	if (!WorldSettings)
	{
		UE_LOG(LogClass, Warning, TEXT("SetOpenDriveAsset: WorldSettings is not AOpenDriveWorldSettings. Check that WorldSettingsClass is set in Project Settings."));
		return false;
	}

	WorldSettings->Modify();
	WorldSettings->OpenDriveAsset = Asset;

	// Trigger property change to load the OpenDRIVE data into RoadManager
	FPropertyChangedEvent PropertyEvent(
		AOpenDriveWorldSettings::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(AOpenDriveWorldSettings, OpenDriveAsset))
	);
	WorldSettings->PostEditChangeProperty(PropertyEvent);

	UE_LOG(LogClass, Log, TEXT("SetOpenDriveAsset: Successfully set and loaded '%s'"), *Asset->GetName());
	return true;
}

UOpenDriveAsset* UOpenDriveEditorSubsystem::GetOpenDriveAsset()
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return nullptr;

	AOpenDriveWorldSettings* WorldSettings = Cast<AOpenDriveWorldSettings>(World->GetWorldSettings());
	if (!WorldSettings) return nullptr;

	return WorldSettings->OpenDriveAsset;
}

// ==========================================
// Spline Generation
// ==========================================

void UOpenDriveEditorSubsystem::GenerateLaneSplines()
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	SplineGen.GenerateLaneSplines(World);
}

void UOpenDriveEditorSubsystem::ClearGeneratedSplines()
{
	SplineGen.ClearGeneratedSplines();
}

void UOpenDriveEditorSubsystem::SetSplineOffset(float Offset)
{
	SplineGen.SetOffset(Offset);
}

void UOpenDriveEditorSubsystem::SetSplineStep(float Step)
{
	SplineGen.SetStep(Step);
}

void UOpenDriveEditorSubsystem::SetSplineMode(int32 Mode)
{
	switch (Mode)
	{
	case 0:
		SplineGen.SetSplineMode(AOpenDriveLaneSpline::Center);
		break;
	case 1:
		SplineGen.SetSplineMode(AOpenDriveLaneSpline::Inside);
		break;
	case 2:
		SplineGen.SetSplineMode(AOpenDriveLaneSpline::Outside);
		break;
	default:
		UE_LOG(LogClass, Warning, TEXT("SetSplineMode: Invalid mode %d. Use 0=Center, 1=Inside, 2=Outside."), Mode);
		break;
	}
}

void UOpenDriveEditorSubsystem::SetGenerateRoads(bool bGenerate)
{
	SplineGen.SetGenerateRoads(bGenerate);
}

void UOpenDriveEditorSubsystem::SetGenerateJunctions(bool bGenerate)
{
	SplineGen.SetGenerateJunctions(bGenerate);
}

void UOpenDriveEditorSubsystem::SetLaneTypeFilter(
	bool bDriving,
	bool bSidewalk,
	bool bBiking,
	bool bParking,
	bool bShoulder,
	bool bRestricted,
	bool bMedian,
	bool bOther,
	bool bReference)
{
	SplineGen.SetGenerateDrivingLane(bDriving);
	SplineGen.SetGenerateSidewalkLane(bSidewalk);
	SplineGen.SetGenerateBikingLane(bBiking);
	SplineGen.SetGenerateParkingLane(bParking);
	SplineGen.SetGenerateShoulderLane(bShoulder);
	SplineGen.SetGenerateRestrictedLane(bRestricted);
	SplineGen.SetGenerateMedianLane(bMedian);
	SplineGen.SetGenerateOtherLane(bOther);
	SplineGen.SetGenerateReferenceLane(bReference);
}

void UOpenDriveEditorSubsystem::SetGenerateOutermostDrivingLaneOnly(bool bOutermostOnly)
{
	SplineGen.SetGenerateOutermostDrivingLaneOnly(bOutermostOnly);
}

// ==========================================
// Signal Generation
// ==========================================

void UOpenDriveEditorSubsystem::GenerateSignals(USignalTypeMapping* MappingAsset)
{
	SignalGen.SetSignalTypeMappingAsset(MappingAsset);

	UWorld* World = GEditor->GetEditorWorldContext().World();
	SignalGen.GenerateSignals(World);
}

void UOpenDriveEditorSubsystem::ClearGeneratedSignals()
{
	SignalGen.ClearGeneratedSignals();
}

void UOpenDriveEditorSubsystem::SetFlipSignalOrientation(bool bFlip)
{
	SignalGen.SetFlipSignalOrientation(bFlip);
}
