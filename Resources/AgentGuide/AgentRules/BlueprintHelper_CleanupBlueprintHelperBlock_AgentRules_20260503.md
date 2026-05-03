# BlueprintHelper Agent 侧规则：CleanupBlueprintHelperBlock 使用规范

日期：2026-05-03  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：CleanupBlueprintHelperBlock Agent 侧规则确认稿  
本文边界：规定 Agent 如何调用和解释 CleanupBlueprintHelperBlock，包括 cleaned_ref、missing_policy、dry_run、失败诊断、ownership 冲突和 rollback 处理。UE 字段映射见独立文档。

---

## 1. 工具职责

`CleanupBlueprintHelperBlock` 只负责：

```text
删除一个明确的 BlueprintHelper-owned block。
```

Agent 应把它理解为：

```text
破坏性删除工具。
只允许删除 BlueprintHelper-owned block。
```

---

## 2. 明确目标规则

CleanupBlueprintHelperBlock 只接受明确目标：

```text
block_id
```

或：

```text
asset_path + graph_id + block_ref
```

Agent 不得用以下字段模糊清理：

```text
entry_name
display_name
custom_event name
graph name alone
feature name
```

这些属于 CleanupBlueprintHelperFeature，而不是 Block cleanup。

---

## 3. target 规则

顶层 `target` 只表示执行路由范围。

字段：

```text
asset_path
graph
cleanup_scope
block_ref
block_id
```

Agent 不应期待：

```text
target_type
```

---

## 4. 成功返回极简规则

CleanupBlock 成功返回不是删除 diff，也不是 Journal 摘要。

Agent 只读取：

```text
data.cleanup_result.cleaned_ref
data.write_ref.transaction_id
validation
```

示例：

```json
{
  "data": {
    "schema": "CleanupBlueprintHelperBlock.v1",
    "cleanup_result": {
      "cleaned_ref": {
        "graph_id": "EG_PhysicsDoor",
        "block_ref": "TogglePhysicsDoor0"
      }
    },
    "write_ref": {
      "transaction_id": "tx_20260503_1601",
      "journal_recorded": true
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

Agent 不应期待：

```text
deleted_nodes
deleted_links
summary
external_dependencies
external_dependents
ownership
review
safety
diagnostics
next
rollback_data
```

这些属于 Journal / Review / verbose/debug。

---

## 5. cleaned_ref 规则

`cleaned_ref` 表示本次正式清理的 owned block。

优先形式：

```text
graph_id
block_ref
```

完整 block_id：

```text
full_block_id = graph_id + "_" + block_ref
```

如果工具只能安全返回完整 block_id，则允许：

```text
cleaned_ref.block_id
```

但默认应使用 graph_id + block_ref，与 Append / Replace 的 block_ref 规则一致。

---

## 6. missing_policy

支持：

```text
error
ignore
```

默认：

```text
error
```

解释：

| policy | Agent 解释 |
|---|---|
| `error` | block 不存在时报错。 |
| `ignore` | block 不存在时返回 no_op。适合恢复、批处理、重复清理。 |

---

## 7. no_op 规则

如果：

```text
missing_policy=ignore
目标 block 缺失
```

工具返回：

```text
status=no_op
modified=false
data.cleanup_result.missing=true
```

Agent 应理解：

```text
目标已不存在，本次无需修改。
```

no_op 通常：

```text
validation.should_compile=false
validation.should_save=false
```

---

## 8. ownership 规则

CleanupBlueprintHelperBlock 只能删除 BlueprintHelper-owned block。

如果目标不是 owned：

```text
error.code=target_not_owned
error.stage=ownership_check
```

Agent 不得绕过 ownership 冲突删除用户节点或来源不明节点。

---

## 9. dry_run 规则

明确 block cleanup 的 dry_run passed 极简：

```json
{
  "dry_run": {
    "result": "passed",
    "can_execute": true
  }
}
```

dry_run blocked：

```json
{
  "dry_run": {
    "result": "blocked",
    "can_execute": false,
    "blocked_by": [
      "external_dependents_exist"
    ],
    "conflicts": [
      {
        "code": "external_dependents_exist",
        "message": "The target block is referenced by external logic."
      }
    ],
    "errors": []
  }
}
```

Agent 规则：

```text
1. result=passed + can_execute=true：可进入正式 cleanup。
2. result=blocked + can_execute=false：不得正式 cleanup。
3. ok=true/status=dry_run 只表示 dry_run 工具运行成功。
4. dry_run 工具自身失败才是 ok=false/status=failed/error。
```

dry_run 不返回 would_delete_nodes / would_delete_links。

---

## 10. 正式失败规则

正式失败不返回：

```text
cleanup_result
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

最小错误字段：

```text
code
stage
message
retryable
rollback_result
```

---

## 11. rollback_result 规则

| rollback_result | Agent 解释 |
|---|---|
| `not_needed` | resolve_target / ownership_check 阶段失败，未修改资产。 |
| `rolled_back` | 写入中失败但已回滚，通常 modified=false。 |
| `blocked` | 回滚被阻断，可能残留修改，必须 stop_and_report。 |
| `failed` | 回滚失败，可能残留修改，必须 stop_and_report。 |

如果 rollback blocked / failed：

```text
Agent 不得继续 compile / save / cleanup / patch / merge / replace。
```

---

## 12. validation 规则

成功 cleanup 通常返回：

```text
validation.should_compile=true
validation.should_save=true
```

no_op 通常返回：

```text
validation.should_compile=false
validation.should_save=false
```

Agent 应根据 validation 继续 compile/save 闭环。

---

## 13. Agent 禁止行为

Agent 不得：

```text
1. 用 CleanupBlueprintHelperBlock 清理用户节点。
2. 用 CleanupBlueprintHelperBlock 做 feature 模糊清理。
3. 仅凭 entry_name / display_name 清理 block。
4. 期待成功返回 deleted_nodes / deleted_links。
5. 期待成功返回 ownership。
6. 忽略 target_not_owned 错误。
7. 在 rollback blocked / failed 后继续写入。
```

---

## 14. 最终报告规则

正常完成时，Agent 可报告：

```text
1. 哪个资产 / 图表的哪个 block 被清理。
2. cleanup 是否 no_op。
3. 编译 / 保存结果。
4. 异常或未完成项。
```

不默认报告：

```text
transaction_id
review_status
journal_path
rollback_data
deleted_nodes / deleted_links 明细
```

---

## 15. 验收标准

```text
1. Agent 能识别 CleanupBlock 只清理明确 owned block。
2. Agent 不用 CleanupBlock 做模糊 feature 清理。
3. Agent 能解析 cleanup_result.cleaned_ref。
4. Agent 能处理 missing_policy=ignore 的 no_op。
5. Agent 能处理 ownership 冲突。
6. Agent 不期待 deleted_nodes / deleted_links。
7. Agent 能处理 dry_run passed / blocked。
8. Agent 能处理 error.rollback_result。
