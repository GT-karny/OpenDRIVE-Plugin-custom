#include "Public/SignalGenerator.h"
#include "RoadManager.hpp"
#include "SignalInfoComponent.h"
#include "SignalTypeMapping.h"
#include "BPI_SignalAutoSetup.h"

void FSignalGenerator::GenerateSignals(UWorld* World)
{
	// Clear existing signals first
	ClearGeneratedSignals();

	if (!bGenerateSignals)
	{
		return;
	}

	if (!World)
	{
		UE_LOG(LogClass, Warning, TEXT("GenerateSignals: World is null"));
		return;
	}

	// Get OpenDrive instance (same pattern as GenerateLaneSplines)
	roadmanager::OpenDrive* Odr = roadmanager::Position::GetOpenDrive();
	if (!Odr)
	{
		UE_LOG(LogClass, Warning, TEXT("GenerateSignals: OpenDrive not loaded"));
		return;
	}

	size_t NumRoads = Odr->GetNumOfRoads();
	if (NumRoads == 0)
	{
		UE_LOG(LogClass, Warning, TEXT("GenerateSignals: No roads found"));
		return;
	}

	// Actor spawn parameters
	FActorSpawnParameters SpawnParams;
	SpawnParams.bHideFromSceneOutliner = false;
	SpawnParams.bTemporaryEditorActor = false;

	int32 TotalSignalsSpawned = 0;

	// Iterate all roads
	for (size_t RoadIdx = 0; RoadIdx < NumRoads; RoadIdx++)
	{
		roadmanager::Road* Road = Odr->GetRoadByIdx(static_cast<int>(RoadIdx));
		if (!Road) continue;

		int RoadId = Road->GetId();
		int SignalCount = Road->GetNumberOfSignals();
		if (SignalCount <= 0) continue;

		// Iterate all signals on this road
		for (int SignalIdx = 0; SignalIdx < SignalCount; SignalIdx++)
		{
			roadmanager::Signal* Signal = Road->GetSignal(SignalIdx);
			if (!Signal) continue;

			// Get signal properties
			FString SignalType = FString(UTF8_TO_TCHAR(Signal->GetType().c_str()));
			FString SignalSubType = FString(UTF8_TO_TCHAR(Signal->GetSubType().c_str()));
			FString SignalCountry = FString(UTF8_TO_TCHAR(Signal->GetCountry().c_str()));

			// Determine actor class to spawn
			TSubclassOf<AActor> ActorClass = nullptr;
			if (SignalTypeMappingAsset)
			{
				ActorClass = SignalTypeMappingAsset->FindActorClassForSignal(
					SignalType,
					SignalSubType,
					SignalCountry
				);
			}

			if (!ActorClass)
			{
				UE_LOG(LogClass, Warning, TEXT("GenerateSignals: No actor class for signal type=%s subtype=%s (Road %d, Signal %d)"),
					*SignalType, *SignalSubType, RoadId, Signal->GetId());
				continue;
			}

			// Use Signal's pre-computed world coordinates
			// Note: Signal->GetH() already includes orientation adjustment (adds π for NEGATIVE orientation)
			double X = Signal->GetX();
			double Y = Signal->GetY();
			double Z = Signal->GetZ() + Signal->GetZOffset();
			double H = Signal->GetH() + Signal->GetHOffset();
			double P = Signal->GetPitch();
			double R = Signal->GetRoll();

			// Apply flip if enabled
			if (bFlipSignalOrientation)
			{
				H += M_PI;
			}

			// Convert coordinates: OpenDRIVE (meters, right-handed) -> Unreal (cm, left-handed)
			FVector Location(
				X * 100.0,    // meters to cm
				-Y * 100.0,   // Y negated for left-handed coordinate system
				Z * 100.0     // meters to cm
			);

			// Convert rotation: h (heading), p (pitch), r (roll) in radians
			FRotator Rotation(
				FMath::RadiansToDegrees(P),    // Pitch
				FMath::RadiansToDegrees(-H),   // Yaw (negated for UE coordinate system)
				FMath::RadiansToDegrees(R)     // Roll
			);

			FTransform SpawnTransform(Rotation, Location);

			// Spawn the actor
			AActor* SignalActor = World->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParams);
			if (!SignalActor) continue;

			// Add SignalInfoComponent and populate it
			USignalInfoComponent* InfoComp = NewObject<USignalInfoComponent>(SignalActor, NAME_None, RF_Transactional);
			InfoComp->SignalId = Signal->GetId();
			InfoComp->RoadId = RoadId;
			InfoComp->S = Signal->GetS();
			InfoComp->T = Signal->GetT();
			InfoComp->Type = SignalType;
			InfoComp->SubType = SignalSubType;
			InfoComp->Country = SignalCountry;
			InfoComp->Value = Signal->GetValue();
			InfoComp->Unit = FString(UTF8_TO_TCHAR(Signal->GetUnit().c_str()));
			InfoComp->Text = FString(UTF8_TO_TCHAR(Signal->GetText().c_str()));
			InfoComp->bIsDynamic = Signal->IsDynamic();
			InfoComp->Height = Signal->GetHeight();
			InfoComp->Width = Signal->GetWidth();
			InfoComp->RegisterComponent();
			SignalActor->AddInstanceComponent(InfoComp);

			// If the actor implements IBPI_SignalAutoSetup, auto-configure it
			if (SignalActor->GetClass()->ImplementsInterface(UBPI_SignalAutoSetup::StaticClass()))
			{
				IBPI_SignalAutoSetup::Execute_OnSignalAutoPlaced(SignalActor, InfoComp);
			}

#if WITH_EDITOR
			// Organize in editor folder
			FString FolderPath = FString::Printf(TEXT("Signals/Road_%d"), RoadId);
			SignalActor->SetFolderPath(FName(*FolderPath));

			// Set actor label
			FString Label = FString::Printf(TEXT("Signal_%d_%s"), Signal->GetId(), *SignalType);
			SignalActor->SetActorLabel(Label);
#endif

			GeneratedSignals.Add(SignalActor);
			TotalSignalsSpawned++;
		}
	}

	UE_LOG(LogClass, Log, TEXT("GenerateSignals: Spawned %d signals"), TotalSignalsSpawned);
}

void FSignalGenerator::ClearGeneratedSignals()
{
	for (AActor* Signal : GeneratedSignals)
	{
		if (IsValid(Signal))
		{
			Signal->Destroy();
		}
	}
	GeneratedSignals.Empty();
}
