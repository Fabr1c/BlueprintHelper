# BlueprintHelper UMG / Widget Blueprint UE 侧 C++ 可执行实现计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置实现  
来源字段稿：`BlueprintHelper_UMG_WidgetBlueprint_UE_FieldMapping_20260503.md`  
实现范围：UE 插件侧 C++  
不包含：MCP Server TypeScript 封装、Agent Skill 文档、Widget Animation、Binding Graph、UMG 事件绑定图表、复杂 Slate Brush 资源批处理

---

## 0. 实现目标

实现 UMG / Widget Blueprint 第一版工具簇：

```text
add_widget_to_tree
set_widget_property
set_widget_properties
set_widget_slot_property
set_widget_slot_properties
remove_widget_from_tree
read_widget_tree
read_widget_properties
read_widget_slot_properties
```

字段契约核心点：

```text
1. UMG 第一版按 Widget Tree / 普通属性 / Slot 属性拆工具。
2. add_widget_to_tree 只创建并插入 widget，不设置普通属性。
3. add_widget_to_tree 成功只返回 added_count。
4. set_widget_property / set_widget_properties 成功只返回 property_result 计数。
5. set_widget_slot_property / set_widget_slot_properties 与普通属性分离。
6. remove_widget_from_tree 必须 dry_run，成功只返回 removed_count。
7. read_widget_tree 返回压缩树结构，不默认返回所有属性。
8. read_widget_properties / read_widget_slot_properties 独立读取属性值。
9. UMG 写工具成功不返回 write_ref / transaction_id / review / safety。
10. UMG 写工具成功保留 validation。
11. 所有 UMG data.schema 使用短命名。
```

---

## 1. 当前依赖与复用前提

本计划假设 UE 插件侧已有或将统一具备：

```text
FBlueprintHelperToolResultBase
FBlueprintHelperToolResultBuilder
FBlueprintHelperPropertyReflectionService
FBlueprintHelperScopedAssetMutation
FBlueprintHelperRequestValidator
FBlueprintHelperBridgeRouter
FBlueprintHelperFailedItem
FBlueprintHelperConflictItem
FBlueprintHelperDryRunResult
FBlueprintHelperValidationResult
```

UMG 相关 UE 依赖模块应确认已加入 `BlueprintHelper.Build.cs`：

```text
UMG
UMGEditor
WidgetBlueprint
Slate
SlateCore
UnrealEd
Kismet
BlueprintGraph
PropertyEditor（如需要属性反射辅助）
```

如当前模块已经使用 Widget Blueprint 能力，则只需核对 PrivateDependencyModuleNames。

---

## 2. Phase A：新增类型文件

### 2.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperWidgetTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperWidgetTypes.cpp
```

如果已有 WidgetService 类型文件，则在现有文件中补齐 DTO，避免重复定义。

### 2.2 新增枚举

```cpp
enum class EBlueprintHelperWidgetScope : uint8
{
    WidgetTree,
    WidgetProperty,
    WidgetSlot
};

enum class EBlueprintHelperWidgetReadScope : uint8
{
    WidgetTree,
    WidgetProperty,
    WidgetSlot
};

enum class EBlueprintHelperNameCollisionPolicy : uint8
{
    FailIfExists,
    ReuseIfExists
};

enum class EBlueprintHelperPropertyWriteMode : uint8
{
    Single,
    Batch
};

enum class EBlueprintHelperWidgetStage : uint8
{
    ParseInput,
    ResolveAsset,
    ResolveWidgetBlueprint,
    ResolveWidgetTree,
    ResolveParentWidget,
    ResolveWidget,
    ResolveSlot,
    NameCollisionCheck,
    ValidateWidgetClass,
    ValidateProperties,
    ValidateSlot,
    DryRun,
    CreateWidget,
    InsertWidget,
    SetProperty,
    SetSlotProperty,
    RemoveWidget,
    MarkModified,
    Rollback
};

enum class EBlueprintHelperWidgetErrorCode : uint8
{
    InvalidRequest,
    AssetNotFound,
    TargetNotWidgetBlueprint,
    WidgetTreeNotFound,
    ParentWidgetNotFound,
    WidgetNotFound,
    WidgetNameAlreadyExists,
    UnsupportedNameCollisionPolicy,
    UnsupportedWidgetClass,
    WidgetClassNotFound,
    WidgetClassNotAllowed,
    InvalidWidgetPropertySettings,
    InvalidWidgetSlotPropertySettings,
    PropertyNotFound,
    PropertyNotWritable,
    TypeMismatch,
    ValueOutOfRange,
    SlotTypeMismatch,
    WidgetHasBindingsOrReferences,
    RemoveWidgetDryRunRequired,
    WidgetCreateFailed,
    WidgetInsertFailed,
    WidgetRemoveFailed,
    RollbackFailed,
    InternalError
};
```

### 2.3 字符串序列化

稳定输出：

```text
widget_tree
widget_property
widget_slot
fail_if_exists
reuse_if_exists
single
batch
asset_not_found
widget_name_already_exists
slot_type_mismatch
```

不要输出 C++ enum 原名。

---

## 3. Phase B：Agent-facing DTO

### 3.1 AddWidgetToTree

```cpp
struct FBlueprintHelperAddWidgetResultData
{
    FString Schema = TEXT("AddWidgetToTree.v1");
    FBlueprintHelperAddWidgetResult AddWidgetResult;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperAddWidgetResult
{
    int32 AddedCount = 0;

    // no_op only, name_collision=reuse_if_exists 且 widget 已存在。
    TOptional<bool> bReusedExisting;

    TSharedRef<FJsonObject> ToJson() const;
};
```

成功只输出：

```json
{
  "schema": "AddWidgetToTree.v1",
  "add_widget_result": {
    "added_count": 1
  }
}
```

no_op 可输出：

```json
{
  "schema": "AddWidgetToTree.v1",
  "add_widget_result": {
    "added_count": 0,
    "reused_existing": true
  }
}
```

### 3.2 Widget property result

普通属性和 Slot 属性共用计数结构，但 data.schema 不同。

```cpp
struct FBlueprintHelperWidgetPropertyResultData
{
    FString Schema; // SetWidgetProperty.v1 or SetWidgetSlotProperty.v1
    FBlueprintHelperPropertyWriteResult PropertyResult;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperPropertyWriteResult
{
    FString Mode; // single | batch
    int32 RequestedCount = 0;
    int32 AppliedCount = 0;
    int32 ChangedCount = 0;
    int32 NoOpCount = 0;

    TSharedRef<FJsonObject> ToJson() const;
};
```

成功不包含：

```text
invalid_settings
before
after
all_properties
property_paths
widget_snapshot
```

invalid 项只在失败时放入 `error.conflicts[]`。

### 3.3 RemoveWidgetFromTree

```cpp
struct FBlueprintHelperRemoveWidgetResultData
{
    FString Schema = TEXT("RemoveWidgetFromTree.v1");
    FBlueprintHelperRemoveWidgetResult RemoveWidgetResult;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperRemoveWidgetResult
{
    int32 RemovedCount = 0;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.4 ReadWidgetTree

```cpp
struct FBlueprintHelperReadWidgetTreeResultData
{
    FString Schema = TEXT("ReadWidgetTree.v1");
    FBlueprintHelperWidgetTreeSummary WidgetTree;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperWidgetTreeSummary
{
    FString Root;
    TArray<FBlueprintHelperWidgetTreeItem> Widgets;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperWidgetTreeItem
{
    FString WidgetName;
    FString WidgetClass;
    TArray<FString> Children;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.5 ReadWidgetProperties / ReadWidgetSlotProperties

```cpp
struct FBlueprintHelperReadWidgetPropertiesResultData
{
    FString Schema = TEXT("ReadWidgetProperties.v1");
    FBlueprintHelperWidgetProperties Properties;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperReadWidgetSlotPropertiesResultData
{
    FString Schema = TEXT("ReadWidgetSlotProperties.v1");
    FBlueprintHelperWidgetSlotProperties SlotProperties;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperWidgetProperties
{
    FString WidgetName;
    int32 PropertyCount = 0;
    TSharedPtr<FJsonObject> Values;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperWidgetSlotProperties
{
    FString WidgetName;
    FString SlotType;
    int32 PropertyCount = 0;
    TSharedPtr<FJsonObject> Values;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.6 明确禁止通用字段

UMG 写工具成功 DTO 不包含：

```cpp
FBlueprintHelperWriteRef
FString TransactionId
FString WidgetRef
FString WidgetPath
FString ParentRef
FString SlotRef
FString ReviewStatus
FString SafetyProfile
```

---

## 4. Phase C：新增 WidgetService

### 4.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperWidgetService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperWidgetService.cpp
```

如果当前已有 `FBlueprintHelperWidgetService`，则按本文拆分方法并收敛 result。

### 4.2 服务接口

```cpp
class FBlueprintHelperWidgetService
{
public:
    FBlueprintHelperToolResultBase AddWidgetToTree(const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase SetWidgetProperty(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperToolResultBase SetWidgetProperties(const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase SetWidgetSlotProperty(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperToolResultBase SetWidgetSlotProperties(const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase RemoveWidgetFromTree(const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase ReadWidgetTree(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperToolResultBase ReadWidgetProperties(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperToolResultBase ReadWidgetSlotProperties(const TSharedPtr<FJsonObject>& Payload) const;

private:
    bool ResolveWidgetBlueprint(
        const FString& AssetPath,
        UWidgetBlueprint*& OutWidgetBlueprint,
        FBlueprintHelperToolError& OutError) const;

    bool GetWidgetTree(
        UWidgetBlueprint* WidgetBlueprint,
        UWidgetTree*& OutWidgetTree,
        FBlueprintHelperToolError& OutError) const;

    UWidget* FindWidgetByName(UWidgetTree* WidgetTree, const FString& WidgetName) const;
};
```

---

## 5. Phase D：Widget Blueprint 解析

### 5.1 Asset 类型

解析目标必须是 `UWidgetBlueprint`：

```cpp
UObject* Asset = LoadObject<UObject>(nullptr, *ObjectPath);
UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Asset);
```

失败：

```text
asset_not_found
target_not_widget_blueprint
widget_tree_not_found
```

### 5.2 WidgetTree

```cpp
UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
```

如果为空：

```text
error.code=widget_tree_not_found
stage=resolve_widget_tree
```

### 5.3 Mark modified

UMG 写操作后：

```cpp
WidgetBlueprint->Modify();
WidgetBlueprint->WidgetTree->Modify();
FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
// 或 MarkBlueprintAsModified，按具体修改类型选择。
WidgetBlueprint->MarkPackageDirty();
```

普通属性 / Slot 属性变更通常不需要编译，但需要 save。结构写入和删除建议 should_compile=true。

---

## 6. Phase E：add_widget_to_tree

### 6.1 Request 结构

```cpp
struct FBlueprintHelperAddWidgetToTreeRequest
{
    FString AssetPath;
    FString WidgetClass;
    FString WidgetName;
    FString ParentWidgetName;
    EBlueprintHelperNameCollisionPolicy NameCollisionPolicy =
        EBlueprintHelperNameCollisionPolicy::FailIfExists;
};
```

### 6.2 输入 JSON 建议

```json
{
  "asset_path": "/Game/UI/WBP_MainMenu",
  "widget_class": "TextBlock",
  "widget_name": "TitleText",
  "parent_widget_name": "CanvasRoot",
  "name_collision": "fail_if_exists"
}
```

### 6.3 创建流程

```text
1. ResolveWidgetBlueprint。
2. ResolveWidgetTree。
3. 检查 widget_name 是否已存在。
4. name_collision=fail_if_exists 且存在：失败。
5. name_collision=reuse_if_exists 且存在：no_op。
6. Resolve widget_class。
7. Resolve parent_widget_name。
8. 创建 widget。
9. 插入父级 panel / named slot。
10. Mark modified。
11. 返回 added_count。
```

### 6.4 创建 Widget

示例：

```cpp
UClass* WidgetClass = ResolveWidgetClass(Request.WidgetClass);
UWidget* NewWidget = WidgetTree->ConstructWidget<UWidget>(
    WidgetClass,
    FName(*Request.WidgetName));
```

### 6.5 插入父级

第一版建议只支持 `UPanelWidget` 父级：

```cpp
UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
if (!ParentPanel)
{
    error.code = "parent_widget_not_panel";
}
ParentPanel->AddChild(NewWidget);
```

Root 场景：

```text
如果 WidgetTree->RootWidget 为空，允许设置 RootWidget。
如果已有 RootWidget，必须指定 parent_widget_name。
```

### 6.6 Slot 属性不在 add 中设置

即使 AddChild 返回 `UPanelSlot*`，也不要在 add 工具里设置 Position / Padding / Alignment 等属性。  
后续必须通过：

```text
set_widget_slot_property / set_widget_slot_properties
```

### 6.7 成功返回

```json
{
  "data": {
    "schema": "AddWidgetToTree.v1",
    "add_widget_result": {
      "added_count": 1
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

### 6.8 成功不返回

```text
widget_name
widget_ref
widget_path
parent_ref
slot_ref
widget_tree
write_ref
transaction_id
review
safety
```

---

## 7. Phase F：set_widget_property / set_widget_properties

### 7.1 Request 结构

```cpp
struct FBlueprintHelperSetWidgetPropertyItem
{
    FString WidgetName;
    FString PropertyPath;
    TSharedPtr<FJsonValue> Value;
};

struct FBlueprintHelperSetWidgetPropertiesRequest
{
    FString AssetPath;
    TArray<FBlueprintHelperSetWidgetPropertyItem> Items;
    bool bIsBatch = false;
};
```

单属性工具内部转换成 `Items.Num()==1`。

### 7.2 执行流程

```text
1. ResolveWidgetBlueprint。
2. ResolveWidgetTree。
3. 对所有 item 做预校验：
   - widget 存在
   - property 存在
   - property 可写
   - JSON value 可转换为 UE 属性类型
4. 只要任意 invalid：整批失败，不应用任何属性。
5. Begin ScopedAssetMutation。
6. 应用所有属性。
7. 计算 requested/applied/changed/no_op。
8. Mark modified。
9. 返回 property_result。
```

### 7.3 属性写入

复用 `FBlueprintHelperPropertyReflectionService`：

```cpp
bool SetObjectPropertyFromJson(
    UObject* TargetObject,
    const FString& PropertyPath,
    const TSharedPtr<FJsonValue>& Value,
    FBlueprintHelperPropertySetResult& OutResult);
```

支持路径示例：

```text
Text
Visibility
IsEnabled
ColorAndOpacity
Brush
Percent
RenderOpacity
```

### 7.4 事务化批量

预校验阶段不能写入。  
只有所有 item 都合法后才执行写入。

如果写入中某个属性失败：

```text
尝试 rollback。
成功 rollback → modified=false。
失败 rollback → modified=true，error.code=rollback_failed。
```

### 7.5 成功返回

单属性：

```json
"property_result": {
  "mode": "single",
  "requested_count": 1,
  "applied_count": 1,
  "changed_count": 1,
  "no_op_count": 0
}
```

批量：

```json
"property_result": {
  "mode": "batch",
  "requested_count": 3,
  "applied_count": 3,
  "changed_count": 2,
  "no_op_count": 1
}
```

validation：

```text
should_compile=false
should_save=true
compiled=false
saved=false
```

### 7.6 失败返回

invalid 时：

```text
ok=false
status=failed
modified=false
error.code=invalid_widget_property_settings
stage=validate_properties
error.conflicts[] 包含 widget_name / property / code
```

不返回 `property_result`。

---

## 8. Phase G：set_widget_slot_property / set_widget_slot_properties

### 8.1 Request 结构

```cpp
struct FBlueprintHelperSetWidgetSlotPropertyItem
{
    FString WidgetName;
    FString PropertyPath;
    TSharedPtr<FJsonValue> Value;
};

struct FBlueprintHelperSetWidgetSlotPropertiesRequest
{
    FString AssetPath;
    TArray<FBlueprintHelperSetWidgetSlotPropertyItem> Items;
    bool bIsBatch = false;
};
```

### 8.2 Slot 解析

```cpp
UWidget* Widget = FindWidgetByName(WidgetTree, WidgetName);
UPanelSlot* Slot = Widget ? Widget->Slot : nullptr;
```

如果 Slot 为空：

```text
error.code=slot_not_found
stage=resolve_slot
```

如果属性不适用于当前 Slot 类型：

```text
error.code=slot_type_mismatch
stage=validate_slot
conflicts[].slot_type = Slot->GetClass()->GetName()
```

### 8.3 属性写入

复用 PropertyReflectionService，对 `UPanelSlot` 对象写属性。

常见类型：

```text
UCanvasPanelSlot
UHorizontalBoxSlot
UVerticalBoxSlot
UOverlaySlot
USizeBoxSlot
UGridSlot
UBorderSlot
```

### 8.4 成功返回

data.schema：

```text
SetWidgetSlotProperty.v1
```

result 同普通 property：

```json
"property_result": {
  "mode": "batch",
  "requested_count": 4,
  "applied_count": 4,
  "changed_count": 4,
  "no_op_count": 0
}
```

validation：

```text
should_compile=false
should_save=true
```

---

## 9. Phase H：remove_widget_from_tree

### 9.1 Request 结构

```cpp
struct FBlueprintHelperRemoveWidgetFromTreeRequest
{
    FString AssetPath;
    FString WidgetName;
    bool bDryRun = false;
};
```

### 9.2 dry_run 强制

正式执行前必须 dry_run。  
服务层可以采用两种策略：

```text
A. 如果 dry_run=false，直接 ProfilePolicyViolation / remove_widget_dry_run_required。
B. 如果调用参数 include confirmed_after_dry_run，才允许正式执行。
```

字段稿只要求工具必须 dry_run，未定义 confirmation token。第一版推荐：

```text
dry_run=true 返回 dry_run。
dry_run=false 正式执行时，服务内部仍重新执行同等 preflight；由 Agent/workflow 保证先 dry_run。
```

若 Safety Profile 要求强制外部 dry_run，则 Runtime/Profile 层拦截。

### 9.3 dry_run checks

```text
1. Widget 存在。
2. 不是 root widget，或允许删除 root。
3. 无绑定 / graph reference / animation reference。
4. 子树删除范围可确定。
5. 不存在外部引用阻断。
```

第一版引用检查可保守：

```text
如果无法确认引用安全，blocked=widget_reference_check_unavailable。
```

### 9.4 删除流程

```text
1. ResolveWidgetBlueprint。
2. ResolveWidgetTree。
3. Find widget。
4. Preflight references。
5. Begin ScopedAssetMutation。
6. 从父 PanelWidget 移除。
7. 对子树节点做必要清理。
8. Mark structurally modified。
9. 返回 removed_count=1。
```

UE API：

```cpp
if (UPanelWidget* Parent = Widget->GetParent())
{
    Parent->RemoveChild(Widget);
}
else if (WidgetTree->RootWidget == Widget)
{
    WidgetTree->RootWidget = nullptr;
}
```

### 9.5 成功返回

```json
{
  "schema": "RemoveWidgetFromTree.v1",
  "remove_widget_result": {
    "removed_count": 1
  }
}
```

validation：

```text
should_compile=true
should_save=true
```

成功不返回 removed widget ref。

### 9.6 blocked / failed 定位

blocked / failed 才可返回：

```text
widget_name
ref
```

例如：

```json
"failed_item": {
  "type": "widget",
  "widget_name": "StartButton"
}
```

---

## 10. Phase I：read_widget_tree

### 10.1 Request

```cpp
struct FBlueprintHelperReadWidgetTreeRequest
{
    FString AssetPath;
};
```

### 10.2 遍历 WidgetTree

```cpp
TArray<UWidget*> AllWidgets;
WidgetTree->GetAllWidgets(AllWidgets);
```

或从 RootWidget 递归遍历，以保持父子层级。

### 10.3 Children 解析

对 `UPanelWidget`：

```cpp
for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
{
    UWidget* Child = Panel->GetChildAt(i);
}
```

对 `UNamedSlot` / `UContentWidget` 视类型补充第一版支持，若未支持可返回 children=[] 并在 internal diagnostics 记录，不进入 Agent-facing 成功结果。

### 10.4 成功返回

```json
{
  "schema": "ReadWidgetTree.v1",
  "widget_tree": {
    "root": "CanvasRoot",
    "widgets": [
      {
        "widget_name": "CanvasRoot",
        "widget_class": "CanvasPanel",
        "children": ["TitleText", "StartButton"]
      }
    ]
  }
}
```

不返回所有属性、slot、binding、animations、graph references。

---

## 11. Phase J：read_widget_properties

### 11.1 Request

```cpp
struct FBlueprintHelperReadWidgetPropertiesRequest
{
    FString AssetPath;
    FString WidgetName;
    TArray<FString> PropertyPaths;
};
```

### 11.2 字段读取

必须明确 property paths。  
不建议默认返回所有属性。

如果 `PropertyPaths` 为空，第一版建议失败：

```text
error.code=invalid_request
message=read_widget_properties requires explicit property paths.
```

这样符合“read_widget_tree 不默认返回所有属性”的 Token 收敛原则。

### 11.3 成功返回

```json
{
  "schema": "ReadWidgetProperties.v1",
  "properties": {
    "widget_name": "TitleText",
    "property_count": 2,
    "values": {
      "Text": "Start Game",
      "Visibility": "Visible"
    }
  }
}
```

---

## 12. Phase K：read_widget_slot_properties

### 12.1 Request

```cpp
struct FBlueprintHelperReadWidgetSlotPropertiesRequest
{
    FString AssetPath;
    FString WidgetName;
    TArray<FString> PropertyPaths;
};
```

### 12.2 Slot 读取

```cpp
UWidget* Widget = FindWidgetByName(WidgetTree, WidgetName);
UPanelSlot* Slot = Widget ? Widget->Slot : nullptr;
```

失败：

```text
slot_not_found
widget_not_found
property_not_found
```

### 12.3 成功返回

```json
{
  "schema": "ReadWidgetSlotProperties.v1",
  "slot_properties": {
    "widget_name": "StartButton",
    "slot_type": "CanvasPanelSlot",
    "property_count": 3,
    "values": {
      "Position": [100, 200],
      "Size": [300, 80],
      "ZOrder": 1
    }
  }
}
```

---

## 13. Phase L：Validation 规则

UMG 写工具成功保留 validation。

### 13.1 WidgetTree 结构写入

```text
add_widget_to_tree:
  should_compile=true
  should_save=true

remove_widget_from_tree:
  should_compile=true
  should_save=true
```

### 13.2 普通属性 / Slot 属性

```text
set_widget_property / set_widget_properties:
  should_compile=false
  should_save=true

set_widget_slot_property / set_widget_slot_properties:
  should_compile=false
  should_save=true
```

### 13.3 no_op

```text
should_compile=false
should_save=false
compiled=false
saved=false
```

### 13.4 read 工具

read 工具不返回 validation。

---

## 14. Phase M：Journal / Review 边界

字段稿要求 UMG 写工具成功不返回 `write_ref / transaction_id / review / safety`。  
但 UE 内部是否记录 Journal 由全局 Transaction / Journal 规则决定。

建议第一版：

```text
UMG 写操作内部记录 Journal / Review / rollback_data。
Agent-facing 成功结果不返回 transaction_id。
```

如果当前 Transaction 系统尚未覆盖 UMG，可先记录 minimal operation log：

```text
operation
asset_path
widget operation summary
rollback snapshot
validation
```

但不要暴露给 Agent-facing 成功结果。

---

## 15. Phase N：Bridge Router 接入

### 15.1 新增 commands

```text
add_widget_to_tree
set_widget_property
set_widget_properties
set_widget_slot_property
set_widget_slot_properties
remove_widget_from_tree
read_widget_tree
read_widget_properties
read_widget_slot_properties
```

### 15.2 Router 分支

```cpp
if (Request.Command == TEXT("add_widget_to_tree"))
{
    return HandleAddWidgetToTree(Request);
}
if (Request.Command == TEXT("set_widget_property"))
{
    return HandleSetWidgetProperty(Request);
}
...
```

### 15.3 Handler 模板

```cpp
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleAddWidgetToTree(
    const FBlueprintHelperBridgeRequest& Req) const
{
    FBlueprintHelperToolResultBase Result =
        WidgetService.AddWidgetToTree(Req.Payload);

    FBlueprintHelperBridgeResponse Resp = Result.bOk
        ? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
        : FBlueprintHelperBridgeResponse::Error(
            Req.RequestId,
            EBlueprintHelperBridgeError::ExecutionFailed,
            Result.Error.IsSet() ? Result.Error->Message : TEXT("UMG tool failed"));

    Resp.Result = Result.ToJson();
    return Resp;
}
```

---

## 16. Phase O：RequestValidator / 权限

### 16.1 add_widget_to_tree

```cpp
RequireString(Payload, TEXT("asset_path"));
RequireString(Payload, TEXT("widget_class"));
RequireString(Payload, TEXT("widget_name"));
OptionalString(Payload, TEXT("parent_widget_name"));
OptionalString(Payload, TEXT("name_collision"));
```

### 16.2 property write

```cpp
RequireString(Payload, TEXT("asset_path"));
RequireString(Payload, TEXT("widget_name"));       // single
RequireString(Payload, TEXT("property_path"));     // single
RequireAny(Payload, TEXT("value"));                // single

RequireArray(Payload, TEXT("properties"));         // batch
```

### 16.3 slot property write

同 property write，但 scope 为 slot。

### 16.4 remove_widget_from_tree

```cpp
RequireString(Payload, TEXT("asset_path"));
RequireString(Payload, TEXT("widget_name"));
OptionalBool(Payload, TEXT("dry_run"));
```

### 16.5 read tools

```cpp
RequireString(Payload, TEXT("asset_path"));
OptionalString(Payload, TEXT("widget_name")); // property/slot required
OptionalArray(Payload, TEXT("property_paths"));
```

### 16.6 权限

写工具：

```text
add_widget_to_tree
set_widget_property
set_widget_properties
set_widget_slot_property
set_widget_slot_properties
remove_widget_from_tree
```

需要写权限 / Token，并受 Safety Profile 约束。

读工具：

```text
read_widget_tree
read_widget_properties
read_widget_slot_properties
```

只读，ReadOnly 下允许，不需要 write token。

---

## 17. 自动化测试计划

新增：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperWidgetContractTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperWidgetWriteTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperWidgetReadTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperWidgetRollbackTests.cpp
```

### 17.1 Contract tests

```text
1. add_widget_success_contract
   - data.schema=AddWidgetToTree.v1
   - added_count=1
   - 不返回 widget_ref/widget_path/parent_ref/write_ref

2. add_widget_reuse_no_op_contract
   - status=no_op
   - added_count=0
   - reused_existing=true

3. set_widget_property_success_contract
   - data.schema=SetWidgetProperty.v1
   - property_result counts only
   - 不返回 invalid_settings / before / after

4. set_widget_slot_property_success_contract
   - data.schema=SetWidgetSlotProperty.v1
   - property_result counts only

5. remove_widget_dry_run_passed_contract
   - status=dry_run
   - data.schema=RemoveWidgetFromTreeDryRun.v1
   - result/can_execute only

6. remove_widget_success_contract
   - data.schema=RemoveWidgetFromTree.v1
   - removed_count=1
   - 不返回 widget ref

7. read_widget_tree_contract
   - data.schema=ReadWidgetTree.v1
   - root/widgets[]/children only
   - 不返回 all_properties
```

### 17.2 Write tests

```text
1. add_textblock_to_canvas
2. add_button_to_vertical_box
3. add_widget_fail_if_name_exists
4. add_widget_reuse_if_exists_no_op
5. set_textblock_text
6. set_visibility_no_op
7. set_batch_property_transactional_failure
8. set_canvas_slot_position
9. set_slot_property_type_mismatch
10. remove_widget_requires_dry_run
11. remove_widget_success
12. remove_widget_blocked_if_reference_detected
```

### 17.3 Read tests

```text
1. read_widget_tree_returns_compressed_tree
2. read_widget_properties_explicit_paths
3. read_widget_slot_properties_explicit_paths
4. read_widget_properties_missing_widget_fails
5. read_slot_properties_without_slot_fails
```

### 17.4 Rollback tests

```text
1. add_widget_failure_rolls_back
2. property_batch_failure_no_partial_apply
3. slot_property_batch_failure_no_partial_apply
4. remove_failure_rolls_back
5. rollback_failed_sets_modified_true
```

---

## 18. 推荐提交顺序

### Commit 1：类型与序列化

```text
Add Widget DTOs and schemas
Add widget scope / error / stage enums
Add property_result and read tree result serialization
```

验收：

```text
所有 UMG data.schema 短命名。
成功 DTO 不包含 write_ref / widget_ref。
```

### Commit 2：WidgetBlueprint resolve

```text
Add WidgetService skeleton
Resolve UWidgetBlueprint
Resolve WidgetTree
FindWidgetByName helper
```

验收：

```text
asset_not_found / target_not_widget_blueprint 正确失败。
```

### Commit 3：read_widget_tree

```text
Implement compressed WidgetTree read
Return root/widgets/children only
```

验收：

```text
不返回属性、Slot、Binding、Animation。
```

### Commit 4：add_widget_to_tree

```text
Implement widget class resolve
Implement name_collision fail/reuse
Create widget and insert into parent panel
Return added_count only
```

验收：

```text
fail_if_exists / reuse_if_exists 行为正确。
成功不返回 widget handle。
```

### Commit 5：普通属性写入

```text
Implement set_widget_property/properties
Reuse PropertyReflectionService
Make batch transactional
Return property_result counts
```

验收：

```text
invalid 时整批失败。
成功不返回 invalid_settings。
```

### Commit 6：Slot 属性写入

```text
Implement set_widget_slot_property/properties
Resolve UPanelSlot
Validate slot type
Return property_result counts
```

验收：

```text
slot_type_mismatch 正确进入 error.conflicts。
```

### Commit 7：remove_widget_from_tree

```text
Implement dry_run
Implement reference checks
Implement remove and rollback
Return removed_count only
```

验收：

```text
remove 必须支持 dry_run。
blocked/failed 才返回 widget_name。
```

### Commit 8：read properties

```text
Implement read_widget_properties
Implement read_widget_slot_properties
Require explicit property paths
```

验收：

```text
属性读取与 WidgetTree 读取分离。
```

### Commit 9：Bridge / Validator / Auth

```text
Register UMG commands
Add request validators
Classify read/write permissions
Add tests
```

验收：

```text
写工具需要 token。
读工具 ReadOnly 可用。
```

### Commit 10：Protocol regression

```text
Add contract tests preventing write_ref/transaction_id/review/safety in UMG success
Add contract tests preserving validation for UMG write success
```

验收：

```text
字段稿验收项全部通过。
```

---

## 19. 第一版不做的内容

```text
1. 不支持 Widget Animation。
2. 不支持 Binding Graph。
3. 不支持 UMG 事件绑定图表。
4. 不支持 Slate Brush 大资源批量处理。
5. 不支持 auto_rename。
6. 不支持 replace_existing。
7. 不返回 widget_ref / widget_path。
8. 不返回 all_properties。
9. 不返回 write_ref / transaction_id。
10. 不自动 compile/save。
11. 不做 Graph Write 逻辑绑定。
```

---

## 20. 最小验收标准

add 成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_widget_to_tree",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/UI/WBP_MainMenu",
    "widget_scope": "widget_tree"
  },
  "data": {
    "schema": "AddWidgetToTree.v1",
    "add_widget_result": {
      "added_count": 1
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

property 成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_widget_properties",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/UI/WBP_MainMenu",
    "widget_scope": "widget_property"
  },
  "data": {
    "schema": "SetWidgetProperty.v1",
    "property_result": {
      "mode": "batch",
      "requested_count": 3,
      "applied_count": 3,
      "changed_count": 2,
      "no_op_count": 1
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

remove dry_run：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_widget_from_tree",
  "status": "dry_run",
  "modified": false,
  "target": {
    "asset_path": "/Game/UI/WBP_MainMenu",
    "widget_scope": "widget_tree"
  },
  "data": {
    "schema": "RemoveWidgetFromTreeDryRun.v1",
    "dry_run": {
      "result": "passed",
      "can_execute": true
    }
  }
}
```

read tree：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_widget_tree",
  "status": "completed",
  "modified": false,
  "data": {
    "schema": "ReadWidgetTree.v1",
    "widget_tree": {
      "root": "CanvasRoot",
      "widgets": [
        {
          "widget_name": "CanvasRoot",
          "widget_class": "CanvasPanel",
          "children": ["TitleText", "StartButton"]
        }
      ]
    }
  }
}
```

必须不出现：

```text
write_ref
transaction_id
review
safety
widget_ref
widget_path
parent_ref
slot_ref
all_properties
before
after
```

---

## 21. 实现风险

### 21.1 UMG Editor API 版本差异

风险：

```text
UWidgetBlueprint / WidgetTree API 在 UE 5.3+ 细节差异。
```

处理：

```text
封装 WidgetBlueprintCompat helper。
优先使用 UWidgetTree 公共 API。
```

### 21.2 AddWidget 时误设置 Slot 属性

风险：

```text
AddChild 返回 Slot 后顺手设置 Position/Padding，破坏工具边界。
```

处理：

```text
AddWidgetToTreeService 禁止解析 slot property 字段。
Slot 属性只在 SlotPropertyService 中处理。
```

### 21.3 批量属性部分成功

风险：

```text
逐项写入时中途失败，留下半配置。
```

处理：

```text
预校验全部 items。
写入失败时 rollback snapshot。
Contract test 覆盖 invalid 时 applied_count 不返回。
```

### 21.4 read_widget_properties 返回过多属性

风险：

```text
默认反射整个 Widget，Token 暴涨。
```

处理：

```text
第一版要求明确 property_paths。
read_widget_tree 只返回压缩树。
```

### 21.5 remove_widget 引用检查不足

风险：

```text
删除 Widget 后 Binding/Graph 引用断裂。
```

处理：

```text
无法确认引用安全时 dry_run blocked。
后续再补引用追踪能力。
```
