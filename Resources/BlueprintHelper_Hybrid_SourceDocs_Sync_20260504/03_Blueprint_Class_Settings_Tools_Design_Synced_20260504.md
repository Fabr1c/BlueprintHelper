# 03 Blueprint Class Settings Tools 设计文档（已同步确认 Diff）

日期：2026-05-03  
工具簇：Blueprint Class Settings Tools / 蓝图类配置工具簇  
状态：同步确认 Diff 后的修正版  
同步范围：移除 `set_parent_class` / Reparent 实现口径、接口操作边界、批量事务规则、Class Settings 字段收敛、普通工具不默认返回 transaction/review/safety。

---

## 0. 本次同步结论

本文件替换旧版中以下过期口径：

```text
1. 第一版 Blueprint Class Settings 不包含 set_parent_class。
2. parent_class 只作为 read_class_settings 返回的只读字段。
3. 不暴露 parent_class_result / requested_parent_class / confirmed_after_dry_run / parent-class dry_run/apply 语义。
4. add_implemented_interface 只修改 Implemented Interfaces，不创建 BPI，不创建接口函数实现图，不写接口函数 body。
5. 批量 Interface / Class Default 写入默认事务式，不支持 partial apply。
6. 普通 Class Settings 成功结果不默认返回 transaction / review / safety。
```

---

## 1. 定位

Blueprint Class Settings Tools 独立成簇，负责蓝图类级设置读取和部分声明层修改。

它不并入 Graph Write，也不并入普通 Blueprint Structure Tools。

第一版范围应保持收敛，避免把“类设置”“接口实现”“函数 Override”“父类迁移”混成一个过宽工具簇。

---

## 2. 第一版覆盖范围

第一版覆盖：

```text
read_class_settings
add_implemented_interface
add_implemented_interfaces
remove_implemented_interface
remove_implemented_interfaces
set_class_default_property
set_class_default_properties
```

第一版不包含：

```text
set_parent_class
blueprint_reparent
parent_class_result
create interface function implementation body
connect interface function to EventGraph
create Blueprint Interface asset
create function override
create engine event entry
```

`parent_class` 仍保留在读取结果中，仅作为只读信息。

---

## 3. Reparent / Parent Class 边界

默认不提供：

```text
set_parent_class
blueprint_reparent
```

规则：

```text
BlueprintHelper 第一版不鼓励 Agent 修改已创建蓝图的父类。
已创建蓝图如果父类错误，推荐重新创建正确父类的新蓝图。
Reparent 不作为 Agent 默认可用能力。
```

如果未来确实需要父类迁移，应作为单独高风险迁移工具重新设计，不能塞回第一版 Class Settings。

Agent 规则：

```text
如果用户任务需要修改 Parent Class，Agent 应 stop_and_report，说明当前 Blueprint Class Settings 第一版不支持该能力。
```

---

## 4. read_class_settings

`read_class_settings` 返回：

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

字段规则：

| 字段 | 规则 |
|---|---|
| `parent_class` | 完整类路径，例如 `/Script/Engine.Actor`，只读。 |
| `generated_class` | 生成类短名，例如 `BP_BH_PhysicsDoor_C`，不是完整对象路径。 |
| `implemented_interfaces` | 接口资产路径列表。 |
| `class_default_count` | 默认属性摘要数量，不是 Class Defaults 快照。 |

`read_class_settings` 不返回完整 Class Defaults 快照。需要读取具体默认属性时，应使用后续专用读取能力或带 filter 的 Class Defaults 读取能力。

Agent 不得把 `generated_class` 当作资产路径或 object path 使用。

---

## 5. Interface 添加 / 移除

接口工具调用层区分：

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

示例：

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

---

## 6. Interface 工具职责边界

`add_implemented_interface` / `add_implemented_interfaces` 只修改目标 Blueprint 的 Class Settings：

```text
Implemented Interfaces
```

它们不会自动：

```text
1. 创建 Blueprint Interface 资产。
2. 创建接口函数实现图。
3. 写接口函数 body。
4. 将接口函数接入 EventGraph。
```

如果任务要求“完整实现接口交互”，Agent 应拆为：

```text
1. Asset Factory 创建 BPI。
2. Blueprint Class Settings 添加 Implemented Interface。
3. Graph Write 创建或实现接口函数逻辑。
4. Compile / Save。
```

Agent 不得把“接口已添加到 Blueprint”误判为“接口功能已实现”。

---

## 7. Interface 事务规则

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

第一版不支持：

```text
partial apply
allow_partial=true
```

Agent 不得假设部分接口已经成功应用。

---

## 8. remove_implemented_interface 边界

移除接口可能影响：

```text
1. 接口函数实现图。
2. 调用方引用。
3. 蓝图编译结果。
```

Agent 应在移除前确认目标明确。必要时先 `read_class_settings`，确认接口确实存在。

删除 Interface 属于高风险类级修改，Conservative 下应 dry_run。

---

## 9. Class Default 属性设置

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

示例：

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

---

## 10. Class Default 不回显快照

成功时不返回：

```text
before
after
all_defaults
```

原因：

```text
1. before / after 属于 UE 内部 diff / Review / debug。
2. 大对象属性回显浪费 Token。
3. 成功结果只需要执行摘要。
```

如果 Agent 需要确认最终默认值，应使用后续专用读取工具或带 filter 的读取能力。

---

## 11. Class Default 事务规则

批量 Class Default 修改默认事务式：

```text
只要存在 invalid_settings，默认不应用任何默认属性修改。
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

第一版不支持：

```text
partial apply
allow_partial=true
```

Agent 不得假设部分属性已经成功写入。

---

## 12. Override / Interface Implementation 远期边界

以下能力不属于第一版 Class Settings 的字段协议：

```text
blueprint_list_overridable_functions
blueprint_create_function_override
create engine event entry
create interface function implementation entry
migrate existing function to interface function
```

如果未来需要，应作为独立工具簇或 Class Settings 后续阶段重新设计，并明确与 Graph Write 的边界：

```text
创建入口不等于写函数体逻辑。
函数体内部逻辑仍交给 Graph Write。
```

第一版文档不再把这些能力列为推荐工具，避免 Agent 误判当前可用能力。

---

## 13. dry_run

所有 Class Settings 写操作必须支持 dry_run。

Conservative 下高风险类级修改必须 dry_run，例如：

```text
删除 Interface
修改用户已有类配置
修改 Tick / Replication / Spawn / Input 类设置
Class Default 修改影响运行时实例行为
```

不再存在：

```text
set_parent_class 必须 dry_run
parent_class dry_run
confirmed_after_dry_run
```

因为第一版不提供修改 Parent Class 的能力。

---

## 14. Agent-facing 成功返回字段

普通 Class Settings 成功结果默认不包含：

```text
transaction
review
safety
```

Agent 不应期待这些字段。

规则：

```text
1. safety_profile 只从 runtime_profile.active_profile 读取。
2. dry_run 信息只在 status=dry_run 时从 data.dry_run 读取。
3. transaction_id 可以由 UE 插件内部生成并写入 Journal / Review，但普通工具不默认暴露给 Agent。
4. Agent 最终报告默认不输出 transaction_id 或 review_status。
```

---

## 15. 最终报告规则

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
父类是否修改
```

父类修改不在第一版能力范围内。

---

## 16. Agent 禁止行为

Agent 不得：

```text
1. 用 add_implemented_interface 创建 BPI 资产。
2. 用 add_implemented_interface 自动生成接口函数 body。
3. 用 Class Settings 工具写图表逻辑。
4. 计划通过 Class Settings 修改 Parent Class。
5. 在批量 Interface 或 Class Default 失败时假设部分修改已应用。
6. 期待 before / after。
7. 期待完整 Class Default 快照。
8. 在最终报告中默认输出 transaction_id 或 review_status。
```

---

## 17. 验收标准

```text
1. read_class_settings 返回 parent_class / generated_class / implemented_interfaces / class_default_count。
2. parent_class 是只读字段，不提供 set_parent_class。
3. generated_class 只返回短名，不返回完整路径。
4. Interface 工具调用层区分单个 / 多个。
5. Interface 返回统一 interface_result。
6. Interface 无效时默认事务式，不应用任何接口。
7. add_implemented_interface 不自动创建 BPI。
8. add_implemented_interface 不自动创建接口函数实现图。
9. Class Defaults 使用 default_property_result。
10. default_property_result 与 Component property_result 同构。
11. 成功时不回显所有默认属性。
12. 成功时不返回 before / after。
13. 无效项只返回 invalid_interfaces 或 invalid_settings。
14. 批量 Class Default 设置默认事务式。
15. 第一版不支持 partial apply。
16. 默认不返回 transaction / review / safety。
17. Agent 遇到修改 Parent Class 需求时 stop_and_report。
```
---

# 2026-05-04 混合架构同步：工具簇暴露层级

## 同步结论

本文档中的工具簇边界不推翻，但 Agent-facing 暴露方式调整。

底层能力簇继续作为：

```text
1. UE Task Runtime step operation。
2. Python / MCP Task Compiler 的 capability 模型。
3. Debug / Expert / 测试入口。
```

普通 Agent 不应默认直接手动拼装本工具簇调用链。普通流程改为：

```text
read_task_context → preview_task → execute_task
```

## 边界仍然有效

本工具簇原有职责边界仍必须被 Task Compiler / Task Runtime 遵守。

例如：

```text
Asset Factory 只创建资产，不添加接口、不写接口函数 body。
Component add_component 只创建组件和 attachment，不设置属性。
Class Settings add_implemented_interface 只修改 Implemented Interfaces。
Enhanced Input 当前不默认自动编辑 IA / IMC。
```

也就是说，混合架构只改变“谁来调用工具”，不改变“工具能做什么”。

## Agent-facing 返回调整

普通 execute_task 成功结果默认不展开本工具簇的底层返回。

底层 transaction / review / safety 仍进入 UE Journal / Review，但普通任务成功摘要只报告：

```text
任务是否完成
修改了哪些资产
执行了多少步骤
是否 compile/save
异常或未完成项
```
