#include "SignalTypeMapping.h"

TSubclassOf<AActor> USignalTypeMapping::FindActorClassForSignal(
	const FString& InType,
	const FString& InSubType,
	const FString& InCountry) const
{
	// Sort by priority (create a copy for sorting)
	TArray<FSignalTypeMappingEntry> SortedMappings = Mappings;
	SortedMappings.Sort([](const FSignalTypeMappingEntry& A, const FSignalTypeMappingEntry& B) {
		return A.Priority > B.Priority;
	});

	for (const FSignalTypeMappingEntry& Entry : SortedMappings)
	{
		bool bTypeMatch = Entry.Type.IsEmpty() || Entry.Type == InType;
		bool bSubTypeMatch = Entry.SubType.IsEmpty() || Entry.SubType == InSubType;
		bool bCountryMatch = Entry.Country.IsEmpty() || Entry.Country == InCountry;

		if (bTypeMatch && bSubTypeMatch && bCountryMatch && Entry.ActorClass)
		{
			return Entry.ActorClass;
		}
	}

	return DefaultActorClass;
}
