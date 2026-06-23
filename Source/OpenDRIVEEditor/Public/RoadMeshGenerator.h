// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RoadMeshSettings.h"

class ARoadMeshActor;
class UWorld;

namespace roadmanager
{
	class Road;
	class LaneSection;
	class Lane;
	class OpenDrive;
	class Junction;
}

/**
 * Generator for OpenDRIVE road meshes using GeometryScript / DynamicMesh.
 * Pattern parallels FSplineGenerator: held by UOpenDriveEditorSubsystem,
 * Generate / Clear from BP/Python.
 */
class OPENDRIVEEDITOR_API FRoadMeshGenerator
{
public:
	/** Generate the road mesh actor and populate its DynamicMesh from the loaded OpenDRIVE. */
	void GenerateRoadMesh(UWorld* World);

	/** Destroy any previously generated road mesh actors. */
	void ClearGeneratedMeshes();

	FRoadMeshSettings Settings;

	/** Returns a 1-line diagnostic about the most recent Generate call (TriCount/VertCount/Materials). */
	const FString& GetLastReport() const { return LastReport; }

	/** The most recently spawned road mesh actor (for collision / bake operations). */
	ARoadMeshActor* GetLatestActor() const;

	/** Enable collision on every generated actor. */
	void EnableCollisionOnAll();

private:
	// The road-surface / curb / junction-fill / deck geometry now lives in the
	// runtime OpenDRIVEMesh:: core (Source/OpenDRIVE/Public/OpenDRIVEMeshMath.h);
	// GenerateRoadMesh orchestrates it. No per-lane geometry methods here anymore.

	/** Spawned actors that we own. */
	TArray<TWeakObjectPtr<ARoadMeshActor>> GeneratedActors;

	FString LastReport;
};
