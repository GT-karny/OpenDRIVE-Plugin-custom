// Fill out your copyright notice in the Description page of Project Settings.

#include "TrafficLightSubsystem.h"

bool UTrafficLightSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return true;
}

void UTrafficLightSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UTrafficLightSubsystem::Deinitialize()
{
	StateCache.Empty();
	OnTrafficLightStateUpdated.Clear();
	Super::Deinitialize();
}

bool UTrafficLightSubsystem::GetTrafficLightState(int32 TrafficLightId, FOsiTrafficLightState& OutState) const
{
	const FOsiTrafficLightState* Found = StateCache.Find(TrafficLightId);
	if (!Found)
	{
		return false;
	}

	OutState = *Found;
	return true;
}

void UTrafficLightSubsystem::UpdateTrafficLightById_Implementation(int32 TrafficLightId, const FOsiTrafficLightState& NewState)
{
	StateCache.Add(TrafficLightId, NewState);
	OnTrafficLightStateUpdated.Broadcast(TrafficLightId, NewState);
}

void UTrafficLightSubsystem::UpdateTrafficLightsBatch_Implementation(const TArray<FOsiTrafficLightBatchEntry>& Updates)
{
	for (const FOsiTrafficLightBatchEntry& Update : Updates)
	{
		StateCache.Add(Update.TrafficLightId, Update.State);
		OnTrafficLightStateUpdated.Broadcast(Update.TrafficLightId, Update.State);
	}
}
