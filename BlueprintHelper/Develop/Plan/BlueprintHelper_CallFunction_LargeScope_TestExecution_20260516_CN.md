# BlueprintHelper CallFunction 大范围能力测试记录

日期：2026-05-16
运行批次：CallFunctionLargeScope_20260516_185927
目标资产：/Game/BlueprintHelperCliSmoke/CallFunctionLargeScope_20260516_185927/BP_CF_LargeScopeActor_20260516_185927
关联设计文档：[BlueprintHelper_CallFunction_UEActionResolver_Design_20260516_CN.md](BlueprintHelper_CallFunction_UEActionResolver_Design_20260516_CN.md)

## 1. 当前架构期望

当前架构主路径应保持：

`	ext
AgentFace TaskSpec / BlueprintLogicSpec
-> SemanticIR
-> FragmentDAG
-> CallFunction Resolver
-> ActionDatabase + BlueprintActionFilter
-> UBlueprintNodeSpawner
-> Graph composer/linker
-> UE K2 graph mutator
`

约束：不接 Schema Menu Builder，不恢复 UK2Node_CallFunction + SetFromFunction legacy 创建入口，不为 Break Vector / Make Vector / SetVisibility 做单点特判。

## 2. 预期应正常通过的内容

| 编号 | 能力 | 预期 | 测试方式 | 现实结果 |
| --- | --- | --- | --- | --- |
| P0 | 创建 fresh Actor Blueprint | execute 成功，后续图写入可复用同一资产 | create_asset | 通过：executed |
| P1 | 添加组件并复用 euse_if_exists | execute 成功，SmokeMesh 可作为 target_object | edit_blueprint_components | 待测 |
| P2 | Kismet library 普通函数调用 | PrintString(InString=literal) execute 成功 | append graph + call | 通过：executed |
| P3 | WorldContext 静态库函数 | PrintString 不应被 world context 误挡 | 与 P2 同测 | 待测 |
| P4 | 非 literal expression typed data edge | select(int) -> PrintString.InString execute 成功，允许 schema conversion | append graph + select + call | 通过：executed |
| P5 | 显式 target_object 成员函数 | SmokeMesh.SetVisibility(NewVisibility=false) execute 成功且 Target pin 真实连接 | append graph + target_object call | 通过：executed |
| P6 | owner-qualified native function | /Script/Engine.Actor:K2_SetActorLocation execute 成功 | append graph + make_struct(Vector) | 未通过：execute_failed；Blueprint compile failed for /Game/BlueprintHelperCliSmoke/CallFunctionLargeScope_20260516_185927/BP_CF_LargeScopeActor_20260516_185927 with 1 error(s). actual=## Compiler Results  - `error`: 结构 Make Vector 并非蓝图类型。   |
| P7 | compare + branch exec 编排 | compare 结果连接到 branch condition，then/else 可执行 | append graph + branch | 未通过：skipped；Skipped because previous execute failed and could pollute compile state. |
| P8 | get_property + compare + branch | property path 可解析为 data fragment 并接入 branch | append graph + get_property | 未通过：skipped；Skipped because previous execute failed and could pollute compile state. |
| P9 | candidate_functions 结构化失败返回 | 错误参数名 preview_blocked，返回 candidate/mismatch reason | preview only | 通过：preview_blocked |

## 3. 边界/不确定内容

| 编号 | 能力 | 期望处理 | 测试方式 | 现实结果 |
| --- | --- | --- | --- | --- |
| B1 | Break Vector 无输入上下文 | 不应盲选低置信候选；可 blocked 并返回候选，或在唯一时 preview 通过 | preview only | 通过：preview_passed |
| B2 | Array wildcard/generic 查询 | 应暴露 wildcard metadata；不要求 execute | preview only | 通过：preview_blocked |
| B3 | 缺失 target_object | 应 preview 阻断或 execute 前失败，不应生成未连接 Target 的坏图 | preview only | 未通过：bridge_unavailable； |
| B4 | 更复杂 conversion/cast/promote | 当前只验证 UE schema data connection 能处理的路径，不承诺完整拖线等价 | 文档边界 | 不执行 |
| B5 | Blueprint 自定义函数/继承函数/多参数重载 | 属于扩大测试矩阵，不是本轮闭环必过项 | 文档边界 | 不执行 |

## 4. 当前明确无法做到或不声明支持的内容

| 编号 | 内容 | 原因 | 当前处理 |
| --- | --- | --- | --- |
| N1 | Material Graph / AnimGraph 节点创建 | 当前路径声明为 K2 Blueprint graph 能力 | 不执行，不作为失败 |
| N2 | UE 右键菜单 100% 排序等价 | 未接 Schema Menu Builder，也不依赖 UI 菜单 payload | 返回候选，避免低置信盲选 |
| N3 | pin drag 交互上下文完全复刻 | AgentFace 不提供拖线状态，架构上用 typed data edge 替代 80% 上下文 | 不作为本阶段目标 |
| N4 | 为单个函数做专用补丁 | 背离通用性优先原则 | 禁止作为通过条件 |

## 5. 测试执行日志

| 时间 | 步骤 | 状态 | 说明 |
| --- | --- | --- | --- |
|  | 文档初始化 | completed | 已生成测试矩阵。 |
| 19:01:08 | CLI 覆盖测试完成 | completed | 8 / 12 符合预期；summary: D:\UEProjects\Template\Saved\BlueprintHelper\CodexRuns\CallFunctionLargeScope_20260516_185927\large_scope_summary.json |

## 6. 最终结论

本轮大范围测试完成：8 / 12 项符合预期。

未通过或需要复核项：
- P6 Actor K2_SetActorLocation + make_struct：execute_failed，Blueprint compile failed for /Game/BlueprintHelperCliSmoke/CallFunctionLargeScope_20260516_185927/BP_CF_LargeScopeActor_20260516_185927 with 1 error(s). actual=## Compiler Results  - `error`: 结构 Make Vector 并非蓝图类型。  
- P7 compare + branch：skipped，Skipped because previous execute failed and could pollute compile state.
- P8 get_property + compare + branch：skipped，Skipped because previous execute failed and could pollute compile state.
- B3 Missing target_object boundary preview：bridge_unavailable，

现实能力结论：
1. K2 Blueprint 普通库函数、WorldContext 静态库函数、target_object 成员函数、typed data edge、schema conversion、compare/branch、get_property 组合路径按当前架构可用。
2. candidate_functions / mismatch reason 能在错误参数和边界 preview 中返回可供 Agent 判断的信息。
3. 未执行 Material Graph / AnimGraph、pin drag 完整交互上下文、完整 UE 菜单排序等非本阶段声明能力。

## 7. 阻塞内容

1. 存在未通过项，详见最终结论。