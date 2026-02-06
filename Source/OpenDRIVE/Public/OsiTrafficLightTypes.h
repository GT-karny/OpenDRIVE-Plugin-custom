// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OsiTrafficLightTypes.generated.h"

/**
 * ASAM OSI TrafficLight.Classification.Color
 * Semantic color classification of a traffic light.
 */
UENUM(BlueprintType)
enum class EOsiTrafficLightColor : uint8
{
	UNKNOWN = 0	UMETA(DisplayName = "Unknown"),
	OTHER		UMETA(DisplayName = "Other"),
	RED			UMETA(DisplayName = "Red"),
	YELLOW		UMETA(DisplayName = "Yellow"),
	GREEN		UMETA(DisplayName = "Green"),
	BLUE		UMETA(DisplayName = "Blue"),
	WHITE		UMETA(DisplayName = "White"),
};

/**
 * ASAM OSI TrafficLight.Classification.Icon
 * Icon type displayed on a traffic light bulb.
 */
UENUM(BlueprintType)
enum class EOsiTrafficLightIcon : uint8
{
	UNKNOWN = 0					UMETA(DisplayName = "Unknown"),
	OTHER						UMETA(DisplayName = "Other"),
	NONE						UMETA(DisplayName = "None"),
	ARROW_STRAIGHT_AHEAD		UMETA(DisplayName = "Arrow Straight Ahead"),
	ARROW_LEFT					UMETA(DisplayName = "Arrow Left"),
	ARROW_DIAG_LEFT				UMETA(DisplayName = "Arrow Diagonal Left"),
	ARROW_STRAIGHT_AHEAD_LEFT	UMETA(DisplayName = "Arrow Straight Ahead Left"),
	ARROW_RIGHT					UMETA(DisplayName = "Arrow Right"),
	ARROW_DIAG_RIGHT			UMETA(DisplayName = "Arrow Diagonal Right"),
	ARROW_STRAIGHT_AHEAD_RIGHT	UMETA(DisplayName = "Arrow Straight Ahead Right"),
	ARROW_LEFT_RIGHT			UMETA(DisplayName = "Arrow Left Right"),
	ARROW_DOWN					UMETA(DisplayName = "Arrow Down"),
	ARROW_DOWN_LEFT				UMETA(DisplayName = "Arrow Down Left"),
	ARROW_DOWN_RIGHT			UMETA(DisplayName = "Arrow Down Right"),
	ARROW_CROSS					UMETA(DisplayName = "Arrow Cross"),
	PEDESTRIAN					UMETA(DisplayName = "Pedestrian"),
	WALK						UMETA(DisplayName = "Walk"),
	DONT_WALK					UMETA(DisplayName = "Don't Walk"),
	BICYCLE						UMETA(DisplayName = "Bicycle"),
	PEDESTRIAN_AND_BICYCLE		UMETA(DisplayName = "Pedestrian and Bicycle"),
	COUNTDOWN_SECONDS			UMETA(DisplayName = "Countdown Seconds"),
	COUNTDOWN_PERCENT			UMETA(DisplayName = "Countdown Percent"),
	TRAM						UMETA(DisplayName = "Tram"),
	BUS							UMETA(DisplayName = "Bus"),
	BUS_AND_TRAM				UMETA(DisplayName = "Bus and Tram"),
};

/**
 * ASAM OSI TrafficLight.Classification.Mode
 * Operating mode of a traffic light.
 */
UENUM(BlueprintType)
enum class EOsiTrafficLightMode : uint8
{
	UNKNOWN = 0	UMETA(DisplayName = "Unknown"),
	OTHER		UMETA(DisplayName = "Other"),
	OFF			UMETA(DisplayName = "Off"),
	CONSTANT	UMETA(DisplayName = "Constant"),
	FLASHING	UMETA(DisplayName = "Flashing"),
	COUNTING	UMETA(DisplayName = "Counting"),
};

/**
 * Composite state for an OSI traffic light bulb.
 * Contains color, icon, mode, and optional counter value.
 */
USTRUCT(BlueprintType)
struct FOsiTrafficLightState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSI Traffic Light")
	EOsiTrafficLightColor Color = EOsiTrafficLightColor::UNKNOWN;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSI Traffic Light")
	EOsiTrafficLightIcon Icon = EOsiTrafficLightIcon::UNKNOWN;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSI Traffic Light")
	EOsiTrafficLightMode Mode = EOsiTrafficLightMode::UNKNOWN;

	/** Counter value. Unit depends on icon: seconds for COUNTDOWN_SECONDS, percent for COUNTDOWN_PERCENT. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSI Traffic Light")
	float Counter = 0.f;
};

/**
 * Entry for batch updating multiple traffic lights at once.
 */
USTRUCT(BlueprintType)
struct FOsiTrafficLightBatchEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSI Traffic Light")
	int32 TrafficLightId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSI Traffic Light")
	FOsiTrafficLightState State;
};
