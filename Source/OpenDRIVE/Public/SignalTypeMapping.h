#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SignalTypeMapping.generated.h"

/**
 * Defines a mapping entry for signal type/subtype to actor class
 */
USTRUCT(BlueprintType)
struct FSignalTypeMappingEntry
{
	GENERATED_BODY()

	/** Signal type to match (e.g., "1000001"). Empty means match any. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	FString Type;

	/** Signal subtype to match. Empty means match any. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	FString SubType;

	/** Country code to match (e.g., "DEU"). Empty means match any. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	FString Country;

	/** Actor class to spawn for this signal type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	TSubclassOf<AActor> ActorClass;

	/** Priority for matching (higher = checked first) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	int32 Priority = 0;
};

/**
 * Data asset containing signal type to actor class mappings
 */
UCLASS(BlueprintType)
class OPENDRIVE_API USignalTypeMapping : public UDataAsset
{
	GENERATED_BODY()

public:
	/** List of mapping entries, sorted by priority during lookup */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mappings")
	TArray<FSignalTypeMappingEntry> Mappings;

	/** Default actor class when no mapping matches */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mappings")
	TSubclassOf<AActor> DefaultActorClass;

	/**
	 * Find the best matching actor class for a given signal
	 */
	UFUNCTION(BlueprintCallable, Category = "Signal Mapping")
	TSubclassOf<AActor> FindActorClassForSignal(
		const FString& InType,
		const FString& InSubType,
		const FString& InCountry) const;
};
