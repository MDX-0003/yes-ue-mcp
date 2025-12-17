// Copyright softdaddy-o 2024. All Rights Reserved.

#include "YesUeMcp.h"

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
