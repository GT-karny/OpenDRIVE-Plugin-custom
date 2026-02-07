// Fill out your copyright notice in the Description page of Project Settings.

#include "TrafficLightHandlerBase.h"

ATrafficLightHandlerBase::ATrafficLightHandlerBase()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool ATrafficLightHandlerBase::RegisterTrafficLight_Implementation(int32 TrafficLightId, const TScriptInterface<IBPI_TrafficLightUpdate>& TrafficLightActor)
{
	if (!IsValid(TrafficLightActor.GetObject()))
	{
		return false;
	}

	if (ManagedTrafficLights.Contains(TrafficLightId))
	{
		return false;
	}

	ManagedTrafficLights.Add(TrafficLightId, TrafficLightActor);
	return true;
}

bool ATrafficLightHandlerBase::UnregisterTrafficLight_Implementation(int32 TrafficLightId)
{
	if (ManagedTrafficLights.Remove(TrafficLightId) > 0)
	{
		StateCache.Remove(TrafficLightId);
		return true;
	}
	return false;
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
	const TScriptInterface<IBPI_TrafficLightUpdate>* ActorPtr = ManagedTrafficLights.Find(TrafficLightId);
	if (!ActorPtr)
	{
		return;
	}

	PropagateStateToActor(TrafficLightId, *ActorPtr, NewState);
}

void ATrafficLightHandlerBase::UpdateTrafficLightsBatch_Implementation(const TArray<FOsiTrafficLightBatchEntry>& Updates)
{
	for (const FOsiTrafficLightBatchEntry& Update : Updates)
	{
		const TScriptInterface<IBPI_TrafficLightUpdate>* ActorPtr = ManagedTrafficLights.Find(Update.TrafficLightId);
		if (ActorPtr)
		{
			PropagateStateToActor(Update.TrafficLightId, *ActorPtr, Update.State);
		}
	}
}

void ATrafficLightHandlerBase::PropagateStateToActor(int32 TrafficLightId, const TScriptInterface<IBPI_TrafficLightUpdate>& Actor, const FOsiTrafficLightState& NewState)
{
	StateCache.Add(TrafficLightId, NewState);

	UObject* Obj = Actor.GetObject();
	if (IsValid(Obj))
	{
		IBPI_TrafficLightUpdate::Execute_OnTrafficLightUpdate(Obj, NewState);
	}
}
