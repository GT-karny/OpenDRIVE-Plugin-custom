"""
OpenDRIVE Plugin - Asset Setup Sample
======================================
Sets the OpenDRIVE asset on World Settings from Python.

Usage:
  Run this script from the UE Python console or Output Log:
    py "path/to/set_opendrive_asset.py"

Prerequisites:
  - An imported OpenDRIVE (.xodr) asset must exist in your Content Browser
  - World Settings class must be set to OpenDriveWorldSettings in Project Settings
    (Project Settings > Maps & Modes > World Settings Class)
"""

import unreal

subsystem = unreal.get_editor_subsystem(unreal.OpenDriveEditorSubsystem)

# --- Set OpenDRIVE Asset ---
# Change this path to your .xodr asset in the Content Browser
OPENDRIVE_ASSET_PATH = "/Game/OpenDRIVE/YourRoadNetwork"

asset = unreal.load_asset(OPENDRIVE_ASSET_PATH)
if asset is None:
    unreal.log_error(
        f"OpenDRIVE asset not found at '{OPENDRIVE_ASSET_PATH}'. "
        "Please update the path."
    )
else:
    success = subsystem.set_open_drive_asset(asset)
    if success:
        unreal.log(f"OpenDRIVE asset set: {OPENDRIVE_ASSET_PATH}")
    else:
        unreal.log_error(
            "Failed to set OpenDRIVE asset. "
            "Make sure World Settings class is set to OpenDriveWorldSettings."
        )

# --- Verify current assignment ---
current = subsystem.get_open_drive_asset()
if current is not None:
    unreal.log(f"Current OpenDRIVE asset: {current.get_name()}")
else:
    unreal.log_warning("No OpenDRIVE asset is currently assigned.")
