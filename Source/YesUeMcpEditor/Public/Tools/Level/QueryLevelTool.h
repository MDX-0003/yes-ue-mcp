// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "QueryLevelTool.generated.h"

/**
 * Tool for querying actors in the currently open level
 */
UCLASS()
class YESUEMCPEDITOR_API UQueryLevelTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("query-level"); }
	virtual FString GetToolDescription() const override;
	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override;

	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;

private:
	/** Convert actor to JSON */
	TSharedPtr<FJsonObject> ActorToJson(class AActor* Actor, bool bIncludeComponents, bool bIncludeTransform) const;

	/** Get transform as JSON */
	TSharedPtr<FJsonObject> TransformToJson(const FTransform& Transform) const;

	/** Check if actor matches class filter (wildcard support) */
	bool MatchesClassFilter(class AActor* Actor, const FString& Filter) const;

	/** Check if actor matches folder filter */
	bool MatchesFolderFilter(class AActor* Actor, const FString& Filter) const;

	/** Check if actor matches tag filter */
	bool MatchesTagFilter(class AActor* Actor, const FString& Filter) const;

	/** Check if string matches wildcard pattern */
	bool MatchesWildcard(const FString& Name, const FString& Pattern) const;
};
