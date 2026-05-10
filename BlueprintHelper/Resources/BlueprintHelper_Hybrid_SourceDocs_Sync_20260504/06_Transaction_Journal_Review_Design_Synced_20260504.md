# 06 Transaction / Journal / Review 设计文档（已同步确认 Diff）

日期：2026-05-03  
工具簇：Transaction / Journal / Review / 审计与回滚工具簇  
状态：同步确认 Diff 后的修正版  
同步范围：transaction 内部事实与 Agent-facing 返回解耦、普通工具不默认返回 transaction/review、安全字段来源修正。

---

## 0. 本次同步结论

本文件保留“所有 UE 写操作内部接入 Journal / Review”的原则，但同步以下口径：

```text
1. transaction_id 可由 UE 插件内部为写操作生成，并写入 Transaction Journal / Review Store。
2. 这不意味着所有写工具都默认把 transaction / review 返回给 Agent。
3. Blueprint Component、Blueprint Class Settings、Asset Factory 等普通能力工具，成功结果默认不暴露 transaction / review / safety。
4. Graph Write、Cleanup、Ownership、Rollback 等高风险或后续需要引用的工具，可按工具需要返回 transaction/block/rollback 摘要。
5. safety_profile 只来自 runtime_profile.active_profile，不来自单次工具结果。
6. dry_run 数据只在 status=dry_run 时放在 data.dry_run。
```

---

## 1. 总接入规则

所有 UE 侧写操作都必须统一接入 Transaction Journal / Review。

包括：

```text
资产创建
组件树修改
组件属性修改
类设置修改
Class Defaults 修改
Interface 添加 / 移除
Override / Event 入口创建
IA 事件入口引用
Graph Write
Cleanup
Review 中触发的 rollback / ownership transfer
```

不同写操作的 diff 类型不同，但必须进入同一套审计系统。

并非所有写操作都使用 block_id。

```text
block_id 只用于 Graph Write 创建或接管的蓝图逻辑块。
资产、组件、类设置、事件入口、输入事件入口等使用 transaction_id / target_ref / before-after diff / rollback_data。
```

---

## 2. Agent-facing 返回边界

内部生成 transaction 不等于 Agent-facing 默认返回 transaction。

普通能力工具默认不返回：

```text
transaction
review
safety
```

适用普通工具：

```text
Asset Factory
Blueprint Component
Blueprint Class Settings
普通 Class Default 写入
普通接口添加 / 移除
```

这些工具的 Agent-facing 成功结果应聚焦：

```text
status
modified
data.*_result
validation
```

高风险或后续引用工具可以按需返回 transaction 摘要：

```text
Graph Write
Cleanup
Rollback
Ownership transfer
ConvertBlueprintHelperBlockToUserOwned
需要用户后续引用 block_id / transaction_id 的工具
```

---

## 3. transaction_id 粒度

一次写工具调用生成一个 transaction_id。

一个 transaction_id 内可以包含多个同一工具语义下的 diff item，例如：

```text
set_component_properties 一次设置多个属性
set_class_default_properties 一次设置多个默认属性
ReplaceBlueprintGraph 一次删除旧节点并创建新节点
CleanupBlueprintHelperFeature 一次删除多个 owned block / component group
```

不把完整功能开发流程合并成一个 transaction。

不为每个被修改目标单独生成 transaction_id。

初期不引入 sub_transaction_id。

---

## 4. block_id 边界

`block_id` 只用于 Graph Write 创建或接管的 BlueprintHelper-owned 蓝图逻辑块。

不使用 block_id 的对象：

```text
资产创建
组件树修改
组件属性修改
Class Settings 修改
Class Defaults 修改
Interface 添加 / 移除
Override / Event 入口
IA 事件入口
```

这些对象使用：

```text
transaction_id
target_ref
entry_ref
component_group_id
before-after diff
rollback_data
```

---

## 5. Tool result 与 Journal 的关系

Journal / Review 是内部审计事实来源。

Agent-facing 普通结果不是 Journal 的完整镜像。

规则：

```text
1. 普通成功结果不默认输出 transaction_id。
2. 普通成功结果不默认输出 review_status。
3. 普通成功结果不默认输出 rollback_data。
4. 普通成功结果不默认输出 safety_profile。
5. 需要调试、rollback、cleanup、ownership 或 block 后续引用时，特定工具可返回必要摘要。
```

Agent 最终报告默认只输出用户可见结果：

```text
任务是否完成
修改资产摘要
主要新增 / 修改逻辑
编译 / 验证结果
保存结果
异常或未完成项
```

不默认输出：

```text
transaction_id
block_id
Journal 路径
Review 状态
rollback_data
schema 细节
```

---

## 6. rollback

初期 rollback 默认按 transaction_id 整体回滚。

工具语义：

```text
rollback_transaction(transaction_id)
```

item-level rollback 后置为高级能力。

rollback 前必须重新读取当前资产状态并做冲突检测。如果目标位置已被用户或其他 transaction 修改，默认停止并报告冲突。

同一 transaction 内部：

```text
优先按 Journal 记录的 dependency graph 回滚。
没有显式 dependency graph 时，按该工具内部写入顺序的逆序回滚。
无法安全解析依赖时 rollback blocked。
```

跨 transaction 不自动级联 rollback。

---

## 7. 跨 transaction 依赖边界

BlueprintHelper 不做复杂跨 transaction 依赖管理。

Reject 某个 transaction 时，默认只回滚该 transaction 自身。

不自动级联回滚后续 transaction。

不因为“可能存在后续依赖”默认阻止用户 Reject。

Reject 后如果后续蓝图逻辑出现断链、编译错误、缺失引用，应由：

```text
blueprint_compile_asset
blueprint_asset_diagnostics
Unreal Editor 编译报错
Review UI validation_result
```

暴露给用户。

用户再决定是否继续 Reject 后续 transaction、Accept 当前状态、或让 Agent 修复。

Journal 可以记录轻量 target_refs / produces_refs / consumes_refs 作为审计信息，但不承担完整依赖调度。

---

## 8. Review UX

Review 面向用户只暴露两个核心动作：

```text
Accept
Reject
```

Accept：

```text
review_status = accepted
不执行 rollback
保留 Journal / diff / validation_result
可后续进入历史记录、归档或压缩
```

Reject：

```text
review_status = rejected
默认自动触发 rollback_transaction(transaction_id)
rollback 成功：rollback_status = succeeded
rollback blocked：rollback_status = blocked + conflict_detected / needs_action
rollback failed：rollback_status = failed + needs_action
```

用户可见 Review 状态：

```text
pending_review
accepted
rejected
```

内部记录：

```text
review_status: pending_review | accepted | rejected
rollback_status: not_required | pending | succeeded | blocked | failed
storage_status: active | archived | compacted
flags: conflict_detected | needs_action | validation_failed
```

---

## 9. Accept 后 ownership

Accept 后不默认移除 BlueprintHelper ownership metadata。

Review UI 提供：

```text
Accept and keep managed
Accept and convert to user-owned
```

默认：

```text
Accept and keep managed
```

`Accept and convert to user-owned` 会清除或转换：

```text
Graph block metadata
component_group metadata
event entry metadata
其他 BlueprintHelper-owned 标记
```

转为 user-owned 后，后续 Cleanup / replace_owned 不再默认管理该内容。

ownership 转换本身也应记录为 transaction 或 review_action 事件。

---

## 10. rollback_data 保留

默认分阶段保留，Setup / Profile 可配置 compact 策略。

规则：

```text
pending_review：必须完整保留 rollback_data。
reject + rollback succeeded：可压缩，但保留 rollback 摘要。
reject + rollback blocked / failed：必须保留完整 rollback_data 和冲突信息。
accepted：默认继续完整保留一段时间或直到 compact。
archived：可根据 Profile 压缩。
compacted：只保留摘要和关键审计字段。
```

compacted 后通常不再支持精确自动 rollback。

---

## 11. 资产创建 rollback

资产创建类 transaction 被 Reject 时，默认尝试删除该 transaction 新建的资产。

允许自动删除的条件：

```text
资产确实由该 transaction 创建
资产创建后未被用户或其他 transaction 修改
资产未被其他资产外部引用
资产未被 Accept and convert to user-owned
资产未被后续 transaction 作为依赖目标使用
```

条件全部满足：

```text
自动删除新建资产
rollback_status = succeeded
```

任一条件不满足：

```text
不自动删除
rollback_status = blocked
flags = conflict_detected / needs_action
返回 blocked_by 和 recommended_action
```

---

## 12. Reject 后验证

Reject 默认触发当前 transaction 的 rollback。

Reject / rollback 后是否自动运行验证，由 Profile 和参数控制：

```text
validate_after_reject: none | diagnostics | compile | diagnostics_and_compile
```

默认建议 lightweight diagnostics，不默认 compile。

结果写入：

```text
rollback_validation_result
```

Review UI 应展示，用于判断拒绝该改动后是否引入断链、编译错误或资产异常。

---

## 13. 批量审查

支持：

```text
AcceptAll
RejectAll
```

不支持任意多选 Accept / Reject。

AcceptAll：

```text
对当前 Review 列表中所有 pending_review transaction 执行 Accept。
默认保留 BlueprintHelper ownership metadata。
```

RejectAll：

```text
对当前 Review 列表中所有 pending_review transaction 执行 Reject。
不做跨 transaction 级联依赖分析。
默认按 transaction 创建时间倒序执行 rollback。
遇到 rollback blocked / failed 默认停止。
```

Feature-level Accept / Reject 后置。

---

## 14. Review 列表

Review 列表默认按 target_asset 分组展示。

每个资产分组内按 transaction 时间排序。

默认主视角是资产视角，而不是纯时间流水账。

后续可增加筛选：

```text
review_status
tool_name
feature_name
modified_time
```

---

## 15. 可视化审计

Review 面板体验参考 vibecoding 审计，但适配蓝图。

对图表修改提供节点级可视化差异审计：

```text
新增节点 / 新增逻辑块：绿色高亮 / 绿色边缘光。
删除节点 / 删除逻辑块：红色高亮 / 红色边缘光。
```

Replace / Cleanup / Merge 可同时显示：

```text
被删除部分（红）
新增部分（绿）
原入口节点或锚点（中性保留）
```

组件树修改：

```text
新增组件：绿色
删除组件：红色
属性修改：后续可考虑黄色或蓝色
```

资产创建 / 删除在资产分组层显示 Created / Deleted 标签。

---

## 16. 审计面板蓝图预览

Review 审计面板默认不直接打开真实 Blueprint Editor。

Review 面板内提供“已修改后的蓝图预览”。

用户审查的是当前 transaction 修改后的蓝图视图，而不是可编辑蓝图资产。

新增内容在审计面板预览中使用绿色高亮。

被删除内容不重新写回真实蓝图，而是由 Review Widget 根据 Journal 中的 before snapshot / rollback_data 生成只读 snapshot。

删除 snapshot：

```text
红色高亮
不是真实蓝图节点
不参与编译
不参与保存
不参与 Graph Write
不允许被编辑
```

真实 Blueprint Editor 可作为辅助动作：

```text
Open in Blueprint Editor
```

但不是默认审计入口。

---

## 17. 审计面板只读

Review 面板预览完全只读。

不允许：

```text
直接编辑蓝图节点
修改新增节点
恢复或编辑删除 snapshot
修改组件树
修改 Class Settings
作为第二个 Blueprint Editor
```

需要人工编辑时，用户主动点击 Open in Blueprint Editor。

需要 Agent 修复时，由 Agent 另起新的写工具调用，并生成新的 transaction_id。

---

## 18. 删除 snapshot 保存粒度

采用双层保存：

```text
review_snapshot：UI 展示用轻量快照
rollback_data：事务回滚用完整 before 数据
```

review_snapshot 可包含：

```text
node_id / node_guid
node_type
node_title
graph_name
position
pin_summary
link_summary
comment / metadata 摘要
ownership 摘要
```

rollback_data 保存完整回滚所需数据。

pending_review 状态必须保留完整 rollback_data。

compact 后可只保留 review_snapshot、diff_summary、transaction metadata、validation summary。

---

## 19. Accept 后高亮

transaction 被 Accept 后，从活跃 Review 列表中移除。

accepted transaction 默认不再显示绿色 / 红色活跃审计高亮。

Reject 成功 rollback 后，同样从活跃 Review 列表移除。

Reject blocked / failed 时，应保留在 Review 或 Needs Action 区域，并显示 rollback 问题。

---

## 20. 验收标准

```text
1. 所有 UE 写操作内部进入 Transaction Journal / Review。
2. 普通能力工具成功结果不默认返回 transaction / review / safety。
3. Graph Write / Cleanup / Rollback 等需要后续引用的工具可返回必要 transaction/block/rollback 摘要。
4. transaction_id 粒度是一写工具调用一次。
5. block_id 只用于 Graph Write owned 逻辑块。
6. safety_profile 只从 runtime_profile.active_profile 获取。
7. dry_run 数据只在 status=dry_run 时位于 data.dry_run。
8. Agent 最终报告默认不输出 transaction_id / review_status / Journal 路径。
9. Review UI 仍可基于内部 Journal 展示完整审计和 rollback。
```
---

# 2026-05-04 TaskRunJournal 同步

## 同步结论

Transaction / Journal / Review 设计不推翻。新增 `task_run_id` 与 `TaskRunJournal`，用于把一次 TaskSpec 执行产生的多个 transaction 组织成用户可理解的任务。

保持旧规则：

```text
一次真实 UE 写操作 = 一个 transaction_id
```

新增规则：

```text
一次 TaskSpec / TaskPlan 执行 = 一个 task_run_id
一个 task_run_id 下可以包含多个 child transaction_id
```

## TaskRunJournal

推荐路径：

```text
<Project>/Saved/BlueprintHelper/Tasks/task_YYYYMMDD_NNNN.json
```

最小结构：

```json
{
  "schema": "BlueprintHelper.TaskRunJournal.v1",
  "task_run_id": "task_20260504_0001",
  "task_type": "create_blueprint_feature",
  "feature_name": "PhysicsDoor",
  "status": "completed",
  "target_assets": ["/Game/BP/BP_Door"],
  "task_spec_hash": "sha256:...",
  "context_snapshot_ref": "ctx_20260504_0001",
  "preview_id": "preview_20260504_0001",
  "steps": [
    {
      "step_id": "step_001",
      "operation": "add_component",
      "status": "applied",
      "transaction_id": "tx_20260504_0001"
    }
  ],
  "child_transactions": ["tx_20260504_0001"],
  "validation": {
    "compiled": true,
    "saved": false,
    "diagnostics": "passed"
  }
}
```

## Review UI 分组

Review UI 默认按 task_run_id 分组显示：

```text
Task: PhysicsDoor
Target: /Game/BP/BP_Door
Transactions: 7
Status: pending_review
```

展开后再显示 child transaction：

```text
1. Created interface asset
2. Added components
3. Configured component properties
4. Added implemented interface
5. Added variables
6. Appended graph
7. Merged input/interface entry
```

## Reject / Rollback

新增 Task-level RejectAll：

```text
reject_task_run(task_run_id)
```

规则：

```text
1. 按 child transaction 创建时间倒序 rollback。
2. 遇到 rollback blocked / failed 立即停止。
3. 不跨 task_run 自动级联 rollback。
4. 不自动回滚 task_run 之外的 transaction。
```

单个 transaction 仍可独立 Reject。

## Agent-facing 返回

execute_task 成功默认不返回全部 transaction_id。

默认只返回：

```text
task_run_id
feature_name
target_assets
applied_steps
created_assets
modified_assets
validation
```

只有 debug / rollback / failure / CLI 无 Review UI 场景才展开 child transaction_ids。
