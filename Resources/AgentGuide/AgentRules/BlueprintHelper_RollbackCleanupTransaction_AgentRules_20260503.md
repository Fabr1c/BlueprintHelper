# BlueprintHelper Agent 侧规则：RollbackCleanupTransaction 使用规范

日期：2026-05-03  
状态：Agent 侧规则确认稿  
范围：Agent 如何调用和解释 RollbackCleanupTransaction，包括 transaction_id 定位、dry_run、rollback_summary、正式成功、no_op、失败诊断和 stop_and_report。

---

## 1. 工具职责

`RollbackCleanupTransaction` 只负责回滚一次 Cleanup 工具产生的 transaction。第一版只适用于：

```text
CleanupBlueprintHelperBlock
CleanupBlueprintHelperFeature
```

不适用于任意 Graph Write 回滚、资产创建回滚、普通 Component / Class Settings 回滚、无 rollback_data 的 transaction、已经 compacted 的 transaction。需要通用 transaction rollback 时，应等待或设计 `RollbackTransaction`，不能扩大解释本工具。

---

## 2. 目标定位规则

RollbackCleanupTransaction 必须以 `transaction_id` 作为唯一事实来源。

允许的 target 字段：

```text
transaction_id
asset_path
rollback_scope
```

Agent 不应使用以下字段决定 rollback 目标：

```text
block_ref
feature_name
graph
entry_name
display_name
```

这些字段可能已经过期。rollback 的事实来源是 Transaction Journal / rollback_data。

---

## 3. rollback_scope

第一版固定：

```text
rollback_scope=cleanup_transaction
```

Agent 不应期待其他 rollback_scope。

---

## 4. 必须 dry_run

Rollback 是高风险写操作，必须 dry_run。dry_run 用于检查：

```text
1. transaction 是否存在。
2. transaction 是否属于 cleanup。
3. rollback_data 是否完整。
4. transaction 是否已被回滚。
5. rollback 目标位置是否仍可恢复。
6. 当前资产状态是否与 Journal 预期一致。
7. 是否存在用户或后续 transaction 修改冲突。
```

dry_run 通过前，Agent 不得执行正式 rollback。

---

## 5. dry_run 返回解释

dry_run passed 示例：

```json
{
  "dry_run": {
    "result": "passed",
    "can_execute": true,
    "rollback_summary": {
      "affected_assets": 1,
      "restorable_blocks": 3,
      "restorable_nodes_available": true,
      "rollback_data_available": true
    },
    "blocked_by": [],
    "conflicts": [],
    "errors": []
  }
}
```

Agent 应读取：

```text
data.dry_run.result
data.dry_run.can_execute
data.dry_run.rollback_summary
data.dry_run.blocked_by
data.dry_run.conflicts
data.dry_run.errors
```

dry_run blocked 仍表示 dry_run 工具运行成功：

```text
ok=true
status=dry_run
modified=false
```

Agent 应根据 `result=blocked / can_execute=false` 停止正式 rollback。

---

## 6. rollback_summary 规则

`rollback_summary` 是恢复可行性摘要，不是 Journal 摘要。

保留字段：

```text
affected_assets
restorable_blocks
restorable_nodes_available
rollback_data_available
```

可选扩展：

```text
restorable_links_available
asset_state_checked
```

明确不包含：

```text
transaction_id
source_operation
rollback_data
node snapshots
full diff
```

原因：`target.transaction_id` 已经提供被回滚 transaction，`operation=rollback_cleanup_transaction` 已说明工具语义。source_operation 由 Journal 判断；若类型不匹配，应通过 `error.code=transaction_type_mismatch` 表达。

---

## 7. 正式成功返回

正式成功只读取：

```text
data.rollback_result
data.write_ref.transaction_id
validation
```

示例：

```json
{
  "data": {
    "schema": "RollbackCleanupTransaction.v1",
    "rollback_result": {
      "rolled_back_transaction_id": "tx_20260503_1704",
      "rollback_status": "succeeded"
    },
    "write_ref": {
      "transaction_id": "tx_20260503_1804",
      "journal_recorded": true
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

---

## 8. 两个 transaction_id 的区别

正式 rollback 成功后，Agent 必须区分：

```text
data.rollback_result.rolled_back_transaction_id
data.write_ref.transaction_id
```

| 字段 | 含义 |
|---|---|
| `rolled_back_transaction_id` | 被回滚的 cleanup transaction。 |
| `write_ref.transaction_id` | 本次 rollback 写操作自己的 transaction。 |

Agent 不得把两者混用。

---

## 9. 成功返回不包含的内容

Agent 不应期待：

```text
restored_nodes
restored_links
rollback_data
node snapshots
full snapshot
source_operation
review
safety
diagnostics
next
```

这些属于 Transaction Journal / Review / verbose/debug。

---

## 10. no_op 规则

如果目标 cleanup transaction 已经成功 rollback，且调用允许幂等：

```text
already_rolled_back_policy=ignore
```

工具返回：

```text
status=no_op
modified=false
rollback_status=already_rolled_back
```

默认：

```text
already_rolled_back_policy=error
```

---

## 11. 正式失败规则

正式失败不返回：

```text
rollback_result
write_ref
review
safety
diagnostics
next
```

但必须读取：

```text
error.code
error.stage
error.message
error.retryable
error.rollback_result
error.failed_item
error.conflicts
```

如果被请求 transaction 不是 cleanup transaction，应返回：

```text
error.code=transaction_type_mismatch
error.stage=validate_transaction
rollback_result=not_needed
modified=false
```

Agent 不得尝试用 RollbackCleanupTransaction 回滚其他写工具 transaction。

---

## 12. error.rollback_result 解释

错误中的 `error.rollback_result` 表示：

```text
本次 rollback 写操作失败后，对本次 rollback 的恢复结果。
```

不是目标 cleanup transaction 的成功状态。

| error.rollback_result | Agent 解释 |
|---|---|
| `not_needed` | resolve / validate 阶段失败，未修改资产。 |
| `rolled_back` | 本次 rollback 写操作中途失败，但已撤销本次部分恢复动作。 |
| `blocked` | 本次 rollback 写操作回滚被阻断，可能残留修改。 |
| `failed` | 本次 rollback 写操作无法恢复到之前状态，可能残留修改。 |

如果 `rollback_result=blocked/failed` 且 `modified=true`，Agent 必须 stop_and_report，不得继续 compile/save/cleanup/patch/merge/replace。

---

## 13. validation 规则

正式成功通常：

```text
validation.should_compile=true
validation.should_save=true
```

no_op 通常：

```text
validation.should_compile=false
validation.should_save=false
```

Agent 应根据 validation 继续 compile/save 闭环。

---

## 14. Agent 禁止行为

```text
1. 用 RollbackCleanupTransaction 做通用 transaction rollback。
2. 用 block_ref / feature_name / graph 重新推断 rollback 目标。
3. 在未 dry_run passed 前执行正式 rollback。
4. 期待 dry_run 返回 rollback_data / node snapshots。
5. 期待成功返回 restored_nodes / restored_links。
6. 混淆 rolled_back_transaction_id 与 write_ref.transaction_id。
7. 在 rollback blocked / failed 后继续写入。
```

---

## 15. 最终报告规则

正常完成时，Agent 可报告：

```text
1. 哪个 cleanup transaction 已回滚。
2. 是否产生新的 rollback transaction。
3. 编译 / 保存结果。
4. 异常或未完成项。
```

不默认报告 Journal 路径、Review 状态、rollback_data、node snapshot、restored_nodes / restored_links 明细。

---

## 16. 验收标准

```text
1. Agent 能识别 RollbackCleanupTransaction 只回滚 cleanup transaction。
2. Agent 使用 transaction_id 定位 rollback 目标。
3. Agent 不用 block_ref / feature_name 重推 rollback 目标。
4. Agent 能解析 rollback_summary，但不期待 rollback_data。
5. Agent 知道 rollback_summary 不包含 transaction_id / source_operation。
6. Agent 能区分 rolled_back_transaction_id 与 write_ref.transaction_id。
7. Agent 能处理 already_rolled_back_policy=ignore 的 no_op。
8. Agent 能处理 error.rollback_result。
9. Agent 在 rollback blocked / failed 后 stop_and_report。
```
