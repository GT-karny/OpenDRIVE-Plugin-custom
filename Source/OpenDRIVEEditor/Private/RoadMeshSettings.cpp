// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoadMeshSettings.h"
#include "RoadManager.hpp"

int32 FRoadMeshSettings::SlotForLaneType(int32 LaneTypeFlag)
{
	using LT = roadmanager::Lane::LaneType;

	const int32 DrivingMask =
		(int32)LT::LANE_TYPE_DRIVING |
		(int32)LT::LANE_TYPE_ENTRY |
		(int32)LT::LANE_TYPE_EXIT |
		(int32)LT::LANE_TYPE_OFF_RAMP |
		(int32)LT::LANE_TYPE_ON_RAMP |
		(int32)LT::LANE_TYPE_BIDIRECTIONAL |
		(int32)LT::LANE_TYPE_BIKING |
		(int32)LT::LANE_TYPE_PARKING |
		(int32)LT::LANE_TYPE_RESTRICTED |
		(int32)LT::LANE_TYPE_STOP |
		(int32)LT::LANE_TYPE_CONNECTING_RAMP;

	if (LaneTypeFlag & (int32)LT::LANE_TYPE_SIDEWALK) return (int32)ERoadMeshMaterialSlot::Sidewalk;
	if (LaneTypeFlag & ((int32)LT::LANE_TYPE_BORDER | (int32)LT::LANE_TYPE_SHOULDER | (int32)LT::LANE_TYPE_CURB)) return (int32)ERoadMeshMaterialSlot::Border;
	if (LaneTypeFlag & DrivingMask) return (int32)ERoadMeshMaterialSlot::Asphalt;
	return (int32)ERoadMeshMaterialSlot::Misc;
}
