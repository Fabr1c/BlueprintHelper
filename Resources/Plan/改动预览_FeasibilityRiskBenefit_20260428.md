# BlueprintHelper 改动审阅功能可行性、风险与收益评估（2026-04-28）

## 一、结论

可以实现。

但第一版不建议直接做成“Agent 写入前的完整预览 Diff”。当前 BlueprintHelper 的核心能力是通过 MCP / Bridge 直接操作 Unreal Editor 内的资产和蓝图图表，很多写操作只有在编辑器对象被实际修改后，才能可靠获得编译结果、节点自动修复结果、Pin 重建结果和蓝图 Diff 结果。

更稳妥的第一版是：

```text
Agent 写操作 -> 插件创建审阅会话 -> 记录修改前快照 -> 执行修改 -> 记录修改后快照 -> 面板展示差异 -> 用户确认保留或回滚
```

也就是“已应用但未确认保存”的审阅模式。它与 UE 编辑器的事务系统、资产脏标记、Blueprint 编译结果、Source Control Diff / Asset Diff 机制更匹配，风险比纯预览低。

推荐功能名称：

```text
BlueprintHelper Change Review
```

推荐第一阶段目标：

- 让 Agent 的写操作默认进入“待审阅”状态。
- 用户面板可以看到本次 Agent 改了哪些资产、图表、节点、变量、UMG 树、DataTable 行或 UObject 属性。
- 用户可以打开 UE 资产 Diff 或查看逻辑摘要 Diff。
- 用户可以确认保留，也可以通过事务或快照回滚。
- 确认前默认不自动保存资产。

---

## 二、与 UE 版本控制 Diff 的关系

### 2.1 可以复用的部分

UE 已经具备若干适合复用的编辑器能力：

- 资产脏标记。
- Editor Transaction / Undo / Redo。
- Blueprint 编译和诊断。
- Blueprint / Asset Diff 工具。
- Source Control 下的 depot revision 对比。
- 资产复制、临时包和对象序列化能力。

这些能力可以作为 Change Review 的底层支撑。

### 2.2 不能直接等同的部分

BlueprintHelper 的 Agent 改动审阅与传统版本控制 Diff 不完全相同：

| 维度 | UE 版本控制 Diff | BlueprintHelper Change Review |
|------|------------------|--------------------------------|
| 对比对象 | 当前资产 vs depot revision 或另一个资产 | 修改前快照 vs 修改后编辑器状态 |
| 触发者 | 人类开发者或版本控制操作 | MCP / Agent 写操作 |
| 关注点 | 资产级差异 | Agent 操作、逻辑差异、资产状态、安全确认 |
| 回滚方式 | Revert / checkout / undo | Editor Transaction、快照恢复、删除新增资产 |
| 面板职责 | 展示差异 | 展示差异 + 审批 + 回滚 + 保存门控 |

因此建议不要把该功能实现为“Source Control Diff 的简单外壳”，而是实现为独立的审阅会话管理器，并在需要时调用 UE Diff 工具。

---

## 三、推荐模式

### 3.1 第一版：Applied Pending Review

第一版推荐使用“已应用待审阅”模式。

流程：

```text
1. 用户授权 Agent 修改蓝图或资产。
2. MCP 写工具进入 ReviewSession。
3. 插件在修改前捕获目标资产快照。
4. 插件执行现有写操作。
5. 插件捕获修改后快照、编译结果和诊断信息。
6. 用户 Widget 面板显示待审阅改动。
7. 用户选择：
   - Approve：保留修改，可选择保存资产。
   - Reject：回滚事务或恢复快照。
   - Open Diff：打开 UE 资产 Diff 或逻辑 Diff。
```

该模式的优点：

- 不要求重写所有写工具为 dry-run。
- 可以利用现有 MCP / Bridge 写能力。
- 能看到真实蓝图编译结果。
- 能处理 K2 节点自动重构、Pin 自动生成、图表重排等编辑器副作用。
- 与用户已有 Widget 面板天然适配。

### 3.2 第二版：Preflight Preview

第二版可以增加预检模式，但不建议作为第一版核心。

预检模式适合：

- 创建变量。
- 设置属性。
- 添加 DataTable 行。
- 添加简单节点。
- 删除明确 ID 的节点或 Widget。

不适合第一阶段完全覆盖：

- 自动连线。
- 节点重建。
- 函数签名变化。
- 蓝图宏 / 函数图复杂编辑。
- UMG 层级移动后涉及命名冲突的操作。

预检模式的输出可以是“操作计划 Diff”，而不是资产真实 Diff。

---

## 四、核心设计判断

### 4.1 审阅对象应以资产为单位

一条 Agent 请求可能修改多个对象：

- Blueprint 图表节点。
- Blueprint 变量、函数、宏、事件分发器。
- UMG Widget 树。
- DataAsset / UObject 属性。
- DataTable 行。
- 新建资产或删除资产。

审阅会话应以 session 为顶层，以 asset 为分组。

推荐结构：

```json
{
  "session_id": "BPHR_20260428_001",
  "state": "pending_review",
  "initiator": "mcp_agent",
  "started_at": "2026-04-28T23:00:00+08:00",
  "assets": [
    {
      "asset_path": "/Game/UI/WBP_Menu",
      "asset_type": "WidgetBlueprint",
      "change_kind": "modified",
      "dirty": true,
      "compile_status": "success",
      "summary": {
        "nodes_added": 2,
        "nodes_removed": 0,
        "properties_changed": 3
      }
    }
  ]
}
```

### 4.2 差异展示应分三层

推荐三层展示，不要只依赖 UE 原生 Diff。

| 层级 | 展示内容 | 目标用户 |
|------|----------|----------|
| Operation Diff | Agent 调用了哪些写操作，参数是什么，结果是什么 | 用户与调试者 |
| Logic Diff | 蓝图逻辑摘要前后差异，变量、入口事件、执行流、函数调用变化 | Agent 与蓝图作者 |
| Asset Diff | 调用 UE 原生 Diff 查看节点/资产图形差异 | 蓝图作者 |

第一版至少实现 Operation Diff 和 Asset Diff。若已经计划 LogicProcessor，则建议同步接入 Logic Diff。

### 4.3 确认前不自动保存

为了降低风险，Agent 写操作完成后资产可以处于 dirty 状态，但不应默认自动保存。

推荐策略：

- Agent 写操作：修改内存资产，标记 dirty。
- Review 面板：显示待审阅。
- Approve：仅标记 session approved，可选保存。
- Approve & Save：保存本 session 涉及资产。
- Reject：回滚或恢复快照。

如果必须支持自动保存，应要求工具显式传入：

```json
{
  "review_policy": "auto_approve_and_save"
}
```

默认不使用该策略。

---

## 五、实现风险评估

### 5.1 风险总览

| 风险 | 等级 | 说明 | 缓解方式 |
|------|------|------|----------|
| 蓝图 Diff API 不稳定或调用入口分散 | 中 | 不同资产类型的 Diff 打开方式不同 | 第一版允许退化为逻辑 Diff / raw JSON Diff |
| Editor Transaction 不能覆盖所有操作 | 高 | 新建资产、删除资产、部分保存行为不一定能完整 Undo | 增加资产快照和恢复策略 |
| 修改后蓝图自动重建导致 Diff 噪音 | 中 | K2 节点会重建 Pin、GUID、默认值 | Logic Diff 忽略布局和非语义字段 |
| 多资产跨会话修改冲突 | 高 | Agent 与用户可能同时编辑同一资产 | session 捕获 revision/dirty 状态，冲突时禁止自动回滚 |
| 大蓝图 raw JSON / Diff 过大 | 中 | Widget 面板可能卡顿 | 摘要分页、懒加载、按图表导出 |
| 用户确认前编辑资产 | 中 | 用户手工改动会混入 session | 捕获审阅开始后的资产变更事件，标记 mixed_changes |
| Source Control 状态复杂 | 中 | 未 checkout、只读文件、外部修改 | SourceControl 只作为附加信息，不作为第一版强依赖 |
| UMG / DataTable / UObject 差异格式不统一 | 中 | 不同资产类型差异表达不同 | 用统一 ChangeRecord，再按资产类型扩展 |
| Agent 绕过审阅工具直接调用写命令 | 中 | 旧工具仍可直接写 | 在 Bridge Router 层统一包裹写命令，默认 review_policy=pending |
| 回滚后编译状态不一致 | 中 | 蓝图恢复后需要重新编译 | Reject 后自动 compile / refresh editor |

### 5.2 最大技术风险

最大风险不是“能否显示 Diff”，而是“能否可靠回滚”。

原因：

- UE Transaction 对普通对象属性修改很强，但对资产创建、删除、保存后的外部文件状态不是万能的。
- 蓝图修改可能触发节点重建和编译副作用。
- 若用户在待审阅期间手工编辑同一资产，简单 Undo 可能回滚掉用户手工改动。

因此第一版必须把回滚能力分级：

| 回滚方式 | 适用场景 | 可靠性 |
|----------|----------|--------|
| Transaction Undo | 单会话、未混入用户编辑、未保存 | 高 |
| Snapshot Restore | 蓝图/UMG/DataAsset 修改，已捕获 before snapshot | 中高 |
| Delete Created Asset | Agent 新建资产且未被其他引用 | 中 |
| Manual Review Required | 已保存、混入用户编辑、外部修改 | 低，需要用户处理 |

### 5.3 最大产品风险

最大产品风险是“Diff 太底层，用户仍然看不懂 Agent 改了什么”。

只打开 UE 原生蓝图 Diff，用户可能看到大量节点布局、Pin 重建和 GUID 差异，但不能快速判断 Agent 意图。

因此必须保留 Change Summary：

- 新增了哪些变量。
- 删除了哪些节点。
- 改了哪些默认值。
- 改了哪些 UMG Widget 属性。
- 改了哪些 DataTable 行。
- 蓝图编译是否成功。
- 是否产生警告或错误。

这比单纯调用 UE Diff 更重要。

---

## 六、收益评估

### 6.1 对用户的收益

- 可以在 Agent 修改后确认具体变化，降低不透明写入的风险。
- 用户可以一键回滚不满意的修改。
- 重要资产可以先审阅再保存，减少误保存。
- 可以把 Agent 修改与人工修改区分开。
- 可以通过逻辑摘要判断蓝图行为是否符合预期。

### 6.2 对 Agent 的收益

- Agent 可以获得审阅反馈闭环。
- Agent 可以通过 review summary 理解自己修改造成的结果。
- 编译失败、Diff 异常、回滚原因可以作为下一步修复输入。
- 多步蓝图编辑可以按 session 分组，降低上下文混乱。

### 6.3 对插件架构的收益

- 写操作从“直接执行”升级为“可审计执行”。
- Bridge / MCP 的安全边界更明确。
- 用户面板不再只是工具入口，也成为 Agent 修改的审批台。
- 后续可扩展为自动测试、规则校验、提交前检查。

---

## 七、功能边界

### 7.1 第一版应包含

- 创建审阅会话。
- 写操作自动绑定当前审阅会话。
- 修改前/后资产快照。
- 操作记录列表。
- 待审阅面板。
- 打开 UE Diff。
- 显示摘要 Diff。
- Approve / Reject。
- Reject 后回滚或提示手动处理。
- 审阅状态持久化到 Saved 目录。

### 7.2 第一版不建议包含

- 完整 dry-run 蓝图预览。
- 自动解决多人或多 Agent 冲突。
- 对所有资产类型做像 Blueprint 一样精细的视觉 Diff。
- 审阅通过后自动提交版本控制。
- 直接改写 UE Source Control 工作流。

### 7.3 后续可扩展

- 审阅规则：禁止 Agent 修改特定路径。
- 审阅策略：高风险操作必须人工确认。
- 规则校验：调用 JSON rule validator、compile、PIE smoke test。
- 自动生成修改说明。
- 与 Git / Perforce changelist 关联。
- 支持保存为审阅报告 Markdown。

---

## 八、推荐接口方向

### 8.1 Bridge 层新增命令

建议新增命令：

```text
review_begin_session
review_get_session
review_list_sessions
review_approve_session
review_reject_session
review_open_asset_diff
review_export_summary
```

现有写命令不必全部改签名，但 Bridge Router 应统一读取可选字段：

```json
{
  "review_policy": "pending | bypass | auto_approve",
  "review_session_id": "optional"
}
```

默认值建议为：

```text
pending
```

### 8.2 MCP Server 层新增工具

建议 MCP 暴露更面向 Agent 的工具：

```text
blueprint_review_begin_session
blueprint_review_list_pending
blueprint_review_get_summary
blueprint_review_approve
blueprint_review_reject
blueprint_review_open_diff
```

注意：

- `approve` 和 `reject` 都是写操作。
- `open_diff` 是编辑器 UI 操作。
- `get_summary` 是读操作。
- Agent 不应默认替用户 approve，除非用户明确授权。

---

## 九、验收标准

第一版可按以下标准验收：

1. Agent 添加一个蓝图变量后，面板出现一条待审阅 session。
2. 面板能显示目标资产、操作类型、变量名称、成功状态。
3. 用户点击 Reject 后变量从蓝图中恢复到修改前状态。
4. Agent 添加节点并连线后，面板能打开 UE 蓝图 Diff 或显示可读摘要。
5. 蓝图编译失败时，session 明确标红并显示错误摘要。
6. 用户点击 Approve & Save 后资产保存，session 状态变为 approved。
7. 用户手工修改同一资产后，session 标记为 mixed_changes，不允许无提示自动回滚。
8. 插件重启后，未完成 session 能从 Saved 目录恢复摘要状态。

---

## 十、最终建议

建议实现。

优先级应高于继续增加更多写工具，因为审阅功能会显著降低 Agent 写蓝图的使用风险。推荐先实现“已应用待审阅”模式，再扩展 dry-run 预检。第一版重点不是做出最完整的视觉 Diff，而是建立可靠的审阅会话、操作记录、回滚边界和用户确认机制。
