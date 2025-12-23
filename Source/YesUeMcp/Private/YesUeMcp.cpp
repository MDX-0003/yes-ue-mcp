// Copyright softdaddy-o 2024. All Rights Reserved.

#include "YesUeMcp.h"
#include "Misc/EngineVersionComparison.h"

// Enforce minimum engine version at compile time
static_assert(UE_VERSION_NEWER_THAN(5, 5, 0), "yes-ue-mcp requires Unreal Engine 5.6 or higher. Please upgrade your engine version.");

DEFINE_LOG_CATEGORY(LogYesUeMcp);

#define LOCTEXT_NAMESPACE "FYesUeMcpModule"

void FYesUeMcpModule::StartupModule()
{
	UE_LOG(LogYesUeMcp, Log, TEXT("YesUeMcp module starting up"));
}

void FYesUeMcpModule::ShutdownModule()
{
	UE_LOG(LogYesUeMcp, Log, TEXT("YesUeMcp module shutting down"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FYesUeMcpModule, YesUeMcp)
