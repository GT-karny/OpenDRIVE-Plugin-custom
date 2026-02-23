#include "SignalAssemblyMapping.h"

TSubclassOf<AActor> USignalAssemblyMapping::FindActorClassForAssembly(
	const TArray<FString>& SignalTypes) const
{
	// Sort by priority (create a copy for sorting)
	TArray<FSignalAssemblyMappingEntry> SortedMappings = Mappings;
	SortedMappings.Sort([](const FSignalAssemblyMappingEntry& A, const FSignalAssemblyMappingEntry& B) {
		return A.Priority > B.Priority;
	});

	// Convert input to a set for fast lookup
	TSet<FString> TypeSet;
	for (const FString& Type : SignalTypes)
	{
		TypeSet.Add(Type);
	}

	for (const FSignalAssemblyMappingEntry& Entry : SortedMappings)
	{
		if (!Entry.ActorClass || Entry.RequiredTypes.Num() == 0)
		{
			continue;
		}

		// Check if all required types are present in the group
		bool bAllPresent = true;
		for (const FString& Required : Entry.RequiredTypes)
		{
			if (!TypeSet.Contains(Required))
			{
				bAllPresent = false;
				break;
			}
		}

		if (bAllPresent)
		{
			return Entry.ActorClass;
		}
	}

	return DefaultActorClass;
}
