// Fill out your copyright notice in the Description page of Project Settings.

#include "OsiTrafficLightActor.h"
#include "TrafficLightSubsystem.h"
#include "SignalInfoComponent.h"

AOsiTrafficLightActor::AOsiTrafficLightActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Scene = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Scene);
}

void AOsiTrafficLightActor::BeginPlay()
{
	Super::BeginPlay();

	UTrafficLightSubsystem* Subsystem = GetWorld()->GetSubsystem<UTrafficLightSubsystem>();
	if (Subsystem)
	{
		Subsystem->OnTrafficLightStateUpdated.AddDynamic(
			this, &AOsiTrafficLightActor::OnSubsystemStateUpdated);
	}
}

void AOsiTrafficLightActor::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UTrafficLightSubsystem* Subsystem = GetWorld()->GetSubsystem<UTrafficLightSubsystem>())
	{
		Subsystem->OnTrafficLightStateUpdated.RemoveDynamic(
			this, &AOsiTrafficLightActor::OnSubsystemStateUpdated);
	}

	Super::EndPlay(Reason);
}

void AOsiTrafficLightActor::OnSubsystemStateUpdated(int32 TrafficLightId, const FOsiTrafficLightState& NewState)
{
	if (TrafficLightId != MyTrafficLightId)
	{
		return;
	}

	IBPI_TrafficLightUpdate::Execute_OnTrafficLightUpdate(this, NewState);
}

void AOsiTrafficLightActor::OnTrafficLightUpdate_Implementation(const FOsiTrafficLightState& NewState)
{
	// Default: no-op. Override in Blueprint or C++ subclass.
}

void AOsiTrafficLightActor::OnSignalAutoPlaced_Implementation(const USignalInfoComponent* SignalInfo)
{
	if (SignalInfo)
	{
		MyTrafficLightId = SignalInfo->SignalId;
	}
}
