// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "AnalyzeUeTraceTool.generated.h"

// --------------------------------------------------------------------------
// Intermediate data structures (Module B output, Modules C-G input)
// --------------------------------------------------------------------------

/** Per-timer aggregated stats collected from the timing profiler */
struct FTraceTimerStat
{
	FString Name;
	FString ThreadName;      // empty = aggregated across all threads
	bool    bIsGpu       = false;
	double  AvgMs        = 0.0;
	double  MaxMs        = 0.0;
	double  TotalMs      = 0.0;
	double  ExclusiveMs  = 0.0; // exclusive time (self cost, no children)
	int64   CallCount    = 0;
};

/** Per-frame record */
struct FTraceFrameRecord
{
	int64  Index      = 0;
	double DurationMs = 0.0;
};

/** Per-LLM-tag memory summary collected from IMemoryProvider */
struct FTraceMemoryStat
{
	FString Name;
	double  PeakMb      = 0.0;
	double  AvgMb       = 0.0;
	double  FinalMb     = 0.0;
	int32   SampleCount = 0;
};

/** One node in a callee/caller butterfly tree */
struct FTraceButterflyNode
{
	FString Name;
	double  InclusiveMs = 0.0;
	double  ExclusiveMs = 0.0;
	int64   CallCount   = 0;
	TArray<FTraceButterflyNode> Children;
};

/** Full parsed result produced by ParseTraceSession() */
struct FTraceParseResult
{
	// Session meta
	FString TracePath;
	double  DurationSeconds = 0.0;

	// Frame data (game thread frames, invalid frames filtered out)
	TArray<FTraceFrameRecord> Frames;

	// CPU + GPU timers ¡ª cross-thread aggregation (same as Insights "Timers" panel)
	TArray<FTraceTimerStat> TimerStats;

	// Per-thread timer hotspots from raw timeline enumeration
	TArray<FTraceTimerStat> ThreadTimerStats;

	// Module F: LLM memory tag stats
	TArray<FTraceMemoryStat> MemoryStats;

	// Optional callee tree for a specific timer (populated when timer_name is specified)
	TOptional<FTraceButterflyNode> CalleeTree;

	bool bValid = false;
};

// --------------------------------------------------------------------------

/**
 * Tool for analyzing UE Unreal Insights .utrace performance files.
 * Parses CPU/GPU/memory/frame-time data and returns structured JSON for AI analysis.
 */
UCLASS()
class YESUEMCPEDITOR_API UAnalyzeUeTraceTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("analyze-ue-trace"); }
	virtual FString GetToolDescription() const override;
	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override;

	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;

private:
	// Module A: file location
	FString FindTraceFile(const FString& HintPath) const;

	// Module B: full session parse via TraceServices API
	// timer_name: if non-empty, also builds a callee tree for that specific timer
	FTraceParseResult ParseTraceSession(
		const FString& TracePath,
		const FMcpToolContext& Context,
		const FString& TimerNameForCallee = TEXT("")) const;

	// Modules C-G: analysis helpers
	TSharedPtr<FJsonObject> BuildResultJson(
		const FTraceParseResult& ParseResult,
		const FString& AnalysisType,
		const FString& FrameRange,
		int32 TopN,
		float ThresholdMs,
		const TArray<FString>& NameFilters) const;

	// Helper: convert a butterfly node tree to JSON recursively
	TSharedPtr<FJsonObject> ButterflyNodeToJson(
		const FTraceButterflyNode& Node,
		int32 MaxDepth,
		int32 CurrentDepth = 0) const;
};
