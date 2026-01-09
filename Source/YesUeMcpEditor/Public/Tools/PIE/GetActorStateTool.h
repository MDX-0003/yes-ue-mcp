// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "GetActorStateTool.generated.h"

/**
 * Get the state of an actor in the PIE world including location, properties, and components.
 */
UCLASS()
class YESUEMCPEDITOR_API UGetActorStateTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("get-actor-state"); }
	virtual FString GetToolDescription() const override;
	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override { return {TEXT("actor_name")}; }
	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;
	virtual bool RequiresGameThread() const override { return true; }

private:
	AActor* FindActorByName(UWorld* World, const FString& ActorName) const;
	TSharedPtr<FJsonValue> GetPropertyValue(UObject* Object, FProperty* Property) const;
};
