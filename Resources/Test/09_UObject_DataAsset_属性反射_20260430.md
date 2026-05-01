---
项目：BlueprintHelper
版本目标：v0.3.0 当前源码包
生成日期：2026-04-30
适用范围：UE5.3+ BlueprintHelper 插件、MCP Server、UE Bridge、Agent 工作流
---

# UObject / DataAsset 属性反射测试

## 通用测试前提

- 使用独立测试工程或 `/Game/BlueprintHelperTest/` 测试目录，不直接操作正式项目资产。
- 测试资产命名建议：
  - Blueprint：`/Game/BlueprintHelperTest/BP_BH_FunctionalActor.BP_BH_FunctionalActor`
  - WidgetBlueprint：`/Game/BlueprintHelperTest/WBP_BH_Test.WBP_BH_Test`
  - DataAsset：`/Game/BlueprintHelperTest/DA_BH_TestConfig.DA_BH_TestConfig`
  - DataTable：`/Game/BlueprintHelperTest/DT_BH_TestItems.DT_BH_TestItems`
- 除生命周期和构建测试外，默认 Unreal Editor 已启动，BlueprintHelper Bridge 可连接到 `BRIDGE_HOST:BRIDGE_PORT`。
- 所有破坏性测试必须在测试资产上执行；删除、导入、控制台命令、关闭编辑器、构建工程属于高风险或关键风险步骤。
- 写操作执行顺序默认是：读取当前状态 → 生成最小写入计划 → 执行写入 → 编译/校验 → 按需保存 → 读取回验。


## 覆盖对象

- `blueprint_get_object_properties`
- `blueprint_set_object_property`
- `BlueprintHelperPropertyReflectionService`
- DataAsset / UObject 属性读取和 UE text import 写入。

## 测试用例

| ID | 功能 | 步骤 | 期望结果 | 优先级 |
|---|---|---|---|---|
| OBJ-001 | 读取 DataAsset 属性 | 对测试 DataAsset 调用 `blueprint_get_object_properties` | 返回属性名、类型、当前值、可编辑性 | P0 |
| OBJ-002 | 读取 Blueprint CDO/资产属性 | 对 Blueprint 或其他 UObject 资产读取 | 返回可反射属性；不可编辑字段标记明确 | P1 |
| OBJ-003 | 读取不存在资产 | asset_path 不存在 | 返回 AssetNotFound；不崩溃 | P0 |
| OBJ-004 | 设置 bool | `property_name=bEnabled`、`value=true` | 回读为 true；保存后仍为 true | P0 |
| OBJ-005 | 设置 int/float | 设置数值字段 | 类型转换正确；非法字符串失败 | P0 |
| OBJ-006 | 设置 string/name/text | 分别设置字符串、Name、Text | UE 文本导入格式正确；中文文本不乱码 | P0 |
| OBJ-007 | 设置 enum | 设置合法枚举名 | 回读枚举一致；非法枚举失败 | P1 |
| OBJ-008 | 设置 object reference | 设置对另一个资产的引用 | 合法路径成功；不存在路径失败 | P1 |
| OBJ-009 | 设置 soft object/class | 使用软引用路径 | 回读路径一致；不强制加载不必要资产 | P2 |
| OBJ-010 | 设置 struct | 设置 Vector/Color/自定义 struct 文本 | 字段解析正确；部分字段缺失时错误明确 | P1 |
| OBJ-011 | 设置 array | 使用 UE text import 设置数组 | 元素数量和值正确；类型不匹配失败 | P1 |
| OBJ-012 | 设置 map/set | 使用 UE text import 设置容器 | 合法值成功；重复/非法 key 失败 | P2 |
| OBJ-013 | 设置只读属性 | 对不可编辑属性写入 | 返回 unsafe flag / not editable；值不变 | P0 |
| OBJ-014 | 设置不存在属性 | property_name 不存在 | 返回 PropertyNotFound；对象不变 | P0 |
| OBJ-015 | 写入后 Undo | 设置属性后调用 undo | 属性恢复旧值 | P1 |
| OBJ-016 | 写入后保存重开 | set → save → reopen editor/asset → get | 值持久化 | P1 |

## 验收标准

- 属性服务必须严格区分可编辑属性、只读属性和不存在属性。
- `value` 字符串必须走 UE 可诊断的文本导入路径。
- 失败写入不应产生脏包或部分写入。
