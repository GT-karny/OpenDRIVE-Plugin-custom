// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoadMeshSettings.h"
#include "OpenDRIVEMeshMath.h"

// Compile-time guard: the editor-facing material-slot enum must agree numerically
// with the runtime geometry core's ERoadMatSlot, because the core stamps slot
// indices into the mesh and the actor's material array is indexed by this enum.
static_assert((uint8)ERoadMeshMaterialSlot::Asphalt  == (uint8)OpenDRIVEMesh::ERoadMatSlot::Asphalt,  "Asphalt slot drift");
static_assert((uint8)ERoadMeshMaterialSlot::Sidewalk == (uint8)OpenDRIVEMesh::ERoadMatSlot::Sidewalk, "Sidewalk slot drift");
static_assert((uint8)ERoadMeshMaterialSlot::Border   == (uint8)OpenDRIVEMesh::ERoadMatSlot::Border,   "Border slot drift");
static_assert((uint8)ERoadMeshMaterialSlot::Marking  == (uint8)OpenDRIVEMesh::ERoadMatSlot::Marking,  "Marking slot drift");
static_assert((uint8)ERoadMeshMaterialSlot::Misc     == (uint8)OpenDRIVEMesh::ERoadMatSlot::Misc,     "Misc slot drift");
// Editor "Deck" == core "Structure" (same numeric slot 5, different display name).
static_assert((uint8)ERoadMeshMaterialSlot::Deck     == (uint8)OpenDRIVEMesh::ERoadMatSlot::Structure, "Deck/Structure slot drift");

int32 FRoadMeshSettings::SlotForLaneType(int32 LaneTypeFlag)
{
	// Single source of truth lives in the runtime geometry core.
	return OpenDRIVEMesh::SlotForLaneType(LaneTypeFlag);
}
