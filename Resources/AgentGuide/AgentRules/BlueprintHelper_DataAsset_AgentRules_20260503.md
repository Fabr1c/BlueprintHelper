# BlueprintHelper Agent 侧规则：DataAsset 使用规范

日期：2026-05-03  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：DataAsset Agent 侧规则确认稿  
本文边界：规定 Agent 如何调用和解释 DataAsset 读取与属性写入工具，包括成功极简返回、批量事务化、失败定位、validation、无 transaction 返回规则。UE 字段映射见独立文档。

---

## 1. 工具边界

DataAsset 第一版覆盖：

```text
read_data_asset_properties
set_data_asset_property
set_data_asset_properties
```

第一版不覆盖：

```text
创建 DataAsset
修改 DataAsset class
修改 C++ class / struct
CSV / JSON 全量导入导出
复杂嵌套 UObject 引用批量解析
资产迁移 / Schema migration
```

---

## 2. 工具职责

`read_data_asset_properties`：

```text
读取 DataAsset 的指定属性或属性摘要。
```

`set_data_asset_property` / `set_data_asset_properties`：

```text
设置 DataAsset 的属性值。
```

写工具不负责创建资产或修改 class/schema。

---

## 3. data.schema 短命名

Agent 应期待：

```text
ReadDataAssetProperties.v1
SetDataAssetProperty.v1
```

不应期待 BlueprintHelper / MCP / Tools 前缀。

---

# 4. read_data_asset_properties

读取工具允许返回 values。

示例：

```json
{
  "data": {
    "schema": "ReadDataAssetProperties.v1",
    "properties": {
      "asset_class": "/Script/Game.WeaponConfigDataAsset",
      "property_count": 3,
      "values": {
        "Damage": 25.0,
        "Cooldown": 0.35,
        "DisplayName": "Blaster"
      }
    }
  }
}
```

Agent 规则：

```text
1. 读工具可返回属性值。
2. 默认不深展开复杂对象引用。
3. 如果需要修改前精确确认，应先读后写。
```

---

# 5. set_data_asset_property / set_data_asset_properties

写成功只返回 property_result 计数。

单属性成功：

```json
{
  "property_result": {
    "mode": "single",
    "requested_count": 1,
    "applied_count": 1,
    "changed_count": 1,
    "no_op_count": 0
  }
}
```

批量成功：

```json
{
  "property_result": {
    "mode": "batch",
    "requested_count": 3,
    "applied_count": 3,
    "changed_count": 2,
    "no_op_count": 1
  }
}
```

Agent 不应期待：

```text
before
after
all_properties
invalid_settings
property_paths
write_ref
transaction_id
```

---

## 6. 批量事务化

批量属性写默认事务化：

```text
只要存在 invalid 项，整批失败，不做部分应用。
```

失败时：

```text
ok=false
status=failed
modified=false
```

并通过 `error.conflicts` 返回问题项。

Agent 不得在批量失败后假设部分字段已写入。

---

## 7. 失败定位

失败示例：

```json
{
  "error": {
    "code": "invalid_data_asset_property_settings",
    "stage": "validate_properties",
    "message": "One or more DataAsset property settings are invalid.",
    "retryable": false,
    "conflicts": [
      {
        "code": "property_not_found",
        "property": "DamageWrong"
      }
    ]
  }
}
```

Agent 应只在错误场景读取 property 定位信息。

---

## 8. validation 规则

DataAsset 写工具成功通常返回：

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

## 9. 不返回事务信息

DataAsset 写工具成功不返回：

```text
write_ref
transaction_id
journal_recorded
review
safety
```

事务、Journal、Review 是 UE 侧内部审计系统。

---

## 10. Agent 禁止行为

Agent 不得：

```text
1. 用 DataAsset 写工具修改 C++ class / struct。
2. 期待写工具返回 before / after。
3. 在批量失败后假设部分应用。
4. 期待写工具返回 transaction_id。
5. 期待 validation.compiled / validation.saved。
6. 让 DataAsset 工具承担 CSV / JSON 全量导入导出。
```

---

## 11. 最终报告规则

正常完成时，Agent 可报告：

```text
1. 修改了几个属性。
2. 是否 no_op。
3. 是否需要保存。
4. 失败时报告 property / expected_type 等定位信息。
```

不默认报告：

```text
transaction_id
review_status
before / after
all properties
```

---

## 12. 验收标准

```text
1. Agent 能用 read_data_asset_properties 读取 values。
2. Agent 能解析 property_result。
3. Agent 知道批量写是事务化的。
4. Agent 不期待 before / after。
5. Agent 不期待 transaction_id。
6. Agent 能处理 validation 只含 should_compile / should_save。
