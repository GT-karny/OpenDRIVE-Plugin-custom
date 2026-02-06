// Fill out your copyright notice in the Description page of Project Settings.

#include "TrafficLightHandlerBase.h"

ATrafficLightHandlerBase::ATrafficLightHandlerBase()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATrafficLightHandlerBase::BeginPlay()
{
	Super::BeginPlay();
	RebuildLookupMap();
}

void ATrafficLightHandlerBase::RebuildLookupMap()
{
	IdToIndexMap.Empty(ManagedTrafficLights.Num());
	for (int32 i = 0; i < ManagedTrafficLights.Num(); ++i)
	{
		IdToIndexMap.Add(ManagedTrafficLights[i].TrafficLightId, i);
	}
}

bool ATrafficLightHandlerBase::RegisterTrafficLight(int32 TrafficLightId, AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return false;
	}

	if (IdToIndexMap.Contains(TrafficLightId))
	{
		return false;
	}

	FManagedTrafficLight Entry;
	Entry.TrafficLightId = TrafficLightId;
	Entry.TrafficLightActor = Actor;
	const int32 NewIndex = ManagedTrafficLights.Add(Entry);
	IdToIndexMap.Add(TrafficLightId, NewIndex);
	return true;
}

bool ATrafficLightHandlerBase::UnregisterTrafficLight(int32 TrafficLightId)
{
	const int32* IndexPtr = IdToIndexMap.Find(TrafficLightId);
	if (!IndexPtr)
	{
		return false;
	}

	ManagedTrafficLights.RemoveAt(*IndexPtr);
	RebuildLookupMap();
	return true;
}

bool ATrafficLightHandlerBase::GetTrafficLightState(int32 TrafficLightId, FOsiTrafficLightState& OutState) const
{
	const int32* IndexPtr = IdToIndexMap.Find(TrafficLightId);
	if (!IndexPtr || !ManagedTrafficLights.IsValidIndex(*IndexPtr))
	{
		return false;
	}

	OutState = ManagedTrafficLights[*IndexPtr].CurrentState;
	return true;
}

void ATrafficLightHandlerBase::UpdateTrafficLightById_Implementation(int32 TrafficLightId, const FOsiTrafficLightState& NewState)
{
	int32* IndexPtr = IdToIndexMap.Find(TrafficLightId);
	if (!IndexPtr || !ManagedTrafficLights.IsValidIndex(*IndexPtr))
	{
		return;
	}

	PropagateStateToActor(ManagedTrafficLights[*IndexPtr], NewState);
}

void ATrafficLightHandlerBase::UpdateTrafficLightsBatch_Implementation(const TArray<FOsiTrafficLightBatchEntry>& Updates)
{
	for (const FOsiTrafficLightBatchEntry& Update : Updates)
	{
		int32* IndexPtr = IdToIndexMap.Find(Update.TrafficLightId);
		if (IndexPtr && ManagedTrafficLights.IsValidIndex(*IndexPtr))
		{
			PropagateStateToActor(ManagedTrafficLights[*IndexPtr], Update.State);
		}
	}
}

void ATrafficLightHandlerBase::PropagateStateToActor(FManagedTrafficLight& Entry, const FOsiTrafficLightState& NewState)
{
	Entry.CurrentState = NewState;

	AActor* Actor = Entry.TrafficLightActor;
	if (IsValid(Actor) && Actor->GetClass()->ImplementsInterface(UBPI_TrafficLightUpdate::StaticClass()))
	{
		IBPI_TrafficLightUpdate::Execute_OnTrafficLightUpdate(Actor, NewState);
	}
}
