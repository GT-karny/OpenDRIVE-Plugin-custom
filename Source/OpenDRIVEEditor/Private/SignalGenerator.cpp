#include "Public/SignalGenerator.h"
#include "RoadManager.hpp"
#include "SignalInfoComponent.h"
#include "SignalTypeMapping.h"
#include "SignalAssemblyMapping.h"

// --- Helper: Populate a USignalInfoComponent from a roadmanager::Signal ---
static void PopulateSignalInfo(
	USignalInfoComponent* InfoComp,
	roadmanager::Signal* Signal,
	int RoadId,
	const FString& SignalType,
	const FString& SignalSubType,
	const FString& SignalCountry,
	int ControllerId)
{
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
	InfoComp->ControllerId = ControllerId;
}

// --- Helper: Compute UE transform from a roadmanager::Signal ---
static FTransform ComputeSignalTransform(roadmanager::Signal* Signal, bool bFlip)
{
	double X = Signal->GetX();
	double Y = Signal->GetY();
	double Z = Signal->GetZ() + Signal->GetZOffset();
	double H = Signal->GetH() + Signal->GetHOffset();
	double P = Signal->GetPitch();
	double R = Signal->GetRoll();

	if (bFlip)
	{
		H += M_PI;
	}

	// Convert coordinates: OpenDRIVE (meters, right-handed) -> Unreal (cm, left-handed)
	FVector Location(
		X * 100.0,
		-Y * 100.0,
		Z * 100.0
	);

	FRotator Rotation(
		FMath::RadiansToDegrees(P),
		FMath::RadiansToDegrees(-H),
		FMath::RadiansToDegrees(R)
	);

	return FTransform(Rotation, Location);
}

// ============================================================================

TMap<int, int> FSignalGenerator::BuildControllerMap(roadmanager::OpenDrive* Odr) const
{
	TMap<int, int> Map;
	int NumControllers = Odr->GetNumberOfControllers();
	for (int i = 0; i < NumControllers; i++)
	{
		roadmanager::Controller* Ctrl = Odr->GetControllerByIdx(i);
		if (!Ctrl) continue;

		int ControllerId = Ctrl->GetId();
		int NumControls = Ctrl->GetNumberOfControls();
		for (int j = 0; j < NumControls; j++)
		{
			roadmanager::Control* Control = Ctrl->GetControl(j);
			if (Control)
			{
				Map.Add(Control->signalId_, ControllerId);
			}
		}
	}
	return Map;
}

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

	// Get OpenDrive instance
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

	// Build Signal -> Controller reverse lookup
	TMap<int, int> ControllerMap = BuildControllerMap(Odr);

	if (bEnableAssembly)
	{
		GenerateSignalAssemblies(World, Odr, ControllerMap);
	}
	else
	{
		GenerateSignalsIndividual(World, Odr, ControllerMap);
	}
}

void FSignalGenerator::GenerateSignalsIndividual(
	UWorld* World,
	roadmanager::OpenDrive* Odr,
	const TMap<int, int>& ControllerMap)
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.bHideFromSceneOutliner = false;
	SpawnParams.bTemporaryEditorActor = false;

	int32 TotalSignalsSpawned = 0;
	size_t NumRoads = Odr->GetNumOfRoads();

	for (size_t RoadIdx = 0; RoadIdx < NumRoads; RoadIdx++)
	{
		roadmanager::Road* Road = Odr->GetRoadByIdx(static_cast<int>(RoadIdx));
		if (!Road) continue;

		int RoadId = Road->GetId();
		int SignalCount = Road->GetNumberOfSignals();
		if (SignalCount <= 0) continue;

		for (int SignalIdx = 0; SignalIdx < SignalCount; SignalIdx++)
		{
			roadmanager::Signal* Signal = Road->GetSignal(SignalIdx);
			if (!Signal) continue;

			FString SignalType = FString(UTF8_TO_TCHAR(Signal->GetType().c_str()));
			FString SignalSubType = FString(UTF8_TO_TCHAR(Signal->GetSubType().c_str()));
			FString SignalCountry = FString(UTF8_TO_TCHAR(Signal->GetCountry().c_str()));

			TSubclassOf<AActor> ActorClass = nullptr;
			if (SignalTypeMappingAsset)
			{
				ActorClass = SignalTypeMappingAsset->FindActorClassForSignal(
					SignalType, SignalSubType, SignalCountry);
			}

			if (!ActorClass)
			{
				UE_LOG(LogClass, Warning, TEXT("GenerateSignals: No actor class for signal type=%s subtype=%s (Road %d, Signal %d)"),
					*SignalType, *SignalSubType, RoadId, Signal->GetId());
				continue;
			}

			FTransform SpawnTransform = ComputeSignalTransform(Signal, bFlipSignalOrientation);

			AActor* SignalActor = World->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParams);
			if (!SignalActor) continue;

			// Populate SignalInfoComponent
			USignalInfoComponent* InfoComp = SignalActor->FindComponentByClass<USignalInfoComponent>();
			bool bCreatedNew = false;
			if (!InfoComp)
			{
				InfoComp = NewObject<USignalInfoComponent>(SignalActor, NAME_None, RF_Transactional);
				bCreatedNew = true;
			}

			int ControllerId = ControllerMap.Contains(Signal->GetId())
				? ControllerMap[Signal->GetId()] : -1;

			PopulateSignalInfo(InfoComp, Signal, RoadId, SignalType, SignalSubType, SignalCountry, ControllerId);

			if (bCreatedNew)
			{
				InfoComp->RegisterComponent();
				SignalActor->AddInstanceComponent(InfoComp);
			}

#if WITH_EDITOR
			FString FolderPath = FString::Printf(TEXT("Signals/Road_%d"), RoadId);
			SignalActor->SetFolderPath(FName(*FolderPath));

			FString Label = FString::Printf(TEXT("Signal_%d_%s"), Signal->GetId(), *SignalType);
			SignalActor->SetActorLabel(Label);
#endif

			GeneratedSignals.Add(SignalActor);
			TotalSignalsSpawned++;
		}
	}

	UE_LOG(LogClass, Log, TEXT("GenerateSignals: Spawned %d signals (individual mode)"), TotalSignalsSpawned);
}

// --- Assembly Mode ---

namespace
{
	struct FSignalEntry
	{
		roadmanager::Signal* Signal;
		int RoadId;
		FVector WorldPos;     // UE coords (cm)
		double Heading;       // radians (OpenDRIVE heading)
		int ControllerId;
		FString Type;
		FString SubType;
		FString Country;
		int GroupId = -1;
	};

	/** Find root of a group (union-find path compression) */
	int FindRoot(TArray<int>& Parent, int i)
	{
		while (Parent[i] != i)
		{
			Parent[i] = Parent[Parent[i]];
			i = Parent[i];
		}
		return i;
	}

	/** Union two groups */
	void UnionGroups(TArray<int>& Parent, TArray<int>& Rank, int a, int b)
	{
		int ra = FindRoot(Parent, a);
		int rb = FindRoot(Parent, b);
		if (ra == rb) return;

		if (Rank[ra] < Rank[rb]) Swap(ra, rb);
		Parent[rb] = ra;
		if (Rank[ra] == Rank[rb]) Rank[ra]++;
	}

	/** Normalize heading difference to [0, PI] */
	double HeadingDifference(double h1, double h2)
	{
		double diff = FMath::Abs(h1 - h2);
		diff = FMath::Fmod(diff, 2.0 * M_PI);
		if (diff > M_PI) diff = 2.0 * M_PI - diff;
		return diff;
	}
}

void FSignalGenerator::GenerateSignalAssemblies(
	UWorld* World,
	roadmanager::OpenDrive* Odr,
	const TMap<int, int>& ControllerMap)
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.bHideFromSceneOutliner = false;
	SpawnParams.bTemporaryEditorActor = false;

	int32 TotalActorsSpawned = 0;
	int32 TotalSignalsProcessed = 0;

	const double DistThresholdCm = AssemblyDistanceThreshold * 100.0;
	const double HeadingThresholdRad = FMath::DegreesToRadians(AssemblyHeadingTolerance);

	size_t NumRoads = Odr->GetNumOfRoads();

	// Process each road independently
	for (size_t RoadIdx = 0; RoadIdx < NumRoads; RoadIdx++)
	{
		roadmanager::Road* Road = Odr->GetRoadByIdx(static_cast<int>(RoadIdx));
		if (!Road) continue;

		int RoadId = Road->GetId();
		int SignalCount = Road->GetNumberOfSignals();
		if (SignalCount <= 0) continue;

		// Phase 1: Collect all signals on this road
		TArray<FSignalEntry> Entries;
		Entries.Reserve(SignalCount);

		for (int SignalIdx = 0; SignalIdx < SignalCount; SignalIdx++)
		{
			roadmanager::Signal* Signal = Road->GetSignal(SignalIdx);
			if (!Signal) continue;

			FSignalEntry Entry;
			Entry.Signal = Signal;
			Entry.RoadId = RoadId;
			Entry.Type = FString(UTF8_TO_TCHAR(Signal->GetType().c_str()));
			Entry.SubType = FString(UTF8_TO_TCHAR(Signal->GetSubType().c_str()));
			Entry.Country = FString(UTF8_TO_TCHAR(Signal->GetCountry().c_str()));
			Entry.Heading = Signal->GetH() + Signal->GetHOffset();
			Entry.ControllerId = ControllerMap.Contains(Signal->GetId())
				? ControllerMap[Signal->GetId()] : -1;

			// Compute UE world position (for distance comparison)
			double X = Signal->GetX();
			double Y = Signal->GetY();
			double Z = Signal->GetZ() + Signal->GetZOffset();
			Entry.WorldPos = FVector(X * 100.0, -Y * 100.0, Z * 100.0);

			Entries.Add(Entry);
		}

		if (Entries.Num() == 0) continue;

		// Phase 2: Group by proximity + heading using union-find
		TArray<int> Parent;
		TArray<int> Rank;
		Parent.SetNum(Entries.Num());
		Rank.SetNum(Entries.Num());
		for (int i = 0; i < Entries.Num(); i++)
		{
			Parent[i] = i;
			Rank[i] = 0;
		}

		for (int i = 0; i < Entries.Num(); i++)
		{
			for (int j = i + 1; j < Entries.Num(); j++)
			{
				double Dist = FVector::Dist(Entries[i].WorldPos, Entries[j].WorldPos);
				double HDiff = HeadingDifference(Entries[i].Heading, Entries[j].Heading);

				if (Dist < DistThresholdCm && HDiff < HeadingThresholdRad)
				{
					UnionGroups(Parent, Rank, i, j);
				}
			}
		}

		// Collect groups
		TMap<int, TArray<int>> Groups;
		for (int i = 0; i < Entries.Num(); i++)
		{
			int Root = FindRoot(Parent, i);
			Groups.FindOrAdd(Root).Add(i);
		}

		// Phase 3: Spawn actors per group
		for (auto& Pair : Groups)
		{
			const TArray<int>& Indices = Pair.Value;

			if (Indices.Num() == 1)
			{
				// Single signal: use individual mapping
				const FSignalEntry& Entry = Entries[Indices[0]];

				TSubclassOf<AActor> ActorClass = nullptr;
				if (SignalTypeMappingAsset)
				{
					ActorClass = SignalTypeMappingAsset->FindActorClassForSignal(
						Entry.Type, Entry.SubType, Entry.Country);
				}

				if (!ActorClass)
				{
					UE_LOG(LogClass, Warning, TEXT("GenerateSignals: No actor class for signal type=%s subtype=%s (Road %d, Signal %d)"),
						*Entry.Type, *Entry.SubType, RoadId, Entry.Signal->GetId());
					continue;
				}

				FTransform SpawnTransform = ComputeSignalTransform(Entry.Signal, bFlipSignalOrientation);
				AActor* SignalActor = World->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParams);
				if (!SignalActor) continue;

				USignalInfoComponent* InfoComp = SignalActor->FindComponentByClass<USignalInfoComponent>();
				bool bCreatedNew = false;
				if (!InfoComp)
				{
					InfoComp = NewObject<USignalInfoComponent>(SignalActor, NAME_None, RF_Transactional);
					bCreatedNew = true;
				}

				PopulateSignalInfo(InfoComp, Entry.Signal, RoadId,
					Entry.Type, Entry.SubType, Entry.Country, Entry.ControllerId);

				if (bCreatedNew)
				{
					InfoComp->RegisterComponent();
					SignalActor->AddInstanceComponent(InfoComp);
				}

#if WITH_EDITOR
				FString FolderPath = FString::Printf(TEXT("Signals/Road_%d"), RoadId);
				SignalActor->SetFolderPath(FName(*FolderPath));
				FString Label = FString::Printf(TEXT("Signal_%d_%s"), Entry.Signal->GetId(), *Entry.Type);
				SignalActor->SetActorLabel(Label);
#endif

				GeneratedSignals.Add(SignalActor);
				TotalActorsSpawned++;
				TotalSignalsProcessed++;
			}
			else
			{
				// Assembly: multiple signals grouped together
				// Collect signal types for assembly mapping lookup
				TArray<FString> GroupTypes;
				for (int Idx : Indices)
				{
					GroupTypes.AddUnique(Entries[Idx].Type);
				}

				// Find assembly actor class
				TSubclassOf<AActor> ActorClass = nullptr;
				if (SignalAssemblyMappingAsset)
				{
					ActorClass = SignalAssemblyMappingAsset->FindActorClassForAssembly(GroupTypes);
				}

				// Fallback: use individual mapping with primary signal
				if (!ActorClass && SignalTypeMappingAsset)
				{
					// Primary = signal with largest Height, or first if equal
					int PrimaryIdx = Indices[0];
					double MaxHeight = Entries[Indices[0]].Signal->GetHeight();
					for (int k = 1; k < Indices.Num(); k++)
					{
						double H = Entries[Indices[k]].Signal->GetHeight();
						if (H > MaxHeight)
						{
							MaxHeight = H;
							PrimaryIdx = Indices[k];
						}
					}

					const FSignalEntry& Primary = Entries[PrimaryIdx];
					ActorClass = SignalTypeMappingAsset->FindActorClassForSignal(
						Primary.Type, Primary.SubType, Primary.Country);
				}

				if (!ActorClass)
				{
					UE_LOG(LogClass, Warning, TEXT("GenerateSignals: No actor class for assembly on Road %d (types: %s)"),
						RoadId, *FString::Join(GroupTypes, TEXT(", ")));
					continue;
				}

				// Determine primary signal for transform (largest Height)
				int PrimaryIdx = Indices[0];
				double MaxHeight = Entries[Indices[0]].Signal->GetHeight();
				for (int k = 1; k < Indices.Num(); k++)
				{
					double H = Entries[Indices[k]].Signal->GetHeight();
					if (H > MaxHeight)
					{
						MaxHeight = H;
						PrimaryIdx = Indices[k];
					}
				}

				FTransform SpawnTransform = ComputeSignalTransform(
					Entries[PrimaryIdx].Signal, bFlipSignalOrientation);

				AActor* AssemblyActor = World->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParams);
				if (!AssemblyActor) continue;

				// Attach SignalInfoComponent for each constituent signal
				// First, populate existing default component if present
				bool bFirstCompUsed = false;
				for (int k = 0; k < Indices.Num(); k++)
				{
					const FSignalEntry& Entry = Entries[Indices[k]];

					USignalInfoComponent* InfoComp = nullptr;
					bool bCreatedNew = false;

					if (!bFirstCompUsed)
					{
						// Try to reuse existing default subobject
						InfoComp = AssemblyActor->FindComponentByClass<USignalInfoComponent>();
						if (InfoComp)
						{
							bFirstCompUsed = true;
						}
					}

					if (!InfoComp)
					{
						FName CompName = *FString::Printf(TEXT("SignalInfo_%d"), Entry.Signal->GetId());
						InfoComp = NewObject<USignalInfoComponent>(AssemblyActor, CompName, RF_Transactional);
						bCreatedNew = true;
					}

					PopulateSignalInfo(InfoComp, Entry.Signal, RoadId,
						Entry.Type, Entry.SubType, Entry.Country, Entry.ControllerId);

					if (bCreatedNew)
					{
						InfoComp->RegisterComponent();
						AssemblyActor->AddInstanceComponent(InfoComp);
					}
				}

#if WITH_EDITOR
				FString FolderPath = FString::Printf(TEXT("Signals/Road_%d"), RoadId);
				AssemblyActor->SetFolderPath(FName(*FolderPath));

				// Label with all signal IDs
				TArray<FString> IdStrings;
				for (int Idx : Indices)
				{
					IdStrings.Add(FString::FromInt(Entries[Idx].Signal->GetId()));
				}
				FString Label = FString::Printf(TEXT("Assembly_%s"), *FString::Join(IdStrings, TEXT("_")));
				AssemblyActor->SetActorLabel(Label);
#endif

				GeneratedSignals.Add(AssemblyActor);
				TotalActorsSpawned++;
				TotalSignalsProcessed += Indices.Num();
			}
		}
	}

	UE_LOG(LogClass, Log, TEXT("GenerateSignals: Spawned %d actors from %d signals (assembly mode)"),
		TotalActorsSpawned, TotalSignalsProcessed);
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
