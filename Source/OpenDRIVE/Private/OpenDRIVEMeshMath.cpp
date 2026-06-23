// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenDRIVEMeshMath.h"

#include "Misc/ScopeLock.h"
#include "UDynamicMesh.h"
#include "GeometryScript/MeshBasicEditFunctions.h"

// OpenDRIVE plugin. CoordTranslate.h transitively includes RoadManager.hpp.
#include "OpenDriveAsset.h"
#include "CoordTranslate.h"

namespace
{
	// --- Junction helpers (file-local; ported from FRoadMeshGenerator) -------

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
}

namespace OpenDRIVEMesh
{
	bool EnsureLoaded(const UOpenDriveAsset* Asset)
	{
		static FCriticalSection LoadCS;
		static uint32 LastLoadedHash = 0;
		static bool bLastLoadOk = false;

		FScopeLock Lock(&LoadCS);

		if (!Asset)
		{
			return false;
		}

		const uint32 Hash = GetTypeHash(Asset->XodrContent);
		if (Hash == LastLoadedHash)
		{
			return bLastLoadOk;
		}

		roadmanager::OpenDrive* Odr = roadmanager::Position::GetOpenDrive();
		bLastLoadOk = Odr && Odr->LoadOpenDriveContent(TCHAR_TO_UTF8(*Asset->XodrContent));
		LastLoadedHash = Hash;
		return bLastLoadOk;
	}

	FVector EvalLanePoint(roadmanager::Road* Road, double s, double tOffset, double ZOffsetCm)
	{
		roadmanager::Position P;
		P.SetTrackPos(Road->GetId(), s, tOffset, true);
		FVector L = CoordTranslate::OdrToUe::ToLocation(P);
		L.Z += ZOffsetCm;
		return L;
	}

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

	TArray<double> BuildSList(
		roadmanager::Road* Road,
		roadmanager::LaneSection* Sec,
		double SectionStartS,
		double MaxStepMeters,
		double MinStepMeters)
	{
		TArray<double> SList;
		if (!Road || !Sec)
		{
			return SList;
		}

		const double SecLen = Sec->GetLength();
		const double SecEnd = SectionStartS + SecLen;
		const double MaxStep = FMath::Max(MaxStepMeters, 0.05);
		const double MinStep = FMath::Max(MinStepMeters, 0.01);

		// Curvature-adaptive: union of OSI s-values from every lane in this section.
		// OSI points are pre-sampled by esmini with curvature awareness.
		TSet<double> SSet;
		SSet.Add(SectionStartS);
		SSet.Add(SecEnd);

		const int32 NumLanes = Sec->GetNumberOfLanes();
		for (int32 li = 0; li < NumLanes; ++li)
		{
			roadmanager::Lane* L = Sec->GetLaneByIdx(li);
			if (!L)
			{
				continue;
			}
			auto* OSI = L->GetOSIPoints();
			if (!OSI)
			{
				continue;
			}
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

		// Enforce MaxStep by inserting splits where gaps exceed it.
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

		// Coalesce too-close samples.
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

	TArray<double> BuildRoadSListAllSections(
		roadmanager::Road* Road,
		double MaxStepMeters,
		double MinStepMeters)
	{
		TArray<double> Final;
		if (!Road)
		{
			return Final;
		}

		const double TotalLen = Road->GetLength();
		const double MaxStep = FMath::Max(MaxStepMeters, 0.05);
		const double MinStep = FMath::Max(MinStepMeters, 0.01);

		// Union OSI s-values across all sections + force section boundaries onto a sample.
		TSet<double> SSet;
		SSet.Add(0.0);
		SSet.Add(TotalLen);

		const int32 NumSections = Road->GetNumberOfLaneSections();
		for (int32 si = 0; si < NumSections; ++si)
		{
			roadmanager::LaneSection* Sec = Road->GetLaneSectionByIdx(si);
			if (!Sec)
			{
				continue;
			}
			const double SecStart = Sec->GetS();
			const double SecEnd = SecStart + Sec->GetLength();
			SSet.Add(FMath::Clamp(SecStart, 0.0, TotalLen));
			SSet.Add(FMath::Clamp(SecEnd, 0.0, TotalLen));

			const int32 NumLanes = Sec->GetNumberOfLanes();
			for (int32 li = 0; li < NumLanes; ++li)
			{
				roadmanager::Lane* L = Sec->GetLaneByIdx(li);
				if (!L)
				{
					continue;
				}
				auto* OSI = L->GetOSIPoints();
				if (!OSI)
				{
					continue;
				}
				for (const auto& P : OSI->GetPoints())
				{
					if (P.s >= -1e-6 && P.s <= TotalLen + 1e-6)
					{
						SSet.Add(FMath::Clamp((double)P.s, 0.0, TotalLen));
					}
				}
			}
		}

		TArray<double> Sorted = SSet.Array();
		Sorted.Sort();

		// Enforce MaxStep by inserting splits in oversized gaps.
		TArray<double> Capped;
		Capped.Reserve(Sorted.Num() * 2);
		for (int32 i = 0; i < Sorted.Num(); ++i)
		{
			if (i > 0)
			{
				const double Gap = Sorted[i] - Sorted[i - 1];
				if (Gap > MaxStep)
				{
					const int32 N = (int32)FMath::FloorToDouble(Gap / MaxStep);
					const double Step = Gap / (N + 1);
					for (int32 k = 1; k <= N; ++k)
					{
						Capped.Add(Sorted[i - 1] + k * Step);
					}
				}
			}
			Capped.Add(Sorted[i]);
		}

		// Coalesce samples closer than MinStep, but always preserve the true road end.
		Final.Reserve(Capped.Num());
		for (int32 i = 0; i < Capped.Num(); ++i)
		{
			const bool bLast = (i == Capped.Num() - 1);
			if (Final.Num() == 0 || (Capped[i] - Final.Last()) > MinStep)
			{
				Final.Add(Capped[i]);
			}
			else if (bLast)
			{
				Final.Last() = Capped[i];
			}
		}
		return Final;
	}

	int32 SlotForLaneType(int32 LaneTypeFlag)
	{
		using LT = roadmanager::Lane::LaneType;

		const int32 DrivingMask =
			(int32)LT::LANE_TYPE_DRIVING |
			(int32)LT::LANE_TYPE_ENTRY |
			(int32)LT::LANE_TYPE_EXIT |
			(int32)LT::LANE_TYPE_OFF_RAMP |
			(int32)LT::LANE_TYPE_ON_RAMP |
			(int32)LT::LANE_TYPE_BIDIRECTIONAL |
			(int32)LT::LANE_TYPE_BIKING |
			(int32)LT::LANE_TYPE_PARKING |
			(int32)LT::LANE_TYPE_RESTRICTED |
			(int32)LT::LANE_TYPE_STOP |
			(int32)LT::LANE_TYPE_CONNECTING_RAMP;

		if (LaneTypeFlag & (int32)LT::LANE_TYPE_SIDEWALK) return (int32)ERoadMatSlot::Sidewalk;
		if (LaneTypeFlag & ((int32)LT::LANE_TYPE_BORDER | (int32)LT::LANE_TYPE_SHOULDER | (int32)LT::LANE_TYPE_CURB)) return (int32)ERoadMatSlot::Border;
		if (LaneTypeFlag & DrivingMask) return (int32)ERoadMatSlot::Asphalt;
		return (int32)ERoadMatSlot::Misc;
	}

	double CurbTopZAddCm(roadmanager::Lane* Lane, double CurbHeightCm)
	{
		if (!Lane || CurbHeightCm <= 0.0)
		{
			return 0.0;
		}
		using LT = roadmanager::Lane::LaneType;
		const int32 t = (int32)Lane->GetLaneType();
		// Sidewalk and curb/border lanes form the raised pedestrian level; everything
		// else (driving, shoulder, parking, none, ...) stays at road level.
		if (t & ((int32)LT::LANE_TYPE_SIDEWALK | (int32)LT::LANE_TYPE_BORDER | (int32)LT::LANE_TYPE_CURB))
		{
			return CurbHeightCm;
		}
		return 0.0;
	}

	bool IsConnectingRoad(roadmanager::Road* R)
	{
		return R && R->GetJunction() >= 0;
	}

	bool IsDrivingLane(int32 LaneType)
	{
		return LaneType == (int32)roadmanager::Lane::LaneType::LANE_TYPE_DRIVING;
	}

	bool BuildRoadSurfaceRing(
		roadmanager::Road* Road,
		double SampleStepMeters,
		int32 LaneTypeMask,
		TArray<FVector>& OutRing)
	{
		OutRing.Reset();
		if (!Road)
		{
			return false;
		}
		const double LengthM = Road->GetLength();
		if (LengthM <= KINDA_SMALL_NUMBER)
		{
			return false;
		}
		const double Step = FMath::Max(SampleStepMeters, 0.25);

		// Signed t-range [Lo, Hi] across the section, counting only lanes whose type
		// bit is set in LaneTypeMask (lane 0 excluded). Considers each lane's inner and
		// outer offset so a one-sided road still spans from the centerline.
		auto MaskedTRange = [&](roadmanager::LaneSection* Sec, double s, bool& bOK, double& Lo, double& Hi)
		{
			bOK = false; Lo = 0.0; Hi = 0.0;
			if (!Sec) { return; }
			const int nLanes = Sec->GetNumberOfLanes();
			for (int li = 0; li < nLanes; ++li)
			{
				roadmanager::Lane* L = Sec->GetLaneByIdx(li);
				if (!L) { continue; }
				const int lid = L->GetId();
				if (lid == 0) { continue; }
				const int32 LT = (int32)L->GetLaneType();
				if ((LT & LaneTypeMask) == 0) { continue; }

				const int sgn = (lid > 0) ? 1 : -1;
				const double outer = sgn * Sec->GetOuterOffset(s, lid);
				const double inner = sgn * Sec->GetOuterOffset(s, lid - sgn);
				const double a = FMath::Min(outer, inner), b = FMath::Max(outer, inner);
				if (!bOK) { Lo = a; Hi = b; bOK = true; }
				else { Lo = FMath::Min(Lo, a); Hi = FMath::Max(Hi, b); }
			}
		};

		TArray<FVector> LeftEdge;  // s ascending (Hi side)
		TArray<FVector> RightEdge; // s ascending (Lo side; reversed when closing)
		for (double s = 0.0; ; s += Step)
		{
			const double sc = FMath::Min(s, LengthM);
			roadmanager::LaneSection* Sec = Road->GetLaneSectionByS(sc);
			bool bOK = false; double Lo = 0.0, Hi = 0.0;
			MaskedTRange(Sec, sc, bOK, Lo, Hi);
			if (bOK)
			{
				const FVector PL = EvalLanePoint(Road, sc, Hi, /*ZOffsetCm=*/0.0);
				const FVector PR = EvalLanePoint(Road, sc, Lo, /*ZOffsetCm=*/0.0);
				if (!PL.ContainsNaN()) { LeftEdge.Add(PL); }
				if (!PR.ContainsNaN()) { RightEdge.Add(PR); }
			}
			if (sc >= LengthM)
			{
				break;
			}
		}

		if (LeftEdge.Num() < 2 || RightEdge.Num() < 2)
		{
			return false;
		}
		OutRing.Reserve(LeftEdge.Num() + RightEdge.Num());
		for (int32 i = 0; i < LeftEdge.Num(); ++i)       { OutRing.Add(LeftEdge[i]); }
		for (int32 i = RightEdge.Num() - 1; i >= 0; --i) { OutRing.Add(RightEdge[i]); }
		return OutRing.Num() >= 3;
	}

	bool BuildLaneSurfaceStripBuffers(
		roadmanager::Road* Road,
		roadmanager::LaneSection* Sec,
		roadmanager::Lane* Lane,
		const TArray<double>& SList,
		double ZOffsetCm,
		double TopZAddCm,
		FGeometryScriptSimpleMeshBuffers& OutBuffers)
	{
		const int32 LaneId = Lane->GetId();
		if (LaneId == 0)
		{
			return false;
		}

		const int32 InnerId = (LaneId > 0) ? (LaneId - 1) : (LaneId + 1);
		const double TotalLen = Road->GetLength();
		const double ZTop = ZOffsetCm + TopZAddCm;

		const int32 N = SList.Num();
		if (N < 2)
		{
			return false;
		}

		OutBuffers.Vertices.Reserve(N * 2);
		OutBuffers.UV0.Reserve(N * 2);

		// SIGN: esmini's GetOuterOffset returns the absolute distance from the
		// reference line. Real t is signed by lane side: positive lane -> +t
		// (left of heading), negative lane -> -t (right of heading).
		const int32 Sign = (LaneId > 0) ? 1 : -1;

		bool bHasAnyWidth = false;

		for (int32 i = 0; i < N; ++i)
		{
			const double s = SList[i];
			const double tInner = Sign * Sec->GetOuterOffset(s, InnerId);
			const double tOuter = Sign * Sec->GetOuterOffset(s, LaneId);

			const FVector PInner = EvalLanePoint(Road, s, tInner, ZTop);
			const FVector POuter = EvalLanePoint(Road, s, tOuter, ZTop);

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

		if (!bHasAnyWidth)
		{
			return false;
		}

		// Per-vertex normals from the local surface frame (along-s x across-t),
		// forced upward, written straight into the buffer so AppendBuffersToMesh
		// sets the normal overlay (otherwise the component falls back to
		// tangent-derived normals and shades flat/grey).
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

		// Two triangles per quad with a provisional winding; OrientTrianglesUp()
		// flips any that end up facing down so the visible face is always the top.
		for (int32 i = 0; i + 1 < N; ++i)
		{
			const int32 A = 2 * i;           // inner at s_i
			const int32 B = 2 * i + 1;       // outer at s_i
			const int32 C = 2 * (i + 1);     // inner at s_{i+1}
			const int32 D = 2 * (i + 1) + 1; // outer at s_{i+1}

			OutBuffers.Triangles.Add(FIntVector(A, C, B));
			OutBuffers.Triangles.Add(FIntVector(B, C, D));
			OutBuffers.TriGroupIDs.Add(LaneId);
			OutBuffers.TriGroupIDs.Add(LaneId);
		}

		OrientTrianglesUp(OutBuffers);
		return true;
	}

	bool BuildCurbRiserBuffers(
		roadmanager::Road* Road,
		roadmanager::LaneSection* Sec,
		roadmanager::Lane* Lane,
		const TArray<double>& SList,
		double ZOffsetCm,
		double FromZAddCm,
		double ToZAddCm,
		FGeometryScriptSimpleMeshBuffers& OutBuffers)
	{
		const int32 LaneId = Lane->GetId();
		if (LaneId == 0)
		{
			return false;
		}
		if (FMath::Abs(ToZAddCm - FromZAddCm) < 1e-3)
		{
			return false; // no elevation step -> no riser
		}

		const int32 N = SList.Num();
		if (N < 2)
		{
			return false;
		}

		const int32 InnerId = (LaneId > 0) ? (LaneId - 1) : (LaneId + 1);
		const int32 Sign = (LaneId > 0) ? 1 : -1;
		const double TotalLen = Road->GetLength();
		const double ZBottom = ZOffsetCm + FromZAddCm;
		const double ZTopAdd = ZOffsetCm + ToZAddCm;

		OutBuffers.Vertices.Reserve(N * 2);
		OutBuffers.UV0.Reserve(N * 2);

		// A second sample nudged toward the road interior at each s, used to point
		// the (horizontal) riser normals back toward the carriageway.
		TArray<FVector> Inward;
		Inward.Reserve(N);

		for (int32 i = 0; i < N; ++i)
		{
			const double s = SList[i];
			const double tInner = Sign * Sec->GetOuterOffset(s, InnerId);

			const FVector Pb = EvalLanePoint(Road, s, tInner, ZBottom);
			const FVector Pt = EvalLanePoint(Road, s, tInner, ZTopAdd);
			OutBuffers.Vertices.Add(Pb);
			OutBuffers.Vertices.Add(Pt);

			const float U = (TotalLen > 0.0) ? (float)(s / TotalLen) : 0.f;
			OutBuffers.UV0.Add(FVector2D(U, 0.0f));
			OutBuffers.UV0.Add(FVector2D(U, 1.0f));

			const double tNudge = tInner - Sign * 0.5; // 0.5 m toward the road centre
			Inward.Add(EvalLanePoint(Road, s, tNudge, ZBottom));
		}

		// Horizontal road-facing normals.
		OutBuffers.Normals.SetNum(N * 2);
		for (int32 i = 0; i < N; ++i)
		{
			int32 a = i, b = i + 1;
			if (b >= N) { a = i - 1; b = i; }
			const FVector AlongS = OutBuffers.Vertices[2 * b] - OutBuffers.Vertices[2 * a];
			FVector Nrm = FVector::CrossProduct(AlongS, FVector::UpVector);
			if (!Nrm.Normalize()) Nrm = FVector::UpVector;
			const FVector InwardDir = Inward[i] - OutBuffers.Vertices[2 * i];
			if (FVector::DotProduct(Nrm, InwardDir) < 0.0) Nrm = -Nrm; // face the road
			OutBuffers.Normals[2 * i]     = Nrm;
			OutBuffers.Normals[2 * i + 1] = Nrm;
		}

		for (int32 i = 0; i + 1 < N; ++i)
		{
			const int32 A = 2 * i;           // bottom at s_i
			const int32 B = 2 * i + 1;       // top at s_i
			const int32 C = 2 * (i + 1);     // bottom at s_{i+1}
			const int32 D = 2 * (i + 1) + 1; // top at s_{i+1}

			OutBuffers.Triangles.Add(FIntVector(A, B, C));
			OutBuffers.Triangles.Add(FIntVector(B, D, C));
			OutBuffers.TriGroupIDs.Add(LaneId);
			OutBuffers.TriGroupIDs.Add(LaneId);
		}

		// Orient each face toward its (road-facing) vertex normal. UE's front face
		// is the negative of the textbook cross product, so flip when it disagrees.
		for (FIntVector& T : OutBuffers.Triangles)
		{
			const FVector& v0 = OutBuffers.Vertices[T.X];
			const FVector& v1 = OutBuffers.Vertices[T.Y];
			const FVector& v2 = OutBuffers.Vertices[T.Z];
			const FVector FaceN = FVector::CrossProduct(v1 - v0, v2 - v0);
			const FVector VN = OutBuffers.Normals[T.X];
			if (FVector::DotProduct(-FaceN, VN) < 0.0)
			{
				Swap(T.Y, T.Z);
			}
		}

		return true;
	}

	void AppendRoadMarksForLane(
		UDynamicMesh* Mesh,
		roadmanager::Road* Road,
		roadmanager::LaneSection* Sec,
		roadmanager::Lane* Lane,
		double SectionStartS,
		double SectionLength,
		double ZOffsetCm,
		double MarkingZOffsetCm,
		double MaxStepMeters,
		int32 MarkingSlot,
		int32& OutMarkTriCount)
	{
		OutMarkTriCount = 0;
		if (!Mesh || !Lane) return;

		const int32 LaneId = Lane->GetId();
		// NOTE: lane 0 (reference line) commonly carries the centerline mark — do NOT skip it here.

		const int32 NumMarks = Lane->GetNumberOfRoadMarks();
		if (NumMarks == 0) return;

		const double SectionEndS = SectionStartS + SectionLength;
		const double TotalLen = Road->GetLength();
		const double Z = ZOffsetCm + MarkingZOffsetCm;
		const float StepM = FMath::Max((float)MaxStepMeters, 0.1f);

		// SIGN: GetOuterOffset returns unsigned distance from reference line — apply lane side.
		// For lane 0 (reference line), the outer offset is 0, so the sign is irrelevant.
		const int32 Sign = (LaneId >= 0) ? 1 : -1;

		// Helper: emit a continuous strip from s0..s1 along the lane's outer edge.
		auto EmitMarkStrip = [&](double s0, double s1, double widthM, int32 MatID)
		{
			if (widthM <= 0.0) return;
			s0 = FMath::Clamp(s0, SectionStartS, SectionEndS);
			s1 = FMath::Clamp(s1, SectionStartS, SectionEndS);
			if (s1 - s0 < 1e-3) return;

			TArray<double> SS;
			for (double s = s0; s < s1 - KINDA_SMALL_NUMBER; s += StepM)
			{
				SS.Add(s);
			}
			SS.Add(s1);

			const int32 NN = SS.Num();
			if (NN < 2) return;

			FGeometryScriptSimpleMeshBuffers B;
			B.Vertices.Reserve(NN * 2);
			B.UV0.Reserve(NN * 2);

			const double Half = widthM * 0.5;

			for (int32 i = 0; i < NN; ++i)
			{
				const double s = SS[i];
				const double tOuter = Sign * Sec->GetOuterOffset(s, LaneId);
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

			B.Normals.SetNum(NN * 2);
			for (int32 i = 0; i < NN; ++i)
			{
				int32 a = i, b = i + 1;
				if (b >= NN) { a = i - 1; b = i; }
				const FVector AlongS  = B.Vertices[2 * b] - B.Vertices[2 * a];
				const FVector AcrossT = B.Vertices[2 * i + 1] - B.Vertices[2 * i];
				FVector Nrm = FVector::CrossProduct(AlongS, AcrossT);
				if (!Nrm.Normalize()) Nrm = FVector::UpVector;
				if (Nrm.Z < 0.f) Nrm = -Nrm;
				B.Normals[2 * i]     = Nrm;
				B.Normals[2 * i + 1] = Nrm;
			}

			for (int32 i = 0; i + 1 < NN; ++i)
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

			const double MarkStart = SectionStartS + RM->GetSOffset();
			double MarkEnd = SectionEndS;
			if (mi + 1 < NumMarks)
			{
				roadmanager::LaneRoadMark* Next = Lane->GetLaneRoadMarkByIdx(mi + 1);
				if (Next) MarkEnd = SectionStartS + Next->GetSOffset();
			}
			if (MarkEnd <= MarkStart) continue;

			const int32 MatID = MarkingSlot;
			const double Width = (RM->GetWidth() > 0.0) ? RM->GetWidth() : 0.12;  // default 12cm

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

	bool BuildJunctionFillRing(
		roadmanager::Junction* Junction,
		double ZOffsetCm,
		TArray<FVector>& OutLoop)
	{
		OutLoop.Reset();
		if (!Junction) return false;
		const int jid = Junction->GetId();

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

		struct FArm { roadmanager::Road* Road; FVector Mid; double Ang; };
		TArray<FArm> Arms;
		TArray<FVector> GatePts;
		for (roadmanager::Road* A : Incoming)
		{
			double s = 0.0; roadmanager::LaneSection* Sec = nullptr;
			if (!GetJunctionFacingSection(A, jid, s, Sec)) continue;
			bool ok; double Lo, Hi; DrivingTRange(Sec, s, ok, Lo, Hi);
			if (!ok) continue;
			const FVector GL = EvalLanePoint(A, s, Hi, ZOffsetCm);
			const FVector GR = EvalLanePoint(A, s, Lo, ZOffsetCm);
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
				const FVector PL = EvalLanePoint(CR, s, Hi, ZOffsetCm);
				const FVector PR = EvalLanePoint(CR, s, Lo, ZOffsetCm);
				if (!PL.ContainsNaN()) Le.Add(PL);
				if (!PR.ContainsNaN()) Re.Add(PR);
			}
			if (Le.Num() < 2 && Re.Num() < 2) return;
			auto MeanR = [&](const TArray<FVector>& P) { double a = 0.0; for (const FVector& q : P) a += FVector::Dist2D(q, Centroid); return P.Num() ? a / P.Num() : 0.0; };
			auto MinR  = [&](const TArray<FVector>& P) { double m = 1e30; for (const FVector& q : P) m = FMath::Min(m, (double)FVector::Dist2D(q, Centroid)); return m; };
			OutEdge = (MeanR(Le) >= MeanR(Re)) ? Le : Re;
			OutMinR = MinR(OutEdge);
		};

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
			if (FVector::Dist2D(Best[0], Arms[i].Mid) > FVector::Dist2D(Best.Last(), Arms[i].Mid))
				for (int x = 0, y = Best.Num() - 1; x < y; ++x, --y) Swap(Best[x], Best[y]);
			Loop.Append(Best);
		}
		if (Loop.Num() < 3) return false;

		OutLoop = MoveTemp(Loop);
		return true;
	}

	bool BuildJunctionFillBuffers(
		roadmanager::Junction* Junction,
		double ZOffsetCm,
		FGeometryScriptSimpleMeshBuffers& OutBuffers)
	{
		TArray<FVector> Loop;
		if (!BuildJunctionFillRing(Junction, ZOffsetCm, Loop)) return false;

		TArray<FIntVector> Tris;
		EarClipPolygon(Loop, Tris);
		if (Tris.Num() < 1) return false;

		FVector LC = FVector::ZeroVector;
		for (const FVector& P : Loop) LC += P;
		LC /= (double)Loop.Num();
		double RMax = 1.0;
		for (const FVector& P : Loop) RMax = FMath::Max(RMax, (double)FVector::Dist2D(P, LC));

		const int32 Mat = (int32)ERoadMatSlot::Asphalt;
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

		return true;
	}

	// ---- Elevated deck structure ------------------------------------------------------

	// Append one quad (P0->P1->P2->P3) to Buf with all four normals = Nrm and the winding chosen
	// so the UE front face points toward Nrm. UE's front face is the negation of the textbook
	// cross product, so we keep the natural winding when cross . Nrm <= 0 and flip otherwise.
	static void AppendQuad(
		FGeometryScriptSimpleMeshBuffers& Buf,
		const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3,
		const FVector& Nrm)
	{
		const int32 b = Buf.Vertices.Num();
		Buf.Vertices.Add(P0); Buf.Vertices.Add(P1); Buf.Vertices.Add(P2); Buf.Vertices.Add(P3);
		for (int32 k = 0; k < 4; ++k) { Buf.Normals.Add(Nrm); }
		Buf.UV0.Add(FVector2D(0, 0)); Buf.UV0.Add(FVector2D(1, 0));
		Buf.UV0.Add(FVector2D(1, 1)); Buf.UV0.Add(FVector2D(0, 1));

		const FVector TextbookN = FVector::CrossProduct(P1 - P0, P2 - P0);
		if (FVector::DotProduct(TextbookN, Nrm) <= 0.0)
		{
			Buf.Triangles.Add(FIntVector(b, b + 1, b + 2));
			Buf.Triangles.Add(FIntVector(b, b + 2, b + 3));
		}
		else
		{
			Buf.Triangles.Add(FIntVector(b, b + 2, b + 1));
			Buf.Triangles.Add(FIntVector(b, b + 3, b + 2));
		}
		Buf.TriGroupIDs.Add(0);
		Buf.TriGroupIDs.Add(0);
	}

	bool BuildRoadEdgePolylines(
		roadmanager::Road* Road,
		const TArray<double>& SList,
		double ZOffsetCm,
		TArray<FVector>& OutLeft,
		TArray<FVector>& OutRight)
	{
		OutLeft.Reset(); OutRight.Reset();
		if (!Road) return false;

		for (double s : SList)
		{
			roadmanager::LaneSection* Sec = Road->GetLaneSectionByS(s);
			if (!Sec) continue;

			bool bOK = false; double Lo = 0.0, Hi = 0.0;
			const int nLanes = Sec->GetNumberOfLanes();
			for (int li = 0; li < nLanes; ++li)
			{
				roadmanager::Lane* L = Sec->GetLaneByIdx(li);
				if (!L) continue;
				const int lid = L->GetId();
				if (lid == 0) continue;
				if (L->GetLaneType() == roadmanager::Lane::LANE_TYPE_NONE) continue;

				const int sgn = (lid > 0) ? 1 : -1;
				const double outer = sgn * Sec->GetOuterOffset(s, lid);
				const double inner = sgn * Sec->GetOuterOffset(s, lid - sgn);
				const double a = FMath::Min(outer, inner), b = FMath::Max(outer, inner);
				if (!bOK) { Lo = a; Hi = b; bOK = true; }
				else { Lo = FMath::Min(Lo, a); Hi = FMath::Max(Hi, b); }
			}
			if (!bOK) continue;

			const FVector PL = EvalLanePoint(Road, s, Hi, ZOffsetCm);
			const FVector PR = EvalLanePoint(Road, s, Lo, ZOffsetCm);
			if (PL.ContainsNaN() || PR.ContainsNaN()) continue;
			OutLeft.Add(PL); OutRight.Add(PR);
		}
		return OutLeft.Num() >= 2 && OutRight.Num() >= 2;
	}

	void BuildDeckBuffers(
		const TArray<FVector>& Left,
		const TArray<FVector>& Right,
		double ThicknessCm,
		FGeometryScriptSimpleMeshBuffers& OutBuffers)
	{
		const int32 N = FMath::Min(Left.Num(), Right.Num());
		if (N < 2) return;
		const FVector Down(0, 0, ThicknessCm);

		// Underside strip (faces down).
		for (int32 i = 0; i + 1 < N; ++i)
		{
			AppendQuad(OutBuffers, Left[i] - Down, Right[i] - Down, Right[i + 1] - Down, Left[i + 1] - Down,
				FVector(0, 0, -1));
		}
		// Left + right fascia (outward-facing).
		for (int32 i = 0; i + 1 < N; ++i)
		{
			const FVector AlongL = (Left[i + 1] - Left[i]);
			FVector NL = FVector::CrossProduct(AlongL, FVector::UpVector).GetSafeNormal();
			if (FVector::DotProduct(NL, Right[i] - Left[i]) > 0.0) NL = -NL; // away from road
			AppendQuad(OutBuffers, Left[i], Left[i + 1], Left[i + 1] - Down, Left[i] - Down, NL);

			const FVector AlongR = (Right[i + 1] - Right[i]);
			FVector NR = FVector::CrossProduct(AlongR, FVector::UpVector).GetSafeNormal();
			if (FVector::DotProduct(NR, Left[i] - Right[i]) > 0.0) NR = -NR;
			AppendQuad(OutBuffers, Right[i], Right[i + 1], Right[i + 1] - Down, Right[i] - Down, NR);
		}
		// End caps.
		{
			const FVector Tan = (((Left[1] + Right[1]) - (Left[0] + Right[0])) * 0.5).GetSafeNormal();
			AppendQuad(OutBuffers, Left[0], Right[0], Right[0] - Down, Left[0] - Down, -Tan);
		}
		{
			const FVector Tan = (((Left[N - 1] + Right[N - 1]) - (Left[N - 2] + Right[N - 2])) * 0.5).GetSafeNormal();
			AppendQuad(OutBuffers, Left[N - 1], Right[N - 1], Right[N - 1] - Down, Left[N - 1] - Down, Tan);
		}
	}

	void BuildParapetBuffers(
		const TArray<FVector>& Left,
		const TArray<FVector>& Right,
		double HeightCm,
		double ThicknessCm,
		const TArray<bool>& LeftFree,
		const TArray<bool>& RightFree,
		FGeometryScriptSimpleMeshBuffers& OutBuffers)
	{
		const int32 N = FMath::Min(Left.Num(), Right.Num());
		if (N < 2 || HeightCm <= 0.0) return;
		const FVector Up(0, 0, HeightCm);

		auto Wall = [&](const TArray<FVector>& Edge, const TArray<FVector>& Opp, const TArray<bool>& Free)
		{
			for (int32 i = 0; i + 1 < N; ++i)
			{
				// Skip interior segments (shared with an adjacent deck) -> no barrier there.
				if (Free.IsValidIndex(i) && Free.IsValidIndex(i + 1) && !(Free[i] && Free[i + 1])) continue;
				FVector InwA = (Opp[i] - Edge[i]); InwA.Z = 0; InwA = InwA.GetSafeNormal();
				FVector InwB = (Opp[i + 1] - Edge[i + 1]); InwB.Z = 0; InwB = InwB.GetSafeNormal();
				const FVector Inw = ((InwA + InwB) * 0.5).GetSafeNormal();

				const FVector oA = Edge[i],     oB = Edge[i + 1];               // outer bottom
				const FVector iA = oA + InwA * ThicknessCm, iB = oB + InwB * ThicknessCm; // inner bottom
				const FVector oAt = oA + Up, oBt = oB + Up, iAt = iA + Up, iBt = iB + Up;

				AppendQuad(OutBuffers, oA, oB, oBt, oAt, -Inw);            // outer face
				AppendQuad(OutBuffers, iA, iB, iBt, iAt, Inw);            // inner face
				AppendQuad(OutBuffers, oAt, oBt, iBt, iAt, FVector(0, 0, 1)); // top
			}
		};
		Wall(Left, Right, LeftFree);
		Wall(Right, Left, RightFree);
	}

	void BuildPierBuffers(
		double X, double Y, double TopZ, double BottomZ, double HalfWidthCm,
		FGeometryScriptSimpleMeshBuffers& OutBuffers)
	{
		if (TopZ <= BottomZ || HalfWidthCm <= 0.0) return;
		const double H = HalfWidthCm;
		const FVector t0(X - H, Y - H, TopZ),    t1(X + H, Y - H, TopZ),    t2(X + H, Y + H, TopZ),    t3(X - H, Y + H, TopZ);
		const FVector b0(X - H, Y - H, BottomZ), b1(X + H, Y - H, BottomZ), b2(X + H, Y + H, BottomZ), b3(X - H, Y + H, BottomZ);
		AppendQuad(OutBuffers, t0, t1, t2, t3, FVector(0, 0, 1));   // top
		AppendQuad(OutBuffers, b0, b1, b2, b3, FVector(0, 0, -1));  // bottom
		AppendQuad(OutBuffers, b0, b1, t1, t0, FVector(0, -1, 0));  // -Y
		AppendQuad(OutBuffers, b1, b2, t2, t1, FVector(1, 0, 0));   // +X
		AppendQuad(OutBuffers, b2, b3, t3, t2, FVector(0, 1, 0));   // +Y
		AppendQuad(OutBuffers, b3, b0, t0, t3, FVector(-1, 0, 0));  // -X
	}
}
