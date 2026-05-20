// Fill out your copyright notice in the Description page of Project Settings.


#include "OpenDrive2Landscape.h"
#include "LevelEditor.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"
#include "LandscapeSplinesComponent.h"
#include "LandscapeSplineSegment.h"
#include "LandscapeSplineControlPoint.h"
#include "LandscapeSplineActor.h"
#include "ILandscapeSplineInterface.h"
#include "Landscape.h"
#include "RoadManager.hpp"
#include "CoordTranslate.h"


UOpenDrive2Landscape::UOpenDrive2Landscape() {
	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
}

void UOpenDrive2Landscape::SculptLandscape(
	float RoadZOffset,
	float Falloff,
	ULandscapeLayerInfoObject *PaintLayer,
	FName LayerName
) {
	// Landscapes to sculpt
	USelection *Selection = GEditor->GetSelectedActors();
	TArray<ALandscapeProxy *> Landscapes;
	for (FSelectionIterator Iter(*Selection); Iter; ++Iter) {
		ALandscapeProxy *Landscape = Cast<ALandscapeProxy>(*Iter);
		if (!Landscape) continue;
		Landscapes.Add(Landscape);
	}

	roadmanager::OpenDrive *Odr = roadmanager::Position::GetOpenDrive();
	roadmanager::Road *road = 0;
	roadmanager::Position p;
	size_t nrOfRoads = Odr->GetNumOfRoads();

	// Building splines
	for (int i = 0; i < (int)nrOfRoads; i++) {
		Spline->ClearSplinePoints();
		bool shouldStop = false;
		road = Odr->GetRoadByIdx(i);
		if (!road) continue;
		double roadLen = road->GetLength();
		FVector sp;
		double WidthStart = road->GetWidth(0., 0) * 50;
		double WidthEnd = road->GetWidth(roadLen, 0) * 50;
		double s = 0.;
		while (!shouldStop) {
			// Adding spline point every 5m
			if (s > roadLen) {
				s = roadLen;
				shouldStop = true;
			}
			// Since UE4's splines want the same width on both sides, I have to place to point to the
			// geometric center of the road, which might totally differ from lane 0 (think one-way road)
			double t = (road->GetWidth(s, 1) - road->GetWidth(s, -1)) / 2.;
			// I can't just do p = roadmanager::Position(...), because Position's default snapping is
			// driving lane, which breaks things. So I reinit the point and set it to snap to any.
			p.Init();
			p.SetSnapLaneTypes(roadmanager::Lane::LaneType::LANE_TYPE_ANY);
			p.SetLanePos(road->GetId(), 0, s, t);
			p.SetHeadingRelativeRoadDirection(0.);
			sp = CoordTranslate::OdrToUe::ToLocation(p);
			// Slight Z down offset to avoid Z-fighting
			sp.Z += RoadZOffset;
			Spline->AddSplinePoint(sp, ESplineCoordinateSpace::World);

			s += 5;
		}

		// Set all points to linear (default causes issues)
		for (int j = 0; j < Spline->GetNumberOfSplinePoints(); j++) {
			Spline->SetSplinePointType(j, ESplinePointType::Type::Linear);
		}

		for (auto &ls : Landscapes) {
			// The following doesn't work (EditorApplySpline isn't accessible via C++)
			// See https://answers.unrealengine.com/questions/228146/editorapplyspline-is-not-accessible-via-c.html
			//ls->EditorApplySpline(Spline);
			ApplySpline(ls, Spline, WidthStart, WidthEnd, Falloff, PaintLayer, LayerName);
		}
	}
}

namespace
{
	// Curvature-adaptive s-list across the whole road, unioning OSI sample s-values from every
	// lane in every section and forcing section boundaries to land on a sample. MaxStepM caps
	// the longest gap; MinStepM coalesces samples that are essentially duplicates.
	// Same recipe as FRoadMeshGenerator::BuildSList but spanning all sections of one road.
	TArray<double> BuildRoadSList(roadmanager::Road* Road, double MaxStepM, double MinStepM)
	{
		TArray<double> SList;
		if (!Road) return SList;

		const double TotalLen = Road->GetLength();
		TSet<double> SSet;
		SSet.Add(0.0);
		SSet.Add(TotalLen);

		const int32 NumSections = Road->GetNumberOfLaneSections();
		for (int32 si = 0; si < NumSections; ++si)
		{
			roadmanager::LaneSection* Sec = Road->GetLaneSectionByIdx(si);
			if (!Sec) continue;
			const double SecStart = Sec->GetS();
			const double SecEnd = SecStart + Sec->GetLength();
			// Section boundaries are width-change discontinuities — must be sampled.
			SSet.Add(FMath::Clamp(SecStart, 0.0, TotalLen));
			SSet.Add(FMath::Clamp(SecEnd,   0.0, TotalLen));

			for (int32 li = 0; li < Sec->GetNumberOfLanes(); ++li)
			{
				roadmanager::Lane* L = Sec->GetLaneByIdx(li);
				if (!L) continue;
				auto* OSI = L->GetOSIPoints();
				if (!OSI) continue;
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

		// Enforce MaxStep by inserting intermediate samples in gaps that exceed it.
		TArray<double> Capped;
		Capped.Reserve(Sorted.Num() * 2);
		const double MaxStep = FMath::Max(MaxStepM, 0.1);
		for (int32 i = 0; i < Sorted.Num(); ++i)
		{
			if (i > 0)
			{
				const double Prev = Sorted[i - 1];
				const double Cur = Sorted[i];
				const double Gap = Cur - Prev;
				if (Gap > MaxStep)
				{
					const int32 N = (int32)FMath::FloorToDouble(Gap / MaxStep);
					const double Step = Gap / (N + 1);
					for (int32 k = 1; k <= N; ++k) Capped.Add(Prev + k * Step);
				}
			}
			Capped.Add(Sorted[i]);
		}

		// Coalesce samples closer than MinStep — but always keep the very last sample.
		TArray<double> Final;
		Final.Reserve(Capped.Num());
		const double MinStep = FMath::Max(MinStepM, 0.01);
		for (int32 i = 0; i < Capped.Num(); ++i)
		{
			const bool bLast = (i == Capped.Num() - 1);
			if (Final.Num() == 0 || (Capped[i] - Final.Last()) > MinStep)
			{
				Final.Add(Capped[i]);
			}
			else if (bLast)
			{
				// Snap the trailing sample to the true road end so we don't lose the endpoint.
				Final.Last() = Capped[i];
			}
		}
		return Final;
	}

	// Find a previously-created CP on the predecessor (or successor) road we can reuse so
	// the new road's first/last segment shares an endpoint with its neighbour. Returns nullptr
	// when the neighbour is a junction or hasn't been processed yet.
	ULandscapeSplineControlPoint* FindNeighbourCP(
		roadmanager::Road* Road,
		roadmanager::LinkType Side,
		const TMap<int, ULandscapeSplineControlPoint*>& FirstCP,
		const TMap<int, ULandscapeSplineControlPoint*>& LastCP)
	{
		if (!Road) return nullptr;
		roadmanager::RoadLink* Link = Road->GetLink(Side);
		if (!Link) return nullptr;
		if (Link->GetElementType() != roadmanager::RoadLink::ELEMENT_TYPE_ROAD) return nullptr;
		const int NeighborId = Link->GetElementId();
		// We connect to the neighbour's endpoint that meets us.
		switch (Link->GetContactPointType())
		{
		case roadmanager::CONTACT_POINT_END:   return LastCP.FindRef(NeighborId);
		case roadmanager::CONTACT_POINT_START: return FirstCP.FindRef(NeighborId);
		default: return nullptr;
		}
	}
}

void UOpenDrive2Landscape::CreateRoadSplines(
	float RoadZOffset,
	float Falloff,
	ULandscapeLayerInfoObject* PaintLayer,
	FName LayerName
) {
	USelection* Selection = GEditor->GetSelectedActors();
	for (FSelectionIterator Iter(*Selection); Iter; ++Iter) {
		ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(*Iter);
		if (!Landscape) continue;
		ILandscapeSplineInterface* SplineOwner = Landscape;
		ULandscapeSplinesComponent* LSC = SplineOwner->GetSplinesComponent();
		if (!LSC) {
			SplineOwner->CreateSplineComponent();
			LSC = SplineOwner->GetSplinesComponent();
		}
		if (!LSC->IsRegistered()) {
			LSC->RegisterComponent();
		} else {
			LSC->MarkRenderStateDirty();
		}

		const FTransform LSCWorld = LSC->GetComponentTransform();

		roadmanager::OpenDrive* Odr = roadmanager::Position::GetOpenDrive();
		if (!Odr) continue;
		const size_t nrOfRoads = Odr->GetNumOfRoads();
		roadmanager::Position p;

		// Map road_id -> first/last CP we've already created, so neighbouring roads can attach
		// to the same endpoint instead of producing a duplicated, slightly-offset CP at the join.
		TMap<int, ULandscapeSplineControlPoint*> RoadFirstCP;
		TMap<int, ULandscapeSplineControlPoint*> RoadLastCP;

		// Tunables. ~8m max gap matches the road preview step; 0.5m floor prevents redundant
		// CPs piling up where OSI is dense (very tight curves) without losing curvature detail.
		const double MaxStepM = 8.0;
		const double MinStepM = 0.5;

		for (int ri = 0; ri < (int)nrOfRoads; ++ri) {
			roadmanager::Road* road = Odr->GetRoadByIdx(ri);
			if (!road) continue;
			const int rid = road->GetId();

			const TArray<double> SList = BuildRoadSList(road, MaxStepM, MinStepM);
			if (SList.Num() < 2) continue;

			// Try to reuse the predecessor's end CP so the new road's first segment is anchored
			// to the same point. Junctions are skipped (they need per-incoming-road dispatch).
			ULandscapeSplineControlPoint* PrevCP = FindNeighbourCP(road, roadmanager::PREDECESSOR, RoadFirstCP, RoadLastCP);

			for (int32 i = 0; i < SList.Num(); ++i) {
				const double s = SList[i];

				// Geometric center: same trick as before — when sides have different widths the
				// reference line isn't the visual centre, so offset t by half the asymmetry.
				const double t = (road->GetWidth(s, 1) - road->GetWidth(s, -1)) / 2.0;

				p.Init();
				p.SetSnapLaneTypes(roadmanager::Lane::LaneType::LANE_TYPE_ANY);
				p.SetLanePos(rid, 0, s, t);
				p.SetHeadingRelativeRoadDirection(0.0);

				FVector sp = CoordTranslate::OdrToUe::ToLocation(p);
				sp.Z += RoadZOffset;
				// Yaw only: OpenDRIVE pitch/roll on a Landscape Spline CP feeds straight into the
				// Hermite tangent's Z component, which gets multiplied by TangentLen and produces
				// large Z overshoots between CPs (the "deep notches" we saw). Linear Z
				// interpolation between consecutive CP positions is what we actually want here.
				const FRotator FullR = CoordTranslate::OdrToUe::ToRotation(p);
				const FRotator YawOnly(0.f, FullR.Yaw, 0.f);

				// Landscape spline Width is half-width in cm; GetWidth returns full width in m.
				const float HalfWidthCm = (float)road->GetWidth(s, 0) * 50.f;

				ULandscapeSplineControlPoint* NewCP = nullptr;
				const bool bFirst = (i == 0);
				const bool bLast  = (i == SList.Num() - 1);

				if (bFirst && PrevCP) {
					// Stitch first sample onto predecessor's last CP — no duplicate point.
					NewCP = PrevCP;
				} else {
					LSC->Modify();
					NewCP = NewObject<ULandscapeSplineControlPoint>(LSC, NAME_None, RF_Transactional);
					LSC->GetControlPoints().Add(NewCP);
					NewCP->Location = LSCWorld.InverseTransformPositionNoScale(sp);
					NewCP->Rotation = FRotator(LSCWorld.InverseTransformRotation(YawOnly.Quaternion()));
					NewCP->Width = HalfWidthCm;
					NewCP->SideFalloff = Falloff;
					// EndFalloff is set in a second pass — see below. Setting it here for every
					// CP makes each one act as a spline endpoint that fades to base terrain,
					// producing rib-shaped notches at every CP.
					NewCP->EndFalloff  = 0.f;
					NewCP->LayerName   = LayerName;
				}

				if (bFirst) RoadFirstCP.Add(rid, NewCP);
				if (bLast)  RoadLastCP.Add(rid, NewCP);

				if (PrevCP && NewCP != PrevCP) {
					LSC->Modify();
					PrevCP->Modify();
					NewCP->Modify();
					ULandscapeSplineSegment* Seg = NewObject<ULandscapeSplineSegment>(LSC, NAME_None, RF_Transactional);
					LSC->GetSegments().Add(Seg);

					Seg->Connections[0].ControlPoint = PrevCP;
					Seg->Connections[1].ControlPoint = NewCP;
					// Each CP picks the socket on ITS OWN side that faces the OTHER endpoint.
					// (Original code queried sockets on the opposite CP, swapping the two.)
					Seg->Connections[0].SocketName = PrevCP->GetBestConnectionTo(NewCP->Location);
					Seg->Connections[1].SocketName = NewCP->GetBestConnectionTo(PrevCP->Location);

					FVector S0Loc; FRotator S0Rot;
					PrevCP->GetConnectionLocationAndRotation(Seg->Connections[0].SocketName, S0Loc, S0Rot);
					FVector S1Loc; FRotator S1Rot;
					NewCP->GetConnectionLocationAndRotation(Seg->Connections[1].SocketName, S1Loc, S1Rot);

					// Tangent length: 1/3 of the segment length is the standard Hermite rule —
					// using full length (the prior code) caused C1 overshoot and the visible bumps.
					const float TangentLen = (S1Loc - S0Loc).Size() / 3.f;
					Seg->Connections[0].TangentLen = TangentLen;
					Seg->Connections[1].TangentLen = TangentLen;
					Seg->AutoFlipTangents();

					// Bake direction: deform terrain both up and down so it actually conforms
					// to the spline Z (otherwise the spline only carves one way).
					Seg->bRaiseTerrain = true;
					Seg->bLowerTerrain = true;
					Seg->LayerName = LayerName;

					PrevCP->ConnectedSegments.Add(FLandscapeSplineConnection(Seg, 0));
					NewCP->ConnectedSegments.Add(FLandscapeSplineConnection(Seg, 1));
					Seg->UpdateSplinePoints();
				}

				PrevCP = NewCP;
			}
		}

		// Pass 2: stitch joints that the per-road loop couldn't.
		//
		// OpenDRIVE allows roads to link s=0<->s=0 or s=end<->s=end (head-to-head /
		// tail-to-tail), and the reciprocal RoadLink isn't required to live on the
		// "predecessor" side — Road A may link to Road B via its successor while B links
		// back via its own successor. The per-road loop only consults PREDECESSOR, so any
		// SUCCESSOR-side join produces two separate CPs at the same world point and
		// leaves the visible gap.
		//
		// We fix this by walking every road again, checking BOTH link sides, and merging
		// the matching CP pairs. Merging means: rewire every segment that pointed at the
		// "loser" CP to point at the "winner" and drop the loser from the spline component.
		auto MergeCPs = [&](ULandscapeSplineControlPoint* Winner, ULandscapeSplineControlPoint* Loser)
		{
			if (!Winner || !Loser || Winner == Loser) return;
			LSC->Modify();
			Winner->Modify();
			Loser->Modify();

			TArray<ULandscapeSplineSegment*> ReparentedSegments;
			for (const TObjectPtr<ULandscapeSplineSegment>& SegPtr : LSC->GetSegments())
			{
				ULandscapeSplineSegment* Seg = SegPtr.Get();
				if (!Seg) continue;
				for (int ci = 0; ci < 2; ++ci)
				{
					if (Seg->Connections[ci].ControlPoint == Loser)
					{
						Seg->Modify();
						Seg->Connections[ci].ControlPoint = Winner;
						// Re-pick the socket on Winner that faces the segment's other end.
						const FVector OtherLoc = Seg->Connections[1 - ci].ControlPoint->Location;
						Seg->Connections[ci].SocketName = Winner->GetBestConnectionTo(OtherLoc);
						Winner->ConnectedSegments.Add(FLandscapeSplineConnection(Seg, ci));
						ReparentedSegments.AddUnique(Seg);
					}
				}
			}
			Loser->ConnectedSegments.Empty();
			LSC->GetControlPoints().Remove(Loser);
			// Keep the road->CP maps consistent so subsequent merges see the surviving CP.
			for (auto& Pair : RoadFirstCP) { if (Pair.Value == Loser) Pair.Value = Winner; }
			for (auto& Pair : RoadLastCP)  { if (Pair.Value == Loser) Pair.Value = Winner; }
			Loser->MarkAsGarbage();

			// CRITICAL: the socket on Winner might face the opposite direction from the old
			// socket on Loser, which means the TangentLen sign set at segment-creation time
			// is now pointing the wrong way through Winner. That mismatch is what produced
			// the asymmetric inward wedge at tail-to-tail (s=end <-> s=end) joins, where
			// Winner's heading is anti-parallel to Loser's. Recompute TangentLen from the
			// new socket positions and re-run AutoFlipTangents on every reparented segment.
			for (ULandscapeSplineSegment* Seg : ReparentedSegments)
			{
				if (!Seg) continue;
				FVector S0Loc; FRotator S0Rot;
				Seg->Connections[0].ControlPoint->GetConnectionLocationAndRotation(
					Seg->Connections[0].SocketName, S0Loc, S0Rot);
				FVector S1Loc; FRotator S1Rot;
				Seg->Connections[1].ControlPoint->GetConnectionLocationAndRotation(
					Seg->Connections[1].SocketName, S1Loc, S1Rot);
				const float TangentLen = (S1Loc - S0Loc).Size() / 3.f;
				Seg->Connections[0].TangentLen = TangentLen;
				Seg->Connections[1].TangentLen = TangentLen;
				Seg->AutoFlipTangents();
			}
		};

		for (int ri = 0; ri < (int)nrOfRoads; ++ri)
		{
			roadmanager::Road* road = Odr->GetRoadByIdx(ri);
			if (!road) continue;
			const int rid = road->GetId();

			// PREDECESSOR side -> our s=0 endpoint should equal the neighbour's matching CP.
			if (ULandscapeSplineControlPoint* MyFirst = RoadFirstCP.FindRef(rid))
			{
				if (ULandscapeSplineControlPoint* Neighbour =
						FindNeighbourCP(road, roadmanager::PREDECESSOR, RoadFirstCP, RoadLastCP))
				{
					MergeCPs(Neighbour, MyFirst);
				}
			}
			// SUCCESSOR side -> our s=end endpoint. This is the case the original loop missed.
			if (ULandscapeSplineControlPoint* MyLast = RoadLastCP.FindRef(rid))
			{
				if (ULandscapeSplineControlPoint* Neighbour =
						FindNeighbourCP(road, roadmanager::SUCCESSOR, RoadFirstCP, RoadLastCP))
				{
					MergeCPs(Neighbour, MyLast);
				}
			}
		}

		// Third pass: only true spline termini (CPs touching <= 1 segment) get an EndFalloff.
		// Mid-CPs must keep EndFalloff=0 so adjacent segments blend seamlessly into one chain.
		LSC->Modify();
		for (const TObjectPtr<ULandscapeSplineControlPoint>& CPPtr : LSC->GetControlPoints()) {
			ULandscapeSplineControlPoint* CP = CPPtr.Get();
			if (!CP) continue;
			CP->Modify();
			CP->EndFalloff = (CP->ConnectedSegments.Num() <= 1) ? Falloff : 0.f;
		}
		// Recompute every segment's raster — EndFalloff feeds into the bake and segments
		// connected to mid-CPs were originally built with the (wrong) per-CP falloff.
		for (const TObjectPtr<ULandscapeSplineSegment>& SegPtr : LSC->GetSegments()) {
			if (ULandscapeSplineSegment* Seg = SegPtr.Get()) {
				Seg->UpdateSplinePoints();
			}
		}

		// Bake the splines into a DEDICATED edit layer so other layers don't add to the
		// road height. Without this step, "Apply Splines to Heightmap" would write into the
		// active edit layer and the final Z would be sum(all layers) — so the road never
		// quite sits at the OpenDRIVE elevation when other height layers exist.
		ALandscape* MasterLandscape = Landscape->GetLandscapeActor();
		if (MasterLandscape && MasterLandscape->HasLayersContent()) {
			static const FName RoadEditLayerName(TEXT("OpenDRIVE Roads"));

			int32 LayerIndex = MasterLandscape->GetLayerIndex(RoadEditLayerName);
			if (LayerIndex == INDEX_NONE) {
				LayerIndex = MasterLandscape->CreateLayer(RoadEditLayerName);
			}

			if (LayerIndex != INDEX_NONE) {
				if (FLandscapeLayer* RoadLayer = MasterLandscape->GetLayer(LayerIndex)) {
					MasterLandscape->Modify();
					// AlphaBlend: the layer's height overrides whatever the layers below
					// contribute. With the default AdditiveBlend our road would only add to
					// the existing terrain Z instead of replacing it.
					RoadLayer->BlendMode = LSBM_AlphaBlend;
					// Reserving this layer for splines locks it from manual sculpt and routes
					// future spline updates into it automatically.
					MasterLandscape->SetLandscapeSplinesReservedLayer(LayerIndex);
					// Note: deliberately not clearing the layer here. Re-baking accumulates
					// onto existing layer data, which is intentional — lets the user keep
					// manual touch-ups or accumulate edits across runs.
					MasterLandscape->UpdateLandscapeSplines(RoadLayer->Guid);
				}
			}
		}
		else if (MasterLandscape) {
			// No edit-layer system on this landscape — just trigger the direct bake.
			MasterLandscape->UpdateLandscapeSplines();
		}
	}
}
