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
	/** Build s-list for a lane section using a fixed-step sampler. */
	TArray<double> BuildSList(roadmanager::Road* Road, roadmanager::LaneSection* Sec, double SectionStartS) const;

	/** Build mesh buffers for one lane strip (between inner-edge and outer-edge along SList). */
	bool BuildLaneStripBuffers(
		roadmanager::Road* Road,
		roadmanager::LaneSection* Sec,
		roadmanager::Lane* Lane,
		const TArray<double>& SList,
		struct FGeometryScriptSimpleMeshBuffers& OutBuffers,
		int32& OutMaterialID) const;

	/** Append road marking strips for one lane into the given UDynamicMesh. */
	void AppendRoadMarksForLane(
		class UDynamicMesh* Mesh,
		roadmanager::Road* Road,
		roadmanager::LaneSection* Sec,
		roadmanager::Lane* Lane,
		double SectionStartS,
		double SectionLength,
		int32& OutMarkTriCount) const;

	/** Build a fan-triangulated asphalt patch that covers a junction's interior.
	 *  Boundary = outermost Driving-lane edges of each incoming road at its
	 *  junction-facing s. Returns false if the junction is degenerate
	 *  (no incoming roads, no driving lanes, or only one gate). */
	bool BuildJunctionFillBuffers(
		roadmanager::Junction* Junction,
		struct FGeometryScriptSimpleMeshBuffers& OutBuffers,
		int32& OutMaterialID) const;

	/** Spawned actors that we own. */
	TArray<TWeakObjectPtr<ARoadMeshActor>> GeneratedActors;

	FString LastReport;
};
