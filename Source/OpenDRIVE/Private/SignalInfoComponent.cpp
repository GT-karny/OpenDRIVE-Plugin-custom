#include "SignalInfoComponent.h"

USignalInfoComponent::USignalInfoComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SignalId = 0;
	RoadId = 0;
	S = 0.0;
	T = 0.0;
	Value = 0.0;
	bIsDynamic = false;
	Height = 0.0;
	Width = 0.0;
}
