# R0-R5 Read Pipeline Optimization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 BlueprintHelper 读链路 R0-R5 优化落成可验证实现：先补全端到端计时与 payload 体积观测，再拆 GameThread 快照和纯数据格式化，最后引入 request-local 复用、纯数据缓存策略和回归测速门槛。  
**Architecture:** CLI/AgentFace 只负责请求解析、桥接、后处理和计时；UE GameThread 只做 UObject/Blueprint/Graph/Property 快照；后台或纯函数层只消费 DTO；缓存只允许 request-local 或纯数据元信息，不缓存可变 UObject。  
**Tech Stack:** UE 5.6 C++、BlueprintHelper Bridge、AgentFace TypeScript、Vitest/Node tests、PowerShell benchmark、BlueprintHelper CLI `--develop` timing。  

---

## 0. Scope And Constraints

本计划只覆盖读工具链路，不修改写工具 TaskRun 并发模型，也不引入 UE UObject 后台读。

- [x] 读链路对象访问必须在 GameThread 完成：`UObject`、`UBlueprint`、`UEdGraph`、`UWidgetTree`、`UProperty` / `FProperty` 反射都不得在后台线程直接访问。
- [x] 后台或纯格式化层只允许消费已拷贝的 DTO、字符串、数组、map、JSON value。
- [x] 不新增 namespace；新增 C++ 行为类默认独立 `.h/.cpp`。
- [x] 不用超长 `if` / `switch` 扩展格式分支；需要通过 formatter registry、route table 或 handler map 分派。
- [x] 不做旧读字段兼容；R0-R5 的输出以当前 ReadSpec / read_context schema 为基线。
- [x] 本计划执行完成后不由 Agent 执行 `git add`、`git commit`、`git push`；只输出建议提交命令。

## 1. Target File Structure

### UE C++

- [x] 新增 `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadSnapshotTypes.h`
- [x] 新增 `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadSnapshotService.h`
- [x] 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadSnapshotService.cpp`
- [x] 新增 `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadSnapshotFormatter.h`
- [x] 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadSnapshotFormatter.cpp`
- [x] 新增 `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadRequestSnapshotCache.h`
- [x] 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadRequestSnapshotCache.cpp`
- [x] 新增 `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperReadCachePolicy.h`
- [x] 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperReadCachePolicy.cpp`
- [x] 修改 `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonReadService.h`
- [x] 修改 `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonReadService.cpp`
- [x] 修改 `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicMdReadService.h`
- [x] 修改 `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicMdReadService.cpp`
- [x] 修改 `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeRouter.cpp`
- [x] 修改 `BlueprintHelper/Source/BlueprintHelper/Private/Shared/Utils/BlueprintHelperToolTimingUtils.h`
- [x] 修改 `BlueprintHelper/Source/BlueprintHelper/Private/Shared/Utils/BlueprintHelperToolTimingUtils.cpp`
- [x] 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Read/BlueprintHelperLogicReadSnapshotFormatterTests.cpp`

### AgentFace TypeScript

- [x] 新增 `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-payload-metrics.ts`
- [x] 修改 `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-handler.ts`
- [x] 修改 `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-payload.ts`
- [x] 新增 `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-handler.test.ts`
- [x] 新增 `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-payload-metrics.test.ts`
- [x] 新增 `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-capability-cache.ts`
- [x] 新增 `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-capability-cache.test.ts`

### Measurement And Docs

- [x] 新增 `BlueprintHelper/Develop/Scripts/MeasureReadContextTiming.ps1`
- [x] 修改 `BlueprintHelper/Develop/Plan/BlueprintHelper_v0.5.0_TaskSpecExecution_PerformanceOptimization_20260519_CN.md`
- [x] 修改 `BlueprintHelper/Develop/Plan/Optimization/BlueprintHelper_v0.5.0_TaskSpecExecution_PerformanceOptimization_20260519_CN/P3_ReadPipelineSnapshotCache_ImplementationPlan_CN.md`

## 2. R0: Timing And Payload Observability

R0 目标是让所有读工具都能在 `--develop` 下返回完整阶段耗时，并能看到 UE 原始返回、AgentFace 后处理前后体积。没有 R0，后续 R1-R5 的收益不可判定。

### Task R0.1: Add AgentFace Payload Metrics Unit Tests

- [x] 新增 `read-context-payload-metrics.test.ts`，覆盖 JSON 字节估算稳定性。

Test cases:

```ts
import { estimateJsonPayloadBytes } from "../../tool-surface/bridge/read-context/read-context-payload-metrics";

describe("read-context-payload-metrics", () => {
  it("estimates utf8 JSON payload bytes", () => {
    expect(estimateJsonPayloadBytes({ value: "abc" })).toBe(Buffer.byteLength(JSON.stringify({ value: "abc" }), "utf8"));
  });

  it("returns 0 for undefined payload", () => {
    expect(estimateJsonPayloadBytes(undefined)).toBe(0);
  });
});
```

### Task R0.2: Add Payload Metrics Helper

- [x] 新增 `read-context-payload-metrics.ts`，只处理纯数据，不依赖 Bridge 或 CLI。

Required API:

```ts
export function estimateJsonPayloadBytes(value: unknown): number {
  if (value === undefined) {
    return 0;
  }
  return Buffer.byteLength(JSON.stringify(value), "utf8");
}

export function buildPayloadSizeMetric(name: string, value: unknown): { name: string; duration_ms: number; bytes: number } {
  return {
    name,
    duration_ms: 0,
    bytes: estimateJsonPayloadBytes(value),
  };
}
```

### Task R0.3: Instrument ReadContext Handler

- [x] 在 `read-context-handler.ts` 的 bridge receive 后、UE timing strip 后、post process 后分别记录体积。
- [x] 新增阶段名称保持读工具通用：
  - `read_context.bridge_payload_bytes`
  - `read_context.ue_raw_payload_bytes`
  - `read_context.post_processed_payload_bytes`
- [x] 如当前 timing API 只支持 `name/duration_ms`，则扩展为兼容额外字段，不破坏已有消费者。
- [x] `--develop` 未开启时不得返回 `data.timing`。

Implementation direction:

```ts
const rawPayloadBytes = buildPayloadSizeMetric("read_context.ue_raw_payload_bytes", rawPayload);
const finalPayloadBytes = buildPayloadSizeMetric("read_context.post_processed_payload_bytes", processedPayload);
timing?.push(rawPayloadBytes);
timing?.push(finalPayloadBytes);
```

### Task R0.4: Split Bridge Receive And Parse Timing

- [x] 在 `executeReadContext()` 中将桥接阶段拆成：
  - `read_context.bridge_send_receive`
  - `read_context.bridge_payload_extract`
  - `read_context.ue_timing_extract`
  - `read_context.post_process_payload`
- [x] 不把 UE nested timing 合并成 AgentFace 阶段，只放在 `data.timing.nested` 或现有 UE nested 容器里。
- [x] 增加测试：fake bridge 返回带 nested UE timing 时，最终 `data.timing` 同时包含 AgentFace 阶段和 UE 阶段。

### Task R0.5: Add UE Route Timing Markers

- [x] 在 `BlueprintHelperBridgeRouter.cpp` 的 `HandleReadBlueprintLogicJson` / `HandleReadBlueprintLogicMd` 中补齐 route 层计时：
  - `ue.route.read_request_parse`
  - `ue.route.snapshot_read`
  - `ue.route.format_output`
  - `ue.route.response_wrap`
- [x] 计时必须通过 `FBlueprintHelperToolTimingUtils`，不能在 route 中手写 JSON timing object。
- [x] timing utils 需要支持整数 bytes marker：

```cpp
static void AddCounter(TArray<FBlueprintHelperToolTimingEntry>& Entries, const FString& Name, int64 Value);
```

Acceptance:

- [x] `read_context` 在 `--develop` 下返回 CLI/AgentFace/UE 分层 timing。
- [x] 普通模式不返回 timing。
- [x] payload byte metric 在读 JSON、读 MD、logic flow 后处理场景都有值。

## 3. R1: GameThread Snapshot And Pure Formatting

R1 目标是将读链路中“碰 UObject 的快照”和“不碰 UObject 的格式化”拆开，为后台格式化和复用创造边界。

### Task R1.1: Define Snapshot DTO Types

- [x] 新增 `BlueprintHelperLogicReadSnapshotTypes.h`，只定义纯数据结构。
- [x] DTO 不持有 `UObject*`、`UEdGraph*`、`UBlueprint*`、`FProperty*`。
- [x] 如必须记录对象身份，只保存 path/name/guid/string。

Required shape:

```cpp
struct FBlueprintHelperLogicReadSnapshot
{
    FString AssetPath;
    FString BlueprintName;
    FString GraphName;
    FString Format;
    TSharedPtr<FJsonObject> LogicObject;
    TArray<FString> Warnings;
    int32 NodeCount = 0;
    int32 EdgeCount = 0;
};
```

### Task R1.2: Add Snapshot Service

- [x] 新增 `FBlueprintHelperLogicReadSnapshotService`，职责只是在 GameThread 读取 UE 对象并生成 DTO。
- [x] Service 不负责 Markdown、compact、logic flow 或 CLI schema 后处理。
- [x] Service 内部复用当前 `LogicJsonReadService` 已有读取逻辑，但读取结果要沉淀为 DTO。

Required API:

```cpp
class FBlueprintHelperLogicReadSnapshotService
{
public:
    bool BuildSnapshot(const TSharedPtr<FJsonObject>& Request, FBlueprintHelperLogicReadSnapshot& OutSnapshot, FString& OutError) const;
};
```

### Task R1.3: Add Pure Formatter

- [x] 新增 `FBlueprintHelperLogicReadSnapshotFormatter`，输入 `FBlueprintHelperLogicReadSnapshot`，输出 JSON 或 MD。
- [x] Formatter 不 include `Engine/Blueprint.h`、`EdGraph/EdGraph.h`、`Blueprint/UserWidget.h` 等 UE 对象头。
- [x] 使用 formatter registry 替代格式分支：

```cpp
using FBlueprintHelperLogicSnapshotFormatHandler = TFunction<bool(const FBlueprintHelperLogicReadSnapshot&, TSharedPtr<FJsonObject>&, FString&)>;
```

- [x] 支持至少：
  - `logic_json`
  - `logic_md`

### Task R1.4: Migrate Logic Json Read Service

- [x] `FBlueprintHelperLogicJsonReadService` 只保留当前对外 API，不再直接承担所有读取和格式化职责。
- [x] 内部流程改为：
  1. parse request
  2. snapshot service build DTO
  3. formatter build JSON payload
  4. wrap result
- [x] 保证既有 ReadSpec 输出字段不变化。
- [x] 添加 golden 输出对比测试或手动基准样本对比：新旧 JSON 的关键字段一致。

### Task R1.5: Migrate Logic Md Read Service

- [x] `FBlueprintHelperLogicMdReadService` 复用同一个 snapshot service。
- [x] Markdown 生成只从 DTO 取值，不回读 UObject。
- [x] 删除 Json/MD 服务之间重复的节点枚举、图遍历、字段提取逻辑。

Acceptance:

- [x] Logic JSON 和 Logic MD 的 UE route timing 中能拆出 `snapshot_read` 与 `format_output`。
- [x] `format_output` 不访问 UObject。
- [x] Logic JSON / MD 输出与当前 ReadSpec 预期一致。

## 4. R2: Formatter Reuse And Route Generalization

R2 目标是让读格式化从单工具实现变成可复用边界，后续 component/widget/object property 读工具可以按同一模式迁移。

### Task R2.1: Create Formatter Contract Tests

- [x] 增加 formatter contract 测试：同一个 snapshot 可以产出 JSON 和 MD。
- [x] 测试不启动 editor，不加载 asset。
- [x] 测试 fixture 只使用 DTO。

### Task R2.2: Introduce Read Format Registry

- [x] 在 `FBlueprintHelperLogicReadSnapshotFormatter` 内维护 handler map。
- [x] 不允许新增 `if (Format == ...)` 或 `switch (Format)` 扩展格式。
- [x] 未知 format 返回结构化错误：

```json
{
  "ok": false,
  "error": {
    "code": "unsupported_read_format",
    "message": "Unsupported read format: ..."
  }
}
```

### Task R2.3: Normalize CLI Post Processing Names

- [x] 在 `read-context-payload.ts` 中将后处理阶段名稳定为：
  - `read_context.logic_flow_build_payload`
  - `read_context.compact_payload`
  - `read_context.filter_payload`
  - `read_context.post_process_payload`
- [x] 后处理函数不得依赖具体 UE route 名称；只消费 payload shape 和 request option。

### Task R2.4: Document Migration Pattern

- [x] 在 P3 计划文档中新增 “DTO formatter migration template” 小节。
- [x] 明确未来组件读、WidgetTree 读、object property 读迁移步骤：
  1. 先建 snapshot DTO
  2. 再建 pure formatter
  3. route 只编排
  4. AgentFace 只做请求和后处理

Acceptance:

- [x] Logic JSON / MD 没有重复格式化实现。
- [x] 新 format 增加点只在 registry，不改 route 超长分支。
- [x] 文档给出后续读工具迁移模板。

## 5. R3: Request-Local Snapshot Cache

R3 目标是复用同一次请求内已经产生的纯 DTO，避免同一 asset/graph 多格式、多视图读取重复访问 UObject。缓存生命周期只在一次 bridge request 内，不跨 CLI 请求。

### Task R3.1: Add Cache Key And Cache Class

- [x] 新增 `FBlueprintHelperLogicReadSnapshotCacheKey`。
- [x] key 至少包含：
  - `AssetPath`
  - `GraphName`
  - `Scope`
  - `ReadDetail`
  - `SchemaVersion`
- [x] 新增 `FBlueprintHelperLogicReadRequestSnapshotCache`。

Required API:

```cpp
struct FBlueprintHelperLogicReadSnapshotCacheKey
{
    FString AssetPath;
    FString GraphName;
    FString Scope;
    FString ReadDetail;
    FString SchemaVersion;

    FString ToStableString() const;
};

class FBlueprintHelperLogicReadRequestSnapshotCache
{
public:
    bool TryGet(const FBlueprintHelperLogicReadSnapshotCacheKey& Key, FBlueprintHelperLogicReadSnapshot& OutSnapshot) const;
    void Put(const FBlueprintHelperLogicReadSnapshotCacheKey& Key, const FBlueprintHelperLogicReadSnapshot& Snapshot);
    void Reset();
};
```

### Task R3.2: Wire Cache Into Snapshot Service

- [x] Snapshot service 增加可选 request cache 参数。
- [x] 有 cache 时先查 cache；miss 后 GameThread build snapshot 并写入 cache。
- [x] cache hit/miss 通过 timing counter 返回：
  - `ue.read_snapshot_cache_hit`
  - `ue.read_snapshot_cache_miss`

### Task R3.3: Keep Cache Request-Local

- [x] cache 实例只能由 `BlueprintHelperBridgeRouter.cpp` 在处理单次 read request 时创建。
- [x] 不允许静态全局 cache 保存 asset snapshot。
- [x] 不允许跨 CLI process 或 editor lifecycle 保存可变 asset snapshot。

### Task R3.4: Prepare Multi-Format Route Without Schema Breakage

- [x] 当前单格式 read route 使用 cache 但不改变输出 schema。
- [x] 如一个请求内只读一种格式，cache 主要作为边界准备，不宣称性能收益。
- [x] 后续若增加多格式 read route，只复用该 request-local cache，不新增第二套 cache。

Acceptance:

- [x] 单格式 read 输出不变。
- [x] cache hit/miss timing 在 `--develop` 可见。
- [x] 无跨请求可变 asset snapshot cache。

## 6. R4: Pure Data Cache Policy

R4 目标是只缓存不会被 UE 编辑器用户操作异步改写的纯数据，避免把 asset 内容缓存成潜在 stale 数据。

### Task R4.1: Define Cache Policy

- [x] 新增或记录 `FBlueprintHelperReadCachePolicy`，声明允许缓存的数据类别：
  - CLI schema metadata
  - read capability matrix
  - runtime profile static info
  - formatter registry metadata
- [x] 明确禁止缓存：
  - `UObject` 指针
  - `UBlueprint` / `UEdGraph` / `UWidgetTree` 指针
  - asset graph snapshot 跨请求缓存
  - property reflection result 跨请求缓存

### Task R4.2: AgentFace Capability Cache

- [x] 对 read capabilities 这类纯数据可在 AgentFace process 内缓存。
- [x] cache key 包含 tool version 和 schema version。
- [x] cache miss 后重新构建，不访问 UE。

### Task R4.3: Runtime Profile Boundary Check

- [x] 检查 runtime profile 读链路，不把 UE 核心对象状态纳入 cache。
- [x] 如 runtime profile 只来自 CLI/process metadata，可并发格式化。
- [x] 如 runtime profile 混入 editor 状态，必须拆为 GameThread snapshot + pure formatter。

Acceptance:

- [x] R4 不引入 asset 内容跨请求缓存。
- [x] capability/runtime static metadata 可复用并有 schema version key。
- [x] 文档记录允许和禁止缓存的边界。

## 7. R5: Benchmark And Regression Gates

R5 目标是用固定样本持续验证优化收益，避免只凭单次手工测速判断。

### Task R5.1: Add Read Benchmark Script

- [x] 新增 `Develop/Scripts/MeasureReadContextTiming.ps1`。
- [x] 参数：
  - `-SpecDir`
  - `-Iterations`
  - `-Warmup`
  - `-CliPath`
  - `-OutputDir`
- [x] 脚本只执行 CLI read，不负责启动/关闭 editor；editor lifecycle 仍使用 MCP。
- [x] 每次执行都附带 `--develop`。
- [x] 脚本解析：
  - total wall time
  - CLI/AgentFace timing
  - nested UE timing
  - payload byte metrics
  - success/failure

Expected command:

```powershell
.\BlueprintHelper\Develop\Scripts\MeasureReadContextTiming.ps1 `
  -SpecDir "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\v0.4.4\ReadSpecs\BP_ThirdPersonCharacter_20260519" `
  -Iterations 5 `
  -Warmup 1 `
  -CliPath ".\AgentFaceService\cli\build\cli\index.js" `
  -OutputDir ".\BlueprintHelper\.tmp\read_timing"
```

### Task R5.2: Define Representative Read Cases

- [x] 至少固定以下样本：
  - full blueprint logic json
  - full blueprint logic md
  - logic flow format
  - compact read
  - filtered component/variable/object read
- [x] 每个样本记录：
  - spec 文件名
  - asset path
  - format/detail/filter
  - success result
  - median / p95 / max
  - 最慢成功样本阶段表

### Task R5.3: Regression Thresholds

- [x] 在主优化文档中新增 R0-R5 regression gate。
- [x] 初始阈值：
  - R0 仅观测，不设性能门槛。
  - R1 后 `ue.route.format_output` 应从 `ue.route.snapshot_read` 中分离。
  - R1/R2 后 logic MD 与 logic JSON 重复快照读取次数不得增加。
  - R3 单格式 read 不要求显著提速，但不能比 R1 median 慢超过 5%。
  - R5 任何读样本失败不得写入成功测速表。

### Task R5.4: Update Main Optimization Doc

- [x] 将每轮测速结果写入主文档 R0-R5 章节。
- [x] 工具性失败不写入结果表，只写到执行日志或本地诊断。
- [x] 成功结果必须包含和写链路同等粒度的阶段表。

Acceptance:

- [x] 可以一键复测读链路典型样本。
- [x] 主文档有读链路成功样本阶段表。
- [x] R1-R3 的收益或无收益都有数据解释。

## 8. End-To-End Execution Order

- [x] 1. 完成 R0.1-R0.4 AgentFace timing 和 payload metrics。
- [x] 2. 完成 R0.5 UE route timing marker。
- [x] 3. 用现有 ReadSpecs 跑一次 baseline，写入主文档，标记为 R0。
- [x] 4. 完成 R1 snapshot DTO / service / formatter。
- [x] 5. 迁移 Logic JSON / MD 服务并跑输出一致性测试。
- [x] 6. 用同一 ReadSpecs 跑 R1 测速，写入主文档。
- [x] 7. 完成 R2 formatter registry 和迁移模板。
- [x] 8. 完成 R3 request-local cache primitive 与 route 接入。
- [x] 9. 完成 R4 pure data cache policy 与 capability metadata cache。
- [x] 10. 完成 R5 benchmark script、代表性样本表和 regression gate。
- [x] 11. 编译 UE plugin。
- [x] 12. 运行 AgentFace build/test。
- [x] 13. 最终更新主优化文档和 P3 计划状态。

## 9. Verification Commands

AgentFace:

```powershell
npm.cmd --prefix .\AgentFaceService\task-core run build
npm.cmd --prefix .\AgentFaceService\task-core run test:node
```

UE compile:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development "D:\UEProjects\Template\Template.uproject" -WaitMutex
```

Read timing:

```powershell
.\BlueprintHelper\Develop\Scripts\MeasureReadContextTiming.ps1 `
  -SpecDir "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\v0.4.4\ReadSpecs\BP_ThirdPersonCharacter_20260519" `
  -Iterations 5 `
  -Warmup 1 `
  -CliPath ".\AgentFaceService\cli\build\cli\index.js" `
  -OutputDir ".\BlueprintHelper\.tmp\read_timing"
```

## 10. Done Definition

- [x] `--develop` 下读工具返回 CLI/AgentFace/UE 分层 timing。
- [x] 普通模式不返回 timing。
- [x] Logic JSON / MD 读链路完成 snapshot 与 pure formatter 拆分。
- [x] request-local cache 已接入且不跨请求。
- [x] 纯数据 cache policy 已落文档并有代码边界。
- [x] 读工具 benchmark script 可复测代表性样本。
- [x] 主优化文档包含 R0-R5 测试结果和对照写链路阶段表。
- [x] UE plugin 编译通过。
- [x] AgentFace build/test 通过。
- [x] 最终输出只给建议 commit message 和手动提交命令，不执行 git 提交。

## 10.1 Execution Record

执行时间：2026-05-20

完成内容：

- R0：AgentFace read_context timing / payload bytes marker / UE route timing 已落地。
- R1：Logic JSON / MD 读链路已拆成 GameThread snapshot DTO 和 pure formatter。
- R2：formatter registry、DTO-only formatter contract test、P3 DTO migration template 已补齐。
- R3：request-local snapshot cache 已接入 route，单格式请求显示 miss=1、hit=0。
- R4：AgentFace read capability pure-data cache 与 C++ cache policy 已落地。
- R5：`MeasureReadContextTiming.ps1` 已可复测 ReadSpecs 并输出 summary/samples。

验证：

- `npm.cmd --prefix .\AgentFaceService\task-core run build`：通过。
- `npm.cmd --prefix .\AgentFaceService\task-core run test:node`：通过，142/142。
- `npm.cmd --prefix .\AgentFaceService\cli run build`：通过。
- `Build.bat TemplateEditor Win64 Development D:\UEProjects\Template\Template.uproject -WaitMutex`：通过。
- MCP 启动 Editor 后，11 个 ReadSpec 使用 `--develop`、`warmup=1`、`iterations=5` 测速成功 55/55，结果目录：`.tmp/read_timing/read_timing_20260520_024547`。
- 普通读调用不带 `--develop` 时返回 `timing_absent`。

未记录为性能失败的工具问题：

- Windows PowerShell 默认 ExecutionPolicy 阻止直接执行 `.ps1`，稳定调用方式为 `powershell -NoProfile -ExecutionPolicy Bypass -File ...`。
- 首版脚本误用 `ConvertFrom-Json -Depth` 且未读取 `tool_result.data.timing`，已修复后重测成功。

## 11. Suggested Manual Commit Message

变更需求：
1. 补齐读链路 R0-R5 分阶段测速、快照 DTO、纯格式化、request-local cache 与回归门槛计划

手动提交命令：

```powershell
git add BlueprintHelper/Develop/Plan/BlueprintHelper_v0.5.0_TaskSpecExecution_PerformanceOptimization_20260519_CN.md `
  BlueprintHelper/Develop/Plan/Optimization/BlueprintHelper_v0.5.0_TaskSpecExecution_PerformanceOptimization_20260519_CN/R0_R5_ReadPipeline_ExecutablePlan_CN.md `
  BlueprintHelper/Develop/Plan/Optimization/BlueprintHelper_v0.5.0_TaskSpecExecution_PerformanceOptimization_20260519_CN/P3_ReadPipelineSnapshotCache_ImplementationPlan_CN.md
git commit -m "变更需求：补齐读链路R0-R5可执行优化计划"
```

