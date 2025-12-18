// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "ActorDetailsTool.generated.h"

/**
 * Tool for deep inspection of a specific actor
 */
UCLASS()
class YESUEMCPEDITOR_API UActorDetailsTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("get-actor-details"); }
	virtual FString GetToolDescription() const override;
	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override;

	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;

private:
	/** Find actor by name in the current level */
	class AActor* FindActorByName(const FString& ActorName) const;

	/** Convert actor to detailed JSON */
	TSharedPtr<FJsonObject> ActorToDetailedJson(class AActor* Actor, bool bIncludeProperties, bool bIncludeComponents) const;

	/** Convert component to detailed JSON */
	TSharedPtr<FJsonObject> ComponentToJson(class UActorComponent* Component, bool bIncludeProperties) const;

	/** Convert property to JSON */
	TSharedPtr<FJsonObject> PropertyToJson(class FProperty* Property, void* ValuePtr, UObject* Owner) const;

	/** Get property type as string */
	FString GetPropertyTypeString(class FProperty* Property) const;

	/** Get transform as JSON */
	TSharedPtr<FJsonObject> TransformToJson(const FTransform& Transform) const;
};
