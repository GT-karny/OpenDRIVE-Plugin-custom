"""
OpenDRIVE Plugin - Signal Generation Sample
=============================================
Generates traffic signal actors from the currently loaded OpenDRIVE data.

Usage:
  Run this script from the UE Python console or Output Log:
    py "path/to/generate_signals.py"

Prerequisites:
  - OpenDRIVE asset must be set in World Settings
  - A SignalTypeMapping data asset must exist in your project
    (maps signal type/subtype to actor classes)
"""

import unreal

# Get the OpenDRIVE Editor Subsystem
subsystem = unreal.get_editor_subsystem(unreal.OpenDriveEditorSubsystem)

# --- Configuration ---
# Load your SignalTypeMapping data asset
# Change this path to match your project's asset path
MAPPING_ASSET_PATH = "/Game/OpenDRIVE/SignalTypeMapping"

mapping = unreal.load_asset(MAPPING_ASSET_PATH)
if mapping is None:
    unreal.log_warning(
        f"SignalTypeMapping asset not found at '{MAPPING_ASSET_PATH}'. "
        "Please update the path in this script."
    )
else:
    # Flip signal orientation by 180 degrees if needed
    subsystem.set_flip_signal_orientation(False)

    # Generate signals
    subsystem.generate_signals(mapping)

    unreal.log("Signal generation complete.")
