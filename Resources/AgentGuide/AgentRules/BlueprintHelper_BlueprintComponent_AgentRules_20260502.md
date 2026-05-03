# BlueprintHelper Agent 侧规则：Blueprint Component 使用规范

日期：2026-05-02  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：Blueprint Component Agent 侧规则确认稿  
本文边界：规定 Agent 如何调用和解释 Blueprint Component 工具簇。UE 字段映射见独立 UE 侧文档。

---

## 1. 工具簇职责

Blueprint Component 工具簇负责：

```text
1. 读取蓝图组件树。
2. 添加组件。
3. 设置组件挂接关系。
4. 设置组件属性。
5. 删除明确目标组件。
```

第一版工具：

```text
read_components
add_component
set_component_property
set_component_properties
remove_component
```

---

## 2. 工具边界

Blueprint Component 不负责：

```text
1. 创建蓝图资产。使用 Asset Factory。
2. 修改蓝图图表节点。使用 Graph Write。
3. 添加 Implemented Interface。使用 Blueprint Class Settings。
4. 编辑 Input Mapping Context 的按键映射。使用 Enhanced Input。
5. 创建 BlueprintHelper-owned block。使用 Graph Write。
```

---

## 3. add_component 使用规则

`add_component` 只做两件事：

```text
1. 创建组件。
2. 建立组件挂接关系。
```

Agent 不得用 `add_component` 同时设置：

```text
1. Transform。
2. Mobility。
3. Collision。
4. Physics。
5. Mesh。
6. Material。
7. Constraint 参数。
8. 任意组件属性。
```

这些必须使用：

```text
set_component_property
set_component_properties
```

---

## 4. add_component 返回解释

成功返回：

```json
{
  "operation": "add_component",
  "status": "applied",
  "modified": true,
  "data": {
    "component": {
      "component_name": "DoorMesh",
      "component_class": "StaticMeshComponent",
      "created": true,
      "already_existed": false
    },
    "attachment": {
      "parent_component": "DefaultSceneRoot",
      "socket_name": null,
      "attach_rule": "keep_relative"
    },
    "name_collision": {
      "policy": "fail_if_exists",
      "handled": false
    }
  }
}
```

Agent 应理解：

```text
组件已创建并挂接。
组件属性尚未配置。
```

---

## 5. 组件命名冲突规则

字段名：

```text
name_collision
```

不是：

```text
collision
```

原因：

```text
collision 容易和物理碰撞属性混淆。
```

第一版策略：

```text
fail_if_exists
reuse_if_exists
```

不支持：

```text
auto_rename
replace_existing
```

Agent 不得自动改名或替换已有组件。

---

## 6. set_component_property / set_component_properties 使用规则

调用层必须区分：

```text
set_component_property      单个属性修改
set_component_properties    多个属性修改
```

返回层统一使用：

```text
data.property_result
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

## 7. property_result 解释规则

成功返回：

```json
{
  "property_result": {
    "mode": "batch",
    "requested_count": 4,
    "applied_count": 4,
    "changed_count": 3,
    "no_op_count": 1,
    "invalid_settings": []
  }
}
```

Agent 应解释：

| 字段 | 含义 |
|---|---|
| `mode` | `single` 或 `batch`。 |
| `requested_count` | 请求设置数量。 |
| `applied_count` | 实际应用数量。 |
| `changed_count` | 实际产生变化的数量。 |
| `no_op_count` | 已应用但值未变化的数量。 |
| `invalid_settings` | 无效设置列表。 |

---

## 8. 不回显 property 值

工具成功时不返回：

```text
before
after
all_properties
```

Agent 不应期待返回完整属性快照。

原因：

```text
1. before / after 属于 UE 内部 diff / review / debug。
2. 大对象属性回显浪费 Token。
3. 成功结果只需要执行摘要。
```

如果 Agent 需要确认最终属性，应使用读取工具或后续专用组件属性读取能力，而不是依赖写工具回显。

---

## 9. invalid_settings 规则

无效设置只出现在：

```text
data.property_result.invalid_settings
```

示例：

```json
{
  "property_path": "BodyInstance.bSimulatePhysics",
  "code": "property_not_writable",
  "expected_type": "bool"
}
```

Agent 应根据 invalid_settings 修正计划或 stop_and_report。

常见 code：

```text
property_not_found
property_not_writable
type_mismatch
value_out_of_range
object_reference_not_found
enum_value_invalid
struct_field_invalid
component_not_found
component_type_mismatch
unsupported_property_type
```

---

## 10. 批量属性事务规则

第一版批量属性修改默认事务式：

```text
只要存在 invalid_settings，默认不应用任何属性。
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

## 11. target 规则

单属性修改可带：

```json
{
  "property_path": "BodyInstance.bSimulatePhysics"
}
```

批量属性修改一般不在 target 中带 property_path。

Agent 不应依赖 target.property_path 获取所有修改内容；写工具返回不会回显所有 property。

---

## 12. Transform / Collision / Physics / Mesh 规则

以下都属于属性修改：

```text
RelativeLocation
RelativeRotation
RelativeScale
Mobility
CollisionEnabled
CollisionProfileName
BodyInstance.bSimulatePhysics
StaticMesh
Material
PhysicsConstraint 参数
```

Agent 必须使用：

```text
set_component_property
set_component_properties
```

而不是：

```text
add_component
```

---

## 13. read_components 使用规则

Agent 在以下情况应读取组件树：

```text
1. 添加组件前需要确认父组件是否存在。
2. 设置属性前需要确认组件是否存在。
3. 删除组件前需要确认目标组件。
4. 需要理解组件层级。
```

`read_components` 是只读工具：

```text
modified=false
status=completed
```

---

## 14. remove_component 使用规则

第一版只允许删除明确目标组件：

```text
component_name 必须明确。
不允许模糊删除。
不允许按 class 批量删除。
```

删除组件属于写操作。Agent 应在删除前确认目标明确，必要时先 `read_components`。

---

## 15. validation 使用规则

Blueprint Component 写工具通常返回：

```json
{
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

Agent 应根据 validation 继续 compile/save 闭环。

如果 `status=no_op` 且 `modified=false`，通常：

```text
should_compile=false
should_save=false
```

---

## 16. Agent 不消费 transaction / review / safety

Blueprint Component 默认返回不包含：

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

物理门任务中，Agent 应拆为：

```text
1. add_component SceneRoot
2. add_component DoorMesh attach to SceneRoot
3. add_component InteractionBox attach to SceneRoot
4. add_component DoorConstraint attach to SceneRoot
5. set_component_properties DoorMesh: mesh / relative transform / collision / physics
6. set_component_properties DoorConstraint: constraint target / angular limits
7. 后续 Graph Write 写交互逻辑
```

不要把第 5 步混入 add_component。

---

## 18. 禁止行为

Agent 不得：

```text
1. 用 add_component 设置属性。
2. 依赖 add_component 返回 transform / properties。
3. 自动改名组件。
4. 自动替换已有组件。
5. 把 successful property_result 当作完整属性快照。
6. 在批量属性失败时假设部分属性已应用。
7. 跨工具簇用 Component 工具写图表逻辑。
8. 在最终报告中默认输出 transaction_id 或 review_status。
```

---

## 19. 最终报告规则

正常完成时，Agent 可报告：

```text
1. 添加了哪些组件。
2. 设置了哪些组件属性类别。
3. 是否有 no_op。
4. 是否需要 compile/save。
```

不报告：

```text
transaction_id
review_status
journal_path
rollback_data
before / after 属性值
完整 property 列表
```

---

## 20. Agent 侧验收标准

```text
1. Agent 能区分 add_component 与 set_component_property / set_component_properties。
2. Agent 不用 add_component 设置属性。
3. Agent 能解析 property_result。
4. Agent 不期待 before / after。
5. Agent 遇到 invalid_settings 能停止或修正计划。
6. Agent 理解批量属性修改是事务式。
7. Agent 不自动改名或替换组件。
8. Agent 能根据 validation 继续 compile/save。
