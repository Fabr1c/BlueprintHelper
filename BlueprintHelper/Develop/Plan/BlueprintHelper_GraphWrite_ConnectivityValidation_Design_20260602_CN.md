# BlueprintHelper GraphWrite 连接校验设计

日期：2026-06-02

状态：设计稿，等待确认后再拆实施计划。

## 目标

在 `BlueprintHelper.TaskSpec.v1 -> TaskPlan -> UE GraphWrite` 的真实写图链路中增加连接正确性门，防止 TaskPlan 解析和 GraphWrite 执行成功后，蓝图中残留没有执行入口、没有数据消费者、或没有纳入回滚记录的孤岛节点。

这项能力不是 UI 展示优化，而是 GraphWrite execute 的正确性约束。成功返回前必须证明本轮生成的节点已经按语义接入蓝图，或在失败时已经完成即时清理。

## 当前源码事实

当前链路已有部分基础，但还没有形成连接校验门：

- TypeScript TaskSpec schema 允许 `BlueprintLogicSpec` 携带自由 `statements[]`，TaskPlan 里 GraphWrite step 可以是 `capability=graph_write` 的结构化 IR，也可以是 append/replace/patch/merge adapter step。
- 旧 adapter schema 中已有 `AgentImportLinkSchema`，区分 `exec` 和 `data` link。
- UE SemanticIR 路径会在 `BuildSemanticStatementArray` 里按 pending exec exits 串接语句，并通过 `ConnectSemanticExecPins` 建立 exec link。
- UE SemanticIR 路径会通过 `ConnectFragmentDataEdges` 建立 data link，并记录 `RequestedLinkCount` / `CreatedLinkCount`。
- `FBlueprintGraphWriteContext` 已经支持 `RegisterNode(..., bGenerated=true)` 和 `GetGeneratedNodes()`，可以作为 validator 的生成节点输入。
- 现有成功条件仍主要是 `UnresolvedNodeCount == 0 && GeneratedNodeCount > 0`，没有把“生成节点是否可达/被消费”作为阻断条件。
- `GraphWriteMutationCoordinator` 对 patch/merge intent 也会统计生成节点和连接数，但成功条件仍是 intent 成功，不校验结果图中的 generated node 连接状态。

## 关联文档与继承结论

本设计不重复既有架构文档，只继承以下结论：

- `BlueprintHelper/Develop/Plan/BlueprintHelper_TaskSpec_To_UEExecution_ArchitectureFlow_20260527_CN.md`：主线必须保持 `TaskSpec -> canonical TS compiler -> TaskPlan -> UE TaskRuntime -> GraphWrite cluster -> Review/Compile/Save/Journal`，不恢复 raw payload 旁路。
- `BlueprintHelper/Develop/Gap/BlueprintHelper_TaskSpec_UnconnectedToolClusters_SourceAudit_20260601_CN.md`：所有 agent-facing 写入能力应统一进入 canonical compiler + TaskPlan 路径，连接校验也应服务这个主线，而不是新增单工具特殊入口。
- `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_CapabilityContract_20260525_CN.md`：GraphWrite 能力需要以契约、preview、execute、readback、测试状态一体收敛；连接校验的最终验收也必须包含 readback。
- `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_TSConnect_UEDoneCapabilities_ImplementationPlan_20260526_CN.md`：TS 语义与 UE SemanticIR 语义要一一映射。validator 不能只相信 TS link 计数，必须以 UE runtime 真实图为准。
- `BlueprintHelper/Develop/Plan/BlueprintHelper_ReviewV2_ComponentLifecycleRejectCascade_ImplementationPlan_20260601_CN.md`：Reject 仍以 Review v2 evidence-before-snapshot 为基线；连接失败清理和 Reject 清理都不能绕开 Review 数据模型。
- `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_ExternalUserAuthoredGraph_MasterImplementationPlan_20260531_CN.md`：外部用户图只通过显式 external anchor / external mutation policy 进入，validator 只校验本轮外部边界，不接管旧用户孤岛。

## 问题定义

缺少连接校验会导致三类错误被误判为成功：

1. Exec 节点已经生成，但没有从入口或上一个 exec 节点连入，实际运行永远不会触发。
2. PureData 节点已经生成，但没有任何输出数据消费者，实际图中只是死数据。
3. 生成节点或生成 link 没有完整进入 mutation batch / review evidence，后续 Reject 只回滚了可见或已连接的部分，孤岛节点残留。

因此校验必须同时覆盖：

- 语义连接是否成立。
- 真实 UE graph 中连接是否落地。
- 本轮生成对象是否完整进入 ownership / rollback 记录。

## 术语

`GWS`：本文把一个 GraphWrite TaskPlan step 内的一次可落地写图单元称为 GraphWriteStep。实际实现中可能对应 `write.ops[]`、SemanticIR body、append/replace service 调用、或 mutation intent batch。

`GeneratedNode`：本轮 GWS 创建或接管为本轮所有的节点。只校验 generated/owned 节点，不扫描并阻断整个蓝图里的旧孤岛。

`EntryRoot`：事件入口、函数入口、CustomEvent 入口、构造入口，或外部 merge 的稳定边界入口。它可以没有 incoming exec。

`ExecNode`：拥有 ExecPin 的执行节点。非 EntryRoot 的 ExecNode 必须有 incoming exec link。

`ExecTerminal`：Return、Destroy、Print 等可以没有 outgoing exec 的执行终点。它仍必须有 incoming exec，除非它本身也是 EntryRoot。

`PureData`：除 `Comment` / `Reroute` 白名单外，所有没有 ExecPin 的 GWS generated node 都归入 PureData。它必须至少有一个 outgoing data consumer，并且最终被可达执行链消费。仅“有输入”不证明有意义，因为没有消费者的无 ExecPin 节点不会影响执行结果。

`Comment` / `Reroute`：注释节点和 reroute 节点是连接校验白名单。它们不需要 exec/data 连接，也不需要被后续节点消费；但如果它们由本轮 GWS 生成，仍必须进入 generated object / ownership / rollback 记录。当前设计不预期 GWS 生成 reroute，但白名单规则先显式保留。

`Structural`：不再作为笼统白名单。除 `Comment` / `Reroute` 外，显式占位节点如果作为 GraphWrite generated node 出现在 GWS 中且没有 ExecPin，仍按 PureData 校验；若它们确实是独立结构语义，应在 GraphWrite body 之外由对应能力表达。

`Layout`：不是 Blueprint node 类型，也不是 GWS generated node 分类。Layout 只表示节点位置、尺寸、排列、注释框展示覆盖等位置/展示元数据，例如 `NodePosX` / `NodePosY`。单纯位置或布局变更应路由到 GraphLayout / position mutation / layout record，不进入 GraphWrite body 连接校验。如果某个请求在 GWS 中生成所谓 `layout` 节点，说明 TaskSpec lowering 或 GraphWrite 表达层分类错误，应在进入连接校验前修正。

## 设计决策

### 1. 校验放在 UE GraphWrite runtime，不只放在 TS compiler

TS compiler 可以做静态预检，但最终阻断必须在 UE runtime。原因是 UE graph 的真实 pin、schema 拒绝、wildcard 重建、pure/impure cast、ActionResolution 选择结果，都只有 UE runtime 才知道。

推荐新增可复用边界：

```text
FBlueprintHelperGraphWriteConnectivityValidator
```

它应位于 GraphWrite runtime / pipeline 边界，而不是 UI、CLI、单个工具函数或某个特殊 service 分支。

### 2. 校验输入以本轮生成对象为中心

Validator 输入应包含：

- target graph。
- 本轮 generated fragments / generated nodes。
- 本轮 expected data edges / expected exec edges。
- 本轮实际 created link 计数和 connection diagnostics。
- GWS operation context：append、replace、merge、patch、external merge、external replace。
- ownership scope：`blueprinthelper_owned` 或 `external_user_authored`。

不要要求 validator 理解完整 TaskSpec 原始 JSON。它消费 runtime 已经解析出的图对象和 fragment/intent 结果。

### 3. 阻断规则

硬阻断：

- `missing_expected_link`：预期 link 数大于实际 created link 数，或关键 expected edge 未落地。
- `unreachable_exec_node`：非 EntryRoot 的 generated ExecNode 没有 incoming exec link。
- `unconsumed_pure_data_node`：没有 ExecPin 且不属于 `Comment` / `Reroute` 白名单的 generated PureData 没有 outgoing data link。
- `unreachable_pure_data_chain`：PureData 虽有输出，但最终没有被任何可达 ExecNode 消费。
- `unregistered_generated_node`：生成节点没有进入本轮 ownership / rollback 记录。

允许例外：

- EntryRoot 可以没有 incoming exec。
- ExecTerminal 可以没有 outgoing exec。
- `Comment` / `Reroute` 节点不要求 exec/data 连接，也不要求被消费。
- Structural 不作为 GWS 笼统连接白名单；除 `Comment` / `Reroute` 外，进入 GWS generated node 集合后同样按 ExecPin 分类，没有 ExecPin 就是 PureData。
- Layout 不进入 GWS node 分类；只改变位置/展示布局时，应由 GraphLayout / position mutation 路径处理。
- 外部图写入只校验本轮插入/改动边界，不要求清理用户已有孤岛。
- 空入口或空函数体如果是用户明确创建声明，应作为 warning 或单独 signature 能力，不应误归为 GraphWrite body 成功。

### 4. 没有 ExecPin 的节点必须按 PureData 消费规则判断

除 `Comment` / `Reroute` 白名单外，所有没有 ExecPin 的 GWS generated node 都视为 PureData。它们的输入只能说明依赖了其他数据，不能说明它们对蓝图有用。PureData 的必要条件是有 outgoing data consumer。

更严格的推荐条件是：

```text
PureData -> ... -> 可达 ExecNode 的输入 pin
```

如果 PureData 只连接到另一个 PureData，而最终没有进入可执行节点，它仍然是死数据。

这条规则避免按普通节点类型名称做例外判断：函数纯节点、变量 getter、Map Find、Select、Construct/Deconstruct、纯 Cast、纯 Convert、临时 result_symbol producer，只要没有 ExecPin 且不是 `Comment` / `Reroute`，都必须被后续节点消费。

### 5. Exec 必须按 exec link 可达判断

Exec 节点不能只看“有任意输入 link”。Data link 不代表执行流可达。非入口 ExecNode 必须有 incoming exec link。

对于 expression context 中误生成的 impure cast，如果它有 exec pin 却没有 exec path，应归入 `unreachable_exec_node` 或 `invalid_expression_exec_node`。

### 6. Preview 和 Execute 都要校验

Preview：

- 在 dry-run / synthetic graph 上运行 validator。
- 返回 `preview_blocked`，附带简短 violation code。
- debug artifact 记录完整 generated node 和 link 诊断。

Execute：

- 在真实 UE graph mutation 后、compile/save/review success 前运行 validator。
- validator 失败时立即 rollback / cleanup 本轮 generated nodes 和 generated links。
- 返回 `graphwrite_connectivity_failed`，不能返回 `completed`。

### 7. Reject 残留需要独立的 ownership 保障

连接校验能防止坏图被当作成功，但不能替代回滚登记。

成功路径要求：

- 每个 generated node 必须登记到 Review evidence / mutation ownership。
- 每个 generated link 或边界变更必须登记到对应 target。
- Review v2 / DebugBundle / UI overlay / Reject 消费同一套模型。

失败路径要求：

- 只要 execute 创建过节点或 link，失败返回前就必须即时清理。
- 不能把失败清理留给用户后续 Reject。

## 推荐执行链路

```text
TaskSpec
  -> TS compile static preflight
  -> TaskPlan
  -> UE preview_task_plan
  -> GraphWrite service builds fragments/intents
  -> ConnectivityValidator on preview graph
  -> preview passed / blocked
  -> UE execute_task_plan
  -> GraphWrite service mutates real graph
  -> ConnectivityValidator on real graph
  -> ownership completeness validation
  -> Review evidence / compile / save / journal
```

## 输出契约

普通 CLI 输出只需要暴露简短阻断信息：

```json
{
  "ok": false,
  "status": "blocked",
  "error_code": "graphwrite_connectivity_failed",
  "violations": [
    {
      "code": "unconsumed_pure_data_node",
      "node_id": "stmt_find_score",
      "message": "Generated PureData node has no outgoing data consumer."
    }
  ]
}
```

完整节点、pin、link 细节只进入 debug artifact 或 expert 输出，避免再次增加普通 Agent 输出负担。

## 失败码建议

| Code | 含义 |
| --- | --- |
| `missing_expected_link` | 预期连接未全部落地 |
| `unreachable_exec_node` | 非入口执行节点没有 incoming exec |
| `unconsumed_pure_data_node` | 非 Comment/Reroute 的无 ExecPin 节点没有 outgoing data consumer |
| `unreachable_pure_data_chain` | 纯数据链最终没有被可达执行链消费 |
| `invalid_expression_exec_node` | 表达式上下文生成了需要 exec 的节点 |
| `unregistered_generated_node` | 生成节点未进入 ownership / rollback 记录 |
| `unregistered_generated_link` | 生成连接未进入 evidence / rollback 记录 |
| `external_boundary_not_connected` | 外部 merge 边界未按策略接上 |

## 测试策略

### TS / task-core

- 编译器静态预检：明显无消费者的 pure expression body 应被拒绝或进入 preview-blocked path。
- TaskPlan fixture：GraphWrite step 应保留足够的 body/statement 信息供 UE runtime 校验，不为了 validator 改成 Agent 手写低层 link。
- Node tests 继续串行执行 build 后 test，避免旧 build 产物误导。

### UE automation

至少覆盖：

1. 非入口 ExecNode 无 incoming exec，应失败并清理。
2. 非 Comment/Reroute 的无 ExecPin 节点无 outgoing data，应失败并清理。
3. 非 Comment/Reroute 的无 ExecPin 节点有输出并被可达 ExecNode 消费，应通过。
4. EntryRoot 无 incoming exec 但 body 接通，应通过。
5. ExecTerminal 无 outgoing exec 但有 incoming exec，应通过。
6. expression context 误生成 impure cast，应失败或强制 pure 后通过。
7. Comment / Reroute 节点即使没有 exec/data 连接，也应通过连接校验，但仍进入 ownership/rollback 记录。
8. Layout / position-only 操作不应生成 GWS node；只移动节点或更新展示布局时，应通过 GraphLayout / position mutation 路径验证，而不是进入连接校验。
9. external merge 只校验本轮 external boundary，不因为旧用户孤岛失败。
10. Reject 后本轮 generated nodes / links 全部消失。

### 真实 E2E

用 CLI 执行一个真实 TaskSpec：

- 创建测试蓝图。
- 写入包含 Entry、Branch、无 ExecPin 数据节点、Exec call 的 graph body。
- read_context / logic_flow 验证执行链和数据链。
- Reject。
- read_context 验证没有本轮 generated node 残留。

## 性能约束

Validator 不做全图昂贵扫描。它只遍历：

- 本轮 generated nodes。
- generated nodes 的 pins。
- 与 generated pins 相邻的 linked pins。
- 必要时从 EntryRoot 沿 exec link 做 bounded reachability。

复杂度应接近：

```text
O(generated_node_count + adjacent_link_count)
```

不要在每个节点上触发 ActionDatabase 查询、完整蓝图序列化或全图 layout 计算。

## 后续实施拆分建议

1. P0：新增设计测试和失败用例，不改 runtime 行为。
2. P1：新增 `FBlueprintHelperGraphWriteConnectivityValidator` 数据结构、分类器和只读诊断。
3. P2：接入 SemanticIR append/replace preview + execute，失败时阻断。
4. P3：接入 patch/merge mutation coordinator，覆盖 external boundary。
5. P4：补齐 ownership completeness validation，确保 Review/Reject 不漏本轮 generated 对象。
6. P5：CLI 普通输出减重，只暴露简短 violations，完整细节进 debug artifact。
7. P6：真实 E2E，验证 execute failure cleanup 和 success Reject cleanup。

## 非目标

- 不把 TaskSpec 改成低层手写 `nodes/links` 入口。
- 不开放任意用户节点接管。
- 不为 RestoreDevice 或某个具体蓝图写特殊分支。
- 不让 UI 层做连接判断。
- 不把旧 legacy GraphWrite raw import 路径重新变成主线。

## 自审结论

本文档聚焦连接正确性门，没有进入代码实现细节。设计边界保持在 GraphWrite runtime / validator / ownership 模型中，符合当前架构硬规则。后续如果进入实现，应先写红灯用例，再分阶段接入 validator，并用真实 E2E 验证 Reject 不残留。
