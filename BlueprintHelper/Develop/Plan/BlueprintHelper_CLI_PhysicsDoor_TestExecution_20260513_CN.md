# BlueprintHelper CLI 物理门测试执行记录 2026-05-13

## 目标

- 使用 BlueprintHelper CLI 在任意 Actor 中实现可开关的物理门相关功能。
- 只实现门 Actor 自身能力，不实现角色侧交互。
- 执行期间记录能力缺失和遭遇 Bug。

## 能力缺失记录

1. `GraphWrite` 当前普通 Agent-facing slice 文档明确说明，显式组件/成员调用不属于第一批 `call_function` resolver 能力，例如 `DoorMesh.AddAngularImpulseInDegrees` 这类写法会被阻止。
   - 影响：不能直接在普通 TaskSpec 图写入中对具体门板组件调用物理冲量、角速度、相对旋转等成员函数。
   - 当前规避：优先使用组件树配置、成员变量、Actor 自身可调用事件和自有逻辑块；必要时把组件级物理驱动能力记录为后续能力缺口。
   - 实测结果：`physics_door_component_impulse_gap.json` 在目标 Blueprint 创建后仍 preview blocked。
   - 实测错误：`explicit_member_call_not_supported`
   - 实测消息：`call_function.name uses an explicit member prefix; first slice supports graph/self/library calls only.`
2. 当前 `GraphWrite append_new_owned_graph` 普通 slice 只覆盖顺序 statements，实测/文档可用语句主要是 `call_function` 和 `set_member_variable`。
   - 影响：无法直接写出 `ToggleDoor` 所需的 `Branch -> Get bDoorOpen -> Not Bool -> Open/Close` 条件逻辑。
   - 当前规避：创建 `OpenDoor` / `CloseDoor` / `ToggleDoor` 事件入口，并在 `OpenDoor` / `CloseDoor` 中维护 `bDoorOpen` 状态；完整 Toggle 分支逻辑记录为后续能力缺口。
3. `blueprinthelper_read_context` 对 `target_type=component` 的 summary 读回只确认目标和状态，未返回组件属性细节。
   - 影响：不能仅靠该读回直接证明 `BodyInstance.bSimulatePhysics` 的最终属性值。
   - 当前佐证：`task result` for `task_7F9B52CD48110151378D4288F28425A2` 返回 `status=applied`、`component_name=DoorPanel`、`applied_count=1`、`invalid_settings=[]`。

## 遭遇 Bug 记录

1. `create_blueprint_feature` 在 `scope_policy.graph_name=BH_PhysicsDoorLogic` 时 preview 通过，但 execute 失败。
   - 错误：`target_graph_not_found`
   - 消息：`Blueprint graph not found: BH_PhysicsDoorLogic.`
   - Artifact：`D:\UEProjects\Template\Plugins\BlueprintHelper\Saved\BlueprintHelper\Cli\cli_1778673321718\result.json`
   - 判断：preview 没有覆盖目标图存在性，存在 preview false positive。
   - 当前规避：改用已存在的 `EventGraph` 重试。
2. `create_blueprint_feature` 的组件属性批量设置失败时，CLI artifact 只给聚合错误，未暴露具体无效属性。
   - 错误：`invalid_component_property_settings`
   - 消息：`One or more component property settings are invalid.`
   - Artifact：`D:\UEProjects\Template\Plugins\BlueprintHelper\Saved\BlueprintHelper\Cli\cli_1778673260007\result.json`
   - 影响：需要拆分属性写入才能定位问题。
   - 当前规避：先执行不带属性的最小门功能，再单独写入 `DoorPanel.BodyInstance.bSimulatePhysics=true`。

## 阶段结果

1. 运行时预检通过。
   - `bridge ping`：通过。
   - `blueprint_get_runtime_profile`：通过。
   - `blueprinthelper_diagnostics_runtime`：通过。
2. `physics_door_create_actor.json` preview 通过。
   - 目标资产：`/Game/BlueprintHelperCliSmoke/BH_PhysicsDoor_20260513/BP_BH_PhysicsDoorActor`
   - 计划步骤：1
3. `physics_door_feature.json` 首次 preview 阻塞。
   - 原因：目标 Blueprint 尚未创建。
   - 处理：先执行 `create_asset` 创建 Actor，再重新 preview。
4. `physics_door_component_impulse_gap.json` 首次 preview 阻塞。
   - 原因：目标 Blueprint 尚未创建。
   - 处理：待 Actor 创建后重新 preview，以确认组件级物理调用缺口。
5. 写权限申请第一次超时。
   - 命令：`blueprinthelper_request_write_session --file Saved\CodexTest\physics_door_write_session.json`
   - 结果：`Bridge request timed out after 30000ms`
   - 判断：需要 Editor 授权弹窗批准后继续，不先判定为产品 Bug。
6. `physics_door_create_actor.json` execute 通过。
   - `task_run_id`：`task_F8A8934A4BAF0AA72196109DD8DB256B`
7. `physics_door_feature_minimal.json` 首次 preview 通过但 execute 失败。
   - 首次图名：`BH_PhysicsDoorLogic`
   - 失败原因：`target_graph_not_found`
   - 处理：改为 `EventGraph` 后重试。
8. `physics_door_feature_minimal.json` 改用 `EventGraph` 后执行通过。
   - `task_run_id`：`task_699FD6CC49E6FDE6CD857E9067AC18C3`
   - 写入内容：`DoorFrame`、`DoorPanel`、`DoorHingeConstraint`、`bDoorOpen`、`OpenKickImpulse`、`CloseKickImpulse`、`MaxOpenAngle`、`InitializeDoor`、`OpenDoor`、`CloseDoor`、`ToggleDoor`
9. `physics_door_set_panel_simulate_physics.json` 执行通过。
   - `task_run_id`：`task_7F9B52CD48110151378D4288F28425A2`
   - 写入内容：`DoorPanel.BodyInstance.bSimulatePhysics=true`
10. 任务结果回读通过。
    - `task_F8A8934A4BAF0AA72196109DD8DB256B`：`result_found`
    - `task_699FD6CC49E6FDE6CD857E9067AC18C3`：`result_found`
    - `task_7F9B52CD48110151378D4288F28425A2`：`result_found`
11. Blueprint 上下文读回通过。
    - `blueprinthelper_read_context` summary：通过。
    - `blueprinthelper_read_task_context`：通过。
    - `EventGraph logic_json`：通过，并在 artifact 中确认 `InitializeDoor`、`OpenDoor`、`CloseDoor`、`ToggleDoor`、`Set bDoorOpen`。

## 当前实现边界

1. 已实现：门 Actor 资产、门框组件、门板组件、物理约束组件、门状态变量、开/关/切换事件入口、门板物理模拟属性写入。
2. 未完整实现：真正的物理开关动作，例如对 `DoorPanel` 施加角冲量或约束 motor 驱动。
3. 未完整实现：`ToggleDoor` 的条件分支逻辑。
4. 原因：当前普通 Agent-facing GraphWrite 能力缺少组件成员物理调用和基础控制流表达能力。

## 2026-05-13 Capability Patch Note

- 已补：GraphWrite `call_function` 对 `ComponentName.FunctionName` 形式的显式对象调用增加 UE 侧最小支持。
- 目标：解除物理门测试中 `DoorPanel.AddAngularImpulseInDegrees` 被 `explicit_member_call_not_supported` 阻断的问题。
- 边界：当前只覆盖成员变量/组件 getter 作为调用目标；复杂控制流 TaskSpec 语义仍需单独讨论。
- 未验证：本轮按要求未执行 UE 构建、CLI preview 或 Editor 写入验证。

## 2026-05-13 Bug Record - CLI TaskSpec Review panel does not refresh dynamically

- Severity: High / Refresh defect, downgraded from severe after user correction.
- Source: User live Editor observation during CLI physics-door test.
- Correction: CLI TaskSpec writes in the current Editor session do not show Review records immediately, but Review records appear after restarting the Editor.
- Symptom: The Blueprint Helper Review panel does not dynamically refresh newly created CLI TaskSpec Review records during the same Editor session.
- Evidence: Review panel initially showed no selected review change and no loaded Blueprint component/outline context after CLI-created actor/components/graph changes were present in the editor; user later confirmed records appear after Editor restart.
- Affected scope: At minimum affects the physics-door CLI write path, including create asset / composite create_blueprint_feature / edit_blueprint_components TaskSpec executions.
- Expected behavior: Every successful Agent/CLI TaskSpec write that mutates Blueprint assets should create Review records and the live Review panel should dynamically refresh without requiring Editor restart.
- Impact: Review evidence exists but is hidden until restart, so the user cannot audit or rollback fresh CLI writes from the live Review UI in the same session.
- Current status: Recorded only. Root cause not investigated in this step.

## 2026-05-13 ReviewPanel Bug Dedupe - Physics Door live Editor observation

- Dedup basis: searched existing ReviewPanel / Review / TaskSpec docs for duplicate, row highlight, ReviewAnchor, Reject, Graph diff, alpha, and visible-change collapse references.
- Existing contract references: ReviewStore should collapse visible changes; non-Graph panels should use row background highlights; ReviewAnchor rows are only fallback for targets without stable rows; Reject of a normal selected row should not cascade to unrelated targets.
- Canonical bug PD-RP-01: Duplicate visible changes for repeated atomic targets.
  - Covers reported Bug1 and Bug6.
  - Observed: four variables each show three identical Review records, and three added components each show two Review records.
  - Expected: repeated writes to the same variable/component/function/etc. collapse to one visible Review item showing the latest write.
  - Dedupe result: same root issue, track as one ReviewStore visible-change collapse / latest-wins bug.
- Canonical bug PD-RP-02: Component rows do not show row-background diff highlight.
  - Covers reported Bug2.
  - Observed: three added components exist in the component panel, but their rows do not get a row BG-color diff highlight.
  - Expected: highlight the changed component Row background, not draw a floating overlay over the panel.
  - Dedupe result: existing docs define this expectation; this is new physics-door live evidence that it is not satisfied.
- Canonical bug PD-RP-03: GraphPanel diff block opacity is too high.
  - Covers reported Bug3.
  - Observed: center GraphPanel diff block is visually too opaque.
  - Expected: GraphPanel diff block fill alpha should be lowered to 0.35.
  - Dedupe result: no exact prior bug found; record as new visual tuning bug / requirement.
- Canonical bug PD-RP-04: ReviewAnchor fallback leaks into MyBlueprint for normal Blueprint content.
  - Covers reported Bug4.
  - Observed: MyBlueprint panel shows ReviewAnchor entries that do not match the native UE My Blueprint panel.
  - Expected: ReviewAnchor fallback should appear only when no stable native or owned row can represent the target.
  - Dedupe result: related to existing MyBlueprint native-parity / ReviewAnchor fallback docs, but the physics-door symptom is recorded as a live bug instance.
- Canonical bug PD-RP-05: Rejecting one normal graph event cascades sibling events.
  - Covers reported Bug5.
  - Observed: Rejecting InitializeDoor also rejected OpenDoor and CloseDoor.
  - Expected: Reject on a normal selected row affects only that selected visible change / target. Cascade is only valid for asset lifecycle root reject after root success.
  - Dedupe result: existing docs state the expected behavior; this is new live evidence of a contract violation.

- Canonical bug PD-RP-06: New signature reviews are not grouped under the asset-creation Review root.
  - Observed: four newly added signature Review records are shown as independent records instead of being nested under the created-asset Review record.
  - Expected: When an asset is newly created by the same CLI TaskSpec flow, same-asset child changes such as newly added signatures should be grouped under the asset lifecycle root Review record.
  - Dedupe result: related to the existing lifecycle-root grouping contract, but recorded as a new physics-door live failure instance.
## 2026-05-13 ReviewPanel Bug 修复 Pass 1

状态说明：本轮仅做源码修复，未执行构建或编辑器验证；因此未将未验证项标记为完全完成。

1. PD-RP-01 重复 Review 记录
   - 状态：源码修复已落地，待验证。
   - 处理：`LoadPendingVisibleChanges` 增加 latest-wins 收敛，同一资产/Surface/Graph/TargetKey 的 pending 可见改动只保留最新写入；同时保留来源事务链。

2. PD-RP-03 GraphPanel diff 框透明度
   - 状态：源码修复已落地，待验证。
   - 处理：中央 GraphPanel diff block 填充透明度调整为 0.35。

3. PD-RP-04 MyBlueprint 中出现 ReviewAnchor
   - 状态：源码修复已落地，待验证。
   - 处理：MyBlueprint presenter 补充扫描 Ubergraph 中的 CustomEvent，并作为事件行参与 Review 定位，避免物理门测试中的自定义事件签名退回到 ReviewAnchor。

4. PD-RP-05 Reject InitializeDoor 连带 Reject OpenDoor/CloseDoor
   - 状态：源码修复已落地，待验证。
   - 处理：TaskSpec evidence 生成 visible change 时不再直接复用 transaction id，改为 transaction id + visual group key 的稳定 ID，避免同一事务内多个可见改动共享 ChangeId 后误命中。

5. PD-RP-06 新增签名 Review 未归到创建资产 Review 记录下
   - 状态：源码修复已落地，待验证。
   - 处理：资产生命周期 root 与子改动匹配改为使用规范化 package path，兼容 `/Game/X/BP` 与 `/Game/X/BP.BP` 形式差异；ChangeTree 分组也使用同样的规范化资产 key。

6. CLI TaskSpec Review Panel 动态刷新
   - 状态：源码修复已落地，待验证。
   - 处理：ReviewPanel 不再轮询；改为 `execute_task_plan` 结束后由 ReviewStoreService 广播一次 pending review changed 事件，ReviewPanel 收到事件后刷新列表、树、当前资产视图和 diff stack。

7. PD-RP-02 Components Panel 新增组件 Row 背景 diff
   - 状态：源码修复已落地，待验证。
   - 处理：native Components row 实际为 STableRow/SBorder 派生；组件行定位成功后直接设置该 row border background color，不再只接受 `SBorder` 类型名。

8. 最终改动面板新增签名未挂到新增资产 root
   - 状态：源码修复已落地，待验证。
   - 处理：ChangeTree 构建阶段增加同资产新增资产 root 的兜底挂载；即使签名记录缺少 `ParentChangeId`，也会作为新增资产 root 的叶子显示。

9. MyBlueprint 宏分类缺失
   - 状态：源码修复已落地，待验证。
   - 处理：Macros section 改为常驻显示，避免空宏分类被清理。
## 2026-05-13 ReviewPanel Accept/Reject Persistence Fix

状态说明：本轮仅做源码修复和 CLI/Bridge 命令接入，未执行构建、CLI build 或编辑器验证。

1. Final Changes 面板 Accept/Reject GraphPanel 同一条 Review 无效
   - 状态：源码修复已落地，待验证。
   - 处理：ReviewActionService 不再只解析第一个持久化 ReviewRecord；对一个可见 Review 项会解析所有匹配的 ReviewRecord + target_keys，并逐个回写 Accept/Reject。

2. 组件/变量 Review Accept/Reject 后重开 ReviewPanel 又出现
   - 状态：源码修复已落地，待验证。
   - 处理：针对 latest-wins 合并后的可见项，Accept/Reject 会回写所有同资产同 target_key 的 pending 持久化记录，避免旧 pending record 在重新加载时复活。

3. DebugBundle 排查能力
   - 状态：源码接入已落地，待验证。
   - 处理：Bridge 增加 `list_debug_cases` 与 `export_debug_bundle`；CLI tool surface 增加 `blueprinthelper_list_debug_cases` 与 `blueprinthelper_export_debug_bundle`。

## 2026-05-13 ReviewPanel Accept/Reject Persistence Fix

状态说明：本轮仅做源码修复和 CLI/Bridge 命令接入，未执行构建、CLI build 或编辑器验证。

1. Final Changes 面板 Accept/Reject GraphPanel 同一条 Review 无效
   - 状态：源码修复已落地，待验证。
   - 处理：ReviewActionService 不再只解析第一个持久化 ReviewRecord；对一个可见 Review 项会解析所有匹配的 ReviewRecord + target_keys，并逐个回写 Accept/Reject。

2. 组件/变量 Review Accept/Reject 后重开 ReviewPanel 又出现
   - 状态：源码修复已落地，待验证。
   - 处理：针对 latest-wins 合并后的可见项，Accept/Reject 会回写所有同资产同 target_key 的 pending 持久化记录，避免旧 pending record 在重新加载时复活。

3. DebugBundle 排查能力
   - 状态：源码接入已落地，待验证。
   - 处理：Bridge 增加 `list_debug_cases` 与 `export_debug_bundle`；CLI tool surface 增加 `blueprinthelper_list_debug_cases` 与 `blueprinthelper_export_debug_bundle`。
