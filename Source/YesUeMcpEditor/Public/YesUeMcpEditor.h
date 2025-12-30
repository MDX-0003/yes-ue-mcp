// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "YesUeMcp.h"

DECLARE_LOG_CATEGORY_EXTERN(LogYesUeMcpEditor, Log, All);
// Note: LogYesUeMcp is declared in YesUeMcp.h

class FYesUeMcpEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Register built-in tools */
	void RegisterBuiltInTools();
};
