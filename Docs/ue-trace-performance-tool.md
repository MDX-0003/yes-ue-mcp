# UE Trace 性能分析 Tool 开发文档

## 一、Tool 子类必须具备的内容

### 1. 头文件结构（`.h`）

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "MyTool.generated.h"   // UHT 生成文件，必须是最后一个 include

UCLASS()
class YESUEMCPEDITOR_API UMyTool : public UMcpToolBase
{
    GENERATED_BODY()

public:
    // ① 必须重写：工具唯一名称（用于注册表的 key）
    virtual FString GetToolName() const override { return TEXT("my-tool-name"); }

    // ② 必须重写：工具功能描述（暴露给 MCP 客户端/AI）
    virtual FString GetToolDescription() const override;

    // ③ 建议重写：输入参数 Schema（AI 通过此了解参数格式）
    virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;

    // ④ 建议重写：必填参数列表
    virtual TArray<FString> GetRequiredParams() const override;

    // ⑤ 必须重写：核心执行逻辑
    virtual FMcpToolResult Execute(
        const TSharedPtr<FJsonObject>& Arguments,
        const FMcpToolContext& Context) override;

private:
    // ⑥ 可选：私有辅助函数
};
```

### 2. 源文件结构（`.cpp`）

```cpp
#include "Tools/MyTool.h"
// 其他依赖 include...

// GetToolName() 可以直接在 .h 里 inline 返回，不需要在 .cpp 实现

FString UMyTool::GetToolDescription() const
{
    return TEXT("...");
}

TMap<FString, FMcpSchemaProperty> UMyTool::GetInputSchema() const
{
    TMap<FString, FMcpSchemaProperty> Schema;
    FMcpSchemaProperty Param;
    Param.Type = TEXT("string");      // "string" / "boolean" / "integer" / "number"
    Param.Description = TEXT("...");
    Param.bRequired = true;
    Schema.Add(TEXT("param_name"), Param);
    return Schema;
}

TArray<FString> UMyTool::GetRequiredParams() const
{
    return { TEXT("param_name") };
}

FMcpToolResult UMyTool::Execute(
    const TSharedPtr<FJsonObject>& Arguments,
    const FMcpToolContext& Context)
{
    // 1. 解析参数（用基类 helper）
    FString MyParam;
    if (!GetStringArg(Arguments, TEXT("param_name"), MyParam))
        return FMcpToolResult::Error(TEXT("Missing: param_name"));

    // 2. 检查取消
    if (Context.IsCancelled())
        return FMcpToolResult::Error(TEXT("Cancelled"));

    // 3. 执行业务逻辑...

    // 4. 返回结果
    return FMcpToolResult::Text(TEXT("result"));
    // 或: FMcpToolResult::Json(JsonObj)
    // 或: FMcpToolResult::Error(TEXT("reason"))
}
```

### 3. 注册到 Registry

在 `YesUeMcpEditor.cpp` 的 `RegisterBuiltInTools()` 里添加一行：

```cpp
Registry.RegisterToolClass(UMyTool::StaticClass());
```

或者使用头文件里提供的宏（在 .cpp 里）：

```cpp
REGISTER_MCP_TOOL(UMyTool)
```

---

### 核心要素总览

| 要素 | 位置 | 是否必须 | 说明 |
|------|------|----------|------|
| `UCLASS()` + `GENERATED_BODY()` | .h | ? 必须 | UE 反射系统要求 |
| 继承 `UMcpToolBase` | .h | ? 必须 | 注册时会校验 `IsChildOf` |
| `GetToolName()` | .h inline | ? 必须 | 注册表的唯一 key，不能重复 |
| `GetToolDescription()` | .cpp | ? 必须 | AI 理解工具用途的依据 |
| `GetInputSchema()` | .cpp | 建议 | 定义参数类型/描述 |
| `GetRequiredParams()` | .cpp | 建议 | 声明哪些参数不可省略 |
| `Execute()` | .cpp | ? 必须 | 核心业务逻辑 |
| 注册到 Registry | YesUeMcpEditor.cpp | ? 必须 | 否则工具不可被发现 |

---

## 二、UE Trace 性能分析 Tool 功能规划

### 工具名称

`analyze-ue-trace`

### 工具定位

读取 UE Unreal Insights 生成的 `.utrace` 文件（或连接运行中的 Trace Server），
解析 CPU/GPU/内存/帧时间等性能数据，以结构化 JSON 返回供 AI 分析瓶颈。

---

### 输入参数设计

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| `trace_file` | string | 否 | `.utrace` 文件的绝对路径；不传则尝试读取最新的 |
| `analysis_type` | string | 否 | `"frame_time"` / `"cpu"` / `"gpu"` / `"memory"` / `"all"`，默认 `"all"` |
| `frame_range` | string | 否 | 帧范围，如 `"100-200"`；不传则分析全部 |
| `top_n` | integer | 否 | 返回耗时 Top N 的函数/事件，默认 20 |
| `threshold_ms` | number | 否 | 只返回耗时超过此阈值（毫秒）的条目，默认 0 |

---

### 输出 JSON 结构设计

```json
{
  "summary": {
    "total_frames": 1000,
    "avg_frame_ms": 16.7,
    "max_frame_ms": 45.2,
    "min_frame_ms": 8.1,
    "fps_avg": 59.8,
    "trace_duration_s": 16.7
  },
  "cpu_hotspots": [
    { "name": "FooSystem::Tick", "avg_ms": 3.2, "max_ms": 8.1, "call_count": 1000 }
  ],
  "gpu_hotspots": [
    { "name": "BasePass", "avg_ms": 6.5, "max_ms": 12.0 }
  ],
  "memory": {
    "peak_mb": 2048,
    "avg_mb": 1800,
    "allocations_per_frame": 120
  },
  "frame_spikes": [
    { "frame": 342, "frame_ms": 45.2, "cause": "GC" }
  ],
  "bottleneck_hint": "CPU bound. Top cost: FooSystem::Tick (3.2ms avg)"
}
```

---

### 功能模块拆分

#### 模块 A：Trace 文件定位
- 接收路径参数，或自动搜索 `Saved/Profiling/UnrealInsights/` 目录下最新的 `.utrace`
- 校验文件存在性和格式合法性

#### 模块 B：数据解析层
- 调用 UE 的 `TraceAnalysis` 模块（`Trace::FStoreClient` / `UE::Trace::IAnalyzer`）
- 或 fork 子进程调用 `UnrealInsights -OpenTraceFile` 导出 CSV，再解析
- 输出统一的中间数据结构

#### 模块 C：帧时间分析
- 计算 avg/max/min 帧时间、FPS
- 识别帧尖峰（超过均值 2 倍）及其触发原因（GC/Streaming/等）

#### 模块 D：CPU 热点分析
- 从 `CpuProfiler` channel 提取 Scope 事件
- 统计各函数/Scope 的累计时间、调用次数、max 单次耗时
- 按 avg_ms 排序，返回 Top N

#### 模块 E：GPU 热点分析
- 从 `GpuProfiler` channel 提取 GPU timer 事件
- 统计 BasePass/ShadowDepth/Translucency 等阶段耗时

#### 模块 F：内存分析
- 从 `Memory` channel 提取分配事件
- 统计峰值、均值、每帧分配次数、最大单次分配

#### 模块 G：瓶颈诊断提示
- 根据 CPU/GPU 时间比较判断是 CPU-bound 还是 GPU-bound
- 生成 `bottleneck_hint` 文字摘要供 AI 直接引用

---

## 三、TODO 列表

### 阶段 1 — 基础框架搭建

- [ ] 新建 `UAnalyzeUeTraceTool.h` / `.cpp`，继承 `UMcpToolBase`
- [ ] 实现 `GetToolName()` 返回 `"analyze-ue-trace"`
- [ ] 实现 `GetInputSchema()` 定义上述 5 个参数
- [ ] 实现 `GetRequiredParams()` 返回空（全部可选）
- [ ] 在 `YesUeMcpEditor.cpp` 的 `RegisterBuiltInTools()` 注册新 Tool
- [ ] 添加头文件 include 到 `YesUeMcpEditor.cpp`

### 阶段 2 — 文件定位模块（模块 A）

- [ ] 实现 `FindTraceFile(const FString& HintPath)` 辅助函数
  - 如果 `HintPath` 非空则直接验证存在性
  - 否则扫描 `FPaths::ProjectSavedDir() / "Profiling/UnrealInsights/"` 找最新文件
- [ ] 文件不存在时返回 `FMcpToolResult::Error`

### 阶段 3 — 数据解析层（模块 B）

- [ ] 调研 UE5.5 `TraceAnalysis` 模块的可用 API
  - 检查 `Engine/Source/Developer/TraceAnalysis` 是否可依赖
  - 或通过 `UnrealInsights -stdout` 命令行导出再解析
- [ ] 确定解析方案，实现 `FTraceParseResult` 中间数据结构
- [ ] 处理解析超时（大文件场景），配合 `Context.IsCancelled()` 检查

### 阶段 4 — 各分析模块实现

- [ ] 实现模块 C：`AnalyzeFrameTime()`
- [ ] 实现模块 D：`AnalyzeCpuHotspots()`
- [ ] 实现模块 E：`AnalyzeGpuHotspots()`
- [ ] 实现模块 F：`AnalyzeMemory()`
- [ ] 实现模块 G：`GenerateBottleneckHint()`

### 阶段 5 — 结果序列化

- [ ] 将各模块输出组装为设计的 JSON 结构
- [ ] 支持 `analysis_type` 参数过滤，只返回请求的部分
- [ ] 支持 `top_n` 和 `threshold_ms` 过滤

### 阶段 6 — 测试与验证

- [ ] 用真实 `.utrace` 文件验证解析结果
- [ ] 验证大文件（>100MB）下的性能和取消响应
- [ ] 验证 `frame_range` 参数裁剪逻辑

---

## 四、文件规划

```
Plugins\yes-ue-mcp\Source\YesUeMcpEditor\
├── Public\Tools\Analysis\
│   └── AnalyzeUeTraceTool.h          ← 新增
└── Private\Tools\Analysis\
    └── AnalyzeUeTraceTool.cpp         ← 新增
```
