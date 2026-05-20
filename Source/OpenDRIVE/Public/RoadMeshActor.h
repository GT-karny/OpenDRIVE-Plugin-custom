// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoadMeshActor.generated.h"

class UDynamicMeshComponent;
class UMaterialInterface;

/**
 * Actor that holds a UDynamicMeshComponent for generated OpenDRIVE road meshes.
 * Material slots are pre-allocated per lane-type (see ERoadMeshMaterialSlot).
 */
UCLASS(Blueprintable, BlueprintType)
class OPENDRIVE_API ARoadMeshActor : public AActor
{
	GENERATED_BODY()

public:
	ARoadMeshActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OpenDRIVE|RoadMesh")
	UDynamicMeshComponent* MeshComp;

	/** Default materials (one per ERoadMeshMaterialSlot) applied on Generate if no override. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenDRIVE|RoadMesh")
	TArray<UMaterialInterface*> DefaultMaterials;

	/** Apply DefaultMaterials to MeshComp. Safe to call repeatedly. */
	UFUNCTION(BlueprintCallable, Category = "OpenDRIVE|RoadMesh")
	void ApplyDefaultMaterials();

	/** Configure complex-as-simple collision and rebuild it. */
	UFUNCTION(BlueprintCallable, Category = "OpenDRIVE|RoadMesh")
	void EnableCollision();
};
