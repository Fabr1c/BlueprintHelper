# BlueprintHelper Agent 侧规则：Blueprint Class Settings 使用规范（移除 set_parent_class 修正版）

日期：2026-05-03  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：Blueprint Class Settings Agent 侧规则修正版  
本文边界：规定 Agent 如何调用和解释 Blueprint Class Settings 工具簇。UE 字段映射见独立 UE 侧文档。  
修订说明：本版移除 `set_parent_class` 及其相关返回字段、dry_run 特例、最终报告与验收条目。`parent_class` 仅作为 `read_class_settings` 的只读信息保留。

---

## 1. 工具簇职责

Blueprint Class Settings 工具簇负责蓝图类级设置中的只读查询、Implemented Interfaces 管理，以及 Class Defaults 设置。

第一版工具：

```text
read_class_settings
add_implemented_interface
add_implemented_interfaces
remove_implemented_interface
remove_implemented_interfaces
set_class_default_property
set_class_default_properties
```

第一版不包含父类修改工具。

---

## 2. 工具边界

Blueprint Class Settings 可以：

```text
1. 读取蓝图 Class Settings。
2. 添加 / 移除 Implemented Interface。
3. 设置 Class Defaults。
```

Blueprint Class Settings 不负责：

```text
1. 创建 Blueprint Interface 资产。使用 Asset Factory。
2. 创建接口函数实现图。
3. 写接口函数 body。
4. 创建组件。
5. 写 EventGraph 执行流。
6. 修改 C++ 源码。
7. 修改蓝图 Parent Class。
```

说明：

```text
read_class_settings 可以读取 parent_class。
第一版不提供修改 parent_class 的写能力。
```

---

## 3. read_class_settings 使用规则

Agent 可使用 `read_class_settings` 检查：

```text
1. 蓝图父类。
2. 生成类短名。
3. 已实现接口列表。
4. Class Default 摘要数量。
```

返回示例：

```json
{
  "class_settings": {
    "parent_class": "/Script/Engine.Actor",
    "generated_class": "BP_BH_PhysicsDoor_C",
    "implemented_interfaces": [
      "/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable"
    ],
    "class_default_count": 12
  }
}
```

Agent 应理解：

```text
1. generated_class 是短名，不是完整路径。
2. parent_class 是完整类路径，但只读。
3. implemented_interfaces 返回接口资产路径。
4. class_default_count 只是数量 / 摘要，不是 Class Defaults 快照。
5. read_class_settings 不返回完整 Class Defaults 快照。
6. Agent 不得把 generated_class 当作 asset path 或 object path。
```

---

## 4. Interface 操作规则

调用层区分：

```text
add_implemented_interface       单个接口
add_implemented_interfaces      多个接口
remove_implemented_interface    单个接口
remove_implemented_interfaces   多个接口
```

返回层统一使用：

```text
data.interface_result
```

单个接口只是：

```text
mode=single
requested_count=1
```

多个接口是：

```text
mode=batch
requested_count>1
```

---

## 5. interface_result 解释规则

```json
{
  "interface_result": {
    "mode": "single",
    "requested_count": 1,
    "applied_count": 1,
    "already_implemented_count": 0,
    "removed_count": 0,
    "invalid_interfaces": []
  }
}
```

字段解释：

| 字段 | 含义 |
|---|---|
| `mode` | `single` 或 `batch`。 |
| `requested_count` | 请求处理接口数量。 |
| `applied_count` | 成功添加接口数量。 |
| `already_implemented_count` | 已经实现的接口数量。 |
| `removed_count` | 成功移除接口数量。 |
| `invalid_interfaces` | 无效接口列表。 |

---

## 6. Interface 事务规则

批量 Interface 操作默认事务式：

```text
只要存在 invalid_interfaces，默认不应用任何接口修改。
```

出现无效接口时：

```text
ok=false
status=failed
modified=false
applied_count=0
removed_count=0
```

Agent 不得假设部分接口已经成功应用。

第一版不支持：

```text
partial apply
allow_partial=true
```

---

## 7. add_implemented_interface 边界

`add_implemented_interface` 只修改 Class Settings 的 Implemented Interfaces。

它不会自动：

```text
1. 创建 BPI 资产。
2. 创建接口函数实现图。
3. 写接口函数逻辑。
4. 将接口函数接入 EventGraph。
```

如果任务要求“完整实现接口交互”，Agent 应拆为：

```text
1. Asset Factory 创建 BPI。
2. Blueprint Class Settings 添加 Implemented Interface。
3. Graph Write 创建 / 实现接口函数逻辑。
4. Compile / Save。
```

Agent 不得把“接口已添加到蓝图”理解为“接口功能已实现”。

---

## 8. remove_implemented_interface 边界

移除接口可能影响：

```text
1. 接口函数实现图。
2. 调用方引用。
3. 蓝图编译结果。
```

Agent 应在移除前确认目标明确。  
必要时先 `read_class_settings`，确认接口确实存在。

---

## 9. Parent Class 只读规则

`parent_class` 在第一版中只作为 `read_class_settings` 的只读字段存在。

规则：

```text
1. Blueprint Class Settings 第一版不提供修改 Parent Class 的工具。
2. 不存在 parent_class_result 返回结构。
3. 不存在 requested_parent_class / before_parent_class / after_parent_class 等 Agent-facing 字段。
4. Agent 不应计划通过 Blueprint Class Settings 修改蓝图父类。
5. 如果用户任务要求改变父类，Agent 应 stop_and_report，说明当前 BlueprintHelper Class Settings 第一版不支持该写能力。
```

---

## 10. Class Default 属性设置规则

调用层区分：

```text
set_class_default_property      单个默认属性
set_class_default_properties    多个默认属性
```

返回层统一使用：

```text
data.default_property_result
```

单属性只是：

```text
mode=single
requested_count=1
```

批量属性是：

```text
mode=batch
requested_count>1
```

---

## 11. default_property_result 解释规则

```json
{
  "default_property_result": {
    "mode": "batch",
    "requested_count": 4,
    "applied_count": 4,
    "changed_count": 3,
    "no_op_count": 1,
    "invalid_settings": []
  }
}
```

字段解释：

| 字段 | 含义 |
|---|---|
| `mode` | `single` 或 `batch`。 |
| `requested_count` | 请求设置数量。 |
| `applied_count` | 实际应用数量。 |
| `changed_count` | 实际产生变化数量。 |
| `no_op_count` | 值相同或无需修改数量。 |
| `invalid_settings` | 无效设置列表。 |

---

## 12. 不回显 Class Default 值

成功时不返回：

```text
before
after
all_defaults
```

Agent 不应期待返回完整默认属性快照。

原因：

```text
1. before / after 属于 UE 内部 diff / review / debug。
2. 大对象属性回显浪费 Token。
3. 成功结果只需要执行摘要。
```

如果 Agent 需要确认最终默认值，应使用后续专用读取工具或带 filter 的读取能力。

---

## 13. Class Default 事务规则

批量 Class Default 修改默认事务式：

```text
只要存在 invalid_settings，默认不应用任何属性修改。
```

出现无效设置时：

```text
ok=false
status=failed
modified=false
applied_count=0
changed_count=0
no_op_count=0
```

Agent 不得假设部分属性已经成功写入。

第一版不支持：

```text
partial apply
allow_partial=true
```

---

## 14. dry_run 使用规则

高风险操作使用：

```text
data.dry_run
```

不使用顶层 `safety`。

Agent 看到：

```text
status=dry_run
data.dry_run.can_execute=false
```

时不得正式执行写入。

说明：

```text
本工具簇第一版不包含 Parent Class 修改，因此不存在 set_parent_class 必须 dry_run 的特例。
```

---

## 15. validation 使用规则

写工具通常返回：

```json
{
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

Agent 应根据 validation 继续 compile/save 闭环。

no_op 且 modified=false 时，通常：

```text
should_compile=false
should_save=false
```

---

## 16. Agent 不消费 transaction / review / safety

Blueprint Class Settings 默认返回不包含：

```text
transaction
review
safety
```

Agent 不应期待这些字段。

规则：

```text
1. Agent 不关心 transaction_id。
2. Agent 不关心 review_status。
3. Agent 不从工具结果读取 safety_profile。
4. safety_profile 来自 runtime_profile。
5. dry_run 信息只在 status=dry_run 时从 data.dry_run 读取。
6. 安全阻断从 error 读取。
```

---

## 17. 物理门任务示例拆分

如果物理门任务使用接口交互，Agent 应拆为：

```text
1. Asset Factory 创建 BPI_BH_Interactable。
2. Blueprint Class Settings 将 BPI 添加到 BP_BH_PhysicsDoor 的 Implemented Interfaces。
3. Graph Write 创建或实现 Interact 接口函数逻辑。
4. Compile / Save。
```

不要把第 2 步误认为接口函数逻辑已经完成。

---

## 18. 禁止行为

Agent 不得：

```text
1. 用 add_implemented_interface 创建 BPI 资产。
2. 用 add_implemented_interface 自动生成接口函数 body。
3. 用 Class Settings 工具写图表逻辑。
4. 用 Class Settings 工具修改 Parent Class。
5. 在批量 Interface 或 Class Default 失败时假设部分修改已应用。
6. 期待 before / after。
7. 期待完整 Class Default 快照。
8. 把 generated_class 当作 asset path 或 object path。
9. 在最终报告中默认输出 transaction_id 或 review_status。
```

---

## 19. 最终报告规则

正常完成时，Agent 可报告：

```text
1. 添加或移除了哪些接口。
2. 修改了多少个默认属性。
3. 是否存在 no_op。
4. 是否需要 compile/save。
```

不报告：

```text
transaction_id
review_status
journal_path
rollback_data
before / after 属性值
完整默认属性列表
```

说明：

```text
第一版不支持修改 Parent Class，因此最终报告中不应出现“父类是否修改”。
```

---

## 20. Agent 侧验收标准

```text
1. Agent 能区分 Asset Factory 创建接口资产与 Class Settings 添加 Implemented Interface。
2. Agent 不把 add_implemented_interface 当作接口函数实现。
3. Agent 能解析 interface_result。
4. Agent 能解析 default_property_result。
5. Agent 不期待 before / after。
6. Agent 理解 Interface 和 Class Default 批量操作是事务式。
7. Agent 不通过 Class Settings 计划修改 Parent Class。
8. Agent 能根据 validation 继续 compile/save。
9. Agent 知道 parent_class 是 read_class_settings 的只读字段。
10. Agent 不把 generated_class 当作资产路径或对象路径。
```
