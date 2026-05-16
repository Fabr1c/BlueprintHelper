# BlueprintHelper 图语句框架实现进度记录

> 2026-05-14 状态转移：本文中的未达期待、待验证项和阻塞项已迁移到 [BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md](BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md)。本文保留为历史上下文；开放项跟踪迁移完成，后续当前状态以总账为准。
## 当前验收总览：2026-05-14 GraphStatementFramework first-slice 闭环状态
状态更新：2026-05-15 `blueprinthelper_read_context` 完整体已完成并通过真实编辑器 Bridge 覆盖测试。该项不再作为 GraphStatementFramework 或统一上下文入口阻塞项。


状态：当前 first-slice 期望已闭环；2026-05-16 CallFunction K2 上下文、target_object、typed data edge、schema conversion 覆盖已完成，历史章节中被后续修复覆盖的差距不再作为当前阻塞。
已收敛的历史差距：
1. `semantic.target_unverified` 针对 `PrintString` 的误报已通过 Kismet library context 注册修复，并通过真实编辑器 CLI 验证 `target.verified=true`。
2. `semantic.target_unverified` 针对 `/Script/Engine.Actor:K2_SetActorLocation` 的 owner-qualified native path 误报已修复，并通过真实编辑器 CLI 验证 `hasTargetUnverified=false`。
3. `select/make_struct/get_property` 已在 append 路径真实覆盖，包含 `get_property -> compare -> branch.condition`、`select -> PrintString.InString`、`make_struct(Vector) -> K2_SetActorLocation.NewLocation`。
4. `replace_owned_graph` 已切换为 `logic_spec: BlueprintLogicSpec.v2` 成功路径，并通过真实编辑器 CLI 覆盖。
5. `patch_owned_graph` 已真实覆盖 `set_node_comment`、`set_pin_default`、`set_node_position`。
6. `merge_owned_graph` 已真实覆盖 `insert_between`、`branch_fork`、`append_after`。
7. `blueprinthelper_read_context` localized node name / artifact JSON 风险已收敛：真实 artifact 可 `JSON.parse`，CLI artifact 使用 ASCII-safe JSON，C++ 节点显示文本加入控制字符清洗，TS 桥接层拒绝不可解析字符串 payload。
8. C++ ToolResult schema 已从 `BlueprintHelper.McpToolResult.v1` 收敛为 `BlueprintHelper.ToolResult.v1`，并通过重启编辑器后的真实 CLI artifact 验证。
9. AgentFace `execute_task` 外层 `modified` 已从 Bridge 执行结果穿透，CLI summary 不再把真实写入误报为 `modified=false`。
10. append preview 已接入 SemanticIR 生成预检，能提前阻断模糊/失败写入；append execute 失败回滚已修复。
11. `close_editor` 直接 `QUIT_EDITOR` 触发蓝图编辑器 `PreviewScene.GetWorld()` 关闭期断言的风险已修复：关闭前先 `CloseAllAssetEditors()`，再延迟一帧调度退出；已编译并通过 MCP 启动/关闭回归验证。
当前非阻塞限制：
1. `connect_pins/disconnect_link/replace_link` 属于低层 patch 能力，当前 AgentFace first-slice schema 未暴露，不作为本轮期望。
2. user-node anchor 合同尚未设计完成，当前验收范围限定为 BlueprintHelper-owned graph anchor。
3. Review UI 按 function/event/macro 聚合仍需要单独 UI 验收；本轮已验证 fragment/debug/evidence 字段进入执行结果，但未把 UI 交互体验纳入 GraphStatementFramework first-slice 闭环。
4. 目标测试资产仍返回 `warning_count=2`；图写入相关 post compile 成功，本轮未展开处理资产既有 warning。
5. Layout model 仍主要用于 debug/evidence 与 fragment 描述，尚未声明为实际 UE 节点排布驱动器。
## 当前循环验证结论：2026-05-14 close_editor 蓝图编辑器关闭崩溃修复

状态：已修复并验证。

已完成内容：
1. 复现依据：关闭编辑器日志显示 `Cmd: QUIT_EDITOR` 后进入世界清理，并触发 `BlueprintEditor.cpp` 的 `PreviewScene.GetWorld()` 断言。
2. 根因判断：`close_editor` 在蓝图资产编辑器窗口仍存在时直接调度 `QUIT_EDITOR`，可能让 BlueprintEditor 在 PreviewScene 已释放或正在释放时继续处理关闭期逻辑。
3. 修复 `FBlueprintHelperEditorCommandService::CloseEditor`：保存脏包逻辑保持不变；退出前通过 `UAssetEditorSubsystem::CloseAllAssetEditors()` 主动关闭所有资产编辑器。
4. 关闭资产编辑器失败时不再继续退出，返回明确错误，避免半关闭状态继续触发退出命令。
5. `QUIT_EDITOR` 改为通过 `FTSTicker` 延迟 0.25 秒调度，给 Bridge 响应返回和资产编辑器 teardown 留出一帧以上缓冲。
6. MCP `blueprint_close_editor` 返回消息已更新为 `Closed asset editors and scheduled delayed editor shutdown without saving.`，确认走到新路径。
7. UE 编译通过：`TemplateEditor Win64 Development`，`Result: Succeeded`。
8. 通过全局 MCP 启动编辑器并执行关闭回归；编辑器进程正常退出，日志尾部出现 `Editor shut down` / `Log file closed`，未再出现 `PreviewScene.GetWorld()` 断言。

距离期望的差距：
1. 本轮自动回归覆盖了 MCP 启动后的关闭路径；由于 `blueprint_open_asset` 不在当前 CLI 注册面内，未自动打开蓝图资产编辑器窗口做同场景复现。当前修复已在关闭前覆盖所有资产编辑器窗口，后续如需更强覆盖，应增加受控调试入口或恢复只读安全的资产打开测试能力。

阻塞内容：
1. 无当前阻塞；剩余是更强复现覆盖能力，不影响本次 crash 修复闭环。
## 当前循环验证结论：2026-05-14 patch/merge 变体覆盖补齐

状态：已通过真实编辑器 CLI 覆盖。
已完成内容：
1. `patch_owned_graph` 的 `set_pin_default` 变体通过：定位 `nodes[4].B`，将比较右值默认值改为 `-998.0`，结果 `patch_type=set_pin_default`、`changed=true`。
2. `patch_owned_graph` 的 `set_node_position` 变体通过：定位 `nodes[10]`，修改节点位置，结果 `patch_type=set_node_position`、`changed=true`。
3. 两个 patch 变体均显示外层 `modified=true`，内部 step `modified=true`，post compile 成功。
4. `merge_owned_graph` 的 `branch_fork` 变体通过：基于 `nodes[5].then` 和 `links[5]` 插入 `PrintString`，并按 `sequence_order=[inserted_logic, original_successor]` 生成 Sequence。
5. branch_fork 结果包含 `sequence_ref=K2Node_ExecutionSequence_0`，post compile 成功。
6. `merge_owned_graph` 的 `append_after` 变体通过：基于 `K2_SetActorLocation.then` 追加 `PrintString`，结果 `inserted_ref=PrintString`，post compile 成功。
距离期望的差距：
1. `patch_owned_graph` 已覆盖 `set_node_comment`、`set_pin_default`、`set_node_position`，但尚未覆盖更底层的 `connect_pins/disconnect_link/replace_link`；这些不是当前 AgentFace first-slice schema 暴露项。
2. `merge_owned_graph` 已覆盖 `insert_between`、`branch_fork` 和 `append_after` 三种当前 AgentFace 暴露策略；当前无 merge 策略覆盖缺口。
3. 所有覆盖均基于 BlueprintHelper-owned graph anchor，尚未验证未来 user-node anchor 合同。
## 当前循环验证结论：2026-05-14 owner-qualified native target resolver 修复

状态：已编译并通过重启编辑器后的真实 CLI 复测。
已完成内容：
1. 修复 SemanticIR call target resolver 对 `ClassPath:FunctionName` 的解析顺序，优先识别 `/Script/Engine.Actor:K2_SetActorLocation` 这类 native owner-qualified 函数路径。
2. dot 形式 owner/member call 的验证逻辑补充 fallback：当 owner struct 无法解析时，可用 member 函数名或完整 target 在函数上下文中验证，避免纯命名形式误报。
3. UE 编译通过：`TemplateEditor Win64 Development`，`Result: Succeeded`。
4. 重启编辑器后执行真实 TaskSpec 覆盖：`get_property(DefaultSceneRoot.RelativeLocation.X) -> compare -> branch`，then 分支调用 `/Script/Engine.Actor:K2_SetActorLocation`，并使用 `make_struct(Vector)` 连接 `NewLocation`。
5. 执行结果显示 `hasTargetUnverified=false`，SemanticIR evidence 不再出现 `semantic.target_unverified`。
6. post compile 成功：`compile_result.success=true`、`status=succeeded`。
距离期望的差距：
1. 当前验证覆盖了 Actor native owner-qualified function path；其他 native library、component owner 和 plugin class owner 仍建议后续按实际场景补充样例。
2. 编译仍返回 `warning_count=2`，当前判断为目标测试资产既有 warning；本轮未展开处理资产 warning 明细。
## 当前循环验证结论：2026-05-14 patch/merge 覆盖与 read_context 输出风险收敛

状态：已编译并通过重启编辑器后的真实 CLI 复测。
已完成内容：
1. `patch_owned_graph` 真实编辑器覆盖通过：基于 `BH_SelectStructProperty_20260514_215723` 的 owned block anchor，对 `nodes[7]` 执行 `set_node_comment`，preview/execute 均通过。
2. `patch_owned_graph` 执行结果显示内部 `bridge_result.modified=true`、`modified_assets=1`、`patch.changed=true`，post compile 成功。
3. `merge_owned_graph` 真实编辑器覆盖通过：基于 `nodes[7].then -> nodes[12].execute` 的 `links[7]` 执行 `insert_between`，插入 `PrintString`，preview/execute 均通过。
4. `merge_owned_graph` 执行结果显示内部 `bridge_result.modified=true`、`modified_assets=1`、`inserted_ref=PrintString`，post compile 成功。
5. 修复 AgentFace `execute_task` 外层 `ToolResult.modified` 未从 Bridge 执行结果穿透的问题，并新增 `task-spec-runner.regression.test.ts` 覆盖。
6. 将 C++ 公共 ToolResult schema 从 `BlueprintHelper.McpToolResult.v1` 收敛为 `BlueprintHelper.ToolResult.v1`，避免 CLI artifact 内层结果继续残留 MCPToolResult 命名。
7. `blueprinthelper_read_context` 真实读取 `logic_json` artifact 已可被 Node `JSON.parse` 解析，中文 localized node name 通过 JSON artifact 路径保持可解析。
8. 新增 `LogicGroupBuilder` 节点显示文本清洗：节点名/owner 去除控制字符并限制长度，不改变 `node_ref/link_ref/node_path` 等结构性 anchor 字段。
9. 强化 AgentFace `read_context` Bridge payload 形态约束：字符串 payload 必须是可解析 JSON 对象，否则返回标准 `invalid_read_context_payload`，不再模糊透传。
距离期望的差距：
1. C++ schema 常量、节点文本清洗、外层 modified 穿透已通过编译与真实 CLI 复测；当前无该项剩余差距。
2. `patch_owned_graph` 和 `merge_owned_graph` 已覆盖最小真实路径，但还未覆盖 `branch_fork`、`set_pin_default`、`set_node_position` 等变体。
3. `merge_owned_graph` 仍属于现有 merge service 的插入型变更路径，不是 statement tree -> fragment DAG 的完整新逻辑生成路径；当前验收目标是 GraphWrite 四策略均可经 TaskSpec 主链路落地。
4. Review UI 按 function/event/macro 聚合仍未在真实 UI 中验收；当前只验证了 fragment/debug/evidence 字段和图写入结果。
5. `/Script/Engine.Actor:K2_SetActorLocation` 的 `semantic.target_unverified` warning 仍需后续补 owner-qualified native path resolver。

## 当前循环验证结论：2026-05-14 select/make_struct/get_property 覆盖与 append preview 校验修复

状态：已修复并验证。

已完成内容：
1. 新增 append dry-run 语义生成预检：`append_new_owned_graph` preview 不再只做 TaskPlan/Preflight 校验，会在可回滚路径中执行一次 SemanticIR -> UE 节点生成。
2. 修复 append execute 失败回滚：SemanticIR 生成失败时，新建图会整图删除，既有图会移除本次新增节点，避免半成品 custom event 残留导致后续蓝图编译重复函数名。
3. 复测模糊目标 `SetActorLocation`：preview 已能提前返回 `semantic_graph_write_failed`，不再等到 execute 阶段才失败。
4. 使用精确目标 `/Script/Engine.Actor:K2_SetActorLocation` 重新执行真实覆盖。
5. 真实覆盖图：`BH_SelectStructProperty_20260514_215723`，事件：`BH_CodexSelectStructProperty_20260514_215723`。
6. 覆盖表达式链路：`get_property(DefaultSceneRoot.RelativeLocation.X) -> compare -> branch.condition`。
7. 覆盖表达式链路：`select(bool, string, string) -> PrintString.InString`。
8. 覆盖表达式链路：`get_property(DefaultSceneRoot.RelativeLocation.X) -> make_struct(/Script/CoreUObject.Vector) -> K2_SetActorLocation.NewLocation`。
9. 执行结果包含 `fragment_debug.fragment_dag`，其中 `data_edges=13`、`exec_edges=5`、`fragment_count=18`。
10. post compile 已执行并成功：`compile_result.success=true`、`status=succeeded`、`warning_count=2`。
11. UE 编译通过：`TemplateEditor Win64 Development`，`Result: Succeeded`。

距离期望的差距：
1. `/Script/Engine.Actor:K2_SetActorLocation` 当前仍在 SemanticIR evidence 中出现 `semantic.target_unverified` warning；CallFunctionResolver 可以成功落地，但 SemanticIR context 尚不能验证 owner-qualified native path。
2. 本轮覆盖了 append 路径中的 `select/make_struct/get_property`，尚未在 replace/merge/patch 路径做同等组合覆盖。
3. append dry-run 语义预检已能捕捉生成失败，但当前实现会在真实蓝图上做可回滚临时生成；后续可评估是否改为 transient duplicate Blueprint，以进一步降低预览副作用风险。
4. `validation_policy` 是无效旧字段，正确字段为 `validation.should_compile/should_save`；本轮已记录到 CLI Tips。
5. Review evidence 仍显示 `unknown:unknown` scope，function/event/macro 聚合 UI 仍未真实验收。

## 当前循环验证结论：2026-05-14 replace_owned_graph SemanticIR 成功路径修复

状态：已修复并验证。

已完成内容：
1. 修复 TypeScript GraphWrite 编译器：`replace_owned_graph` 不再输出旧 `replacement.nodes/links`，改为输出 `logic_spec: BlueprintLogicSpec.v2`。
2. 修复 Python GraphWrite 编译器：CLI 默认 Python 编译链已同步输出 `logic_spec`，并将旧输入 `call_function/name`、`set_member_variable/name` 规范化为短名 `call/target`、`set/target`。
3. 修复 UE TaskRuntime replace 适配层：`replace_body` structural op 读取 `logic_spec` 并转发给 `replace_blueprint_graph`，不再要求旧 `replacement`。
4. 同步 TypeScript、Python 和 fixture 测试期望，确保短名 SemanticIR 是 TaskPlan/Bridge payload 成功路径。
5. AgentFace task-core 全量测试通过：Node 101/101，Python unittest 48/48。
6. 工作区 CLI build 通过：`npm.cmd --prefix AgentFaceService\cli run build`。
7. UE 编译通过：`TemplateEditor Win64 Development`，`Result: Succeeded`。
8. 真实编辑器覆盖通过：先用 `append_new_owned_graph` 创建 `BH_ReplaceRegression_20260514_213405`，再用 `replace_owned_graph` 替换 `BH_CodexReplaceBase_20260514_213405` 自定义事件体。
9. replace preview 已通过，不再出现 `logic_spec_required` 或 `GraphWrite structural op requires logic_spec object`。
10. replace execute 已通过，返回 `status=completed`，Bridge step `adapter_operation=replace_blueprint_graph`，结果包含 `fragment_debug.fragment_dag`、`data_edges`、`fragment_evidence`。
11. 已按闭环流程关闭编辑器、编译、重新启动编辑器，并确认 Bridge 可用。

距离期望的差距：
1. `replace_owned_graph` 的最小真实成功路径已闭环，但 `patch_owned_graph` 和 `merge_owned_graph` 仍需同级真实编辑器覆盖。
2. 当前 replace 覆盖使用单条 `call PrintString`，尚未覆盖 `branch/select/make_struct/get_property` 在 replace 路径中的组合写入。
3. `fragment_evidence.review_scopes` 当前仍出现 `unknown:unknown`，Review UI 按 function/event/macro 图体聚合的真实体验仍需单独验证。
4. append/replace 结果内仍显示低层 `adapter_operation`，这是运行时执行记录，不影响普通 TaskSpec 成功路径；后续若要完全隐藏低层语言，需要另做结果裁切/展示策略。
5. 本轮发现两个 CLI 使用层问题并已写入 Tips：Windows PowerShell `Set-Content -Encoding utf8` 会写 BOM 导致 JSON parse error；全局 `bh.cmd` 可能滞后于工作区源码，开发验证应优先使用工作区 CLI build。

## 当前循环验证结论：2026-05-14 PrintString semantic resolver 修复

状态：已修复并验证。

已完成内容：
1. 通过真实 CLI TaskSpec 复现 `PrintString` 写入成功但 SemanticIR 报 `semantic.target_unverified` 的问题。
2. 根因定位为 SemanticIR context 仅登记蓝图自身、父类、组件和变量成员，未登记常用 Blueprint library 函数；实际节点生成阶段的 CallFunctionResolver 能解析 `PrintString`，但 SemanticIR 预验证阶段不能验证该目标。
3. 已在 `FBlueprintHelperGraphSemanticContext::FromBlueprint` 中登记常用 Kismet library 函数。
4. 已同步登记函数 display name，避免 `Print String` 这类显示名在 semantic context 中继续误报。
5. UE 编译通过：`TemplateEditor Win64 Development`，`Result: Succeeded`。
6. 重启编辑器后重新执行 PrintString 最小 TaskSpec，通过真实写入验证 `target.verified=true`。
7. 复测结果中 `fragment_debug.fragment_dag.diagnostics=[]`，`fragment_evidence.diagnostics=[]`。

距离期望的差距：
1. 本轮仅登记 `UKismetSystemLibrary` 和 `UKismetMathLibrary` 两类常用 Blueprint library；其他 library 例如 GameplayStatics 是否纳入，需要后续基于实际场景决定。
2. SemanticIR 仍只是“目标可被上下文识别”的预验证，不替代最终 CallFunctionResolver 的唯一性、图兼容性和歧义判断。
3. 尚未为 SemanticIR context 单独补 C++ 自动化测试；本轮以真实编辑器 CLI 覆盖验证为准。

## 当前循环验证结论：2026-05-14 read_task_context 资产存在性修复

状态：已修复并验证。

已完成内容：
1. 已按闭环流程写入记忆：补全文档期望内容、启动编辑器、CLI 测试、发现问题、分析问题、修复、关闭编辑器、编译、继续补全文档。
2. 已新增 CLI Tips 文档，用于记录非插件代码导致的错误，例如错误 CLI 参数、PowerShell 语法、编码和本地命令调用问题。
3. 已通过全局 MCP 启动编辑器并确认 Bridge 可用。
4. 复测确认真实资产 `/Game/BP_BH_SemanticCoverageActor` 的 `asset_info` 已规范化为 `/Game/BP_BH_SemanticCoverageActor.BP_BH_SemanticCoverageActor`、`name=BP_BH_SemanticCoverageActor`、`class=Blueprint`。
5. 复测发现缺失资产 `/Game/BlueprintHelper/Smoke/BP_TaskSpecSmoke` 仍被 `read_task_context` 误报为 `exists=true`，根因是 AgentFace 将 Bridge 返回的空 `asset_info={}` 当作有效资产。
6. 已修复 AgentFace `buildTaskContextPack`：只有包含有效 `path/name/class` 的 asset info 才会被认定为存在资产。
7. 已新增 `task-context.regression.test.ts`，覆盖空 `get_asset_info` 结果不得被视为存在资产，以及有效 asset info 应保持存在资产两条路径。
8. AgentFace 全量测试通过：Node 101/101，Python unittest 48/48。
9. CLI 复测通过：缺失资产返回 `exists=false`；真实资产返回 `exists=true` 且保留规范化 `asset_info`。
10. 已通过 MCP 关闭编辑器并保存脏资源。
11. UE 编译通过：`TemplateEditor Win64 Development`，`Result: Succeeded`。

距离期望的差距：
1. UE Bridge 的 `get_asset_info` 对缺失资产仍可能返回空对象；本轮是在 AgentFace 层收敛为空对象无效，后续可进一步让 UE 侧直接返回明确 Bridge error 或 `exists=false`。
2. `PrintString` 仍存在 `semantic.target_unverified` resolver warning，尚未在本轮处理。
3. `request_write_session` pending approval/timeout 状态精确化尚未处理；当前 AutoRepair 已绕过弹窗，但普通档位仍需独立验证。
4. replace/merge/patch 的完整真实编辑器回归仍需继续覆盖。
5. Review UI 按 function/event/macro 聚合后的真实审核体验尚未在本轮验证。

日期：2026-05-13

关联设计文档：`BlueprintHelper_GraphStatementFramework_Design_20260513_CN.md`

## 当前覆盖测试结论：2026-05-14 SemanticIR branch/compare smoke

状态：通过。`BH_SemanticIR_BranchSmoke_20260514_004` 已验证 `let -> compare -> branch.condition` 的端到端写入、data edge 消费和蓝图编译。

已通过部分：
1. `bh.cmd` 连接重启后的 Editor/Bridge 成功，`blueprint_get_runtime_profile` 返回 completed，`127.0.0.1:54321` 可连接。
2. `blueprinthelper_preview_task` 使用 `BlueprintLogicSpec.v2`、短名 `let/compare/branch/call` 通过 preview，产物为 `preview_1778748919479_0001`。
3. `blueprinthelper_request_write_session` 返回 completed，产物为 `cli_1778748950718`。
4. `blueprinthelper_execute_task` 成功写入 `/Game/BlueprintHelperCliSmoke/BH_PhysicsDoor_20260513/BP_BH_PhysicsDoorActor`，任务为 `task_F48444134EA5A67A0F29699A8C402825`。
5. 写入结果创建图 `BH_SemanticIR_BranchSmoke_20260514_004` 和 custom event `BH_CodexBranchSmoke_20260514_004`，GraphWrite 状态为 applied。
6. `blueprinthelper_read_context` 回读目标图，确认 `ReturnValue -> Condition` data link 已真实写入，`data_links: 1`、`orphan_nodes: 0`、`exec_links: 3`、`nodes: 5`。
7. 执行后的 post compile 返回 `success: true`、`status: succeeded`、`warning_count: 0`。
8. 执行产物包含 `fragment_debug.fragment_dag`、`data_edges`、`fragment_evidence`，说明 SemanticIR/fragment evidence 已进入真实写入结果。

本次修复：
1. 修复 SemanticIR UE 节点生成 fragment id 与 FragmentDAG fragment id 不一致导致 `FilterSemanticDataEdges` 丢弃 data edge 的问题。
2. 修复 GraphComposer 对 `result/value/return` 这类语义 output pin 的解析不足问题，并增加受限 data pin fallback 连接。
3. 修复 `K2Node_PromotableOperator` 在默认值比较场景下 A/B 输入仍为 wildcard，导致蓝图编译失败的问题；现在创建后按目标 UFunction 固定输入/输出 pin 类型再应用默认值。
4. 修复后已重新编译 `TemplateEditor Win64 Development`，结果成功，并通过 `004` 编辑器写入 smoke。

距离期望的差距：
1. 本次真实写入已覆盖 `let/compare/branch/call` 的最小链路，但尚未端到端覆盖 `select/make_struct/get_property`。
2. `PrintString` 在 Semantic resolver 中仍给出 `semantic.target_unverified` warning；UE 节点写入和编译通过，但 resolver 目标验证还不完整。
3. `blueprinthelper_read_context` 产物中 localized node name 仍存在 mojibake/非法 JSON 风险，PowerShell `ConvertFrom-Json` 不能稳定解析完整 artifact；本次通过文本字段确认关键统计和连线。
4. 低层 `append_blueprint_graph` 仍作为 adapter_operation 出现在结果中；当前验证证明普通 CLI TaskSpec 路径已进入 SemanticIR/FragmentDAG 写入链路，但 replace/merge/patch 的同等覆盖仍需单独验证。
5. Review evidence 已随结果输出，但 Review UI 中按 function/event/macro 图体聚合后的实际审核体验还未在本轮确认。
## 当前进度总览：2026-05-14 SemanticIR 主路径化同步

### 阶段状态调整
1. SemanticIR -> fragment DAG 的 data edge emission 已进入 GraphGenerationPipeline 主执行路径：payload 中存在 `logic_spec` 时，会现场构建 SemanticIR 和 FragmentDag，并在节点生成后通过 GraphLinker/GraphComposer 连接可落地 data edge。
2. UE 节点生成侧的 statement/expression fragment id 已与 FragmentDAG id 规则对齐，`FilterSemanticDataEdges` 不再丢弃 `compare.result -> branch.condition` 这类 data edge。
3. `compare` 的最小 typed 写入链路已通过真实编辑器验证：`K2Node_PromotableOperator.ReturnValue` 成功连接到 `K2Node_IfThenElse.Condition`。
4. `K2Node_PromotableOperator` 创建后会基于目标 UFunction 固定输入/输出 pin 类型，再应用默认值，避免 wildcard 输入在蓝图编译阶段失败。
5. fragment data edge 消费已验证走通 `statement tree -> fragment DAG -> composer/linker -> UE pin link` 的主数据连线路径；exec edge 仍主要沿现有语句编排/显式 pin 连接路径处理。
6. `get_property` resolver 已补充无类型 fallback：当 `TargetStructs` 未命中时，会基于目标类型名通过 `UObjectIterator` 查找 `UStruct/UClass`，再解析 property path。

### 已验证结果
1. UE 编译通过：`TemplateEditor Win64 Development`，`Result: Succeeded`。
2. `BH_SemanticIR_BranchSmoke_20260514_004` 真实编辑器写入通过，execute 状态为 `executed`。
3. 回读 `logic_json` 确认 `nodes: 5`、`exec_links: 3`、`data_links: 1`、`orphan_nodes: 0`。
4. 回读连线确认 `from_pin: ReturnValue` -> `to_pin: Condition`。
5. 蓝图 post compile 返回 `success: true`、`status: succeeded`、`warning_count: 0`。
6. AgentFace TypeScript build 通过：`npm.cmd --prefix .\AgentFaceService\task-core run build`。
7. AgentFace Python compileall 通过：`python -m compileall -q .\AgentFaceService\task-core\python`。

### 距离期望的差距
1. 本轮只真实验证了 `let/compare/branch/call` 最小链路；`select/make_struct/get_property` 仍需同级别编辑器写入覆盖。
2. exec edge 尚未完整切换到 `FragmentDag.ExecEdges` 的统一消费路径。
3. 复杂 `compare` 的最终能力仍受 UE 可解析函数/PromotableOperator 支持范围限制，自定义结构体、容器、枚举、对象比较还没有完整 typed compare resolver。
4. `blueprinthelper_read_context` artifact 中 localized node name 的编码/转义仍有问题，会影响 JSON 解析型自动验收。
5. Review evidence 已具备 fragment 字段承载能力，但按 function/event/macro 图体聚合后的实际 UI 审核体验仍需要真实蓝图写入场景验证。
6. Layout model 目前主要用于 debug/evidence 与 fragment 描述，尚未完整驱动实际 UE 节点排布。
7. Pattern Registry 的 JSON 数据驱动 alias / pin mapping / 类型转换扩展面还没有完全固化为稳定配置契约。
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
## 2026-05-14 快速修复：写会话批准提醒

状态：已修复，待编译和重启编辑器后验证。

完成内容：
1. `request_write_session` 授权弹窗出现前会触发 BlueprintHelper 编辑器通知，提示当前正在等待写入批准。
2. Windows 平台会同步触发任务栏闪烁和系统提示音，降低用户错过批准弹窗的概率。
3. 授权弹窗文案从旧的 `MCP write access` 改为通用 `BlueprintHelper write access`，符合 CLI-only 工具口径。

距离期望差距：
1. 当前实现是编辑器通知 + Windows 任务栏提醒，不是 Windows Action Center 原生 toast。
2. 需要编译并重启编辑器后，通过 `blueprinthelper_request_write_session` 实测提醒是否可见。
## 2026-05-14 修复：make_struct Vector 原生结构编译失败

状态：已修复，待编译和重启编辑器后验证。

发现问题：
1. SemanticIR 完整覆盖测试中，`make_struct` 使用 `/Script/CoreUObject.Vector` 时 preview 通过，但 execute 后蓝图编译失败。
2. UE 日志显示：`结构 Make Vector 并非蓝图类型`。

分析结论：
1. 当前 `make_struct` builder 对所有结构体都生成 `K2Node_MakeStruct`。
2. `Vector` 这类 UE 原生数学结构应走 KismetMathLibrary 构造函数，而不是 `K2Node_MakeStruct`。

完成内容：
1. `make_struct` 遇到 `Vector`、`FVector` 或 `/Script/CoreUObject.Vector` 时，改为生成 `/Script/Engine.KismetMathLibrary:MakeVector` 调用片段。
2. 为该调用片段补充 `value` 输出别名，保持 `statement tree -> fragment DAG -> data edge` 的既有连接语义。

距离期望差距：
1. 当前只修复 `Vector`，`Rotator`、`Transform`、`Vector2D` 等其他原生结构还未建立统一 alias 表。
2. 需要编译、重启编辑器并重跑完整覆盖测试确认。
## 2026-05-14 覆盖测试：SemanticIR 完整表达式链路

状态：通过，存在非阻塞问题。

测试环境：
1. 用户重新启动编辑器后，CLI `blueprint_get_runtime_profile` 返回 completed，Bridge 可用。
2. 写会话最终生效，runtime profile 不再报告 `write_permission.disabled`。

完成内容：
1. 在真实资产 `/Game/BP_BH_SemanticCoverageActor` 上执行变量 TaskSpec，创建 `BH_CodexVector_20260514_190820`，preview 和 execute 均通过。
2. 执行图写入 TaskSpec，创建 `BH_SemanticIR_FullSmoke_20260514_190820` 和 custom event `BH_CodexFullSmoke_20260514_190820`，preview 和 execute 均通过。
3. 覆盖链路包含 `make_struct(Vector)`、`set`、`get_property(Vector.X)`、`compare(>)`、`branch.condition`、`compare(==)`、`select`、then/else `call(PrintString)`。
4. 执行结果包含 `fragment_debug.fragment_dag` 与 `fragment_evidence`，`fragment_count=19`、`evidence_fragment_count=19`。
5. 回读 `read_task_context` 确认新图 `BH_SemanticIR_FullSmoke_20260514_190820` 存在，节点数为 13。
6. 蓝图编译后置操作通过，`compile_result.status=succeeded`。

距离期望差距：
1. `read_task_context.target.asset_info` 返回异常：`path` 被拼成 `/Game/BP_BH_SemanticCoverageActor./Game/BP_BH_SemanticCoverageActor`，`name` 为完整路径，`class` 为 `Package`，需要修正资产信息规范化。
2. 之前对不存在的 `/Game/BlueprintHelper/Smoke/BP_TaskSpecSmoke`，`read_task_context` 曾返回 `exists=true`，需要修复资产存在性误报。
3. `fragment_evidence.diagnostics` 中 `PrintString` 仍被 semantic resolver 标记为 `semantic.target_unverified`，说明 resolver 还未把常用库函数验证结果和后续 function resolver 对齐。
4. `request_write_session` 在等待用户批准期间曾返回 `bridge_unavailable`，但随后 runtime profile 显示写权限已生效；需要让 CLI 对 pending approval/timeout 给出更准确状态。
5. 编译结果仍有 2 个 warning，当前未展开 warning 明细。
## 2026-05-14 修复：read_task_context 资产存在性与 asset_info 规范化

状态：修复中，第一轮验证发现缺失资产仍误报，已补二次修复，待重新编译和重启编辑器后验证。

发现问题：
1. `read_task_context` 对不存在的 `/Game/BlueprintHelper/Smoke/BP_TaskSpecSmoke` 曾返回 `exists=true`。
2. `read_task_context` 对真实资产 `/Game/BP_BH_SemanticCoverageActor` 的 `asset_info` 返回异常：`path` 被拼接为重复路径，`name` 是完整路径，`class` 为 `Package`。

分析结论：
1. AgentFace `buildTaskContextPack` 将 `get_asset_info` 的 Bridge 失败对象也当作有效 asset，因此产生存在性误报。
2. UE `get_asset_info` 直接把 `/Game/AssetName` 传给 AssetRegistry object path 查询，可能解析到 package 而不是 blueprint asset。

完成内容：
1. AgentFace `task-context.ts` 新增 Bridge error result 判断，`get_asset_info` 失败时 `target.exists=false` 且不再填充错误对象为 `asset_info`。
2. UE `FBlueprintHelperAssetBrowseService::GetAssetInfo` 新增 object path 规范化：`/Game/AssetName` 会优先按 `/Game/AssetName.AssetName` 查询，再 fallback 原路径。
3. 第一轮验证发现 fallback 原路径仍会把缺失 package path 解析成 `Package`，因此已移除 fallback，只按规范化 object path 查询；缺失资产应返回 Bridge error。

距离期望差距：
1. 需要重新构建 AgentFace task-core/CLI，并编译 UE 插件后验证。
2. 尚未为该问题补单元测试或自动化覆盖。

## 2026-05-15 ReadContext 完整体闭环

状态：已完成。

完成内容：
1. AgentFace `blueprinthelper_read_context` 从 Blueprint logic 单一路径扩展为统一上下文入口。
2. 支持 asset、component、variable/event dispatcher、graph/blueprint logic、widget tree/widget property、data table/row、object/data asset property。
3. 非 logic 读取统一封装为 `ReadContextPack.v1`，具体 payload 保持短 schema。
4. 变量读取兼容底层 `member_variables` 字段，`target_name` 过滤后统计正确。

验证记录：
1. asset_context: AssetContext.v1 completed
2. component_context: BlueprintComponent.v1 completed, components=4, root_components=1
3. variable_context: ReadMemberVariables.v1 completed, variables=1 after target_name filter
4. graph_context: LogicJson.v1 completed, nodes=2, exec_links=1
5. data_table_context: DataTableContext.v1 completed, rows=1, columns=4
6. data_table_row_context: DataTableContext.v1 completed, row_names=JsonNumberBool, rows=1
7. object_property_context: ObjectPropertyContext.v1 completed, properties=1 after target_name filter
8. data_asset_context: DataAssetContext.v1 completed against DA_RC_DataAsset, properties=0 because fixture class has no custom fields
9. widget_context: WidgetContext.v1 completed, widgets=2
10. widget_property_context: WidgetPropertyContext.v1 completed, properties=40 for TitleText

编译记录：
1. `AgentFaceService/task-core` build 通过。
2. `AgentFaceService/cli` build 通过。

距离期望差距：无当前阻塞；DataAsset fixture 无自定义属性导致 `properties=0`，不影响链路完成结论。
阻塞内容：无。


## 2026-05-15 AgentFace tool-surface 拆分

状态：已完成。

完成内容：
1. `bridge-tool-handlers.ts` 已减重为兼容 facade，只保留 `bridgeCommandByToolName`、`bridgeToolSchemas`、`executeBridgeTool` 三个既有导出，外部 registry 调用方式不变。
2. 新增 `tool-surface/bridge/bridge-tool-command-map.ts`，集中维护 CLI tool name 到 UE Bridge command 的映射。
3. 新增 `tool-surface/bridge/bridge-tool-schemas.ts`，集中维护 Bridge-facing tool 的 Zod schema。
4. 新增 `tool-surface/bridge/bridge-tool-dispatcher.ts`，负责 read_context、write_session、generic bridge tool 的分发。
5. 新增 `tool-surface/bridge/generic-bridge-tool-handler.ts` 和 `write-session-handler.ts`，将通用 Bridge 调用和写会话特殊处理从 read_context 业务中剥离。
6. 新增 `tool-surface/bridge/read-context/` 子目录，按 schema、target、route builder、payload postprocess、handler 拆分 read_context 业务。

验证记录：
1. `AgentFaceService/task-core` 执行 `npm.cmd run build` 通过。
2. `AgentFaceService/cli` 执行 `npm.cmd run build` 通过。

距离期望差距：当前仅完成 Bridge/read_context 侧拆分；`local-tool-handlers.ts` 仍包含本地进程、编辑器启动/关闭、诊断、AgentGuide 读取等多类职责，后续如果继续按同一标准，需要再拆 local tool surface。
阻塞内容：无。

## 2026-05-15 tool-surface 完全解耦记录
- 状态：完成。
- 完成内容：tool-surface 顶层 ridge-tool-handlers.ts、local-tool-handlers.ts、	ask-tool-handlers.ts、	ool-registry.ts 均已降级为兼容 facade。
- 完成内容：Bridge/read_context、本地工具、任务工具、注册层均拆成 schema/handler/dispatcher/source/builder 职责边界。
- 完成内容：本地工具拆分为 AgentGuide、Diagnostics、BuildProject、ProjectFileResolver、ProcessRunner、EditorLifecycle 子模块。
- 完成内容：任务工具拆分为 ReadReferenceContext schema、Task schemas、Context handlers、Execution handlers、Dispatcher。
- 完成内容：注册层拆分为 ToolMeta、ToolSource 接口、task/local/bridge ToolSource、ToolSource 列表、handler router、registry builder。
- 验证：AgentFaceService/task-core npm.cmd run build 通过；AgentFaceService/cli npm.cmd run build 通过。
- 距离期望差距：当前 tool-surface 中间层已完成解耦；未发现剩余拆分阻塞。
- 阻塞内容：无。
## 2026-05-16 进度：CallFunction 通用 UE ActionDatabase 解析路径

状态：已完成本轮实现并通过构建验证。

已完成内容：
1. `callfunction` 候选解析从单纯扫描 `UFunction` 扩展为优先使用 UE `FBlueprintActionDatabase`。
2. 接入 `FBlueprintActionFilter`，按当前 `UBlueprint` 与 `UEdGraph` 过滤可用 `UK2Node_CallFunction` action。
3. resolver candidate 保存 `UBlueprintNodeSpawner`，执行创建节点时优先走 UE spawner 路径，fallback 才走旧 `UK2Node_CallFunction + SetFromFunction`。
4. 移除 `Break Vector / Make Vector` 的局部硬编码排序补丁，改为通用 `category_priority` 排序加权。
5. `search_mode`、`ambiguity`、`category_priority` 已从 SemanticIR statement/expression 解析贯通到 call fragment 构建。
6. `candidate_functions` ambiguity message 保持按目标函数分组格式，便于 Agent 在不唯一时回填稳定候选。
7. `make_struct` 移除 Vector 专用 `MakeVector` 创建路径，回到通用 struct operation builder。
8. `reuse_existing -> reuse_if_exists` 兼容 alias 已在 TS/Python/C++ 三侧保留，模板应改用规范值 `reuse_if_exists`。

验证结果：
1. `npm.cmd --prefix .\AgentFaceService\task-core run build` 通过。
2. `npm.cmd --prefix .\AgentFaceService\cli run build` 通过。
3. `Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload` 通过。

距离期望差距：
1. 尚未运行真实编辑器覆盖测试确认复杂函数搜索场景的排序与 UE 菜单完全一致。
2. 多 call TaskSpec 的 `candidate_functions` 当前在单 resolver message 中分组，尚未由 preview 汇总层聚合成跨目标数组。
3. typed pin compatibility 评分尚未完整接入候选 pin 级别的类型约束，只完成了 SemanticIR 层的基础类型校验和通用 category priority。

阻塞内容：
1. 无当前编译阻塞；后续需要编辑器端覆盖测试与 preview 汇总层实现。
## 2026-05-16 进度：CallFunction 候选函数结构化返回闭环

时间：2026-05-16 13:36:20

新增内容：
1. 完成 UE dry_run issue 的结构化 candidate_functions 字段输出。
2. 完成 AgentFace TaskIssue 扩展字段透传，避免 Agent 从 message 文本解析候选函数。
3. 保持通用 UE ActionDatabase/Filter/Spawner 解析路径，不对 Break Vector / Make Vector 做片面特判。

修复内容：
1. 修复 candidate_functions 仅作为 message 内嵌 JSON 字符串出现的问题。
2. 修复测试 TaskSpec 中 Graph 名与 Custom Event 名相同导致 execute 编译失败的用例问题。

变更需求：
1. 候选函数返回按目标函数分组，返回形状为 candidate_functions: [{ target, candidates }]。

快速修复：
1. TS task-core build 通过。
2. CLI build 通过。
3. UE TemplateEditor Win64 Development 编译通过。

验证结果：
1. create_asset Actor Blueprint execute 成功。
2. Print String callfunction preview 成功。
3. Print String callfunction execute 成功。
4. Break 模糊查询 preview 被正确 blocked，并返回结构化 candidate_functions 数组。

距离期望差距：
1. 当前已达到本轮关于通用 CallFunction 解析和候选函数结构化返回的核心期望。
2. 多目标、多 call 同一 TaskSpec 的批量候选聚合尚未做压力测试；当前结构已支持继续扩展。

阻塞内容：
1. 无。
## 2026-05-16 进度：close_editor PreviewScene 崩溃修复

时间：2026-05-16 13:41:05

新增内容：
1. close_editor 关闭策略从延迟 QUIT_EDITOR 调整为延迟 CLOSE_SLATE_MAINFRAME。
2. 关闭命令延迟从 0.25 秒调整为 0.75 秒，确保 Bridge 响应返回后再进入 MainFrame 关闭流程。

修复内容：
1. 修复 close_editor 仍可能触发 BlueprintEditor PreviewScene.GetWorld() 断言崩溃的问题。
2. 关闭路径不再直接进入 UUnrealEdEngine::CloseEditor()，改为让 MainFrame/Slate 的 CanCloseManager 先处理资产编辑器 Tab teardown。

变更需求：
1. MCP 仍保留编辑器生命周期职责；普通 BlueprintHelper 操作继续走 CLI。

快速修复：
1. TS task-core build 通过。
2. CLI build 通过。
3. UE TemplateEditor Win64 Development 编译通过。

验证结果：
1. MCP lueprint_open_editor 启动成功并等待 Bridge 可用。
2. MCP lueprint_close_editor(save_all=true) 返回成功。
3. 关闭命令返回 8 秒后未发现 UnrealEditor.exe 残留进程。

距离期望差距：
1. 当前 close_editor 基础开关编辑器验证通过。
2. 尚未覆盖“打开多个 BlueprintEditor/ReviewPanel 子窗口后关闭”的压力场景；如果再次出现断言，需要进一步把关闭动作拆为显式关闭资产编辑器 Tab + MainFrame close 的分阶段状态机。

阻塞内容：
1. 无当前阻塞。
## 2026-05-16 进度：typed pin model 继续约束 CallFunction 候选

时间：2026-05-16 14:54:29

新增内容：
1. CallFunction 候选函数从字符串升级为结构化对象返回。
2. resolver 请求增加参数类型、pin 类型、target object 类型约束。
3. SemanticIR call 路径把表达式类型与 target 类型下沉到 CallFunction resolver。

修复内容：
1. ActionDatabase 已过滤通过的函数不再被二次 graph compatibility 误挡。
2. typed target object 存在时，候选扫描收敛到目标类继承链，降低 member call 的全量扫描风险。
3. world context、latent、pure/callable、argument compatibility、target object compatibility 过滤进入主路径。

变更需求：
1. candidate_functions[].candidates 当前期望为结构化对象数组，不再是旧的字符串数组。

快速修复：
1. 无。

验证结果：
1. UE 构建已执行；CallFunction resolver 源文件编译动作未报错。
2. smoke 创建 Actor Blueprint 与 StaticMeshComponent 成功。
3. smoke preview 确认 candidate_functions 已返回结构化候选对象。

距离期望差距：
1. 新 DLL 未能完成全流程重启复测，因为全量编译被 Review 系统无关错误阻断。
2. PrintString、typed target object member call 需要在 Review 编译错误修复后重跑。
3. typed pin 的容器/ref/const 细粒度评分仍待后续补齐。

阻塞内容：
1. Review 系统文件 BlueprintHelperReviewGraphBounds.cpp 编译失败，不属于本任务可修改范围。
## 2026-05-16 进度：GraphWrite 去旧兼容与新架构主路径化

时间：2026-05-16 15:10:25

新增内容：
1. CallFunction 创建入口收敛到 ActionDatabase -> BlueprintActionFilter -> UBlueprintNodeSpawner。
2. out_links 与 graph-level links 使用同一 link 分类规则。

修复内容：
1. 修复 out_links data edge 被默认当成 exec edge 的风险。
2. 修复 typed target pin-only 情况会误清空 ActionDatabase 候选的问题。
3. 修复候选函数 message JSON 控制字符转义不足的问题。

变更需求：
1. 移除旧 FBlueprintGraphNodeSpawner::SpawnFunctionNode。
2. 移除 SpawnResolvedNode 的旧 SetFromFunction fallback，不再做旧 Agent/旧写图兼容。

快速修复：
1. 无。

验证结果：
1. UE C++ 编译通过并完成 UnrealEditor-BlueprintHelper.dll 链接。

距离期望差距：
1. 尚未运行编辑器端 CLI 覆盖测试，需要下一轮启动编辑器验证 PrintString、模糊候选、typed target member call。

阻塞内容：
1. 无。

## 2026-05-16 Layout Deprecation Note

Historical references in this file to `LayoutHints`, fragment `layout`, `set_node_position`, `preserve_layout`, or payload-level `layout:auto` are retained only as old verification/progress records. The current boundary is: TaskPlan / GraphWrite generate nodes and links only; UE-side GraphLayout owns final configurable node placement. Central source fields now carry `DEPRECATED_LAYOUT` comments and should be migrated or removed.

## 2026-05-16 CallFunction 架构硬性修复记录

时间：2026-05-16 17:28:45

完成内容：
1. call 语义保持函数调用语义，不再自动降级或升级为变量/属性写入；set / 后续 set_property 仍是变量/属性写入语义。
2. 普通函数调用不再无条件携带 TargetObjectType，避免 PrintString 等库函数被错误当成对象成员调用过滤。
3. 显式对象调用改为先创建对象 getter 并读取输出 pin 的 typed pin 信息，再用该 typed target 约束 CallFunctionResolver，从而区分 SmokeMesh.SetVisibility 的组件函数版本与 UMG Widget 同名函数版本。
4. GraphPatternRegistry 将 pin alias 和 defaults 拆分：解析阶段只应用 alias；默认值延后到候选函数解析完成后再应用，避免全局默认值污染 resolver 参数过滤。
5. 参数名匹配补充 UE 风格兼容：支持 property display name，以及 bool 参数 NewVisibility 与 AgentFace NewVisibility 的匹配。
6. CallFunction 节点创建路径保持 UBlueprintNodeSpawner::Invoke() 主路径，不恢复 NewObject<UK2Node_CallFunction> + SetFromFunction legacy 入口。

验证结果：
1. 编译通过：TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload。
2. PrintString(InString=...) preview 通过，execute 通过。
3. candidate_functions 已按 [{ target, candidates }] 返回结构化候选。
4. 在测试蓝图补齐 SmokeMesh: StaticMeshComponent 后，SmokeMesh.SetVisibility(NewVisibility=true) preview 通过，execute 通过。

距离期望差距：
1. 当前结论覆盖 K2 Blueprint 图表中的函数调用，不声明支持 Material Graph 或 AnimGraph 节点创建。
2. 更广泛的组件函数、Blueprint 自定义函数、继承函数和多参数重载仍需要继续扩大烟测矩阵，但主路径已不再依赖旧 legacy 节点创建。

阻塞内容：
1. 无当前阻塞。

## 2026-05-16 进度：CallFunction K2 上下文与 typed data edge 闭环

状态：已完成本轮实现、编译、编辑器启动和 CLI 覆盖测试。

新增内容：
1. 新增 `FBlueprintHelperK2CallContext`，用于承载 TaskSpec/Graph 可推导出的 K2 上下文，不要求 AgentFace 增加大量字段。
2. `FParsedNode`、SemanticIR builder、GraphGenerationPipeline 已接入 `ArgumentPinTypes` / `TargetObjectPinType`。
3. FragmentDAG data edge 的真实 pin type 已传入 call resolver；非 literal expression 可以参与 CallFunction 候选约束。
4. `candidate_functions` 结构化候选增加 input pin type、wildcard metadata 和 mismatch 摘要字段。
5. Graph composer 增加 wildcard 节点连接后的延迟 `ReconstructNode` 路径。

修复内容：
1. 修复 `FUNC_CustomThunk` 未定义导致的 UE 5.6 编译失败。
2. 修复 CallFunction 候选无法暴露 wildcard/generic 元数据的问题。
3. 修复非 literal expression 数据边只能停留在 DAG evidence、不能进入 call 参数类型约束的问题。

变更需求：
1. 按用户确认，不接 Schema Menu Builder；继续使用 ActionDatabase、BlueprintActionFilter、NodeSpawner、SemanticIR typed context 的非 UI 菜单主路径。
2. candidate_functions 当前期望为结构化对象数组，而不是旧的字符串数组。

验证结果：
1. UE C++ 编译通过。
2. 编辑器通过 MCP 启动，Bridge 可用。
3. `PrintString(InString=literal)` preview/execute 通过。
4. `SmokeMesh.SetVisibility(NewVisibility=false)` preview/execute 通过。
5. `candidate_functions` 模糊查询返回结构化候选，包含 `input_pin_types`。
6. `Array` 模糊查询返回 `Array_Add` / `Array_AddUnique`，可见 `custom_thunk=true`、`array_parm=true`、`array_type_dependent_params=true`。
7. `select -> PrintString.InString` preview/execute 通过，fragment DAG 记录 `select.result(string)` 到 `PrintString.InString(string)` 的 data edge。

距离期望差距：
1. 本轮期望内差距已补齐：Context effective consumption、target_object 主路径、schema data connection、candidate mismatch reason 均已覆盖到本轮验证场景。
2. 本阶段只声明 K2 Blueprint 支持；Material Graph / AnimGraph 与更大规模函数矩阵不纳入本阶段能力声明。

阻塞内容：
1. 无当前阻塞。
## 2026-05-16 进度：CallFunction 剩余差距补齐

新增内容：
1. `target_object` 已进入 SemanticIR statement/expression 解析、resolve 和 CallFunction fragment builder 主路径。
2. `FParsedNode.TargetObjectName` 连接 AgentFace 精简 target_object 表达与 UE object getter/Target pin wiring。
3. data edge 连接收敛到 schema-aware connection，支持 UE schema conversion/promotion 路径。

修复内容：
1. 修复存在 target_object 时 `SetVisibility` 被直接解析成未连接 Target 的函数节点，导致 execute 编译失败的问题。
2. 修复显式 object call Target pin 连接成功判定不够严格的问题。
3. 修复 candidate_functions JSON 控制字符转义风险和 mismatch reason 覆盖不足的问题。

变更需求：
1. 不接 Schema Menu Builder；维持 ActionDatabase + BlueprintActionFilter + NodeSpawner + SemanticIR typed context 架构。

快速修复：
1. 本轮自动化继续使用 `bh.cmd` 绕过 PowerShell ExecutionPolicy 对 `bh.ps1` 的拦截。

验证结果：
1. UE C++ 编译通过。
2. 全局 MCP 启动编辑器成功。
3. Fresh asset 覆盖：创建 Actor Blueprint、添加 `SmokeMesh`、执行 `SmokeMesh.SetVisibility(NewVisibility=false)` 成功。
4. `select(int) -> PrintString.InString` execute 成功。
5. mismatch preview 正常 blocked 并返回结构化候选。

距离期望差距：
1. 本轮期望内差距已补齐。
2. 非 K2 Blueprint 图表和更大规模函数矩阵不纳入本阶段声明。

阻塞内容：
1. 无当前阻塞。