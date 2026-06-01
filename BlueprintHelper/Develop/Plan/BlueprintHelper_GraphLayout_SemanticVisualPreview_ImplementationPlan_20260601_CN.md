# GraphLayout Semantic Visual Editing And Native Preview Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 LayoutRuleEditor 五个语义页 role center 持久化问题，移除旧 `editor_canvas.role_centers`，并新增语义箭头、中文 Hover Tips、异步原生 GraphPanel Preview。

**Architecture:** 以 GraphLayout semantic scene model 为共同基础，`FRuleSet` 只保存 scene-scoped visual state，Runtime 只消费 scene adapter 投影后的 RuleSet 参数或 normalized anchors。Preview 使用 worker 生成纯数据 sample/layout plan，GameThread 分帧 materialize transient `UEdGraph`，`SBlueprintHelperLayoutRuleEditor` 只做 UI 状态切换和事件转发。

**Tech Stack:** Unreal Engine 5.6 C++、Slate、SGraphEditor、UEdGraph/K2Node、GraphLayout RuleSet JSON、UE Automation Tests、BlueprintHelper CLI / Editor E2E。

---

## Non-Negotiable Boundaries

- 不保留旧 `editor_canvas.role_centers`。
- 不兼容读取旧 `editor_canvas.role_centers`。
- 不自动迁移旧 `editor_canvas.role_centers` 到 `editor_canvas.scenes`。
- 不让 Runtime 继续直接消费旧 `EditorCanvasRoleCenters` map。
- 不恢复 `RoleOverview` 对 `EditorCanvasRoleCenters` 的特殊读写语义。
- 不在 `SBlueprintHelperLayoutRuleEditor` 内实现 sample graph 构建、solver 编排、worker lifecycle 或 materialization 队列。
- Preview 不读取当前打开的真实 `UEdGraph`。
- Preview 不移动真实资产节点，不 mark dirty，不 save。
- Worker 只能处理纯数据 descriptor、snapshot、layout plan；UObject / `UEdGraph` / K2 node / Pin link / `SGraphEditor` 必须在 GameThread 创建或刷新。
- 本仓库规则禁止自动执行 `git add`、`git commit`、`git push`。本计划中的 checkpoint 只记录状态，不提交。

## Reference Documents

- Design: `BlueprintHelper/Develop/Design/BlueprintHelper_GraphLayout_SemanticVisualPreview_Design_20260601_CN.md`
- Debug: `Debug/GraphLayout_LayoutRuleEditor_DraggablePersistence_20260601.md`

## File Structure

### Create

- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutSemanticScene.h`
  - 定义五个 semantic scene 的 metadata、edge、node、scene state、adapter API。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutSemanticScene.cpp`
  - 提供五个 scene 的固定 metadata、默认 centers、scene state 读写、RuleSet 投影逻辑。
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewTypes.h`
  - 定义 preview request、sample descriptor、materialization node/link、job result、preview status。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSampleFactory.cpp`
  - 根据 scene 构建固定复杂 sample graph descriptor 和 `FGraphSnapshot`。
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSampleFactory.h`
  - 暴露 sample factory API。
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewService.h`
  - 管理 preview job id、worker build、cancel、result handoff。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewService.cpp`
  - 实现 worker 纯数据 preview build。
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewMaterializer.h`
  - 定义 GameThread 分帧 materializer API。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewMaterializer.cpp`
  - 实现 transient `UBlueprint` / `UEdGraph` / K2 node 创建、pin 连接、坐标写入。
- `BlueprintHelper/Develop/Report/BlueprintHelper_GraphLayout_SemanticVisualPreview_Report_20260601_CN.md`
  - 记录本会话代码改动原因、过程、结果和范围。

### Modify

- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h`
  - 移除 `FRuleSet::EditorCanvasRoleCenters`，新增 scene-scoped editor canvas state 类型和字段。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.cpp`
  - 导出 `editor_canvas.scenes.<scene>.role_centers`，不再导出旧 `editor_canvas.role_centers`。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSetJson.cpp`
  - 导入/校验 `editor_canvas.scenes`，拒绝旧 `editor_canvas.role_centers`。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutRoleAnchorResolver.cpp`
  - 移除旧 `EditorCanvasRoleCenters` runtime 消费，改为只基于 RuleSet 参数或 adapter 后的 normalized values。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutDataInputPlacement.cpp`
  - 如 resolver API 调整，更新调用。
- `BlueprintHelper/Source/BlueprintHelper/Public/UI/Layout/SBlueprintHelperLayoutRuleEditor.h`
  - 增加 Edit/Preview mode state、preview service/materializer handles、preview widget references。
- `BlueprintHelper/Source/BlueprintHelper/Private/UI/Layout/SBlueprintHelperLayoutRuleEditor.cpp`
  - 使用 semantic scene metadata 驱动 canvas、箭头、Tips、scene persistence、Preview shell 和 `SGraphEditor`。
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`
  - 更新旧 canvas offset tests，新增 RuleSet JSON、semantic scene、preview factory/service/materializer tests。
- `Debug/GraphLayout_LayoutRuleEditor_DraggablePersistence_20260601.md`
  - 追加 RED/GREEN、E2E、阻塞记录。

### Do Not Modify

- `AgentFaceService/*`
- `ClaudePlugin/*`
- `CodexPlugin/*`
- TaskSpec / TaskPlan schema
- GraphWrite mutation semantics
- Review v1 / legacy Transaction paths

---

## Task 1: RuleSet Scene State RED Tests

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`
- Modify: `Debug/GraphLayout_LayoutRuleEditor_DraggablePersistence_20260601.md`

- [ ] **Step 1: Add failing RuleSet JSON tests**

Add the include near the existing GraphLayout includes:

```cpp
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSemanticScene.h"
```

Append these tests before `#endif`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_RuleSetJsonRejectsLegacyEditorCanvasRoleCenters,
	"BlueprintHelper.GraphLayout.RuleSetJson.RejectsLegacyEditorCanvasRoleCenters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_RuleSetJsonRejectsLegacyEditorCanvasRoleCenters::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	const FString LegacyJson = TEXT(R"JSON(
{
  "schema": "BlueprintHelper.GraphLayoutRuleSet.v1",
  "editor_canvas": {
    "role_centers": {
      "ExecNode": { "x": 300, "y": 100 }
    }
  },
  "role_rules": []
}
)JSON");

	FRuleSet Parsed;
	FValidationResult Validation;
	TestFalse(TEXT("legacy editor_canvas.role_centers is rejected"), FRuleSetJson::ImportString(LegacyJson, Parsed, Validation));
	TestTrue(TEXT("validation has an explicit legacy error"), Validation.Errors.ContainsByPredicate([](const FString& Error)
	{
		return Error.Contains(TEXT("editor_canvas.role_centers"));
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_RuleSetJsonRoundTripsSemanticSceneCenters,
	"BlueprintHelper.GraphLayout.RuleSetJson.RoundTripsSemanticSceneCenters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_RuleSetJsonRoundTripsSemanticSceneCenters::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	FEditorCanvasSceneState PureDataScene;
	PureDataScene.RoleCenters.Add(ENodeRole::VariableInput, FVector2D(91.0f, 120.0f));
	PureDataScene.RoleCenters.Add(ENodeRole::OperatorOrCompare, FVector2D(355.0f, 164.0f));
	PureDataScene.RoleCenters.Add(ENodeRole::PureFunction, FVector2D(682.0f, 212.0f));
	RuleSet.EditorCanvasScenes.Add(ESemanticScene::PureDataSubgraph, PureDataScene);

	const FString Json = FRuleSetJson::ExportString(RuleSet);
	TestTrue(TEXT("exports editor_canvas.scenes"), Json.Contains(TEXT("\"scenes\"")));
	TestFalse(TEXT("does not export legacy role_centers at editor_canvas root"), Json.Contains(TEXT("\"editor_canvas\":{\"role_centers\"")));

	FRuleSet Parsed;
	FValidationResult Validation;
	TestTrue(TEXT("scene json imports"), FRuleSetJson::ImportString(Json, Parsed, Validation));
	const FEditorCanvasSceneState* ParsedScene = Parsed.EditorCanvasScenes.Find(ESemanticScene::PureDataSubgraph);
	TestNotNull(TEXT("pure data scene exists"), ParsedScene);
	if (ParsedScene)
	{
		TestEqual(TEXT("variable input x"), ParsedScene->RoleCenters.FindRef(ENodeRole::VariableInput).X, 91.0f);
		TestEqual(TEXT("operator y"), ParsedScene->RoleCenters.FindRef(ENodeRole::OperatorOrCompare).Y, 164.0f);
		TestEqual(TEXT("pure function y"), ParsedScene->RoleCenters.FindRef(ENodeRole::PureFunction).Y, 212.0f);
	}
	return true;
}
```

- [ ] **Step 2: Run RED build**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

Expected: compile fails because `BlueprintHelperGraphLayoutSemanticScene.h`, `FEditorCanvasSceneState`, `ESemanticScene`, and `FRuleSet::EditorCanvasScenes` do not exist yet.

- [ ] **Step 3: Record RED**

Append to `Debug/GraphLayout_LayoutRuleEditor_DraggablePersistence_20260601.md`:

```markdown
## RuleSet Scene State RED - 2026-06-01

- Added failing tests:
  - `BlueprintHelper.GraphLayout.RuleSetJson.RejectsLegacyEditorCanvasRoleCenters`
  - `BlueprintHelper.GraphLayout.RuleSetJson.RoundTripsSemanticSceneCenters`
- RED expectation: build fails until semantic scene storage replaces legacy `EditorCanvasRoleCenters`.
```

---

## Task 2: RuleSet Scene State And Legacy Removal

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSetJson.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutRoleAnchorResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`
- Modify: `Debug/GraphLayout_LayoutRuleEditor_DraggablePersistence_20260601.md`

- [ ] **Step 1: Add scene enum and state to `BlueprintHelperGraphLayoutTypes.h`**

In namespace `BlueprintHelper::GraphLayout`, after `EPinDirection`, add:

```cpp
enum class ESemanticScene : uint8
{
	LinearExecChain,
	PureDataSubgraph,
	NodeInputCluster,
	MultiExecOutput,
	Occupancy
};

struct FEditorCanvasSceneState
{
	TMap<ENodeRole, FVector2D> RoleCenters;
};
```

In `FRuleSet`, replace:

```cpp
TMap<ENodeRole, FVector2D> EditorCanvasRoleCenters;
```

with:

```cpp
TMap<ESemanticScene, FEditorCanvasSceneState> EditorCanvasScenes;
```

Add declarations near the existing `ToString` declarations:

```cpp
BLUEPRINTHELPER_API const TCHAR* ToString(ESemanticScene Scene);
BLUEPRINTHELPER_API bool LexTryParseString(ESemanticScene& OutScene, const FString& Value);
```

- [ ] **Step 2: Implement scene string conversion in `BlueprintHelperGraphLayoutTypes.cpp`**

Add:

```cpp
const TCHAR* ToString(ESemanticScene Scene)
{
	switch (Scene)
	{
	case ESemanticScene::LinearExecChain: return TEXT("linear_exec_chain");
	case ESemanticScene::PureDataSubgraph: return TEXT("pure_data_subgraph");
	case ESemanticScene::NodeInputCluster: return TEXT("node_input_cluster");
	case ESemanticScene::MultiExecOutput: return TEXT("multi_exec_output");
	case ESemanticScene::Occupancy: return TEXT("occupancy");
	default: return TEXT("linear_exec_chain");
	}
}

bool LexTryParseString(ESemanticScene& OutScene, const FString& Value)
{
	if (Value.Equals(TEXT("linear_exec_chain"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("LinearExecChain"), ESearchCase::IgnoreCase))
	{
		OutScene = ESemanticScene::LinearExecChain;
		return true;
	}
	if (Value.Equals(TEXT("pure_data_subgraph"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("PureDataSubgraph"), ESearchCase::IgnoreCase))
	{
		OutScene = ESemanticScene::PureDataSubgraph;
		return true;
	}
	if (Value.Equals(TEXT("node_input_cluster"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("NodeInputCluster"), ESearchCase::IgnoreCase))
	{
		OutScene = ESemanticScene::NodeInputCluster;
		return true;
	}
	if (Value.Equals(TEXT("multi_exec_output"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("MultiExecOutput"), ESearchCase::IgnoreCase))
	{
		OutScene = ESemanticScene::MultiExecOutput;
		return true;
	}
	if (Value.Equals(TEXT("occupancy"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("Occupancy"), ESearchCase::IgnoreCase))
	{
		OutScene = ESemanticScene::Occupancy;
		return true;
	}
	return false;
}
```

- [ ] **Step 3: Export scene-scoped JSON and remove legacy export**

In `ToJson(const FRuleSet& RuleSet)`, remove the block that exports `RuleSet.EditorCanvasRoleCenters`.

Add:

```cpp
if (RuleSet.EditorCanvasScenes.Num() > 0)
{
	TSharedRef<FJsonObject> EditorCanvasJson = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> ScenesJson = MakeShared<FJsonObject>();
	for (const TPair<ESemanticScene, FEditorCanvasSceneState>& ScenePair : RuleSet.EditorCanvasScenes)
	{
		TSharedRef<FJsonObject> SceneJson = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> RoleCentersJson = MakeShared<FJsonObject>();
		for (const TPair<ENodeRole, FVector2D>& RolePair : ScenePair.Value.RoleCenters)
		{
			if (RolePair.Key != ENodeRole::Unknown)
			{
				RoleCentersJson->SetObjectField(ToString(RolePair.Key), VectorToJson(RolePair.Value));
			}
		}
		SceneJson->SetObjectField(TEXT("role_centers"), RoleCentersJson);
		ScenesJson->SetObjectField(ToString(ScenePair.Key), SceneJson);
	}
	EditorCanvasJson->SetObjectField(TEXT("scenes"), ScenesJson);
	Json->SetObjectField(TEXT("editor_canvas"), EditorCanvasJson);
}
```

- [ ] **Step 4: Reject legacy field and import scenes in `BlueprintHelperGraphLayoutRuleSetJson.cpp`**

In `FRuleSetJson::Validate`, after `editor_canvas` is read or before role rules validation, add:

```cpp
const TSharedPtr<FJsonObject>* EditorCanvasObjectForValidation = nullptr;
if (Json->TryGetObjectField(TEXT("editor_canvas"), EditorCanvasObjectForValidation) &&
	EditorCanvasObjectForValidation &&
	EditorCanvasObjectForValidation->IsValid())
{
	if ((*EditorCanvasObjectForValidation)->HasField(TEXT("role_centers")))
	{
		Validation.AddError(TEXT("Legacy field 'editor_canvas.role_centers' is not supported. Use 'editor_canvas.scenes.<scene>.role_centers'."));
	}
}
```

In `FRuleSetJson::Import`, replace the old `editor_canvas.role_centers` import block with:

```cpp
const TSharedPtr<FJsonObject>* EditorCanvasObject = nullptr;
if (Json->TryGetObjectField(TEXT("editor_canvas"), EditorCanvasObject) && EditorCanvasObject && EditorCanvasObject->IsValid())
{
	const TSharedPtr<FJsonObject>* ScenesObject = nullptr;
	if ((*EditorCanvasObject)->TryGetObjectField(TEXT("scenes"), ScenesObject) && ScenesObject && ScenesObject->IsValid())
	{
		OutRuleSet.EditorCanvasScenes.Reset();
		for (const TPair<FString, TSharedPtr<FJsonValue>>& ScenePair : (*ScenesObject)->Values)
		{
			ESemanticScene Scene = ESemanticScene::LinearExecChain;
			if (!LexTryParseString(Scene, ScenePair.Key))
			{
				continue;
			}

			const TSharedPtr<FJsonObject> SceneObject = ScenePair.Value.IsValid() ? ScenePair.Value->AsObject() : nullptr;
			if (!SceneObject.IsValid())
			{
				continue;
			}

			const TSharedPtr<FJsonObject>* RoleCentersObject = nullptr;
			if (!SceneObject->TryGetObjectField(TEXT("role_centers"), RoleCentersObject) || !RoleCentersObject || !RoleCentersObject->IsValid())
			{
				continue;
			}

			FEditorCanvasSceneState SceneState;
			for (const TPair<FString, TSharedPtr<FJsonValue>>& RolePair : (*RoleCentersObject)->Values)
			{
				ENodeRole Role = ENodeRole::Unknown;
				if (IsDeprecatedIgnoredRoleText(RolePair.Key) || !LexTryParseString(Role, RolePair.Key) || Role == ENodeRole::Unknown)
				{
					continue;
				}

				FVector2D Center;
				if (TryReadVector2D(RolePair.Value.IsValid() ? RolePair.Value->AsObject() : nullptr, Center))
				{
					SceneState.RoleCenters.Add(Role, Center);
				}
			}
			OutRuleSet.EditorCanvasScenes.Add(Scene, SceneState);
		}
	}
}
```

- [ ] **Step 5: Remove legacy runtime consumption from `FRoleAnchorResolver`**

Replace `ResolveDataInputAnchor` with scalar-only fallback:

```cpp
FRoleAnchor FRoleAnchorResolver::ResolveDataInputAnchor(const FRuleSet& RuleSet, ENodeRole Role)
{
	FRoleAnchor Anchor;
	Anchor.OffsetFromConsumer = GetFallbackOffset(RuleSet, Role);
	Anchor.bFromEditorCanvas = false;
	return Anchor;
}
```

- [ ] **Step 6: Update old tests that used `EditorCanvasRoleCenters`**

In `MakeRuleSetWithCanvasOffsets()`, remove all `EditorCanvasRoleCenters.Add` calls.

Set scalar values to preserve the existing data-input placement intent:

```cpp
RuleSet.PureInputOffsetX = 220.0f;
RuleSet.VariableInputOffsetX = 230.0f;
RuleSet.InputPinRowSpacing = 90.0f;
```

For tests whose names explicitly mention editor canvas, rename them to scalar/scene projection tests in Task 3 rather than preserving the old premise. The old premise must not survive.

- [ ] **Step 7: Run GREEN for RuleSet JSON**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.RuleSetJson.RejectsLegacyEditorCanvasRoleCenters;BlueprintHelper.GraphLayout.RuleSetJson.RoundTripsSemanticSceneCenters;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_SemanticScene_RuleSet_GREEN_20260601_001"
```

Expected: build succeeds and both focused tests pass.

- [ ] **Step 8: Checkpoint**

Append result paths and pass/fail counts to `Debug/GraphLayout_LayoutRuleEditor_DraggablePersistence_20260601.md`. Do not run git commands.

---

## Task 3: Semantic Scene Metadata And Adapter

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutSemanticScene.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutSemanticScene.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`
- Modify: `Debug/GraphLayout_LayoutRuleEditor_DraggablePersistence_20260601.md`

- [ ] **Step 1: Add failing semantic scene tests**

Append:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_SemanticSceneCatalogContainsFiveScenes,
	"BlueprintHelper.GraphLayout.SemanticScene.CatalogContainsFiveScenes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_SemanticSceneCatalogContainsFiveScenes::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	const TArray<FSemanticSceneDefinition> Scenes = FSemanticSceneCatalog::GetAllScenes();
	TestEqual(TEXT("five scenes"), Scenes.Num(), 5);
	for (const FSemanticSceneDefinition& Scene : Scenes)
	{
		TestTrue(TEXT("scene has nodes"), Scene.Nodes.Num() >= 2);
		TestTrue(TEXT("scene has directed edges"), Scene.Edges.Num() >= 1);
		for (const FSemanticSceneNodeDefinition& Node : Scene.Nodes)
		{
			TestFalse(TEXT("node chinese tooltip is set"), Node.TooltipZh.IsEmpty());
			TestTrue(TEXT("default center exists"), Scene.DefaultRoleCenters.Contains(Node.Role));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_SemanticSceneProjectsPureDataCenters,
	"BlueprintHelper.GraphLayout.SemanticScene.ProjectsPureDataCenters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_SemanticSceneProjectsPureDataCenters::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	FEditorCanvasSceneState State;
	State.RoleCenters.Add(ENodeRole::VariableInput, FVector2D(100.0f, 100.0f));
	State.RoleCenters.Add(ENodeRole::OperatorOrCompare, FVector2D(360.0f, 150.0f));
	State.RoleCenters.Add(ENodeRole::PureFunction, FVector2D(660.0f, 210.0f));

	FSemanticSceneAdapter::ApplySceneStateToRuleSet(ESemanticScene::PureDataSubgraph, State, RuleSet, 1.0f);

	TestEqual(TEXT("variable offset projected"), RuleSet.VariableInputOffsetX, 260.0f);
	TestEqual(TEXT("pure offset projected"), RuleSet.PureInputOffsetX, 300.0f);
	TestEqual(TEXT("pin row projected"), RuleSet.InputPinRowSpacing, 60.0f);
	TestTrue(TEXT("scene state stored"), RuleSet.EditorCanvasScenes.Contains(ESemanticScene::PureDataSubgraph));
	return true;
}
```

- [ ] **Step 2: Run RED**

Run build. Expected: compile fails because `FSemanticSceneCatalog`, `FSemanticSceneDefinition`, `FSemanticSceneAdapter` do not exist.

- [ ] **Step 3: Create `BlueprintHelperGraphLayoutSemanticScene.h`**

Use this public API:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
enum class ESemanticSceneEdgeKind : uint8
{
	Exec,
	Data,
	Collision
};

struct FSemanticSceneNodeDefinition
{
	ENodeRole Role = ENodeRole::Unknown;
	FText Label;
	FText TooltipZh;
	bool bDraggable = true;
};

struct FSemanticSceneEdgeDefinition
{
	ENodeRole FromRole = ENodeRole::Unknown;
	ENodeRole ToRole = ENodeRole::Unknown;
	ESemanticSceneEdgeKind Kind = ESemanticSceneEdgeKind::Data;
	FLinearColor Color = FLinearColor::White;
};

struct FSemanticSceneDefinition
{
	ESemanticScene Scene = ESemanticScene::LinearExecChain;
	FText DisplayName;
	TArray<FSemanticSceneNodeDefinition> Nodes;
	TArray<FSemanticSceneEdgeDefinition> Edges;
	TMap<ENodeRole, FVector2D> DefaultRoleCenters;
};

class BLUEPRINTHELPER_API FSemanticSceneCatalog
{
public:
	static TArray<FSemanticSceneDefinition> GetAllScenes();
	static const FSemanticSceneDefinition* FindScene(ESemanticScene Scene);
	static FEditorCanvasSceneState BuildDefaultState(ESemanticScene Scene);
};

class BLUEPRINTHELPER_API FSemanticSceneAdapter
{
public:
	static FEditorCanvasSceneState ResolveSceneState(const FRuleSet& RuleSet, ESemanticScene Scene);
	static void ApplySceneStateToRuleSet(ESemanticScene Scene, const FEditorCanvasSceneState& State, FRuleSet& RuleSet, float Scale);
};
}
```

- [ ] **Step 4: Implement five scene definitions**

In `BlueprintHelperGraphLayoutSemanticScene.cpp`, implement `GetAllScenes()` with exactly five scene definitions:

- `LinearExecChain`: `EventEntry -> ExecNode`
- `PureDataSubgraph`: `VariableInput -> OperatorOrCompare -> PureFunction`
- `NodeInputCluster`: `VariableInput -> OperatorOrCompare -> PureFunction -> ExecNode`
- `MultiExecOutput`: `EventEntry -> ExecNode`, `EventEntry -> BranchControl`
- `Occupancy`: `ExecNode -> Comment`, `ExecNode -> AsyncNode`

Each node must have a Chinese tooltip. Example:

```cpp
{ ENodeRole::VariableInput,
  LOCTEXT("PureDataVariableInputLabel", "Data Leaf"),
  LOCTEXT("PureDataVariableInputTip", "数据叶子节点，例如变量 Get、Self 或 Literal。拖动会影响纯数据输入链最左侧节点的位置。"),
  true }
```

- [ ] **Step 5: Implement adapter projection**

`ResolveSceneState`:

- returns saved state from `RuleSet.EditorCanvasScenes` when present.
- otherwise returns `FSemanticSceneCatalog::BuildDefaultState(Scene)`.

`ApplySceneStateToRuleSet`:

- stores `State` into `RuleSet.EditorCanvasScenes[Scene]`.
- updates the same scalar fields currently updated in `SBlueprintHelperLayoutRuleCanvas::ExportCanvasToRuleSet`.
- uses the supplied `Scale` to convert canvas distance back to RuleSet units.
- clamps to the same ranges currently used by the canvas.

- [ ] **Step 6: Run GREEN**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.SemanticScene;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_SemanticScene_GREEN_20260601_001"
```

Expected: semantic scene tests pass.

- [ ] **Step 7: Checkpoint**

Append GREEN evidence to Debug. Do not run git commands.

---

## Task 4: LayoutRuleEditor Uses Semantic Scenes

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Layout/SBlueprintHelperLayoutRuleEditor.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Layout/SBlueprintHelperLayoutRuleEditor.cpp`
- Modify: `Debug/GraphLayout_LayoutRuleEditor_DraggablePersistence_20260601.md`

- [ ] **Step 1: Replace local canvas page enum with semantic scene**

In `SBlueprintHelperLayoutRuleEditor.cpp`, remove `ECanvasPage::RoleOverview`. Keep no RoleOverview branch.

The canvas state should become:

```cpp
BlueprintHelper::GraphLayout::ESemanticScene CurrentScene =
	BlueprintHelper::GraphLayout::ESemanticScene::LinearExecChain;
```

Replace `SetCanvasPage(...)` with:

```cpp
void SetScene(BlueprintHelper::GraphLayout::ESemanticScene InScene)
{
	if (CurrentScene == InScene)
	{
		return;
	}

	ExportCanvasToRuleSet();
	CurrentScene = InScene;
	BuildCanvasFromRuleSet();
	Invalidate(EInvalidateWidgetReason::Paint);
}
```

- [ ] **Step 2: Remove Role Overview toolbar button**

Delete the `Role Overview` button block from the toolbar.

The toolbar should expose only:

- `Linear Exec`
- `Pure Data`
- `Input Cluster`
- `Multi Exec`
- `Occupancy`
- `Align Exec Row`
- `Preview`

The `Preview` button is wired in Task 9; in this task it can be present but disabled only if the preview service has not been added yet.

- [ ] **Step 3: Build role nodes from scene metadata**

Replace `BuildRoleNodes()` switch body with logic based on `FSemanticSceneCatalog::FindScene(CurrentScene)`:

```cpp
TArray<BlueprintHelperLayoutRuleEditorLocal::FCanvasRoleNode> BuildRoleNodes() const
{
	TArray<BlueprintHelperLayoutRuleEditorLocal::FCanvasRoleNode> Result;
	const FSemanticSceneDefinition* Scene = FSemanticSceneCatalog::FindScene(CurrentScene);
	if (!Scene)
	{
		return Result;
	}

	for (const FSemanticSceneNodeDefinition& Node : Scene->Nodes)
	{
		Result.Add({ Node.Role, Node.Label, RoleCenters.FindRef(Node.Role), Node.bDraggable });
	}
	return Result;
}
```

- [ ] **Step 4: Build canvas state through adapter**

Replace `BuildCanvasFromRuleSet()` implementation with:

```cpp
void BuildCanvasFromRuleSet()
{
	using namespace BlueprintHelper::GraphLayout;
	RoleCenters = FSemanticSceneAdapter::ResolveSceneState(RuleSet, CurrentScene).RoleCenters;
}
```

- [ ] **Step 5: Export canvas state through adapter**

Replace the switch-heavy scalar projection in `ExportCanvasToRuleSet()` with:

```cpp
void ExportCanvasToRuleSet()
{
	using namespace BlueprintHelper::GraphLayout;
	FEditorCanvasSceneState State;
	State.RoleCenters = RoleCenters;
	FSemanticSceneAdapter::ApplySceneStateToRuleSet(CurrentScene, State, RuleSet, LayoutRuleEditorSettings.CanvasRuleScale);

	const FString UpdatedJson = FRuleSetJson::ExportString(RuleSet);
	if (RuleSetChangedDelegate.IsBound())
	{
		RuleSetChangedDelegate.Execute(UpdatedJson);
	}
}
```

- [ ] **Step 6: Update Align Exec Row**

`AlignExecRowToEntry()` should force `CurrentScene = ESemanticScene::LinearExecChain`, update `ExecNode.Y` to `EventEntry.Y`, and call `ExportCanvasToRuleSet()`.

- [ ] **Step 7: Add public canvas submit/accessors for Preview**

Add these public methods to `SBlueprintHelperLayoutRuleCanvas`:

```cpp
void CommitCurrentScene()
{
	ExportCanvasToRuleSet();
}

BlueprintHelper::GraphLayout::ESemanticScene GetScene() const
{
	return CurrentScene;
}
```

These methods are the only Preview integration surface needed from the draggable canvas.

- [ ] **Step 8: Build and focused smoke**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.SemanticScene;BlueprintHelper.GraphLayout.RuleSetJson;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_LayoutRuleEditorScene_GREEN_20260601_001"
```

Expected: build succeeds and scene/RuleSet tests pass.

- [ ] **Step 9: Manual editor persistence check**

Launch:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe" "D:\UEProjects\Template\Template.uproject"
```

Manual steps:

1. Open BlueprintHelper window.
2. Open Layout page.
3. Select `Pure Data`.
4. Drag `Data Leaf` and `Data Aggregate`.
5. Switch to `Multi Exec`.
6. Switch back to `Pure Data`.
7. Confirm nodes retain dragged positions.
8. Export or inspect `.blueprinthelper/GraphLayoutRules.json`.
9. Confirm JSON has `editor_canvas.scenes.pure_data_subgraph.role_centers`.
10. Confirm JSON does not contain root `editor_canvas.role_centers`.

- [ ] **Step 10: Checkpoint**

Append manual evidence and any blocker to Debug. Do not run git commands.

---

## Task 5: Canvas Arrows And Chinese Hover Tips

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Layout/SBlueprintHelperLayoutRuleEditor.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`
- Modify: `Debug/GraphLayout_LayoutRuleEditor_DraggablePersistence_20260601.md`

- [ ] **Step 1: Add metadata assertions**

Extend `FBlueprintHelperGraphLayout_SemanticSceneCatalogContainsFiveScenes` to assert:

```cpp
for (const FSemanticSceneEdgeDefinition& Edge : Scene.Edges)
{
	TestTrue(TEXT("edge from role has center"), Scene.DefaultRoleCenters.Contains(Edge.FromRole));
	TestTrue(TEXT("edge to role has center"), Scene.DefaultRoleCenters.Contains(Edge.ToRole));
	TestTrue(TEXT("edge color is visible"), Edge.Color.A > 0.0f);
}
```

- [ ] **Step 2: Draw edge list instead of switch relationships**

In `OnPaint`, remove the switch that calls `DrawRelationship` per page.

Replace it with:

```cpp
if (const FSemanticSceneDefinition* Scene = FSemanticSceneCatalog::FindScene(CurrentScene))
{
	for (const FSemanticSceneEdgeDefinition& Edge : Scene->Edges)
	{
		DrawRelationshipWithArrow(OutDrawElements, AllottedGeometry, LayerId + 1, Edge);
	}
}
```

- [ ] **Step 3: Implement arrow drawing**

Add:

```cpp
void DrawRelationshipWithArrow(
	FSlateWindowElementList& OutDrawElements,
	const FGeometry& AllottedGeometry,
	int32 LayerId,
	const BlueprintHelper::GraphLayout::FSemanticSceneEdgeDefinition& Edge) const
{
	const FVector2D From = RoleCenters.FindRef(Edge.FromRole);
	const FVector2D To = RoleCenters.FindRef(Edge.ToRole);
	if (From.IsZero() || To.IsZero())
	{
		return;
	}

	const FVector2D Delta = To - From;
	const float Length = Delta.Size();
	if (Length <= 1.0f)
	{
		return;
	}

	const FVector2D Direction = Delta / Length;
	const FVector2D Normal(-Direction.Y, Direction.X);
	const FVector2D End = To - Direction * (LayoutRuleEditorSettings.NodeSize.X * 0.5f + 6.0f);
	const FVector2D Start = From + Direction * (LayoutRuleEditorSettings.NodeSize.X * 0.5f + 6.0f);

	TArray<FVector2D> Points;
	Points.Add(Start);
	Points.Add(End);
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		Points,
		ESlateDrawEffect::None,
		Edge.Color,
		true,
		2.0f);

	TArray<FVector2D> ArrowLeft;
	ArrowLeft.Add(End);
	ArrowLeft.Add(End - Direction * 10.0f + Normal * 5.0f);
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), ArrowLeft, ESlateDrawEffect::None, Edge.Color, true, 2.0f);

	TArray<FVector2D> ArrowRight;
	ArrowRight.Add(End);
	ArrowRight.Add(End - Direction * 10.0f - Normal * 5.0f);
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), ArrowRight, ESlateDrawEffect::None, Edge.Color, true, 2.0f);
}
```

- [ ] **Step 4: Add hover role tracking**

Add member:

```cpp
TOptional<BlueprintHelper::GraphLayout::ENodeRole> HoveredRole;
```

In `OnMouseMove`, when not dragging, update hover:

```cpp
if (!DraggedRole.IsSet())
{
	const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	HoveredRole = HitTestRole(LocalPos);
	return FReply::Unhandled();
}
```

In `Construct`, set a dynamic tooltip after arguments are assigned:

```cpp
SetToolTipText(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SBlueprintHelperLayoutRuleCanvas::GetCurrentToolTipText)));
```

Add:

```cpp
FText GetCurrentToolTipText() const
{
	if (!HoveredRole.IsSet())
	{
		return FText::GetEmpty();
	}

	if (const FSemanticSceneDefinition* Scene = FSemanticSceneCatalog::FindScene(CurrentScene))
	{
		for (const FSemanticSceneNodeDefinition& Node : Scene->Nodes)
		{
			if (Node.Role == HoveredRole.GetValue())
			{
				return Node.TooltipZh;
			}
		}
	}
	return FText::GetEmpty();
}
```

- [ ] **Step 5: Run focused tests and build**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.SemanticScene.CatalogContainsFiveScenes;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_CanvasMetadata_GREEN_20260601_001"
```

Expected: build and metadata tests pass.

- [ ] **Step 6: Manual visual check**

In Unreal Editor Layout page:

1. Open each of the five scenes.
2. Confirm every relationship line has a visible arrow.
3. Hover every draggable node.
4. Confirm Chinese Tips appear and match the current scene.

- [ ] **Step 7: Checkpoint**

Append visual check notes to Debug. Do not run git commands.

---

## Task 6: Preview Sample Descriptor And Layout Plan

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewTypes.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSampleFactory.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSampleFactory.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`
- Modify: `Debug/GraphLayout_LayoutRuleEditor_DraggablePersistence_20260601.md`

- [ ] **Step 1: Add failing preview sample tests**

Add includes:

```cpp
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSampleFactory.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewTypes.h"
```

Append:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewSampleFactoryBuildsFiveComplexSamples,
	"BlueprintHelper.GraphLayout.Preview.SampleFactoryBuildsFiveComplexSamples",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewSampleFactoryBuildsFiveComplexSamples::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	for (const FSemanticSceneDefinition& Scene : FSemanticSceneCatalog::GetAllScenes())
	{
		FGraphLayoutPreviewSample Sample;
		FString Error;
		TestTrue(FString::Printf(TEXT("sample builds for %s"), ToString(Scene.Scene)), FGraphLayoutPreviewSampleFactory::BuildSample(Scene.Scene, Sample, Error));
		TestTrue(TEXT("sample has nodes"), Sample.Snapshot.Nodes.Num() >= 4);
		TestTrue(TEXT("sample has materialization nodes"), Sample.Nodes.Num() >= Sample.Snapshot.Nodes.Num());
		TestTrue(TEXT("sample has links"), Sample.Links.Num() >= 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewSampleFactoryProducesLayoutPlan,
	"BlueprintHelper.GraphLayout.Preview.SampleFactoryProducesLayoutPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewSampleFactoryProducesLayoutPlan::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(TEXT("pure data sample builds"), FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::PureDataSubgraph, Sample, Error));
	const FLayoutPlan Plan = FSolver::Solve(Sample.Snapshot, RuleSet);
	TestTrue(TEXT("layout produces placements"), Plan.Placements.Num() >= 4);
	TestEqual(TEXT("no issues"), Plan.Issues.Num(), 0);
	return true;
}
```

- [ ] **Step 2: Run RED**

Run build. Expected: compile fails because preview types/factory do not exist.

- [ ] **Step 3: Create preview types**

`BlueprintHelperGraphLayoutPreviewTypes.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
enum class EGraphLayoutPreviewNodeFactory : uint8
{
	CustomEvent,
	CallFunction,
	IfThenElse,
	ExecutionSequence,
	MakeArray,
	Self,
	Comment,
	GenericK2
};

struct FGraphLayoutPreviewNodeSpec
{
	FString NodeId;
	FString Title;
	EGraphLayoutPreviewNodeFactory Factory = EGraphLayoutPreviewNodeFactory::GenericK2;
	ENodeRole Role = ENodeRole::Unknown;
	FVector2D Size = FVector2D(220.0f, 96.0f);
};

struct FGraphLayoutPreviewLinkSpec
{
	FString FromNodeId;
	FString FromPinName;
	FString ToNodeId;
	FString ToPinName;
	bool bExec = false;
};

struct FGraphLayoutPreviewSample
{
	ESemanticScene Scene = ESemanticScene::LinearExecChain;
	FGraphSnapshot Snapshot;
	TArray<FGraphLayoutPreviewNodeSpec> Nodes;
	TArray<FGraphLayoutPreviewLinkSpec> Links;
};

struct FGraphLayoutPreviewBuildResult
{
	uint64 JobId = 0;
	bool bSuccess = false;
	FString Error;
	FGraphLayoutPreviewSample Sample;
	FLayoutPlan LayoutPlan;
};
}
```

- [ ] **Step 4: Create sample factory API**

`BlueprintHelperGraphLayoutPreviewSampleFactory.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewTypes.h"

namespace BlueprintHelper::GraphLayout
{
class BLUEPRINTHELPER_API FGraphLayoutPreviewSampleFactory
{
public:
	static bool BuildSample(ESemanticScene Scene, FGraphLayoutPreviewSample& OutSample, FString& OutError);
};
}
```

- [ ] **Step 5: Implement fixed complex samples**

`BuildSample` must dispatch to five private builders:

- Linear: event, reset call, set-like generic node, print call, async-like call.
- PureData: self/leaf nodes, operator generic, make array, consumer call.
- NodeInputCluster: consumer call plus three pure chains of different lengths.
- MultiExec: custom event, sequence, branch, two print calls, completed print.
- Occupancy: blocker comment/generic nodes, candidate exec chain, fallback generic node.

Each sample must populate both:

- `Sample.Snapshot` for solver.
- `Sample.Nodes` / `Sample.Links` for materializer.

Use `FPinSnapshot` names that materializer can later match, for example:

- exec input: `execute`
- exec output: `then`, `Then_0`, `Then_1`, `Completed`
- data input: `In`, `Array`, `Condition`
- data output: `ReturnValue`, `Array`, `Value`

- [ ] **Step 6: Run GREEN**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.Preview.SampleFactory;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_PreviewSample_GREEN_20260601_001"
```

Expected: sample factory tests pass.

- [ ] **Step 7: Checkpoint**

Append evidence to Debug. Do not run git commands.

---

## Task 7: Async Preview Service

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewService.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`
- Modify: `Debug/GraphLayout_LayoutRuleEditor_DraggablePersistence_20260601.md`

- [ ] **Step 1: Add failing preview service tests**

Append:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewServiceBuildsPureDataResult,
	"BlueprintHelper.GraphLayout.Preview.ServiceBuildsPureDataResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewServiceBuildsPureDataResult::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewService Service;
	FGraphLayoutPreviewRequest Request;
	Request.Scene = ESemanticScene::PureDataSubgraph;
	Request.RuleSetJson = FRuleSetJson::ExportString(FRuleSet());

	FGraphLayoutPreviewBuildResult Result;
	TestTrue(TEXT("sync test helper builds"), Service.BuildPreviewDataForTest(Request, Result));
	TestTrue(TEXT("result success"), Result.bSuccess);
	TestEqual(TEXT("scene"), Result.Sample.Scene, ESemanticScene::PureDataSubgraph);
	TestTrue(TEXT("has layout"), Result.LayoutPlan.Placements.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewServiceCancelsStaleJob,
	"BlueprintHelper.GraphLayout.Preview.ServiceCancelsStaleJob",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewServiceCancelsStaleJob::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewService Service;
	const uint64 FirstJob = Service.StartPreviewBuild(FGraphLayoutPreviewRequest{ ESemanticScene::LinearExecChain, FRuleSetJson::ExportString(FRuleSet()) });
	Service.Cancel(FirstJob);
	TestTrue(TEXT("job marked stale"), Service.IsJobCancelledForTest(FirstJob));
	return true;
}
```

- [ ] **Step 2: Create service API**

`BlueprintHelperGraphLayoutPreviewService.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewTypes.h"

namespace BlueprintHelper::GraphLayout
{
struct FGraphLayoutPreviewRequest
{
	ESemanticScene Scene = ESemanticScene::LinearExecChain;
	FString RuleSetJson;
};

DECLARE_DELEGATE_OneParam(FGraphLayoutPreviewBuildCompleted, const FGraphLayoutPreviewBuildResult&);

class BLUEPRINTHELPER_API FGraphLayoutPreviewService
{
public:
	uint64 StartPreviewBuild(const FGraphLayoutPreviewRequest& Request, FGraphLayoutPreviewBuildCompleted OnCompleted = FGraphLayoutPreviewBuildCompleted());
	void Cancel(uint64 JobId);
	void CancelAll();

	bool BuildPreviewDataForTest(const FGraphLayoutPreviewRequest& Request, FGraphLayoutPreviewBuildResult& OutResult) const;
	bool IsJobCancelledForTest(uint64 JobId) const;

private:
	uint64 NextJobId = 1;
	TSet<uint64> CancelledJobs;
	FCriticalSection CancelledJobsLock;
};
}
```

- [ ] **Step 3: Implement pure-data worker build**

`BuildPreviewDataForTest` must:

1. Import `Request.RuleSetJson` via `FRuleSetJson::ImportString`.
2. Build sample via `FGraphLayoutPreviewSampleFactory::BuildSample`.
3. Run `FSolver::Solve`.
4. Fill `FGraphLayoutPreviewBuildResult`.

`StartPreviewBuild` must:

1. Allocate a job id on GameThread caller.
2. Copy request into worker lambda.
3. Run `BuildPreviewDataForTest` inside `Async(EAsyncExecution::ThreadPool, ...)`.
4. Post completion back to GameThread with `AsyncTask(ENamedThreads::GameThread, ...)`.
5. Drop completion if `CancelledJobs` contains job id.

Worker lambda must not touch UObjects.

- [ ] **Step 4: Run GREEN**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.Preview.Service;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_PreviewService_GREEN_20260601_001"
```

Expected: preview service tests pass.

- [ ] **Step 5: Checkpoint**

Append evidence to Debug. Do not run git commands.

---

## Task 8: GameThread Preview Materializer

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewMaterializer.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewMaterializer.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`
- Modify: `Debug/GraphLayout_LayoutRuleEditor_DraggablePersistence_20260601.md`

- [ ] **Step 1: Add failing materializer test**

Append:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewMaterializerCreatesTransientGraph,
	"BlueprintHelper.GraphLayout.Preview.MaterializerCreatesTransientGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewMaterializerCreatesTransientGraph::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	TestTrue(TEXT("runs on game thread"), IsInGameThread());

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(TEXT("sample builds"), FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::MultiExecOutput, Sample, Error));

	FRuleSet RuleSet;
	const FLayoutPlan Plan = FSolver::Solve(Sample.Snapshot, RuleSet);

	FGraphLayoutPreviewMaterializer Materializer;
	FGraphLayoutPreviewMaterializerResult Result;
	TestTrue(TEXT("materializes"), Materializer.MaterializeForTest(Sample, Plan, Result));
	TestNotNull(TEXT("preview blueprint"), Result.PreviewBlueprint.Get());
	TestNotNull(TEXT("preview graph"), Result.PreviewGraph.Get());
	TestTrue(TEXT("graph has nodes"), Result.PreviewGraph.IsValid() && Result.PreviewGraph->Nodes.Num() >= Sample.Nodes.Num());
	TestFalse(TEXT("preview graph is not editable"), Result.PreviewGraph.IsValid() && Result.PreviewGraph->bEditable);
	return true;
}
```

- [ ] **Step 2: Create materializer API**

`BlueprintHelperGraphLayoutPreviewMaterializer.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewTypes.h"
#include "UObject/StrongObjectPtr.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;

namespace BlueprintHelper::GraphLayout
{
struct FGraphLayoutPreviewMaterializerResult
{
	TStrongObjectPtr<UBlueprint> PreviewBlueprint;
	TStrongObjectPtr<UEdGraph> PreviewGraph;
	FString Error;
};

class BLUEPRINTHELPER_API FGraphLayoutPreviewMaterializer
{
public:
	void Begin(const FGraphLayoutPreviewSample& Sample, const FLayoutPlan& LayoutPlan);
	bool Tick(float MaxMillisecondsPerFrame);
	void Cancel();
	bool IsComplete() const;
	const FGraphLayoutPreviewMaterializerResult& GetResult() const;

	bool MaterializeForTest(const FGraphLayoutPreviewSample& Sample, const FLayoutPlan& LayoutPlan, FGraphLayoutPreviewMaterializerResult& OutResult);

private:
	FGraphLayoutPreviewSample PendingSample;
	FLayoutPlan PendingPlan;
	FGraphLayoutPreviewMaterializerResult Result;
	int32 NextNodeIndex = 0;
	int32 NextLinkIndex = 0;
	bool bCancelled = false;
	bool bComplete = false;
	TMap<FString, UEdGraphNode*> NodeById;
};
}
```

- [ ] **Step 3: Implement transient blueprint and graph creation**

Use the same transient blueprint pattern as Review graph presenter:

```cpp
UBlueprint* PreviewBlueprint = NewObject<UBlueprint>(
	GetTransientPackage(),
	MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), FName(TEXT("BlueprintHelperLayoutPreviewBP"))),
	RF_Transient);

UEdGraph* PreviewGraph = NewObject<UEdGraph>(
	PreviewBlueprint,
	MakeUniqueObjectName(PreviewBlueprint, UEdGraph::StaticClass(), FName(TEXT("GraphLayoutPreview"))),
	RF_Transient);
PreviewGraph->Schema = UEdGraphSchema_K2::StaticClass();
PreviewGraph->bEditable = false;
PreviewBlueprint->UbergraphPages.Add(PreviewGraph);
```

- [ ] **Step 4: Implement minimal K2 node factory**

For v1 materialization, support these factories:

- `CustomEvent`: `UK2Node_CustomEvent`
- `IfThenElse`: `UK2Node_IfThenElse`
- `ExecutionSequence`: `UK2Node_ExecutionSequence`
- `MakeArray`: `UK2Node_MakeArray`
- `Self`: `UK2Node_Self`
- `CallFunction`: `UK2Node_CallFunction` using `UKismetSystemLibrary::PrintString` when an exec node is needed
- `Comment`: `UEdGraphNode_Comment`
- `GenericK2`: plain `UEdGraphNode` with pins from descriptor when a concrete K2 factory is not available

Every created node must:

```cpp
Node->CreateNewGuid();
Node->NodePosX = FMath::RoundToInt(TargetPosition.X);
Node->NodePosY = FMath::RoundToInt(TargetPosition.Y);
Graph->AddNode(Node, false, false);
Node->AllocateDefaultPins();
```

If a concrete K2 node does not expose the sample pin names, create a `GenericK2` fallback for that sample node and record a warning in `Result.Error` only if no pins can be represented. Do not block the whole preview for one unsupported factory unless the graph cannot be created.

- [ ] **Step 5: Implement pin linking by name**

Use `UEdGraphSchema_K2::TryCreateConnection`:

```cpp
const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(PreviewGraph->GetSchema());
if (Schema && FromPin && ToPin)
{
	Schema->TryCreateConnection(FromPin, ToPin);
}
```

Pin lookup must match `FGraphLayoutPreviewLinkSpec` by `PinName.ToString()`.

If a materialized node does not already have a pin named by a link spec, create a preview-only pin using the corresponding `FPinSnapshot` from `Sample.Snapshot`. Exec pins use `UEdGraphSchema_K2::PC_Exec`; data pins use the snapshot category when present and fall back to `UEdGraphSchema_K2::PC_Object`.

- [ ] **Step 6: Run GREEN**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.Preview.MaterializerCreatesTransientGraph;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_PreviewMaterializer_GREEN_20260601_001"
```

Expected: materializer creates a transient graph on GameThread.

- [ ] **Step 7: Checkpoint**

Append evidence to Debug. Do not run git commands.

---

## Task 9: LayoutRuleEditor Preview Mode Integration

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Layout/SBlueprintHelperLayoutRuleEditor.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Layout/SBlueprintHelperLayoutRuleEditor.cpp`
- Modify: `Debug/GraphLayout_LayoutRuleEditor_DraggablePersistence_20260601.md`

- [ ] **Step 1: Add preview state to header**

In `SBlueprintHelperLayoutRuleEditor`, add:

```cpp
enum class ELayoutRuleEditorMode : uint8
{
	Edit,
	PreviewLoading,
	PreviewMaterializing,
	PreviewReady,
	PreviewError
};

FReply OnPreviewClicked();
FReply OnReturnToEditClicked();
void HandlePreviewBuildCompleted(const BlueprintHelper::GraphLayout::FGraphLayoutPreviewBuildResult& Result);
TSharedRef<SWidget> BuildSceneToolbar();
TSharedRef<SWidget> BuildEditCanvasArea();
TSharedRef<SWidget> BuildPreviewArea();
void RefreshWorkspaceArea();
virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
```

Add members:

```cpp
ELayoutRuleEditorMode EditorMode = ELayoutRuleEditorMode::Edit;
uint64 ActivePreviewJobId = 0;
FString PreviewStatusMessage;
TSharedPtr<SBox> WorkspaceBox;
TSharedPtr<SGraphEditor> PreviewGraphEditor;
TUniquePtr<BlueprintHelper::GraphLayout::FGraphLayoutPreviewService> PreviewService;
TUniquePtr<BlueprintHelper::GraphLayout::FGraphLayoutPreviewMaterializer> PreviewMaterializer;
```

- [ ] **Step 2: Initialize preview services**

In `Construct`, initialize:

```cpp
PreviewService = MakeUnique<BlueprintHelper::GraphLayout::FGraphLayoutPreviewService>();
PreviewMaterializer = MakeUnique<BlueprintHelper::GraphLayout::FGraphLayoutPreviewMaterializer>();
```

Wrap the current canvas slot in `WorkspaceBox` so it can be replaced:

```cpp
SAssignNew(WorkspaceBox, SBox)
[
	BuildEditCanvasArea()
]
```

- [ ] **Step 3: Hide scene buttons in preview mode**

Move the scene buttons into `BuildSceneToolbar()`.

`BuildSceneToolbar()` returns `SNullWidget::NullWidget` unless `EditorMode == ELayoutRuleEditorMode::Edit`.

- [ ] **Step 4: Implement Preview button**

`OnPreviewClicked()`:

1. Calls `RuleCanvas->CommitCurrentScene()`.
2. Validates `RuleSetJson`.
3. Sets `EditorMode = PreviewLoading`.
4. Sets `PreviewStatusMessage = TEXT("正在创建预览...")`.
5. Calls `RefreshWorkspaceArea()`.
6. Starts preview build:

```cpp
BlueprintHelper::GraphLayout::FGraphLayoutPreviewRequest Request;
Request.Scene = RuleCanvas->GetScene();
Request.RuleSetJson = RuleSetJson;
ActivePreviewJobId = PreviewService->StartPreviewBuild(
	Request,
	BlueprintHelper::GraphLayout::FGraphLayoutPreviewBuildCompleted::CreateSP(
		this,
		&SBlueprintHelperLayoutRuleEditor::HandlePreviewBuildCompleted));
```

- [ ] **Step 5: Implement Preview shell**

`BuildPreviewArea()` should show:

- `返回编辑` button.
- current scene name.
- status text.
- `SGraphEditor` only when `EditorMode == PreviewReady`.

While loading/materializing, show a simple text status in the same area. Do not block the UI thread.

- [ ] **Step 6: Handle preview build completion**

`HandlePreviewBuildCompleted`:

1. Ignore stale job id.
2. If failed, set `PreviewError`.
3. If success, call `PreviewMaterializer->Begin(Result.Sample, Result.LayoutPlan)`.
4. Set `EditorMode = PreviewMaterializing`.
5. `RefreshWorkspaceArea()`.

- [ ] **Step 7: Pump materializer in `Tick`**

In `Tick`:

```cpp
if (EditorMode == ELayoutRuleEditorMode::PreviewMaterializing && PreviewMaterializer)
{
	const bool bStillRunning = PreviewMaterializer->Tick(LayoutRuleEditorSettings.MaxMillisecondsPerFrame);
	if (!bStillRunning && PreviewMaterializer->IsComplete())
	{
		const auto& Result = PreviewMaterializer->GetResult();
		if (Result.PreviewGraph.IsValid())
		{
			FGraphAppearanceInfo Appearance;
			Appearance.CornerText = FText::FromString(TEXT("GraphLayout Preview"));
			Appearance.InstructionText = FText::FromString(TEXT("Layout Preview"));
			Appearance.ReadOnlyText = FText::FromString(TEXT("Preview Only"));

			SAssignNew(PreviewGraphEditor, SGraphEditor)
				.IsEditable(false)
				.DisplayAsReadOnly(false)
				.GraphToEdit(Result.PreviewGraph.Get())
				.Appearance(Appearance)
				.ShowGraphStateOverlay(false);

			EditorMode = ELayoutRuleEditorMode::PreviewReady;
			RefreshWorkspaceArea();
		}
		else
		{
			EditorMode = ELayoutRuleEditorMode::PreviewError;
			PreviewStatusMessage = Result.Error.IsEmpty() ? TEXT("预览图创建失败。") : Result.Error;
			RefreshWorkspaceArea();
		}
	}
}
```

- [ ] **Step 8: Implement return to edit**

`OnReturnToEditClicked()`:

1. Cancels `ActivePreviewJobId`.
2. Cancels materializer.
3. Resets `PreviewGraphEditor`.
4. Sets `EditorMode = Edit`.
5. Calls `RefreshWorkspaceArea()`.

- [ ] **Step 9: Build**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

Expected: build succeeds.

- [ ] **Step 10: Manual preview E2E**

Launch Unreal Editor and perform:

1. Open BlueprintHelper Layout page.
2. Select `Pure Data`.
3. Drag at least two semantic nodes.
4. Click `Preview`.
5. Confirm scene buttons disappear immediately.
6. Confirm the area switches to preview shell before graph is complete.
7. Confirm `SGraphEditor` appears with native graph nodes.
8. Confirm UI remains responsive while materializing.
9. Click `返回编辑`.
10. Confirm draggable canvas returns with dragged positions preserved.

- [ ] **Step 11: Checkpoint**

Append manual E2E evidence to Debug. Do not run git commands.

---

## Task 10: Broad Verification And Report

**Files:**
- Modify: `Debug/GraphLayout_LayoutRuleEditor_DraggablePersistence_20260601.md`
- Create/Modify: `BlueprintHelper/Develop/Report/BlueprintHelper_GraphLayout_SemanticVisualPreview_Report_20260601_CN.md`

- [ ] **Step 1: Run full GraphLayout automation**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_SemanticVisualPreview_Full_20260601_001"
```

Expected: GraphLayout tests pass.

- [ ] **Step 2: Run focused UI/preview tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.Preview;BlueprintHelper.GraphLayout.SemanticScene;BlueprintHelper.GraphLayout.RuleSetJson;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_SemanticVisualPreview_Focused_20260601_001"
```

Expected: focused tests pass.

- [ ] **Step 3: Run real Editor E2E**

Use the manual E2E from Task 9 for all five scenes:

1. Drag one node in each scene.
2. Switch away and back.
3. Confirm position persists.
4. Preview each scene.
5. Confirm scene buttons hide in Preview.
6. Confirm `SGraphEditor` renders native transient graph.
7. Return to Edit.

Record scene-by-scene result in Debug.

- [ ] **Step 4: Create report**

Create `BlueprintHelper/Develop/Report/BlueprintHelper_GraphLayout_SemanticVisualPreview_Report_20260601_CN.md`:

```markdown
# GraphLayout Semantic Visual Preview Report

Date: 2026-06-01

## 改动原因

- LayoutRuleEditor 五个语义页缺少 scene-scoped role center 持久化，切页后会回到默认值。
- 新增箭头、中文 Hover Tips、原生 GraphPanel Preview 都需要同一套 semantic scene model。
- 旧 `editor_canvas.role_centers` 不符合当前架构要求，必须移除。

## 改动范围

- GraphLayout RuleSet JSON scene state。
- Semantic scene metadata / adapter。
- LayoutRuleEditor canvas scene persistence、箭头、中文 Tips。
- Preview sample factory、async service、GameThread materializer。
- LayoutRuleEditor Preview UI state。
- GraphLayout automation tests。

## 验证结果

- Build:
- Focused automation:
- Full GraphLayout automation:
- Real Editor E2E:

## 结果

- 旧 `editor_canvas.role_centers` 已移除。
- 五个语义页拥有 scene-scoped role centers。
- Preview 使用 transient graph，不影响真实资产。
```

Fill the verification bullets with actual report paths and pass/fail counts.

- [ ] **Step 5: Final status check**

Run:

```powershell
git status --short
git status --ignored --short -- Debug\GraphLayout_LayoutRuleEditor_DraggablePersistence_20260601.md
```

Expected:

- Source, test, design/report/debug files are visible as modified or untracked.
- `Debug/...` may remain ignored and requires manual `git add -f` by the user.
- No `git add`, `git commit`, or `git push` has been executed by the agent.

---

## Final Manual Commit Guidance

After implementation and verification, the agent must not commit. The final response should give a suggested commit message and manual commands only.

Suggested commit message shape:

```text
新增内容：
1. 新增 GraphLayout semantic scene model 和原生 Preview

修复内容：
1. 移除旧 editor_canvas.role_centers 并修复语义页拖拽持久化
```

Manual commands should include only files changed by this work, and `Debug/...` must use `git add -f` if it remains ignored.
