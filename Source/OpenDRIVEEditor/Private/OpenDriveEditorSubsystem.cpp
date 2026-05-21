#include "Public/OpenDriveEditorSubsystem.h"
#include "SignalTypeMapping.h"
#include "OpenDriveWorldSettings.h"
#include "OpenDriveAsset.h"
#include "RoadMeshActor.h"
#include "RoadManager.hpp"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Engine/StaticMesh.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "PackageTools.h"
#include "UObject/SavePackage.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "GeometryScript/MeshAssetFunctions.h"

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

void UOpenDriveEditorSubsystem::SetGenerateLeftLanes(bool bGenerate)
{
	SplineGen.SetGenerateLeftLanes(bGenerate);
}

void UOpenDriveEditorSubsystem::SetGenerateRightLanes(bool bGenerate)
{
	SplineGen.SetGenerateRightLanes(bGenerate);
}

void UOpenDriveEditorSubsystem::SetLanePositionFilter(int32 FilterMode)
{
	switch (FilterMode)
	{
	case 0: SplineGen.SetLanePositionFilter(FSplineGenerator::ELanePositionFilter::All); break;
	case 1: SplineGen.SetLanePositionFilter(FSplineGenerator::ELanePositionFilter::OutermostOnly); break;
	case 2: SplineGen.SetLanePositionFilter(FSplineGenerator::ELanePositionFilter::OutermostDrivingOnly); break;
	case 3: SplineGen.SetLanePositionFilter(FSplineGenerator::ELanePositionFilter::InnermostOnly); break;
	case 4: SplineGen.SetLanePositionFilter(FSplineGenerator::ELanePositionFilter::InnermostDrivingOnly); break;
	case 5: SplineGen.SetLanePositionFilter(FSplineGenerator::ELanePositionFilter::SpecificIndex); break;
	default:
		UE_LOG(LogClass, Warning, TEXT("SetLanePositionFilter: Invalid mode %d. Use 0=All, 1=Outermost, 2=OutermostDriving, 3=Innermost, 4=InnermostDriving, 5=SpecificIndex."), FilterMode);
		break;
	}
}

void UOpenDriveEditorSubsystem::SetSpecificLaneIndex(int32 Index)
{
	SplineGen.SetSpecificLaneIndex(Index);
}

// ==========================================
// Road Mesh Generation
// ==========================================

bool UOpenDriveEditorSubsystem::LoadXodrFile(const FString& AbsoluteFilePath)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *AbsoluteFilePath))
	{
		UE_LOG(LogClass, Warning, TEXT("LoadXodrFile: failed to read '%s'"), *AbsoluteFilePath);
		return false;
	}

	roadmanager::OpenDrive* Odr = roadmanager::Position::GetOpenDrive();
	if (!Odr)
	{
		UE_LOG(LogClass, Warning, TEXT("LoadXodrFile: roadmanager OpenDrive instance is null"));
		return false;
	}

	const bool bOk = Odr->LoadOpenDriveContent(TCHAR_TO_UTF8(*Content));
	UE_LOG(LogClass, Log, TEXT("LoadXodrFile: '%s' -> %s (roads=%d)"),
		*AbsoluteFilePath,
		bOk ? TEXT("OK") : TEXT("FAILED"),
		(int)Odr->GetNumOfRoads());
	return bOk;
}

FString UOpenDriveEditorSubsystem::GenerateRoadMesh()
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	MeshGen.GenerateRoadMesh(World);
	return MeshGen.GetLastReport();
}

void UOpenDriveEditorSubsystem::ClearGeneratedRoadMeshes()
{
	MeshGen.ClearGeneratedMeshes();
}

void UOpenDriveEditorSubsystem::SetRoadMeshMaxStep(float MaxStepMeters)
{
	MeshGen.Settings.MaxStepMeters = MaxStepMeters;
}

void UOpenDriveEditorSubsystem::SetRoadMeshZOffset(float ZOffsetCm)
{
	MeshGen.Settings.ZOffsetCm = ZOffsetCm;
}

void UOpenDriveEditorSubsystem::SetRoadMeshMarkingZOffset(float MarkingZOffsetCm)
{
	MeshGen.Settings.MarkingZOffsetCm = MarkingZOffsetCm;
}

void UOpenDriveEditorSubsystem::SetRoadMeshMaterials(const TArray<UMaterialInterface*>& Materials)
{
	MeshGen.Settings.Materials = Materials;
}

void UOpenDriveEditorSubsystem::SetRoadMeshMaterialFolder(const FString& ContentPath)
{
	MeshGen.Settings.MaterialFolder = ContentPath;
}

void UOpenDriveEditorSubsystem::EnableRoadMeshCollision()
{
	MeshGen.EnableCollisionOnAll();
}

UStaticMesh* UOpenDriveEditorSubsystem::BakeRoadMeshToStaticMesh(const FString& AssetPath)
{
	ARoadMeshActor* Actor = MeshGen.GetLatestActor();
	if (!Actor || !Actor->MeshComp)
	{
		UE_LOG(LogClass, Warning, TEXT("BakeRoadMeshToStaticMesh: no generated actor"));
		return nullptr;
	}
	UDynamicMesh* DM = Actor->MeshComp->GetDynamicMesh();
	if (!DM)
	{
		UE_LOG(LogClass, Warning, TEXT("BakeRoadMeshToStaticMesh: actor has no DynamicMesh"));
		return nullptr;
	}

	const FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	const FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);
	if (PackagePath.IsEmpty() || AssetName.IsEmpty())
	{
		UE_LOG(LogClass, Warning, TEXT("BakeRoadMeshToStaticMesh: invalid AssetPath '%s'"), *AssetPath);
		return nullptr;
	}

	UPackage* Package = CreatePackage(*AssetPath);
	if (!Package)
	{
		UE_LOG(LogClass, Warning, TEXT("BakeRoadMeshToStaticMesh: CreatePackage failed for '%s'"), *AssetPath);
		return nullptr;
	}
	Package->FullyLoad();

	UStaticMesh* SM = NewObject<UStaticMesh>(Package, *AssetName, RF_Public | RF_Standalone);
	if (!SM)
	{
		UE_LOG(LogClass, Warning, TEXT("BakeRoadMeshToStaticMesh: NewObject<UStaticMesh> failed"));
		return nullptr;
	}

	FGeometryScriptCopyMeshToAssetOptions Opt;
	Opt.bEnableRecomputeNormals = true;
	Opt.bEnableRecomputeTangents = true;
	Opt.bReplaceMaterials = true;
	Opt.NewMaterials = Actor->DefaultMaterials;
	Opt.NewMaterialSlotNames.SetNum(Actor->DefaultMaterials.Num());
	{
		static const TCHAR* kSlotNames[] = { TEXT("Asphalt"), TEXT("Sidewalk"), TEXT("Border"), TEXT("Marking"), TEXT("Misc") };
		for (int32 i = 0; i < Opt.NewMaterialSlotNames.Num(); ++i)
		{
			Opt.NewMaterialSlotNames[i] = (i < UE_ARRAY_COUNT(kSlotNames)) ? FName(kSlotNames[i]) : *FString::Printf(TEXT("Slot_%d"), i);
		}
	}
	Opt.bApplyNaniteSettings = false;

	FGeometryScriptMeshWriteLOD WriteLOD;
	WriteLOD.LODIndex = 0;

	EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
	UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(
		DM, SM, Opt, WriteLOD, Outcome, nullptr);

	if (Outcome != EGeometryScriptOutcomePins::Success)
	{
		UE_LOG(LogClass, Warning, TEXT("BakeRoadMeshToStaticMesh: CopyMeshToStaticMesh outcome=%d"), (int32)Outcome);
		return nullptr;
	}

	// Generate lightmap UVs + rebuild so the baked mesh lights correctly when placed
	// as a Static actor (the original DynamicMesh had none, which is why it rendered dark).
	if (SM->GetNumSourceModels() > 0)
	{
		FStaticMeshSourceModel& SrcModel = SM->GetSourceModel(0);
		SrcModel.BuildSettings.bGenerateLightmapUVs = true;
		SrcModel.BuildSettings.SrcLightmapIndex = 0;
		SrcModel.BuildSettings.DstLightmapIndex = 1;
		SM->SetLightMapCoordinateIndex(1);
		SM->Build(/*bSilent=*/true);
	}

	SM->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(SM);

	// Persist the asset to disk so it survives editor restarts (the "save" half of the request).
	const FString FileName = FPackageName::LongPackageNameToFilename(
		AssetPath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	const bool bSaved = UPackage::SavePackage(Package, SM, *FileName, SaveArgs);
	UE_LOG(LogClass, Log, TEXT("BakeRoadMeshToStaticMesh: created '%s' (saved=%d -> %s)"),
		*SM->GetPathName(), bSaved ? 1 : 0, *FileName);
	return SM;
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
