// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/** Plugin version - keep in sync with YesUeMcp.uplugin */
#define YESUEMCP_VERSION TEXT("1.7.4")

DECLARE_LOG_CATEGORY_EXTERN(LogYesUeMcp, Log, All);

class FYesUeMcpModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
