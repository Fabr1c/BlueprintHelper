# BlueprintHelper Agent 侧规则：DataTable 使用规范

日期：2026-05-03  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：DataTable Agent 侧规则确认稿  
本文边界：规定 Agent 如何调用和解释 DataTable 读取、行新增、行更新、行删除工具，包括成功极简返回、批量事务化、dry_run、validation 和无 transaction 返回规则。UE 字段映射见独立文档。

---

## 1. 工具边界

DataTable 第一版覆盖：

```text
read_data_table
read_data_table_rows
add_data_table_row
update_data_table_row
update_data_table_rows
remove_data_table_row
remove_data_table_rows
```

第一版不覆盖：

```text
CSV / JSON 全量导入导出
批量结构迁移
RowStruct 自动修改
DataTable RowStruct 替换
C++ struct/class 修改
复杂嵌套 UObject 引用批量解析
```

---

## 2. data.schema 短命名

Agent 应期待：

```text
ReadDataTable.v1
ReadDataTableRows.v1
AddDataTableRow.v1
UpdateDataTableRow.v1
RemoveDataTableRow.v1
RemoveDataTableRowDryRun.v1
```

不应期待 BlueprintHelper / MCP / Tools 前缀。

---

# 3. read_data_table

`read_data_table` 只返回结构摘要：

```json
{
  "data_table": {
    "row_struct": "/Script/Game.WeaponRow",
    "row_count": 3,
    "row_names": [
      "Pistol",
      "Rifle",
      "Shotgun"
    ]
  }
}
```

Agent 不应期待：

```text
all rows
row values
full struct schema
serialized binary data
```

如需读取行值，应调用 `read_data_table_rows`。

---

# 4. read_data_table_rows

`read_data_table_rows` 读取指定行内容。

示例：

```json
{
  "rows": [
    {
      "row_name": "Pistol",
      "values": {
        "Damage": 12,
        "Cooldown": 0.25,
        "Ammo": 12
      }
    }
  ]
}
```

Agent 规则：

```text
1. 只读取请求行。
2. 请求行不存在时，通过 error.conflicts 返回 row_name。
3. 不用 read_data_table 默认拉全表。
```

---

# 5. add_data_table_row

成功只返回：

```json
{
  "row_result": {
    "added_count": 1
  }
}
```

Agent 不应期待：

```text
row_name
row_values
before
after
transaction_id
```

---

## 5.1 row collision

第一版只支持：

```text
fail_if_exists
reuse_if_exists
```

不支持：

```text
auto_rename
replace_existing
```

`reuse_if_exists` 命中已有行时：

```text
status=no_op
added_count=0
reused_existing=true
```

---

# 6. update_data_table_row / update_data_table_rows

成功只返回 row_result 计数：

```json
{
  "row_result": {
    "mode": "batch",
    "requested_count": 3,
    "updated_count": 3,
    "changed_count": 2,
    "no_op_count": 1
  }
}
```

Agent 不应期待 row values。

如果需要确认最终值，应调用：

```text
read_data_table_rows
```

---

## 6.1 批量事务化

批量更新默认事务化：

```text
任何 row / field invalid，整批失败。
```

失败时：

```text
ok=false
status=failed
modified=false
```

并通过 `error.conflicts` 返回 row_name / field。

Agent 不得在批量失败后假设部分行已更新。

---

# 7. remove_data_table_row / remove_data_table_rows

删除 row 是破坏性操作，必须 dry_run。

dry_run passed：

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
      "row_external_dependents_exist"
    ],
    "conflicts": [
      {
        "code": "row_external_dependents_exist",
        "row_name": "Pistol",
        "message": "The row appears to be referenced by external logic or assets."
      }
    ],
    "errors": []
  }
}
```

如果第一版无法可靠检测外部引用，不应返回假精确依赖。

---

## 7.1 remove 成功

成功只返回：

```json
{
  "row_result": {
    "removed_count": 1
  }
}
```

Agent 不应期待 removed row values。

---

# 8. validation 规则

DataTable 写工具成功通常返回：

```json
"validation": {
  "should_compile": false,
  "should_save": true
}
```

no_op 返回：

```json
"validation": {
  "should_compile": false,
  "should_save": false
}
```

Agent 不应期待：

```text
compiled
saved
```

compile/save 闭环由独立工具完成。

---

# 9. 不返回事务信息

DataTable 写工具成功不返回：

```text
write_ref
transaction_id
journal_recorded
review
safety
```

事务、Journal、Review 是 UE 侧内部审计系统。

---

# 10. Agent 禁止行为

Agent 不得：

```text
1. 用 DataTable 工具修改 RowStruct。
2. 用 DataTable 工具修改 C++ struct/class。
3. 默认读取全表所有行值。
4. 期待写工具返回 before / after。
5. 期待 add/update/remove 返回 row values。
6. 在批量失败后假设部分应用。
7. 在 remove 未 dry_run passed 前正式删除。
8. 期待写工具返回 transaction_id。
9. 期待 validation.compiled / validation.saved。
```

---

# 11. 最终报告规则

正常完成时，Agent 可报告：

```text
1. 新增 / 更新 / 删除了几行。
2. 是否 no_op。
3. 是否需要保存。
4. 失败时报告 row_name / field 定位信息。
```

不默认报告：

```text
transaction_id
review_status
row values
before / after
```

---

# 12. 验收标准

```text
1. Agent 能用 read_data_table 获取 row list。
2. Agent 能用 read_data_table_rows 读取指定行 values。
3. Agent 能解析 add_data_table_row.added_count。
4. Agent 能解析 update_data_table_row / rows 的 row_result。
5. Agent 能解析 remove_data_table_row.removed_count。
6. Agent 知道批量更新是事务化的。
7. Agent 知道 remove 必须 dry_run。
8. Agent 不期待 transaction_id。
9. Agent 能处理 validation 只含 should_compile / should_save。
