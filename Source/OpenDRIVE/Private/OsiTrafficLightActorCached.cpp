// Fill out your copyright notice in the Description page of Project Settings.

#include "OsiTrafficLightActorCached.h"

void AOsiTrafficLightActorCached::OnTrafficLightUpdate_Implementation(const FOsiTrafficLightState& NewState)
{
	if (bHasReceivedState && CachedState == NewState)
	{
		return;
	}

	bHasReceivedState = true;
	CachedState = NewState;
	OnTrafficLightStateChanged(NewState);
}

void AOsiTrafficLightActorCached::OnTrafficLightStateChanged_Implementation(const FOsiTrafficLightState& NewState)
{
	// Default: no-op. Override in Blueprint or C++ subclass.
}
