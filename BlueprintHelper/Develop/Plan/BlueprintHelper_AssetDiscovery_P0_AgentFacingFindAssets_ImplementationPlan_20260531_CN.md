# BlueprintHelper AssetDiscovery P0 Agent-facing FindAssets Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增默认 Agent 可用的只读 `blueprinthelper_find_assets`，让未知 `asset_path` 的 UE 资产任务先通过正式 `AssetDiscovery` 边界获得候选路径，再进入 `read_context -> preview -> execute` 主流程。

**Architecture:** 新建独立 `FBlueprintHelperAssetDiscoveryService`，从旧 `AssetBrowseService` 抽取架构中立的 AssetRegistry 查询逻辑，但不复用其混合职责。P0 保持当前 Bridge GameThread 调度模型，只执行有限的 `limit + 1` AssetRegistry 枚举并返回 `FindAssets.v1`；事件驱动快照和非 GameThread read lane 明确留给 P1。

**Tech Stack:** Unreal Engine 5.6 C++ Editor Plugin、UE AssetRegistry、Bridge Router、TypeScript + Zod、AgentFaceService CLI、Node test runner、UE Automation。

**Authoritative decision:** `BlueprintHelper/Develop/Gap/BlueprintHelper_AssetDiscovery_AgentFacingCapability_DecisionAndImplementationPlan_20260531_CN.md`

**Project rule override:** 本仓库 `AGENT.md` 禁止 Agent 执行 `git add`、`git commit`、`git push`。本计划中的每个 checkpoint 只记录建议提交范围，不自动提交。

---

## 0. P0 Scope Guard

P0 必须完成：

- 新增默认 Agent 工具 `blueprinthelper_find_assets`。
- 新增独立 UE `AssetDiscoveryService`。
- 新增 Bridge command `find_assets` 和 route cluster `AssetDiscovery`。
- 使用 AssetRegistry UE object path，不扫描磁盘 `.uasset`。
- 使用完整 class path 和集中式 semantic type 映射。
- 只返回有限页：`assets[] + page.limit + page.has_more`。
- `limit` 限制为 `1..100`，内部最多枚举到 `limit + 1`。
- 不返回精确 `total_count`。
- 不接受 cursor；cursor、稳定分页、快照、并行 read lane 属于 P1。
- 删除旧 `blueprint_list_assets` / `blueprint_search_assets` frozen direct 注册和 dead map。
- 删除旧 `AssetBrowseService::ListAssets` / `SearchAssets`、Bridge route 与 validator 分支，不保留兼容适配。

P0 不做：

- Bridge 并行调度器改造。
- 非 GameThread AssetRegistry 查询。
- AssetRegistry 生命周期事件订阅。
- cursor 编码。
- Content Browser UI 自动化。
- 打开、保存、编译资产。
- GraphWrite statement 扩展。

---

## 1. File Structure

### New UE files

| File | Responsibility |
|---|---|
| `BlueprintHelper/Source/BlueprintHelper/Public/Shared/AssetDiscovery/BlueprintHelperAssetDiscoveryTypes.h` | `FindAssetsRequest.v1`、`FindAssets.v1`、错误码、有限页 DTO。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/AssetDiscovery/BlueprintHelperAssetDiscoveryService.h` | 独立只读服务接口。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/AssetDiscovery/BlueprintHelperAssetDiscoveryService.cpp` | AssetRegistry 过滤、完整 class path 解析、semantic type 映射、`limit + 1` 停止。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/Routes/BlueprintHelperAssetDiscoveryBridgeRoutes.h` | 独立 AssetDiscovery route 适配器接口。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Routes/BlueprintHelperAssetDiscoveryBridgeRoutes.cpp` | `find_assets` 解析、调用 service、序列化 `FindAssets.v1`。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Tests/AssetDiscovery/BlueprintHelperAssetDiscoveryServiceTests.cpp` | UE 自动化覆盖查询、范围、类型、截断和输出边界。 |

### Modified UE files

| File | Responsibility |
|---|---|
| `BlueprintHelper/Source/BlueprintHelper/Public/Shared/Debug/BlueprintHelperAssetDiscoveryTypes.h` | 删除；未接线草案迁移到正式 Shared/AssetDiscovery 边界。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Debug/BlueprintHelperAssetBrowseService.h` | 删除旧搜索 DTO 与方法；打开、保存、详情仍留在 legacy 边界。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Debug/BlueprintHelperAssetBrowseService.cpp` | 删除旧 `ListAssets` / `SearchAssets` 算法；保留非搜索职责。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/Entry/BlueprintHelper.h` | 模块持有正式 `AssetDiscoveryService`。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Entry/BlueprintHelper.cpp` | 构造服务并注入 Bridge Router。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/BlueprintHelperBridgeRoutePlanner.h` | 新增 `AssetDiscovery` cluster。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Utils/BlueprintHelperBridgeRoutePlannerUtils.cpp` | `find_assets -> AssetDiscovery` route plan。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/BlueprintHelperBridgeRouter.h` | 组合 `AssetDiscoveryRoutes`。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeRouter.cpp` | 注入 service 并分发到独立 route 适配器。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperRequestValidator.cpp` | 校验新命令根字段类型。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/BlueprintHelperToolClusterConfigResolver.cpp` | 将 `find_assets` 纳入只读输出限制。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Bridge/BlueprintHelperBridgeRoutePlannerTests.cpp` | 证明 cluster 和 P0 GameThread lane。 |

### New AgentFaceService files

| File | Responsibility |
|---|---|
| `AgentFaceService/task-core/src/tool-surface/bridge/asset-discovery-schema.ts` | Zod 输入协议。 |
| `AgentFaceService/task-core/src/tool-surface/bridge/asset-discovery-schema.test.ts` | schema 边界测试。 |
| `AgentFaceService/agent-guide/Templates/blueprinthelper_find_assets_template.json` | CLI 最小模板。 |

### Modified AgentFaceService files

| File | Responsibility |
|---|---|
| `AgentFaceService/task-core/src/tool-surface/registry/tool-metas.ts` | 默认工具元数据。 |
| `AgentFaceService/task-core/src/tool-surface/bridge/bridge-tool-schemas.ts` | 注册 Zod schema。 |
| `AgentFaceService/task-core/src/tool-surface/bridge/bridge-tool-command-map.ts` | 添加 `blueprinthelper_find_assets -> find_assets`，删除旧搜索 dead map。 |
| `AgentFaceService/task-core/src/tests/tool-surface/tool-registry.contract.test.ts` | 默认暴露与旧搜索入口移除契约。 |
| `AgentFaceService/mcp/src/mcp/tools/register-tools.ts` | 删除 suppressed frozen `blueprint_list_assets` / `blueprint_search_assets` direct 注册。 |
| `AgentFaceService/cli/src/cli/help.ts` | CLI help 和模板导航。 |
| `AgentFaceService/cli/src/tests/cli/cli-tool-bridge.test.ts` | help 与 Bridge command 测试。 |
| `AgentFaceService/docs/CLI_Tools_API_Reference.md` | CLI 命令、模板和输入形状参考，必须与 help/tool-metas/templates 同步。 |
| `AgentFaceService/agent-guide/Templates/SEMANTIC_INDEX.md` | 根模板索引。 |
| `AgentFaceService/agent-guide/Templates/INDEX.md` | category 描述。 |
| `AgentFaceService/agent-guide/00_Agent_Onboarding_Index.md` | 未知路径先发现规则。 |
| `AgentFaceService/agent-guide/Reference/02_TaskSpec_First_Tool_Selection.md` | 工具选择流程。 |
| `AgentFaceService/agent-guide/Reference/04_Tool_Surface_Field_Templates.md` | 请求字段契约。 |

### Plugin bundle sync

在 AgentFaceService 主文档验证完成后，同步：

- `CodexPlugin/skills/blueprint-helper/SKILL.md`
- `CodexPlugin/skills/blueprint-helper/references/00_Agent_Onboarding_Index_20260504.md`
- `CodexPlugin/skills/blueprint-helper/references/02_TaskSpec_First_Tool_Selection_20260504.md`
- `CodexPlugin/skills/blueprint-helper/references/04_Tool_Surface_Field_Templates_20260512.md`
- `ClaudePlugin/skills/blueprint-helper/SKILL.md`
- `ClaudePlugin/skills/blueprint-helper/references/00_Agent_Onboarding_Index_20260504.md`
- `ClaudePlugin/skills/blueprint-helper/references/02_TaskSpec_First_Tool_Selection_20260504.md`
- `ClaudePlugin/skills/blueprint-helper/references/04_Tool_Surface_Field_Templates_20260512.md`

---

## Task 1: Move The DTO Into The Formal AssetDiscovery Boundary

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Shared/AssetDiscovery/BlueprintHelperAssetDiscoveryTypes.h`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Public/Shared/Debug/BlueprintHelperAssetDiscoveryTypes.h`

- [ ] **Step 1: Create the formal DTO header**

Define the P0 request and response without cursor or total count:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FBlueprintHelperFindAssetsRequest
{
	FString Schema = TEXT("BlueprintHelper.FindAssetsRequest.v1");
	FString Query;
	TArray<FString> PathPrefixes;
	TArray<FString> AssetTypes;
	TArray<FString> AssetClasses;
	bool bRecursive = true;
	int32 Limit = 20;
	bool bIncludePluginContent = false;
	bool bIncludeEngineContent = false;
	bool bIncludeRedirectors = false;
};

struct FBlueprintHelperAssetListItem
{
	FString AssetPath;
	FString AssetType;
	FString AssetClass;

	TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperAssetPageInfo
{
	int32 Limit = 20;
	bool bHasMore = false;

	TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperFindAssetsResultData
{
	FString Schema = TEXT("FindAssets.v1");
	TArray<FBlueprintHelperAssetListItem> Assets;
	FBlueprintHelperAssetPageInfo Page;

	TSharedRef<FJsonObject> ToJson() const;
};
```

- [ ] **Step 2: Preserve only P0 response fields**

Keep the small pure-DTO `ToJson()` implementations inline in this header. Do not add a separate DTO `.cpp`.

`ToJson()` must write:

```text
schema
assets[].asset_path
assets[].asset_type
assets[].asset_class
page.limit
page.has_more
```

It must not write:

```text
path
name
class
disk_size
total_count
next_cursor
```

- [ ] **Step 3: Delete the unused Debug DTO header**

Remove the old `Public/Shared/Debug/BlueprintHelperAssetDiscoveryTypes.h` after the formal header exists. Do not leave duplicate DTO declarations.

- [ ] **Step 4: Verify no stale include or duplicate type remains**

Run:

```powershell
rg -n "Shared/Debug/BlueprintHelperAssetDiscoveryTypes|struct FBlueprintHelper(FindAssetsRequest|AssetListItem|AssetPageInfo|FindAssetsResultData)" BlueprintHelper\Source\BlueprintHelper
```

Expected:

```text
No stale Debug include remains. Only the formal Shared/AssetDiscovery header declares the P0 DTO structs.
```

**Checkpoint scope:** DTO move only.

---

## Task 2: Add The Independent Read-Only UE Service

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/AssetDiscovery/BlueprintHelperAssetDiscoveryService.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/AssetDiscovery/BlueprintHelperAssetDiscoveryService.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/AssetDiscovery/BlueprintHelperAssetDiscoveryServiceTests.cpp`

- [ ] **Step 1: Write failing UE automation tests**

Add tests for:

```text
BlueprintHelper.AssetDiscovery.FindAssets.DefaultScopeIsGame
BlueprintHelper.AssetDiscovery.FindAssets.QueryFiltersByName
BlueprintHelper.AssetDiscovery.FindAssets.SemanticTypeMapsToClassPath
BlueprintHelper.AssetDiscovery.FindAssets.FullClassPathFiltersExactly
BlueprintHelper.AssetDiscovery.FindAssets.ReturnsLimitAndHasMore
BlueprintHelper.AssetDiscovery.FindAssets.ExcludesRedirectorsByDefault
```

The service tests must assert that `FindAssets.v1` contains only UE object asset paths and compact metadata.

- [ ] **Step 2: Run the focused suite and confirm RED**

Run through the running UE editor:

```text
Automation RunTests BlueprintHelper.AssetDiscovery
```

Expected: FAIL because `FBlueprintHelperAssetDiscoveryService` does not exist.

- [ ] **Step 3: Define the service interface**

```cpp
#pragma once

#include "Shared/AssetDiscovery/BlueprintHelperAssetDiscoveryTypes.h"

struct FAssetData;
struct FTopLevelAssetPath;

struct BLUEPRINTHELPER_API FBlueprintHelperFindAssetsResult
{
	bool bSuccess = false;
	FString ErrorCode;
	FString ErrorMessage;
	FBlueprintHelperFindAssetsResultData Data;
};

class BLUEPRINTHELPER_API FBlueprintHelperAssetDiscoveryService
{
public:
	FBlueprintHelperFindAssetsResult FindAssets(
		const FBlueprintHelperFindAssetsRequest& Request) const;

private:
	static bool TryResolveAssetClassPath(
		const FString& ClassPath,
		FTopLevelAssetPath& OutClassPath);
	static bool TryResolveSemanticAssetType(
		const FString& AssetType,
		FTopLevelAssetPath& OutClassPath);
	static FString ResolveSemanticAssetType(const FAssetData& AssetData);
};
```

- [ ] **Step 4: Implement centralized semantic type mapping**

The service must recognize at least:

```text
blueprint          -> /Script/Engine.Blueprint
widget_blueprint   -> /Script/UMGEditor.WidgetBlueprint
data_table         -> /Script/Engine.DataTable
data_asset         -> /Script/Engine.DataAsset
user_defined_struct -> /Script/Engine.UserDefinedStruct
```

Unknown semantic types must fail with:

```text
invalid_asset_type
```

Do not guess a module from a short class name.

- [ ] **Step 5: Implement bounded AssetRegistry enumeration**

Build an `FARFilter` from:

```text
path_prefixes
asset_types
asset_classes
recursive
include_engine_content
include_plugin_content
include_redirectors
```

Set:

```cpp
Filter.bIncludeOnlyOnDiskAssets = true;
```

Use `IAssetRegistry::EnumerateAssets` with only-on-disk semantics. Stop as soon as the collected count exceeds `Limit`, remove the extra item, and set:

```cpp
Result.Data.Page.bHasMore = true;
```

Clamp `Limit` to `1..100`. Do not compute `total_count`.

- [ ] **Step 6: Keep P0 on GameThread**

Add a defensive check:

```cpp
checkf(IsInGameThread(), TEXT("P0 AssetDiscoveryService must run on GameThread."));
```

This is intentional. P1 will add an immutable snapshot read lane instead of moving UE API calls to workers.

- [ ] **Step 7: Run the focused suite and confirm GREEN**

Run:

```text
Automation RunTests BlueprintHelper.AssetDiscovery
```

Expected: all AssetDiscovery tests pass.

**Checkpoint scope:** formal DTO + standalone read-only service.

---

## Task 3: Remove Deprecated Legacy List/Search Entrypoints

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Debug/BlueprintHelperAssetBrowseService.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Debug/BlueprintHelperAssetBrowseService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/BlueprintHelperBridgeRouter.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeRouter.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperRequestValidator.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Utils/BlueprintHelperBridgeRoutePlannerUtils.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/BlueprintHelperToolClusterConfigResolver.cpp`
- Modify: `AgentFaceService/mcp/src/mcp/tools/register-tools.ts`
- Modify: `AgentFaceService/task-core/src/tool-surface/bridge/bridge-tool-command-map.ts`
- Modify: `AgentFaceService/task-core/src/tests/tool-surface/tool-registry.contract.test.ts`
- Modify: `AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix.md`

- [ ] **Step 1: Record the current stale entrypoint scan**

Run:

```powershell
rg -n 'blueprint_(list|search)_assets|TEXT\("(list_assets|search_assets)"\)|"(list_assets|search_assets)"|ListAssets\(|SearchAssets\(' BlueprintHelper\Source\BlueprintHelper AgentFaceService --glob '!**/build/**' --glob '!**/node_modules/**'
```

Expected before cleanup: only legacy MCP direct registrations, dead task-core mapping, legacy UE Bridge wiring,
legacy service declarations/implementations, config entries, tests, and documentation references.

- [ ] **Step 2: Remove frozen AgentFace entrypoints**

Delete:

```text
AgentFaceService/mcp/src/mcp/tools/register-tools.ts
  blueprint_list_assets
  blueprint_search_assets

AgentFaceService/task-core/src/tool-surface/bridge/bridge-tool-command-map.ts
  blueprint_list_assets -> list_assets
  blueprint_search_assets -> search_assets

AgentFaceService/task-core/src/tests/tool-surface/tool-registry.contract.test.ts
  frozenToolNames entries for blueprint_list_assets / blueprint_search_assets
```

Update `TaskSpec_UE_Editor_Capability_Matrix.md` so it no longer presents `list_assets` as a retained direct Bridge capability.

- [ ] **Step 3: Remove UE legacy search wiring and algorithms**

Delete:

```text
FBlueprintHelperListAssetsRequest
FBlueprintHelperListAssetsResult
FBlueprintHelperAssetBrowseService::ListAssets
FBlueprintHelperAssetBrowseService::SearchAssets
FBlueprintHelperBridgeRouter::HandleListAssets
FBlueprintHelperBridgeRouter::HandleSearchAssets
BLUEPRINTHELPER_ROUTE("list_assets", ...)
BLUEPRINTHELPER_ROUTE("search_assets", ...)
request validator branches for list_assets / search_assets
route planner entries for list_assets / search_assets
output limiter entries for list_assets / search_assets
```

- [ ] **Step 4: Preserve non-discovery legacy responsibilities unchanged**

Leave these existing methods in `AssetBrowseService` for their existing callers:

```text
OpenAsset
SaveAsset
GetAssetInfo
AssetDataToInfo
```

They do not become part of `AssetDiscoveryService`.

- [ ] **Step 5: Verify the deprecated search surface is gone**

Run:

```powershell
rg -n 'blueprint_(list|search)_assets|TEXT\("(list_assets|search_assets)"\)|"(list_assets|search_assets)"|ListAssets\(|SearchAssets\(' BlueprintHelper\Source\BlueprintHelper AgentFaceService --glob '!**/build/**' --glob '!**/node_modules/**'
```

Expected:

```text
No deprecated list/search entrypoint remains.
```

**Checkpoint scope:** remove deprecated list/search surface without changing open/save/get-info behavior.

---

## Task 4: Wire The UE Module And Bridge Route

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/BlueprintHelper.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/BlueprintHelper.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/BlueprintHelperBridgeRoutePlanner.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Utils/BlueprintHelperBridgeRoutePlannerUtils.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/Routes/BlueprintHelperAssetDiscoveryBridgeRoutes.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Routes/BlueprintHelperAssetDiscoveryBridgeRoutes.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/BlueprintHelperBridgeRouter.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeRouter.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperRequestValidator.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/BlueprintHelperToolClusterConfigResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Bridge/BlueprintHelperBridgeRoutePlannerTests.cpp`

- [ ] **Step 1: Write the route planner failing test**

Add:

```cpp
{TEXT("find_assets"), EBlueprintHelperBridgeRouteCluster::AssetDiscovery},
```

to `FBlueprintHelperBridgeRoutePlanner_KnownCommandsMapToClusters`.

The existing test must continue to assert:

```cpp
TestTrue(TEXT("find_assets requires GameThread execution"), Plan.bRequiresGameThread);
```

- [ ] **Step 2: Run route planner suite and confirm RED**

Run:

```text
Automation RunTests BlueprintHelper.Router.Cluster
```

Expected: FAIL because `AssetDiscovery` cluster and `find_assets` plan are missing.

- [ ] **Step 3: Add the route cluster**

Add:

```cpp
AssetDiscovery,
```

to `EBlueprintHelperBridgeRouteCluster`.

Map:

```cpp
{TEXT("find_assets"), EBlueprintHelperBridgeRouteCluster::AssetDiscovery},
{EBlueprintHelperBridgeRouteCluster::AssetDiscovery, TEXT("AssetDiscovery")},
```

P0 keeps:

```cpp
Plan.bRequiresGameThread = Plan.bKnownCommand;
```

- [ ] **Step 4: Own and inject the new service**

Construct in module startup before Router creation:

```cpp
AssetDiscoveryService = MakeUnique<FBlueprintHelperAssetDiscoveryService>();
```

Keep the existing independent `AssetBrowseService` construction for open/save/get-info. Inject
`*AssetDiscoveryService` into `FBlueprintHelperBridgeRouter`, then construct:

```cpp
FBlueprintHelperAssetDiscoveryBridgeRoutes AssetDiscoveryRoutes;
```

from that dependency in the Router initializer list.

- [ ] **Step 5: Write the route adapter failing tests**

Add Bridge route tests for:

```text
BlueprintHelper.AssetDiscovery.Route.AcceptsFindAssetsPayload
BlueprintHelper.AssetDiscovery.Route.RejectsCursorInP0
BlueprintHelper.AssetDiscovery.Route.SerializesFindAssetsResult
```

These tests own protocol-field rejection. The service request struct has no `cursor` field.

- [ ] **Step 6: Add the independent `AssetDiscoveryBridgeRoutes` adapter**

Follow the current static cluster route pattern used by `BlueprintHelperAssetFactoryBridgeRoutes.*`:

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperAssetDiscoveryBridgeRoutes
{
public:
	explicit FBlueprintHelperAssetDiscoveryBridgeRoutes(
		const FBlueprintHelperAssetDiscoveryService& InAssetDiscoveryService);

	static bool IsAssetDiscoveryCommand(const FString& Command);
	FBlueprintHelperBridgeResponse HandleRequest(
		const FBlueprintHelperBridgeRequest& Request) const;

private:
	const FBlueprintHelperAssetDiscoveryService& AssetDiscoveryService;
};
```

`HandleRequest` parses:

```text
schema
query
path_prefixes
asset_types
asset_classes
recursive
limit
include_plugin_content
include_engine_content
include_redirectors
```

Reject `cursor` in P0 with:

```text
cursor_not_supported_in_p0
```

Serialize:

```cpp
Resp.Result = FindResult.Data.ToJson();
```

- [ ] **Step 7: Delegate the new cluster from the Router**

Include and own `FBlueprintHelperAssetDiscoveryBridgeRoutes` in the Router. Delegate with:

```cpp
if (RoutePlan.Cluster == EBlueprintHelperBridgeRouteCluster::AssetDiscovery &&
	FBlueprintHelperAssetDiscoveryBridgeRoutes::IsAssetDiscoveryCommand(Request.Command))
{
	return FBlueprintHelperBridgeRouterLocalUtils::ExecuteRouteWithTiming(
		Request,
		[&]() { return AssetDiscoveryRoutes.HandleRequest(Request); });
}
```

Do not add an inline `HandleFindAssets` method to `FBlueprintHelperBridgeRouter`.

- [ ] **Step 8: Register request validation and output limiting**

Add `find_assets` to:

```text
BlueprintHelperRequestValidator.cpp
BlueprintHelperToolClusterConfigResolver.cpp
Bridge read timing command set
```

The request validator must require:

```text
schema: string
```

and type-check all optional fields.

- [ ] **Step 9: Run route planner, route adapter, and AssetDiscovery suites**

Run:

```text
Automation RunTests BlueprintHelper.Router.Cluster
Automation RunTests BlueprintHelper.AssetDiscovery.Route
Automation RunTests BlueprintHelper.AssetDiscovery
```

Expected: all three suites pass.

**Checkpoint scope:** UE service, route adapter, Router composition, module wiring.

---

## Task 5: Add The Default AgentFace Tool Surface

**Files:**
- Create: `AgentFaceService/task-core/src/tool-surface/bridge/asset-discovery-schema.ts`
- Create: `AgentFaceService/task-core/src/tool-surface/bridge/asset-discovery-schema.test.ts`
- Modify: `AgentFaceService/task-core/src/tool-surface/registry/tool-metas.ts`
- Modify: `AgentFaceService/task-core/src/tool-surface/bridge/bridge-tool-schemas.ts`
- Modify: `AgentFaceService/task-core/src/tool-surface/bridge/bridge-tool-command-map.ts`
- Modify: `AgentFaceService/task-core/src/tests/tool-surface/tool-registry.contract.test.ts`

- [ ] **Step 1: Write failing schema and registry tests**

Cover:

```text
accepts query + /Game path + semantic type
accepts full asset class path
rejects limit=0
rejects limit=101
rejects cursor in P0
registry includes blueprinthelper_find_assets
deprecated blueprint_list_assets / blueprint_search_assets names are absent from task-core map and frozen contract
```

- [ ] **Step 2: Run task-core node tests and confirm RED**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
```

Expected: FAIL because the new schema and registry tool do not exist.

- [ ] **Step 3: Add the Zod schema**

```ts
import { z } from 'zod';

export const FindAssetsInputSchema = z.object({
  schema: z.literal('BlueprintHelper.FindAssetsRequest.v1'),
  query: z.string().optional(),
  path_prefixes: z.array(z.string().startsWith('/')).optional(),
  asset_types: z.array(z.string().min(1)).optional(),
  asset_classes: z.array(z.string().startsWith('/Script/')).optional(),
  recursive: z.boolean().optional(),
  limit: z.number().int().min(1).max(100).optional(),
  include_plugin_content: z.boolean().optional(),
  include_engine_content: z.boolean().optional(),
  include_redirectors: z.boolean().optional(),
}).strict();
```

P0 intentionally omits `cursor`.

- [ ] **Step 4: Register the default tool**

Add:

```ts
{
  name: 'blueprinthelper_find_assets',
  description: 'Find Unreal assets through AssetRegistry before a target asset_path is known.',
  audience: 'default',
  risk: 'low',
},
```

Map:

```ts
blueprinthelper_find_assets: 'find_assets',
```

Register:

```ts
blueprinthelper_find_assets: FindAssetsInputSchema,
```

Use the existing generic Bridge tool handler. Do not add a second dispatcher branch unless payload normalization later proves necessary.

- [ ] **Step 5: Run task-core tests and confirm GREEN**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
```

Expected: all task-core tests pass.

**Checkpoint scope:** AgentFace shared registry and schema.

---

## Task 6: Add CLI Help, Template Navigation, And Agent Rules

**Files:**
- Create: `AgentFaceService/agent-guide/Templates/blueprinthelper_find_assets_template.json`
- Modify: `AgentFaceService/cli/src/cli/help.ts`
- Modify: `AgentFaceService/cli/src/tests/cli/cli-tool-bridge.test.ts`
- Modify: `AgentFaceService/docs/CLI_Tools_API_Reference.md`
- Modify: `AgentFaceService/agent-guide/Templates/SEMANTIC_INDEX.md`
- Modify: `AgentFaceService/agent-guide/Templates/INDEX.md`
- Modify: `AgentFaceService/agent-guide/00_Agent_Onboarding_Index.md`
- Modify: `AgentFaceService/agent-guide/Reference/02_TaskSpec_First_Tool_Selection.md`
- Modify: `AgentFaceService/agent-guide/Reference/04_Tool_Surface_Field_Templates.md`

- [ ] **Step 1: Add the root CLI template**

```json
{
  "schema": "BlueprintHelper.FindAssetsRequest.v1",
  "query": "__ASSET_NAME_QUERY__",
  "path_prefixes": [
    "/Game"
  ],
  "asset_types": [
    "blueprint"
  ],
  "recursive": true,
  "limit": 20,
  "include_plugin_content": false,
  "include_engine_content": false,
  "include_redirectors": false
}
```

- [ ] **Step 2: Add tool-specific CLI help**

Document:

```text
bh blueprinthelper_find_assets --file <find-assets.json> --select status,artifacts.full_result
```

Point template navigation to:

```text
AgentFaceService/agent-guide/Templates/SEMANTIC_INDEX.md
AgentFaceService/agent-guide/Templates/blueprinthelper_find_assets_template.json
```

- [ ] **Step 3: Add CLI tests**

Assert:

```text
blueprinthelper_find_assets --help contains the template path
blueprinthelper_find_assets dispatches Bridge command find_assets
stdout contains compact FindAssets.v1 result
```

- [ ] **Step 4: Add Agent-facing workflow rule**

Add this rule to onboarding and tool-selection docs:

```text
Unknown Unreal asset_path -> blueprinthelper_find_assets
Known Unreal asset_path -> blueprinthelper_read_context
Write request -> resolve one explicit asset_path before preview_task
```

Add:

```text
Agents MUST NOT scan filesystem .uasset files and infer BlueprintHelper write targets.
Multiple candidates MUST be narrowed or confirmed before any write preview.
```

- [ ] **Step 5: Document the request fields**

Add `BlueprintHelper.FindAssetsRequest.v1` and `FindAssets.v1` compact examples to the field template reference. Explicitly state:

```text
P0 does not accept cursor and does not return total_count.
```

- [ ] **Step 6: Update CLI API reference**

Update:

```text
AgentFaceService/docs/CLI_Tools_API_Reference.md
```

with:

```text
blueprinthelper_find_assets command entry
template path
stdin/file invocation examples
P0 field contract
unknown asset_path -> find_assets workflow
removed blueprint_list_assets / blueprint_search_assets note if they are mentioned nearby
```

This document explicitly tracks `help.ts`, `tool-metas.ts`, and `Templates/`, so it is part of the CLI sync boundary.

- [ ] **Step 7: Build and run CLI tests**

Run:

```powershell
npm.cmd --prefix AgentFaceService/cli run build
npm.cmd --prefix AgentFaceService/cli run test:node
```

Expected: all CLI tests pass.

**Checkpoint scope:** CLI, CLI API reference, and AgentGuide.

---

## Task 7: Sync Codex And Claude Plugin Bundles

**Files:**
- Modify: `CodexPlugin/skills/blueprint-helper/SKILL.md`
- Modify: `CodexPlugin/skills/blueprint-helper/references/00_Agent_Onboarding_Index_20260504.md`
- Modify: `CodexPlugin/skills/blueprint-helper/references/02_TaskSpec_First_Tool_Selection_20260504.md`
- Modify: `CodexPlugin/skills/blueprint-helper/references/04_Tool_Surface_Field_Templates_20260512.md`
- Modify: `ClaudePlugin/skills/blueprint-helper/SKILL.md`
- Modify: `ClaudePlugin/skills/blueprint-helper/references/00_Agent_Onboarding_Index_20260504.md`
- Modify: `ClaudePlugin/skills/blueprint-helper/references/02_TaskSpec_First_Tool_Selection_20260504.md`
- Modify: `ClaudePlugin/skills/blueprint-helper/references/04_Tool_Surface_Field_Templates_20260512.md`

- [ ] **Step 1: Mirror the AgentGuide workflow**

Add:

```text
blueprinthelper_find_assets
```

to supported default commands.

Add:

```text
When target_asset_path is unknown, use blueprinthelper_find_assets before dispatching reads or writes.
Do not infer Unreal asset targets from filesystem .uasset paths.
```

- [ ] **Step 2: Remove stale ordinary-flow references encountered in touched blocks**

If touched supported-command lists still expose removed tools such as:

```text
blueprinthelper_read_task_context
```

remove them while syncing the AssetDiscovery rule. Do not expand this task into unrelated plugin documentation cleanup.

- [ ] **Step 3: Verify source and bundle lists agree**

Run:

```powershell
rg -n "blueprinthelper_find_assets|blueprinthelper_read_task_context|\\.uasset" AgentFaceService\agent-guide CodexPlugin\skills\blueprint-helper ClaudePlugin\skills\blueprint-helper
```

Expected:

```text
find_assets appears in normal workflow docs.
Removed ordinary-flow tools do not remain in touched supported-command lists.
.uasset guidance forbids write-target inference.
```

**Checkpoint scope:** packaged Agent instructions only.

---

## Task 8: Run Current Verification Gates

- [ ] **Step 1: Run task-core build and tests**

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
```

- [ ] **Step 2: Run MCP build and tests**

`register-tools.ts` is part of this P0 change set, so MCP must be verified independently:

```powershell
npm.cmd --prefix AgentFaceService/mcp run build:mcp
npm.cmd --prefix AgentFaceService/mcp run test:node
```

- [ ] **Step 3: Run CLI build and tests**

```powershell
npm.cmd --prefix AgentFaceService/cli run build
npm.cmd --prefix AgentFaceService/cli run test:node
```

- [ ] **Step 4: Open the Editor through the global MCP lifecycle tool when needed**

Use only:

```text
mcp__blueprint_helper__blueprint_open_editor
```

Do not use CLI lifecycle aliases.

- [ ] **Step 5: Run focused UE automation**

Run these as separate invocations:

```text
Automation RunTests BlueprintHelper.AssetDiscovery
Automation RunTests BlueprintHelper.AssetDiscovery.Route
Automation RunTests BlueprintHelper.Router.Cluster
```

- [ ] **Step 6: Build the plugin against UE 5.6**

Use the repository's normal UE 5.6 plugin build command with:

```text
E:\UE_5.6
```

Do not run compilation speed benchmarking.

- [ ] **Step 7: Run static closure checks**

```powershell
git diff --check
rg -n "T[B]D|T[O]DO" BlueprintHelper\Source\BlueprintHelper\Public\Shared\AssetDiscovery BlueprintHelper\Source\BlueprintHelper\Public\Systems\ToolClusters\AssetDiscovery BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\AssetDiscovery BlueprintHelper\Source\BlueprintHelper\Public\Entry\Bridge\Routes\BlueprintHelperAssetDiscoveryBridgeRoutes.h BlueprintHelper\Source\BlueprintHelper\Private\Entry\Bridge\Routes\BlueprintHelperAssetDiscoveryBridgeRoutes.cpp AgentFaceService\task-core\src\tool-surface\bridge\asset-discovery-schema.ts AgentFaceService\agent-guide\Templates\blueprinthelper_find_assets_template.json
rg -n 'blueprint_(list|search)_assets|TEXT\("(list_assets|search_assets)"\)|"(list_assets|search_assets)"|ListAssets\(|SearchAssets\(' BlueprintHelper\Source\BlueprintHelper AgentFaceService --glob '!**/build/**' --glob '!**/node_modules/**'
rg -n "Shared/Debug/BlueprintHelperAssetDiscoveryTypes|HandleFindAssets" BlueprintHelper\Source\BlueprintHelper AgentFaceService\task-core\src AgentFaceService\cli\src
rg -n "\"total_count\"|\"next_cursor\"|\"cursor\"" BlueprintHelper\Source\BlueprintHelper\Public\Shared\AssetDiscovery BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\AssetDiscovery BlueprintHelper\Source\BlueprintHelper\Private\Entry\Bridge\Routes\BlueprintHelperAssetDiscoveryBridgeRoutes.cpp AgentFaceService\task-core\src\tool-surface\bridge\asset-discovery-schema.ts AgentFaceService\agent-guide\Templates\blueprinthelper_find_assets_template.json
git status --short
```

Expected:

```text
No whitespace errors.
No deprecated list/search entrypoint remains.
No stale Debug DTO include.
No inline Router HandleFindAssets method.
Review protocol-field matches: only the explicit P0 cursor rejection may remain; find_assets does not expose total_count or cursor fields.
Dirty-tree review identifies only task-owned changes plus pre-existing user changes.
```

- [ ] **Step 8: Close the Editor through the global MCP lifecycle tool when required**

Use only:

```text
mcp__blueprint_helper__blueprint_close_editor
```

- [ ] **Step 9: Report manual commit scope**

Do not stage or commit. Report only files changed by this implementation and a suggested commit message.

---

## 9. P1 Follow-Up Boundary

Create a separate P1 plan after P0 live gates pass. P1 scope:

```text
AssetRegistry lifecycle events
-> immutable AssetDiscovery snapshot
-> atomic publication
-> Bridge execution lane enum
-> non-GameThread snapshot read lane
-> stable sorting
-> query-hash cursor
-> cursor invalidation
```

P1 must prove that read-lane code:

- does not read UObject;
- does not invoke AssetRegistry in-memory enumeration;
- does not use timer or polling refresh;
- permits parallel `find_assets` snapshot filtering without adding pressure to UE GameThread.
