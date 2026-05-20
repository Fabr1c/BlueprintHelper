# P3 读链路快照、缓存与 Bridge Gap 收口实施计划

日期：2026-05-19

状态：2026-05-20 已完成 v0.5.0 范围的首轮实现和测速。实际执行入口与验收记录见 `R0_R5_ReadPipeline_ExecutablePlan_CN.md`；本文件同步阶段背景、现实落地边界和后续未纳入项。

## 目标

P3 承接当前 v0.5.0 文档中的 R1-R5。它不是把 UE 读操作并发化，而是在 GameThread 读取 UE 对象后，把纯 DTO 格式化、同请求快照复用、纯数据缓存和 Bridge gap 细分做成通用架构。

读链路必须坚持：UObject / Blueprint / UEdGraph / UWidgetTree / FProperty 只在 GameThread 读取，后台线程只能处理已经脱离 UE 对象生命周期的 Snapshot DTO。

更细的 R0-R5 执行 checklist、目标文件结构、验收命令和回归门槛见：`R0_R5_ReadPipeline_ExecutablePlan_CN.md`。P3 文档保留阶段背景和架构收口，R0-R5 文档作为实际执行入口。

## 范围

- GameThread Snapshot DTO。
- 后台 formatter。
- request-local snapshot cache。
- 纯数据缓存。
- Bridge gap 拆分：queue、transport、response serialization、AgentFace JSON parse / receive。
- 读工具指标回归门槛。

## 架构边界

- UE read service 负责 GameThread snapshot。
- Formatter 只消费 Snapshot DTO，不访问 UE 对象。
- AgentFace `read_context` 只做 route、Bridge payload、compact/filter 和 result wrap。
- DebugBundle、Review evidence、UI overlay 如需读模型，应复用 Snapshot/DTO，不重新解释 Blueprint 状态。
- 长期资产内容缓存不进 v0.5.0，除非先有明确失效策略。

## DTO formatter migration template

后续 component、WidgetTree、object property 等读工具迁移时复用同一模板：

1. 新建 `SnapshotTypes`：只保存字符串、数字、数组、map、JSON value，不保存 `UObject*` / `UBlueprint*` / `UEdGraph*` / `UWidgetTree*` / `FProperty*`。
2. 新建 `SnapshotService`：唯一职责是在 GameThread 读取 UE 对象并拷贝 DTO；不做 Markdown、compact、filter、UI 解释。
3. 新建 `SnapshotFormatter`：只消费 DTO，可在后台或纯函数路径运行；通过 registry / handler map 扩展格式，不在 route 中写格式特判。
4. route 只负责 parse request、调用 snapshot、调用 formatter、wrap response、记录 timing。
5. request-local cache 只能由 route 创建和销毁；asset 内容 snapshot 不跨 CLI 请求、不跨 editor lifecycle。
6. AgentFace 只做 schema parse、Bridge request、UE timing extract、payload size marker、compact/filter/logic_flow 后处理。

当前已落地的模板实例：

- `FBlueprintHelperLogicReadSnapshotService`
- `FBlueprintHelperLogicReadSnapshotFormatter`
- `FBlueprintHelperLogicReadRequestSnapshotCache`
- `FBlueprintHelperReadCachePolicy`

## 现实文件结构

### UE Logic Read

- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadSnapshotTypes.h`
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadSnapshotService.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadSnapshotService.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadSnapshotFormatter.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadSnapshotFormatter.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadRequestSnapshotCache.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadRequestSnapshotCache.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperReadCachePolicy.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperReadCachePolicy.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonReadService.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonReadService.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicMdReadService.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicMdReadService.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeRouter.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Read/BlueprintHelperLogicReadSnapshotFormatterTests.cpp`

### AgentFace Read

- `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-handler.ts`
- `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-route-builder.ts`
- `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-payload.ts`
- `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-logic-flow.ts`
- `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-payload-metrics.ts`
- `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-capability-cache.ts`
- `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-handler.test.ts`
- `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-payload-metrics.test.ts`
- `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-capability-cache.test.ts`
- `AgentFaceService/task-core/src/tests/tool-surface/tool-registry.contract.test.ts`

### Measurement And Docs

- `BlueprintHelper/Develop/Scripts/MeasureReadContextTiming.ps1`
- `BlueprintHelper/Develop/Plan/BlueprintHelper_v0.5.0_TaskSpecExecution_PerformanceOptimization_20260519_CN.md`
- `BlueprintHelper/Develop/Plan/Optimization/BlueprintHelper_v0.5.0_TaskSpecExecution_PerformanceOptimization_20260519_CN/R0_R5_ReadPipeline_ExecutablePlan_CN.md`

## Snapshot DTO 规则

- DTO 字段必须是值类型、字符串、数组、map 或自有结构体。
- 禁止保存 `UObject*`、`UEdGraph*`、`UEdGraphNode*`、`UEdGraphPin*`、`UWidget*`、`FProperty*`。
- DTO 必须能在没有 Editor 对象访问的单元测试里格式化。
- cache key 至少包含 asset path、target graph/function/event/block、read detail、format options。
- request 结束后释放 snapshot cache。

## 现实进度 Checklist

### R1 / P3-1 GameThread Snapshot + 后台格式化

- [x] 测试：Logic formatter 可以只用 `FBlueprintHelperLogicReadSnapshot` 生成 `LogicJson.v1` / `LogicMd.v1`。
- [x] 测试：formatter 不依赖 UObject。
- [x] UE timing 增加 `ue.route.snapshot_read` 和 `ue.route.format_output`。
- [x] `blueprint_logic_json` / `blueprint_logic_md` 已迁移到 `BuildSnapshot -> FormatSnapshot`。
- [x] 输出 schema 与现有 `LogicJson.v1`、`LogicMd.v1`、`ReadContextPack.v1` 保持一致。

### R2 / P3-2 DTO / Formatter 复用

- [x] 抽出 Logic snapshot service、formatter、request-local cache 和 cache policy 边界。
- [x] `read_context` 只做 schema parse、Bridge request、UE timing extract、payload size marker、compact/filter/logic_flow 后处理。
- [x] `logic_flow` 已按现实实现适配：复用 `read_blueprint_logic_json`，由 AgentFace 从结构化 `LogicJson.v1` 生成 `LogicFlow.v1`，不单独读取 UE 对象。
- [x] AgentFace compact/filter 不改变 UE DTO 业务含义，并通过 contract tests 验证 `logic_flow` 不暴露 raw anchors / UE identity fields。
- Component / Widget / ObjectProperty formatter 迁移未纳入 v0.5.0 完成范围；后续只在 timing 证据证明必要时按同一模板迁移。

### R3 / P3-3 同请求快照复用

- [x] 实现 request-local snapshot cache primitive。
- [x] request-local cache 已接入 `read_blueprint_logic_json` / `read_blueprint_logic_md` route。
- [x] cache key 包含 asset path、scope、target name、block id、schema version，避免不同 target graph/function/event 误用同一 snapshot。
- [x] request-local cache 只由 route 创建并在请求结束释放。
- [x] 不实现跨请求 Blueprint 内容缓存；当前单格式 ReadSpec 样本显示 miss=1、hit=0，这是现实请求形态的预期结果。

### R4 / P3-4 纯数据缓存

- [x] AgentFace read capability pure-data cache 已落地。
- [x] C++ `FBlueprintHelperReadCachePolicy` 已落地，用于明确可缓存与不可缓存边界。
- [x] 缓存仅用于 CLI schema metadata、capability matrix、纯 runtime profile 等纯数据。
- [x] Blueprint 图、WidgetTree、DataTable rows、DataAsset properties 不进入长期缓存。
- [x] 如果未来需要资产内容缓存，必须先定义 package dirty、asset save、editor change event 或 request-local invalidation。

### R5 / P3-5 Bridge Gap 细分和回归指标

- [x] AgentFace timing 已增加 Bridge payload bytes、UE raw payload bytes、post processed payload bytes。
- [x] UE read router timing 已增加 `read_request_parse`、`snapshot_read`、`format_output`、`response_wrap`。
- [x] Bridge transport timing 已拆出 CLI connect/write/client_parse、UE Bridge receive、GameThread enqueue wait、route execute、response serialize。
- [x] response socket write 发生在 response JSON 序列化之后，不写回同一 response body，避免伪造同帧 timing。
- [x] 11 个 ReadSpec 已使用 `--develop`、`warmup=1`、`iterations=5` 重跑成功 55/55。
- [x] 每个长读案例已记录 min / avg / p50 / max / payload size；代表性数据写入主优化文档。

## 验收命令

```powershell
npm.cmd --prefix .\AgentFaceService\task-core run build
node .\AgentFaceService\task-core\scripts\run-node-tests.mjs
npm.cmd --prefix .\AgentFaceService\cli run build
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development "D:\UEProjects\Template\Template.uproject" -WaitMutex
git diff --check
```

读链路测速样本：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\BlueprintHelper\Develop\Scripts\MeasureReadContextTiming.ps1 `
  -SpecDir "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\v0.4.4\ReadSpecs\BP_ThirdPersonCharacter_20260519" `
  -Iterations 5 `
  -Warmup 1 `
  -CliPath ".\AgentFaceService\cli\build\cli\index.js" `
  -OutputDir ".\.tmp\read_timing"
```

## 指标表

| Spec | median_wall_ms | slowest_bridge_ms | slowest_ue_ms | raw_bytes | final_bytes | 备注 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `02_blueprint_logic_json.json` | 1994.332 | 1894.589 | 0.401 | 2894 | 2061 | 代表性最慢成功样本；nested `snapshot_read=0.231ms`、`format_output=0.125ms`。 |
| `03_blueprint_logic_md.json` | 1997.072 | 1900.322 | 0.353 | 2026 | 1103 | 复用 snapshot + formatter；Markdown 输出由 DTO 生成。 |
| `11_blueprint_logic_flow.json` | 1997.659 | 1898.527 | 0.429 | 2895 | 445 | 复用 `read_blueprint_logic_json`，AgentFace `logic_flow_build_payload=0.971ms`。 |

Bridge close 修复后，同一批 11 个 ReadSpec 成功样本的 median wall 范围降为 328.634-330.013ms，slowest bridge 范围降为 222.467-233.793ms，UE route 仍为 0.016-0.454ms。该结果证明 P3 内部 UObject 读取和 formatter 不是当前主瓶颈。

完成标准：
- [x] 后台 formatter 不触碰 UObject / Blueprint / UEdGraph / UWidgetTree / FProperty。
- [x] `blueprint_logic_json/md` 至少能显示 `snapshot_read` 与 `format_output` 占比。
- [x] 普通读工具不返回 develop timing。
- [x] 同一 ReadSpec 优化前后 payload schema 兼容。
- [x] Bridge gap 能进一步拆分，不再只有 `bridge - UE route` 单一差值。

## 风险控制

- 不把 full blueprint read 已覆盖的场景改成泛化 batch read 主线。
- 不把用户可编辑资产内容放进长期缓存。
- 不让后台线程直接访问 UE 反射对象。
- 不在 CLI 或 UI 入口写 read type 特判。
