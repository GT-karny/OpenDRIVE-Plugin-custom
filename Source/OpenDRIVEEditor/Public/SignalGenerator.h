#pragma once
#include "CoreMinimal.h"

class USignalTypeMapping;

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

private:

	/** Array of spawned signal actors for cleanup */
	TArray<AActor*> GeneratedSignals;
};
