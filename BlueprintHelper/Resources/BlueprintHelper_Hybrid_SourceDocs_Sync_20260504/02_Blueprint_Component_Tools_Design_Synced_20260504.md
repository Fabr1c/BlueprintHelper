# 02 Blueprint Component Tools 设计文档（已同步确认 Diff）

日期：2026-05-03  
工具簇：Blueprint Component Tools / 蓝图组件树工具簇  
状态：同步确认 Diff 后的修正版  
同步范围：`add_component` 职责收窄、组件属性设置返回摘要、`name_collision` 语义、普通工具不默认返回 transaction/review/safety。

---

## 0. 本次同步结论

本文件替换旧版中以下过期口径：

```text
1. add_component 不再承担 transform / mobility / collision / physics / mesh / material / constraint 参数设置。
2. add_component 只负责创建组件和建立 attachment。
3. 组件属性必须通过 set_component_property / set_component_properties 设置。
4. 组件属性写入成功时只返回 property_result 摘要，不回显 before / after / all_properties。
5. name_collision 表示组件命名冲突，不是物理 collision。
6. 第一版 name_collision 只支持 fail_if_exists / reuse_if_exists。
7. 普通 Component 成功结果不默认返回 transaction / review / safety。
```

---

## 1. 定位

Blueprint Component Tools 负责编辑 Actor Blueprint 的组件树，即 UE 的 SCS / Component Template 层。

它不属于 Graph Write，不通过 Append / Replace / Patch / Merge 编辑组件树，也不使用 `block_id`。

组件树修改仍是 UE 写操作，UE 插件内部必须接入 Transaction Journal / Review，但普通 Component 工具结果不默认把 `transaction / review / safety` 暴露给 Agent。

---

## 2. 第一版工具形态

第一版工具建议收敛为：

```text
read_components
add_component
set_component_property
set_component_properties
remove_component
```

后续可扩展：

```text
attach_component
detach_component
set_root_component
rename_component
cleanup_blueprint_helper_component_group
```

高频复杂组件配置工具可以作为后续安全封装，但第一版 Agent 规则仍应遵守：

```text
创建组件 = add_component
设置属性 = set_component_property / set_component_properties
```

---

## 3. add_component 职责

`add_component` 只做两件事：

```text
1. 创建组件。
2. 建立组件挂接关系。
```

`add_component` 不设置：

```text
Transform
RelativeLocation / RelativeRotation / RelativeScale
Mobility
CollisionEnabled / CollisionProfileName / Collision Response
Physics / BodyInstance.bSimulatePhysics
StaticMesh
Material
PhysicsConstraint 参数
任意组件属性
```

这些全部属于组件属性修改，必须使用：

```text
set_component_property
set_component_properties
```

---

## 4. add_component Agent-facing 返回

成功示例：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_component",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "blueprint"
  },
  "data": {
    "schema": "BlueprintHelper.BlueprintComponent.v1",
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
  },
  "validation": {
    "should_compile": true,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

Agent 应理解：

```text
组件已创建并挂接。
组件属性尚未配置。
```

默认不返回：

```text
transaction
review
safety
transform
properties
collision
physics
mesh
material
```

---

## 5. name_collision 规则

字段名固定为：

```text
name_collision
```

不是：

```text
collision
```

原因：`collision` 容易和物理碰撞设置混淆。

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

## 6. 组件属性设置

调用层区分：

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

字段解释：

| 字段 | 含义 |
|---|---|
| `mode` | `single` 或 `batch`。 |
| `requested_count` | 请求设置数量。 |
| `applied_count` | 实际应用数量。 |
| `changed_count` | 实际产生变化的数量。 |
| `no_op_count` | 已应用但值未变化的数量。 |
| `invalid_settings` | 无效设置列表。 |

---

## 7. 不回显 property 快照

成功时不返回：

```text
before
after
all_properties
```

原因：

```text
1. before / after 属于 UE 内部 diff / Review / debug。
2. 大对象属性回显浪费 Token。
3. 成功结果只需要执行摘要。
```

如果 Agent 需要确认最终属性值，应调用读取工具或未来专用组件属性读取能力，而不是依赖写工具回显。

---

## 8. invalid_settings 规则

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

Agent 应根据 `invalid_settings` 修正计划或 stop_and_report。

---

## 9. 批量属性事务规则

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

第一版不支持：

```text
partial apply
allow_partial=true
```

Agent 不得在批量失败后假设部分属性已经成功写入。

---

## 10. 常见属性路径归属

以下都属于属性修改，不能混入 `add_component`：

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

必须使用：

```text
set_component_property
set_component_properties
```

---

## 11. Ownership 与 component_group_id

组件 ownership 采用 Metadata + Journal 双写。

规则：

```text
不使用 block_id。
组件 / SCS 节点 / Component Template 写入最小 ownership metadata。
Journal 记录完整 diff、rollback_data、组件组关系、创建来源和事务历史。
不依赖组件命名约定判断 ownership。
不只依赖 Journal。
```

`component_group_id` 可用于：

```text
replace_owned
cleanup owned component group
Review 分组展示
Rollback 冲突检测
```

但普通 Component 工具成功结果是否向 Agent 暴露 `component_group_id` 应按工具簇需要决定，不应与通用 `transaction / review / safety` 混为默认返回 envelope。

---

## 12. dry_run

所有组件写操作都必须支持 dry_run。

Conservative 下高风险组件操作必须 dry_run：

```text
set_root_component
reattach existing component
attach / modify user-owned component
replace_owned
remove_component
configure_physics_constraint
修改 SimulatePhysics / Collision / Mobility
修改组件 parent / root / constraint target
```

新建空蓝图内添加 BlueprintHelper-owned 组件，可以不强制 dry_run，但工具仍必须支持 dry_run。

dry_run 结果位置：

```text
status=dry_run
modified=false
data.dry_run
```

---

## 13. 删除与清理

单个组件精确删除：

```text
remove_component
```

规则：

```text
component_name 必须明确。
不允许模糊删除。
不允许按 class 批量删除。
```

删除组件属于写操作。Agent 应在删除前确认目标明确，必要时先 `read_components`。

owned 组件组清理由 Cleanup 工具簇负责：

```text
cleanup_blueprint_helper_component_group
```

---

## 14. 与 Graph Write 的关系

```text
组件工具不属于 Graph Write。
组件工具不使用 block_id。
组件树修改仍是写操作，内部必须接入 Journal / Review。
Graph Write 不用于创建或配置组件树。
```

---

## 15. 物理门任务拆分示例

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

不要把第 5 步混入 `add_component`。

---

## 16. Agent 禁止行为

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

## 17. 验收标准

```text
1. Agent 能区分 add_component 与 set_component_property / set_component_properties。
2. add_component 返回 data.component / data.attachment / data.name_collision。
3. add_component 不返回 transform / properties。
4. name_collision 不被误解为物理 collision。
5. set_component_property / set_component_properties 统一返回 data.property_result。
6. 成功属性写入不回显 before / after / all_properties。
7. invalid_settings 是唯一无效设置列表。
8. 批量属性失败时整体失败，不应用任何属性。
9. 普通成功结果不默认返回 transaction / review / safety。
10. Agent 能根据 validation 继续 compile/save。
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
