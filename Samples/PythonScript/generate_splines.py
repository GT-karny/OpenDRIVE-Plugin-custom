"""
OpenDRIVE Plugin - Spline Generation Sample
=============================================
Generates lane spline actors from OpenDRIVE data.

Usage:
  Run this script from the UE Python console or Output Log:
    py "path/to/generate_splines.py"

Prerequisites:
  - An OpenDRIVE asset must be set in World Settings (or set it via script below)
  - World Settings class must be set to OpenDriveWorldSettings in Project Settings
"""

import unreal

# Get the OpenDRIVE Editor Subsystem
subsystem = unreal.get_editor_subsystem(unreal.OpenDriveEditorSubsystem)

# --- (Optional) Set OpenDRIVE asset from Python ---
# odr_asset = unreal.load_asset("/Game/OpenDRIVE/YourRoadNetwork")
# if odr_asset:
#     subsystem.set_open_drive_asset(odr_asset)

# --- Configuration ---
# Z-offset in cm (raise splines above ground to avoid z-fighting)
subsystem.set_spline_offset(20.0)

# Step distance in meters (lower = more precise, higher = better performance)
subsystem.set_spline_step(5.0)

# Spline generation mode: 0=Center, 1=Inside, 2=Outside
subsystem.set_spline_mode(0)

# Generate for roads and/or junctions
subsystem.set_generate_roads(True)
subsystem.set_generate_junctions(True)

# Lane type filters
subsystem.set_lane_type_filter(
    b_driving=True,
    b_sidewalk=True,
    b_biking=True,
    b_parking=True,
    b_shoulder=False,
    b_restricted=False,
    b_median=False,
    b_other=False,
    b_reference=False
)

# Only outermost driving lane per side (useful for simplified road networks)
subsystem.set_generate_outermost_driving_lane_only(False)

# --- Generate ---
subsystem.generate_lane_splines()

unreal.log("Spline generation complete.")
