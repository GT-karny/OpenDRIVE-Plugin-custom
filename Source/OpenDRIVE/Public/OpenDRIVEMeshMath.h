// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// FGeometryScriptSimpleMeshBuffers is a USTRUCT defined in
// GeometryScript/MeshBasicEditFunctions.h. We only take it by reference here,
// so a forward declaration keeps this header light; callers include the real
// header to populate / append the buffers.
struct FGeometryScriptSimpleMeshBuffers;

class UOpenDriveAsset;
class UDynamicMesh;

namespace roadmanager
{
	class Road;
	class LaneSection;
	class Lane;
	class Junction;
}

/**
 * Shared OpenDRIVE -> mesh geometry helpers: the single source of truth for the
 * road-surface, curb, junction-fill and elevated-deck geometry. Lives in the
 * Runtime OpenDRIVE module and is exported (OPENDRIVE_API) so the editor-side
 * FRoadMeshGenerator -- and any future runtime / PCG consumer -- can drive it
 * without re-implementing the math.
 *
 * The functions are pure geometry: they read the process-global roadmanager
 * singleton and emit FGeometryScriptSimpleMeshBuffers / append into a
 * UDynamicMesh. No PCG, no actor, no asset-registry dependency.
 */
namespace OpenDRIVEMesh
{
	/**
	 * Material slot indices for the assembled road mesh. Numerically mirrors the
	 * editor generator's ERoadMeshMaterialSlot so the two agree on slot order
	 * (a static_assert in RoadMeshSettings.cpp guards against drift).
	 */
	enum class ERoadMatSlot : uint8
	{
		Asphalt  = 0,
		Sidewalk = 1,
		Border   = 2,
		Marking  = 3,
		Misc     = 4,
		Structure = 5, // elevated deck slab / fascia-parapet / piers (UI calls this "Deck")
		Count    = 6
	};

	/**
	 * Load the asset's xodr into the process-global roadmanager singleton,
	 * guarded so identical content is not re-parsed by repeated/concurrent
	 * executions. Must be called on the game thread. (The editor generator
	 * usually has the singleton already loaded via WorldSettings; this exists
	 * for runtime / PCG consumers that drive the core directly.)
	 */
	bool OPENDRIVE_API EnsureLoaded(const UOpenDriveAsset* Asset);

	/** Sample a single (s, t) position on a road into a UE world-space FVector (in cm). */
	FVector OPENDRIVE_API EvalLanePoint(roadmanager::Road* Road, double s, double tOffset, double ZOffsetCm);

	/**
	 * Flip each triangle's winding so its UE front face points up. Road surfaces
	 * are walked/driven on, so the top face should always be the front face.
	 * (UE is left-handed; the front face points up exactly when the textbook
	 * cross product's Z is negative, so we flip when it is positive.)
	 */
	void OPENDRIVE_API OrientTrianglesUp(FGeometryScriptSimpleMeshBuffers& Buf);

	/**
	 * Curvature-adaptive arc-length sample list for a lane section: union of the
	 * OSI s-values from every lane (esmini pre-samples those with curvature
	 * awareness), with gaps larger than MaxStep split and samples closer than
	 * MinStep coalesced.
	 */
	TArray<double> OPENDRIVE_API BuildSList(
		roadmanager::Road* Road,
		roadmanager::LaneSection* Sec,
		double SectionStartS,
		double MaxStepMeters,
		double MinStepMeters);

	/**
	 * Whole-road version of BuildSList: unions the OSI s-values from every lane in every
	 * section of the road (plus every section boundary, which is a width discontinuity),
	 * caps gaps larger than MaxStep and coalesces samples closer than MinStep. Returns a
	 * monotonically increasing s-list spanning [0, Road->GetLength()].
	 */
	TArray<double> OPENDRIVE_API BuildRoadSListAllSections(
		roadmanager::Road* Road,
		double MaxStepMeters,
		double MinStepMeters);

	/** Map a roadmanager::Lane::LaneType bitfield to an ERoadMatSlot (as int32). */
	int32 OPENDRIVE_API SlotForLaneType(int32 LaneTypeFlag);

	/**
	 * Raised-curb elevation (cm) for a lane top: sidewalk / border / curb lanes
	 * are lifted by CurbHeightCm; every other lane stays at road level (0).
	 * Returns 0 when CurbHeightCm <= 0 (the step is then fully disabled).
	 */
	double OPENDRIVE_API CurbTopZAddCm(roadmanager::Lane* Lane, double CurbHeightCm);

	/** True when this Road is a "connecting road" inside a junction (<road junction="N"> with N >= 0). */
	bool OPENDRIVE_API IsConnectingRoad(roadmanager::Road* R);

	/** True only for LANE_TYPE_DRIVING. */
	bool OPENDRIVE_API IsDrivingLane(int32 LaneType);

	/**
	 * Build a closed XY ring (UE world cm; Z carried from the road surface) hugging a
	 * road's outermost edges across its length, counting only lanes whose type bit is
	 * set in LaneTypeMask (a bitwise OR of roadmanager::Lane::LaneType flags; e.g.
	 * LANE_TYPE_DRIVING for the drivable area). The reference line (lane 0) is always
	 * excluded. Left edge forward + right edge reversed; the first point is NOT repeated.
	 * Returns false if fewer than 3 ring points result.
	 */
	bool OPENDRIVE_API BuildRoadSurfaceRing(
		roadmanager::Road* Road,
		double SampleStepMeters,
		int32 LaneTypeMask,
		TArray<FVector>& OutRing);

	/**
	 * Build a quad-strip ribbon for a single lane between its inner and outer
	 * t-offsets across SList, with per-vertex up-facing normals and s/t UVs.
	 * TopZAddCm lifts both edges by the same amount (a flat raised step, e.g. a
	 * sidewalk surface); pass 0 for a road-level lane.
	 * Returns false for the reference line (id 0) or a zero-width lane.
	 * Caller assigns the material slot (see SlotForLaneType).
	 */
	bool OPENDRIVE_API BuildLaneSurfaceStripBuffers(
		roadmanager::Road* Road,
		roadmanager::LaneSection* Sec,
		roadmanager::Lane* Lane,
		const TArray<double>& SList,
		double ZOffsetCm,
		double TopZAddCm,
		FGeometryScriptSimpleMeshBuffers& OutBuffers);

	/**
	 * Build a vertical curb-face (riser) quad strip along the lane's INNER edge,
	 * spanning Z from (ZOffsetCm + FromZAddCm) to (ZOffsetCm + ToZAddCm). Used to
	 * close the elevation step between a road-level lane and a raised sidewalk/curb.
	 * Normals are horizontal (road-facing). Returns false if there is no step or
	 * fewer than 2 samples.
	 */
	bool OPENDRIVE_API BuildCurbRiserBuffers(
		roadmanager::Road* Road,
		roadmanager::LaneSection* Sec,
		roadmanager::Lane* Lane,
		const TArray<double>& SList,
		double ZOffsetCm,
		double FromZAddCm,
		double ToZAddCm,
		FGeometryScriptSimpleMeshBuffers& OutBuffers);

	/**
	 * Append every road mark on a lane (centerline lane 0 included) onto Mesh as
	 * thin strips straddling the lane's outer edge, lifted by MarkingZOffsetCm
	 * above the road to avoid z-fighting. Strips are appended with MarkingSlot as
	 * the material id. Adds the emitted triangle count to OutMarkTriCount.
	 */
	void OPENDRIVE_API AppendRoadMarksForLane(
		UDynamicMesh* Mesh,
		roadmanager::Road* Road,
		roadmanager::LaneSection* Sec,
		roadmanager::Lane* Lane,
		double SectionStartS,
		double SectionLength,
		double ZOffsetCm,
		double MarkingZOffsetCm,
		double MaxStepMeters,
		int32 MarkingSlot,
		int32& OutMarkTriCount);

	/**
	 * Build the closed outline ring of a junction's asphalt fill, bounded by the
	 * outermost driving edges of the incoming roads and the corner-hugging connecting
	 * roads (the same outline BuildJunctionFillBuffers triangulates). OutLoop is an
	 * ordered XY ring (UE cm). Returns false if the junction has too few arms.
	 */
	bool OPENDRIVE_API BuildJunctionFillRing(
		roadmanager::Junction* Junction,
		double ZOffsetCm,
		TArray<FVector>& OutLoop);

	/**
	 * Build a single asphalt fill patch for a junction, bounded by the outermost
	 * driving edges of the incoming roads and the corner-hugging connecting roads,
	 * triangulated by ear clipping. Returns false if the junction has too few arms.
	 */
	bool OPENDRIVE_API BuildJunctionFillBuffers(
		roadmanager::Junction* Junction,
		double ZOffsetCm,
		FGeometryScriptSimpleMeshBuffers& OutBuffers);

	// ---- Elevated deck structure ------------------------------------------------------

	/**
	 * Sample the road's left (Hi-t) and right (Lo-t) OUTERMOST cross-section edges at each s in
	 * SList, with Z carried from the road surface (+ ZOffsetCm). Spans the full paved width
	 * (every lane except the reference line, all lane types). OutLeft / OutRight are parallel and
	 * s-ascending (a sample with no lanes is skipped from both). Returns false if < 2 samples.
	 * Used as the boundary for the deck slab / fascia of an elevated span.
	 */
	bool OPENDRIVE_API BuildRoadEdgePolylines(
		roadmanager::Road* Road,
		const TArray<double>& SList,
		double ZOffsetCm,
		TArray<FVector>& OutLeft,
		TArray<FVector>& OutRight);

	/**
	 * Deck slab from parallel top-edge polylines (the road surface outer edges): the underside
	 * (top edges dropped by ThicknessCm, faces down), the two outward-facing side fascia walls,
	 * and the two end caps. The top face is the existing lane mesh and is NOT re-emitted here.
	 */
	void OPENDRIVE_API BuildDeckBuffers(
		const TArray<FVector>& Left,
		const TArray<FVector>& Right,
		double ThicknessCm,
		FGeometryScriptSimpleMeshBuffers& OutBuffers);

	/**
	 * Parapet (barrier) walls standing on the deck edges: each a thin wall of height HeightCm and
	 * thickness ThicknessCm (outer face flush with the edge, inner face offset toward the road
	 * centre). Inward direction at each station is derived from the opposite edge. A wall segment
	 * [i,i+1] is emitted only where BOTH endpoints are flagged free in the per-station masks, so
	 * interior edges shared with an adjacent deck (LeftFree/RightFree = false) get no barrier.
	 * Pass all-true masks for a parapet on the full perimeter.
	 */
	void OPENDRIVE_API BuildParapetBuffers(
		const TArray<FVector>& Left,
		const TArray<FVector>& Right,
		double HeightCm,
		double ThicknessCm,
		const TArray<bool>& LeftFree,
		const TArray<bool>& RightFree,
		FGeometryScriptSimpleMeshBuffers& OutBuffers);

	/**
	 * An axis-aligned rectangular box column (pier) centred at (X,Y), spanning Z from BottomZ to
	 * TopZ, with horizontal half-extent HalfWidthCm. Outward-facing normals; capped top & bottom.
	 */
	void OPENDRIVE_API BuildPierBuffers(
		double X, double Y, double TopZ, double BottomZ, double HalfWidthCm,
		FGeometryScriptSimpleMeshBuffers& OutBuffers);
}
