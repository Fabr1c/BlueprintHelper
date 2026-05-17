# Baseline Semantic Hash 全量替换迁移计划

日期：2026-05-17

状态：决策已定稿，等待实现。

## 1. 硬性规则

1. Stage 2/3 直接使用 semantic target snapshot hash 全量替换旧 graph hash。
2. 不保留 legacy graph hash 兼容层。
3. 新 Review record 的 `BaselineHash` / `RecordedAfterHash` 必须来自统一 semantic target snapshot hash。
4. Graph node / graph block 也必须迁移到同一套 semantic hash，不再继续写入旧 graph hash。
5. Reject 前统一按当前 target semantic hash 校验 `RecordedAfterHash`。
6. 当前 hash 与 `RecordedAfterHash` 不一致时，Reject 返回 `needs_action`，不执行回滚。
7. snapshot-restore 类目标缺少可恢复 `BeforeSnapshotJson` 时，Reject 返回 `needs_action`。
8. 内部 snapshot artifact 可以保留 UE GUID 作为定位/hash 输入；Agent-facing 和 summary 输出不得暴露 GUID 字段。

## 2. 旧数据策略

本迁移不兼容旧 hash。

旧 pending Review records 的处理规则：

1. 迁移前生成、且仍使用 legacy graph hash 的 pending records，不保证继续可 reject。
2. 实现后遇到旧 hash record，应进入 `needs_action` 或要求重新生成 Review evidence。
3. 不做旧 hash 到新 hash 的自动转换。
4. 不做双写、双读、fallback verifier。
5. 文档和 UI 诊断需要明确提示：旧记录需要重新执行任务、重新生成 Review，或在迁移前完成 accept/reject。

## 3. 影响区域

### Review baseline capture

涉及文件：

- `Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewBaselineSnapshotService.h`
- `Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewBaselineSnapshotService.cpp`
- `Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp`

影响：

- `CaptureTargetSnapshot` 成为 hash 的唯一主数据源。
- Graph node / graph block 需要补全 target snapshot 结构。
- target snapshot JSON 必须稳定序列化，避免字段顺序、临时字段、时间戳进入 hash payload。
- `exists=false` 必须参与 hash，用于新增/删除目标的 before/after 判定。

### ReviewStore / visible changes

涉及文件：

- `Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewStoreService.cpp`
- `Source/BlueprintHelper/Private/Systems/Review/Utils/BlueprintHelperReviewStoreTargetUtils.cpp`
- `Source/BlueprintHelper/Private/Systems/Review/Utils/BlueprintHelperReviewStoreMergeUtils.cpp`

影响：

- `BaselineHash` / `RecordedAfterHash` 的来源统一为 semantic target snapshot hash。
- `BeforeSnapshotJson` / `AfterSnapshotJson` 成为 Review/Diff/Reject 的主数据。
- `RemoveNetNoChangeVisibleChanges` 继续使用 hash 相等判断，但输入必须是 semantic hash。
- 缺少可恢复 before snapshot 的 snapshot-restore target 必须标记 `needs_action`。

### Graph write / journal producer

涉及文件：

- `Source/BlueprintHelper/Private/Systems/Transactions/BlueprintHelperTransactionJournalService.cpp`
- `Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.cpp`

影响：

- 不再生成或传递 legacy graph hash。
- `BaselineHashesByTargetKey` / `RecordedAfterHashesByTargetKey` 如继续保留，只能保存 semantic hash。
- Graph node/block 的 before/after snapshot 应由统一 snapshot service 生成。

### Reject

涉及文件：

- `Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewActionService.cpp`
- `Source/BlueprintHelper/Private/Systems/Review/Utils/BlueprintHelperReviewRejectService.cpp`
- `Source/BlueprintHelper/Private/Systems/Review/Utils/BlueprintHelperReviewSnapshotRestoreService.cpp`

影响：

- Reject verifier 只计算当前 semantic target snapshot hash。
- 不再调用旧 graph hash verifier 作为兼容 fallback。
- current semantic hash 不匹配时返回 `needs_action:current_state_changed:<target_key>`。
- graph target 仍可使用 rollback journal 执行实际图回滚，但 guard 必须来自 semantic hash。

### DebugBundle / Review summary

涉及文件：

- `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewDebugBundleService.cpp`
- `Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewStoreService.cpp`

影响：

- Debug summary 应显示 semantic snapshot 是否存在、schema、hash source、baseline trust、dirty policy、disk refs、semantic refs。
- Summary 输出不得暴露 GUID。
- 本地 DebugBundle artifact 可以保留完整内部 snapshot 文件，用于开发诊断。

### Configure / retention policy

涉及区域：

- Codex/Agent 配置文档
- BlueprintHelper configure/profile 输出
- Review cleanup / compaction 后续实现

需要新增用户可选档位：

| Mode | 行为 | 用途 |
|---|---|---|
| `diagnostic` | 保留完整 semantic baseline、per-target before/after snapshot、disk evidence。 | 开发调试 |
| `standard` | 默认。Pending / needs_action / reject_failed 保留完整 snapshot；accept/reject 后压缩为 summary + hash + 必要诊断字段。 | 日常推荐 |
| `lean` | 只保留可恢复所需 snapshot 与 hash；关闭后尽量只留 summary/hash。 | 长期低占用 |

## 4. 实施计划

### Step 1：定义 canonical semantic hash payload

- 建立统一 helper：输入 target snapshot JSON，输出稳定 hash。
- hash payload 必须排除 `captured_at`、临时 warning、debug-only 字段。
- hash 字符串可以继续使用 `crc32_xxxxxxxx` 形态，但语义上只代表 semantic target snapshot hash。
- 不添加 `legacy_graph_v1` 或其他兼容前缀。

### Step 2：补全 graph target snapshot

- Graph node snapshot 支持 `graph_node`、`graph_pin`、`graph_link` 归并到 node 级 target。
- Graph block snapshot 根据 `BlueprintHelperBlockId` 收集 block 内节点。
- block hash 基于 block target snapshot，不再基于旧 `ComputeGraphBlockHash`。
- 新增/删除节点时，before 或 after snapshot 必须能表达 `exists=false`。

### Step 3：ReviewStore 全面改用 semantic snapshots

- build review record 时优先使用 evidence 内 before/after snapshots。
- 如 evidence 缺少 before snapshot，则从 ArchiveSession 的 `baseline.semantic.json` 解析 target snapshot。
- 如仍缺少可恢复 before snapshot，snapshot-restore 类 target 标记 `needs_action`。
- 所有新记录写入 semantic `BaselineHash` / `RecordedAfterHash`。

### Step 4：Reject verifier 切换

- 删除 Reject 路径对旧 graph hash verifier 的依赖。
- Reject 时重新捕获当前 target snapshot 并计算 semantic hash。
- semantic hash 不匹配时进入 `needs_action`。
- hash 通过后再执行 snapshot restore 或 graph rollback journal。

### Step 5：Debug / summary 对齐

- Review summary 输出 semantic snapshot schema、hash source、presence flags。
- DebugBundle 复制 semantic artifacts，并显示 retention mode。
- Agent-facing summary 不输出 GUID。

### Step 6：配置 retention 档位

- configure 文档新增 `review_snapshot_retention_mode`。
- 默认值为 `standard`。
- 实现前文档标记为 planned，避免误导用户以为当前已可配置。

## 5. 验证计划

必须新增或更新自动化：

1. `ReviewBaselineSemanticHashCapturesGraphNode`
2. `ReviewBaselineSemanticHashCapturesGraphBlock`
3. `ReviewStoreUsesSemanticHashForGraphTarget`
4. `ReviewStoreUsesSemanticHashForSnapshotRestoreTarget`
5. `ReviewRejectBlocksWhenSemanticHashChanged`
6. `ReviewRejectGraphTargetUsesSemanticHashGuard`
7. `ReviewRejectNeedsActionWithoutRecoverableBeforeSnapshot`
8. `ReviewDebugBundleSummarizesSemanticHashSource`
9. `ReviewAgentFacingSummaryDoesNotExposeGuid`
10. `ReviewOldLegacyHashRecordNeedsAction`

验证命令应使用 UBT / UE Automation / CLI，不新增废弃 MCP 工具测试。

## 6. 风险与接受条件

风险：

1. 旧 pending records 不兼容，会产生一次性迁移成本。
2. Graph hash payload 扩展后可能改变净变更折叠行为。
3. snapshot JSON 稳定性不足会导致误报 `current_state_changed`。
4. retention 档位未实现前，semantic artifact 可能增加 Saved 目录体积。

接受条件：

1. 新生成 Review records 不再写入旧 graph hash。
2. Graph 和非 Graph target 都使用同一 semantic hash 入口。
3. Reject hash guard 对所有支持 target kind 行为一致。
4. 缺少可恢复 before snapshot 的 target 不执行猜测回滚。
5. DebugBundle 可以解释 hash 来源、snapshot 存在性和 retention 状态。
6. Agent-facing 输出没有 GUID 字段。

## 7. 非目标

1. 不保留旧 graph hash 兼容层。
2. 不自动迁移旧 pending Review records。
3. 不通过 MCP 废弃工具验证。
4. 不在本阶段实现 Accept 后完整 compaction，除非 retention 配置阶段明确排期。

## 8. 2026-05-17 S2/S3 Implementation Closeout

状态：S2/S3 core path 已完成并通过编译与目标自动化验证。

已完成内容：

1. 删除旧 `FBlueprintHelperReviewHashService` 实现与所有 `ComputeAtomicTargetHash` 调用点。
2. `FBlueprintHelperReviewBaselineSnapshotService` 成为唯一 semantic target snapshot hash 入口。
3. `CaptureTargetSnapshot` 支持 graph node / graph block，并使用 canonical JSON 计算 `crc32_xxxxxxxx` hash。
4. ReviewStore 优先使用 before/after target snapshot 写入 `BaselineHash` / `RecordedAfterHash`；缺少可恢复 before snapshot 的 snapshot-restore target 标记为 `needs_action`。
5. Reject 默认路径改为先按当前 semantic target snapshot hash 校验 `RecordedAfterHash`；不再执行 legacy graph hash fallback。
6. GraphWrite journal / ReplaceGraph 不再传递旧 graph hash；改为传递 target snapshot JSON。
7. DebugBundle / Review summary 增加 `hash_source=semantic_target_snapshot`、snapshot schema、retention mode 摘要字段；agent-facing summary 不暴露 GUID。
8. 新增自动化覆盖 graph node、graph block、Store semantic hash、Reject semantic guard、缺失 recoverable snapshot、DebugBundle hash source。

验证：

1. `Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -FromMsBuild`：通过。
2. `BlueprintHelper.Review.Baseline.SemanticHashCapturesGraphNode`：通过。
3. `BlueprintHelper.Review.Baseline.SemanticHashCapturesGraphBlock`：通过。
4. `BlueprintHelper.Review.Store.UsesSemanticHashForGraphTarget`：通过。
5. `BlueprintHelper.Review.Store.UsesSemanticHashForSnapshotRestoreTarget`：通过。
6. `BlueprintHelper.Review.Reject.BlocksWhenSemanticHashChanged`：通过。
7. `BlueprintHelper.Review.Reject.GraphTargetUsesSemanticHashGuard`：通过。
8. `BlueprintHelper.Review.Reject.NeedsActionWithoutRecoverableBeforeSnapshot`：通过。
9. `BlueprintHelper.Review.DebugBundle.SummarizesSemanticHashSource`：通过。
10. `BlueprintHelper.Review.Summary.AgentFacingSummaryDoesNotExposeGuid`：通过。
11. `BlueprintHelper.Review.Reject.OldLegacyHashRecordNeedsAction`：通过。
12. `BlueprintHelper.Review.Action` regression：18 项通过。
13. `BlueprintHelper.Review.Rollback.RejectRemovesOnlySelectedGraphNode`：通过。

备注：

- UE 启动日志仍会出现与本轮无关的 EOS/JetBrains port/early `Condition failed` 噪声，但 Automation 结果均为 `Result={成功}` 且退出码为 0。
- Retention mode 本轮作为 summary/config contract 字段落地为 `standard` 摘要；Accept 后 compaction 仍属于非目标项，未在本阶段实现。
