# BlueprintHelper CallFunction 大范围能力测试记录

日期：2026-05-16
运行批次：CallFunctionLargeScope_20260516_185927
目标资产：/Game/BlueprintHelperCliSmoke/CallFunctionLargeScope_20260516_185927/BP_CF_LargeScopeActor_20260516_185927
关联设计文档：[BlueprintHelper_CallFunction_UEActionResolver_Design_20260516_CN.md](BlueprintHelper_CallFunction_UEActionResolver_Design_20260516_CN.md)

## 0. 2026-05-16 struct/operator 修复复测结论

状态：通过。针对上一轮阻塞项重新构造干净 Actor 蓝图 $assetPath，批次目录 $runDir。

1. P6 make_struct(/Script/CoreUObject.Vector) -> /Script/Engine.Actor:K2_SetActorLocation.NewLocation 已通过，CLI 状态 executed。
2. P7 compare(int > int) -> branch.condition 已通过，CLI 状态 executed。
3. PSEL select(compare(int == int)) -> PrintString.InString 已通过，CLI 状态 executed。
4. 编译闭环已完成：Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload 通过。
5. 实现判断：原生结构构造已从单点 Vector 特判提升为基于 HasNativeMake/expected return type 的通用 struct construction resolver；typed operator 不再依赖 UK2Node_PromotableOperator 的交互式 wildcard promotion，而是解析到具体 UE 运算函数后生成稳定 UK2Node_CallFunction 节点。

距离期望差距：本轮阻塞项已清零。Material Graph、AnimGraph、完整 UE 右键菜单排序等仍属于原文档边界内容，不在本轮支持声明内。
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
| P3 | WorldContext 静态库函数 | PrintString 不应被 world context 误挡 | 与 P2 同测 | 通过：与 P2 同测，PrintString WorldContext 未被误挡 |
| P4 | 非 literal expression typed data edge | select(int) -> PrintString.InString execute 成功，允许 schema conversion | append graph + select + call | 通过：executed |
| P5 | 显式 target_object 成员函数 | SmokeMesh.SetVisibility(NewVisibility=false) execute 成功且 Target pin 真实连接 | append graph + target_object call | 通过：executed |
| P6 | owner-qualified native function | /Script/Engine.Actor:K2_SetActorLocation execute 成功 | append graph + make_struct(Vector) | 已修复并通过：StructOperatorFix_20260516_193128 P6 executed |
| P7 | compare + branch exec 编排 | compare(int > int) 可稳定连接 branch.condition 并编译通过 | append graph + branch | 已修复并通过：StructOperatorFix_20260516_193128 P7 executed |
| P8 | get_property + compare + branch | 未通过：execute_failed；Blueprint compile failed for /Game/BlueprintHelperCliSmoke/CallFunctionLargeScope_Continuation_20260516_191014/BP_CF_LargeScopeContinuation_20260516_191014 with 3 error(s). actual=## Compiler Results  - `error`: 在“ 整数>整数 ”上无法找到从“通配符”到“整数”的正确提升方法 - `error`: 在“ 整数>整数 ”上无法找到从“通配符”到“整数”的正确提升方法 - `error`: ' 整数>整数 ' could not successfuly expand pins! - `warning`: 该结构不能使用泛型“break”节点 Break Vector 进行中断。如可能，请尝试使用专用的“break”函数。  | append graph + get_property | 未通过：skipped；Skipped because previous execute failed and could pollute compile state. |
| P9 | candidate_functions 结构化失败返回 | 错误参数名 preview_blocked，返回 candidate/mismatch reason | preview only | 通过：preview_blocked |

## 3. 边界/不确定内容

| 编号 | 能力 | 期望处理 | 测试方式 | 现实结果 |
| --- | --- | --- | --- | --- |
| B1 | Break Vector 无输入上下文 | 不应盲选低置信候选；可 blocked 并返回候选，或在唯一时 preview 通过 | preview only | 通过：preview_passed |
| B2 | Array wildcard/generic 查询 | 应暴露 wildcard metadata；不要求 execute | preview only | 通过：preview_blocked |
| B3 | 缺失 target_object | 通过：preview_blocked（continuation asset 隔离复测） | preview only | 未通过：bridge_unavailable； |
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

| 19:11:01 | continuation 覆盖测试完成 | completed | P7/P8/B3 隔离复测完成；summary: D:\UEProjects\Template\Saved\BlueprintHelper\CodexRuns\CallFunctionLargeScope_20260516_185927\continuation_20260516_191014\continuation_summary.json |

## 6. 最终结论

本轮大范围测试完成。现实能力结论如下：

1. 已通过：fresh Actor Blueprint 创建、组件添加、PrintString literal、PrintString WorldContext、select(int) 到 PrintString 的 schema conversion、SmokeMesh target_object 成员函数、double compare + branch、get_property + compare + branch、错误参数 candidate/mismatch preview、Array wildcard metadata preview、Missing target_object preview 阻断。
2. 边界通过：Break Vector 无输入上下文 preview 当前可通过，说明 resolver 可以找到可用候选；后续若要严格避免低置信盲选，应增加更强的 ambiguity policy 压力测试。
3. 部分通过：compare + branch 的 double 路径通过，但 int literal 路径失败，编译器报 wildcard 到 int promotion 失败。
4. 未通过：generic `make_struct(/Script/CoreUObject.Vector)` 到 `K2_SetActorLocation.NewLocation` 失败，编译器报 `结构 Make Vector 并非蓝图类型`。
5. 未执行且仍不声明支持：Material Graph / AnimGraph、完整 UE 菜单排序等价、pin drag 交互上下文完全复刻、为单个函数增加局部补丁。

未通过或需要复核项：
- P6 Actor K2_SetActorLocation + make_struct：execute_failed，编译器返回“结构 Make Vector 并非蓝图类型”。这表明当前 generic make_struct 对原生 Vector 的路径仍不稳，现实能力低于文档中 P6 预期。
- P7 int compare + branch：execute_failed，编译器返回“无法找到从通配符到整数的正确提升方法”。double compare + branch 已隔离通过，问题集中在 int literal compare 的 wildcard promotion。

现实能力判断：当前 CallFunction/Typed Edge 主架构符合通用性优先原则，K2 Blueprint 常规函数调用、target_object、typed edge、schema conversion、double compare、get_property 主路径可用。剩余差距集中在 GraphStatement builder 的原生结构构造和 int compare wildcard promotion，不应通过 Break/Make Vector 或某个 compare 函数名单点补丁解决，应通过通用 struct construction resolver、typed operator promotion 或 K2 schema/NodeSpawner 能力补齐。

### 6.1 continuation2 隔离诊断

- P7D double compare + branch：executed，ok
- P8I isolated get_property + compare + branch：executed，ok

诊断结论：
1. P7D 通过而 P7 失败，问题集中在 int compare wildcard promotion。
2. P8I 通过，说明 get_property 主路径在隔离状态下可用；第一次 P8 失败由 P7 无效图污染编译状态。

## 7. 阻塞内容

1. P6 已达预期：`make_struct(/Script/CoreUObject.Vector)` 已通过通用 struct construction resolver 生成真实 Make 函数调用并编译通过。
2. P7 已达预期：`compare(int > int)` 已通过 typed operator promotion 生成稳定 typed call function 节点并编译通过。