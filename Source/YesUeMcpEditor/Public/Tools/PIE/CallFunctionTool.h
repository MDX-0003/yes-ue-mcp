// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "CallFunctionTool.generated.h"

/**
 * Call a Blueprint or C++ function on an actor in the PIE world.
 */
UCLASS()
class YESUEMCPEDITOR_API UCallFunctionTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("call-function"); }
	virtual FString GetToolDescription() const override;
	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override { return {TEXT("actor_name"), TEXT("function_name")}; }
	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;
	virtual bool RequiresGameThread() const override { return true; }

private:
	AActor* FindActorByName(UWorld* World, const FString& ActorName) const;
	bool SetPropertyFromJson(void* PropertyAddr, FProperty* Property, const TSharedPtr<FJsonValue>& JsonValue) const;
};
