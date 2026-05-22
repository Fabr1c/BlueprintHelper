# BlueprintHelper GraphLayout 边界问题记录 2026-05-22

## 范围

本文记录 GraphLayout 第一版落地后的边界问题修复。GraphLayout 仍保持后置视觉排版职责：

- TaskSpec / TaskPlan / GraphWrite 只负责生成节点和连线。
- GraphLayout 在 Task 全部内容生成完成后处理视觉位置。
- Layout 不写 Review diff，不改变 TaskRun success / failed。

## 1. 多个 CustomEvent 同 Task 生成时重叠

### 现象

一个 TaskSpec 同时创建多个 CustomEvent 时，新建事件节点初始都在 `(0, 0)`，Task 结束后的 GraphLayout 没有把它们上下排开。

### 原因

GraphLayout solver 已经支持多个 `EventEntry` root 按 root index 分配纵向 lane，但 SignatureService 创建 CustomEvent 时没有调用 `FBlueprintHelperGraphLayoutCoordinator::RecordGeneratedNodes`。

因此这些新 CustomEvent 在 snapshot 中被视为 existing node。默认 RuleSet 中 `move_existing_nodes=false`，apply 阶段会跳过它们，导致它们保留创建时的重叠坐标。

### 修复

- SignatureService 新建 CustomEvent 并成功 commit 后，将新事件节点登记到 GraphLayout generated-node 集合。
- 新 graph 分支和现有 graph 新建事件分支都登记。
- 只登记真正新建的 CustomEvent；已有 CustomEvent 只补 pin 时不强制移动，避免破坏用户已有布局。

### 验收

- 同一 TaskSpec 创建多个 CustomEvent 时，Task 结束后的 GraphLayout flush 能看到所有新事件节点。
- 新事件节点按 `EventEntry` root lane 上下排布，而不是停留在 `(0, 0)` 重叠。
- 2026-05-22：`TemplateEditor Win64 Development` 编译通过。
- 2026-05-22：在现有 smoke 蓝图上执行同一 TaskSpec 内两个 `ensure_custom_event`，preview 通过、execute 完成，`blueprinthelper_read_context` 可读回目标 graph 内 2 个 custom event。该 smoke 使用 `should_save=false`，验证后关闭 editor 且不保存。
- 当前公开 ReadSpec 不输出节点坐标，因此本轮没有把最终 `NodePosX/NodePosY` 作为 CLI 读回证据；源码修复点是让 SignatureService 新建 CustomEvent 进入 GraphLayout generated-node 集合，避免 apply 阶段按 existing node 跳过。
