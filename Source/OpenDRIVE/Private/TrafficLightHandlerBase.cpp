// Fill out your copyright notice in the Description page of Project Settings.

#include "TrafficLightHandlerBase.h"

ATrafficLightHandlerBase::ATrafficLightHandlerBase()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool ATrafficLightHandlerBase::GetTrafficLightState(int32 TrafficLightId, FOsiTrafficLightState& OutState) const
{
	const FOsiTrafficLightState* Found = StateCache.Find(TrafficLightId);
	if (!Found)
	{
		return false;
	}

	OutState = *Found;
	return true;
}

void ATrafficLightHandlerBase::UpdateTrafficLightById_Implementation(int32 TrafficLightId, const FOsiTrafficLightState& NewState)
{
	StateCache.Add(TrafficLightId, NewState);
	OnTrafficLightStateUpdated.Broadcast(TrafficLightId, NewState);
}

void ATrafficLightHandlerBase::UpdateTrafficLightsBatch_Implementation(const TArray<FOsiTrafficLightBatchEntry>& Updates)
{
	for (const FOsiTrafficLightBatchEntry& Update : Updates)
	{
		StateCache.Add(Update.TrafficLightId, Update.State);
		OnTrafficLightStateUpdated.Broadcast(Update.TrafficLightId, Update.State);
	}
}
