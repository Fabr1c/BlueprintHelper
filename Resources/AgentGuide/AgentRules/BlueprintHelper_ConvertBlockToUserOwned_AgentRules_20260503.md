# BlueprintHelper Agent 侧规则：ConvertBlueprintHelperBlockToUserOwned 使用规范

日期：2026-05-03  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：ConvertBlueprintHelperBlockToUserOwned Agent 侧规则确认稿  
本文边界：规定 Agent 如何调用和解释 ConvertBlueprintHelperBlockToUserOwned，包括 ownership 转换语义、成功极简返回、dry_run、no_op、失败诊断，以及不返回 transaction / write_ref 的规则。UE 字段映射见独立文档。

---

## 1. 工具职责

`ConvertBlueprintHelperBlockToUserOwned` 负责：

```text
把一个明确的 BlueprintHelper-owned block 转为 user-owned。
```

Agent 应理解：

```text
转换后，该 block 不再由 BlueprintHelper 默认管理。
```

工具只修改 ownership 标记与审计状态，不修改业务逻辑。

---

## 2. 不负责的事情

本工具不负责：

```text
删除节点
删除连线
修改蓝图业务逻辑
替换图表实现
Patch 节点 / Pin
Merge 执行流
Cleanup
Rollback
重新接管 user-owned 节点
```

---

## 3. 目标定位规则

调用输入层可以使用：

```text
block_id
```

或：

```text
graph_id + block_ref
```

但 Agent-facing 成功返回不回显：

```text
block_ref
block_id
graph_id
converted_ref
```

原因：

```text
转换成功后，该内容已脱离 BlueprintHelper 管理，不应继续把 managed handle 返回给 Agent。
```

---

## 4. target 规则

顶层 `target` 只表示执行路由范围。

字段：

```text
asset_path
graph
ownership_scope
```

Agent 不应期待：

```text
target_type
target_kind
block_ref
block_id
```

第一版：

```text
ownership_scope=block
```

---

## 5. 必须 dry_run

本工具必须 dry_run。

原因：

```text
1. ownership 边界变化会影响后续 Cleanup / Replace / Patch 管理权限。
2. 转换后目标内容不再默认由 BlueprintHelper 管理。
3. 需要在写入前确认目标确实是 BlueprintHelper-owned block。
```

---

## 6. dry_run passed

dry_run passed 极简：

```json
{
  "dry_run": {
    "result": "passed",
    "can_execute": true
  }
}
```

Agent 不应期待：

```text
ownership_change_summary
current_owner
new_owner
metadata diff
node list
journal_will_record_conversion
```

---

## 7. dry_run blocked

blocked 时读取：

```text
blocked_by
conflicts
errors
```

冲突项可以包含：

```text
block_id
block_ref
ref
```

用于定位问题。

示例：

```json
{
  "dry_run": {
    "result": "blocked",
    "can_execute": false,
    "blocked_by": [
      "target_not_owned"
    ],
    "conflicts": [
      {
        "code": "target_not_owned",
        "block_id": "EG_PhysicsDoor_TogglePhysicsDoor0",
        "message": "The target block is not BlueprintHelper-owned."
      }
    ],
    "errors": []
  }
}
```

---

## 8. 成功返回极简规则

正式成功只读取：

```text
data.conversion_result.converted_count
validation
```

示例：

```json
{
  "ok": true,
  "status": "applied",
  "modified": true,
  "data": {
    "schema": "ConvertBlueprintHelperBlockToUserOwned.v1",
    "conversion_result": {
      "converted_count": 1
    }
  },
  "validation": {
    "should_compile": false,
    "should_save": true
  }
}
```

Agent 不应期待：

```text
converted_ref
graph_id
block_ref
block_id
converted_nodes
metadata_removed
comments_rewritten
metadata_before
metadata_after
write_ref
transaction_id
journal_recorded
review
safety
diagnostics
next
```

---

## 9. 成功结果不返回 write_ref

本工具成功结果不返回：

```text
write_ref
transaction_id
journal_recorded
```

Agent 应理解：

```text
1. transaction_id / Journal / Review / rollback_data 是 UE 侧内部审计系统。
2. Agent 默认不需要知道事务信息。
3. 如果用户要审查或回滚，应通过 UE Review UI、Journal 查询工具、Debug 工具或显式导出流程。
```

Agent 不得把普通执行返回体当成事务索引来源。

---

## 10. no_op 规则

如果目标已经是 user-owned，且调用允许幂等：

```text
already_user_owned_policy=ignore
```

工具返回：

```text
status=no_op
modified=false
converted_count=0
conversion_status=already_user_owned
```

示例：

```json
{
  "status": "no_op",
  "modified": false,
  "data": {
    "conversion_result": {
      "converted_count": 0,
      "conversion_status": "already_user_owned",
      "already_user_owned_policy": "ignore"
    }
  },
  "validation": {
    "should_compile": false,
    "should_save": false
  }
}
```

默认：

```text
already_user_owned_policy=error
```

---

## 11. 正式失败规则

正式失败不返回：

```text
conversion_result
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

## 12. 错误定位信息

只有在错误或冲突中返回目标定位信息：

```text
error.failed_item.block_id
conflicts[].block_id
conflicts[].ref
```

成功 / no_op 不返回 block handle。

---

## 13. rollback_result 规则

| rollback_result | Agent 解释 |
|---|---|
| `not_needed` | resolve / ownership_check 阶段失败，未修改资产。 |
| `rolled_back` | 写入中失败但已回滚，通常 modified=false。 |
| `blocked` | 回滚被阻断，可能残留修改，必须 stop_and_report。 |
| `failed` | 回滚失败，可能残留修改，必须 stop_and_report。 |

如果：

```text
rollback_result=blocked
rollback_result=failed
modified=true
```

Agent 必须 stop_and_report，不得继续 cleanup / patch / merge / replace / save。

---

## 14. validation 规则

正式成功通常：

```text
validation.should_compile=false
validation.should_save=true
```

原因：

```text
ownership metadata / comment 修改通常不改变 K2 逻辑，不需要编译；但资产已修改，需要保存。
```

no_op 通常：

```text
validation.should_compile=false
validation.should_save=false
```

---

## 15. Agent 禁止行为

Agent 不得：

```text
1. 把转换成功后的 block 当成 BlueprintHelper-owned 继续管理。
2. 期待成功返回 block_ref / block_id。
3. 期待成功返回 write_ref / transaction_id。
4. 期待成功返回 metadata diff / node list。
5. 在未 dry_run passed 前正式转换。
6. 在 target_not_owned 后强行转换。
7. 在 rollback blocked / failed 后继续写入。
```

---

## 16. 最终报告规则

正常完成时，Agent 可报告：

```text
1. ownership 转换是否完成。
2. 转换数量。
3. 是否需要保存。
4. 异常或未完成项。
```

不默认报告：

```text
transaction_id
review_status
journal_path
block_id
metadata diff
node list
```

---

## 17. 验收标准

```text
1. Agent 能识别 Convert 只做 ownership 转换。
2. Agent 知道成功后目标不再 managed。
3. Agent 不期待成功返回 block_ref / block_id。
4. Agent 不期待成功返回 write_ref / transaction_id。
5. Agent 能解析 converted_count。
6. Agent 能处理 dry_run passed / blocked。
7. Agent 能处理 already_user_owned_policy=ignore 的 no_op。
8. Agent 只在 error / conflicts 中读取问题 block_id / ref。
9. Agent 能处理 error.rollback_result。
10. Agent 在 rollback blocked / failed 后 stop_and_report。
```
