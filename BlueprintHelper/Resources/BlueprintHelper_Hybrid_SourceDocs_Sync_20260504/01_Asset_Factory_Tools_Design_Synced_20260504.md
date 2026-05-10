# 01 Asset Factory Tools 设计文档（已同步确认 Diff）

日期：2026-05-03  
工具簇：Asset Factory Tools / 资产创建工具簇  
状态：同步确认 Diff 后的修正版  
同步范围：字段协议收敛、普通工具不默认返回 transaction/review/safety、Asset Factory 职责边界、Asset 字段去冗余、validation 返回规则。

---

## 0. 本次同步结论

本文件替换旧版中以下过期口径：

```text
1. 不再要求普通 Asset Factory 成功结果默认返回 transaction_id / review / safety。
2. 不再默认返回 asset_name。
3. 不再默认返回 package_path。
4. Asset Factory 只创建资产，不把接口添加到蓝图，也不写接口函数逻辑。
5. Agent-facing 成功结果以 data.asset / data.factory / validation 为主。
6. transaction / review / rollback_data 仍可由 UE 插件内部写入 Journal / Review，但不是普通 Asset Factory 工具默认暴露给 Agent 的字段。
```

---

## 1. 定位

Asset Factory Tools 负责通过 Unreal Editor 的 AssetTools / Factory 系统创建 UE 资产。

它不是普通文件创建工具，不应命名为 `create_file`。所有创建行为都应理解为 UE Editor 内的 `.uasset` 资产创建。

Asset Factory 可以负责创建如下资产类型：

```text
Blueprint Class
Blueprint Interface
UserDefinedStruct
UserDefinedEnum
DataAsset
DataTable
WidgetBlueprint
InputAction（仅在当前版本 / runtime profile 明确支持且用户目标明确时）
```

Asset Factory 不负责：

```text
1. 将 Blueprint Interface 添加到某个 Blueprint 的 Implemented Interfaces。
2. 创建接口函数实现图。
3. 写接口函数 body。
4. 将接口函数或事件接入 EventGraph。
5. 修改 InputMappingContext 中的按键映射。
6. 修改 C++ 源码或项目配置文件。
```

完整接口交互工作流必须拆分：

```text
Asset Factory 创建 BPI 资产
→ Blueprint Class Settings 添加 Implemented Interface
→ Graph Write 创建或实现接口函数逻辑
→ Compile / Save
```

---

## 2. 工具形态

采用混合方案：

```text
asset_create = 统一 Asset Factory 入口
专用工具 = 高频复杂资产的安全封装
```

推荐专用工具：

```text
blueprint_create_interface
widget_create_blueprint
asset_create_struct
asset_create_enum
asset_create_data_asset
asset_create_data_table
asset_create_blueprint_class
```

`input_create_action` / `input_create_mapping_context` 是否可用，必须以 runtime profile 和 Enhanced Input 边界文档为准。当前阶段 Agent 不应默认自动创建或修改输入资产，除非用户明确要求且能力可用。

专用工具不是另一套实现，内部仍走 `asset_create` 及 UE 插件侧同一套 AssetTools / Factory 后端。

返回中可以标明：

```json
{
  "underlying_operation": "asset_create"
}
```

Agent 调用优先级：

```text
有专用工具时，优先使用专用工具。
asset_create 作为统一底层入口和后备工具。
```

---

## 3. asset_type 开放策略

采用双层模式：

```text
默认：白名单 asset_type。
高级：Expert / 受控 Profile 下才允许低层 asset_class / factory_class。
```

默认白名单覆盖已验证、可测试、Agent 高频使用的资产类型，例如：

```text
BlueprintClass
BlueprintInterface
UserDefinedStruct
UserDefinedEnum
DataAsset
DataTable
WidgetBlueprint
```

普通 Agent 不直接使用 `factory_class`，避免误传 UE 类名或绕过安全边界。

---

## 4. 已存在资产处理

默认：

```text
if_exists = error
```

目标路径已存在时：

```text
不自动覆盖。
不自动重命名。
不自动 create_unique。
```

只有显式传入：

```text
reuse_if_type_matches
```

时，才允许复用已有同类型资产。

规则：

```text
同类型：ok=true, status=no_op 或 reused, modified=false。
类型不一致：error。
```

Conservative 下复用已有资产属于高风险或至少需 dry_run 的路径，因为它可能改变后续 Agent 计划的目标归属。

---

## 5. Agent-facing 成功返回字段

普通 Asset Factory 成功结果默认使用精简 ToolResultBase：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "asset_create",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/Input/IA_Interact",
    "target_type": "asset"
  },
  "data": {
    "schema": "BlueprintHelper.AssetFactory.v1",
    "asset": {
      "asset_path": "/Game/Input/IA_Interact",
      "asset_class": "InputAction",
      "asset_type": "input_action"
    },
    "factory": {
      "factory_type": "input_action"
    }
  },
  "validation": {
    "should_compile": false,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

默认不返回：

```text
transaction
review
safety
asset_name
package_path
```

字段解释：

| 字段 | 规则 |
|---|---|
| `data.asset.asset_path` | Agent 侧主定位字段。 |
| `data.asset.asset_class` | 创建出的资产类或资源类别。 |
| `data.asset.asset_type` | 白名单语义类型。 |
| `data.factory.factory_type` | 实际使用的 Factory 类型摘要。 |
| `data.factory.parent_class` | 仅 Blueprint Class 创建等需要父类时返回。 |
| `validation.should_save` | 创建资产后通常为 true。 |
| `validation.should_compile` | 取决于资产类型。 |

`asset_name` 不默认返回，因为可由 `asset_path` 推导。  
`package_path` 不默认返回，因为会与 `asset_path` / object path 产生冗余或歧义。

---

## 6. Blueprint Interface 创建示例

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "blueprint_create_interface",
  "status": "applied",
  "modified": true,
  "data": {
    "schema": "BlueprintHelper.AssetFactory.v1",
    "asset": {
      "asset_path": "/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable",
      "asset_class": "BlueprintInterface",
      "asset_type": "blueprint_interface"
    },
    "factory": {
      "factory_type": "blueprint_interface"
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

创建 BPI 资产后，不代表任何 Blueprint 已经实现该接口。Agent 后续必须显式调用 Blueprint Class Settings 工具添加 Implemented Interface。

---

## 7. 创建后行为

`asset_create` 默认只创建资产：

```text
不自动打开。
不自动编译，除非 workflow / profile 明确允许。
不自动保存，除非 workflow / profile 明确允许。
```

Agent-facing 返回通过 `validation` 告诉 Agent 后续闭环：

```text
should_compile
should_save
compiled
saved
```

无需编译资产返回：

```text
should_compile=false
```

例如 DataTable、DataAsset 等。

Blueprint Class、Blueprint Interface、Widget Blueprint 等可返回：

```text
should_compile=true
```

是否自动 compile / save 由 Safety Profile、workflow 参数和当前 runtime profile 决定。

---

## 8. dry_run

所有 Asset Factory 写操作必须支持 `dry_run`。

dry_run 不创建真实资产，只返回创建计划和预检结果。

返回位置：

```text
status=dry_run
modified=false
data.dry_run
```

dry_run 示例：

```json
{
  "ok": true,
  "operation": "asset_create",
  "status": "dry_run",
  "modified": false,
  "data": {
    "schema": "BlueprintHelper.AssetFactoryDryRun.v1",
    "dry_run": {
      "would_create_asset": true,
      "asset_path": "/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable",
      "asset_type": "blueprint_interface",
      "resolved_factory": "BlueprintFactory",
      "resolved_asset_class": "BlueprintInterface",
      "parent_class": null,
      "name_conflict": false,
      "required_modules": [],
      "can_execute": true,
      "warnings": [],
      "errors": []
    }
  }
}
```

不默认返回 `package_path`。

Conservative 下，创建全新不存在路径的白名单资产可以不强制 dry_run，但工具必须支持 dry_run。

以下情况必须 dry_run：

```text
目标路径已存在并请求 reuse_if_type_matches
使用高级 asset_class / factory_class
创建高风险资产类型
路径冲突
parent_class 解析不明确
factory_options 包含复杂配置
```

dry_run 成功不等于正式创建。正式创建时必须重新检查当前资产状态，避免 TOCTOU 问题。

---

## 9. Journal / Review 边界

所有资产创建都属于 UE 写操作，UE 插件内部必须接入：

```text
Transaction Journal
Review Store
rollback_data
```

但普通 Asset Factory Agent-facing 成功结果不默认返回：

```text
transaction
review
safety
```

这些属于内部审计 / Review UI / rollback 工作流。只有调试、失败定位、rollback 或后续必须引用时，相关工具才按需暴露必要摘要。

资产创建不使用 `block_id`。

资产创建 rollback 采用条件删除，具体规则见 Transaction / Journal / Review 文档。

---

## 10. 最小工具 Schema 草案

```ts
asset_create({
  asset_path: string,
  asset_type: string,
  parent_class?: string,
  factory_options?: object,
  if_exists?: "error" | "reuse_if_type_matches",
  dry_run?: boolean
})
```

高级字段仅在 Expert / 受控 Profile 下允许：

```ts
asset_create({
  asset_path: string,
  asset_class?: string,
  factory_class?: string,
  factory_options?: object
})
```

如果 Profile 不允许低层字段，返回：

```text
ProfilePolicyViolation
```

---

## 11. Agent 禁止行为

Agent 不得：

```text
1. 把 Asset Factory 创建 BPI 误认为目标 Blueprint 已实现该接口。
2. 期待 asset_name / package_path 默认返回。
3. 期待普通 Asset Factory 结果默认返回 transaction_id / review_status。
4. 在最终报告中默认输出 transaction_id 或 review_status。
5. 用 Asset Factory 修改蓝图 Class Settings。
6. 用 Asset Factory 写接口函数 body 或 EventGraph 逻辑。
7. 未经 runtime profile / user target 确认自动创建或修改输入映射资产。
```

---

## 12. 验收标准

```text
1. 成功创建资产时返回 data.asset.asset_path / asset_class / asset_type。
2. 成功创建资产时返回 data.factory.factory_type。
3. asset_name 不默认返回。
4. package_path 不默认返回。
5. 普通成功结果不默认返回 transaction / review / safety。
6. dry_run 结果只在 status=dry_run 时返回 data.dry_run。
7. validation 能正确反映 should_compile / should_save。
8. Blueprint Interface 创建不自动添加到 Blueprint。
9. Asset Factory 不写接口函数逻辑。
10. Asset Factory 内部写 Journal / Review，但 Agent 默认不消费这些内部字段。
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
