# BlueprintHelper ReadContext FunctionGraph Logic 读取审计

日期：2026-05-31

## 结论

审计定位到的根因是 UE 侧 `logic_json` / `logic_md` function-target 路径存在契约缺口：function graph 的真实导出 JSON 不包含 `UK2Node_FunctionEntry` 节点，而修复前的 target-function formatter 必须在导出 JSON 的 `nodes[]` 中找到 FunctionEntry 才会构建 function body payload。二者组合会让普通 Agent 使用官方 function ReadSpec 模板时读到空 logic，或者只看到缺失入口的结果。

本次修复后，`target_type=function` 仍然是 function body read 的唯一语义入口。UE logic read formatter 会在目标 function graph 与请求的 function name 匹配、但 exporter 没有输出 `FunctionEntry` 节点时，合成只属于 `LogicJson/LogicMd` 非 importable payload 的 function entry/result boundary metadata，然后返回该 function graph 的 boundary nodes 与 body nodes。普通 Agent 不需要也不应该退回 `target_type=graph`。

## 最小相关调用

官方/模板导向的 function 读取形态如下：

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "/Game/Blueprints/BP_YourBlueprint",
    "target_type": "function",
    "target_name": "YourFunction"
  },
  "view": {
    "format": "logic_json"
  }
}
```

不应再把 `target_type=graph` + `target_name=<FunctionGraphName>` 当作普通 Agent 的 function body read 路径。该方式读取的是 graph-scope grouped body，不等同于 function-target 语义；它不会恢复 function entry 作为稳定入口语义，也不会让被 exporter 隐藏的 `FunctionEntry` / `FunctionResult` 重新出现在 `nodes[]` 中。

## 证据链

| 层级 | 当前证据 | 判断 |
| --- | --- | --- |
| AgentFace schema | `ReadContextInputSchema` 允许 `read_type=blueprint_logic`、`target_type=function`、`view.format=logic_flow/logic_md/logic_json`。证据：`AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-schemas.ts`。 | TS schema 没有阻止 function logic read。 |
| AgentFace payload | `buildBlueprintLogicReadPayload` 会把 function target 转成 `target_type=function`、`target_name`、`function`、`scope=target_function`。证据：`read-context-route-builder.ts:58-92`。 | TS payload 方向正确。 |
| UE snapshot | `TargetFunction` 且 `Target.Graph` 为空时，`BuildSnapshot` 选择 `FullBlueprint` 导出，然后设置 `bTargetEntryScope=true`。证据：`BlueprintHelperLogicReadSnapshotService.cpp:65-96`。 | function target 会进入 target-entry slice。 |
| UE formatter | `BuildLogicJsonData` / `BuildLogicMdData` 在 target-entry scope 下调用 `GroupBuilder.BuildTargetEntry(...)`。证据：`BlueprintHelperLogicReadSnapshotFormatter.cpp:279-285`, `338-348`。 | function body 是否出现取决于 `BuildTargetEntry`。 |
| target-entry builder | 修复前 `BuildTargetEntry` 对 `TargetFunction` 要求在 graph `nodes[]` 中找到 `FunctionEntry`，找不到则返回空 payload。修复后仅在 `TargetFunction`、graph 名匹配 target function、且 graph 有 body nodes 时合成 function entry/result boundary metadata，并让 graph-level links 解析 `__function_entry__` / `__function_result__` alias。证据：`BlueprintHelperLogicGroupBuilder.cpp`。 | 空读条件与隐藏 boundary link 丢失问题已在 function-target formatter 边界修复，不污染 raw exporter。 |
| 导出层 | `ExportGraphNodesAndLinks` 明确不把 `FunctionEntry / FunctionResult` 导出为节点，只写入 `NodeToIdMap` 的 `__function_entry__` / `__function_result__`。证据：`BlueprintTextConverter.cpp:939-946`。 | 当前真实导出 JSON 不满足 builder 的 FunctionEntry 前提。 |
| FullBlueprint 导出 | `ExportBlueprintToJsonObject` 确实枚举 `Blueprint->FunctionGraphs` 并导出 graph body。证据：`BlueprintTextConverter.cpp:1455-1473`。 | function graph body 会进入 `graphs[]`，但入口节点不在 `nodes[]`。 |
| 图级 workaround | `ExportService` 在 `SingleGraph` 下调用 `ResolveGraph` + `ConvertGraphToJsonObject`；`ResolveGraph::FindGraph` 会列入 `Blueprint->FunctionGraphs`。证据：`BlueprintHelperExportService.cpp:52-58`, `BlueprintHelperGraphResolver.cpp:182-197`。 | `target_type=graph` 可以直接定位同名 function graph。 |

## 现有测试为什么没有挡住

现有 TS 测试只证明 AgentFace 会把 function ReadSpec 转成正确 Bridge payload，没有证明 UE 真的能读出 function body：

- `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-handler.test.ts:138-154` 校验的是 command/payload 字段。

现有 UE `BuildTargetEntry` 测试使用手写 raw JSON，里面人为包含了 `K2Node_FunctionEntry`：

- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/BlueprintHelperObjectFirstLogicTests.cpp:97-113` 构造了带 FunctionEntry 的 fixture。
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/BlueprintHelperObjectFirstLogicTests.cpp:531-553` 断言该 fixture 能读出 function graph。

这个测试覆盖的是 builder 理想输入，不覆盖当前 exporter 的真实输出。真实 exporter 在 `BlueprintTextConverter.cpp:939-946` 隐藏 FunctionEntry，因此测试与运行时输入形态脱节。

## 问题分级

### P1: function-target logic read 运行时能力缺口

`target_type=function` 当前是官方模板和 schema 暗示的主入口，但 UE 运行时导出和 formatter 的契约不一致，导致 function body 可能为空。这里的“空读”不是 Bridge 请求失败：`logic_json` 会得到空/缺 entry 的 payload，`logic_md` 会走到 `Entry: <missing>` / `Nodes: None` 这类缺入口结果。这会让普通 Agent 误以为没有任何 read 方法能读出 function graph 内逻辑。

修复状态：

1. 已选择方案 2：在 `BuildTargetEntry` 的 `TargetFunction` 分支中，当 graph 名与 function 名匹配但 `nodes[]` 没有 FunctionEntry 时，合成 function entry/result boundary metadata 并返回该 graph 的 body nodes。
2. 合成的 entry 使用稳定 `node_ref=__function_entry__` 和 `node_path=$.graphs[<FunctionName>].__function_entry__`；如 graph-level links 引用了 `__function_result__`，则合成 return boundary node。二者只存在于非 importable 的 logic read payload 中。
3. `AttachGraphLevelLinksToNodes` 已识别 synthetic boundary node ref，使 `__function_entry__ -> body -> __function_result__` 这类真实 exporter 边界链路可以进入 LogicJson/LogicMd。
4. 当 function name 不匹配时不会合成 entry，也不会返回无关 graph body nodes。
5. 已保留显式 `K2Node_FunctionEntry` fixture 的旧行为，避免破坏已有 raw JSON 输入。

验证记录：

1. RED：`Automation RunTests BlueprintHelper.ObjectFirst.Logic.FunctionTargetUsesExportedFunctionGraphWithoutEntry` 失败，报告 `Payload.Graph=""`、`Entry` 未设置、`Nodes=0`。
2. BOUNDARY RED：加强 fixture 后，`Automation RunTests BlueprintHelper.ObjectFirst.Logic.FunctionTargetUsesExportedFunctionGraphWithoutEntry` 失败，报告只返回 1 个 body node，缺少 synthetic entry/result boundary nodes。
3. BOUNDARY GREEN：`Automation RunTests BlueprintHelper.ObjectFirst.Logic.FunctionTargetUsesExportedFunctionGraphWithoutEntry` 通过，1 succeeded / 0 failed。
4. FORMATTER GREEN：`Automation RunTests BlueprintHelper.Read.LogicSnapshotFormatter` 通过，2 succeeded / 0 failed，覆盖 `logic_json` nodes 与 `logic_md` boundary execution 文本。
5. REGRESSION：`Automation RunTests BlueprintHelper.ObjectFirst.Logic.FunctionTarget` 通过，3 succeeded / 0 failed；`Automation RunTests BlueprintHelper.ObjectFirst.Logic` 通过，13 succeeded / 0 failed。
6. LOGIC_FLOW GREEN：`AgentFaceService/task-core` 中新增 FunctionGraph synthetic boundary 覆盖；`npm run build` 成功，`node -e "await import('./build/tool-surface/bridge/read-context/read-context-handler.test.js')"` 通过 5 tests / 0 failed，`npm run test:node` 通过 282 tests / 0 failed。

### P1: guide/template 没有给出 function 级 full read 主路径

`read_context_function_logic_flow_template.json` 和 `read_context_function_logic_md_template.json` 存在，但没有明确 function 级 `logic_json` full read 模板。普通 Agent 如果只跟模板走，会在 `logic_flow` / `logic_md` 之间反复试，而不会自然落到能观察完整 nodes/links 的格式。

修复方向：

- 增加 `read_context_function_logic_json_template.json`。
- 在 `SEMANTIC_INDEX.md` 和 `04_Tool_Surface_Field_Templates.md` 中明确：function body 完整读取应使用 `blueprint_logic + target_type=function + view.format=logic_json`；不要引导普通 Agent 用 `target_type=graph` 代替 function read。

### P2: `view.max_items` 是伪截断开关

schema 和 guide 暗示 `view.max_items` 可作为 truncation guard，但 `executeReadContext` 当前没有把它传给 UE，也没有在 post-process 中消费。真正会裁剪输出的是 UE Bridge 侧 `tool_clusters.read_context.max_output_rows/max_output_bytes`，且默认是无限制。

相关证据：

- `read-context-schemas.ts` 定义 `view.max_items`。
- `read-context-handler.ts` 构建逻辑 payload 时只使用 format/target/timing。
- `BlueprintHelperToolClusterConfigResolver.cpp` 才负责 Bridge 侧输出裁剪。

修复方向：

- 要么删除/降级 `view.max_items` 文档承诺；
- 要么把 max_items 纳入 payload 和 formatter 逻辑，并让外层 `ReadContextPack.v1.truncated` 真实反映裁剪状态。

## 建议处理顺序

1. 已补测试复现 `target_type=function` 在真实导出形态下读空，并完成 `BuildTargetEntry` 修复。
2. 后续补 function `logic_json` 模板和 agent-guide 说明。
3. 单独处理 `view.max_items` 与 truncation 状态上浮，不要和 function graph 读取修复混在一起。
