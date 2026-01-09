// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "PieActorTool.generated.h"

/**
 * Consolidated PIE actor control tool.
 * Actions: spawn, destroy, get-state, call-function
 */
UCLASS()
class YESUEMCPEDITOR_API UPieActorTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("pie-actor"); }
	virtual FString GetToolDescription() const override;
	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override { return {TEXT("action")}; }
	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;
	virtual bool RequiresGameThread() const override { return true; }

private:
	FMcpToolResult ExecuteSpawn(const TSharedPtr<FJsonObject>& Arguments, UWorld* PIEWorld);
	FMcpToolResult ExecuteDestroy(const TSharedPtr<FJsonObject>& Arguments, UWorld* PIEWorld);
	FMcpToolResult ExecuteGetState(const TSharedPtr<FJsonObject>& Arguments, UWorld* PIEWorld);
	FMcpToolResult ExecuteCallFunction(const TSharedPtr<FJsonObject>& Arguments, UWorld* PIEWorld);

	UWorld* GetPIEWorld() const;
	AActor* FindActorByName(UWorld* World, const FString& ActorName) const;
	UClass* FindActorClass(const FString& ClassName) const;
	TSharedPtr<FJsonValue> GetPropertyValue(UObject* Object, FProperty* Property) const;
	bool SetPropertyFromJson(void* PropertyAddr, FProperty* Property, const TSharedPtr<FJsonValue>& JsonValue) const;
};
