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

	/** Flip each triangle's winding so its UE front face points up. Roads and markings are
	 *  walked-on surfaces, so the top face should always be the front face; this stops
	 *  one-sided materials from being culled (or rendered inside-out) from above. It replaces
	 *  the old per-lane-sign hardcoded winding, which got the centerline (lane 0) and one
	 *  road side backwards. Vertex normals are forced up separately, so geometry facing and
	 *  shading normals stay consistent.
	 *
	 *  Sign note: UE is left-handed and its front-face normal is the NEGATIVE of the
	 *  textbook cross product (v1-v0)x(v2-v0). So the front face points up exactly when that
	 *  cross product's Z is NEGATIVE; we flip when it is positive. (Verified against the
	 *  original working lane winding, whose cross-Z is negative.) */
	void OrientTrianglesUp(FGeometryScriptSimpleMeshBuffers& Buf)
	{
		for (FIntVector& T : Buf.Triangles)
		{
			const FVector& v0 = Buf.Vertices[T.X];
			const FVector& v1 = Buf.Vertices[T.Y];
			const FVector& v2 = Buf.Vertices[T.Z];
			if (FVector::CrossProduct(v1 - v0, v2 - v0).Z > 0.0)
			{
				Swap(T.Y, T.Z);
			}
		}
	}

	/** Ear-clip a simple polygon (XY) given in order. Index triples into Poly. Handles concave
	 *  polygons without a centroid hub, so it triangulates the junction outline directly. */
	void EarClipPolygon(const TArray<FVector>& Poly, TArray<FIntVector>& OutTris)
	{
		const int n = Poly.Num();
		if (n < 3) return;
		auto Cross = [](const FVector& a, const FVector& b, const FVector& c)
		{ return (b.X - a.X) * (c.Y - a.Y) - (b.Y - a.Y) * (c.X - a.X); };

		double area2 = 0.0;
		for (int i = 0; i < n; ++i) { const FVector& a = Poly[i]; const FVector& b = Poly[(i + 1) % n]; area2 += a.X * b.Y - b.X * a.Y; }
		TArray<int> V; V.SetNum(n);
		for (int i = 0; i < n; ++i) V[i] = (area2 >= 0.0) ? i : (n - 1 - i);

		auto InTri = [&](const FVector& p, const FVector& a, const FVector& b, const FVector& c)
		{
			const double d1 = Cross(p, a, b), d2 = Cross(p, b, c), d3 = Cross(p, c, a);
			const bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
			const bool pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
			return !(neg && pos);
		};

		int guard = 0;
		while (V.Num() > 3 && guard++ < 100000)
		{
			const int m = V.Num();
			bool clipped = false;
			for (int k = 0; k < m; ++k)
			{
				const int i0 = V[(k + m - 1) % m], i1 = V[k], i2 = V[(k + 1) % m];
				const FVector& a = Poly[i0]; const FVector& b = Poly[i1]; const FVector& c = Poly[i2];
				if (Cross(a, b, c) <= 0.0) continue;
				bool ear = true;
				for (int j = 0; j < m; ++j)
				{
					const int vj = V[j];
					if (vj == i0 || vj == i1 || vj == i2) continue;
					if (InTri(Poly[vj], a, b, c)) { ear = false; break; }
				}
				if (!ear) continue;
				OutTris.Add(FIntVector(i0, i1, i2));
				V.RemoveAt(k);
				clipped = true;
				break;
			}
			if (!clipped) break;
		}
		if (V.Num() == 3) OutTris.Add(FIntVector(V[0], V[1], V[2]));
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

	/** The road linked at the connecting road's far end (the end NOT joined to Incoming).
	 *  Implemented with plain RoadLink lookups because the newer
	 *  Junction::GetRoadAtOtherEndOfConnectingRoad is declared in the header but is NOT
	 *  exported by the prebuilt RoadManager.lib this plugin links against. */
	roadmanager::Road* RoadAtOtherEnd(roadmanager::Road* Connecting, roadmanager::Road* Incoming)
	{
		if (!Connecting || !Incoming) return nullptr;
		roadmanager::OpenDrive* Odr = roadmanager::Position::GetOpenDrive();
		if (!Odr) return nullptr;
		const int InId = Incoming->GetId();
		const roadmanager::LinkType Ends[2] = { roadmanager::PREDECESSOR, roadmanager::SUCCESSOR };
		for (roadmanager::LinkType T : Ends)
		{
			roadmanager::RoadLink* Lnk = Connecting->GetLink(T);
			if (!Lnk) continue;
			if (Lnk->GetElementType() != roadmanager::RoadLink::ELEMENT_TYPE_ROAD) continue;
			const int Eid = Lnk->GetElementId();
			if (Eid == InId) continue; // this end is the incoming road
			if (roadmanager::Road* R = Odr->GetRoadById(Eid)) return R;
		}
		return nullptr;
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

	// Per-vertex normals from the local surface frame (along-s x across-t), forced upward.
	// We write them into the buffer instead of relying on a post-build RecomputeNormals:
	// AppendBuffersToMesh on a normal-less buffer left the normal overlay unset, so the
	// DynamicMeshComponent fell back to tangent-derived normals — which is why the surface
	// shaded flat/grey ("hazy") and the World-Normal debug view showed the road-direction
	// (tangent) colour instead of up-blue.
	OutBuffers.Normals.SetNum(N * 2);
	for (int32 i = 0; i < N; ++i)
	{
		int32 a = i, b = i + 1;
		if (b >= N) { a = i - 1; b = i; }                 // last row: backward difference
		const FVector AlongS  = OutBuffers.Vertices[2 * b] - OutBuffers.Vertices[2 * a];
		const FVector AcrossT = OutBuffers.Vertices[2 * i + 1] - OutBuffers.Vertices[2 * i];
		FVector Nrm = FVector::CrossProduct(AlongS, AcrossT);
		if (!Nrm.Normalize()) Nrm = FVector::UpVector;
		if (Nrm.Z < 0.f) Nrm = -Nrm;                      // road surface normals point up
		OutBuffers.Normals[2 * i]     = Nrm;
		OutBuffers.Normals[2 * i + 1] = Nrm;
	}

	// Emit two triangles per quad with a provisional winding; OrientTrianglesUp() below
	// flips any that end up facing down, so the visible face is always the top regardless
	// of lane side or the CoordTranslate Y-flip.
	for (int32 i = 0; i + 1 < N; ++i)
	{
		const int32 A = 2 * i;       // inner at s_i
		const int32 B = 2 * i + 1;   // outer at s_i
		const int32 C = 2 * (i + 1); // inner at s_{i+1}
		const int32 D = 2 * (i + 1) + 1; // outer at s_{i+1}

		OutBuffers.Triangles.Add(FIntVector(A, C, B));
		OutBuffers.Triangles.Add(FIntVector(B, C, D));
		OutBuffers.TriGroupIDs.Add(LaneId);
		OutBuffers.TriGroupIDs.Add(LaneId);
	}

	OrientTrianglesUp(OutBuffers);

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

		// Per-vertex up-facing normals (see BuildLaneStripBuffers for the rationale).
		B.Normals.SetNum(N * 2);
		for (int32 i = 0; i < N; ++i)
		{
			int32 a = i, b = i + 1;
			if (b >= N) { a = i - 1; b = i; }
			const FVector AlongS  = B.Vertices[2 * b] - B.Vertices[2 * a];
			const FVector AcrossT = B.Vertices[2 * i + 1] - B.Vertices[2 * i];
			FVector Nrm = FVector::CrossProduct(AlongS, AcrossT);
			if (!Nrm.Normalize()) Nrm = FVector::UpVector;
			if (Nrm.Z < 0.f) Nrm = -Nrm;
			B.Normals[2 * i]     = Nrm;
			B.Normals[2 * i + 1] = Nrm;
		}

		for (int32 i = 0; i + 1 < N; ++i)
		{
			const int32 A = 2 * i;
			const int32 Bv = 2 * i + 1;
			const int32 C = 2 * (i + 1);
			const int32 D = 2 * (i + 1) + 1;
			B.Triangles.Add(FIntVector(A, C, Bv));
			B.Triangles.Add(FIntVector(Bv, C, D));
			B.TriGroupIDs.Add(LaneId);
			B.TriGroupIDs.Add(LaneId);
		}

		// Same up-orientation fix as the lane strips: the old per-sign winding rendered the
		// centerline (lane 0) and one side's marks inside-out.
		OrientTrianglesUp(B);

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

	// Incoming roads anchor the junction mouths. Each connecting road joins two of them (an
	// incoming arm and an outgoing arm); we keep that (In, Connecting, Out) topology so we
	// can pick, per corner, the connecting road that actually forms that corner instead of
	// guessing the outermost edge from a sampled envelope.
	struct FJConn { roadmanager::Road* In; roadmanager::Road* Con; roadmanager::Road* Out; };
	TArray<roadmanager::Road*> Incoming;
	TArray<FJConn> Conns;
	{
		TSet<roadmanager::Road*> SeenIn;
		const int nConn = Junction->GetNumberOfConnections();
		for (int ci = 0; ci < nConn; ++ci)
		{
			roadmanager::Connection* Conn = Junction->GetConnectionByIdx(ci);
			if (!Conn) continue;
			roadmanager::Road* IR = Conn->GetIncomingRoad();
			roadmanager::Road* CR = Conn->GetConnectingRoad();
			if (!IR || !CR) continue;
			if (!SeenIn.Contains(IR)) { SeenIn.Add(IR); Incoming.Add(IR); }
			roadmanager::Road* OR = RoadAtOtherEnd(CR, IR);
			Conns.Add({ IR, CR, OR });
		}
	}
	if (Incoming.Num() < 2) return false;

	// Drivable t-range [Lo,Hi] across all driving lanes of a section at s.
	auto DrivingTRange = [&](roadmanager::LaneSection* Sec, double s, bool& bOK, double& Lo, double& Hi)
	{
		bOK = false; Lo = 0.0; Hi = 0.0;
		if (!Sec) return;
		const int nLanes = Sec->GetNumberOfLanes();
		for (int li = 0; li < nLanes; ++li)
		{
			roadmanager::Lane* L = Sec->GetLaneByIdx(li);
			if (!L) continue;
			const int lid = L->GetId();
			if (lid == 0 || !IsDrivingLane((int32)L->GetLaneType())) continue;
			const int sgn = (lid > 0) ? 1 : -1;
			const double outer = sgn * Sec->GetOuterOffset(s, lid);
			const double inner = sgn * Sec->GetOuterOffset(s, lid - sgn);
			const double a = FMath::Min(outer, inner), b = FMath::Max(outer, inner);
			if (!bOK) { Lo = a; Hi = b; bOK = true; }
			else { Lo = FMath::Min(Lo, a); Hi = FMath::Max(Hi, b); }
		}
	};

	// Per-arm gate (full driving span at the junction-facing s) + centroid + angle.
	struct FArm { roadmanager::Road* Road; FVector Mid; double Ang; };
	TArray<FArm> Arms;
	TArray<FVector> GatePts;
	for (roadmanager::Road* A : Incoming)
	{
		double s = 0.0; roadmanager::LaneSection* Sec = nullptr;
		if (!GetJunctionFacingSection(A, jid, s, Sec)) continue;
		bool ok; double Lo, Hi; DrivingTRange(Sec, s, ok, Lo, Hi);
		if (!ok) continue;
		const FVector GL = EvalLanePoint(A, s, Hi, Settings.ZOffsetCm);
		const FVector GR = EvalLanePoint(A, s, Lo, Settings.ZOffsetCm);
		if (GL.ContainsNaN() || GR.ContainsNaN()) continue;
		GatePts.Add(GL); GatePts.Add(GR);
		FArm Arm; Arm.Road = A; Arm.Mid = (GL + GR) * 0.5; Arm.Ang = 0.0; Arms.Add(Arm);
	}
	if (GatePts.Num() < 3 || Arms.Num() < 2) return false;

	FVector Centroid = FVector::ZeroVector;
	for (const FVector& P : GatePts) Centroid += P;
	Centroid /= (double)GatePts.Num();
	if (Centroid.ContainsNaN()) return false;

	for (FArm& A : Arms) A.Ang = FMath::Atan2(A.Mid.Y - Centroid.Y, A.Mid.X - Centroid.X);
	Arms.Sort([](const FArm& X, const FArm& Y) { return X.Ang < Y.Ang; });

	// Outer edge of a connecting road = the driving-strip edge (left=max-t or right=min-t)
	// with the larger mean distance from the centroid. OutMinR = its closest approach to the
	// centroid: large => the road hugs/rounds a corner; small => it dives through the centre.
	// Only DISTANCES are compared, so this is invariant under the CoordTranslate Y-flip
	// (which previously flipped a left/right test and produced the bow-tie).
	auto OuterEdgeMinR = [&](roadmanager::Road* CR, TArray<FVector>& OutEdge, double& OutMinR)
	{
		OutEdge.Reset(); OutMinR = -1.0;
		if (!CR) return;
		const double len = CR->GetLength();
		if (len <= KINDA_SMALL_NUMBER) return;
		const int ns = FMath::Max(4, (int)(len / 0.4) + 1);
		TArray<FVector> Le, Re;
		for (int i = 0; i < ns; ++i)
		{
			const double s = len * (double)i / (double)(ns - 1);
			roadmanager::LaneSection* Sec = CR->GetLaneSectionByS(s);
			bool ok; double Lo, Hi; DrivingTRange(Sec, s, ok, Lo, Hi);
			if (!ok) continue;
			const FVector PL = EvalLanePoint(CR, s, Hi, Settings.ZOffsetCm);
			const FVector PR = EvalLanePoint(CR, s, Lo, Settings.ZOffsetCm);
			if (!PL.ContainsNaN()) Le.Add(PL);
			if (!PR.ContainsNaN()) Re.Add(PR);
		}
		if (Le.Num() < 2 && Re.Num() < 2) return;
		auto MeanR = [&](const TArray<FVector>& P) { double a = 0.0; for (const FVector& q : P) a += FVector::Dist2D(q, Centroid); return P.Num() ? a / P.Num() : 0.0; };
		auto MinR  = [&](const TArray<FVector>& P) { double m = 1e30; for (const FVector& q : P) m = FMath::Min(m, (double)FVector::Dist2D(q, Centroid)); return m; };
		OutEdge = (MeanR(Le) >= MeanR(Re)) ? Le : Re;
		OutMinR = MinR(OutEdge);
	};

	// Boundary loop: for each angularly-adjacent arm pair, the connecting road joining them
	// whose outer edge hugs the corner most (largest OutMinR). Arcs chained in arm order.
	TArray<FVector> Loop;
	const int NA = Arms.Num();
	for (int i = 0; i < NA; ++i)
	{
		roadmanager::Road* A = Arms[i].Road;
		roadmanager::Road* B = Arms[(i + 1) % NA].Road;
		TArray<FVector> Best; double BestMin = -1.0;
		for (const FJConn& C : Conns)
		{
			if (!C.Con) continue;
			if (!((C.In == A && C.Out == B) || (C.In == B && C.Out == A))) continue;
			TArray<FVector> E; double mn; OuterEdgeMinR(C.Con, E, mn);
			if (E.Num() >= 2 && mn > BestMin) { BestMin = mn; Best = E; }
		}
		if (Best.Num() < 2) continue;
		// orient A -> B so consecutive arcs chain end-to-end.
		if (FVector::Dist2D(Best[0], Arms[i].Mid) > FVector::Dist2D(Best.Last(), Arms[i].Mid))
			for (int x = 0, y = Best.Num() - 1; x < y; ++x, --y) Swap(Best[x], Best[y]);
		Loop.Append(Best);
	}
	if (Loop.Num() < 3) return false;

	// Triangulate the chained outline by ear clipping (handles the concave outline, no hub).
	TArray<FIntVector> Tris;
	EarClipPolygon(Loop, Tris);
	if (Tris.Num() < 1) return false;

	FVector LC = FVector::ZeroVector;
	for (const FVector& P : Loop) LC += P;
	LC /= (double)Loop.Num();
	double RMax = 1.0;
	for (const FVector& P : Loop) RMax = FMath::Max(RMax, (double)FVector::Dist2D(P, LC));

	const int32 Mat = (int32)ERoadMeshMaterialSlot::Asphalt;
	const int N = Loop.Num();
	OutBuffers.Vertices.Reserve(N);
	OutBuffers.Normals.Reserve(N);
	OutBuffers.UV0.Reserve(N);
	OutBuffers.Triangles.Reserve(Tris.Num());
	OutBuffers.TriGroupIDs.Reserve(Tris.Num());
	for (const FVector& P : Loop)
	{
		OutBuffers.Vertices.Add(P);
		OutBuffers.Normals.Add(FVector::UpVector);
		OutBuffers.UV0.Add(FVector2D(0.5f + 0.5f * (float)((P.X - LC.X) / RMax), 0.5f + 0.5f * (float)((P.Y - LC.Y) / RMax)));
	}
	for (const FIntVector& T : Tris)
	{
		OutBuffers.Triangles.Add(T);
		OutBuffers.TriGroupIDs.Add(Mat);
	}

	OrientTrianglesUp(OutBuffers);

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

	// Normals are supplied per-vertex by each Build*Buffers call and written straight into
	// the normal overlay by AppendBuffersToMesh. We deliberately do NOT call RecomputeNormals
	// here: on this assembled mesh it was leaving the overlay effectively unset (the component
	// then fell back to tangent-derived normals -> flat/grey "hazy" shading). The component's
	// AutoCalculated tangents are derived from these normals + the UVs.

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
