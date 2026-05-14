# BlueprintHelper 图语句框架实现进度记录

日期：2026-05-13

关联设计文档：`BlueprintHelper_GraphStatementFramework_Design_20260513_CN.md`

## 当前覆盖测试结论：2026-05-14 编辑器重启后 SemanticIR smoke

状态：最小端到端 smoke 通过；未覆盖全部 statement 种类。
已通过部分：
1. `bh.cmd` 连接重启后的 Editor/Bridge 成功，`blueprint_get_runtime_profile` 与 `blueprinthelper_diagnostics_runtime` 返回 completed。
2. `blueprinthelper_preview_task` 使用 `BlueprintLogicSpec.v2`、短名 `call`、`target: PrintString` 通过 preview，产物为 `preview_1778743995346_0001`。
3. `blueprinthelper_execute_task` 成功写入 `/Game/BlueprintHelperCliSmoke/BH_PhysicsDoor_20260513/BP_BH_PhysicsDoorActor`，任务为 `task_3C29F27442081416763C7B997D18880D`。
4. 写入结果创建图 `BH_SemanticIR_CodexSmoke_20260514_001` 和 custom event `BH_CodexSemanticIRSmoke_20260514_001`，GraphWrite 状态为 applied。
5. 执行产物包含 `fragment_debug.fragment_dag`、`data_edges`、`fragment_evidence`，说明 SemanticIR/fragment evidence 已进入真实写入结果。
6. 执行后的 post compile 返回 `success: true`、`status: succeeded`、`warning_count: 0`。
7. `blueprinthelper_read_task_context` 回读目标资产，确认新增图 `BH_SemanticIR_CodexSmoke_20260514_001` 已存在，节点数为 2。

本次修复：
1. 修复 `append_new_owned_graph` 在真实 execute 阶段错误要求新图 custom event 预先存在的问题；现在只有目标图已存在且启用 `reuse_existing_entries` 时才检查已有 custom event。
2. 修复后已重新编译 `TemplateEditor Win64 Development`，结果成功。

距离期望的差距：
1. 本次只验证了最小 `call` statement 的真实写入链路，尚未覆盖 `let/compare/branch/select/make_struct/get_property` 的端到端 UE 写入。
2. 执行结果仍显示 `adapter_operation: append_blueprint_graph`，虽然 fragment_debug/evidence 已随结果输出，但仍需继续确认 SemanticIR 是否已成为唯一 UE 节点创建入口。
3. Semantic resolver 对 `PrintString` 仍给出 `semantic.target_unverified` warning；UE 节点写入和编译通过，但 resolver 目标验证还不完整。
4. 低层 `append_blueprint_graph` 不是当前普通 CLI 暴露命令，本次无法通过普通 CLI 直接验证旧 adapter payload 的拒绝路径。
5. 热重载后 write session 会丢失，需要重新执行 `blueprinthelper_request_write_session`；这是测试流程约束，不属于本次 GraphStatement 主链路通过证明。
## 当前进度总览：2026-05-14 SemanticIR 主路径化同步

### 阶段状态调整
1. SemanticIR -> fragment DAG 的 data edge emission 已进入 GraphGenerationPipeline 主执行路径：payload 中存在 `logic_spec` 时，会现场构建 SemanticIR 和 FragmentDag，并在节点生成后通过 GraphLinker/GraphComposer 连接可落地 data edge。
2. AgentFace TypeScript/Python 编译链已为 `logic_spec` statement/expression 注入与 `nodes/links` 一致的稳定 `id`，用于 DAG fragment 与真实 UE node fragment 对齐。
3. `get_property` resolver 已补充无类型 fallback：当 `TargetStructs` 未命中时，会基于目标类型名通过 `UObjectIterator` 查找 `UStruct/UClass`，再解析 property path。
4. `compare` resolver 已在 SemanticIR 层稳定回填 `bool` 类型，并保留 UE 侧复杂类型解析由现有 PromotableOperator/函数解析兜底。
5. fragment data edge 消费已收敛为 `statement tree -> fragment DAG -> composer/linker -> UE pin link` 的主数据连线路径；节点创建仍保留 `nodes/links` 兼容承载。

### 已验证结果
1. UE 编译通过：`TemplateEditor Win64 Development`，`Result: Succeeded`。
2. AgentFace TypeScript build 通过：`npm.cmd --prefix .\AgentFaceService\task-core run build`。
3. AgentFace Python compileall 通过：`python -m compileall -q .\AgentFaceService\task-core\python`。

### 距离期望的差距
1. SemanticIR 尚未成为唯一 UE 节点创建入口；当前节点实例化仍由 `nodes/links` payload 承载，DAG 主路径化主要完成 data edge emission/consumption。
2. exec edge 仍主要由现有 explicit links 与 linear composer 兼容路径处理，尚未完整切换到 `FragmentDag.ExecEdges`。
3. 复杂 `compare` 的最终能力仍受 UE 可解析函数/PromotableOperator 支持范围限制，自定义结构体/容器比较还没有通用签名匹配策略。
4. 本轮只做编译级验证，未运行 TS/Python 单元测试，也未执行真实编辑器写入场景回归。

## 当前进度总览（2026-05-14 同步）

### 阶段完成状态
1. 阶段 1：branch、let、compare、select、make_struct 已进入 AgentFace schema / TypeScript 编译器 / Python 编译器；旧字段仍仅作为兼容路径保留，尚未完成按短名进度逐步移除。
2. 阶段 1.5：IR builder 已接入 AgentFace TypeScript / Python 编译链与 UE append / replace / merge / patch preflight 路径；semantic resolver、symbol table、expression data fragment dispatch 已完成当前可编译闭环。
3. 阶段 2：statement tree -> fragment DAG、typed pin / layout model、branch/get/let/compare/select/make_struct/get_property fragment builder、fragment DebugBundle / Review evidence 字段已完成当前可编译闭环。
4. 本轮阻塞项：get_property 真实 property access builder、compare operator alias / type inference、非 literal expression data edge 到 UE pin link 消费、TS/Python 单元测试均已完成并验证通过。

### 已验证结果
1. UE 编译通过：TemplateEditor Win64 Development，Result: Succeeded。
2. AgentFace TaskCore 全量测试通过：npm.cmd --prefix .\\AgentFaceService\\task-core run test。
3. Node 测试通过：99 tests passed。
4. Python unittest 通过：48 tests OK。

### 距离期望的差距
1. SemanticIR 到 UE 节点写入路径尚未完全成为唯一主路径；当前仍保留 nodes / links 输出路径，fragment DAG data_edges 已可被 pipeline 消费，但 SemanticIR -> NodeFragment -> UE Graph Mutator 还没有完全主路径化。
2. get_property 依赖 UE pin type 的 PinSubCategoryObject；当上游表达式缺少可推断 struct / object 类型时，会失败并返回诊断，尚未实现无类型上下文下的反射兜底 resolver。
3. compare 已覆盖常见短操作符与基础类型候选推断，但复杂容器、枚举、自定义结构、对象比较仍依赖 UE 函数解析是否命中，尚未形成完整 typed compare resolver。
4. fragment data edge 消费当前是兼容式接入；最终仍需要收敛为统一的 statement tree -> fragment DAG -> composer/linker -> UE mutator 编排，而不是长期并行维护 nodes / links 与 fragment DAG 两条主路径。
5. Review evidence 已具备 fragment 字段承载能力，但按 function / event / macro 图体聚合后的实际 UI 审核体验仍需要真实蓝图写入场景验证。
6. Layout model 目前主要用于 debug/evidence 与 fragment 描述，尚未完整驱动实际 UE 节点排布。
7. Pattern Registry 的 JSON 数据驱动 alias / pin mapping / 类型转换扩展面还没有完全固化为稳定配置契约。

### 下一优先级
1. 优先把 SemanticIR -> NodeFragment emission 主路径化。
2. 其次补无类型 get_property resolver 与复杂 compare typed resolver。
3. 再继续收敛 Review evidence UI 验证、layout 驱动和 Pattern Registry 外部配置契约。
## 目标

按设计文档推进 `AgentFace TaskSpec -> BlueprintLogicSpec -> Graph Semantic IR -> Pattern Registry -> NodeFragment -> Graph Composer -> Append / Replace / Merge / Patch Adapter -> UE Graph Mutator` 架构。

每个阶段只在完全达到该阶段目标时标记完成；未完全完成的阶段必须记录距离期望的差距。

## 阶段 0：进度文档

状态：完成

已完成内容：
1. 新建本进度文档。
2. 建立阶段边界和“未完成不可虚标完成”的记录规则。

距离期望的差距：
1. 无。

## 阶段 1：AgentFace 短名语句兼容层

状态：完成，未验证

已完成内容：
1. TypeScript TaskSpec schema 允许 `BlueprintLogicSpec.v1` 和 `BlueprintLogicSpec.v2`。
2. TypeScript 编译器支持短名 `call` 和 `set`。
3. TypeScript 编译器继续兼容旧字段 `call_function` 和 `set_member_variable`。
4. Python 编译器支持短名 `call` 和 `set`。
5. Python 编译器继续兼容旧字段 `call_function` 和 `set_member_variable`。
6. Task contract 主字段切换为 `statement_kinds: ["call", "set"]`。
7. Task contract 增加 `legacy_statement_kinds: ["call_function", "set_member_variable"]`。
8. 增加短名 `call/set` 编译到现有 append bridge payload 形状的回归用例。

距离期望的差距：
1. 尚未运行测试或构建验证。
2. `branch`、`let`、`compare`、`select`、`make_struct` 尚未进入 schema 和编译器。
3. 旧字段仅进入兼容路径，尚未完成按短名进度逐步移除。

## 阶段 1.5：Statement IR / Expression IR contract

状态：部分完成，编译通过

已完成内容：
1. 新增 `FBlueprintHelperGraphSemanticIR`，作为 BlueprintLogicSpec v2 的内部语义 IR 容器。
2. 新增 `FBlueprintHelperGraphStatementIR`，支持 `call`、`set`、`branch`、`let`、`return` 的 statement contract。
3. 新增 `FBlueprintHelperGraphExpressionIR`，支持 `literal`、`get`、`ref`、`call`、`compare`、`select`、`make_struct` 的 expression contract。
4. 新增 `FBlueprintHelperGraphSemanticIRBuilder`，可从 `BlueprintLogicSpec.v2` JSON object 构建 statement tree。
5. `branch.then` 和 `branch.else` 已在 IR 层递归保留为子 statement list。
6. `let.name`、`value`、`condition`、`args`、`left/right`、`options` 已进入 IR contract。
7. IR builder 会记录 schema、path、diagnostics，保留后续 DebugBundle 和 Review identity 基础。
8. 2026-05-14 已通过 `TemplateEditor Win64 Development` 编译。

距离期望的差距：
1. IR builder 尚未接入 AgentFace TypeScript/Python 编译链。
2. IR builder 尚未接入 UE append/replace/merge/patch 运行路径。
3. 尚未实现 semantic resolver，`target` 还没有解析为 component、variable、function、property path 等 typed target。
4. 尚未实现 symbol table，`let/ref` 还不能建立命名临时值引用。
5. 尚未实现 expression 到 data fragment 的 pattern dispatch。

## 阶段 2：NodeFragment 和 GraphStatementBuilder 基础层

状态：部分完成，编译通过

已完成内容：
1. 新增 `FBlueprintHelperNodeFragment`，用于承载一个语句生成出的主节点、相关节点、exec entry、exec exit 和诊断信息。
2. 新增 `FBlueprintHelperGraphStatementBuilder`，作为语句到节点片段的 C++ 入口。
3. 将 call function 节点生成迁移到 `BuildCallFunctionFragment`。
4. `FCallFunctionNodeHandler` 改为委托 `FBlueprintHelperGraphStatementBuilder::BuildCallFunctionFragment`。
5. 显式对象调用路径仍保留在 builder 内，包括 member getter 创建、target pin 查找和对象 target pin wiring。
6. 将 variable set 节点生成迁移到 `BuildVariableSetFragment`。
7. `FVariableSetNodeHandler` 改为委托 `FBlueprintHelperGraphStatementBuilder::BuildVariableSetFragment`。
8. variable set 本地变量 `ensure_exists` 行为仍保留在 builder 内。
9. 扩展 `FBlueprintHelperNodeFragment`，加入 `InternalLinks`、`DataInputs`、`DataOutputs`、`PinBindings`、`LayoutHints`、`OwnershipTags`、`ReviewTargets`。
10. `call` fragment 会记录 exec pin binding、statement ownership、layout hint、review target。
11. 显式对象调用 fragment 会记录 object getter 到 call target 的 internal link。
12. `set` fragment 会记录 exec pin binding、statement ownership、layout hint、review target 和变量 data input 占位。
13. 2026-05-14 已通过 `TemplateEditor Win64 Development` 编译。

距离期望的差距：
1. 还没有形成 statement tree 到 fragment DAG 的统一编排。
2. `NodeFragment` 字段已补齐基础形状，但还没有完整 typed pin model 和 layout model。
3. 还没有 `branch`、`get`、`let`、`compare`、`select`、`make_struct` 的 fragment builder。
4. Review 粒度尚未按 function、event、macro 图体聚合。
5. fragment 字段尚未完整接入 DebugBundle 和 Review evidence。

## 阶段 3：Pattern Registry 数据目录和 loader

状态：部分完成，未验证

已完成内容：
1. 新增插件内置 Pattern 数据目录 `BlueprintHelper/Resources/GraphPatterns`。
2. 新增 `call.defaults.json`，记录 `call` pattern 的 alias、pin alias 和默认值示例。
3. 新增目录说明，明确插件内置目录和项目覆盖目录的职责。
4. 新增 `FBlueprintHelperGraphPatternRegistry`。
5. Pattern Registry 会加载插件内置 `Resources/GraphPatterns/*.json`。
6. Pattern Registry 会加载项目覆盖 `Config/BlueprintHelper/GraphPatterns/*.json`。
7. Pattern Registry 支持 `aliases`、`pin_aliases`、`defaults`、`enabled`。
8. `BuildCallFunctionFragment` 会应用 `call` pattern 的 function alias、pin alias 和默认值。
9. `DoorPanel.add_angular_impulse` 这类 qualified call 会只对函数段应用 alias，保留对象段。

距离期望的差距：
1. 尚未运行编译验证。
2. 尚未实现简单类型转换。
3. 尚未实现 Pattern Registry diagnostics。
4. 尚未给 `set`、`get`、`branch`、`compare` 等 pattern 添加 binding 文件和应用点。
5. 当前默认值仅支持 JSON primitive 到字符串，复杂对象默认值尚未进入稳定 contract。

## 阶段 4：Graph Composer

状态：部分完成，未验证

已完成内容：
1. 新增 `FBlueprintHelperGraphComposer`。
2. 新增 `FBlueprintHelperGraphComposeResult`。
3. 实现最小 `ConnectLinearExecChain`，可按 fragment 的 `ExecExitPin -> ExecEntryPin` 顺序串接执行链。
4. 串接失败时记录 diagnostics，不吞掉 schema 拒绝原因。
5. `GenerateBlueprintFromJson` 会收集 `call/set` fragment。
6. `GenerateNodesAndLinksForGraph` 会收集 `call/set` fragment。
7. 当 payload 没有显式 exec links 且存在多个 fragment 时，生成流程会调用 Composer 自动串接 `call/set` 执行链。
8. Composer diagnostics 会进入 connection diagnostics。

距离期望的差距：
1. 尚未运行编译验证。
2. Composer 当前只处理线性 exec chain。
3. 尚未实现 branch then/else 自动接回后续语句。
4. 尚未实现显式命名临时值的 fragment 间引用。
5. 尚未实现语句树到数据流图的组合规则。
6. 尚未实现 data dependency 连接。
7. 尚未实现 fragment diagnostics 到 CLI DebugBundle 的完整上报。

## 阶段 5：Append / Replace / Merge / Patch Adapter 统一路径

状态：部分完成，未验证

已完成内容：
1. append 生成流程已经开始收集 `call/set` fragment。
2. append 生成流程在没有显式 exec links 时会使用 Graph Composer 自动串接 `call/set` fragment。
3. append 仍保留已有 explicit links 路径，避免对现有 payload 造成重复 exec 连线。

距离期望的差距：
1. 尚未运行编译验证。
2. append 尚未完全由 Composer 主导，当前只是 `call/set` 的最小接入。
3. replace 尚未接入 Graph Composer。
4. merge 尚未接入 Graph Composer。
5. patch 尚未接入 Graph Composer。
6. merge 仍未解决当前“直接创建函数调用节点”与 append owned graph 路径不一致的问题。
7. composed fragment 插入 anchor 和 successor 之间的 reachability 尚未实现。

## 阶段 5.5：TextToBlueprintGenerator 拆分减重

状态：部分完成，未验证

已完成内容：
1. `TextToBlueprintGenerator` 已降为 façade，只保留旧 public API 转发入口。
2. 新增 `FBlueprintGraphGenerationPipeline`，承接单图生成编排入口。
3. 新增 `FBlueprintMultiGraphGenerationPipeline`，承接多图生成编排入口。
4. 新增 `FBlueprintGraphJsonParser`，承接 JSON 字段解析和 parsed model 构造函数。
5. 新增 `FBlueprintGraphNodeSpawner`，承接 function、variable get/set、macro 节点生成。
6. 新增 `FBlueprintGraphDefaultValueApplier`，承接默认值应用和 pin default 写入。
7. 新增 `FBlueprintGraphLocalVariableService`，承接本地变量声明、类型转换和变量来源解析。
8. 新增 `FBlueprintGraphNodeUtility`，承接节点类型归一化、pin alias、函数解析、函数列表等小型工具。
9. 新增 `FBlueprintGraphLinker`，承接显式 links 和 Composer exec links 的连接职责。
10. 新增 `FBlueprintGraphExistingNodeMapper`，承接 FunctionEntry、FunctionResult、existing_node_refs 映射职责。
11. 保留 `TextToBlueprintGenerator` 的旧 public API，避免破坏现有调用点。

距离期望的差距：
1. 尚未运行编译验证。
2. `FBlueprintGraphGenerationPipeline` 当前仍保留较多编排细节，后续还需要进一步调用 `FBlueprintGraphExistingNodeMapper` 和 `FBlueprintGraphLinker` 替换内联逻辑。
3. `FBlueprintGraphJsonParser` 当前是字段解析函数集合，还没有形成一个完整的 `ParseGraphObject -> ParsedGraphData` 聚合 API。
4. `FBlueprintGraphNodeUtility` 仍偏宽，后续需要防止它演变成新的巨类。
5. 该拆分没有改变 statement tree、fragment DAG、branch、replace、merge、patch 的未完成状态。

## 阶段 6：验证和回归

状态：部分完成

已完成内容：
1. 2026-05-14 使用项目内日志路径执行编译：`TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -Log=D:\UEProjects\Template\Saved\UBT-Codex.log`。
2. 编译结果：`Result: Succeeded`。
3. 首次编译因 `AppData\Local\UnrealBuildTool` 日志备份权限被拒绝，随后改用项目内 `Saved\UBT-Codex.log` 成功绕过。
4. 首次成功编译输出 include-order 诊断后，已修复新拆分 pipeline `.cpp` 的首 include 顺序。

距离期望的差距：
1. 尚未运行 TypeScript 测试。
2. 尚未运行 Python 测试。
3. 尚未用物理门 TaskSpec 验证短名 `call/set`。
4. 尚未验证 `DoorPanel.AddAngularImpulseInDegrees` 显式对象调用路径。
5. 尚未验证 Pattern Registry alias、pin alias、defaults 是否在编辑器内生效。
6. 尚未验证 append Composer 自动串接是否会和旧 explicit links 路径冲突。
7. 尚未执行编辑器内功能回归。

## 本次已修改文件

1. `AgentFaceService/task-core/src/task/schema/task-schemas.ts`
2. `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
3. `AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py`
4. `AgentFaceService/task-core/src/task/schema/task-contract.ts`
5. `AgentFaceService/task-core/src/tests/task/task-contract-graphwrite.test.ts`
6. `AgentFaceService/task-core/src/tests/task/task-compiler.regression.test.ts`
7. `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h`
8. `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
9. `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/NodeHandlers/CallFunctionNodeHandler.cpp`
10. `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/NodeHandlers/VariableSetNodeHandler.cpp`
11. `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphComposer.h`
12. `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphComposer.cpp`
13. `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphPatternRegistry.h`
14. `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphPatternRegistry.cpp`
15. `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/TextToBlueprintGenerator.cpp`
16. `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/TextToBlueprintGenerator.h`
17. `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h`
18. `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp`
19. `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintMultiGraphGenerationPipeline.h`
20. `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintMultiGraphGenerationPipeline.cpp`
21. `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphJsonParser.h`
22. `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphJsonParser.cpp`
23. `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeSpawner.h`
24. `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeSpawner.cpp`
25. `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphDefaultValueApplier.h`
26. `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphDefaultValueApplier.cpp`
27. `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLinker.h`
28. `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLinker.cpp`
29. `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphExistingNodeMapper.h`
30. `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphExistingNodeMapper.cpp`
31. `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLocalVariableService.h`
32. `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLocalVariableService.cpp`
33. `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeUtility.h`
34. `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeUtility.cpp`
35. `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
36. `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
37. `BlueprintHelper/Resources/GraphPatterns/README.md`
38. `BlueprintHelper/Resources/GraphPatterns/call.defaults.json`

## 当前结论

当前实现完成了短名 `call/set` 到既有 IR 的前置兼容层，新增了 Statement IR / Expression IR contract，把 `call/set` 节点生成抽到 `NodeFragment` builder，补了 Pattern Registry loader，让 append 的 `call/set` 最小路径接入 Graph Composer，并将 `TextToBlueprintGenerator` 拆成 façade 加多个职责类。

它还不是完整的新架构。尚未完成的核心差距是编译验证、`branch` 语句树、数据依赖、named temporary、replace/merge/patch 统一 Adapter、Review body 聚合和验证回归。

## 2026-05-14 进度：Branch 语句树最小闭环

### 已完成

1. AgentFace TypeScript 编译器支持 `branch` 语句，`then` / `else` 可嵌套语句树。
2. AgentFace Python 编译器同步支持 `branch` 语句，避免 PowerShell/API 路径能力分叉。
3. `branch` 编译从线性 `previousNodeId` 改为 flow 编排，分支出口会自动接回后续语句。
4. `branch.condition` 支持布尔字面量，以及 `{ kind: "get" | "ref", target/name/var }` 变量读取表达式。
5. UE JSON 解析器支持 AgentFace 短字段：`kind: call/set/get/branch/custom_event`、`function`、`var`。
6. UE 图导入链路支持 `from` / `to` endpoint 字段，并继续兼容 `from_id/from_pin/to_id/to_pin`。
7. UE 图导入链路补齐 `set` 节点的 `value` 字段落地，避免 AgentFace 输出的赋值在导入端丢失。
8. Task contract 的当前 `statement_kinds` 增加 `branch`。

### 未完全完成

1. 还没有形成完整的通用 semantic resolver / symbol table；当前只覆盖 branch 条件所需的 bool literal 与 get/ref。
2. 还没有把所有 statement 都统一下沉到 `Graph Semantic IR -> Pattern Registry -> NodeFragment -> Graph Composer` 的完整链路；本次仍保留 AgentFace 直接生成 import payload 的兼容路径。
3. compare / select / make_struct / 运算节点等复杂表达式还没有进入 statement tree 到 fragment DAG 的统一编排。
4. JSON 数据驱动 binding / alias 仍只在既有 pattern registry 基础上局部可用，未完成完整低代码扩展面。

### 距离期望的差距

1. 当前完成的是 `branch` 的最小可用闭环，不等同于完整大图表框架完成。
2. 后续需要把表达式从局部条件处理升级成通用数据流子图，并接入 symbol table、Pattern Registry 和 Graph Composer。
3. 需要补一轮真实 TaskSpec 执行验证，确认 UE 侧 Branch、Get、Set 和自动回接连线在编辑器内行为符合预期。

## 2026-05-14 进度：阶段 1.5 补充

### 已完成

1. `FBlueprintHelperGraphSemanticIR` 新增 `Symbols`，用于记录 `let` 产生的命名临时值。
2. 新增 `FBlueprintHelperGraphSymbol`，记录 symbol 名称、来源 statement、来源 expression、类型和 path。
3. 新增 `FBlueprintHelperGraphResolvedTarget`，用于表达 target 的规范化结果。
4. 新增 `EBlueprintHelperGraphTargetKind`，区分 `Function`、`ComponentMemberFunction`、`Variable`、`PropertyPath`、`Temporary` 等语义目标。
5. Statement IR 新增 `PatternName`、`ResolvedTarget`、`ResultSymbolName`。
6. Expression IR 新增 `PatternName`、`ResolvedTarget`。
7. Expression IR 支持 `get_property`，用于覆盖设计文档里的显式属性读取示例。
8. IR builder 在 parse 后执行 semantic resolve，开始对 `call`、`set`、`branch`、`let`、`ref`、`compare`、`select`、`make_struct` 生成基础诊断。
9. `let/ref` 建立最小 symbol table 绑定，重复 symbol 和未找到 symbol 会进入 diagnostics。
10. `target` 字符串现在会被初步解析为函数、组件成员函数、变量或属性路径。

### 未完全完成

1. IR builder 仍未完全接入 AgentFace TypeScript/Python 编译主链；当前 TS/Python 仍以 payload lowering 为主。
2. IR builder 仍未成为 UE append/replace/merge/patch 的主运行路径。
3. semantic resolver 目前是字符串级启发式解析，还没有查 UE Blueprint 的真实组件、变量、函数和属性类型。
4. symbol table 目前是最小全局表，尚未实现分支/作用域隔离和生命周期规则。
5. expression 到 data fragment 的 pattern dispatch 只补了 `PatternName` contract，尚未真正生成 data fragment DAG。

### 距离期望的差距

1. 阶段 1.5 的 IR contract 更完整了，但还不能替代现有 GraphWrite payload 路径。
2. 后续需要在阶段 2/4 中让 Pattern Registry 和 Graph Composer 消费这些 `ResolvedTarget`、`Symbols`、`PatternName` 字段。
3. 后续需要接入真实 Blueprint 上下文解析，避免仅凭字符串把 target 错判为 component/function/property。

## 2026-05-14 进度：前三个阻塞项处理

### 已完成

1. 新增 `FBlueprintHelperGraphSemanticContext`，可从 `UBlueprint` 收集成员变量、SCS 组件、函数和基础类型信息。
2. `FBlueprintHelperGraphSemanticIRBuilder` 新增 Blueprint 上下文重载，后续可在真实资产上下文中构建 IR。
3. `ResolvedTarget` 增加 `bVerifiedByContext`，resolver 会标记 target 是否被真实 Blueprint 上下文验证。
4. `call/set/get/get_property` 的 target 解析开始使用 Blueprint 上下文区分函数、组件、变量和属性路径。
5. 未在 Blueprint 上下文中找到的 target 会生成 `target_unverified` warning，而不是静默按字符串猜测。
6. `let/ref` 改为作用域栈解析，branch 的 `then` / `else` 会创建子作用域，分支内 `let` 不向外泄漏。
7. 重复 `let` 现在只在同一作用域内报错，允许不同分支内使用相同临时名。
8. `ref` 解析改为从内到外查找作用域栈，未找到时继续生成 `ref_symbol_not_found` diagnostic。
9. 新增 `FBlueprintHelperGraphStatementBuilder::BuildExpressionFragment`，作为 expression 到 data fragment 的最小公共入口。
10. `BuildExpressionFragment` 当前支持 `get` expression 生成 variable get fragment，并输出 `DataOutputs` / `PinBindings`。
11. `literal` expression 明确标记为应绑定 pin default，不生成独立 fragment。
12. `compare/select/make_struct/get_property` 暂时返回明确的 unsupported pattern error，避免静默失败。

### 未完全完成

1. Blueprint 上下文 resolver 已建立入口，但还没有接入 append/replace/merge/patch 主运行路径。
2. target 验证目前只验证 Blueprint 级变量、组件、函数名称；组件成员函数是否真实存在还没有按组件类继续验证。
3. 属性路径只解析出 owner/path，还没有沿 `FProperty` 链验证每一段属性。
4. symbol scope 已支持分支隔离，但尚未支持 loop、macro scope、function-local shadowing 策略。
5. expression fragment 只完成 `get` 最小实现；`compare/select/make_struct/get_property` 还没有真实节点生成和 data dependency wiring。

### 阻塞内容

1. 还需要把 `BuildFromLogicSpec(..., UBlueprint*)` 接入真实 GraphWrite adapter，否则真实 Blueprint resolver 只是在 contract 层可用。
2. 还需要为组件成员函数和属性路径补 class/property 级验证。
3. 还需要继续补 expression pattern：`compare`、`select`、`make_struct`、`get_property`。

## 2026-05-14 进度：阻塞项第二轮

### 已完成

1. `FBlueprintHelperGraphSemanticContext` 新增 `TargetStructs`，可记录变量、组件、类成员对应的 `UStruct/UClass`。
2. Blueprint 上下文构建时会把 SCS 组件类、变量类型对象、GeneratedClass/SkeletonGeneratedClass/ParentClass 属性类型写入 resolver context。
3. 组件成员函数 target 现在不只判断 owner 名称存在，还会在 owner class 上查找目标函数。
4. 属性路径 target 现在会沿 `FProperty` 链逐段验证，并输出最终属性类型。
5. `ResolvedTarget.Type` 对组件成员、变量、属性路径的类型信息更准确，可供后续 pattern dispatch 使用。
6. `make_struct` expression fragment 增加最小真实节点生成：支持 literal 字段默认值，生成 `K2Node_MakeStruct` 并暴露 data output。
7. `make_struct` 的非 literal 字段会返回明确错误，等待 data dependency wiring，而不是静默生成错误节点。

### 未完全完成

1. 组件成员函数验证当前只支持 owner 能解析到 `UClass` 的场景；接口、动态对象、软引用对象还未覆盖。
2. 属性路径验证当前支持 struct/object/class property 链；数组、Map、Set、optional 和函数返回值链还未覆盖。
3. `make_struct` 目前只支持 literal 字段默认值，尚未支持字段值来自其他 expression fragment。
4. `compare/select/get_property` 还没有真实 fragment 生成。
5. Semantic IR 仍未接入 append/replace/merge/patch 主运行路径。

### 阻塞内容

1. 需要补 data dependency wiring，把 expression fragment 输出连接到 make_struct/call/set/branch 输入。
2. 需要实现 `get_property` 的实际 property access fragment，才能支持 `DoorPanel.RelativeRotation.Yaw` 这类路径。
3. 需要实现 `compare/select` 的类型推断和 operator/select 节点生成。
4. 需要把 Semantic IR builder 接入 GraphWrite adapter 主路径。

## 2026-05-14 进度：按新完成标准同步阶段 1 / 1.5 / 2

### 阶段 1 当前已完成

1. AgentFace TypeScript 编译器已支持 `branch`、`let`、`return` 语句短名。
2. AgentFace Python 编译器已同步支持 `branch`、`let`、`return` 语句短名。
3. AgentFace TypeScript/Python 编译器已支持 `get`、`get_property`、`ref`、`call`、`compare`、`select`、`make_struct` expression 的最小 payload lowering。
4. `let/ref` 在 TypeScript/Python lowering 中已建立最小命名临时值引用。
5. Task contract 已将 `statement_kinds` 扩展到 `call/set/branch/let/return`。
6. Task contract 已新增 `expression_kinds`，覆盖 `literal/get/get_property/ref/call/compare/select/make_struct`。
7. UE JSON parser 已支持 `make_struct` 短名和 `type -> struct_path` 回退。
8. UE JSON parser 已支持 `compare` 短名映射到 promotable operator 节点类型。
9. AgentFace append bridge payload 会携带 `logic_spec`，供 UE 侧 Semantic IR builder 预检。

### 阶段 1 尚未完成

1. 旧字段尚未按短名实现进度逐步移除，目前仍保留兼容路径。
2. `compare/select/make_struct` 的 lowering 只是最小 payload 形状，还没有经过完整编辑器内验证。
3. TypeScript/Python 单元测试尚未更新和执行。

### 阶段 1.5 当前已完成

1. Semantic IR builder 已支持从 `logic_spec` 构建 Statement IR / Expression IR。
2. Semantic IR builder 已支持 Blueprint 上下文重载，可解析变量、组件、函数、属性路径等 typed target。
3. Semantic IR builder 已实现最小 symbol table，`let/ref` 可建立命名临时值引用。
4. UE append preflight 已接入 Semantic IR builder：存在 `logic_spec` 时会在真实 Blueprint 上下文中构建 IR，并将 error diagnostic 作为 preflight error 返回。
5. append 的 AgentImport 兼容 payload 会保留 `logic_spec`，为后续 DebugBundle / Review evidence 提供输入来源。

### 阶段 1.5 尚未完成

1. Semantic IR builder 尚未接入 replace/merge/patch 运行路径。
2. Semantic IR builder 目前在 append 中只作为 preflight 入口，尚未成为实际落图主路径。
3. expression 到 data fragment 的 pattern dispatch 只完成 `get` 和 `make_struct` 最小入口，尚未覆盖 `branch/compare/select/get_property` 的完整 dispatch。

### 阶段 2 当前已完成

1. `FBlueprintHelperNodeFragment` 已具备 exec pin、data input/output、pin binding、layout hint、ownership、review target、diagnostics 等基础字段。
2. `call`、`set`、`get`、`make_struct` 已有 fragment builder 或最小 expression fragment builder。
3. `get` expression fragment 可生成 variable get，并暴露 data output。
4. `make_struct` expression fragment 可生成 literal-only 的 `K2Node_MakeStruct`，并暴露 data output。

### 阶段 2 尚未完成

1. 尚未形成 statement tree 到 fragment DAG 的统一编排。
2. typed pin model 和 layout model 仍是基础字段，不是完整模型。
3. `branch`、`let`、`compare`、`select`、`get_property` 尚未完成 fragment builder。
4. Review 粒度尚未按 function/event/macro 图体聚合。
5. fragment 字段尚未完整接入 DebugBundle 和 Review evidence。

### 阻塞内容

1. 需要补 replace/merge/patch 的 Semantic IR preflight/运行路径接入。
2. 需要补 statement tree 到 fragment DAG 的统一编排器。
3. 需要补 branch/compare/select/get_property fragment builder 和 data dependency wiring。
4. 需要补 Review 聚合和 DebugBundle evidence 输出。
5. 需要更新并执行 TypeScript/Python 相关测试。

## 2026-05-14 进度：阶段 1 / 1.5 / 2 本轮同步

### 已完成
1. 修复 AgentFace TypeScript 编译器的语句流控制：`let` 不再截断上一条可执行语句的出口，`return` 会清空后续可接回出口，避免 `return` 后续语句被错误串接。
2. 修复 AgentFace Python 编译器的同类语句流控制，保持 PowerShell/API 路径与 TypeScript 编译链一致。
3. 确认 Python append bridge payload 已携带 `logic_spec`，可供 UE 侧 Semantic IR preflight 使用。
4. 新增并编译通过 `FBlueprintHelperGraphFragmentDag` 窄接口，包含 fragment ref、exec/data edge、entry/exit、diagnostic、metadata 等基础字段。
5. 新增并编译通过 `FBlueprintHelperGraphFragmentDagBuilder`，可从 Semantic IR 形成 statement tree 到 fragment DAG 的结构化编排，支持 sequence、branch 自动 join、let/ref symbol data edge、compare/select/make_struct placeholder expression fragment。
6. 修复 `FBlueprintHelperGraphFragmentDagBuilder` 中持有 `TArray` fragment 引用后继续追加 fragment 的潜在引用失效风险，改为保存稳定 `FragmentId` 后再连接子表达式。
7. 新增并编译通过 `FBlueprintHelperGraphFragmentEvidence` 窄接口，可从 fragment DAG 生成 Review/Debug evidence bundle 的基础结构。
8. Replace / Merge / Patch 已补入 `logic_spec` Semantic IR preflight 入口；Append 已保留 `logic_spec` 并在 preflight 中构建 Semantic IR。
9. 2026-05-14 本轮 UE 编译通过：`TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject`，结果 `Succeeded`。

### 未完全完成
1. 阶段 1 的 `branch`、`let`、`compare`、`select`、`make_struct` 已进入 AgentFace schema/contract 和 TypeScript/Python 编译器最小 lowering；旧字段仍保留兼容路径，尚未按短名实现进度逐步移除。
2. 阶段 1.5 的 IR builder 已能在 UE append/replace/merge/patch preflight 中消费 `logic_spec`，但还没有替代现有 nodes/links 成为实际落图主路径。
3. Semantic resolver 已可解析 component、variable、function、property path 等 typed target，并能基于 Blueprint 上下文验证一部分目标；复杂容器属性、接口、动态对象、返回值链仍未覆盖。
4. Symbol table 已支持 `let/ref` 和 branch 子作用域；尚未覆盖 loop、macro scope、function-local shadowing 等完整生命周期策略。
5. Expression 到 data fragment 的 dispatch 已形成 DAG 结构入口，但 `compare/select/make_struct` 在 DAG builder 中仍是 placeholder fragment，不等同于真实 UE 节点生成。
6. 阶段 2 已形成 statement tree 到 fragment DAG 的结构编排，但 typed pin model 和 layout model 仍是基础字段集合，不是完整模型。
7. Review evidence 已有 bundle 数据结构和 DAG 转换入口，但 Review 粒度按 function/event/macro 图体聚合尚未接入真实 ReviewStore / ReviewPanel 输出链路。
8. fragment 字段尚未完整接入 CLI DebugBundle；当前只是 evidence 窄接口，未进入 `ExportDebugBundle` 或 CLI debug command 的真实输出。
9. TypeScript/Python 单元测试本轮未执行；仅执行并通过 UE C++ 编译。

### 阻塞内容
1. Stage 2 的“完整 typed pin model / layout model”需要先明确与现有 `FBlueprintHelperNodeFragment`、DAG endpoint、UE pin mutation 三者的字段边界，否则会出现两个 fragment model 并行膨胀。
2. Review 按 function/event/macro 聚合需要先决定聚合发生在 transaction 写入层、ReviewStore 归并层，还是 FragmentEvidence 输出层；不同选择会影响 Reject/Accept 的事务边界。
3. DebugBundle 接入需要明确 CLI 侧 debug schema 是否新增 `fragment_dag` / `fragment_evidence` 顶层字段，还是挂到现有 artifacts/debug_case 下，避免再次造成返回字段 schema 漂移。
## 2026-05-14 进度：Stage 2 typed pin / layout model 补强

### 已完成
1. `FBlueprintHelperGraphFragmentDag` 新增 `EBlueprintHelperGraphFragmentPortDirection`，区分 `ExecInput`、`ExecOutput`、`DataInput`、`DataOutput`。
2. `FBlueprintHelperGraphFragmentEndpointRef` 新增 `Direction` 与 `PinType`，保留原 `Type` 字符串兼容字段，同时提供可扩展 pin type 描述。
3. 新增 `FBlueprintHelperGraphFragmentPinTypeRef`，包含 category、subcategory、object path、container type、value type、reference/const 标记等可序列化字段。
4. 新增 `EBlueprintHelperGraphFragmentLayoutKind` 与 `FBlueprintHelperGraphFragmentLayoutRef`，用于描述 statement、expression、join fragment 的布局类别、行列、位置、尺寸和 layout hints。
5. DAG builder 已为 exec/data endpoint 填充 direction，并为 statement/expression/join fragment 填充 layout kind 与 source path hint。
6. 2026-05-14 本轮补强后再次执行 UE 编译，结果 `Succeeded`。

### 未完全完成
1. typed pin model 仍未从真实 UE `FEdGraphPinType` 完整映射 category/subcategory/object/container/value type，目前主要承载字符串 type 与方向信息。
2. layout model 已形成可序列化字段，但还没有接入真实 layout solver 或 UE 节点位置生成策略。
3. DAG typed endpoint 还没有反向同步到 `FBlueprintHelperNodeFragment` 的运行时 pin 指针模型；当前两层通过 fragment id / port id / pin name 约定衔接。
4. DebugBundle 和 Review evidence 尚未输出这些 typed pin/layout 字段到 CLI 可见结果。

### 阻塞内容
1. 需要定义 `FEdGraphPinType -> FBlueprintHelperGraphFragmentPinTypeRef` 的统一转换函数，否则各 builder 会各自拼 pin type，后续难以维护。
2. 需要明确 layout solver 是 DAG 层输出相对布局，还是落图层根据 UE 节点尺寸回填绝对布局；这会影响 `Position` / `Row` / `Column` 的权威来源。
## 2026-05-14 进度：Review 聚合与 DebugBundle fragment 引用接入

### 已完成
1. `FBlueprintHelperReviewStoreService` 增加 graph body 聚合规则：Graph surface 且带 `GraphName` 的图体级目标会统一归并到 `graph_body|<GraphName>`，用于把 function/event/macro 图体内的节点级变更聚合为图体级 Review change。
2. 聚合规则排除了 component、variable、property 相关目标，避免破坏组件增减、变量增减、继承/默认值变更仍保持当前粒度的要求。
3. `MakeAtomicTargetsForInput` 与 `AddEvidenceAtomicTargets` 都接入了 graph body 聚合，覆盖 transaction input 和 write review evidence 两条 Review 构建路径。
4. `FBlueprintHelperDebugCase` 与 `FBlueprintHelperDebugCaseSummary` 新增 `fragment_artifacts` 可选字段，用于承载 fragment DAG / fragment evidence 的相对 artifact 引用、计数和签名。
5. `FBlueprintHelperDebugBundleManifest` 新增 `fragment_artifacts` 可选字段，DebugBundle manifest 可显式列出 fragment artifact 引用。
6. `FBlueprintHelperDebugEntryService` 会从 `ToolResultSummary`、`data.fragment_artifacts` 或 `data.fragment_debug.fragment_artifacts` 提取 fragment artifact 引用并持久化到 DebugCase。
7. `FBlueprintHelperDebugCaseStoreService::ExportDebugBundleSummary` 会在 DebugCase 存在 fragment artifact 引用时导出 `artifacts/graph_fragment.summary.json`，并把引用写入 manifest。
8. 2026-05-14 本轮 Review/DebugBundle 接入后执行 UE 编译，结果 `Succeeded`。

### 未完全完成
1. Review 聚合已经按 graph body 归并，但还没有从 fragment evidence 自动生成 function/event/macro 专用 Review atomic target；当前依赖上游 evidence/transaction target 已带 Graph surface 和 GraphName。
2. DebugBundle 已具备 fragment artifact 引用承载、持久化和 manifest 输出能力，但还没有由 GraphWrite 运行路径实际生成 `graph_fragment_dag.v1.json` 与 `graph_fragment_evidence.v1.json` 文件本体。
3. `FBlueprintHelperGraphFragmentEvidenceBundle` 仍未转换为 `FBlueprintHelperWriteReviewEvidence`，因此 fragment evidence 还没有驱动真实 ReviewStore 记录生成。
4. `fragment_artifacts` 当前只通过 `ToolResultSummary` 提取，尚未接入成功路径的主动 debug bundle 生成策略。

### 阻塞内容
1. 需要确定 fragment artifact 文件本体的 schema：直接序列化完整 DAG/evidence，还是输出裁切后的 summary + hash。该决策会影响 DebugBundle 大小和敏感字段裁切策略。
2. 需要确定 fragment evidence 到 ReviewStore 的转换位置：在 GraphWrite service 生成 `FBlueprintHelperWriteReviewEvidence`，还是在 ReviewStore 直接消费 `FBlueprintHelperGraphFragmentEvidenceBundle`。
3. 需要确定成功路径是否也生成 DebugBundle fragment artifacts；当前 DebugCase 主要服务失败路径，成功路径若强制产出会带来额外 IO 和存储开销。
## 2026-05-14 进度：fragment DAG / evidence JSON 序列化入口

### 已完成
1. `FBlueprintHelperGraphFragmentDag` 及其 fragment、endpoint、exec edge、data edge、entry/exit、diagnostic、typed pin、layout 子结构新增 `ToJson()`。
2. DAG JSON 输出包含 `schema`、`fragments`、`exec_edges`、`data_edges`、`entry_exit_refs`、`diagnostics`、`metadata`，并保留 typed pin direction / pin type / layout 信息。
3. `FBlueprintHelperGraphFragmentEvidenceBundle` 及其 review scope、fragment ref、diagnostic 子结构新增 `ToJson()`。
4. Evidence JSON 输出包含 `review_scopes`、`fragments`、`source_statement_ids`、`fragment_ids`、`diagnostics`、`metadata`、`debug_metadata`。
5. 序列化输出全部使用可序列化字符串和结构化 JSON，不暴露运行时 `UEdGraphPin*` 或 `UK2Node*` 指针。
6. 2026-05-14 本轮序列化入口补充后执行 UE 编译，结果 `Succeeded`。

### 未完全完成
1. 序列化入口已经完成，但 GraphWrite append/replace/merge/patch 尚未在执行结果中主动产出 `fragment_dag` / `fragment_evidence` JSON。
2. DebugBundle 当前可承载 fragment artifact 引用和摘要，但还没有从 GraphWrite 运行路径自动写入完整 `graph_fragment_dag.v1.json` / `graph_fragment_evidence.v1.json`。
3. ReviewStore 仍未直接消费 `FBlueprintHelperGraphFragmentEvidenceBundle`，需要后续补 adapter 将 scope/fragment 转成 `FBlueprintHelperWriteReviewEvidence`。

### 阻塞内容
1. 需要确定 fragment JSON 在 ToolResult 中是以内联 `data.fragment_debug` 暂存，还是直接写入 debug bundle artifact 目录并只返回 ref。
2. 需要确定成功路径是否默认生成 fragment debug artifacts；如果默认生成，可能影响 CLI 返回大小和磁盘 IO。
## 2026-05-14 进度：GraphWrite fragment_debug 接入 DebugBundle 输出链路

### 已完成
1. 新增 `FBlueprintHelperGraphFragmentDebugData`，统一从 `logic_spec + UBlueprint` 构建 Semantic IR、fragment DAG、fragment evidence，并输出 `data.fragment_debug` JSON。
2. Append / Replace / Merge / Patch 的 `logic_spec` preflight 结果都增加 `FragmentDebugData`，避免四条 adapter 路径重复实现 DAG/evidence 构造逻辑。
3. Append / Replace / Merge / Patch 的 dry-run 成功、dry-run blocked、write preflight blocked、write success 数据输出都会附加 `data.fragment_debug`。
4. `data.fragment_debug` 包含 `fragment_dag`、`fragment_evidence`、`fragment_artifacts` 计数摘要。
5. `FBlueprintHelperDebugCaseStoreService::ExportDebugBundleSummary` 现在会从 DebugCase event 的 `tool_result_summary.data.fragment_debug` 中提取 inline DAG/evidence。
6. DebugBundle 导出时会写入 `artifacts/graph_fragment_dag.v1.json` 与 `artifacts/graph_fragment_evidence.v1.json`，并把相对引用写入 manifest 的 `fragment_artifacts`。
7. DebugBundle 还会写入 `artifacts/graph_fragment.summary.json`，用于摘要记录 fragment refs 与计数信息。
8. 2026-05-14 本轮 GraphWrite / DebugBundle 接入后执行 UE 编译，结果 `Succeeded`。

### 未完全完成
1. fragment evidence 已经能进入 DebugBundle，但还没有直接转换为 `FBlueprintHelperWriteReviewEvidence` 并驱动 ReviewStore 创建记录。
2. `fragment_artifacts` 当前主要通过 DebugCase event 的 `ToolResultSummary` 导出；成功路径若没有创建 DebugCase，则不会自动生成 bundle 文件，仍需要 CLI 主动保存 artifacts 或失败后导出 bundle。
3. `compare/select/make_struct` 在 DAG 中仍是 structural/placeholder fragment，DebugBundle 能看到结构，但不能代表真实 UE 节点生成已经完成。
4. `fragment_debug` 目前会随 ToolResult data 返回，后续可能需要按 CLI 输出大小策略默认裁切或只进入 artifacts。

### 阻塞内容
1. 需要补 `FBlueprintHelperGraphFragmentEvidenceBundle -> FBlueprintHelperWriteReviewEvidence` adapter，才能把 fragment evidence 真正接入 ReviewStore。
2. 需要决定 CLI 默认是否隐藏 `data.fragment_debug`，只通过 artifact 或 `--select` 暴露，避免大型图导致 stdout 过大。
3. 需要把 placeholder expression fragment 替换为真实 `compare/select/make_struct` UE 节点生成，才能将 Stage 2 标记为完整完成。
## 2026-05-14 进度：fragment evidence 到 ReviewStore adapter

### 已完成
1. `FBlueprintHelperReviewStoreService` 新增 `BuildReviewRecordsFromFragmentEvidence`，可直接消费 `FBlueprintHelperGraphFragmentEvidenceBundle`。
2. 新 adapter 会把 fragment evidence 的 review scope 转成图体级 `FBlueprintHelperReviewAtomicTarget`，使用 `graph_body|<GraphName>` 作为视觉聚合键。
3. adapter 生成的 Review target 保持 Graph surface，不影响组件、变量、属性等非图体改动的现有 Review 粒度。
4. fragment evidence 缺少 scope 时会生成一个 fallback graph body target，避免 evidence bundle 无法落入 Review 记录。
5. 2026-05-14 本轮 Review evidence adapter 接入后执行 UE 编译，结果 `Succeeded`。

### 未完全完成
1. adapter 已存在并编译通过，但 GraphWrite 写入路径尚未在保存 ReviewRecord 时调用该 adapter。
2. ReviewPanel 尚未针对 fragment evidence scope 增加专用显示文案；目前会复用现有 visible change / atomic target 机制。
3. `compare/select/make_struct` 的 placeholder fragment 仍需要升级为真实 fragment builder，才能把 Review evidence 的 fragment 语义视为完整。

### 阻塞内容
1. 需要确定 GraphWrite transaction 写入后由哪一层负责调用 `BuildReviewRecordsFromFragmentEvidence`：GraphWrite service、TaskRuntimeCluster，还是 ReviewStore 归档流程。
2. 需要确定同一个 TaskRun 内同时存在旧 `FBlueprintHelperWriteReviewEvidence` 和 fragment evidence 时的去重规则。
## 2026-05-14 进度：AgentFace 编译验证

### 已完成
1. Python 编译级检查通过：`python -m compileall -q AgentFaceService/task-core/python`。
2. TypeScript 编译发现并修复 `compileStatementSequence` 中 `flow.entry` 可选字段未稳定收窄的问题。
3. 修复后使用 `npm.cmd run build` 完成 `@blueprinthelper/task-core` TypeScript build，结果通过。
4. `npm run build` 首次失败原因是 PowerShell Execution Policy 拦截 `npm.ps1`，不是代码编译错误；已改用 `npm.cmd` 验证。

### 未完全完成
1. 本轮执行的是编译级检查，没有运行 TypeScript/Python 单元测试。
2. AgentFace 编译链已通过 build，但尚未用真实物理门 TaskSpec 重新执行 editor 写入验证。

### 阻塞内容
1. 若后续要在 PowerShell 直接使用 `npm` 而不是 `npm.cmd`，仍需要用户侧调整 Execution Policy 或 shell 策略；当前开发验证不依赖该调整。
## 2026-05-14 进度：compare / select / make_struct fragment builder 收口

### 已完成
1. `FBlueprintHelperGraphStatementBuilder::BuildExpressionFragment` 新增 `compare` expression fragment builder，使用现有 `FPromotableOperatorNodeHandler` 生成真实 `K2Node_PromotableOperator` 节点。
2. `compare` fragment 会暴露 `left` / `right` data input 和 `result` data output，literal 输入会写入默认值，非 literal 输入留给 data dependency wiring。
3. `FBlueprintHelperGraphStatementBuilder::BuildExpressionFragment` 新增 `select` expression fragment builder，使用现有 `FSelectNodeHandler` 生成真实 `K2Node_Select` 节点。
4. `select` fragment 会暴露 `condition`、`OptionN` data input 和 `result` data output，literal 条件/选项会写入默认值，非 literal 输入留给 data dependency wiring。
5. `make_struct` 之前已使用 `FStructOperationNodeHandler` 生成真实 `K2Node_MakeStruct`；本轮保留该路径。
6. `FBlueprintHelperGraphFragmentDagBuilder` 中 `compare/select/make_struct` 不再作为 placeholder diagnostic 输出，而是作为正式 structural expression fragment 进入 DAG。
7. 2026-05-14 本轮 fragment builder 收口后执行 UE 编译，结果 `Succeeded`。

### 未完全完成
1. `compare` 的 operator 当前直接使用 `Expression.Operator` 作为 `PromotableOperator` function name；还需要 Pattern Registry alias 或类型推断把 `>`、`==` 等短操作符稳定映射到 UE 可解析函数。
2. `select` 当前支持基础 option 数量和 literal default，尚未处理 enum select 的 AgentFace 字段映射。
3. 非 literal expression 的 data dependency wiring 已在 fragment/DAG 层暴露端口，但实际落图连线还需要 composer/linker 继续消费这些 data edges。
4. `get_property` 仍未有真实 property access node fragment builder。

### 阻塞内容
1. 需要补 operator alias / type inference，否则 `compare` 能生成 fragment，但某些短操作符输入可能在 UE function resolve 阶段失败。
2. 需要补 data edge 到 UE pin link 的落图消费，才能把非 literal compare/select/make_struct 字段值完全连上。
3. 需要补 `get_property` 的真实节点生成策略，尤其是 struct/object property chain 如何拆成节点链。
## 2026-05-14 进度：阻塞项收口 - get_property / compare / data edge / TS-Python 测试

### 已完成
1. get_property 增加真实 UE 节点片段构建：Owner 先生成变量读取节点；struct 属性段通过 BreakStruct 展开；object/interface 属性段通过带 target/self 输入的变量读取节点展开；最终输出统一暴露 value 与属性路径 pin binding。
2. compare 增加短操作符 alias 与基础类型推断候选：支持 >、>=、<、<=、==、!=、gt、gte、lt、lte、eq、ne、and、or 等输入稳定映射到 UE PromotableOperator 可解析函数名。
3. PromotableOperator 旧 JSON 路径同步接入短操作符 fallback，避免只有 GraphStatementBuilder 路径支持 alias。
4. make_struct 不再强制所有字段必须 literal；非 literal 字段会暴露 DataInputs/PinBindings，交给 data edge 连接消费。
5. GraphComposer 增加 ConnectDataEdges，能从 fragment DAG data edge 解析 Fragment endpoint 并连接 UE data pin，重复连接会跳过不报错。
6. GraphLinker 增加 ConnectFragmentDataEdges 作为 pipeline 统一入口。
7. GraphGenerationPipeline 增加 data-only fragment 收集与 data_edges / fragment_dag / fragment_debug.dag 消费入口，节点生成后会尝试连接 fragment data edge。
8. AgentFace TaskCore 测试快照同步 logic_spec、短名 statement/expression 合约、debug bundle 工具名与当前 then 分支节点 ID 规则。

### 验证
1. UE 编译通过：TemplateEditor Win64 Development。
2. TaskCore 全量测试通过：npm.cmd --prefix .\\AgentFaceService\\task-core run test。
3. Python unittest 通过：48 tests OK。
4. Node 测试通过：99 tests passed。

### 距离期望的差距
1. get_property 当前依赖 UE pin 类型推断 struct/object owner；如果上游节点 pin 缺少 PinSubCategoryObject，会明确失败并返回诊断，尚未实现无类型上下文下的反射兜底搜索。
2. data edge 消费已接入 pipeline，但完整语义 IR 直接生成 UE 节点仍依赖现有 nodes/links 输出路径；fragment DAG 作为外部 data_edges 输入已可消费，后续仍应把 SemanticIR -> NodeFragment emission 做成一条完整写入路径。
3. compare alias 已覆盖常见比较/布尔操作符，复杂容器、枚举、自定义结构比较仍依赖 UE 函数可解析性。

### 阻塞状态
1. 本轮列出的四个阻塞项已完成可编译、可测试闭环。

## 2026-05-14 进度：SemanticIR -> NodeFragment emission 主路径化

### 已完成
1. `FBlueprintHelperGraphSemanticIRBuilder` 解析 expression 时读取稳定 `id`，并支持 `value_type`、`operator`、`condition/index` 兼容字段，确保 AgentFace 语句树能携带到 DAG 层。
2. `get_property` semantic resolver 在 `TargetStructs` 缺失时增加基于类型名的 `UObjectIterator<UStruct>` fallback，可从目标类型名反查 `UStruct/UClass` 后继续解析 property path。
3. `compare` semantic resolver 统一回填 `bool` 类型，避免比较表达式向后传播空类型。
4. FragmentDagBuilder 对稳定 statement/expression `id` 不再额外加 `stmt_`/`expr_` 前缀，使 SemanticIR 产生的 fragment id 可与实际 generated node fragment id 对齐。
5. `let/ref` 的 symbol producer 改为优先指向 value expression producer，而不是只指向 let 结构 fragment，减少命名临时值在 data edge 落图时找不到真实节点的概率。
6. expression `call` 在 DAG 中不再输出 placeholder warning，改为正式 structural expression fragment；真实节点生成仍由现有 node emission 路径承载。
7. `FBlueprintGraphGenerationPipeline` 在连接 data edge 前会从 `logic_spec` 现场构建 `SemanticIR -> FragmentDag`，并把可落到真实 generated fragment 的 DAG data edge 交给 `FBlueprintGraphLinker::ConnectFragmentDataEdges`。
8. data-only fragment 增加 `left/right/condition/value` 输入 alias，GraphComposer 增加大小写不敏感 pin lookup，使 DAG 的语义 pin 名能匹配 UE 真实 pin 名。
9. AgentFace TypeScript/Python 编译链会把 `logic_spec` statement/expression 克隆为带稳定 `id` 的版本，id 规则与生成 `nodes/links` 使用的 node id 保持一致。

### 验证
1. UE 编译通过：`TemplateEditor Win64 Development`，`Result: Succeeded`。
2. AgentFace TypeScript build 通过：`npm.cmd --prefix .\AgentFaceService\task-core run build`。
3. AgentFace Python compileall 通过：`python -m compileall -q .\AgentFaceService\task-core\python`。

### 距离期望的差距
1. SemanticIR -> NodeFragment emission 已成为 data edge emission/consumption 主路径，但 UE 节点实例化仍依赖 `nodes/links` 兼容 payload；尚未完成“SemanticIR 直接创建所有 UE 节点”的唯一主路径化。
2. `FragmentDag.ExecEdges` 尚未成为执行流落图的主数据源，当前 exec 仍主要由 existing links 与 linear composer 处理。
3. literal/ref 等不产生真实 UE 节点的结构 fragment 目前在 pipeline 侧按 generated fragment id 过滤，不会报错，但这也意味着它们暂时只服务 debug/evidence，不直接落图。
4. 复杂 `compare` 仍受 UE PromotableOperator/函数解析命中范围限制，自定义结构体、容器、枚举等比较还没有完整 typed resolver 签名匹配策略。
5. 本轮未运行 TS/Python 单元测试，也未做真实编辑器 TaskSpec 写入回归。

### 阻塞内容
1. 若要彻底移除 `nodes/links` 作为节点创建主承载，需要新增 SemanticIR pattern -> NodeFragment -> UE node mutator 的统一创建入口，并让 append/replace/merge/patch 都只调用该入口。
2. 若要 exec flow 完全统一，需要让 `FragmentDag.ExecEdges` 驱动 branch then/else、join、后续语句接回以及 existing anchor/successor 的执行流插入。
3. 若要复杂 compare 完整稳定，需要建立“操作符 alias + operand typed pin inference + UE 函数签名匹配”的二阶段 resolver，而不是只依赖现有名称候选。
## 2026-05-14 快速修复：SemanticIR ReconstructNode 顺序风险

### 已完成
1. 移除 SemanticIR 唯一节点创建路径中位于 exec 连线之后的 `ReconstructNode` 循环，避免 UE 重建节点 pin 时破坏已经创建的执行流连接。
2. 保留节点 handler 自身的 `PostPlacedNewNode` / `AllocateDefaultPins` / default value 应用路径，SemanticIR data edge 仍在节点创建完成后由 GraphLinker/GraphComposer 连接。

### 距离期望的差距
1. 尚未执行真实编辑器 TaskSpec 覆盖测试，不能确认 exec/data 连线在实际蓝图中完全符合预期。
2. 后续如果某些节点类型确实需要重建 pin，应改为在该节点创建后、任何连线发生前局部处理，而不是在统一 semantic 连线之后全量重建。

### 阻塞内容
1. 需要端到端覆盖测试确认 SemanticIR statement tree -> UE node emission -> exec/data pin link -> Review/DebugBundle 是否完整跑通。

## 2026-05-14 覆盖测试：TaskSpec -> Editor/Bridge -> GraphWrite

### 已完成
1. Runtime profile 检查完成：Editor 正在运行、Bridge 已连接；profile 因缺少 write session 和 risk command 显示 degraded。
2. Runtime diagnostics 检查完成：Blocking 为 None；Warning 包含 write_permission.disabled、risk_command.disabled、project_marker.missing。
3. 专用覆盖测试资产创建完成：`/Game/BP_BH_SemanticCoverageActor`，`task_run_id=task_EDC7473F4FB994D86FE9B29A13C42780`，compile result succeeded，warning_count=0。
4. 最小 EventGraph 写入完成：自定义事件 `BH_SemanticCoverage_Minimal_EG` + `PrintString`，`task_run_id=task_74FBDD484DCCFFF7DDD0B8924E81E96D`，GraphWrite applied，compile result succeeded，warning_count=0。
5. 确认写入授权工具的正确入参为 `{ reason, scope, ttl_seconds, asset_paths }`；直接传 TaskSpec 给 `blueprinthelper_request_write_session` 会失败。

### 未完全完成
1. `let/compare/branch` 覆盖用例未通过 preview；错误为 `unsupported_graph_write_statement_kind`，路径为 `task_plan.steps[1].write.ops[0].body.statements[0].kind`。
2. 端到端成功用例仍通过 `append_blueprint_graph` adapter 落图，未证明 SemanticIR 是唯一 UE 节点创建入口。
3. `append_new_owned_graph` 指向不存在图名时 preview 未阻止，execute 才返回 `target_graph_not_found`。
4. 尚未执行 `select/make_struct/get_property` 的真实编辑器写入覆盖，因为 `let/branch/compare` 已在 preview 阶段被 TaskRuntime 旧 statement gate 阻断。

### 距离期望的差距
1. 当前真实跑通的是旧兼容最小链路：TaskSpec `body.statements` -> TaskRuntime statement 降级 -> `append_blueprint_graph` -> compile。
2. 期望链路应为：TaskSpec `BlueprintLogicSpec` -> SemanticIR -> NodeFragment/FragmentDAG -> Composer/Linker -> UE Mutator -> Review/DebugBundle。
3. 需要修复 TaskRuntime `ensure_entry` 对 `body.statements` 的旧 gate，改为消费 `logic_spec` 并走统一 SemanticIR 执行路径。
4. 需要补 preview 对目标 graph 存在性和 append 目标图策略的验证，避免 preview passed / execute failed 的不一致。

### 阻塞内容
1. TaskRuntime 当前 `ensure_entry` 执行分支仍只支持 `call_function` / `set_member_variable`，阻断 `branch/let/compare` 进入真实 GraphWrite 写入。
2. TaskRuntime 当前仍生成旧 `nodes/links` payload，导致 SemanticIR-only 入口无法通过 TaskSpec 端到端证明。
3. preview 缺少目标图存在性校验，导致覆盖测试中错误图名无法在执行前拦截。

## 2026-05-14 修正：append_new_owned_graph 新图语义与 SemanticIR 写入链路

### 已完成
1. 修正 `append_new_owned_graph` 语义判断：目标图名不存在是创建新 owned graph 的正常路径，不应作为 preview 阻塞条件。
2. TaskRuntime graph_write `ensure_entry` 不再把 `body.statements` 降级为旧 `nodes/links` payload，而是输出 `logic_spec`，交给 GraphWrite SemanticIR 路径落图。
3. AppendGraph preflight 允许 `logic_spec` 单独作为写入 payload，不再要求旧 `nodes` 数组非空。
4. `ensure_custom_event` 在目标 EventGraph 不存在时返回 deferred/no-op，让后续 GraphWrite 创建新图和事件节点；如果同名 custom event 已存在于其他图中，仍返回冲突。
5. UE 编译通过：`TemplateEditor Win64 Development`，结果 `Succeeded`。

### 未完全完成
1. 尚未在重载新 DLL 的编辑器中重新执行 CLI 覆盖测试，不能确认运行中的 Editor/Bridge 已使用本次新代码。
2. 尚未验证 `let/compare/branch` 在真实蓝图中新建 graph 的端到端写入结果。

### 距离期望的差距
1. 需要重启或重载编辑器模块后，重新运行覆盖测试确认 `append_new_owned_graph + 新图名 + let/compare/branch` 从 preview 到 execute 全链路通过。
2. 需要确认成功返回中的 adapter 数据能够证明 `logic_spec/SemanticIR` 是实际节点创建入口，而不是旧 `nodes/links` 兼容路径。

### 阻塞内容
1. 运行态验证依赖编辑器加载新编译后的插件 DLL；如果当前编辑器仍是旧模块，CLI 测试会继续反映旧行为。

## 2026-05-14 覆盖测试二次执行：SemanticIR 新图写入

### 已完成
1. Editor 重启后 runtime diagnostics 通过，Bridge connected，Blocking 为 None。
2. 写入授权重新获取成功。
3. 专用测试资产 `/Game/BP_BH_SemanticCoverageActor` ensure 通过。
4. 旧 `call_function` 用例已不再触发 TaskRuntime 旧 `unsupported_graph_write_statement_kind`，而是在 SemanticIR 阶段报告 `statement_kind_unsupported`，说明链路已进入 SemanticIR。
5. 使用短名 `call` 的 `append_new_owned_graph + 新图名 + let/compare/branch` preview 通过。
6. 使用短名 `call` 的 execute 返回 `applied`，签名步骤对新图名返回 deferred/no-op，GraphWrite 返回 applied，编译返回 succeeded。

### 未完全完成
1. 读回发现新建图 `BH_SemanticCoverage_NewGraph_20260514` 存在，但 `node_count=0`；执行返回成功但真实节点未落图，覆盖测试不能视为通过。
2. 根因定位为 Append 服务把 `logic_spec` 传给旧 AgentImportService，而 AgentImportService 仍只消费 `nodes` 数组；空 `nodes` 导致 no-op 被上层误判为成功。
3. 已改为 `logic_spec` 存在时由 Append 服务直接调用 `FBlueprintGraphGenerationPipeline::GenerateBlueprintFromJson`，绕过旧 AgentImport 节点数组入口；该修复尚未编译通过。

### 距离期望的差距
1. 需要完成编译后重新测试，确认新图真实生成 custom event、branch、compare、PrintString 等节点，并且 readback `node_count > 0`。
2. 需要确认 GraphWrite 成功结果不再允许 `CreatedNodeCount=0` 的 no-op 伪成功。

### 阻塞内容
1. 当前 UE Live Coding active，UBT 编译被阻止：`Unable to build while Live Coding is active`。
2. 需要在编辑器中按 `Ctrl+Alt+F11` 结束 Live Coding，或关闭编辑器后再编译。

## 2026-05-14 AgentImportService SemanticIR-only ���

### �����
1. `FBlueprintHelperAgentImportService` �Ѽ���Ϊ facade���������������ί�� SemanticIR ִ������
2. ���� `FBlueprintHelperAgentImportJsonParser`������ AgentImport JSON schema��Ŀ��ͼ��ѡ��� `logic_spec` У�顣
3. ���� `FBlueprintHelperAgentImportSemanticExecutor`���������Ŀ��ͼ��ͨ�� `FBlueprintGraphGenerationPipeline` ���� SemanticIR ͼд����·����
4. �� `nodes`��`links`��`variables`��`declarations` �ǿ��������� parser ��ܾ�������·������֧�־� AgentImport �ڵ��ʽ��
5. `TextToBlueprintGenerator` ��ֽ����ȷ�ϣ������ pipeline/parser/spawner/linker/default-value/local-variable/utility/multi-graph ������Ƕ��� `.h/.cpp`��facade �ļ������� facade ������ݽṹ/ö�١�

### �����������
1. �ɸ�ʽ��ؽṹ���Ա����� public header �У����ڽ��͵�ǰ��������գ�����ʱ�Ѿ��ܾ��� payload���� header ������δִ�С�
2. `AgentImportService` �� direct dry_run ��ǰֻ�� schema��Ŀ��ͼ�� `logic_spec` ������У�飬��ִ���޸����õ����� SemanticIR Ԥ�ݡ�
3. ���μ���д����ɳ����Ȩд�� `C:\Users\CharlieNotFound\.codex\memories\extensions\ad_hoc\notes` δ��ɣ���Ҫ�û������Ȩ�޻�����д��

## 2026-05-14 AgentImportService SemanticIR-only 拆分

### 已完成
1. `FBlueprintHelperAgentImportService` 已减重为 facade，仅负责解析请求并委托 SemanticIR 执行器。
2. 新增 `FBlueprintHelperAgentImportJsonParser`，负责 AgentImport JSON schema、目标图、选项和 `logic_spec` 校验。
3. 新增 `FBlueprintHelperAgentImportSemanticExecutor`，负责解析目标图并通过 `FBlueprintGraphGenerationPipeline` 进入 SemanticIR 图写入主路径。
4. 旧 `nodes`、`links`、`variables`、`declarations` 非空输入已在 parser 层拒绝，运行路径不再支持旧 AgentImport 节点格式。
5. `TextToBlueprintGenerator.h/.cpp` 已重命名为 `BlueprintGraphWriteFacade.h/.cpp`，类名同步改为 `FBlueprintGraphWriteFacade`，用于承载图写入 facade 与相关数据模型入口。

### 距离期望差距
1. 旧格式相关结构体仍保留在 public header 中，用于降低当前编译面风险；运行时已经拒绝旧 payload，但 header 清理尚未执行。
2. `AgentImportService` 的 direct dry_run 当前只做 schema、目标图和 `logic_spec` 存在性校验，不执行无副作用的完整 SemanticIR 预演。
3. 本次记忆写入因沙箱无权写入 `C:\Users\CharlieNotFound\.codex\memories\extensions\ad_hoc\notes` 未完成，需要用户侧或有权限环境补写。
## 2026-05-14 Source 旧 AgentImport 写入路径清理

### 已完成
1. 移除 `FBlueprintHelperAgentImportParsedRequest` 中旧 `nodes/links/variables/declarations` 数据模型。
2. `append_blueprint_graph` 不再接收或转发旧 `nodes/links`，缺少 `logic_spec` 时直接 preflight 失败。
3. `replace_blueprint_graph` 不再接收或转发旧 replacement `nodes/links`，缺少 `logic_spec` 时直接 preflight 失败。
4. Append/Replace 写入路径不再依赖 `FBlueprintHelperAgentImportService`，统一直接调用 SemanticIR graph generation pipeline。
5. 删除 SafetyTests 中旧 AgentImportGraph legacy nodes/links 成功和回滚用例。

### 距离期望差距
1. Source 中仍有 Review/Transaction/BlockScopedAnchor 等历史数据迁移相关 legacy 代码，本次未删除，避免破坏已有 Review/Journal 兼容读取语义。
2. `FBlueprintHelperAgentImportService` 仍作为 direct SemanticIR import facade 保留；是否完全删除该服务和 bridge command 需要确认是否还需要对外入口。
## 2026-05-14 SemanticIR smoke execute preflight 修复

### 已完成
1. 覆盖测试发现 `append_new_owned_graph` 新图尚不存在时，`reuse_existing_entries=true` 会错误触发 `custom_event_entry_not_found`。
2. 已将该检查收窄为仅在目标图已存在时要求 Custom Event entry 已存在，新图创建路径不再被误阻塞。

### 距离期望差距
1. 仍需重新编译并重跑同一 TaskSpec execute，确认 SemanticIR 实际落图成功。
## 2026-05-14 SemanticIR 唯一 UE 节点创建入口收敛

状态：部分完成，GraphWrite 主写入路径已收敛；签名/结构服务不在本轮迁移范围。

已完成内容：
1. `append_blueprint_graph` / `replace_blueprint_graph` 继续强制 `logic_spec/SemanticIR`，缺少 `logic_spec` 的旧 nodes/links payload 会被拒绝。
2. `BlueprintGraphGenerationPipeline::GenerateBlueprintFromJson` 与 `GenerateNodesAndLinksForGraph` 已删除可达的旧 nodes/links 解析和 `Handler->Spawn` 路径，仅允许 `logic_spec` 进入 `GenerateSemanticGraphFromJsonObject`。
3. `BlueprintMultiGraphGenerationPipeline` 的单图 nodes fallback 改为直接拒绝，避免多图入口绕回旧 nodes/links 创建路径。
4. `merge_blueprint_graph` 插入 call node 改为调用 `FBlueprintHelperGraphStatementBuilder::BuildCallFunctionFragment`，不再由 MergeService 直接 `SpawnResolvedNode` 或 `SpawnFunctionNode`。
5. `merge_blueprint_graph` 的 sequence node 创建改为 `FBlueprintHelperGraphStatementBuilder::BuildSequenceFragment`，MergeService 不再直接 `NewObject<UK2Node_ExecutionSequence>` / `AddNode`。
6. 编辑器 UI 的 unresolved function mapping 直接生成节点入口已禁用，提示必须走 `logic_spec/SemanticIR`。
7. AgentFace TypeScript / Python append bridge payload 不再输出旧 `nodes/links`，普通 append payload 改为只携带 `logic_spec`。

距离期望的差距：
1. `NodeHandlers` 内部仍保留具体 `UK2Node` 实例化代码，但其目标定位已从外部入口迁到 GraphStatementBuilder/fragment 构建内部调用；后续如果要物理消除所有 `NewObject<UK2Node*>` 分散点，需要新增专门 `NodeFragmentMutator` 类并迁移 handler 实现。
2. `patch_blueprint_graph` 主要修改已有节点/连线/default，不负责创建新 `UK2Node`；本轮没有强制它必须携带 `logic_spec`。
3. BlueprintSignatureService 仍会创建 custom event / override event 等签名节点；该服务属于签名/结构入口，不在本轮 GraphWrite 节点创建入口收敛范围。
4. 尚未执行编译验证；需要下一步编译确认本轮 C++/TS/Python 改动无语法问题。
## 2026-05-14 SemanticIR branch smoke 覆盖测试

状态：通过，仍有 resolver warning 待处理。

已完成内容：
1. 使用 `BH_SemanticIR_BranchSmoke_20260514_001` 执行 `let -> compare -> branch -> then/else call` 端到端 smoke。
2. `blueprinthelper_preview_task` 返回 `preview_passed`，产物为 `preview_1778746031719_0001`。
3. `blueprinthelper_execute_task` 返回 `executed`，任务为 `task_AFC403B84D27A0997547D4BCB871E46E`，产物为 `preview_1778746218510_0001`。
4. 执行结果包含 `fragment_debug.fragment_dag` 和 `fragment_evidence`，fragment_count 为 12，覆盖 `statement_let`、`expr_compare`、`statement_branch`、`join`、then/else `statement_call`。
5. 执行后 Blueprint post compile 成功，`warning_count: 0`。
6. `blueprinthelper_read_task_context` 回读确认新增图 `BH_SemanticIR_BranchSmoke_20260514_001` 已存在，节点数为 5。
7. AgentFace TaskCore 全量测试通过：Node 99/99，Python unittest 48/48。
8. TS/Python 测试断言已同步为 `logic_spec`-only payload，不再期待旧 `nodes/links`。

距离期望的差距：
1. Semantic resolver 对 `PrintString` 仍给出 `semantic.target_unverified` warning；UE 写入和编译通过，但 resolver 目标验证仍需补全。
2. 普通 CLI 不暴露低层 `append_blueprint_graph` 直接命令，本轮没有通过普通 CLI 直接测试旧 `nodes/links` Bridge payload 拒绝路径。
3. 本轮覆盖了 append 新 owned graph；replace/merge/patch 的真实编辑器写入回归仍需单独用例覆盖。