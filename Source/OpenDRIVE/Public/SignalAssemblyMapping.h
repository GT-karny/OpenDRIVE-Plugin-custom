#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SignalAssemblyMapping.generated.h"

/**
 * Defines a mapping entry for a signal assembly (group of signal types) to an actor class.
 */
USTRUCT(BlueprintType)
struct FSignalAssemblyMappingEntry
{
	GENERATED_BODY()

	/**
	 * Set of signal types that must ALL be present in the assembly group.
	 * e.g., ["1000001", "1000011"] means the group must contain both types.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	TArray<FString> RequiredTypes;

	/** Actor class to spawn for this assembly combination */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	TSubclassOf<AActor> ActorClass;

	/** Priority for matching (higher = checked first) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	int32 Priority = 0;
};

/**
 * Data asset containing signal assembly mappings.
 * Maps combinations of signal types to actor classes for grouped signal spawning.
 */
UCLASS(BlueprintType)
class OPENDRIVE_API USignalAssemblyMapping : public UDataAsset
{
	GENERATED_BODY()

public:
	/** List of assembly mapping entries */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mappings")
	TArray<FSignalAssemblyMappingEntry> Mappings;

	/** Default actor class when no assembly mapping matches */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mappings")
	TSubclassOf<AActor> DefaultActorClass;

	/**
	 * Find the best matching actor class for a group of signal types.
	 * Returns the highest-priority entry whose RequiredTypes are all present in the given set.
	 * @param SignalTypes The set of signal types in the assembly group
	 * @return The matching actor class, or DefaultActorClass if no match
	 */
	UFUNCTION(BlueprintCallable, Category = "Signal Assembly Mapping")
	TSubclassOf<AActor> FindActorClassForAssembly(const TArray<FString>& SignalTypes) const;
};
