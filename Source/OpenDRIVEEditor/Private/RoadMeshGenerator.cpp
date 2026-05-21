// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoadMeshGenerator.h"

#include "RoadMeshActor.h"
#include "CoordTranslate.h"
#include "RoadManager.hpp"

#include "Engine/World.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/AssetManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogOpenDriveRoadMesh, Log, All);

namespace
{
	/** Sample a single (s, t) position on a road into a UE world-space FVector (in cm). */
	FVector EvalLanePoint(roadmanager::Road* Road, double s, double tOffset, double ZOffsetCm)
	{
		roadmanager::Position P;
		P.SetTrackPos(Road->GetId(), s, tOffset, true);
		FVector L = CoordTranslate::OdrToUe::ToLocation(P);
		L.Z += ZOffsetCm;
		return L;
	}

	/** True when this Road is a "connecting road" inside a junction (xodr <road junction="N"> with N >= 0). */
	bool IsConnectingRoad(roadmanager::Road* R)
	{
		return R && R->GetJunction() >= 0;
	}

	/** True only for LANE_TYPE_DRIVING. Other paved types (bidirectional, ramps) and
	 *  non-paved types (sidewalk, biking, shoulder, parking, border, etc.) keep their
	 *  per-lane strip even on connecting roads — the junction fill only replaces the
	 *  driving surface. */
	bool IsDrivingLane(int32 LaneType)
	{
		return LaneType == (int32)roadmanager::Lane::LaneType::LANE_TYPE_DRIVING;
	}

	/** If Road R touches Junction(jid) via SUCC or PRED, fill OutS with the junction-facing
	 *  s value (0 or road length) and OutSec with the lane section at that s. */
	bool GetJunctionFacingSection(
		roadmanager::Road* R, int jid,
		double& OutS, roadmanager::LaneSection*& OutSec)
	{
		OutS = 0.0;
		OutSec = nullptr;
		if (!R) return false;

		auto Touches = [&](roadmanager::LinkType T) {
			roadmanager::RoadLink* L = R->GetLink(T);
			return L
				&& L->GetElementType() == roadmanager::RoadLink::ELEMENT_TYPE_JUNCTION
				&& L->GetElementId() == jid;
		};

		const int n = R->GetNumberOfLaneSections();
		if (n <= 0) return false;

		if (Touches(roadmanager::SUCCESSOR))
		{
			OutS = R->GetLength();
			OutSec = R->GetLaneSectionByIdx(n - 1);
		}
		else if (Touches(roadmanager::PREDECESSOR))
		{
			OutS = 0.0;
			OutSec = R->GetLaneSectionByIdx(0);
		}
		return OutSec != nullptr;
	}

	/**
	 * Scan a Content folder for UMaterialInterface assets and assign them to ERoadMeshMaterialSlot
	 * by name keyword. Matching is case-insensitive on the asset name.
	 *   Asphalt/Driving/Road  -> slot 0 (Asphalt)
	 *   Sidewalk/Walk         -> slot 1 (Sidewalk)
	 *   Border/Shoulder/Curb  -> slot 2 (Border)
	 *   Mark/Line/Stripe      -> slot 3 (Marking)
	 *   otherwise             -> slot 4 (Misc)
	 *
	 * Returns an array sized to the number of slots, with nullptr where no match was found.
	 */
	TArray<UMaterialInterface*> DiscoverMaterialsAtPath(const FString& ContentFolderPath)
	{
		TArray<UMaterialInterface*> Out;
		Out.Init(nullptr, 5);

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
			         Name.Contains(TEXT("Road"),    ESearchCase::IgnoreCase)) SlotMatch = 0;
			else if (Name.Contains(TEXT("Sidewalk"), ESearchCase::IgnoreCase) ||
			         Name.Contains(TEXT("Walk"),     ESearchCase::IgnoreCase)) SlotMatch = 1;
			else if (Name.Contains(TEXT("Border"),   ESearchCase::IgnoreCase) ||
			         Name.Contains(TEXT("Shoulder"), ESearchCase::IgnoreCase) ||
			         Name.Contains(TEXT("Curb"),     ESearchCase::IgnoreCase)) SlotMatch = 2;
			else if (Name.Contains(TEXT("Mark"),   ESearchCase::IgnoreCase) ||
			         Name.Contains(TEXT("Line"),   ESearchCase::IgnoreCase) ||
			         Name.Contains(TEXT("Stripe"), ESearchCase::IgnoreCase)) SlotMatch = 3;
			else
				SlotMatch = 4;

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

TArray<double> FRoadMeshGenerator::BuildSList(roadmanager::Road* Road, roadmanager::LaneSection* Sec, double SectionStartS) const
{
	TArray<double> SList;
	if (!Road || !Sec) return SList;

	const double SecLen = Sec->GetLength();
	const double SecEnd = SectionStartS + SecLen;
	const double MaxStep = FMath::Max((double)Settings.MaxStepMeters, 0.05);
	const double MinStep = FMath::Max((double)Settings.MinStepMeters, 0.01);

	// Curvature-adaptive: union of OSI s-values from every lane in this section.
	// OSI points are pre-sampled by esmini with curvature awareness.
	TSet<double> SSet;
	SSet.Add(SectionStartS);
	SSet.Add(SecEnd);

	const int32 NumLanes = Sec->GetNumberOfLanes();
	for (int32 li = 0; li < NumLanes; ++li)
	{
		roadmanager::Lane* L = Sec->GetLaneByIdx(li);
		if (!L) continue;
		auto* OSI = L->GetOSIPoints();
		if (!OSI) continue;
		const auto& Points = OSI->GetPoints();
		for (const auto& P : Points)
		{
			if (P.s >= SectionStartS - 1e-6 && P.s <= SecEnd + 1e-6)
			{
				SSet.Add(FMath::Clamp((double)P.s, SectionStartS, SecEnd));
			}
		}
	}

	SList = SSet.Array();
	SList.Sort();

	// Enforce MaxStep by inserting splits where gaps exceed it
	TArray<double> Out;
	Out.Reserve(SList.Num() * 2);
	for (int32 i = 0; i < SList.Num(); ++i)
	{
		if (i > 0)
		{
			const double Prev = SList[i - 1];
			const double Cur = SList[i];
			const double Gap = Cur - Prev;
			if (Gap > MaxStep)
			{
				const int32 NumSplits = (int32)FMath::FloorToDouble(Gap / MaxStep);
				const double Step = Gap / (NumSplits + 1);
				for (int32 k = 1; k <= NumSplits; ++k)
				{
					Out.Add(Prev + k * Step);
				}
			}
		}
		Out.Add(SList[i]);
	}

	// Coalesce too-close samples
	TArray<double> Final;
	Final.Reserve(Out.Num());
	for (int32 i = 0; i < Out.Num(); ++i)
	{
		if (Final.Num() == 0 || (Out[i] - Final.Last()) > MinStep || i == Out.Num() - 1)
		{
			if (Final.Num() > 0 && i == Out.Num() - 1 && (Out[i] - Final.Last()) <= MinStep)
			{
				Final.Last() = Out[i];
			}
			else
			{
				Final.Add(Out[i]);
			}
		}
	}
	return Final;
}

bool FRoadMeshGenerator::BuildLaneStripBuffers(
	roadmanager::Road* Road,
	roadmanager::LaneSection* Sec,
	roadmanager::Lane* Lane,
	const TArray<double>& SList,
	FGeometryScriptSimpleMeshBuffers& OutBuffers,
	int32& OutMaterialID) const
{
	const int32 LaneId = Lane->GetId();
	if (LaneId == 0) return false;

	const int32 InnerId = (LaneId > 0) ? (LaneId - 1) : (LaneId + 1);
	const double TotalLen = Road->GetLength();

	const int32 N = SList.Num();
	if (N < 2) return false;

	OutBuffers.Vertices.Reserve(N * 2);
	OutBuffers.UV0.Reserve(N * 2);

	// SIGN: esmini's GetOuterOffset always returns the absolute distance from the
	// reference line (see RoadManager.cpp). Real t is signed by lane side:
	// positive lane -> +t (left of heading), negative lane -> -t (right of heading).
	// Match roadgeom.cpp:733 which does: SIGN(lane->GetId()) * GetOuterOffset(...).
	const int32 Sign = (LaneId > 0) ? 1 : -1;

	bool bHasAnyWidth = false;

	for (int32 i = 0; i < N; ++i)
	{
		const double s = SList[i];
		const double tInner = Sign * Sec->GetOuterOffset(s, InnerId);
		const double tOuter = Sign * Sec->GetOuterOffset(s, LaneId);

		const FVector PInner = EvalLanePoint(Road, s, tInner, Settings.ZOffsetCm);
		const FVector POuter = EvalLanePoint(Road, s, tOuter, Settings.ZOffsetCm);

		OutBuffers.Vertices.Add(PInner);
		OutBuffers.Vertices.Add(POuter);

		const float U = (TotalLen > 0.0) ? (float)(s / TotalLen) : 0.f;
		OutBuffers.UV0.Add(FVector2D(U, 0.0f));
		OutBuffers.UV0.Add(FVector2D(U, 1.0f));

		if (FMath::Abs(tOuter - tInner) > 1e-4)
		{
			bHasAnyWidth = true;
		}
	}

	if (!bHasAnyWidth) return false;

	// Triangle winding: UE (left-handed) renders the FRONT face when vertices wind clockwise
	// when viewed from the front. After the CoordTranslate Y-flip, the lane geometry inherits
	// the OpenDRIVE-side handedness — so we use the opposite winding from my first attempt.
	for (int32 i = 0; i + 1 < N; ++i)
	{
		const int32 A = 2 * i;       // inner at s_i
		const int32 B = 2 * i + 1;   // outer at s_i
		const int32 C = 2 * (i + 1); // inner at s_{i+1}
		const int32 D = 2 * (i + 1) + 1; // outer at s_{i+1}

		if (LaneId > 0)
		{
			OutBuffers.Triangles.Add(FIntVector(A, C, B));
			OutBuffers.Triangles.Add(FIntVector(B, C, D));
		}
		else
		{
			OutBuffers.Triangles.Add(FIntVector(A, B, C));
			OutBuffers.Triangles.Add(FIntVector(B, D, C));
		}
		OutBuffers.TriGroupIDs.Add(LaneId);
		OutBuffers.TriGroupIDs.Add(LaneId);
	}

	OutMaterialID = FRoadMeshSettings::SlotForLaneType((int32)Lane->GetLaneType());
	return true;
}

void FRoadMeshGenerator::AppendRoadMarksForLane(
	UDynamicMesh* Mesh,
	roadmanager::Road* Road,
	roadmanager::LaneSection* Sec,
	roadmanager::Lane* Lane,
	double SectionStartS,
	double SectionLength,
	int32& OutMarkTriCount) const
{
	OutMarkTriCount = 0;
	if (!Mesh || !Lane) return;

	const int32 LaneId = Lane->GetId();
	// NOTE: lane 0 (reference line) commonly carries the centerline mark — do NOT skip it here.

	const int32 NumMarks = Lane->GetNumberOfRoadMarks();
	if (NumMarks == 0) return;

	const double SectionEndS = SectionStartS + SectionLength;
	const double TotalLen = Road->GetLength();
	const float Z = Settings.ZOffsetCm + Settings.MarkingZOffsetCm;
	const float StepM = FMath::Max((float)Settings.MaxStepMeters, 0.1f);

	// SIGN: GetOuterOffset returns unsigned distance from reference line — apply lane side.
	// For lane 0 (reference line), the outer offset is 0, so the sign is irrelevant.
	const int32 Sign = (LaneId >= 0) ? 1 : -1;

	// Helper: emit a continuous strip from s0..s1 along the lane's outer edge,
	// with width (m) and color-derived material slot.
	auto EmitMarkStrip = [&](double s0, double s1, double widthM, int32 MatID)
	{
		if (widthM <= 0.0) return;
		s0 = FMath::Clamp(s0, SectionStartS, SectionEndS);
		s1 = FMath::Clamp(s1, SectionStartS, SectionEndS);
		if (s1 - s0 < 1e-3) return;

		// Build s-list across [s0, s1]
		TArray<double> SS;
		for (double s = s0; s < s1 - KINDA_SMALL_NUMBER; s += StepM)
		{
			SS.Add(s);
		}
		SS.Add(s1);

		const int32 N = SS.Num();
		if (N < 2) return;

		FGeometryScriptSimpleMeshBuffers B;
		B.Vertices.Reserve(N * 2);
		B.UV0.Reserve(N * 2);

		const double Half = widthM * 0.5;

		for (int32 i = 0; i < N; ++i)
		{
			const double s = SS[i];
			const double tOuter = Sign * Sec->GetOuterOffset(s, LaneId);
			// Mark straddles the outer edge: [tOuter - half, tOuter + half] in t-space.
			const double tLow = tOuter - Half;
			const double tHigh = tOuter + Half;

			const FVector PLow  = EvalLanePoint(Road, s, tLow, Z);
			const FVector PHigh = EvalLanePoint(Road, s, tHigh, Z);
			B.Vertices.Add(PLow);
			B.Vertices.Add(PHigh);

			const float U = (TotalLen > 0.0) ? (float)(s / TotalLen) : 0.f;
			B.UV0.Add(FVector2D(U, 0.0f));
			B.UV0.Add(FVector2D(U, 1.0f));
		}

		for (int32 i = 0; i + 1 < N; ++i)
		{
			const int32 A = 2 * i;
			const int32 Bv = 2 * i + 1;
			const int32 C = 2 * (i + 1);
			const int32 D = 2 * (i + 1) + 1;
			if (LaneId > 0)
			{
				B.Triangles.Add(FIntVector(A, C, Bv));
				B.Triangles.Add(FIntVector(Bv, C, D));
			}
			else
			{
				B.Triangles.Add(FIntVector(A, Bv, C));
				B.Triangles.Add(FIntVector(Bv, D, C));
			}
			B.TriGroupIDs.Add(LaneId);
			B.TriGroupIDs.Add(LaneId);
		}

		FGeometryScriptIndexList NewTris;
		UGeometryScriptLibrary_MeshBasicEditFunctions::AppendBuffersToMesh(
			Mesh, B, NewTris, MatID, /*bDeferChangeNotifications=*/true, nullptr);
		OutMarkTriCount += B.Triangles.Num();
	};

	for (int32 mi = 0; mi < NumMarks; ++mi)
	{
		roadmanager::LaneRoadMark* RM = Lane->GetLaneRoadMarkByIdx(mi);
		if (!RM) continue;
		const auto RType = RM->GetType();
		if (RType == roadmanager::LaneRoadMark::RoadMarkType::NONE_TYPE) continue;

		// Range along s for this mark: from s_offset to the next mark's s_offset (or section end).
		const double MarkStart = SectionStartS + RM->GetSOffset();
		double MarkEnd = SectionEndS;
		if (mi + 1 < NumMarks)
		{
			roadmanager::LaneRoadMark* Next = Lane->GetLaneRoadMarkByIdx(mi + 1);
			if (Next) MarkEnd = SectionStartS + Next->GetSOffset();
		}
		if (MarkEnd <= MarkStart) continue;

		const int32 MatID = (int32)ERoadMeshMaterialSlot::Marking;
		const double Width = (RM->GetWidth() > 0.0) ? RM->GetWidth() : 0.12;  // default 12cm

		// If the mark has explicit type lines with dashed length/space, treat as BROKEN.
		// Otherwise emit per-type.
		auto EmitBroken = [&](double SegLen, double SegSpace, double LineWidth)
		{
			if (SegLen <= 0.0) SegLen = 3.0;
			if (SegSpace < 0.0) SegSpace = 3.0;
			for (double s = MarkStart; s < MarkEnd - KINDA_SMALL_NUMBER; s += SegLen + SegSpace)
			{
				const double e = FMath::Min(s + SegLen, MarkEnd);
				EmitMarkStrip(s, e, LineWidth, MatID);
			}
		};

		switch (RType)
		{
		case roadmanager::LaneRoadMark::RoadMarkType::SOLID:
		case roadmanager::LaneRoadMark::RoadMarkType::CURB:
			EmitMarkStrip(MarkStart, MarkEnd, Width, MatID);
			break;
		case roadmanager::LaneRoadMark::RoadMarkType::BROKEN:
			EmitBroken(3.0, 3.0, Width);
			break;
		case roadmanager::LaneRoadMark::RoadMarkType::SOLID_SOLID:
			// Approximate as a single thicker line for now
			EmitMarkStrip(MarkStart, MarkEnd, Width * 2.5, MatID);
			break;
		case roadmanager::LaneRoadMark::RoadMarkType::SOLID_BROKEN:
		case roadmanager::LaneRoadMark::RoadMarkType::BROKEN_SOLID:
			EmitMarkStrip(MarkStart, MarkEnd, Width, MatID);
			EmitBroken(3.0, 3.0, Width);
			break;
		case roadmanager::LaneRoadMark::RoadMarkType::BROKEN_BROKEN:
			EmitBroken(3.0, 3.0, Width);
			break;
		case roadmanager::LaneRoadMark::RoadMarkType::BOTTS_DOTS:
		case roadmanager::LaneRoadMark::RoadMarkType::GRASS:
		default:
			// Skip for now; needs special geometry
			break;
		}
	}
}

bool FRoadMeshGenerator::BuildJunctionFillBuffers(
	roadmanager::Junction* Junction,
	FGeometryScriptSimpleMeshBuffers& OutBuffers,
	int32& OutMaterialID) const
{
	if (!Junction) return false;
	const int jid = Junction->GetId();

	// Collect distinct incoming roads (for the centroid + arm bounds) and distinct
	// connecting roads (whose outer edges shape the curved boundary).
	TArray<roadmanager::Road*> Incoming;
	TArray<roadmanager::Road*> Connecting;
	{
		TSet<roadmanager::Road*> SeenIn, SeenCon;
		const int nConn = Junction->GetNumberOfConnections();
		for (int ci = 0; ci < nConn; ++ci)
		{
			roadmanager::Connection* Conn = Junction->GetConnectionByIdx(ci);
			if (!Conn) continue;
			if (roadmanager::Road* IR = Conn->GetIncomingRoad())
			{
				if (!SeenIn.Contains(IR)) { SeenIn.Add(IR); Incoming.Add(IR); }
			}
			if (roadmanager::Road* CR = Conn->GetConnectingRoad())
			{
				if (!SeenCon.Contains(CR)) { SeenCon.Add(CR); Connecting.Add(CR); }
			}
		}
	}
	if (Incoming.Num() < 1) return false;

	// Find the outermost Driving lane ids (left/right) at (R, s).
	auto FindDrivingEdgeLanes = [&](roadmanager::LaneSection* Sec, int& OutLeft, int& OutRight)
	{
		OutLeft = 0; OutRight = 0;
		if (!Sec) return;
		const int nLanes = Sec->GetNumberOfLanes();
		for (int li = 0; li < nLanes; ++li)
		{
			roadmanager::Lane* L = Sec->GetLaneByIdx(li);
			if (!L) continue;
			const int lid = L->GetId();
			if (lid == 0) continue;
			if (!IsDrivingLane((int32)L->GetLaneType())) continue;
			if (lid > OutLeft)  OutLeft = lid;
			if (lid < OutRight) OutRight = lid;
		}
	};

	// Both outermost driving-lane edge points at (R, s). Skips non-finite results.
	auto BothEdges = [&](roadmanager::Road* R, double s, roadmanager::LaneSection* Sec,
	                     bool& bHasL, FVector& L, bool& bHasR, FVector& Rt)
	{
		bHasL = bHasR = false;
		if (!R || !Sec) return;
		int leftmost, rightmost;
		FindDrivingEdgeLanes(Sec, leftmost, rightmost);
		if (leftmost > 0)
		{
			const FVector P = EvalLanePoint(R, s, +Sec->GetOuterOffset(s, leftmost), Settings.ZOffsetCm);
			if (!P.ContainsNaN()) { L = P; bHasL = true; }
		}
		if (rightmost < 0)
		{
			const FVector P = EvalLanePoint(R, s, -Sec->GetOuterOffset(s, rightmost), Settings.ZOffsetCm);
			if (!P.ContainsNaN()) { Rt = P; bHasR = true; }
		}
	};

	// Incoming gates: both driving edges at each arm. Anchor the arms + stable centroid.
	TArray<FVector> Gates;
	for (roadmanager::Road* R : Incoming)
	{
		double s = 0.0;
		roadmanager::LaneSection* Sec = nullptr;
		if (!GetJunctionFacingSection(R, jid, s, Sec)) continue;
		bool bL, bR; FVector L, Rt;
		BothEdges(R, s, Sec, bL, L, bR, Rt);
		if (bL) Gates.Add(L);
		if (bR) Gates.Add(Rt);
	}
	if (Gates.Num() < 2) return false;

	FVector Centroid = FVector::ZeroVector;
	for (const FVector& P : Gates) Centroid += P;
	Centroid /= (double)Gates.Num();

	auto Radius2D = [&](const FVector& P) {
		const double dx = P.X - Centroid.X, dy = P.Y - Centroid.Y;
		return FMath::Sqrt(dx * dx + dy * dy);
	};

	// Candidates = incoming gates + connecting-road OUTER edges. For each connecting
	// road sample we keep only the edge point FARTHER from the centroid: a connecting
	// road sweeps through the junction interior, so its inner edge (and mid-span points
	// near the centre) are interior — feeding them to the envelope produced the
	// inward "sea-urchin" spikes. The outer edge alone traces the rounded corner.
	TArray<FVector> Candidates = Gates;
	for (roadmanager::Road* CR : Connecting)
	{
		const double len = CR->GetLength();
		if (len <= KINDA_SMALL_NUMBER) continue;
		const double step = FMath::Clamp(len / 8.0, 0.5, 2.0);
		for (double s = 0.0; s <= len + 1e-3; s += step)
		{
			const double ss = FMath::Min(s, len);
			roadmanager::LaneSection* Sec = CR->GetLaneSectionByS(ss);
			bool bL, bR; FVector L, Rt;
			BothEdges(CR, ss, Sec, bL, L, bR, Rt);
			if (bL && bR)      Candidates.Add(Radius2D(L) >= Radius2D(Rt) ? L : Rt);
			else if (bL)       Candidates.Add(L);
			else if (bR)       Candidates.Add(Rt);
		}
	}

	// Radial-max envelope: bin candidates by angle around the centroid and keep the
	// farthest point per bin. Star-shaped by construction → always safe to fan.
	const int NumBins = 120;
	TArray<FVector> BinPt; BinPt.SetNum(NumBins);
	TArray<float>   BinR;  BinR.Init(-1.f, NumBins);
	for (const FVector& P : Candidates)
	{
		if (P.ContainsNaN()) continue;
		const float r = (float)Radius2D(P);
		const double ang = FMath::Atan2(P.Y - Centroid.Y, P.X - Centroid.X); // [-PI, PI]
		int bin = (int)FMath::FloorToDouble((ang + PI) / (2.0 * PI) * NumBins);
		bin = FMath::Clamp(bin, 0, NumBins - 1);
		if (r > BinR[bin]) { BinR[bin] = r; BinPt[bin] = P; }
	}

	// Non-empty bins, already in angular order.
	TArray<FVector> Boundary;
	Boundary.Reserve(NumBins);
	for (int b = 0; b < NumBins; ++b)
	{
		if (BinR[b] >= 0.f && !BinPt[b].ContainsNaN()) Boundary.Add(BinPt[b]);
	}
	if (Boundary.Num() < 3) return false;
	if (Centroid.ContainsNaN()) return false;

	const int N = Boundary.Num();

	// Decide winding from the polygon's signed area in XY so the fan always faces up
	// (otherwise a back-facing fill gets culled and the landscape shows through — which
	// looked like an un-materialed "tan" patch).
	double SignedArea2 = 0.0;
	for (int i = 0; i < N; ++i)
	{
		const FVector& P0 = Boundary[i];
		const FVector& P1 = Boundary[(i + 1) % N];
		SignedArea2 += (P0.X - Centroid.X) * (P1.Y - Centroid.Y)
		             - (P1.X - Centroid.X) * (P0.Y - Centroid.Y);
	}
	// UE is left-handed: a CW loop (negative signed area) fans into upward normals.
	const bool bReverse = (SignedArea2 > 0.0);

	OutBuffers.Vertices.Reserve(N + 1);
	OutBuffers.UV0.Reserve(N + 1);
	OutBuffers.Triangles.Reserve(N);
	OutBuffers.TriGroupIDs.Reserve(N);

	OutBuffers.Vertices.Add(Centroid);
	OutBuffers.UV0.Add(FVector2D(0.5f, 0.5f));
	for (int i = 0; i < N; ++i)
	{
		OutBuffers.Vertices.Add(Boundary[i]);
		const double ang = FMath::Atan2(Boundary[i].Y - Centroid.Y, Boundary[i].X - Centroid.X);
		OutBuffers.UV0.Add(FVector2D(
			(float)(0.5 + 0.5 * FMath::Cos(ang)),
			(float)(0.5 + 0.5 * FMath::Sin(ang))));
	}

	const int32 Mat = (int32)ERoadMeshMaterialSlot::Asphalt;
	for (int i = 0; i < N; ++i)
	{
		const int32 A = 0;
		const int32 B = 1 + i;
		const int32 C = 1 + ((i + 1) % N);

		// Skip degenerate (near-zero-area) triangles. Slivers have unstable face
		// normals, and RecomputeNormals averages those into the shared overlay,
		// which is what made the patch shade as if in shadow.
		const FVector& VA = OutBuffers.Vertices[A];
		const FVector& VB = OutBuffers.Vertices[B];
		const FVector& VC = OutBuffers.Vertices[C];
		const double Area2 = FVector::CrossProduct(VB - VA, VC - VA).Size();
		if (Area2 < 1.0) continue; // < ~0.5 cm^2 projected area

		OutBuffers.Triangles.Add(bReverse ? FIntVector(A, C, B) : FIntVector(A, B, C));
		OutBuffers.TriGroupIDs.Add(Mat);
	}

	if (OutBuffers.Triangles.Num() < 1) return false;

	OutMaterialID = Mat;
	return true;
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

	const size_t NumRoads = Odr->GetNumOfRoads();
	if (NumRoads == 0)
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
	// Always present 5 slots even if all nullptr (so baked StaticMesh gets explicit slot count).
	while (Mats.Num() < 5) Mats.Add(nullptr);
	Actor->DefaultMaterials = Mats;
	Actor->ApplyDefaultMaterials();

	UDynamicMesh* Mesh = Actor->MeshComp->GetDynamicMesh();
	if (!Mesh)
	{
		UE_LOG(LogOpenDriveRoadMesh, Error, TEXT("GenerateRoadMesh: actor has no DynamicMesh"));
		return;
	}
	Mesh->Reset();

	int32 TotalTri = 0;
	int32 TotalVert = 0;
	int32 TotalMarkTri = 0;
	TSet<int32> UsedMaterialIDs;
	int32 LanesEmitted = 0;
	int32 RoadsProcessed = 0;

	for (int32 r = 0; r < (int32)NumRoads; ++r)
	{
		roadmanager::Road* Road = Odr->GetRoadByIdx(r);
		if (!Road) continue;
		RoadsProcessed++;

		const int32 NumSections = Road->GetNumberOfLaneSections();
		for (int32 ls = 0; ls < NumSections; ++ls)
		{
			roadmanager::LaneSection* Sec = Road->GetLaneSectionByIdx(ls);
			if (!Sec) continue;

			const double SectionStartS = Sec->GetS();
			const TArray<double> SList = BuildSList(Road, Sec, SectionStartS);
			if (SList.Num() < 2) continue;

			const bool bConnecting = IsConnectingRoad(Road);

			for (int32 li = 0; li < Sec->GetNumberOfLanes(); ++li)
			{
				roadmanager::Lane* L = Sec->GetLaneByIdx(li);
				if (!L) continue;
				const int32 Lid = L->GetId();

				// Skip Driving lanes on connecting roads — the junction fill below
				// replaces them with one continuous patch. Other lane types (sidewalk,
				// biking, shoulder, ...) on connecting roads are still emitted so they
				// sit beside the fill.
				if (bConnecting && Settings.bGenerateJunctionPatches
					&& IsDrivingLane((int32)L->GetLaneType()))
				{
					continue;
				}

				// Surface strip: skip the reference line (zero width).
				if (Lid != 0)
				{
					FGeometryScriptSimpleMeshBuffers Buf;
					int32 Mat = 0;
					if (BuildLaneStripBuffers(Road, Sec, L, SList, Buf, Mat))
					{
						const int32 TriBefore = Buf.Triangles.Num();
						const int32 VertBefore = Buf.Vertices.Num();

						FGeometryScriptIndexList NewTris;
						UGeometryScriptLibrary_MeshBasicEditFunctions::AppendBuffersToMesh(
							Mesh, Buf, NewTris, Mat, /*bDeferChangeNotifications=*/true, nullptr);

						TotalTri += TriBefore;
						TotalVert += VertBefore;
						UsedMaterialIDs.Add(Mat);
						LanesEmitted++;
					}
				}

				// Road marks: also process for lane 0 — that's where the centerline mark lives.
				if (Settings.bGenerateMarkings)
				{
					int32 MarkTris = 0;
					AppendRoadMarksForLane(Mesh, Road, Sec, L, SectionStartS, Sec->GetLength(), MarkTris);
					if (MarkTris > 0)
					{
						TotalMarkTri += MarkTris;
						UsedMaterialIDs.Add((int32)ERoadMeshMaterialSlot::Marking);
					}
				}
			}
		}
	}

	// Junction fills: one continuous asphalt patch per junction, bounded by the
	// outermost Driving edges of every incoming road at the junction interface.
	// Driving lanes of the connecting roads inside the junction were skipped above,
	// so this patch replaces them rather than overlapping.
	int32 JunctionFillsEmitted = 0;
	int32 JunctionFillTris = 0;
	if (Settings.bGenerateJunctionPatches)
	{
		const int NumJunctions = Odr->GetNumOfJunctions();
		for (int ji = 0; ji < NumJunctions; ++ji)
		{
			roadmanager::Junction* J = Odr->GetJunctionByIdx(ji);
			if (!J) continue;

			FGeometryScriptSimpleMeshBuffers Buf;
			int32 Mat = 0;
			if (BuildJunctionFillBuffers(J, Buf, Mat))
			{
				const int32 TriBefore = Buf.Triangles.Num();
				const int32 VertBefore = Buf.Vertices.Num();

				FGeometryScriptIndexList NewTris;
				UGeometryScriptLibrary_MeshBasicEditFunctions::AppendBuffersToMesh(
					Mesh, Buf, NewTris, Mat, /*bDeferChangeNotifications=*/true, nullptr);

				TotalTri += TriBefore;
				TotalVert += VertBefore;
				JunctionFillTris += TriBefore;
				JunctionFillsEmitted++;
				UsedMaterialIDs.Add(Mat);
			}
		}
	}

	// Normals
	FGeometryScriptCalculateNormalsOptions NormOpt;
	UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, NormOpt, /*bDefer=*/false, nullptr);

	Actor->MeshComp->NotifyMeshUpdated();

	GeneratedActors.Add(Actor);

	FString MatList;
	for (int32 M : UsedMaterialIDs)
	{
		MatList += FString::Printf(TEXT("%d "), M);
	}

	LastReport = FString::Printf(
		TEXT("Roads=%d Lanes=%d Vertices=%d Triangles=%d MarkTris=%d JunctionFills=%d (%d tris) Materials=[%s]"),
		RoadsProcessed, LanesEmitted, TotalVert, TotalTri, TotalMarkTri,
		JunctionFillsEmitted, JunctionFillTris, *MatList);
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
