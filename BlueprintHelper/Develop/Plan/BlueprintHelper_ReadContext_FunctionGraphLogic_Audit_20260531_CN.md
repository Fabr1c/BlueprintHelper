# BlueprintHelper ReadContext FunctionGraph Logic 读取审计

日期：2026-05-31

## 结论

当前 `blueprinthelper_read_context` 的 TypeScript / CLI 层确实暴露了 `target_type=function` 的用法，但当前 UE 侧 `logic_json` / `logic_md` function-target 路径存在高可信能力缺口：function graph 的真实导出 JSON 不包含 `UK2Node_FunctionEntry` 节点，而 target-function formatter 又必须在导出 JSON 的 `nodes[]` 中找到 FunctionEntry 才会构建 function body payload。二者组合会让普通 Agent 使用官方 function ReadSpec 模板时读到空 logic，或者只看到缺失入口的结果。

这不是普通 Agent 反复换读法能稳定绕过的问题。短期可以用 `target_type=graph` + `target_name=<FunctionGraphName>` 读取同名 function graph 的 `logic_json`；长期应修 UE read pipeline，使 function target 不依赖被导出层隐藏的 FunctionEntry 节点。

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

当前更可靠的临时读取方式是图级读取 function graph：

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "/Game/Blueprints/BP_YourBlueprint",
    "target_type": "graph",
    "target_name": "YourFunction"
  },
  "view": {
    "format": "logic_json"
  }
}
```

注意：这个 workaround 读取的是 graph-scope grouped body，不等同于修复 function-target 语义；它不会恢复 function entry 作为稳定入口语义，也不会让被 exporter 隐藏的 `FunctionEntry` / `FunctionResult` 重新出现在 `nodes[]` 中。

## 证据链

| 层级 | 当前证据 | 判断 |
| --- | --- | --- |
| AgentFace schema | `ReadContextInputSchema` 允许 `read_type=blueprint_logic`、`target_type=function`、`view.format=logic_flow/logic_md/logic_json`。证据：`AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-schemas.ts`。 | TS schema 没有阻止 function logic read。 |
| AgentFace payload | `buildBlueprintLogicReadPayload` 会把 function target 转成 `target_type=function`、`target_name`、`function`、`scope=target_function`。证据：`read-context-route-builder.ts:58-92`。 | TS payload 方向正确。 |
| UE snapshot | `TargetFunction` 且 `Target.Graph` 为空时，`BuildSnapshot` 选择 `FullBlueprint` 导出，然后设置 `bTargetEntryScope=true`。证据：`BlueprintHelperLogicReadSnapshotService.cpp:65-96`。 | function target 会进入 target-entry slice。 |
| UE formatter | `BuildLogicJsonData` / `BuildLogicMdData` 在 target-entry scope 下调用 `GroupBuilder.BuildTargetEntry(...)`。证据：`BlueprintHelperLogicReadSnapshotFormatter.cpp:279-285`, `338-348`。 | function body 是否出现取决于 `BuildTargetEntry`。 |
| target-entry builder | `BuildTargetEntry` 对 `TargetFunction` 要求在 graph `nodes[]` 中找到 `FunctionEntry`，找不到则返回空 payload。证据：`BlueprintHelperLogicGroupBuilder.cpp:681`, `756-809`。 | 这是空读的直接条件。 |
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

修复方向：

1. 在 read pipeline 中把 function graph 的 entry 信息作为 graph-level entry metadata 导出，并让 `BuildTargetEntry` 消费该 metadata；或
2. 在 `BuildTargetEntry` 的 `TargetFunction` 分支中，当 graph 名与 function 名匹配但 `nodes[]` 没有 FunctionEntry 时，合成 function entry 并返回该 graph 的 body nodes/links；或
3. 为 logic read 单独保留 FunctionEntry/FunctionResult 节点输出，不影响 import/write 路径。

建议优先选择 1 或 2：保持 exporter 对 FunctionEntry 的既有隐藏策略，同时让 read formatter 不再依赖隐藏节点。

必须补的测试：

1. 创建真实 `UBlueprint` function graph，添加至少一个 body 节点。
2. 调 `FBlueprintHelperLogicJsonReadService::ReadLogicJson`，Target 为 `TargetType=Function` + `Function=<Name>`，断言 `Data.Logic.Nodes.Num() > 0`，`Data.Logic.Function == <Name>`。
3. 同路径覆盖 `logic_md` 或 formatter markdown，确认不是只修 JSON。
4. 增加一个 negative case：function name 不存在时返回明确诊断或空 payload 带原因，不应静默伪成功。

### P1: guide/template 没有给出 function 级 full read 主路径

`read_context_function_logic_flow_template.json` 和 `read_context_function_logic_md_template.json` 存在，但没有明确 function 级 `logic_json` full read 模板。普通 Agent 如果只跟模板走，会在 `logic_flow` / `logic_md` 之间反复试，而不会自然落到能观察完整 nodes/links 的格式。

修复方向：

- 增加 `read_context_function_logic_json_template.json`。
- 在 `SEMANTIC_INDEX.md` 和 `04_Tool_Surface_Field_Templates.md` 中明确：function body 完整读取应使用 `blueprint_logic + target_type=function + view.format=logic_json`；在代码修复前，临时 workaround 是 `target_type=graph + target_name=<FunctionName> + logic_json`。

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

1. 先补一条真实 UE runtime 测试，复现 `target_type=function` 读空。
2. 修 `BuildTargetEntry` / read snapshot formatter 与 exporter 的契约不一致。
3. 补 function `logic_json` 模板和 agent-guide 说明。
4. 单独处理 `view.max_items` 与 truncation 状态上浮，不要和 function graph 读取修复混在一起。
