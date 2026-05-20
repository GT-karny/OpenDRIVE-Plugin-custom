// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoadMeshActor.h"
#include "Components/DynamicMeshComponent.h"
#include "Materials/MaterialInterface.h"

ARoadMeshActor::ARoadMeshActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComp = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	MeshComp->SetMobility(EComponentMobility::Static);
	MeshComp->bUseAsyncCooking = true;
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
	MeshComp->EnableComplexAsSimpleCollision();
	MeshComp->UpdateCollision(/*bOnlyIfPending=*/false);
}
