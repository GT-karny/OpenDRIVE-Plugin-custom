// Fill out your copyright notice in the Description page of Project Settings.

#include "OsiTrafficLightAssemblyActor.h"
#include "SignalInfoComponent.h"
#include "TrafficLightSubsystem.h"

AOsiTrafficLightAssemblyActor::AOsiTrafficLightAssemblyActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Scene = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Scene);
}

TArray<USignalInfoComponent*> AOsiTrafficLightAssemblyActor::GetSignalInfoComponents() const
{
	TArray<USignalInfoComponent*> Result;
	GetComponents<USignalInfoComponent>(Result);
	return Result;
}

void AOsiTrafficLightAssemblyActor::BeginPlay()
{
	Super::BeginPlay();

	// Build the set of managed signal IDs from all SignalInfoComponents
	ManagedSignalIds.Empty();
	TArray<USignalInfoComponent*> InfoComps = GetSignalInfoComponents();
	for (const USignalInfoComponent* Comp : InfoComps)
	{
		ManagedSignalIds.Add(Comp->SignalId);
	}

	// Bind to the traffic light subsystem
	UTrafficLightSubsystem* Subsystem = GetWorld()->GetSubsystem<UTrafficLightSubsystem>();
	if (Subsystem)
	{
		Subsystem->OnTrafficLightStateUpdated.AddDynamic(
			this, &AOsiTrafficLightAssemblyActor::OnSubsystemStateUpdated);
	}
}

void AOsiTrafficLightAssemblyActor::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UTrafficLightSubsystem* Subsystem = GetWorld()->GetSubsystem<UTrafficLightSubsystem>())
	{
		Subsystem->OnTrafficLightStateUpdated.RemoveDynamic(
			this, &AOsiTrafficLightAssemblyActor::OnSubsystemStateUpdated);
	}

	Super::EndPlay(Reason);
}

void AOsiTrafficLightAssemblyActor::OnSubsystemStateUpdated(
	int32 TrafficLightId, const FOsiTrafficLightState& NewState)
{
	if (!ManagedSignalIds.Contains(TrafficLightId))
	{
		return;
	}

	IBPI_TrafficLightUpdate::Execute_OnTrafficLightAssemblyUpdate(
		this, TrafficLightId, NewState);
}

void AOsiTrafficLightAssemblyActor::OnTrafficLightUpdate_Implementation(
	const FOsiTrafficLightState& NewState)
{
	// Default: no-op. Assembly uses OnTrafficLightAssemblyUpdate instead.
}

void AOsiTrafficLightAssemblyActor::OnTrafficLightAssemblyUpdate_Implementation(
	int32 SignalId, const FOsiTrafficLightState& NewState)
{
	// Default: no-op. Override in Blueprint or C++ subclass.
}
