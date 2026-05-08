# BlueprintHelper Asset Discovery / Editor Navigation UE 侧 C++ 可执行实现计划

状态：[x] 已完成
日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置实现  
来源字段稿：`BlueprintHelper_AssetDiscovery_EditorNavigation_UE_FieldMapping_20260503.md`  
实现范围：UE 插件侧 C++  
不包含：MCP Server TypeScript 封装、Agent Skill 文档、资产创建、资产修改、Graph Logic 读取、依赖/引用完整查询、Review / Journal 写入

---

## 0. 实现目标

实现 Asset Discovery / Editor Navigation 第一版工具簇：

```text
find_assets
read_asset_summary
open_asset_in_editor
get_editor_context
```

该工具簇负责：

```text
查找资产
读取资产轻量摘要
在 UE 编辑器中打开 / 聚焦资产
读取当前编辑器上下文
```

不负责：

```text
创建资产
修改资产
保存资产
编译资产
写 Transaction Journal
写 Review
推断写入目标
读取蓝图图表逻辑
读取 UMG WidgetTree
读取 DataTable 行内容
读取依赖 / referencer 全量图
```

字段契约核心点：

```text
1. 所有工具默认 modified=false。
2. 所有工具不返回 validation / write_ref / transaction_id / review / safety。
3. find_assets 返回 assets[]，每项只包含 asset_path / asset_type / asset_class。
4. find_assets 不返回 asset_name / package_path / object_path。
5. find_assets 空结果 status=completed / assets=[]，不是失败。
6. find_assets 可在 assets[].asset_path 中使用 %{path_filter} 压缩前缀。
7. %{path_filter} 只限 find_assets 列表型结果。
8. 单资产工具必须返回完整 asset_path。
9. open_asset_in_editor 是 Editor UI 操作，modified=false。
10. get_editor_context 只读；Agent 不得把当前焦点或选中资产作为写入目标事实来源。
11. 所有 data.schema 使用短命名。
```

---

## 1. UE 模块依赖

确认 `BlueprintHelper.Build.cs` 至少包含：

```text
Core
CoreUObject
Engine
UnrealEd
AssetRegistry
ContentBrowser
EditorSubsystem
ToolMenus（如已有 editor integration，可选）
Kismet（如已有 Blueprint editor opening，可选）
```

打开资产通常需要：

```cpp
#include "AssetRegistry/AssetRegistryModule.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
```

如果当前插件已经有 AssetBrowse / EditorNavigation 服务，应改造现有服务返回字段，不新增平行实现。

---

## 2. Phase A：新增类型文件

### 2.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperAssetDiscoveryTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperAssetDiscoveryTypes.cpp
```

### 2.2 新增枚举

```cpp
enum class EBlueprintHelperAssetQueryScope : uint8
{
    AssetRegistry
};

enum class EBlueprintHelperAssetReadScope : uint8
{
    AssetSummary
};

enum class EBlueprintHelperEditorScope : uint8
{
    AssetEditor
};

enum class EBlueprintHelperEditorReadScope : uint8
{
    EditorContext
};

enum class EBlueprintHelperAssetDiscoveryStage : uint8
{
    ParseInput,
    QueryAssetRegistry,
    ResolveAsset,
    ReadAssetSummary,
    OpenAssetEditor,
    ReadEditorContext,
    BuildPage
};

enum class EBlueprintHelperAssetDiscoveryErrorCode : uint8
{
    InvalidRequest,
    AssetRegistryUnavailable,
    AssetNotFound,
    AssetLoadFailed,
    OpenAssetFailed,
    EditorSubsystemUnavailable,
    ContentBrowserUnavailable,
    CursorInvalid,
    InternalError
};
```

### 2.3 字符串序列化

稳定输出：

```text
asset_registry
asset_summary
asset_editor
editor_context
asset_registry_unavailable
asset_not_found
open_asset_failed
```

不要输出 C++ enum 原名。

---

## 3. Phase B：Agent-facing DTO

### 3.1 find_assets DTO

```cpp
struct FBlueprintHelperFindAssetsResultData
{
    FString Schema = TEXT("FindAssets.v1");
    TArray<FBlueprintHelperAssetListItem> Assets;
    FBlueprintHelperPageInfo Page;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperAssetListItem
{
    FString AssetPath;   // find_assets 中可使用 %{path_filter}
    FString AssetType;
    FString AssetClass;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperPageInfo
{
    int32 Limit = 20;
    bool bHasMore = false;
    TOptional<FString> NextCursor;

    TSharedRef<FJsonObject> ToJson() const;
};
```

成功：

```json
{
  "schema": "FindAssets.v1",
  "assets": [
    {
      "asset_path": "%{path_filter}/Door/BP_BH_PhysicsDoor",
      "asset_type": "Blueprint",
      "asset_class": "/Script/Engine.Blueprint"
    }
  ],
  "page": {
    "limit": 20,
    "has_more": false
  }
}
```

### 3.2 read_asset_summary DTO

```cpp
struct FBlueprintHelperReadAssetSummaryResultData
{
    FString Schema = TEXT("ReadAssetSummary.v1");
    FBlueprintHelperAssetSummary Asset;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperAssetSummary
{
    FString AssetPath;   // 必须完整路径，不压缩
    FString AssetType;
    FString AssetClass;
    bool bLoaded = false;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.3 open_asset_in_editor DTO

```cpp
struct FBlueprintHelperOpenAssetInEditorResultData
{
    FString Schema = TEXT("OpenAssetInEditor.v1");
    FBlueprintHelperOpenAssetResult OpenResult;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperOpenAssetResult
{
    bool bOpened = false;
    bool bAlreadyOpen = false;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.4 get_editor_context DTO

```cpp
struct FBlueprintHelperGetEditorContextResultData
{
    FString Schema = TEXT("GetEditorContext.v1");
    FBlueprintHelperEditorContext EditorContext;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperEditorContext
{
    TArray<FString> OpenAssets;
    TArray<FString> SelectedAssets;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.5 明确禁止字段

所有 DTO 均不包含：

```cpp
FBlueprintHelperWriteRef
FString TransactionId
FBlueprintHelperValidationResult
FString PackagePath
FString ObjectPath
FString AssetName
TArray<FString> Dependencies
TArray<FString> Referencers
```

---

## 4. Phase C：AssetDiscoveryService

### 4.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperAssetDiscoveryService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperAssetDiscoveryService.cpp
```

### 4.2 服务接口

```cpp
class FBlueprintHelperAssetDiscoveryService
{
public:
    FBlueprintHelperToolResultBase FindAssets(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperToolResultBase ReadAssetSummary(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperToolResultBase OpenAssetInEditor(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperToolResultBase GetEditorContext(const TSharedPtr<FJsonObject>& Payload) const;

private:
    bool GetAssetRegistry(IAssetRegistry& OutRegistry, FBlueprintHelperToolError& OutError) const;

    bool ResolveAssetData(
        const FString& AssetPath,
        FAssetData& OutAssetData,
        FBlueprintHelperToolError& OutError) const;

    FString BuildAssetType(const FAssetData& AssetData) const;
    FString BuildAssetClassPath(const FAssetData& AssetData) const;
};
```

---

## 5. Phase D：find_assets

### 5.1 Request 结构

```cpp
struct FBlueprintHelperFindAssetsRequest
{
    FString QueryScope = TEXT("asset_registry");
    TOptional<FString> PathFilter;
    TOptional<FString> AssetTypeFilter;
    TOptional<FString> NameFilter;
    int32 Limit = 20;
    TOptional<FString> Cursor;
};
```

### 5.2 输入 JSON

```json
{
  "path_filter": "/Game/BlueprintHelperTest",
  "asset_type_filter": "Blueprint",
  "name_filter": "Door",
  "limit": 20
}
```

`query_scope` 可选，默认：

```text
asset_registry
```

### 5.3 Asset Registry 查询

```cpp
FARFilter Filter;
Filter.bRecursivePaths = true;

if (Request.PathFilter.IsSet())
{
    Filter.PackagePaths.Add(FName(*Request.PathFilter.GetValue()));
}

if (Request.AssetTypeFilter.IsSet())
{
    // 方式一：ClassPaths。
    Filter.ClassPaths.Add(ResolveClassPath(Request.AssetTypeFilter.GetValue()));

    // 方式二：先查全部再按 AssetType 过滤。
}
```

UE5.3 中 AssetRegistry class 字段使用 `FTopLevelAssetPath`。建议封装：

```cpp
bool ResolveAssetClassFilter(
    const FString& AssetTypeFilter,
    TArray<FTopLevelAssetPath>& OutClassPaths) const;
```

常见映射：

```text
Blueprint -> /Script/Engine.Blueprint
WidgetBlueprint -> /Script/UMGEditor.WidgetBlueprint
DataTable -> /Script/Engine.DataTable
DataAsset -> /Script/Engine.DataAsset 或子类后置过滤
BlueprintInterface -> /Script/Engine.Blueprint
InputAction -> /Script/EnhancedInput.InputAction
```

### 5.4 NameFilter

NameFilter 不返回 asset_name，但可用于过滤：

```cpp
if (Request.NameFilter.IsSet())
{
    AssetData.AssetName.ToString().Contains(Request.NameFilter.GetValue())
}
```

不要在结果中输出 AssetName。

### 5.5 AssetPath 构建

结果 `asset_path` 使用 Unreal long package/object path 的资产路径格式，推荐：

```cpp
FString FullAssetPath = AssetData.GetSoftObjectPath().ToString();
```

但字段示例使用：

```text
/Game/Folder/BP_Name
```

而不是：

```text
/Game/Folder/BP_Name.BP_Name
```

必须统一本项目资产路径规范。建议：

```cpp
FString FullAssetPath = AssetData.PackageName.ToString();
```

因为后续工具 target.asset_path 多使用 package-style 路径：

```text
/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor
```

若后续 LoadObject 需要 object path，再由 resolver 内部补 `.AssetName`。

### 5.6 path_filter 压缩

仅在 `find_assets` 列表结果中允许：

```cpp
FString MaybeCompressPathFilterPrefix(
    const FString& FullAssetPath,
    const TOptional<FString>& PathFilter)
{
    if (PathFilter.IsSet() && FullAssetPath.StartsWith(PathFilter.GetValue()))
    {
        FString Suffix = FullAssetPath.RightChop(PathFilter.GetValue().Len());
        return TEXT("%{path_filter}") + Suffix;
    }
    return FullAssetPath;
}
```

约束：

```text
1. 只使用 %{path_filter}。
2. 不使用 %{filter}。
3. 只压缩 find_assets 的 assets[].asset_path。
4. read_asset_summary / open_asset_in_editor / compile / save / 写工具不使用压缩路径。
5. Agent 后续使用前必须展开完整路径。
```

### 5.7 分页

```cpp
int32 Offset = DecodeCursor(Request.Cursor).Get(0);
int32 Limit = FMath::Clamp(Request.Limit, 1, 100);
```

排序建议：

```text
asset_path asc
```

返回：

```text
page.limit = Limit
page.has_more = Offset + Limit < Filtered.Num()
page.next_cursor = has_more ? EncodeCursor(Offset + Limit) : unset
```

### 5.8 空结果

空结果仍返回：

```text
ok=true
status=completed
modified=false
assets=[]
```

不是失败。

---

## 6. Phase E：read_asset_summary

### 6.1 Request

```cpp
struct FBlueprintHelperReadAssetSummaryRequest
{
    FString AssetPath;
};
```

### 6.2 输入路径

`read_asset_summary` 必须要求完整 asset_path。  
如果收到 `%{path_filter}` 压缩路径：

```text
error.code=invalid_request
stage=parse_input
message=read_asset_summary requires a full asset_path.
```

不要在单资产工具内部猜测 path_filter。

### 6.3 实现

```cpp
FAssetData AssetData;
ResolveAssetData(Request.AssetPath, AssetData, OutError);

FBlueprintHelperAssetSummary Summary;
Summary.AssetPath = Request.AssetPath;
Summary.AssetType = BuildAssetType(AssetData);
Summary.AssetClass = BuildAssetClassPath(AssetData);
Summary.bLoaded = AssetData.IsAssetLoaded();
```

### 6.4 不加载资产

`read_asset_summary` 应优先只读 AssetRegistry，不主动加载资产。  

```cpp
AssetData.IsAssetLoaded()
```

或尝试 `FindObject`，但不要 `GetAsset()` 强制加载，除非当前实现必须加载。字段名是 `loaded`，不是 `loaded_after_call`。

### 6.5 成功返回

```json
{
  "schema": "ReadAssetSummary.v1",
  "asset": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "asset_type": "Blueprint",
    "asset_class": "/Script/Engine.Blueprint",
    "loaded": true
  }
}
```

不返回：

```text
generated_class
implemented_interfaces
class defaults
graph list
function list
dependencies
referencers
tags
package_path
asset_name
```

---

## 7. Phase F：open_asset_in_editor

### 7.1 Request

```cpp
struct FBlueprintHelperOpenAssetInEditorRequest
{
    FString AssetPath;
};
```

### 7.2 路径规则

必须使用完整 asset_path。  
不接受 `%{path_filter}` 压缩路径。

### 7.3 already_open 判断

使用 `UAssetEditorSubsystem`：

```cpp
UAssetEditorSubsystem* AssetEditorSubsystem =
    GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;

bool bAlreadyOpen = AssetEditorSubsystem->FindEditorForAsset(Asset, false) != nullptr;
```

或：

```cpp
IAssetEditorInstance* ExistingEditor = AssetEditorSubsystem->FindEditorForAsset(Asset, false);
```

### 7.4 打开资产

```cpp
bool bOpened = AssetEditorSubsystem->OpenEditorForAsset(Asset);
```

如果已打开：

```text
status=no_op
open_result.opened=false
open_result.already_open=true
modified=false
```

如果本次打开：

```text
status=completed
open_result.opened=true
open_result.already_open=false
modified=false
```

### 7.5 不写 validation / Journal

`open_asset_in_editor` 是 UI 操作：

```text
modified=false
不返回 validation
不返回 write_ref
不生成 transaction_id
不写 Journal
不写 Review
```

### 7.6 错误

常见错误：

```text
asset_not_found
editor_subsystem_unavailable
open_asset_failed
```

---

## 8. Phase G：get_editor_context

### 8.1 Request

无必填参数：

```json
{}
```

### 8.2 open_assets 读取

通过 `UAssetEditorSubsystem` 获取打开资产。

可选实现：

```cpp
TArray<UObject*> EditedAssets;
AssetEditorSubsystem->GetAllEditedAssets(EditedAssets);
```

再转换为 package asset path：

```cpp
FString AssetPath = EditedAsset->GetOutermost()->GetName();
```

### 8.3 selected_assets 读取

通过 Content Browser：

```cpp
FContentBrowserModule& ContentBrowserModule =
    FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

TArray<FAssetData> SelectedAssets;
ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);
```

转换：

```cpp
SelectedAsset.PackageName.ToString()
```

### 8.4 成功返回

```json
{
  "schema": "GetEditorContext.v1",
  "editor_context": {
    "open_assets": [
      "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
    ],
    "selected_assets": [
      "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
    ]
  }
}
```

### 8.5 安全规则

`get_editor_context` 只是只读诊断 / 辅助 UI context。

Agent 不得用：

```text
open_assets
selected_assets
current editor focus
```

作为写入目标事实来源。

写工具仍必须显式指定：

```text
asset_path
graph / function / event / target
```

---

## 9. Phase H：ToolResult 构建

### 9.1 find_assets success

```cpp
FBlueprintHelperToolResultBase Result;
Result.bOk = true;
Result.Operation = TEXT("find_assets");
Result.Status = TEXT("completed");
Result.bModified = false;
Result.Target = MakeFindAssetsTarget(Request);
Result.Data = Data.ToJson();
```

### 9.2 read_asset_summary success

```cpp
Result.Operation = TEXT("read_asset_summary");
Result.Status = TEXT("completed");
Result.bModified = false;
Result.Target = {
    asset_path,
    read_scope = asset_summary
};
```

### 9.3 open_asset_in_editor no_op

```cpp
Result.Operation = TEXT("open_asset_in_editor");
Result.Status = TEXT("no_op");
Result.bModified = false;
Data.OpenResult.bOpened = false;
Data.OpenResult.bAlreadyOpen = true;
```

### 9.4 get_editor_context success

```cpp
Result.Operation = TEXT("get_editor_context");
Result.Status = TEXT("completed");
Result.bModified = false;
Result.Target = {
    read_scope = editor_context
};
```

### 9.5 Failure

```cpp
Result.bOk = false;
Result.Status = TEXT("failed");
Result.bModified = false;
Result.Error = MakeAssetDiscoveryError(...);
```

---

## 10. Phase I：Bridge Router 接入

### 10.1 新增 commands

```text
find_assets
read_asset_summary
open_asset_in_editor
get_editor_context
```

### 10.2 Router 分支

```cpp
if (Request.Command == TEXT("find_assets"))
{
    return HandleFindAssets(Request);
}
if (Request.Command == TEXT("read_asset_summary"))
{
    return HandleReadAssetSummary(Request);
}
if (Request.Command == TEXT("open_asset_in_editor"))
{
    return HandleOpenAssetInEditor(Request);
}
if (Request.Command == TEXT("get_editor_context"))
{
    return HandleGetEditorContext(Request);
}
```

### 10.3 Handler 模板

```cpp
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleFindAssets(
    const FBlueprintHelperBridgeRequest& Req) const
{
    FBlueprintHelperToolResultBase Result =
        AssetDiscoveryService.FindAssets(Req.Payload);

    FBlueprintHelperBridgeResponse Resp = Result.bOk
        ? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
        : FBlueprintHelperBridgeResponse::Error(
            Req.RequestId,
            EBlueprintHelperBridgeError::ExecutionFailed,
            Result.Error.IsSet() ? Result.Error->Message : TEXT("find_assets failed"));

    Resp.Result = Result.ToJson();
    return Resp;
}
```

---

## 11. Phase J：RequestValidator / 权限

### 11.1 find_assets

```cpp
OptionalString(Payload, TEXT("query_scope"));
OptionalString(Payload, TEXT("path_filter"));
OptionalString(Payload, TEXT("asset_type_filter"));
OptionalString(Payload, TEXT("name_filter"));
OptionalInt(Payload, TEXT("limit"));
OptionalString(Payload, TEXT("cursor"));
```

### 11.2 read_asset_summary

```cpp
RequireString(Payload, TEXT("asset_path"));
RejectCompressedPath(Payload, TEXT("asset_path"));
```

### 11.3 open_asset_in_editor

```cpp
RequireString(Payload, TEXT("asset_path"));
RejectCompressedPath(Payload, TEXT("asset_path"));
```

### 11.4 get_editor_context

```cpp
// Empty payload allowed.
```

### 11.5 权限

所有工具只读 / UI 操作：

```text
不需要 write token
不生成 transaction
不写 Journal
ReadOnly 下允许
modified=false
```

`open_asset_in_editor` 是 UI 操作，不是资产写操作。可在 ReadOnly 下允许。

---

## 12. Phase K：路径工具

### 12.1 新增路径 helper

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperAssetPathUtils.h
Source/BlueprintHelper/Private/Services/BlueprintHelperAssetPathUtils.cpp
```

接口：

```cpp
class FBlueprintHelperAssetPathUtils
{
public:
    static bool IsCompressedPath(const FString& Path);
    static FString CompressWithPathFilter(const FString& FullPath, const FString& PathFilter);
    static bool RejectCompressedPathForSingleAssetTool(
        const FString& Path,
        FBlueprintHelperToolError& OutError);

    static FString PackagePathFromAssetData(const FAssetData& AssetData);
    static FString ObjectPathFromPackagePathIfNeeded(const FString& PackagePath);
};
```

### 12.2 压缩格式

唯一合法 alias：

```text
%{path_filter}
```

非法：

```text
%{filter}
%{asset_path}
%{root}
```

### 12.3 使用范围

允许：

```text
find_assets data.assets[].asset_path
```

禁止：

```text
find_assets target.path_filter
read_asset_summary target.asset_path
open_asset_in_editor target.asset_path
compile_blueprint_asset target.asset_path
save_asset target.asset_path
任何写工具 target.asset_path
```

---

## 13. Phase L：AssetType / AssetClass 规则

### 13.1 asset_class

优先输出 ClassPath：

```cpp
AssetData.AssetClassPath.ToString()
```

UE5.3 可能得到：

```text
/Script/Engine.Blueprint
/Script/UMGEditor.WidgetBlueprint
/Script/Engine.DataTable
```

### 13.2 asset_type

asset_type 是给 Agent 的短类型：

```text
Blueprint
WidgetBlueprint
DataTable
DataAsset
BlueprintInterface
InputAction
InputMappingContext
Material
Texture
SoundCue
```

实现：

```cpp
FString BuildAssetType(const FAssetData& AssetData)
{
    if (AssetData.AssetClassPath == UBlueprint::StaticClass()->GetClassPathName())
    {
        // Blueprint / BlueprintInterface 需要根据 tags 或 GeneratedClass 后续判断。
        return TEXT("Blueprint");
    }
    return AssetData.AssetClassPath.GetAssetName().ToString();
}
```

第一版不强行完美区分所有 Blueprint 子类型；只需稳定、简短、可过滤。

### 13.3 不返回 asset_name

即使内部使用：

```cpp
AssetData.AssetName
```

结果也不返回 asset_name。

---

## 14. 自动化测试计划

新增：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperAssetDiscoveryContractTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperAssetDiscoveryRuntimeTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperEditorNavigationTests.cpp
```

### 14.1 Contract tests

```text
1. find_assets_success_contract
   - operation=find_assets
   - data.schema=FindAssets.v1
   - assets[].asset_path / asset_type / asset_class
   - 不返回 asset_name / package_path / object_path
   - modified=false
   - 不返回 validation / write_ref

2. find_assets_empty_contract
   - ok=true
   - status=completed
   - assets=[]
   - page.has_more=false

3. find_assets_path_filter_compression_contract
   - target.path_filter=/Game/Test
   - result asset_path=%{path_filter}/...

4. read_asset_summary_contract
   - data.schema=ReadAssetSummary.v1
   - asset.asset_path 是完整路径
   - asset.loaded 存在
   - 不返回 generated_class / graph list / dependencies

5. open_asset_in_editor_contract
   - data.schema=OpenAssetInEditor.v1
   - open_result.opened/already_open
   - modified=false
   - 不返回 validation

6. get_editor_context_contract
   - data.schema=GetEditorContext.v1
   - open_assets / selected_assets arrays
```

### 14.2 Runtime tests

```text
1. find_blueprints_by_path
2. find_assets_by_name_filter
3. find_assets_paginates
4. read_summary_existing_asset
5. read_summary_missing_asset_fails
6. open_asset_success
7. open_asset_already_open_no_op
8. get_editor_context_returns_arrays
9. single_asset_tools_reject_compressed_path
```

---

## 15. 推荐提交顺序

### Commit 1：DTO 与路径工具

```text
Add Asset Discovery DTOs
Add short schemas
Add path_filter compression utility
Add contract serialization tests
```

验收：

```text
find_assets 可输出 %{path_filter}。
单资产 DTO 不压缩。
```

### Commit 2：AssetRegistry 查询

```text
Implement AssetDiscoveryService.FindAssets
Support path/name/type filters
Support pagination
Return empty results as completed
```

验收：

```text
assets[] 只含 asset_path / asset_type / asset_class。
```

### Commit 3：read_asset_summary

```text
Implement read_asset_summary
Use AssetRegistry without forced load where possible
Reject compressed paths
Return loaded flag
```

验收：

```text
不返回 graph/class/dependency 细节。
```

### Commit 4：open_asset_in_editor

```text
Implement open_asset_in_editor
Use UAssetEditorSubsystem
Detect already_open
Return completed/no_op
```

验收：

```text
modified=false。
不返回 validation/write_ref。
```

### Commit 5：get_editor_context

```text
Implement get_editor_context
Collect open assets
Collect selected assets
Return arrays only
```

验收：

```text
只读，ReadOnly 可调用。
```

### Commit 6：Bridge / Validator / Auth

```text
Register commands
Add validators
Classify all commands read-only or UI-only
Reject compressed path in single-asset tools
```

验收：

```text
不需要 write token。
```

### Commit 7：Protocol regression

```text
Add tests preventing asset_name/package_path/object_path leaks
Add tests preventing validation/write_ref/transaction_id
Add tests for path compression boundaries
```

验收：

```text
字段稿验收项全部通过。
```

---

## 16. 第一版不做的内容

```text
1. 不创建资产。
2. 不修改资产。
3. 不保存资产。
4. 不编译资产。
5. 不读取蓝图图表逻辑。
6. 不读取 WidgetTree。
7. 不读取 DataTable 行内容。
8. 不返回 dependencies / referencers。
9. 不返回 generated_class / implemented_interfaces。
10. 不返回 package_path / object_path / asset_name。
11. 不写 Journal / Review。
12. 不用 editor focus 作为写入目标。
```

---

## 17. 最小验收标准

find_assets：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "find_assets",
  "status": "completed",
  "modified": false,
  "target": {
    "query_scope": "asset_registry",
    "path_filter": "/Game/BlueprintHelperTest",
    "asset_type_filter": "Blueprint"
  },
  "data": {
    "schema": "FindAssets.v1",
    "assets": [
      {
        "asset_path": "%{path_filter}/Door/BP_BH_PhysicsDoor",
        "asset_type": "Blueprint",
        "asset_class": "/Script/Engine.Blueprint"
      }
    ],
    "page": {
      "limit": 20,
      "has_more": false
    }
  }
}
```

read_asset_summary：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_asset_summary",
  "status": "completed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "read_scope": "asset_summary"
  },
  "data": {
    "schema": "ReadAssetSummary.v1",
    "asset": {
      "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
      "asset_type": "Blueprint",
      "asset_class": "/Script/Engine.Blueprint",
      "loaded": true
    }
  }
}
```

open_asset_in_editor：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "open_asset_in_editor",
  "status": "completed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "editor_scope": "asset_editor"
  },
  "data": {
    "schema": "OpenAssetInEditor.v1",
    "open_result": {
      "opened": true,
      "already_open": false
    }
  }
}
```

get_editor_context：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "get_editor_context",
  "status": "completed",
  "modified": false,
  "target": {
    "read_scope": "editor_context"
  },
  "data": {
    "schema": "GetEditorContext.v1",
    "editor_context": {
      "open_assets": [],
      "selected_assets": []
    }
  }
}
```

必须不出现：

```text
validation
write_ref
transaction_id
journal_recorded
review
safety
asset_name
package_path
object_path
generated_class
graph list
dependencies
referencers
```

---

## 18. 实现风险

### 18.1 AssetPath 规范混乱

风险：

```text
AssetRegistry 返回 /Game/A/B.B，写工具期望 /Game/A/B。
```

处理：

```text
统一 Agent-facing asset_path 使用 PackageName。
内部 resolver 负责转换为 object path。
Contract test 锁定。
```

### 18.2 path_filter alias 被误用于写工具

风险：

```text
Agent 把 %{path_filter}/... 直接传给写工具。
```

处理：

```text
所有单资产工具 Validator 拒绝压缩路径。
错误提示要求展开完整路径。
```

### 18.3 open_asset_in_editor 被误判为 modified

风险：

```text
```

处理：

```text
modified=false 固定。
不写 Journal。
```

### 18.4 get_editor_context 被误用为写目标

风险：

```text
Agent 根据 selected_assets 直接写入当前选中资产。
```

处理：

```text
Skill/Agent 规则禁止。
UE 工具层仍要求写工具显式 asset_path。
本工具只返回 context，不提供 write target confirmation。
```

### 18.5 AssetType 过滤不精确

风险：

```text
Blueprint / WidgetBlueprint / BlueprintInterface 分类不完全准确。
```

处理：

```text
第一版先按 AssetClassPath + 标签做基本分类。
后续增加专用分类 helper。
不要返回误导性内部 tags。
```
