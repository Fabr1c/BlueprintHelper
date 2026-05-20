# BlueprintHelper Settings Runtime Consumption Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 `DefaultSetting.json` / project `setting.json` 中声明的配置全部通过统一运行时配置入口被实际消费，消除“配置存在但代码仍硬编码”的分裂状态。

**Architecture:** `FBlueprintHelperSettingStore` 保持 JSON 存储层，新增 merged effective config 和 typed read 能力；业务代码不直接读 dot-path JSON，而是通过 domain resolver 取得 typed policy/value object。Review/UI/Runtime/Tool/Debug/GraphLayout 各自只消费本域 resolver，避免 widget 或 service 分散硬编码。

**Tech Stack:** Unreal Engine 5.6、C++、Slate、JsonObject、BlueprintHelper `Systems/Config`、ReviewPanel UI、TaskRuntime、ToolClusters、GraphLayout、DebugBundle。

---

## 执行状态同步（2026-05-20）

- [x] Task 1/2：SettingsStore effective config、array path、Runtime typed resolver 已实现，编译通过。
- [x] Task 3：ReviewPanel / GraphPanel 已接入 `ui.review_panel.*` typed settings，编译通过。
- [x] Task 4：MainWindow / Notification / TaskSpecWorkbench / LayoutRuleEditor 已接入 typed UI settings，编译通过。
- [x] Task 5/6：Bridge / TaskRuntime / Review / DebugExport settings resolver 与消费链路已实现，编译通过。
- [x] Task 7：ToolCluster / ReadContext settings 已接入；`tool_clusters.read_context.default_scope` 按最新需求移出配置面，不再作为 Settings runtime consumption 范围。
- [x] Task 8：GraphLayout `rules_source` 已通过 resolver 接入，编译通过。
- [x] Task 9：SettingsPanel 行状态与 color array 文本行已实现，编译通过。
- [x] Task 10：全量编译、Settings Automation 与 runtime smoke 已通过。
  - 验证结果：`Build.bat TemplateEditor Win64 Development` 成功；`BlueprintHelper.Settings` Automation 3/3 succeeded；Bridge ping、runtime profile、runtime diagnostics smoke 均返回 `ok=true`。
  - 验证报告：`D:\UEProjects\Template\Saved\Automation\SettingsRuntime_20260520_214053\index.json`。
  - 额外修正：`AutoRepair` 已接入 high-risk command 判定，runtime diagnostics 显示 `risk_command.enabled`，不再依赖 `BLUEPRINTHELPER_ENABLE_HIGH_RISK_COMMANDS`。

## 审计输入摘要

本计划基于并发只读审计结论：

- `FBlueprintHelperSettingStore` 目前主要服务 Settings UI，缺少完整 merged effective config。
- `ui.review_panel.*` 在 ReviewPanel / GraphPanel runtime 全部未消费。
- `ui.main_window`、`ui.notifications`、`ui.task_spec_workbench` 基本未消费。
- `ui.layout_rule_editor` 与 `GraphLayoutRules.json` 存在语义重复，需要明确边界。
- `runtime.bridge`、`runtime.task_runtime.cache`、`runtime.task_runtime.execution_policy` 未消费 setting。
- `review.*`、`debug.*`、`tool_clusters.*`、`graph_layout.rules_source` 大多未消费。
- `tool_clusters.signature.reference_context_max_results` 未消费，并存在 `max_results` / `max_result_count` 字段错配。

## Agent Profile 与 Setting 边界同步（2026-05-20）

`agent-profile.json` 仍负责 Editor 启动前必须可读的 Agent / project bootstrap 信息；`setting.json` 负责 UE 插件运行时可消费的配置。当前可合并性如下：

| agent-profile 字段 | 是否适合合并到 setting | 结论 |
| --- | --- | --- |
| `active_profile.safety_profile` | 是，但需要迁移 resolver | 可与 `setting.profiles.*.safety_profile` 统一；本轮先保持 agent-profile 作为 AutoRepair 来源，并已让 AutoRepair 影响 high-risk command 判定。 |
| `safety.preview_required` / `write_approval_required` / `approval_bypass` | 是 | 属于 UE runtime 授权策略，后续可进入 `setting.runtime` 或 `setting.safety` 域。 |
| `active_profile.auto_save_policy` | 部分适合 | 如果影响 TaskRuntime save 默认值，可映射到 `runtime.task_runtime.execution_policy.should_save`；如果只是 Agent 行为偏好，则保留在 agent-profile / UserPreferences。 |
| `active_profile.missing_capability_policy` / `agent.fallback_when_task_tools_unavailable` | 不建议 | 这是 Agent 决策策略，不是 UE runtime 配置。 |
| `agent.agent_entry_mode` | 不建议 | 属于 Agent-facing workflow 入口策略。 |
| `editor_lifecycle.*` | 不建议 | Editor 启动前需要读取，不能依赖 UE 插件 runtime setting。 |
| `environment.ue_engine_dir` / `ue_version` | 不建议 | CLI/MCP 生命周期工具启动 Editor 前需要读取，属于 bootstrap 配置。 |
| `schema` | 不合并 | 两个文件有不同 schema 边界。 |

## 非目标

- 不让 UI widget 直接读 JSON。
- 不引入旧字段兼容。
- 不把 SettingsPanel 的“编辑入口”当作 runtime consumer。
- 不用 timer、delay、polling 来刷新配置。
- 不改变 UE 5.6 主实现路径以兼容旧引擎。
- 不把 `GraphLayoutRules.json` 的规则内容整体塞进 `setting.json`；只解决 setting 与规则源的边界。

## 文件结构总览

### 基础配置层

Create:

- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\Config\BlueprintHelperRuntimeSettingResolver.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\Config\BlueprintHelperRuntimeSettingResolver.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Tests\Config\BlueprintHelperRuntimeSettingResolverTests.cpp`

Modify:

- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\Config\BlueprintHelperSettingStore.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\Config\BlueprintHelperSettingStore.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Tests\Config\BlueprintHelperSettingStoreTests.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Config\DefaultSetting.json`

### ReviewPanel / GraphPanel

Create:

- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\UI\Review\BlueprintHelperReviewPanelSettings.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\UI\Review\BlueprintHelperReviewPanelSettingsResolver.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Review\BlueprintHelperReviewPanelSettingsResolver.cpp`

Modify:

- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\UI\Review\BlueprintHelperReviewPresenterTypes.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\UI\Review\BlueprintHelperReviewSurfaceFrameBuilder.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Review\BlueprintHelperReviewSurfaceFrameBuilder.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Review\SBlueprintHelperReviewDiffFrame.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Review\SBlueprintHelperReviewDiffFrame.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Review\BlueprintHelperReviewDiffBlockNode.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Review\Utils\BlueprintHelperReviewGraphBoundsUtils.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Review\Utils\BlueprintHelperReviewGraphBoundsUtils.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Review\BlueprintHelperReviewGraphPresenter.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Review\SBlueprintHelperReviewPanel.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Review\SBlueprintHelperReviewPanel.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Review\SBlueprintHelperReviewPanelLayout.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Review\SBlueprintHelperReviewPanelDebug.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Review\BlueprintHelperReviewSurfaceFrameWidgetUtils.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Review\BlueprintHelperReviewSurfaceFrameGeometryUtils.cpp`

### MainWindow / Notifications / TaskSpecWorkbench / LayoutRuleEditor

Create:

- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\UI\BlueprintHelperUiSettings.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\UI\BlueprintHelperUiSettingsResolver.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\BlueprintHelperUiSettingsResolver.cpp`

Modify:

- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\UI\SBlueprintHelperMainWindow.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\SBlueprintHelperMainWindow.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\TaskSpecWorkbench\SBlueprintHelperTaskSpecWorkbench.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\UI\Layout\SBlueprintHelperLayoutRuleEditor.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Layout\SBlueprintHelperLayoutRuleEditor.cpp`

### Runtime / Tool / Review / Debug / GraphLayout

Create:

- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Runtime\TaskRuntime\BlueprintHelperTaskRuntimeSettingsResolver.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Runtime\TaskRuntime\BlueprintHelperTaskRuntimeSettingsResolver.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Entry\Bridge\BlueprintHelperBridgeRuntimeConfigResolver.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Entry\Bridge\BlueprintHelperBridgeRuntimeConfigResolver.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\Review\BlueprintHelperReviewConfigResolver.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\Review\BlueprintHelperReviewConfigResolver.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\ToolClusters\BlueprintHelperToolClusterConfigResolver.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\BlueprintHelperToolClusterConfigResolver.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\Debug\BlueprintHelperDebugExportPolicyResolver.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\Debug\BlueprintHelperDebugExportPolicyResolver.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\GraphLayout\BlueprintHelperGraphLayoutRuleSourceResolver.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\GraphLayout\BlueprintHelperGraphLayoutRuleSourceResolver.cpp`

Modify:

- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Entry\Bridge\BlueprintHelperBridgeServer.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Entry\BlueprintHelper.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Entry\Bridge\BlueprintHelperBridgeServer.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Runtime\TaskRuntime\BlueprintHelperTaskRuntimeCacheConfig.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Runtime\TaskRuntime\BlueprintHelperTaskRuntimeService.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Runtime\TaskRuntime\BlueprintHelperTaskRuntimeDryRunPolicy.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Runtime\TaskRuntime\PostOperations\BlueprintHelperTaskRuntimePostOperationPlanner.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\Review\BlueprintHelperReviewBaselineSnapshotService.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\Review\BlueprintHelperReviewStoreService.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Review\BlueprintHelperReviewDebugBundleService.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\Debug\BlueprintHelperDebugCaseStoreService.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Shared\Debug\BlueprintHelperDebugTypes.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\BlueprintSignature\Utils\BlueprintHelperSignatureReferenceContextUtils.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Entry\Bridge\BlueprintHelperBridgeRouter.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Entry\Bridge\BlueprintHelperRequestValidator.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\BlueprintVariables\BlueprintHelperBlueprintVariableService.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Runtime\TaskRuntime\TaskPlanAdapters\BlueprintComponent\BlueprintHelperComponentTaskPlanAdapter.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\BlueprintHelperReplaceBlueprintGraphService.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\Pipeline\BlueprintGraphGenerationPipeline.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\GraphLayout\BlueprintHelperGraphLayoutCoordinator.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\Config\BlueprintHelperProjectConfigPaths.cpp`

## Task 1: Build Effective Settings Store

**Files:**

- Modify: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\Config\BlueprintHelperSettingStore.h`
- Modify: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\Config\BlueprintHelperSettingStore.cpp`
- Modify: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Tests\Config\BlueprintHelperSettingStoreTests.cpp`

- [x] **Step 1: Add effective config APIs**

Add these APIs to `FBlueprintHelperSettingStore`:

```cpp
static bool LoadEffectiveSettingObject(TSharedPtr<FJsonObject>& OutObject, FString& OutError);
static bool LoadEffectiveSettingJson(FString& OutJson, FString& OutError);
static bool TryGetEffectiveJsonValue(const FString& DotPath, TSharedPtr<FJsonValue>& OutValue, FString& OutError);
static bool TryGetProjectJsonValue(const FString& DotPath, TSharedPtr<FJsonValue>& OutValue, FString& OutError);
```

- [x] **Step 2: Implement recursive deep merge**

Implement a private helper:

```cpp
static void MergeJsonObjectInto(TSharedPtr<FJsonObject> Target, const TSharedPtr<FJsonObject>& Source)
{
	if (!Target.IsValid() || !Source.IsValid())
	{
		return;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Source->Values)
	{
		const TSharedPtr<FJsonValue>* ExistingValue = Target->Values.Find(Pair.Key);
		if (ExistingValue
			&& ExistingValue->IsValid()
			&& Pair.Value.IsValid()
			&& (*ExistingValue)->Type == EJson::Object
			&& Pair.Value->Type == EJson::Object)
		{
			MergeJsonObjectInto((*ExistingValue)->AsObject(), Pair.Value->AsObject());
			continue;
		}
		Target->SetField(Pair.Key, Pair.Value);
	}
}
```

Merge order must be:

```text
built-in fallback -> Config/DefaultSetting.json -> .blueprinthelper/setting.json -> Saved/BlueprintHelper/setting.user.json
```

- [x] **Step 3: Keep `Load()` display semantics but change `EffectiveJson`**

`FBlueprintHelperSettingStore::Load()` should still report paths and existence, but `EffectiveJson` must use `LoadEffectiveSettingJson()` rather than single-source JSON. `EffectiveSourcePath` should become a readable source summary, for example:

```cpp
View.EffectiveSourcePath = TEXT("built-in + default + project + user");
```

- [x] **Step 4: Add array-aware path parsing**

Support path segments:

```text
ui.review_panel.surface_geometry_padding[0]
ui.review_panel.surface_geometry_padding[1]
```

Keep current object-only dot paths working. This is required for future typed array consumers, but SettingsPanel can continue editing whole arrays as JSON strings.

- [x] **Step 5: Extend tests**

Add tests:

```cpp
TestTrue(TEXT("project override deep merges without dropping default siblings"), ...);
TestTrue(TEXT("array index path reads first element"), ...);
TestTrue(TEXT("effective object preserves default when project partial override exists"), ...);
```

- [x] **Step 6: Compile**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex
```

Expected: `Succeeded`.

## Task 2: Add Runtime Setting Resolver Typed API

**Files:**

- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\Config\BlueprintHelperRuntimeSettingResolver.h`
- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\Config\BlueprintHelperRuntimeSettingResolver.cpp`
- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Tests\Config\BlueprintHelperRuntimeSettingResolverTests.cpp`

- [x] **Step 1: Add typed read API**

Create:

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperRuntimeSettingResolver
{
public:
	static bool GetBool(const FString& DotPath, bool DefaultValue);
	static int32 GetInt(const FString& DotPath, int32 DefaultValue);
	static double GetDouble(const FString& DotPath, double DefaultValue);
	static FString GetString(const FString& DotPath, const FString& DefaultValue);
	static FVector2D GetVector2(const FString& DotPath, const FVector2D& DefaultValue);
	static FMargin GetMargin(const FString& DotPath, const FMargin& DefaultValue);
	static TSharedPtr<FJsonValue> GetJsonValue(const FString& DotPath);
};
```

- [x] **Step 2: Implement typed conversion from `TryGetEffectiveJsonValue`**

Rules:

```text
bool: JSON boolean first, string "true"/"false" second, otherwise default
int: JSON number rounded to int, string parse second, otherwise default
double: JSON number first, string parse second, otherwise default
string: JSON string first, number/bool stringified second, otherwise default
Vector2: JSON array [x,y] first, string "x,y" second, otherwise default
Margin: JSON array [left,top,right,bottom] first, string "l,t,r,b" second, otherwise default
```

- [x] **Step 3: Add resolver tests**

Test:

```cpp
TestEqual(TEXT("double reads number"), FBlueprintHelperRuntimeSettingResolver::GetDouble(TEXT("ui.review_panel.diff_frame_outer_padding"), 1.0), 3.0);
TestEqual(TEXT("vector reads array"), FBlueprintHelperRuntimeSettingResolver::GetVector2(TEXT("ui.review_panel.surface_geometry_padding"), FVector2D::ZeroVector), FVector2D(10.0, 10.0));
TestEqual(TEXT("missing int returns default"), FBlueprintHelperRuntimeSettingResolver::GetInt(TEXT("missing.path"), 42), 42);
```

- [x] **Step 4: Compile**

Run the Build command. Expected: `Succeeded`.

## Task 3: Consume ReviewPanel and GraphPanel UI Settings

**Files:**

- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\UI\Review\BlueprintHelperReviewPanelSettings.h`
- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\UI\Review\BlueprintHelperReviewPanelSettingsResolver.h`
- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Review\BlueprintHelperReviewPanelSettingsResolver.cpp`
- Modify Review files listed in the ReviewPanel / GraphPanel section.

- [x] **Step 1: Add ReviewPanel settings value object**

Create:

```cpp
struct FBlueprintHelperReviewPanelSettings
{
	TArray<float> MainSplitRatio = {0.14f, 0.86f};
	TArray<float> ComponentBlueprintSplit = {0.42f, 0.58f};
	TArray<float> MainGraphRatio = {0.62f, 0.20f};
	TArray<float> RightBottomRatio = {0.76f, 0.24f};
	FMargin RootRowPadding = FMargin(7.0f, 5.0f);
	float RowContentPadding = 6.0f;
	float DiffFrameOuterPadding = 3.0f;
	float DiffActionPadding = 5.0f;
	FMargin DiffActionSpacing = FMargin(0.0f, 0.0f, 6.0f, 0.0f);
	float SurfaceOverlayFillAlpha = 0.60f;
	float SurfaceOverlaySelectedFillAlpha = 0.74f;
	FVector2D SurfaceGeometryPadding = FVector2D(10.0f, 10.0f);
	float FlashTickDecay = 1.8f;
	int32 DebugMaxMessages = 200;
	bool bOverlayFilterCurrentAssetOnly = true;
};
```

- [x] **Step 2: Add resolver**

`FBlueprintHelperReviewPanelSettingsResolver::Load()` reads all `ui.review_panel.*` keys through `FBlueprintHelperRuntimeSettingResolver`.

- [x] **Step 3: Store settings on `SBlueprintHelperReviewPanel`**

Add member:

```cpp
FBlueprintHelperReviewPanelSettings ReviewPanelSettings;
```

In Construct:

```cpp
ReviewPanelSettings = FBlueprintHelperReviewPanelSettingsResolver::Load();
```

- [x] **Step 4: Replace split and row padding hardcodes**

Replace:

```text
SBlueprintHelperReviewPanelLayout.cpp:67,72,81,87,94,99,103,108
SBlueprintHelperReviewPanel.cpp:360,965
```

with `ReviewPanelSettings` values.

- [x] **Step 5: Replace DiffFrame style hardcodes**

Extend `FBlueprintHelperReviewPanelSurfacePresenterArgs` or `FBlueprintHelperReviewSurfaceFrameBuilder` args with:

```cpp
float DiffFrameOuterPadding;
float DiffActionPadding;
FMargin DiffActionSpacing;
float SurfaceOverlayFillAlpha;
float SurfaceOverlaySelectedFillAlpha;
```

Pass these values into `SBlueprintHelperReviewDiffFrame` and `FBlueprintHelperReviewSurfaceFrameWidgetUtils`.

- [x] **Step 6: Replace GraphPanel bounds hardcode**

Change `FBlueprintHelperReviewGraphBoundsUtils::BuildPaddedBounds` signature:

```cpp
static bool BuildPaddedBounds(const FBox2D& Bounds, bool bHasBounds, const FVector2D& Padding, FVector2D& OutPosition, FVector2D& OutSize);
```

Call it from `BlueprintHelperReviewGraphPresenter.cpp` with `ReviewPanelSettings.SurfaceGeometryPadding`. Remove runtime dependence on `CommentStylePadding = 20.0f` for new graph diff block bounds.

- [x] **Step 7: Replace DiffBlock action hardcodes**

Add style args to `UBlueprintHelperReviewDiffBlockNode::Configure`:

```cpp
float InActionPadding;
FMargin InActionSpacing;
FMargin InActionAnchorPadding;
```

Use these instead of:

```text
10.0 bottom-right offset
5.0 action padding
6.0 action spacing
```

- [x] **Step 8: Replace debug max and filter hardcodes**

Use `ReviewPanelSettings.DebugMaxMessages` in `SBlueprintHelperReviewPanel::AddDebugMessage`.

Use `ReviewPanelSettings.bOverlayFilterCurrentAssetOnly` wherever current code assumes current asset only.

- [x] **Step 9: Compile**

Run the Build command. Expected: `Succeeded`.

## Task 4: Consume MainWindow, Notification, Workbench, and Layout Editor Settings

**Files:**

- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\UI\BlueprintHelperUiSettings.h`
- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\UI\BlueprintHelperUiSettingsResolver.h`
- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\BlueprintHelperUiSettingsResolver.cpp`
- Modify UI files listed above.

- [x] **Step 1: Add UI settings structs**

Create structs:

```cpp
struct FBlueprintHelperMainWindowSettings
{
	FString DefaultTab = TEXT("tools");
	float TabBarPadding = 6.0f;
	FMargin TabButtonSpacing = FMargin(0.0f, 0.0f, 6.0f, 0.0f);
	FLinearColor ActiveTabColor = FLinearColor(0.18f, 0.34f, 0.62f, 1.0f);
	FLinearColor InactiveTabColor = FLinearColor(0.08f, 0.08f, 0.08f, 1.0f);
	FText CleanupButtonLabel = FText::FromString(TEXT("Clean Review Data"));
	float CleanupButtonMarginLeft = 10.0f;
};

struct FBlueprintHelperNotificationSettings
{
	bool bCleanupUseThrobber = true;
	bool bCleanupUseSuccessFailIcons = false;
	bool bCleanupFireAndForget = false;
	float CleanupFadeOutSeconds = 0.5f;
	float CleanupExpireSeconds = 4.0f;
};
```

Add TaskSpecWorkbench and LayoutRuleEditor settings in the same header as data structs.

- [x] **Step 2: Resolve UI settings once per widget construction**

`SBlueprintHelperMainWindow` consumes `FBlueprintHelperUiSettingsResolver::LoadMainWindowSettings()` and `LoadNotificationSettings()`.

`SBlueprintHelperTaskSpecWorkbench` consumes `LoadTaskSpecWorkbenchSettings()`.

`SBlueprintHelperLayoutRuleEditor` consumes `LoadLayoutRuleEditorSettings()` for UI/editor defaults only.

- [x] **Step 3: Keep GraphLayoutRules as graph layout rule source**

Do not duplicate rule semantics into `ui.layout_rule_editor`. `ui.layout_rule_editor` controls editor defaults and UI values; actual graph layout rule execution continues to use `GraphLayoutRules.json` unless `graph_layout.rules_source` is changed by Task 8.

- [x] **Step 4: Compile**

Run the Build command. Expected: `Succeeded`.

## Task 5: Consume Runtime Bridge and TaskRuntime Settings

**Files:**

- Create bridge and task runtime resolvers listed above.
- Modify bridge and task runtime files listed above.

- [x] **Step 1: Add bridge runtime config**

Create:

```cpp
struct FBlueprintHelperBridgeRuntimeConfig
{
	int32 Port = 54321;
	int32 MaxPendingConnections = 8;
	int32 AcceptWaitMs = 250;
	double IdleTimeoutSeconds = 2.0;
	int32 MaxFrameBytes = 16777216;
	int32 SocketBufferBytes = 262144;
};
```

Resolve from `runtime.bridge.*`.

- [x] **Step 2: Use bridge config in startup**

`BlueprintHelper.cpp` constructs bridge server using resolved port. `BlueprintHelperBridgeServer.cpp` uses `AcceptWaitMs`, queue/backlog, frame and socket buffer values where supported.

- [x] **Step 3: Add TaskRuntime config resolver**

Resolve:

```text
runtime.task_runtime.cache.partial_preview.*
runtime.task_runtime.cache.call_function_fact.*
runtime.task_runtime.cache.graph_write_plan.*
runtime.task_runtime.cache.prune_on_access_min_interval_seconds
runtime.task_runtime.execution_policy.should_compile
runtime.task_runtime.execution_policy.should_save
runtime.task_runtime.execution_policy.dry_run_mode
```

- [x] **Step 4: Merge setting defaults with TaskPlan overrides**

Policy:

```text
settings provide default
TaskPlan execution_policy overrides setting when present
request payload overrides setting only for explicitly request-scoped flags
```

- [x] **Step 5: Compile**

Run the Build command. Expected: `Succeeded`.

## Task 6: Consume Review, DebugBundle, and Debug Export Settings

**Files:**

- Create ReviewConfig and DebugExport resolvers listed above.
- Modify Review/Debug files listed above.

- [x] **Step 1: Add Review config resolver**

Resolve:

```text
review.version
review.evidence_required
review.artifact.snapshot_root
review.debug_bundle.root_dir
review.debug_bundle.sub_dir
review.debug_bundle.filename_pattern
review.debug_bundle.schema_review_panel
review.debug_bundle.schema_snapshot
review.debug_bundle.hash_source
review.debug_bundle.retention
review.debug_bundle.enforce_root_path
```

- [x] **Step 2: Replace hardcoded snapshot/debug paths**

`BlueprintHelperReviewBaselineSnapshotService.cpp`, `BlueprintHelperReviewDebugBundleService.cpp`, `BlueprintHelperReviewStoreService.cpp`, and `BlueprintHelperDebugCaseStoreService.cpp` consume `FBlueprintHelperReviewConfigResolver`.

- [x] **Step 3: Add Debug export policy**

Resolve:

```text
debug.export_profile
debug.contains_full_settings
```

Use it in debug manifest serialization instead of fixed `false`.

- [x] **Step 4: Compile**

Run the Build command. Expected: `Succeeded`.

## Task 7: Consume ToolCluster Settings and Fix Reference Context Field Split


> 执行状态：Task 7 已完成。`max_results`、read_context output limiter、GraphWrite/ToolCluster 默认设置已接入；`tool_clusters.read_context.default_scope` 按最新需求移出配置面，不再需要统一应用点。
> 距离期望差距：无。**Files:**

- Create `FBlueprintHelperToolClusterConfigResolver`.
- Modify tool files listed above.

- [x] **Step 1: Add typed cluster policies**

Create typed policies for:

```text
asset_factory
component
class_settings
blueprint_variables
object_property
data_table
umg_widget
signature
graph_write
read_context
```

Each policy reads default values from `tool_clusters.<cluster>.*`.

- [x] **Step 2: Apply request override rule**

Policy:

```text
settings provide defaults
request payload values override settings
TaskSpec values override settings through TaskPlan adapter
```

- [x] **Step 3: Fix signature reference context field split**

Make validator and router accept one canonical field:

```text
max_results
```

If `max_results` is absent, resolver default comes from `tool_clusters.signature.reference_context_max_results`.

Remove internal dependence on `max_result_count` unless user explicitly requests compatibility, which is not requested here.

- [x] **Step 4: Add ReadContext output limiter**

Add `FBlueprintHelperReadContextOutputLimiter` that consumes:

```text
tool_clusters.read_context.max_output_rows
tool_clusters.read_context.max_output_bytes
```

All read_context result builders must pass through this limiter before returning CLI result artifacts.

- [x] **Step 5: Compile**

Run the Build command. Expected: `Succeeded`.

## Task 8: Consume GraphLayout Rule Source Setting

**Files:**

- Create `FBlueprintHelperGraphLayoutRuleSourceResolver`.
- Modify GraphLayout files listed above.

- [x] **Step 1: Resolve `graph_layout.rules_source`**

Rules:

```text
"GraphLayoutRules.json" -> .blueprinthelper/GraphLayoutRules.json
relative path -> .blueprinthelper/<relative path>
absolute path -> use absolute path only if inside project or plugin config roots
```

- [x] **Step 2: Replace direct `GetGraphLayoutRulesPath()` consumers**

`FBlueprintHelperGraphLayoutCoordinator::LoadConfiguredRuleSetJson` and `SaveConfiguredRuleSetJson` use the resolver.

- [x] **Step 3: Preserve LayoutRuleEditor semantic boundary**

`ui.layout_rule_editor.*` remains UI/editor default settings. Actual rules remain in the resolved graph layout rules file.

- [x] **Step 4: Compile**

Run the Build command. Expected: `Succeeded`.

## Task 9: SettingsPanel Coverage for Developer-Only Settings

**Files:**

- Modify: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Settings\BlueprintHelperSettingsPresenter.cpp`
- Modify: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\UI\Settings\BlueprintHelperSettingRowViewModel.h`
- Modify: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Settings\SBlueprintHelperSettingRow.cpp`

- [x] **Step 1: Do not expose developer-only settings in normal user list**

Keep normal Settings page user-editable only. Add a developer mode section only if the current profile/safety level allows developer settings.

- [x] **Step 2: Add row support for color arrays**

Settings such as `ui.main_window.active_tab_color` and `ui.task_spec_workbench.block_colors.*` need color-array rendering:

```text
[R,G,B,A]
```

Use text input first; a color picker is not required in this plan.

- [x] **Step 3: Add display-only source and consumer status**

For each settings row, show whether it is:

```text
User editable
Developer only
Runtime consumed
Not yet consumed
```

After Tasks 1-8, all configured keys must be `Runtime consumed` or explicitly documented as schema metadata.

- [x] **Step 4: Compile**

Run the Build command. Expected: `Succeeded`.

## Task 10: Full Verification


> 执行状态：编译已通过；Automation / runtime smoke 已完成。
> 验证报告：`D:\UEProjects\Template\Saved\Automation\SettingsRuntime_20260520_214053\index.json`。**Files:**

- Read during validation: `D:\UEProjects\Template\BlueprintHelper\Saved\Logs` if needed.
- Read during validation: `D:\UEProjects\Template\.blueprinthelper\setting.json`
- Read during validation: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Config\DefaultSetting.json`

- [x] **Step 1: Build**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex
```

Expected: `Succeeded`.

- [x] **Step 2: Run automation tests**

Run editor automation for:

```text
BlueprintHelper.Settings.Store.UpdateJsonPath
BlueprintHelper.Settings.RuntimeResolver
```

Result: `BlueprintHelper.Settings.Store.EffectiveMerge`、`BlueprintHelper.Settings.Store.UpdateJsonPath`、`BlueprintHelper.Settings.RuntimeResolver` 均为 Success，warnings=0，errors=0。

- [x] **Step 3: Runtime smoke validation**

Runtime smoke result:

```text
bh.cmd bridge ping -> ok=true, status=bridge_available
bh.cmd blueprint_get_runtime_profile -> ok=true
bh.cmd blueprinthelper_diagnostics_runtime -> ok=true, diagnostics contains risk_command.enabled
```

- [x] **Step 4: Update this plan document**

Mark only fully completed items as `[x]`. Incomplete items must record:

```text
距离期望差距：
阻塞内容：
```

## Manual Commit Message Template

Do not run `git add`, `git commit`, or `git push` from Codex.

When all implementation and verification tasks pass, use:

```text
新增内容：
1. 添加统一运行时 Settings Resolver 和 typed policy 消费入口
2. 添加 ReviewPanel、Runtime、ToolCluster、Debug、GraphLayout 配置消费链路

修复内容：
1. 修复 setting 配置存在但运行时仍使用硬编码的问题
2. 修复 signature reference context 的 max_results 字段分裂问题

变更需求：
1. 将 SettingsStore 从 SettingsPanel 专用读取扩展为全局有效配置存储底座
```

## Self-Review

- Spec coverage: 覆盖 SettingsStore merged config、typed resolver、ReviewPanel、GraphPanel、非 Review UI、Runtime、ToolClusters、Review、Debug、GraphLayout 和 SettingsPanel 展示状态。
- Placeholder scan: 无 `TBD`、`TODO`、`implement later`。
- Type consistency: `FBlueprintHelperRuntimeSettingResolver`、`FBlueprintHelperReviewPanelSettingsResolver`、`FBlueprintHelperUiSettingsResolver`、`FBlueprintHelperToolClusterConfigResolver` 等命名全篇一致。
- Architecture fit: Store 只做 JSON 存储与 effective config；业务层只消费 typed resolver；UI 不直接读 JSON；不引入 delay/polling。

