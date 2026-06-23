// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoadMeshGenerator.h"

#include "RoadMeshActor.h"
#include "OpenDRIVEMeshMath.h"   // runtime geometry core (single source of truth)
#include "RoadManager.hpp"

#include "Engine/World.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

DEFINE_LOG_CATEGORY_STATIC(LogOpenDriveRoadMesh, Log, All);

namespace
{
	// --- Deck pier-avoidance / parapet helpers (file-local; used by the deck pass) ----

	// XY point-in-polygon (even-odd ray cast). Poly is an open ring (last connects to first).
	bool PointInPoly2D(const TArray<FVector2D>& Poly, const FVector2D& P)
	{
		bool bIn = false;
		const int32 n = Poly.Num();
		for (int32 i = 0, j = n - 1; i < n; j = i++)
		{
			const FVector2D& A = Poly[i];
			const FVector2D& B = Poly[j];
			if (((A.Y > P.Y) != (B.Y > P.Y)) &&
				(P.X < (B.X - A.X) * (P.Y - A.Y) / (B.Y - A.Y + KINDA_SMALL_NUMBER) + A.X))
			{
				bIn = !bIn;
			}
		}
		return bIn;
	}

	// Squared XY distance from P to the nearest polygon edge.
	double DistToPolyEdgesSq2D(const TArray<FVector2D>& Poly, const FVector2D& P)
	{
		double Best = TNumericLimits<double>::Max();
		const int32 n = Poly.Num();
		for (int32 i = 0, j = n - 1; i < n; j = i++)
		{
			const FVector2D& A = Poly[j];
			const FVector2D& B = Poly[i];
			const FVector2D AB = B - A;
			const double L2 = FVector2D::DotProduct(AB, AB);
			double t = (L2 > 0.0) ? (double)FVector2D::DotProduct(P - A, AB) / L2 : 0.0;
			t = FMath::Clamp(t, 0.0, 1.0);
			const FVector2D Proj = A + AB * t;
			Best = FMath::Min(Best, (double)FVector2D::DistSquared(P, Proj));
		}
		return Best;
	}

	/**
	 * Scan a Content folder for UMaterialInterface assets and assign them to ERoadMeshMaterialSlot
	 * by name keyword. Matching is case-insensitive on the asset name.
	 *   Asphalt/Driving/Road        -> slot 0 (Asphalt)
	 *   Sidewalk/Walk               -> slot 1 (Sidewalk)
	 *   Border/Shoulder/Curb        -> slot 2 (Border)
	 *   Mark/Line/Stripe            -> slot 3 (Marking)
	 *   Deck/Bridge/Overpass/Struct -> slot 5 (Deck / Structure)
	 *   otherwise                   -> slot 4 (Misc)
	 *
	 * Returns an array sized to the number of slots, with nullptr where no match was found.
	 */
	TArray<UMaterialInterface*> DiscoverMaterialsAtPath(const FString& ContentFolderPath)
	{
		TArray<UMaterialInterface*> Out;
		Out.Init(nullptr, (int32)ERoadMeshMaterialSlot::Deck + 1); // 6 slots

		FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AR = ARM.Get();
		AR.ScanPathsSynchronous({ ContentFolderPath }, /*bForceRescan=*/false);

		FARFilter Filter;
		Filter.PackagePaths.Add(*ContentFolderPath);
		Filter.bRecursivePaths = true;
		Filter.ClassPaths.Add(UMaterialInterface::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;

		TArray<FAssetData> Assets;
		AR.GetAssets(Filter, Assets);

		for (const FAssetData& AD : Assets)
		{
			const FString Name = AD.AssetName.ToString();
			int32 SlotMatch = -1;
			if      (Name.Contains(TEXT("Asphalt"), ESearchCase::IgnoreCase) ||
			         Name.Contains(TEXT("Driving"), ESearchCase::IgnoreCase) ||
			         Name.Contains(TEXT("Road"),    ESearchCase::IgnoreCase)) SlotMatch = (int32)ERoadMeshMaterialSlot::Asphalt;
			else if (Name.Contains(TEXT("Sidewalk"), ESearchCase::IgnoreCase) ||
			         Name.Contains(TEXT("Walk"),     ESearchCase::IgnoreCase)) SlotMatch = (int32)ERoadMeshMaterialSlot::Sidewalk;
			else if (Name.Contains(TEXT("Border"),   ESearchCase::IgnoreCase) ||
			         Name.Contains(TEXT("Shoulder"), ESearchCase::IgnoreCase) ||
			         Name.Contains(TEXT("Curb"),     ESearchCase::IgnoreCase)) SlotMatch = (int32)ERoadMeshMaterialSlot::Border;
			else if (Name.Contains(TEXT("Mark"),   ESearchCase::IgnoreCase) ||
			         Name.Contains(TEXT("Line"),   ESearchCase::IgnoreCase) ||
			         Name.Contains(TEXT("Stripe"), ESearchCase::IgnoreCase)) SlotMatch = (int32)ERoadMeshMaterialSlot::Marking;
			else if (Name.Contains(TEXT("Deck"),     ESearchCase::IgnoreCase) ||
			         Name.Contains(TEXT("Bridge"),   ESearchCase::IgnoreCase) ||
			         Name.Contains(TEXT("Overpass"), ESearchCase::IgnoreCase) ||
			         Name.Contains(TEXT("Structure"),ESearchCase::IgnoreCase)) SlotMatch = (int32)ERoadMeshMaterialSlot::Deck;
			else
				SlotMatch = (int32)ERoadMeshMaterialSlot::Misc;

			if (SlotMatch >= 0 && SlotMatch < Out.Num() && Out[SlotMatch] == nullptr)
			{
				if (UMaterialInterface* M = Cast<UMaterialInterface>(AD.GetAsset()))
				{
					Out[SlotMatch] = M;
				}
			}
		}
		return Out;
	}
}

void FRoadMeshGenerator::GenerateRoadMesh(UWorld* World)
{
	LastReport.Empty();

	if (!World)
	{
		UE_LOG(LogOpenDriveRoadMesh, Warning, TEXT("GenerateRoadMesh: World is null"));
		return;
	}

	roadmanager::OpenDrive* Odr = roadmanager::Position::GetOpenDrive();
	if (!Odr)
	{
		UE_LOG(LogOpenDriveRoadMesh, Warning, TEXT("GenerateRoadMesh: OpenDrive not loaded"));
		return;
	}

	const int32 NumRoads = (int32)Odr->GetNumOfRoads();
	if (NumRoads <= 0)
	{
		UE_LOG(LogOpenDriveRoadMesh, Warning, TEXT("GenerateRoadMesh: no roads"));
		return;
	}

	// Spawn the actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.bHideFromSceneOutliner = false;
	ARoadMeshActor* Actor = World->SpawnActor<ARoadMeshActor>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!Actor)
	{
		UE_LOG(LogOpenDriveRoadMesh, Error, TEXT("GenerateRoadMesh: failed to spawn ARoadMeshActor"));
		return;
	}
#if WITH_EDITOR
	Actor->SetActorLabel(TEXT("OpenDriveRoadMesh"));
#endif

	// Material assignment policy:
	//   1) If Settings.Materials is non-empty, use it.
	//   2) Otherwise auto-discover under Settings.MaterialFolder (default /Game/RoadGeneration/Material)
	//      and a couple of common fallback paths.
	//   3) If still empty, slots are populated with nullptr — DynamicMeshComponent will show its default.
	TArray<UMaterialInterface*> Mats = Settings.Materials;
	if (Mats.Num() == 0)
	{
		const TArray<FString> Candidates = {
			Settings.MaterialFolder,
			TEXT("/OpenDRIVE/RoadGeneration/Material"),
			TEXT("/OpenDRIVE/RoadGeneration/Materials"),
			TEXT("/Game/RoadGeneration/Materials"),
			TEXT("/Game/RoadGeneration"),
		};
		for (const FString& Path : Candidates)
		{
			if (Path.IsEmpty()) continue;
			Mats = DiscoverMaterialsAtPath(Path);
			const int32 Hits = Mats.FilterByPredicate([](UMaterialInterface* M){ return M != nullptr; }).Num();
			UE_LOG(LogOpenDriveRoadMesh, Log, TEXT("GenerateRoadMesh: scan '%s' -> %d materials"), *Path, Hits);
			if (Hits > 0) break;
		}
	}
	// Always present 6 slots even if all nullptr (so the baked StaticMesh gets an explicit slot count).
	const int32 NumSlots = (int32)ERoadMeshMaterialSlot::Deck + 1;
	while (Mats.Num() < NumSlots) Mats.Add(nullptr);
	Actor->DefaultMaterials = Mats;
	Actor->ApplyDefaultMaterials();

	UDynamicMesh* Mesh = Actor->MeshComp->GetDynamicMesh();
	if (!Mesh)
	{
		UE_LOG(LogOpenDriveRoadMesh, Error, TEXT("GenerateRoadMesh: actor has no DynamicMesh"));
		return;
	}
	Mesh->Reset();

	// Cached sampling/elevation params.
	const double MaxStep    = FMath::Max((double)Settings.MaxStepMeters, 0.05);
	const double MinStep    = FMath::Max((double)Settings.MinStepMeters, 0.01);
	const double ZOff       = Settings.ZOffsetCm;
	const double MarkOff    = Settings.MarkingZOffsetCm;
	const double CurbHeight = FMath::Max((double)Settings.CurbHeightCm, 0.0);

	// Append a buffer into the single road mesh with a material slot id, deferring change
	// notifications until NotifyMeshUpdated() at the end. Reports the appended tri/vert counts.
	auto AppendBuf = [&Mesh](FGeometryScriptSimpleMeshBuffers& Buf, int32 MaterialID, int32& OutTri, int32& OutVert)
	{
		OutTri = Buf.Triangles.Num();
		OutVert = Buf.Vertices.Num();
		FGeometryScriptIndexList NewTris;
		UGeometryScriptLibrary_MeshBasicEditFunctions::AppendBuffersToMesh(
			Mesh, Buf, NewTris, MaterialID, /*bDeferChangeNotifications=*/true, nullptr);
	};

	int32 RoadsProcessed = 0;
	int32 LanesEmitted = 0;
	int32 RisersEmitted = 0;
	int32 TotalMarkTri = 0;
	int32 JunctionFills = 0;
	int32 TotalTri = 0;
	int32 TotalVert = 0;

	// (1) Junction fills (pre-pass): triangulate every junction BEFORE the lane loop so we know
	// which junctions actually produced a closed fill patch. The lane loop then skips only the
	// connecting-road driving lanes of *filled* junctions (the patch replaces them). Junctions
	// whose fill fails keep their connecting-road lanes rendered individually below (no holes).
	TSet<int32> FilledJunctionIds;
	if (Settings.bGenerateJunctionPatches)
	{
		const int32 NumJunctions = (int32)Odr->GetNumOfJunctions();
		for (int32 ji = 0; ji < NumJunctions; ++ji)
		{
			roadmanager::Junction* J = Odr->GetJunctionByIdx(ji);
			if (!J) continue;
			FGeometryScriptSimpleMeshBuffers Buf;
			if (OpenDRIVEMesh::BuildJunctionFillBuffers(J, ZOff, Buf))
			{
				int32 Tri = 0, Vert = 0;
				AppendBuf(Buf, (int32)OpenDRIVEMesh::ERoadMatSlot::Asphalt, Tri, Vert);
				TotalTri += Tri;
				TotalVert += Vert;
				++JunctionFills;
				FilledJunctionIds.Add(J->GetId());
			}
		}
	}

	// (2) Main lane loop: per-lane surface strips + raised-curb risers + road marks.
	for (int32 ri = 0; ri < NumRoads; ++ri)
	{
		roadmanager::Road* Road = Odr->GetRoadByIdx(ri);
		if (!Road) continue;
		++RoadsProcessed;

		const bool bConnecting = OpenDRIVEMesh::IsConnectingRoad(Road);

		const int32 NumSections = Road->GetNumberOfLaneSections();
		for (int32 ls = 0; ls < NumSections; ++ls)
		{
			roadmanager::LaneSection* Sec = Road->GetLaneSectionByIdx(ls);
			if (!Sec) continue;

			const double SectionStartS = Sec->GetS();
			const TArray<double> SList = OpenDRIVEMesh::BuildSList(Road, Sec, SectionStartS, MaxStep, MinStep);
			if (SList.Num() < 2) continue;

			const int32 NumLanes = Sec->GetNumberOfLanes();
			for (int32 li = 0; li < NumLanes; ++li)
			{
				roadmanager::Lane* L = Sec->GetLaneByIdx(li);
				if (!L) continue;
				const int32 Lid = L->GetId();
				const roadmanager::Lane::LaneType LT = L->GetLaneType();

				// Driving lanes of connecting roads are replaced by the junction fill patch — skip
				// the whole lane (strip + marks) to avoid overlap, but ONLY when that junction
				// actually got a fill. Unfilled junctions fall through and render their lanes here.
				if (bConnecting && Settings.bGenerateJunctionPatches && OpenDRIVEMesh::IsDrivingLane((int32)LT)
					&& FilledJunctionIds.Contains(Road->GetJunction()))
				{
					continue;
				}

				// Surface strip: skip the reference line (zero width) and NONE lanes.
				if (Lid != 0 && LT != roadmanager::Lane::LANE_TYPE_NONE)
				{
					const bool bDriving = OpenDRIVEMesh::IsDrivingLane((int32)LT);
					if (bDriving || Settings.bGenerateNonDrivingLanes)
					{
						const int32 Slot = OpenDRIVEMesh::SlotForLaneType((int32)LT);
						const double TopZ = OpenDRIVEMesh::CurbTopZAddCm(L, CurbHeight);

						FGeometryScriptSimpleMeshBuffers Buf;
						if (OpenDRIVEMesh::BuildLaneSurfaceStripBuffers(Road, Sec, L, SList, ZOff, TopZ, Buf))
						{
							int32 Tri = 0, Vert = 0;
							AppendBuf(Buf, Slot, Tri, Vert);
							TotalTri += Tri;
							TotalVert += Vert;
							++LanesEmitted;

							// Raised-curb riser: close the vertical step at this lane's inner edge
							// when its top elevation differs from the inner neighbour's.
							const int32 InnerId = (Lid > 0) ? (Lid - 1) : (Lid + 1);
							roadmanager::Lane* InnerLane = Sec->GetLaneById(InnerId);
							const double Tin = OpenDRIVEMesh::CurbTopZAddCm(InnerLane, CurbHeight);
							if (FMath::Abs(TopZ - Tin) > 1e-3)
							{
								FGeometryScriptSimpleMeshBuffers RiserBuf;
								if (OpenDRIVEMesh::BuildCurbRiserBuffers(Road, Sec, L, SList, ZOff, Tin, TopZ, RiserBuf))
								{
									int32 RTri = 0, RVert = 0;
									AppendBuf(RiserBuf, (int32)OpenDRIVEMesh::ERoadMatSlot::Border, RTri, RVert);
									TotalTri += RTri;
									TotalVert += RVert;
									++RisersEmitted;
								}
							}
						}
					}
				}

				// Road marks (centerline lane 0 included).
				if (Settings.bGenerateMarkings)
				{
					int32 MarkTris = 0;
					OpenDRIVEMesh::AppendRoadMarksForLane(
						Mesh, Road, Sec, L, SectionStartS, Sec->GetLength(),
						ZOff, MarkOff, MaxStep, (int32)OpenDRIVEMesh::ERoadMatSlot::Marking, MarkTris);
					TotalMarkTri += MarkTris;
					TotalTri += MarkTris;
				}
			}
		}
	}

	// (3) Elevated deck structure: turn elevated road spans into a bridge (thick slab + parapets +
	// piers). Opt-in. The top surface is the lane mesh built above; here we add the underside/fascia,
	// edge parapets, and support piers down to GroundZCm, skipping any pier on (or near) a road below.
	int32 DeckSpans = 0, PiersBuilt = 0, PiersSkipped = 0;
	if (Settings.bGenerateDeckStructure)
	{
		const double GroundZ = Settings.GroundZCm;
		const double ThrCm = Settings.DeckHeightThresholdMeters * 100.0;
		const int32 DeckSlot = (int32)OpenDRIVEMesh::ERoadMatSlot::Structure;

		auto SurfZAt = [&](roadmanager::Road* R, double s) -> double
		{
			return OpenDRIVEMesh::EvalLanePoint(R, s, 0.0, ZOff).Z;
		};

		// Full-width XY footprint of every road, with its Z range. Built from the SAME outer-edge
		// polylines used for the deck so the parapet adjacency probe lines up with where decks meet.
		struct FRoadFootprint { TArray<FVector2D> Poly; double ZMin; double ZMax; int32 RoadIdx; };
		TArray<FRoadFootprint> Footprints;
		for (int32 ri = 0; ri < NumRoads; ++ri)
		{
			roadmanager::Road* R = Odr->GetRoadByIdx(ri);
			if (!R || R->GetLength() <= KINDA_SMALL_NUMBER) continue;
			const TArray<double> FSList = OpenDRIVEMesh::BuildRoadSListAllSections(R, MaxStep, MinStep);
			if (FSList.Num() < 2) continue;
			TArray<FVector> FL, FR;
			if (!OpenDRIVEMesh::BuildRoadEdgePolylines(R, FSList, ZOff, FL, FR)) continue;
			FRoadFootprint FP;
			FP.RoadIdx = ri;
			FP.ZMin = TNumericLimits<double>::Max();
			FP.ZMax = -TNumericLimits<double>::Max();
			FP.Poly.Reserve(FL.Num() + FR.Num());
			auto AddPt = [&](const FVector& P) { FP.Poly.Add(FVector2D(P.X, P.Y)); FP.ZMin = FMath::Min(FP.ZMin, (double)P.Z); FP.ZMax = FMath::Max(FP.ZMax, (double)P.Z); };
			for (int32 k = 0; k < FL.Num(); ++k) AddPt(FL[k]);
			for (int32 k = FR.Num() - 1; k >= 0; --k) AddPt(FR[k]); // close: left fwd + right reversed
			if (FP.Poly.Num() >= 3) Footprints.Add(MoveTemp(FP));
		}

		// Drop free-flag runs shorter than this many stations: kills the short interior parapet
		// stubs left by marginal per-station adjacency tests near merges / span ends.
		auto CleanShortRuns = [](TArray<bool>& Free, int32 MinRun)
		{
			const int32 n = Free.Num();
			int32 i = 0;
			while (i < n)
			{
				if (!Free[i]) { ++i; continue; }
				int32 j = i;
				while (j < n && Free[j]) ++j; // [i, j) is a free run
				if (j - i < MinRun) { for (int32 k = i; k < j; ++k) Free[k] = false; }
				i = j;
			}
		};
		const double ClearCm = Settings.PierRoadClearanceMeters * 100.0;
		const double ClearSq = ClearCm * ClearCm;
		const double GroundBandTop = GroundZ + ThrCm; // footprints staying below this are under-roads
		const double ParapetProbeCm = 150.0;          // outboard probe distance for a neighbour deck
		const double ParapetZBandCm = Settings.DeckThicknessCm + 50.0; // only same-level neighbours count

		auto PierBlocked = [&](double X, double Y) -> bool
		{
			const FVector2D P(X, Y);
			for (const FRoadFootprint& FP : Footprints)
			{
				if (FP.ZMax > GroundBandTop) continue; // only avoid near-ground (under) roads
				if (PointInPoly2D(FP.Poly, P)) return true;
				if (ClearCm > 0.0 && DistToPolyEdgesSq2D(FP.Poly, P) < ClearSq) return true;
			}
			return false;
		};

		// True when the deck edge point is a free outer edge (open air outboard -> wants a parapet).
		auto EdgeFree = [&](int32 SelfIdx, const FVector& EdgePt, const FVector& OutwardDir) -> bool
		{
			const FVector2D Q(EdgePt.X + OutwardDir.X * ParapetProbeCm, EdgePt.Y + OutwardDir.Y * ParapetProbeCm);
			for (const FRoadFootprint& FP : Footprints)
			{
				if (FP.RoadIdx == SelfIdx) continue;
				if (EdgePt.Z < FP.ZMin - ParapetZBandCm || EdgePt.Z > FP.ZMax + ParapetZBandCm) continue;
				if (PointInPoly2D(FP.Poly, Q)) return false; // covered outboard -> interior edge
			}
			return true;
		};

		for (int32 ri = 0; ri < NumRoads; ++ri)
		{
			roadmanager::Road* R = Odr->GetRoadByIdx(ri);
			if (!R) continue;
			const double Len = R->GetLength();
			if (Len <= KINDA_SMALL_NUMBER) continue;

			const TArray<double> SList = OpenDRIVEMesh::BuildRoadSListAllSections(R, MaxStep, MinStep);
			const int32 M = SList.Num();
			if (M < 2) continue;

			// Walk contiguous on-deck runs (surface above ground+threshold).
			int32 i = 0;
			while (i < M)
			{
				if (SurfZAt(R, SList[i]) - GroundZ <= ThrCm) { ++i; continue; }
				int32 j = i;
				while (j + 1 < M && SurfZAt(R, SList[j + 1]) - GroundZ > ThrCm) ++j;
				if (j - i + 1 >= 2)
				{
					TArray<double> Sub;
					Sub.Reserve(j - i + 1);
					for (int32 k = i; k <= j; ++k) Sub.Add(SList[k]);

					TArray<FVector> Left, Right;
					if (OpenDRIVEMesh::BuildRoadEdgePolylines(R, Sub, ZOff, Left, Right))
					{
						++DeckSpans;

						FGeometryScriptSimpleMeshBuffers DeckBuf;
						OpenDRIVEMesh::BuildDeckBuffers(Left, Right, Settings.DeckThicknessCm, DeckBuf);
						{ int32 T = 0, V = 0; AppendBuf(DeckBuf, DeckSlot, T, V); TotalTri += T; TotalVert += V; }

						if (Settings.ParapetHeightCm > 0.0)
						{
							// Per-station: emit a parapet only where the edge faces open air, not
							// where another deck abuts (parallel/merging ramps share an interior seam).
							const int32 NE = FMath::Min(Left.Num(), Right.Num());
							TArray<bool> LeftFree, RightFree;
							LeftFree.SetNum(NE); RightFree.SetNum(NE);
							for (int32 e = 0; e < NE; ++e)
							{
								FVector OutL = (Left[e] - Right[e]); OutL.Z = 0; OutL = OutL.GetSafeNormal();
								FVector OutR = (Right[e] - Left[e]); OutR.Z = 0; OutR = OutR.GetSafeNormal();
								LeftFree[e]  = EdgeFree(ri, Left[e], OutL);
								RightFree[e] = EdgeFree(ri, Right[e], OutR);
							}
							// Remove isolated short free runs (the stubs at merges / deck ends).
							CleanShortRuns(LeftFree, 4);
							CleanShortRuns(RightFree, 4);

							FGeometryScriptSimpleMeshBuffers ParBuf;
							OpenDRIVEMesh::BuildParapetBuffers(Left, Right, Settings.ParapetHeightCm, Settings.ParapetThicknessCm, LeftFree, RightFree, ParBuf);
							int32 T = 0, V = 0; AppendBuf(ParBuf, DeckSlot, T, V); TotalTri += T; TotalVert += V;
						}

						// Support piers at fixed arc-length spacing along the run centerline.
						const double SpacingCm = FMath::Max((double)Settings.PierSpacingMeters * 100.0, 200.0);
						double Accum = SpacingCm; // place the first pier ~one spacing in
						const int32 NN = FMath::Min(Left.Num(), Right.Num());
						for (int32 k = 1; k < NN; ++k)
						{
							const FVector MidPrev = (Left[k - 1] + Right[k - 1]) * 0.5;
							const FVector MidCur  = (Left[k]     + Right[k])     * 0.5;
							Accum += FVector::Dist(MidPrev, MidCur);
							if (Accum < SpacingCm) continue;
							Accum = 0.0;

							const double DeckUnderZ = MidCur.Z - Settings.DeckThicknessCm;
							if (DeckUnderZ - GroundZ < 50.0) continue; // too short to be worth a pier
							if (PierBlocked(MidCur.X, MidCur.Y)) { ++PiersSkipped; continue; }

							FGeometryScriptSimpleMeshBuffers PierBuf;
							OpenDRIVEMesh::BuildPierBuffers(MidCur.X, MidCur.Y, DeckUnderZ, GroundZ, Settings.PierHalfWidthCm, PierBuf);
							int32 T = 0, V = 0; AppendBuf(PierBuf, DeckSlot, T, V); TotalTri += T; TotalVert += V;
							++PiersBuilt;
						}
					}
				}
				i = j + 1;
			}
		}
	}

	// (4) At-grade road slab: gives every non-elevated road a vertical thickness so the drivable
	// surface is a real volume instead of a paper-thin ribbon. Reuses BuildDeckBuffers with a
	// smaller thickness. When the deck structure is also on we partition each road's SList into
	// at-grade vs on-deck runs and only emit the slab on at-grade runs (no overlap with the deck).
	int32 SlabSpans = 0;
	if (Settings.RoadThicknessCm > 0.0)
	{
		const double SlabT = Settings.RoadThicknessCm;
		const int32 SlabSlot = (int32)OpenDRIVEMesh::ERoadMatSlot::Structure;
		const bool bSkipOnDeck = Settings.bGenerateDeckStructure;
		const double GroundZ = Settings.GroundZCm;
		const double ThrCm = Settings.DeckHeightThresholdMeters * 100.0;

		auto SurfZAt = [&](roadmanager::Road* R, double s) -> double
		{
			return OpenDRIVEMesh::EvalLanePoint(R, s, 0.0, ZOff).Z;
		};
		auto IsOnDeck = [&](roadmanager::Road* R, double s) -> bool
		{
			return bSkipOnDeck && (SurfZAt(R, s) - GroundZ > ThrCm);
		};

		for (int32 ri = 0; ri < NumRoads; ++ri)
		{
			roadmanager::Road* R = Odr->GetRoadByIdx(ri);
			if (!R || R->GetLength() <= KINDA_SMALL_NUMBER) continue;
			const TArray<double> SList = OpenDRIVEMesh::BuildRoadSListAllSections(R, MaxStep, MinStep);
			const int32 M = SList.Num();
			if (M < 2) continue;

			// Walk contiguous at-grade runs (mirrors the on-deck walk above but inverted).
			int32 i = 0;
			while (i < M)
			{
				if (IsOnDeck(R, SList[i])) { ++i; continue; }
				int32 j = i;
				while (j + 1 < M && !IsOnDeck(R, SList[j + 1])) ++j;
				if (j - i + 1 >= 2)
				{
					TArray<double> Sub;
					Sub.Reserve(j - i + 1);
					for (int32 k = i; k <= j; ++k) Sub.Add(SList[k]);

					TArray<FVector> Left, Right;
					if (OpenDRIVEMesh::BuildRoadEdgePolylines(R, Sub, ZOff, Left, Right))
					{
						++SlabSpans;
						FGeometryScriptSimpleMeshBuffers SlabBuf;
						OpenDRIVEMesh::BuildDeckBuffers(Left, Right, SlabT, SlabBuf);
						int32 T = 0, V = 0; AppendBuf(SlabBuf, SlabSlot, T, V); TotalTri += T; TotalVert += V;
					}
				}
				i = j + 1;
			}
		}
	}

	// Normals are supplied per-vertex by each Build*Buffers call and written straight into the
	// normal overlay by AppendBuffersToMesh. We deliberately do NOT call RecomputeNormals here.
	Actor->MeshComp->NotifyMeshUpdated();

	GeneratedActors.Add(Actor);

	LastReport = FString::Printf(
		TEXT("Roads=%d Lanes=%d Risers=%d MarkTris=%d JunctionFills=%d/%d DeckSpans=%d Piers=%d (skipped=%d) SlabSpans=%d (thk=%.0fcm) Vertices=%d Triangles=%d"),
		RoadsProcessed, LanesEmitted, RisersEmitted, TotalMarkTri, JunctionFills, (int32)Odr->GetNumOfJunctions(),
		DeckSpans, PiersBuilt, PiersSkipped, SlabSpans, Settings.RoadThicknessCm, TotalVert, TotalTri);
	UE_LOG(LogOpenDriveRoadMesh, Log, TEXT("GenerateRoadMesh: %s"), *LastReport);
}

void FRoadMeshGenerator::ClearGeneratedMeshes()
{
	for (TWeakObjectPtr<ARoadMeshActor>& Weak : GeneratedActors)
	{
		if (ARoadMeshActor* A = Weak.Get())
		{
			A->Destroy();
		}
	}
	GeneratedActors.Empty();
}

ARoadMeshActor* FRoadMeshGenerator::GetLatestActor() const
{
	for (int32 i = GeneratedActors.Num() - 1; i >= 0; --i)
	{
		if (ARoadMeshActor* A = GeneratedActors[i].Get())
		{
			return A;
		}
	}
	return nullptr;
}

void FRoadMeshGenerator::EnableCollisionOnAll()
{
	for (TWeakObjectPtr<ARoadMeshActor>& Weak : GeneratedActors)
	{
		if (ARoadMeshActor* A = Weak.Get())
		{
			A->EnableCollision();
		}
	}
}
