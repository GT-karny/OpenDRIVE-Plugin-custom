#pragma once
#include "CoreMinimal.h"

class USignalTypeMapping;
class USignalAssemblyMapping;

namespace roadmanager { class OpenDrive; }

class FSignalGenerator
{
public:

	/**
	 * Generates signal actors from OpenDRIVE data.
	 * @param World The world to spawn actors in
	 */
	void GenerateSignals(UWorld* World);

	/**
	 * Clears all generated signal actors.
	 */
	void ClearGeneratedSignals();

	// --- Settings ---

	bool bGenerateSignals = true;
	void SetGenerateSignals(bool Val) { bGenerateSignals = Val; }
	bool GetGenerateSignals() const { return bGenerateSignals; }

	bool bFlipSignalOrientation = false;
	void SetFlipSignalOrientation(bool Val) { bFlipSignalOrientation = Val; }
	bool GetFlipSignalOrientation() const { return bFlipSignalOrientation; }

	USignalTypeMapping* SignalTypeMappingAsset = nullptr;
	void SetSignalTypeMappingAsset(USignalTypeMapping* Asset) { SignalTypeMappingAsset = Asset; }
	USignalTypeMapping* GetSignalTypeMappingAsset() const { return SignalTypeMappingAsset; }

	// --- Assembly Settings ---

	bool bEnableAssembly = false;
	void SetEnableAssembly(bool Val) { bEnableAssembly = Val; }
	bool GetEnableAssembly() const { return bEnableAssembly; }

	/** Distance threshold for assembly grouping (meters) */
	float AssemblyDistanceThreshold = 5.0f;
	void SetAssemblyDistanceThreshold(float Val) { AssemblyDistanceThreshold = Val; }
	float GetAssemblyDistanceThreshold() const { return AssemblyDistanceThreshold; }

	/** Heading tolerance for assembly grouping (degrees) */
	float AssemblyHeadingTolerance = 15.0f;
	void SetAssemblyHeadingTolerance(float Val) { AssemblyHeadingTolerance = Val; }
	float GetAssemblyHeadingTolerance() const { return AssemblyHeadingTolerance; }

	USignalAssemblyMapping* SignalAssemblyMappingAsset = nullptr;
	void SetSignalAssemblyMappingAsset(USignalAssemblyMapping* Asset) { SignalAssemblyMappingAsset = Asset; }
	USignalAssemblyMapping* GetSignalAssemblyMappingAsset() const { return SignalAssemblyMappingAsset; }

private:

	/** Build a reverse lookup map: SignalId -> ControllerId */
	TMap<int, int> BuildControllerMap(roadmanager::OpenDrive* Odr) const;

	/** Generate signals as assemblies (grouped by proximity + heading) */
	void GenerateSignalAssemblies(UWorld* World, roadmanager::OpenDrive* Odr, const TMap<int, int>& ControllerMap);

	/** Generate signals individually (existing behavior, with ControllerId support) */
	void GenerateSignalsIndividual(UWorld* World, roadmanager::OpenDrive* Odr, const TMap<int, int>& ControllerMap);

	/** Array of spawned signal actors for cleanup */
	TArray<AActor*> GeneratedSignals;
};
