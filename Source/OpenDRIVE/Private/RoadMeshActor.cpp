// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoadMeshActor.h"
#include "Components/DynamicMeshComponent.h"
#include "Materials/MaterialInterface.h"

ARoadMeshActor::ARoadMeshActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComp = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	// Movable, not Static: a procedurally generated DynamicMesh has no baked lightmap
	// (and no lightmap UVs), so as a Static component it renders dark / with grainy
	// preview lighting until lighting is built (which never happens for runtime meshes).
	// Movable forces fully dynamic lighting, so it is lit correctly regardless of the
	// scene's lighting-build state.
	MeshComp->SetMobility(EComponentMobility::Movable);
	MeshComp->bUseAsyncCooking = true;

	// Without tangents, any normal-mapped material (the asphalt M_Road) renders with
	// broken tangent-space lighting — a grainy, blotchy surface. AutoCalculated derives
	// orthonormal tangents from the mesh UVs + normals so normal maps shade correctly.
	MeshComp->SetTangentsType(EDynamicMeshComponentTangentsMode::AutoCalculated);
}

void ARoadMeshActor::ApplyDefaultMaterials()
{
	if (!MeshComp) return;
	if (DefaultMaterials.Num() == 0) return;
	MeshComp->ConfigureMaterialSet(DefaultMaterials);
}

void ARoadMeshActor::EnableCollision()
{
	if (!MeshComp) return;

	// UDynamicMeshComponent defaults to the NoCollision profile, and
	// EnableComplexAsSimpleCollision() only flips the complex-as-simple trace flag — it does
	// NOT enable the BodyInstance, so without this the physics state (and queryable collision)
	// is never created. Enable query+physics with a blocking profile so vehicles / line traces
	// actually collide with the road surface, then build the complex-as-simple body.
	MeshComp->SetCollisionProfileName(TEXT("BlockAll"));
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComp->EnableComplexAsSimpleCollision();
	MeshComp->UpdateCollision(/*bOnlyIfPending=*/false);
}
