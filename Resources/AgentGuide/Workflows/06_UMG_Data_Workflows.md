# 06 - UMG / DataAsset / DataTable 工作流

## 1. UMG WidgetTree 读取

流程：

```text
resolve widget blueprint asset
 -> read widget tree
 -> locate widget by name/type/path
 -> read relevant properties
 -> plan tree/property changes
```

注意：

- 控件名必须唯一或使用完整层级路径。
- 删除或移动控件前必须读取父子关系。
- 对 Slot 属性、RenderTransform、Visibility、Text、Brush 等属性要区分控件属性和 Slot 属性。

## 2. UMG 添加控件

流程：

```text
read widget tree
 -> identify parent panel
 -> add widget with explicit class/name
 -> set properties
 -> read back tree
 -> save asset
```

默认规则：

- 不要自动覆盖已有同名控件。
- 不要在不知道父容器类型时猜 Slot 参数。
- UI 布局变更后建议读取回查树结构和关键属性。

## 3. DataAsset / UObject 属性读写

流程：

```text
resolve asset path
 -> read object properties
 -> verify property name/type/current value
 -> set property
 -> read back property
 -> save asset
```

注意：

- 枚举值、软对象路径、类路径、结构体字段必须按 UE 可序列化格式写入。
- 不要把复杂结构体当作普通字符串写入。
- 批量改属性时逐项报告成功/失败。

## 4. DataTable 读取

流程：

```text
resolve data table asset
 -> read row names or sample rows
 -> inspect row struct fields
 -> locate target RowName
```

注意：

- RowName 是主键，不是普通字段。
- 更新行前要确认字段名和字段类型。

## 5. DataTable 添加 / 更新行

流程：

```text
read schema/sample row
 -> validate new row fields
 -> add or update row
 -> read back row
 -> save asset
```

默认规则：

- 更新行时只改用户要求的字段，除非用户明确要求整行覆盖。
- 新增行应填齐必需字段；缺失字段时使用表结构默认值或报告无法确认。

## 6. DataTable 删除行

删除前必须读取目标行并确认 RowName。删除后读取 RowNames 回查，不要误删相近名称。
