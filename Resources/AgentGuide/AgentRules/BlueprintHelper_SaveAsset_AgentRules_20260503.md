# BlueprintHelper Agent 侧规则：save_asset 使用规范

日期：2026-05-03  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：save_asset Agent 侧规则确认稿  
本文边界：规定 Agent 如何调用和解释 save_asset，包括保存成功、no_op、保存失败和无 validation / transaction 返回规则。UE 字段映射见独立文档。

---

## 1. 工具职责

`save_asset` 负责：

```text
保存一个明确资产。
```

它不负责：

```text
编译
修复
修改蓝图
生成 transaction_id
进入 Review
```

---

## 2. 保存成功

示例：

```json
{
  "ok": true,
  "status": "completed",
  "modified": false,
  "data": {
    "schema": "SaveAsset.v1",
    "save_result": {
      "saved": true,
      "was_dirty": true
    }
  }
}
```

Agent 应理解：

```text
资产已保存。
```

---

## 3. no_op：资产不 dirty

示例：

```json
{
  "ok": true,
  "status": "no_op",
  "modified": false,
  "data": {
    "schema": "SaveAsset.v1",
    "save_result": {
      "saved": false,
      "was_dirty": false,
      "reason": "asset_not_dirty"
    }
  }
}
```

Agent 应理解：

```text
资产本来没有未保存修改，本次无需保存。
```

---

## 4. 保存失败

示例：

```json
{
  "ok": false,
  "status": "failed",
  "modified": false,
  "error": {
    "code": "save_failed",
    "stage": "save_package",
    "message": "The asset package could not be saved.",
    "retryable": true,
    "conflicts": [
      {
        "code": "file_locked",
        "message": "The asset file appears to be locked by another process."
      }
    ]
  }
}
```

Agent 应根据 error 判断是否可重试或 stop_and_report。

---

## 5. 不返回 validation

save 工具不返回：

```text
validation
```

原因：

```text
save_asset 本身就是保存闭环动作。
```

Agent 不应期待：

```text
validation.saved
validation.should_save
```

---

## 6. 不返回事务信息

save 工具不返回：

```text
write_ref
transaction_id
journal_recorded
review
safety
rollback_data
```

Agent 不应把 save 当成 BlueprintHelper 写事务。

---

## 7. modified 规则

save 默认：

```text
modified=false
```

原因：

```text
save 是落盘动作，不代表 Agent 修改资产内容。
```

---

## 8. Agent 禁止行为

Agent 不得：

```text
1. 期待 save 返回 validation。
2. 期待 save 返回 transaction_id。
3. 把 status=no_op 当成失败。
4. 把 saved=false / was_dirty=false 当成异常。
5. 在 save_failed 时忽略 error.conflicts。
```

---

## 9. 最终报告规则

正常完成时，Agent 可报告：

```text
1. 是否保存成功。
2. 是否 no_op。
3. 保存失败原因。
```

不默认报告：

```text
transaction_id
review_status
journal_path
```

---

## 10. 验收标准

```text
1. Agent 能解析 save_result.saved。
2. Agent 能解析 was_dirty。
3. Agent 能处理 status=no_op。
4. Agent 不期待 validation。
5. Agent 不期待 write_ref / transaction_id。
