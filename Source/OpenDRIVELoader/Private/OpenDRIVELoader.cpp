// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenDRIVELoader.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FOpenDRIVELoaderModule"

void FOpenDRIVELoaderModule::StartupModule() {
	FString BaseDir = IPluginManager::Get().FindPlugin("OpenDRIVE")->GetBaseDir();

	// Load RoadManager.dll
	FString LibraryPath = FPaths::Combine(*BaseDir, TEXT("Source/ThirdParty/RoadManager/bin/RelWithDebInfo/RoadManager.dll"));
	RoadManagerHandle = !LibraryPath.IsEmpty() ? FPlatformProcess::GetDllHandle(*LibraryPath) : nullptr;

	// Load GT_esminiLib.dll for GT_RM API
	FString GT_esminiLibPath = FPaths::Combine(*BaseDir, TEXT("Source/ThirdParty/GT_RM/bin/GT_esminiLib.dll"));
	GT_esminiLibHandle = !GT_esminiLibPath.IsEmpty() ? FPlatformProcess::GetDllHandle(*GT_esminiLibPath) : nullptr;
}

void FOpenDRIVELoaderModule::ShutdownModule() {
	FPlatformProcess::FreeDllHandle(GT_esminiLibHandle);
	GT_esminiLibHandle = nullptr;

	FPlatformProcess::FreeDllHandle(RoadManagerHandle);
	RoadManagerHandle = nullptr;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FOpenDRIVELoaderModule, OpenDRIVELoader)
