// Fill out your copyright notice in the Description page of Project Settings.

#include "OsiTrafficLightActor.h"
#include "TrafficLightSubsystem.h"

AOsiTrafficLightActor::AOsiTrafficLightActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Scene = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Scene);

	SignalInfo = CreateDefaultSubobject<USignalInfoComponent>(TEXT("SignalInfo"));
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
	if (!SignalInfo || TrafficLightId != SignalInfo->SignalId)
	{
		return;
	}

	IBPI_TrafficLightUpdate::Execute_OnTrafficLightUpdate(this, NewState);
}

void AOsiTrafficLightActor::OnTrafficLightUpdate_Implementation(const FOsiTrafficLightState& NewState)
{
	// Default: no-op. Override in Blueprint or C++ subclass.
}
