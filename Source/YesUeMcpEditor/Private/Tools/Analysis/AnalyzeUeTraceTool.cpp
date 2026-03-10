// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Analysis/AnalyzeUeTraceTool.h"
#include "Tools/McpToolResult.h"
#include "YesUeMcpEditor.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

// TraceServices API (Module B)
#include "TraceServices/ITraceServicesModule.h"
#include "TraceServices/AnalysisService.h"
#include "TraceServices/ModuleService.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/Model/Frames.h"
#include "TraceServices/Model/TimingProfiler.h"
#include "TraceServices/Model/Threads.h"
#include "TraceServices/Model/Memory.h"
#include "TraceServices/Containers/Tables.h"

FString UAnalyzeUeTraceTool::GetToolDescription() const
{
	return TEXT("Analyze a UE Unreal Insights .utrace file to extract CPU/GPU/frame-time "
		"performance data. Returns structured JSON with hotspots and a bottleneck hint. "
		"If trace_file is omitted, the most recent .utrace in Saved/Profiling/UnrealInsights/ is used.");
}

TMap<FString, FMcpSchemaProperty> UAnalyzeUeTraceTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty TraceFile;
	TraceFile.Type        = TEXT("string");
	TraceFile.Description = TEXT("Absolute path to the .utrace file. Omit to use the most recent file under Saved/Profiling/UnrealInsights/.");
	TraceFile.bRequired   = false;
	Schema.Add(TEXT("trace_file"), TraceFile);

	FMcpSchemaProperty AnalysisType;
	AnalysisType.Type        = TEXT("string");
	AnalysisType.Description = TEXT("Data to return: 'frame_time', 'cpu', 'gpu', 'memory', or 'all' (default: 'all').");
	AnalysisType.bRequired   = false;
	Schema.Add(TEXT("analysis_type"), AnalysisType);

	FMcpSchemaProperty FrameRange;
	FrameRange.Type        = TEXT("string");
	FrameRange.Description = TEXT("Frame range to analyze, e.g. '100-200'. Omit to analyze all frames.");
	FrameRange.bRequired   = false;
	Schema.Add(TEXT("frame_range"), FrameRange);

	FMcpSchemaProperty TopN;
	TopN.Type        = TEXT("integer");
	TopN.Description = TEXT("Return only the top N most expensive timers (default: 20).");
	TopN.bRequired   = false;
	Schema.Add(TEXT("top_n"), TopN);

	FMcpSchemaProperty ThresholdMs;
	ThresholdMs.Type        = TEXT("number");
	ThresholdMs.Description = TEXT("Exclude timers whose avg cost is below this threshold in ms (default: 0).");
	ThresholdMs.bRequired   = false;
	Schema.Add(TEXT("threshold_ms"), ThresholdMs);

	FMcpSchemaProperty NameFilter;
	NameFilter.Type        = TEXT("string");
	NameFilter.Description = TEXT("Comma-separated keywords to filter timer names (case-insensitive contains match). E.g. 'Render,Shadow,Draw' to focus on rendering timers.");
	NameFilter.bRequired   = false;
	Schema.Add(TEXT("name_filter"), NameFilter);

	FMcpSchemaProperty TimerName;
	TimerName.Type        = TEXT("string");
	TimerName.Description = TEXT("Timer name (exact or partial match) to drill into. When set, a 'callee_tree' showing its full call chain is returned alongside the hotspot tables.");
	TimerName.bRequired   = false;
	Schema.Add(TEXT("timer_name"), TimerName);

	FMcpSchemaProperty CalleeDepth;
	CalleeDepth.Type        = TEXT("integer");
	CalleeDepth.Description = TEXT("Maximum depth of the callee tree (default: 5).");
	CalleeDepth.bRequired   = false;
	Schema.Add(TEXT("callee_depth"), CalleeDepth);

	return Schema;
}

TArray<FString> UAnalyzeUeTraceTool::GetRequiredParams() const
{
	return {};
}

FMcpToolResult UAnalyzeUeTraceTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	const FString TraceFilePath = GetStringArgOrDefault(Arguments, TEXT("trace_file"),    TEXT(""));
	const FString AnalysisType  = GetStringArgOrDefault(Arguments, TEXT("analysis_type"), TEXT("all")).ToLower();
	const FString FrameRange    = GetStringArgOrDefault(Arguments, TEXT("frame_range"),   TEXT(""));
	const int32   TopN          = GetIntArgOrDefault   (Arguments, TEXT("top_n"),         20);
	const float   ThresholdMs   = GetFloatArgOrDefault (Arguments, TEXT("threshold_ms"),  0.0f);
	const FString NameFilterRaw = GetStringArgOrDefault(Arguments, TEXT("name_filter"),   TEXT(""));
	const FString TimerName     = GetStringArgOrDefault(Arguments, TEXT("timer_name"),    TEXT(""));
	const int32   CalleeDepth   = GetIntArgOrDefault   (Arguments, TEXT("callee_depth"),  5);

	// Parse name_filter into keywords array
	TArray<FString> NameFilters;
	if (!NameFilterRaw.IsEmpty())
	{
		NameFilterRaw.ParseIntoArray(NameFilters, TEXT(","));
		for (FString& KW : NameFilters)
		{
			KW = KW.TrimStartAndEnd().ToLower();
		}
		NameFilters.RemoveAll([](const FString& S){ return S.IsEmpty(); });
	}

	UE_LOG(LogYesUeMcp, Log,
		TEXT("analyze-ue-trace: file='%s' type='%s' range='%s' top_n=%d threshold=%.2f filter='%s' timer='%s'"),
		*TraceFilePath, *AnalysisType, *FrameRange, TopN, ThresholdMs, *NameFilterRaw, *TimerName);

	static const TArray<FString> ValidTypes = { TEXT("frame_time"), TEXT("cpu"), TEXT("gpu"), TEXT("memory"), TEXT("all") };
	if (!ValidTypes.Contains(AnalysisType))
	{
		return FMcpToolResult::Error(FString::Printf(
			TEXT("Invalid analysis_type '%s'. Must be one of: frame_time, cpu, gpu, memory, all."), *AnalysisType));
	}

	if (Context.IsCancelled())
	{
		return FMcpToolResult::Error(TEXT("Request cancelled."));
	}

	const FString ResolvedPath = FindTraceFile(TraceFilePath);
	if (ResolvedPath.IsEmpty())
	{
		return FMcpToolResult::Error(
			TraceFilePath.IsEmpty()
				? TEXT("No .utrace file found in Saved/Profiling/UnrealInsights/.")
				: FString::Printf(TEXT("Trace file not found: %s"), *TraceFilePath));
	}

	UE_LOG(LogYesUeMcp, Log, TEXT("analyze-ue-trace: resolved -> %s"), *ResolvedPath);

	const FTraceParseResult ParseResult = ParseTraceSession(ResolvedPath, Context, TimerName);
	if (!ParseResult.bValid)
	{
		return FMcpToolResult::Error(TEXT("Failed to parse trace file. Ensure TraceServices module is available."));
	}

	if (Context.IsCancelled())
	{
		return FMcpToolResult::Error(TEXT("Request cancelled after trace parsing."));
	}

	TSharedPtr<FJsonObject> ResultJson = BuildResultJson(ParseResult, AnalysisType, FrameRange, TopN, ThresholdMs, NameFilters);

	// Append callee tree if available
	if (ParseResult.CalleeTree.IsSet())
	{
		ResultJson->SetObjectField(TEXT("callee_tree"),
			ButterflyNodeToJson(ParseResult.CalleeTree.GetValue(), CalleeDepth));
	}

	return FMcpToolResult::Json(ResultJson);
}

// --- Module A ---

FString UAnalyzeUeTraceTool::FindTraceFile(const FString& HintPath) const
{
	if (!HintPath.IsEmpty())
	{
		UE_LOG(LogYesUeMcp, Log, TEXT("analyze-ue-trace: searching for .utrace in '%s'"), *HintPath);

		return IFileManager::Get().FileExists(*HintPath) ? HintPath : TEXT("");
	}

	const FString SearchDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Profiling/UnrealInsights/"));
	UE_LOG(LogYesUeMcp, Log, TEXT("analyze-ue-trace: searching for .utrace in '%s'"), *SearchDir);

	TArray<FString> FoundFiles;
	IFileManager::Get().FindFilesRecursive(FoundFiles, *SearchDir, TEXT("*.utrace"), true, false);

	if (FoundFiles.IsEmpty())
	{
		return TEXT("");
	}

	FoundFiles.Sort([](const FString& A, const FString& B)
	{
		return IFileManager::Get().GetTimeStamp(*A) > IFileManager::Get().GetTimeStamp(*B);
	});

	return FoundFiles[0];
}

// --- Module B ---

FTraceParseResult UAnalyzeUeTraceTool::ParseTraceSession(
	const FString& TracePath,
	const FMcpToolContext& Context,
	const FString& TimerNameForCallee) const
{
	FTraceParseResult Result;
	Result.TracePath = TracePath;

	// Prepare trimmed search string for timer_name (support partial contains match)
	FString TimerNameSearch = TimerNameForCallee;
	TimerNameSearch.TrimStartAndEndInline();

	ITraceServicesModule*
		TraceServicesModule = FModuleManager::GetModulePtr<ITraceServicesModule>("TraceServices");
	if (!TraceServicesModule)
	{
		UE_LOG(LogYesUeMcp, Error, TEXT("analyze-ue-trace: TraceServices module not available."));
		return Result;
	}

	TSharedPtr<TraceServices::IAnalysisService> AnalysisService =
		TraceServicesModule->GetAnalysisService();//��CreateAnalysisService��Ϊlazy����
	if (!AnalysisService.IsValid())
	{
		UE_LOG(LogYesUeMcp, Error, TEXT("analyze-ue-trace: Failed to create IAnalysisService."));
		return Result;
	}

	// Synchronous: blocks until file is fully processed
	TSharedPtr<const TraceServices::IAnalysisSession> Session =
		AnalysisService->Analyze(*TracePath);
	if (!Session.IsValid())
	{
		UE_LOG(LogYesUeMcp, Error,
			TEXT("analyze-ue-trace: Analyze() returned null for '%s'."), *TracePath);
		return Result;
	}

	if (Context.IsCancelled()) { return Result; }

	TraceServices::FAnalysisSessionReadScope ReadScope(*Session);
	//GetDuration�ڲ������Lock.ReadAccessCheck()����Ҫ��ReadScope�ڴ�֮ǰ����
	Result.DurationSeconds = Session->GetDurationSeconds();

	// B3: frames
	{
		static const FName FrameProviderName(TEXT("FrameProvider"));
		const TraceServices::IFrameProvider* FrameProvider =
			Session->ReadProvider<TraceServices::IFrameProvider>(FrameProviderName);

		if (FrameProvider)
		{
			const uint64 FrameCount = FrameProvider->GetFrameCount(ETraceFrameType::TraceFrameType_Game);
			Result.Frames.Reserve((int32)FMath::Min(FrameCount, (uint64)100000));

			FrameProvider->EnumerateFrames(
				ETraceFrameType::TraceFrameType_Game, (uint64)0, FrameCount,
				[&](const TraceServices::FFrame& Frame)
				{
					const double DurMs = (Frame.EndTime - Frame.StartTime) * 1000.0;
					if (DurMs <= 0.0) { return; } // skip incomplete frame records
					FTraceFrameRecord Rec;
					Rec.Index      = (int64)Frame.Index;
					Rec.DurationMs = DurMs;
					Result.Frames.Add(Rec);
				});

			UE_LOG(LogYesUeMcp, Log, TEXT("analyze-ue-trace: %d game frames"), Result.Frames.Num());
		}
		else
		{
			UE_LOG(LogYesUeMcp, Warning,
				TEXT("analyze-ue-trace: IFrameProvider not found (trace may lack Frame channel)."));
		}
	}

	if (Context.IsCancelled()) { return Result; }

	// B4: timer aggregations
	{
		const TraceServices::ITimingProfilerProvider* TimingProvider =
			TraceServices::ReadTimingProfilerProvider(*Session);

		if (TimingProvider)
		{
			TraceServices::FCreateAggreationParams AggParams;
			AggParams.IntervalStart   = 0.0;
			AggParams.IntervalEnd     = Result.DurationSeconds;
			AggParams.CpuThreadFilter = [](uint32) { return true; };
			AggParams.IncludeGpu      = true;
			AggParams.FrameType       = ETraceFrameType::TraceFrameType_Count;

			TraceServices::ITable<TraceServices::FTimingProfilerAggregatedStats>* AggTable =
				TimingProvider->CreateAggregation(AggParams);

			if (AggTable)
			{
				TSharedPtr<TraceServices::ITableReader<TraceServices::FTimingProfilerAggregatedStats>>
					TableReader(AggTable->CreateReader());

				while (TableReader.IsValid() && TableReader->IsValid())
				{
					const TraceServices::FTimingProfilerAggregatedStats* Row = TableReader->GetCurrentRow();
					if (Row && Row->Timer && Row->InstanceCount > 0)
					{
						FTraceTimerStat Stat;
						Stat.Name        = FString(Row->Timer->Name ? Row->Timer->Name : TEXT("(unknown)"));
						Stat.bIsGpu      = (Row->Timer->IsGpuTimer != 0);
						Stat.AvgMs       = Row->AverageInclusiveTime * 1000.0;
						Stat.MaxMs       = Row->MaxInclusiveTime     * 1000.0;
						Stat.TotalMs     = Row->TotalInclusiveTime   * 1000.0;
						Stat.ExclusiveMs = Row->TotalExclusiveTime   * 1000.0;
						Stat.CallCount   = (int64)Row->InstanceCount;
						Result.TimerStats.Add(Stat);
					}
					TableReader->NextRow();
				}

				delete AggTable;
				UE_LOG(LogYesUeMcp, Log, TEXT("analyze-ue-trace: %d timer stats"), Result.TimerStats.Num());
			}
		}
		else
		{
			UE_LOG(LogYesUeMcp, Warning,
				TEXT("analyze-ue-trace: ITimingProfilerProvider not found."));
		}
	}

	// B4b: per-thread timeline hotspots via raw event enumeration
	{
		const TraceServices::ITimingProfilerProvider* TimingProvider =
			TraceServices::ReadTimingProfilerProvider(*Session);
		const TraceServices::IThreadProvider& ThreadProvider =
			TraceServices::ReadThreadProvider(*Session);

		if (TimingProvider)
		{
			// Build threadId -> name map
			TMap<uint32, FString> ThreadNames;
			ThreadProvider.EnumerateThreads([&](const TraceServices::FThreadInfo& Info)
			{
				ThreadNames.Add(Info.Id, FString(Info.Name ? Info.Name : TEXT("Unknown")));
			});

			// For each CPU thread timeline, accumulate exclusive time per timer
			TimingProvider->ReadTimers([&](const TraceServices::ITimingProfilerTimerReader& TimerReader)
			{
				const uint32 TimerCount = TimerReader.GetTimerCount();

				ThreadProvider.EnumerateThreads([&](const TraceServices::FThreadInfo& ThreadInfo)
				{
					uint32 TimelineIndex = 0;
					if (!TimingProvider->GetCpuThreadTimelineIndex(ThreadInfo.Id, TimelineIndex))
					{
						return;
					}

					// Per-timer exclusive time accumulator for this thread
					TMap<uint32, double> ExclusiveAccum;
					TMap<uint32, double> InclusiveAccum;
					TMap<uint32, int64>  CallAccum;
					TMap<uint32, double> MaxAccum;

					TimingProvider->ReadTimeline(TimelineIndex,
						[&](const TraceServices::ITimeline<TraceServices::FTimingProfilerEvent>& Timeline)
						{
							Timeline.EnumerateEvents(
								0.0, Result.DurationSeconds,
								[&](double StartTime, double EndTime, uint32 Depth,
									const TraceServices::FTimingProfilerEvent& Event)
									-> TraceServices::EEventEnumerate
								{
									const uint32 TimerId = Event.TimerIndex;
									if (TimerId == uint32(-1)) { return TraceServices::EEventEnumerate::Continue; }

									const double DurSec = EndTime - StartTime;
									InclusiveAccum.FindOrAdd(TimerId) += DurSec;
									CallAccum.FindOrAdd(TimerId)      += 1;
									double& MaxRef = MaxAccum.FindOrAdd(TimerId);
									if (DurSec > MaxRef) MaxRef = DurSec;
									return TraceServices::EEventEnumerate::Continue;
								});
						});

					for (auto& KV : InclusiveAccum)
					{
						const TraceServices::FTimingProfilerTimer* Timer = TimerReader.GetTimer(KV.Key);
						if (!Timer || !Timer->Name) { continue; }

						const int64 Calls = CallAccum.FindRef(KV.Key);
						if (Calls == 0) { continue; }

						FTraceTimerStat Stat;
						Stat.Name       = FString(Timer->Name);
						Stat.ThreadName = ThreadInfo.Name ? FString(ThreadInfo.Name) : TEXT("Unknown");
						Stat.bIsGpu     = false;
						Stat.TotalMs    = KV.Value * 1000.0;
						Stat.AvgMs      = Stat.TotalMs / (double)Calls;
						Stat.MaxMs      = MaxAccum.FindRef(KV.Key) * 1000.0;
						Stat.CallCount  = Calls;
						Result.ThreadTimerStats.Add(MoveTemp(Stat));
					}
				});
			});

			UE_LOG(LogYesUeMcp, Log, TEXT("analyze-ue-trace: %d thread-timer stats"), Result.ThreadTimerStats.Num());
		}
	}

	// B4c: callee butterfly tree for a specific named timer
	if (!TimerNameForCallee.IsEmpty())
	{
		const TraceServices::ITimingProfilerProvider* TimingProvider =
			TraceServices::ReadTimingProfilerProvider(*Session);

		if (TimingProvider)
		{
			// Step 1: find the timer ID by name via ReadTimers
			uint32 FoundTimerId = uint32(-1);
			TimingProvider->ReadTimers([&](const TraceServices::ITimingProfilerTimerReader& TimerReader)
			{
				const uint32 Count = TimerReader.GetTimerCount();
				for (uint32 i = 0; i < Count; ++i)
				{
					const TraceServices::FTimingProfilerTimer* T = TimerReader.GetTimer(i);
							if (T && T->Name)
							{
								const FString TimerNameStr = FString(T->Name);
								// If user provided a timer_name, treat it as a case-insensitive substring search
								if (!TimerNameSearch.IsEmpty() && TimerNameStr.Contains(TimerNameSearch, ESearchCase::IgnoreCase))
								{
									FoundTimerId = i;
									break;
								}
							}
				}
			});

			if (FoundTimerId != uint32(-1))
			{
				// Step 2: build the callee tree
				TraceServices::ITimingProfilerButterfly* Butterfly =
					TimingProvider->CreateButterfly(
						0.0, Result.DurationSeconds,
						[](uint32) { return true; },
						true);

				if (Butterfly)
				{
					const TraceServices::FTimingProfilerButterflyNode& CalleeRoot =
						Butterfly->GenerateCalleesTree(FoundTimerId);

					// Recursive lambda to convert butterfly nodes to our struct
					TFunction<FTraceButterflyNode(const TraceServices::FTimingProfilerButterflyNode&)> ConvertNode;
					ConvertNode = [&](const TraceServices::FTimingProfilerButterflyNode& Src) -> FTraceButterflyNode
					{
						FTraceButterflyNode Dst;
						Dst.Name        = Src.Timer ? FString(Src.Timer->Name) : TEXT("(unknown)");
						Dst.InclusiveMs = Src.InclusiveTime * 1000.0;
						Dst.ExclusiveMs = Src.ExclusiveTime * 1000.0;
						Dst.CallCount   = (int64)Src.Count;
						for (const TraceServices::FTimingProfilerButterflyNode* Child : Src.Children)
						{
							if (Child)
							{
								Dst.Children.Add(ConvertNode(*Child));
							}
						}
						return Dst;
					};

					Result.CalleeTree = ConvertNode(CalleeRoot);
					delete Butterfly;

					UE_LOG(LogYesUeMcp, Log, TEXT("analyze-ue-trace: callee tree built for '%s'"), *TimerNameForCallee);
				}
			}
			else
			{
				UE_LOG(LogYesUeMcp, Warning,
					TEXT("analyze-ue-trace: timer_name '%s' not found in trace."), *TimerNameForCallee);
			}
		}
	}

	// B5: Module F �� LLM memory stats via IMemoryProvider
	{
		const TraceServices::IMemoryProvider* MemoryProvider =
			TraceServices::ReadMemoryProvider(*Session);

		if (MemoryProvider && MemoryProvider->GetTrackerCount() > 0)
		{
			// Find the default tracker (id == 0, usually "Default" LLM tracker)
			TraceServices::FMemoryTrackerId DefaultTracker = 0;

			MemoryProvider->EnumerateTrackers([&](const TraceServices::FMemoryTrackerInfo& Tracker)
			{
				if (Tracker.Id == 0)
				{
					DefaultTracker = Tracker.Id;
				}
			});

			MemoryProvider->EnumerateTags([&](const TraceServices::FMemoryTagInfo& Tag)
			{
				// Only process tags belonging to the default tracker
				if (!(Tag.Trackers & (uint64(1) << DefaultTracker)))
				{
					return;
				}

				const uint64 SampleCount = MemoryProvider->GetTagSampleCount(DefaultTracker, Tag.Id);
				if (SampleCount == 0)
				{
					return;
				}

				double SumBytes   = 0.0;
				double PeakBytes  = 0.0;
				double FinalBytes = 0.0;
				int32  Samples    = 0;

				MemoryProvider->EnumerateTagSamples(
					DefaultTracker, Tag.Id,
					0.0, Result.DurationSeconds,
					false,
					[&](double /*Time*/, double /*Duration*/, const TraceServices::FMemoryTagSample& Sample)
					{
						const double BytesD = (double)Sample.Value;
						SumBytes  += BytesD;
						if (BytesD > PeakBytes) PeakBytes = BytesD;
						FinalBytes = BytesD;
						++Samples;
					});

				if (Samples > 0)
				{
					constexpr double BytesToMb = 1.0 / (1024.0 * 1024.0);
					FTraceMemoryStat Stat;
					Stat.Name        = Tag.Name;
					Stat.PeakMb      = PeakBytes  * BytesToMb;
					Stat.AvgMb       = (SumBytes / (double)Samples) * BytesToMb;
					Stat.FinalMb     = FinalBytes * BytesToMb;
					Stat.SampleCount = Samples;
					Result.MemoryStats.Add(MoveTemp(Stat));
				}
			});

			UE_LOG(LogYesUeMcp, Log, TEXT("analyze-ue-trace: %d memory tag stats"), Result.MemoryStats.Num());
		}
		else
		{
			UE_LOG(LogYesUeMcp, Warning,
				TEXT("analyze-ue-trace: IMemoryProvider not found (trace may lack LLM channel)."));
		}
	}

	Result.bValid = true;
	return Result;
}

// --- Modules C-G ---

TSharedPtr<FJsonObject> UAnalyzeUeTraceTool::BuildResultJson(
	const FTraceParseResult& ParseResult,
	const FString& AnalysisType,
	const FString& FrameRange,
	int32 TopN,
	float ThresholdMs,
	const TArray<FString>& NameFilters) const
{
	// Helper: returns true if the timer name passes the keyword filter
	auto PassesFilter = [&](const FString& Name) -> bool
	{
		if (NameFilters.IsEmpty()) { return true; }
		const FString NameLower = Name.ToLower();
		for (const FString& KW : NameFilters)
		{
			if (NameLower.Contains(KW)) { return true; }
		}
		return false;
	};

	TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject);
	Root->SetStringField(TEXT("trace_file"), ParseResult.TracePath);
	if (!NameFilters.IsEmpty())
	{
		TArray<TSharedPtr<FJsonValue>> FilterArr;
		for (const FString& KW : NameFilters)
		{
			FilterArr.Add(MakeShareable(new FJsonValueString(KW)));
		}
		Root->SetArrayField(TEXT("active_filters"), FilterArr);
	}

	// Module C: frame time
	const bool bWantFrameTime = (AnalysisType == TEXT("all") || AnalysisType == TEXT("frame_time"));
	if (bWantFrameTime && !ParseResult.Frames.IsEmpty())
	{
		int64 RangeStart = 0;
		int64 RangeEnd   = (int64)ParseResult.Frames.Num();

		if (!FrameRange.IsEmpty())
		{
			TArray<FString> Parts;
			FrameRange.ParseIntoArray(Parts, TEXT("-"));
			if (Parts.Num() == 2)
			{
				RangeStart = FCString::Atoi64(*Parts[0]);
				RangeEnd   = FCString::Atoi64(*Parts[1]) + 1;
				RangeStart = FMath::Clamp(RangeStart, (int64)0, (int64)ParseResult.Frames.Num());
				RangeEnd   = FMath::Clamp(RangeEnd,   RangeStart, (int64)ParseResult.Frames.Num());
			}
		}

		double SumMs = 0.0, MaxMs = 0.0, MinMs = DBL_MAX;
		TArray<TSharedPtr<FJsonValue>> SpikesArray;

		for (int64 i = RangeStart; i < RangeEnd; ++i)
		{
			const FTraceFrameRecord& F = ParseResult.Frames[(int32)i];
			SumMs += F.DurationMs;
			if (F.DurationMs > MaxMs) MaxMs = F.DurationMs;
			if (F.DurationMs < MinMs) MinMs = F.DurationMs;
		}

		const int64  FrameCount = RangeEnd - RangeStart;
		const double AvgMs      = FrameCount > 0 ? SumMs / (double)FrameCount : 0.0;

		for (int64 i = RangeStart; i < RangeEnd; ++i)
		{
			const FTraceFrameRecord& F = ParseResult.Frames[(int32)i];
			if (F.DurationMs > AvgMs * 2.0)
			{
				TSharedPtr<FJsonObject> Spike = MakeShareable(new FJsonObject);
				Spike->SetNumberField(TEXT("frame"),    (double)F.Index);
				Spike->SetNumberField(TEXT("frame_ms"), F.DurationMs);
				SpikesArray.Add(MakeShareable(new FJsonValueObject(Spike)));
			}
		}

		TSharedPtr<FJsonObject> Summary = MakeShareable(new FJsonObject);
		Summary->SetNumberField(TEXT("total_frames"),     (double)FrameCount);
		Summary->SetNumberField(TEXT("avg_frame_ms"),     AvgMs);
		Summary->SetNumberField(TEXT("max_frame_ms"),     MaxMs);
		Summary->SetNumberField(TEXT("min_frame_ms"),     MinMs < DBL_MAX ? MinMs : 0.0);
		Summary->SetNumberField(TEXT("fps_avg"),          AvgMs > 0.0 ? 1000.0 / AvgMs : 0.0);
		Summary->SetNumberField(TEXT("trace_duration_s"), ParseResult.DurationSeconds);

		Root->SetObjectField(TEXT("summary"), Summary);
		Root->SetArrayField(TEXT("frame_spikes"), SpikesArray);
	}

	// Modules D + E: CPU / GPU hotspots
	const bool bWantCpu = (AnalysisType == TEXT("all") || AnalysisType == TEXT("cpu"));
	const bool bWantGpu = (AnalysisType == TEXT("all") || AnalysisType == TEXT("gpu"));

	if ((bWantCpu || bWantGpu) && !ParseResult.TimerStats.IsEmpty())
	{
		TArray<FTraceTimerStat> Sorted = ParseResult.TimerStats;
		Sorted.Sort([](const FTraceTimerStat& A, const FTraceTimerStat& B)
		{
			return A.AvgMs > B.AvgMs;
		});

		auto BuildHotspotArray = [&](bool bGpu) -> TArray<TSharedPtr<FJsonValue>>
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FTraceTimerStat& S : Sorted)
			{
				if (S.bIsGpu != bGpu)               continue;
				if (S.AvgMs  < (double)ThresholdMs)  continue;
				if (!PassesFilter(S.Name))           continue;
				if (Arr.Num() >= TopN)               break;

				TSharedPtr<FJsonObject> Entry = MakeShareable(new FJsonObject);
				Entry->SetStringField(TEXT("name"),         S.Name);
				Entry->SetNumberField(TEXT("avg_ms"),       S.AvgMs);
				Entry->SetNumberField(TEXT("max_ms"),       S.MaxMs);
				Entry->SetNumberField(TEXT("total_ms"),     S.TotalMs);
				Entry->SetNumberField(TEXT("exclusive_ms"), S.ExclusiveMs);
				Entry->SetNumberField(TEXT("call_count"),   (double)S.CallCount);
				Arr.Add(MakeShareable(new FJsonValueObject(Entry)));
			}
			return Arr;
		};

		if (bWantCpu) { Root->SetArrayField(TEXT("cpu_hotspots"), BuildHotspotArray(false)); }
		if (bWantGpu) { Root->SetArrayField(TEXT("gpu_hotspots"), BuildHotspotArray(true));  }

		// Per-thread hotspots (only when cpu or all requested)
		if (bWantCpu && !ParseResult.ThreadTimerStats.IsEmpty())
		{
			TArray<FTraceTimerStat> SortedThread = ParseResult.ThreadTimerStats;
			SortedThread.Sort([](const FTraceTimerStat& A, const FTraceTimerStat& B)
			{
				return A.TotalMs > B.TotalMs;
			});

			TArray<TSharedPtr<FJsonValue>> ThreadArr;
			for (const FTraceTimerStat& S : SortedThread)
			{
				if (S.TotalMs < (double)ThresholdMs) continue;
				if (!PassesFilter(S.Name))           continue;
				if (ThreadArr.Num() >= TopN)         break;

				TSharedPtr<FJsonObject> Entry = MakeShareable(new FJsonObject);
				Entry->SetStringField(TEXT("name"),       S.Name);
				Entry->SetStringField(TEXT("thread"),     S.ThreadName);
				Entry->SetNumberField(TEXT("avg_ms"),     S.AvgMs);
				Entry->SetNumberField(TEXT("max_ms"),     S.MaxMs);
				Entry->SetNumberField(TEXT("total_ms"),   S.TotalMs);
				Entry->SetNumberField(TEXT("call_count"), (double)S.CallCount);
				ThreadArr.Add(MakeShareable(new FJsonValueObject(Entry)));
			}
			Root->SetArrayField(TEXT("thread_hotspots"), ThreadArr);
		}

		// Module G: bottleneck hint �� only when both CPU and GPU data are present
		if (AnalysisType == TEXT("all"))
		{
			double TotalCpu = 0.0, TotalGpu = 0.0;
			for (const FTraceTimerStat& S : ParseResult.TimerStats)
			{
				(S.bIsGpu ? TotalGpu : TotalCpu) += S.TotalMs;
			}

			const FTraceTimerStat* TopCpu =
				Sorted.FindByPredicate([](const FTraceTimerStat& S){ return !S.bIsGpu; });
			const FTraceTimerStat* TopGpu =
				Sorted.FindByPredicate([](const FTraceTimerStat& S){ return  S.bIsGpu; });

			FString Hint;
			if (TotalCpu >= TotalGpu)
			{
				Hint = FString::Printf(TEXT("CPU bound. Top CPU cost: %s (%.2f ms avg)"),
					TopCpu ? *TopCpu->Name : TEXT("N/A"), TopCpu ? TopCpu->AvgMs : 0.0);
			}
			else
			{
				Hint = FString::Printf(TEXT("GPU bound. Top GPU cost: %s (%.2f ms avg)"),
					TopGpu ? *TopGpu->Name : TEXT("N/A"), TopGpu ? TopGpu->AvgMs : 0.0);
			}
			Root->SetStringField(TEXT("bottleneck_hint"), Hint);
		}
	}

	// Module F: memory stats
	const bool bWantMemory = (AnalysisType == TEXT("all") || AnalysisType == TEXT("memory"));
	if (bWantMemory && !ParseResult.MemoryStats.IsEmpty())
	{
		// Sort by peak descending
		TArray<FTraceMemoryStat> SortedMem = ParseResult.MemoryStats;
		SortedMem.Sort([](const FTraceMemoryStat& A, const FTraceMemoryStat& B)
		{
			return A.PeakMb > B.PeakMb;
		});

		TArray<TSharedPtr<FJsonValue>> MemArray;
		const int32 MemTopN = FMath::Min(TopN, SortedMem.Num());
		for (int32 i = 0; i < MemTopN; ++i)
		{
			const FTraceMemoryStat& M = SortedMem[i];
			if (M.PeakMb < (double)ThresholdMs) // ThresholdMs reused as MB threshold when type==memory
			{
				break;
			}
			TSharedPtr<FJsonObject> Entry = MakeShareable(new FJsonObject);
			Entry->SetStringField(TEXT("tag"),          M.Name);
			Entry->SetNumberField(TEXT("peak_mb"),      M.PeakMb);
			Entry->SetNumberField(TEXT("avg_mb"),       M.AvgMb);
			Entry->SetNumberField(TEXT("final_mb"),     M.FinalMb);
			Entry->SetNumberField(TEXT("sample_count"), (double)M.SampleCount);
			MemArray.Add(MakeShareable(new FJsonValueObject(Entry)));
		}

		// Overall peak = largest peak across all tags
		double OverallPeakMb = SortedMem.IsEmpty() ? 0.0 : SortedMem[0].PeakMb;

		TSharedPtr<FJsonObject> MemSummary = MakeShareable(new FJsonObject);
		MemSummary->SetNumberField(TEXT("overall_peak_mb"), OverallPeakMb);
		MemSummary->SetNumberField(TEXT("tag_count"),       (double)ParseResult.MemoryStats.Num());

		Root->SetObjectField(TEXT("memory_summary"), MemSummary);
		Root->SetArrayField(TEXT("memory_tags"), MemArray);
	}

	return Root;
}

TSharedPtr<FJsonObject> UAnalyzeUeTraceTool::ButterflyNodeToJson(
	const FTraceButterflyNode& Node,
	int32 MaxDepth,
	int32 CurrentDepth) const
{
	TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
	Obj->SetStringField(TEXT("name"),         Node.Name);
	Obj->SetNumberField(TEXT("inclusive_ms"), Node.InclusiveMs);
	Obj->SetNumberField(TEXT("exclusive_ms"), Node.ExclusiveMs);
	Obj->SetNumberField(TEXT("call_count"),   (double)Node.CallCount);

	if (CurrentDepth < MaxDepth && !Node.Children.IsEmpty())
	{
		// Build sorted index array by inclusive time descending
		TArray<int32> SortedIndices;
		SortedIndices.Reserve(Node.Children.Num());
		for (int32 i = 0; i < Node.Children.Num(); ++i)
		{
			SortedIndices.Add(i);
		}
		SortedIndices.Sort([&Node](int32 A, int32 B)
		{
			return Node.Children[A].InclusiveMs > Node.Children[B].InclusiveMs;
		});

		TArray<TSharedPtr<FJsonValue>> ChildArr;
		for (int32 Idx : SortedIndices)
		{
			ChildArr.Add(MakeShareable(new FJsonValueObject(
				ButterflyNodeToJson(Node.Children[Idx], MaxDepth, CurrentDepth + 1))));
		}
		Obj->SetArrayField(TEXT("callees"), ChildArr);
	}

	return Obj;
}
