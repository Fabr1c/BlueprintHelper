# BlueprintHelper Agent 侧规则：Debug / Export Bundle / Large Payload 使用规范

日期：2026-05-03  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：Debug / Export Bundle / Large Payload Agent 侧规则确认稿  
本文边界：规定 Agent 如何调用和解释 debug bundle、transaction debug bundle、asset logic snapshot、large payload ref 工具，包括大 payload 引用化、chunk 读取、隐私边界、无 validation / transaction 返回规则。UE 字段映射见独立文档。

---

## 1. 工具簇职责

Debug / Export Bundle / Large Payload 工具簇用于：

```text
排错
审计导出
失败分析
离线比较
读取大 payload 引用摘要或分片
```

它不用于：

```text
普通执行闭环
普通写工具成功返回
普通 rollback 流程
自动上传日志
自动提交 GitHub Issue
打包整个项目
导出私有源码
```

---

## 2. 工具列表

第一版包含：

```text
export_debug_bundle
export_transaction_debug_bundle
export_asset_logic_snapshot
read_large_payload_ref
```

---

## 3. 通用规则

本簇工具默认：

```text
modified=false
```

Agent 不应期待：

```text
validation
write_ref
transaction_id
journal_recorded
review
safety
```

例外：

```text
export_transaction_debug_bundle.target.transaction_id
```

因为该工具的目标就是明确 transaction debug 导出。

所有 `data.schema` 使用短命名。

---

## 4. 普通执行流程不依赖 Debug Bundle

Agent 不得把 Debug Bundle 作为普通任务闭环的一部分。

普通任务应依赖：

```text
read tools
write tools
compile_blueprint_asset
save_asset
runtime_profile
diagnostics
```

Debug / Export 工具只在以下场景使用：

```text
用户明确要求导出
失败排查
审计导出
人工分析
调试大 payload
```

---

# 5. export_debug_bundle

## 5.1 职责

`export_debug_bundle` 导出当前 BlueprintHelper 运行环境的调试包。

可能包含：

```text
runtime 摘要
diagnostics Markdown
MCP tool registry 摘要
UE Bridge 状态摘要
最近错误摘要
版本状态摘要
```

不应包含：

```text
Token
secret
完整 settings.json
完整 CLAUDE.md
完整 Skill / AgentGuide
完整 Transaction Journal
完整资产快照
私有源码
整个项目目录
本地绝对路径
```

---

## 5.2 成功返回

```json
{
  "data": {
    "schema": "ExportDebugBundle.v1",
    "export_result": {
      "exported": true,
      "bundle_ref": "resource://blueprinthelper/debug/debug_bundle_20260503_4601.zip",
      "format": "zip"
    }
  }
}
```

Agent 不应期待 bundle bytes 或完整内容 inline。

---

# 6. export_transaction_debug_bundle

## 6.1 职责

`export_transaction_debug_bundle` 导出某个明确 transaction 的调试包。

用途：

```text
用户明确要求审计导出
失败排查
回滚冲突排查
Review UI 外部分析
```

不用于普通 rollback。

Rollback 工具内部读取 rollback_data，不需要 Agent 先导出 transaction debug bundle。

---

## 6.2 成功返回

```json
{
  "target": {
    "transaction_id": "tx_20260503_1704",
    "export_scope": "transaction_debug"
  },
  "data": {
    "schema": "ExportTransactionDebugBundle.v1",
    "export_result": {
      "exported": true,
      "bundle_ref": "resource://blueprinthelper/transactions/tx_20260503_1704_debug.zip",
      "format": "zip"
    }
  }
}
```

Agent 可使用 `bundle_ref` 供用户下载、审查或后续 debug 读取。

---

# 7. export_asset_logic_snapshot

## 7.1 职责

`export_asset_logic_snapshot` 导出某个资产的逻辑快照。

必须指定：

```text
asset_path
snapshot_type
```

可选 snapshot_type：

```text
logic_md
logic_json
raw_json
widget_tree
class_settings
data_table_rows
```

第一版不应一次默认导出全部。

---

## 7.2 成功返回

```json
{
  "data": {
    "schema": "ExportAssetLogicSnapshot.v1",
    "export_result": {
      "exported": true,
      "snapshot_ref": "resource://blueprinthelper/snapshots/BP_BH_PhysicsDoor_logic_json_20260503.json",
      "format": "json"
    }
  }
}
```

Agent 不应期待完整 LogicJson / RawJson / WidgetTree inline。

---

# 8. read_large_payload_ref

## 8.1 职责

`read_large_payload_ref` 读取 resource_ref / bundle_ref / snapshot_ref 的摘要或分片。

支持：

```text
mode=summary
mode=chunk
```

不默认支持：

```text
mode=full
```

---

## 8.2 summary

```json
{
  "data": {
    "schema": "ReadLargePayloadRef.v1",
    "payload": {
      "available": true,
      "format": "json",
      "size_bytes": 48231,
      "chunk_count": 5
    }
  }
}
```

---

## 8.3 chunk

```json
{
  "data": {
    "schema": "ReadLargePayloadRef.v1",
    "payload": {
      "format": "json",
      "chunk_index": 0,
      "chunk_count": 5,
      "content": "{ "logic": { ... } }",
      "truncated": false
    }
  }
}
```

Agent 规则：

```text
1. chunk 模式可以返回内容。
2. 内容必须是受控分片。
3. 不得要求普通工具直接返回完整大对象。
4. 需要完整内容时，Agent 应逐块读取，并注意 token 成本。
```

---

# 9. ref 字段语义

字段含义：

```text
resource_ref：通用大 payload 引用。
bundle_ref：zip / bundle 类导出包引用。
snapshot_ref：资产快照引用。
```

底层可以是同一种 URI / handle。

Agent-facing 返回不使用本地绝对路径。

统一建议：

```text
resource://blueprinthelper/...
```

---

# 10. 隐私边界

Debug bundle 默认不包含：

```text
Token
secret
完整 settings.json
用户完整 CLAUDE.md
私有源码
整个项目目录
绝对路径
```

如果用户要求包含敏感信息，应使用后续独立工具设计，例如：

```text
export_sensitive_debug_bundle
```

并要求明确确认。第一版不做。

---

# 11. 与 Transaction / Rollback 的关系

普通写工具成功不返回 transaction_id。

transaction_id 可通过：

```text
Transaction Journal Query
Review Query
用户明确查询
Debug Export
```

暴露。

Rollback 工具不依赖：

```text
export_transaction_debug_bundle
```

Rollback 工具应内部读取 Journal / rollback_data。

---

# 12. Agent 禁止行为

Agent 不得：

```text
1. 在普通任务中默认导出 debug bundle。
2. 用 export_transaction_debug_bundle 作为普通 rollback 前置步骤。
3. 期待 debug bundle inline 返回完整内容。
4. 期待 snapshot 导出 inline 返回完整 LogicJson。
5. 要求 read_large_payload_ref 默认 full 读取。
6. 把本地绝对路径暴露给用户。
7. 将 Token / secret / 完整 settings.json 打包导出。
8. 期待本簇工具返回 validation / transaction_id。
```

---

# 13. 最终报告规则

正常完成时，Agent 可报告：

```text
1. 已导出的 bundle_ref / snapshot_ref。
2. 导出格式。
3. large payload 的 size / chunk_count。
4. 失败时报告 error.code / stage / message。
```

不默认报告：

```text
本地绝对路径
Token / secret
settings.json 内容
完整 CLAUDE.md
完整大 payload
```

---

# 14. 验收标准

```text
1. Agent 能使用 export_debug_bundle 获取 bundle_ref。
2. Agent 知道 debug bundle 不内联内容。
3. Agent 能使用 export_transaction_debug_bundle 进行事务调试导出。
4. Agent 知道 transaction debug bundle 不参与普通 rollback。
5. Agent 能使用 export_asset_logic_snapshot 获取 snapshot_ref。
6. Agent 不期待 snapshot inline。
7. Agent 能使用 read_large_payload_ref summary / chunk。
8. Agent 不要求默认 full 读取。
9. Agent 不暴露本地绝对路径。
10. Agent 不期待本簇工具返回 validation / transaction_id。
