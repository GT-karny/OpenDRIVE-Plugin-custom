"""
OpenDRIVE Plugin - Full Generation Sample
==========================================
Sets the OpenDRIVE asset, then generates both lane splines and traffic signals.

Usage:
  Run this script from the UE Python console or Output Log:
    py "path/to/generate_all.py"

Prerequisites:
  - An imported OpenDRIVE (.xodr) asset must exist in your project
  - A SignalTypeMapping data asset must exist for signal generation
  - World Settings class must be set to OpenDriveWorldSettings in Project Settings
"""

import unreal

subsystem = unreal.get_editor_subsystem(unreal.OpenDriveEditorSubsystem)

# ===========================================
# 0. Set OpenDRIVE Asset (optional)
# ===========================================
# If the OpenDRIVE asset is already set in World Settings, skip this step.
# Change the path below to match your project's .xodr asset path.
OPENDRIVE_ASSET_PATH = "/Game/OpenDRIVE/YourRoadNetwork"

odr_asset = unreal.load_asset(OPENDRIVE_ASSET_PATH)
if odr_asset is not None:
    subsystem.set_open_drive_asset(odr_asset)
    unreal.log(f"OpenDRIVE asset set: {OPENDRIVE_ASSET_PATH}")
else:
    # If asset not found, continue with whatever is already set in World Settings
    unreal.log_warning(
        f"OpenDRIVE asset not found at '{OPENDRIVE_ASSET_PATH}'. "
        "Using existing World Settings assignment."
    )

# ===========================================
# 1. Clear previous generation
# ===========================================
subsystem.clear_generated_splines()
subsystem.clear_generated_signals()
unreal.log("Cleared previous generation.")

# ===========================================
# 2. Generate Splines
# ===========================================
subsystem.set_spline_offset(20.0)
subsystem.set_spline_step(5.0)
subsystem.set_spline_mode(0)  # Center

subsystem.set_generate_roads(True)
subsystem.set_generate_junctions(True)

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
subsystem.set_generate_outermost_driving_lane_only(False)

subsystem.generate_lane_splines()
unreal.log("Spline generation complete.")

# ===========================================
# 3. Generate Signals
# ===========================================
MAPPING_ASSET_PATH = "/Game/OpenDRIVE/SignalTypeMapping"

mapping = unreal.load_asset(MAPPING_ASSET_PATH)
if mapping is not None:
    subsystem.set_flip_signal_orientation(False)
    subsystem.generate_signals(mapping)
    unreal.log("Signal generation complete.")
else:
    unreal.log_warning(
        f"SignalTypeMapping not found at '{MAPPING_ASSET_PATH}'. "
        "Skipping signal generation."
    )

unreal.log("All generation complete.")
