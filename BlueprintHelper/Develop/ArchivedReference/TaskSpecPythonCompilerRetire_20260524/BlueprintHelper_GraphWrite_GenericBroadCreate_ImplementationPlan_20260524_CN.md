# GraphWrite Generic Broad Create Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add real GraphWrite `Create` support under `GenericAssetStructControlActionCluster` for explicit broad-create operations without reusing struct construct/deconstruct or legacy parsed-node fallback.

**Architecture:** Keep first-level dispatch on `SpawnerClusterKind=GenericAssetStructControlAction`; broad create is a second-stage semantic selected by `create_operation`. Create resolution must consume `ActionContext` evidence, return UE NodeSpawner evidence, and invoke nodes through `FBlueprintHelperActionNodeSpawnerAdapter`; it must not route through `AssetFactory`, parsed DTOs, or struct fallback.

**Tech Stack:** UE 5.6 C++, GraphWrite SemanticIR, ActionContextPipeline, ActionResolutionCore, GenericAssetStructControlActionCluster, UBlueprintNodeSpawner, AgentFace task-core TypeScript/Python compilers, Unreal Automation Tests, BlueprintHelper CLI.

---

## Execution Result - 2026-05-24

Status: implemented and verified for the first broad-create slice.

- `Create` now enters `GenericAssetStructControlActionCluster` through explicit `create_operation` evidence.
- Supported operations: `spawn_actor`, `create_widget`, `construct_object`, `make_array`, `make_map`, `make_set`.
- `asset_action` remains honest: without projected selected ActionDatabase/spawner evidence it returns `needs_more_semantic_context` and does not fake success.
- Struct `construct/deconstruct` remains separate from broad create.
- Focused UE resolver coverage lives in `BlueprintHelperGenericCreateActionResolverTests.cpp`; broader cluster no-fallback coverage remains in `BlueprintHelperGenericAssetStructControlActionClusterTests.cpp`.

Verification evidence recorded after implementation:

- `npm.cmd run build` in `AgentFaceService/task-core`: pass.
- `python -m unittest discover -s python/tests -t python` in `AgentFaceService/task-core`: pass, 69 tests.
- `npm.cmd run test:node` in `AgentFaceService/task-core`: pass, 162 tests / 12 suites.
- UE 5.6 `Build.bat TemplateEditor Win64 Development`: `Result: Succeeded`.
- Unreal Automation `BlueprintHelper.GraphWrite.ActionResolution.Contract`: pass, 8 tests.
- Unreal Automation `BlueprintHelper.GraphWrite.ActionResolution.Generic.Create`: pass, 3 tests.
- Unreal Automation `BlueprintHelper.GraphWrite.ActionResolution.Generic`: pass, 8 tests.
- Unreal Automation `BlueprintHelper.GraphWrite.LegacyMainline`: pass, 8 tests.
- Unreal Automation `BlueprintHelper.GraphWrite`: pass, 158 tests, exit code 0.

---

## Scope

This plan implements item 1 from the 2026-05-24 truth audit: Generic broad `create`.

Supported first slice:

| `create_operation` | Node family | Required evidence | Expected result |
|---|---|---|---|
| `spawn_actor` | `UK2Node_SpawnActorFromClass` | actor class path or class pin type, graph allows spawn | selected spawner and stable id `generic_create:spawn_actor:<class>` |
| `create_widget` | `UK2Node_CreateWidget` | widget class path, owning player context if required by graph | selected spawner and stable id `generic_create:create_widget:<class>` |
| `construct_object` | `UK2Node_GenericCreateObject` | object class path and outer context evidence | selected spawner and stable id `generic_create:construct_object:<class>` |
| `make_array` | `UK2Node_MakeArray` | element pin type | selected spawner and stable id `generic_create:make_array:<pin_type>` |
| `make_map` | `UK2Node_MakeMap` | key/value pin types | selected spawner and stable id `generic_create:make_map:<key>:<value>` |
| `make_set` | `UK2Node_MakeSet` | element pin type | selected spawner and stable id `generic_create:make_set:<pin_type>` |
| `asset_action` | `UBlueprintAssetNodeSpawner` or explicit unsupported diagnostic | asset path plus action category evidence | resolved only when UE exposes a stable selected spawner |

Out of scope:

- Do not treat `construct/deconstruct` struct operations as `Create`.
- Do not add `create_asset` behavior to GraphWrite; asset creation remains in the AssetFactory tool cluster.
- Do not reuse `BlueprintGraphJsonParser` node strings or `FParsedNode`.
- Do not add broad create success without selected spawner evidence.

## File Structure

- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`
  - Add `FString CreateOperation` and create evidence fields to `FBlueprintHelperActionSemanticConstraints`.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h`
  - Add `CreateOperation`, `ClassPath`, `AssetPath`, `ContainerElementPinType`, `ContainerKeyPinType`, and `ContainerValuePinType` to `FBlueprintHelperActionContextDemand`.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
  - Map public/internal create syntax into `Create` demand and required evidence.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp`
  - Project create fields into `FBlueprintHelperActionSemanticConstraints` and semantic hash.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.h`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.cpp`
  - Classify `Create + create_operation` as `NodeSpawnerCandidate`; classify missing evidence as `NeedsMoreSemanticContext`.
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericCreateActionResolver.h`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericCreateActionResolver.cpp`
  - Resolve explicit create operations into selected spawner evidence.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionCluster.cpp`
  - Route `Create` with `create_operation` to `FBlueprintHelperGenericCreateActionResolver`.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
  - Add create fragment builder path that uses `FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner`.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py`
  - Accept `kind:"create"` and lower explicit `create_operation` fields into canonical GraphWrite body.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericAssetStructControlActionClusterTests.cpp`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericCreateActionResolverTests.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`

---

## Task 1: Lock RED Tests For Broad Create Semantics

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericAssetStructControlActionClusterTests.cpp`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericCreateActionResolverTests.cpp`

- [x] **Step 1.1: Replace the old no-success assertion for `Create` only**

In `FBlueprintHelperGenericActionRejectsUnsupportedWideSurfaceWithoutStructFallbackTest`, remove `EBlueprintHelperActionSemanticKind::Create` from the `UnsupportedKinds` array. Keep `Convert` and `Schedule` in that test until item 2 is implemented.

```cpp
const EBlueprintHelperActionSemanticKind UnsupportedKinds[] = {
	EBlueprintHelperActionSemanticKind::Convert,
	EBlueprintHelperActionSemanticKind::Schedule
};
```

- [x] **Step 1.2: Add RED tests for missing create evidence**

Add this test to `BlueprintHelperGenericCreateActionResolverTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericCreateRequiresOperationTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Create.RequiresOperation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericCreateRequiresOperationTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericActionTestBlueprint();
	UEdGraph* Graph = GetGenericActionTestGraph(Blueprint);

	FBlueprintHelperActionResolutionRequest Request =
		MakeGenericActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Create);
	Request.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Create;

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(Request);

	TestEqual(TEXT("missing create operation status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("missing create operation error"), Result.ErrorCode, FString(TEXT("needs_more_semantic_context")));
	TestFalse(TEXT("missing create operation has no spawner"), Result.SelectedSpawner.IsValid());
	return true;
}
```

- [x] **Step 1.3: Add RED tests for concrete create operations**

Add this test next to Step 1.2:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericCreateConcreteOperationsTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Create.ConcreteOperations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericCreateConcreteOperationsTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericActionTestBlueprint();
	UEdGraph* Graph = GetGenericActionTestGraph(Blueprint);

	struct FCase
	{
		FString Operation;
		FString ClassPath;
		FString ElementPinType;
		FString ExpectedStableIdPart;
	};

	const FCase Cases[] = {
		{TEXT("spawn_actor"), TEXT("/Script/Engine.StaticMeshActor"), FString(), TEXT("generic_create:spawn_actor")},
		{TEXT("create_widget"), TEXT("/Script/UMG.UserWidget"), FString(), TEXT("generic_create:create_widget")},
		{TEXT("construct_object"), TEXT("/Script/Engine.Object"), FString(), TEXT("generic_create:construct_object")},
		{TEXT("make_array"), FString(), TEXT("object|/Script/Engine.Actor"), TEXT("generic_create:make_array")}
	};

	for (const FCase& Case : Cases)
	{
		FBlueprintHelperActionResolutionRequest Request =
			MakeGenericActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Create);
		Request.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Create;
		Request.Semantic.CreateOperation = Case.Operation;
		Request.Semantic.TargetPath = Case.ClassPath;
		Request.Semantic.ArgumentTypes.Add(TEXT("element"), Case.ElementPinType);

		const FBlueprintHelperActionResolutionResult Result =
			FBlueprintHelperActionResolutionCore::Resolve(Request);

		TestEqual(FString::Printf(TEXT("%s status"), *Case.Operation), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
		TestTrue(FString::Printf(TEXT("%s stable id"), *Case.Operation), Result.SelectedStableId.Contains(Case.ExpectedStableIdPart));
		TestTrue(FString::Printf(TEXT("%s selected spawner"), *Case.Operation), Result.SelectedSpawner.IsValid());
		TestEqual(FString::Printf(TEXT("%s cluster"), *Case.Operation), Result.ClusterKind, EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction);
		TestTrue(FString::Printf(TEXT("%s match reason"), *Case.Operation), Result.MatchReason.Contains(TEXT("create_operation=")));
	}

	return true;
}
```

- [x] **Step 1.4: Run the RED create resolver tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic.Create;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_GenericCreate_RED_001'
```

Expected before implementation: tests fail because `CreateOperation` does not exist or `Create` still returns `unsupported_generic_action_provider_boundary`.

## Task 2: Add Create Semantic Fields And Context Projection

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp`

- [x] **Step 2.1: Add create fields to semantic constraints**

Add these fields to both `FBlueprintHelperActionSemanticConstraints` and `FBlueprintHelperActionContextDemand`:

```cpp
FString CreateOperation;
FString ClassPath;
FString AssetPath;
FBlueprintHelperCallFunctionPinType ContainerElementPinType;
FBlueprintHelperCallFunctionPinType ContainerKeyPinType;
FBlueprintHelperCallFunctionPinType ContainerValuePinType;
```

- [x] **Step 2.2: Populate create demand**

In `FBlueprintHelperActionContextDemandCollector::ApplyDemandKinds`, set:

```cpp
case EBlueprintHelperActionSemanticKind::Create:
	Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Demand.SemanticFamily = EBlueprintHelperActionSemanticFamily::Create;
	Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::Target);
	Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::TypedPins);
	break;
```

- [x] **Step 2.3: Project create fields into request semantics**

In the bundle projector, copy these values from resolved context to `Request.Semantic`:

```cpp
OutRequest.Semantic.CreateOperation = Demand.CreateOperation;
OutRequest.Semantic.ClassPath = Demand.ClassPath;
OutRequest.Semantic.AssetPath = Demand.AssetPath;
OutRequest.Semantic.ContainerElementPinType = Demand.ContainerElementPinType;
OutRequest.Semantic.ContainerKeyPinType = Demand.ContainerKeyPinType;
OutRequest.Semantic.ContainerValuePinType = Demand.ContainerValuePinType;
```

- [x] **Step 2.4: Run context projection tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionContext;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_GenericCreate_Context_GREEN_001'
```

Expected after this task: existing ActionContext tests pass; create resolver tests still fail until Task 3.

## Task 3: Implement Generic Create Resolver

**Files:**
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericCreateActionResolver.h`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericCreateActionResolver.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionCluster.cpp`

- [x] **Step 3.1: Create resolver interface**

`BlueprintHelperGenericCreateActionResolver.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class FBlueprintHelperActionClusterContextView;

class BLUEPRINTHELPER_API FBlueprintHelperGenericCreateActionResolver
{
public:
	static bool IsSupportedCreateOperation(const FString& CreateOperation);
	static FBlueprintHelperActionResolutionResult Resolve(
		const FBlueprintHelperActionResolutionRequest& Request,
		const FBlueprintHelperActionClusterContextView& Context);
};
```

- [x] **Step 3.2: Implement operation gate and stable diagnostics**

`BlueprintHelperGenericCreateActionResolver.cpp` must start with:

```cpp
bool FBlueprintHelperGenericCreateActionResolver::IsSupportedCreateOperation(const FString& CreateOperation)
{
	const FString Normalized = CreateOperation.TrimStartAndEnd().ToLower();
	return Normalized == TEXT("spawn_actor")
		|| Normalized == TEXT("create_widget")
		|| Normalized == TEXT("construct_object")
		|| Normalized == TEXT("make_array")
		|| Normalized == TEXT("make_map")
		|| Normalized == TEXT("make_set")
		|| Normalized == TEXT("asset_action");
}
```

Missing operation returns:

```cpp
Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
Result.ErrorCode = TEXT("needs_more_semantic_context");
Result.Message = TEXT("Create semantic requires create_operation.");
```

Unsupported operation returns:

```cpp
Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
Result.ErrorCode = TEXT("unsupported_create_operation");
```

- [x] **Step 3.3: Resolve NodeSpawner-backed create operations**

Use UE class-specific spawners through `UBlueprintNodeSpawner::Create` for singleton K2Node create families:

```cpp
UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(UK2Node_SpawnActorFromClass::StaticClass());
Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
Result.SelectedSpawner = Spawner;
Result.SelectedStableId = FString::Printf(TEXT("generic_create:%s:%s"), *Operation, *ClassEvidence);
Result.SpawnerClass = Spawner ? Spawner->GetClass()->GetPathName() : FString();
Result.NodeClass = TEXT("/Script/BlueprintGraph.K2Node_SpawnActorFromClass");
Result.MatchReason = FString::Printf(TEXT("create_operation=%s"), *Operation);
```

Use the matching node classes for widget/object/container operations:

```cpp
UK2Node_CreateWidget::StaticClass()
UK2Node_GenericCreateObject::StaticClass()
UK2Node_MakeArray::StaticClass()
UK2Node_MakeMap::StaticClass()
UK2Node_MakeSet::StaticClass()
```

- [x] **Step 3.4: Keep asset action honest**

If `asset_action` cannot produce a stable `UBlueprintAssetNodeSpawner` with selected action evidence, return:

```cpp
Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
Result.ErrorCode = TEXT("needs_more_semantic_context");
Result.Message = TEXT("asset_action create requires projected asset action database evidence.");
```

Do not return success for `asset_action` without `SelectedSpawner`.

- [x] **Step 3.5: Route Create from Generic cluster**

In `FBlueprintHelperGenericAssetStructControlActionCluster::Resolve`, route before the generic provider boundary switch:

```cpp
if (Context.GetSemantic().Kind == EBlueprintHelperActionSemanticKind::Create)
{
	return FBlueprintHelperGenericCreateActionResolver::Resolve(Request, Context);
}
```

- [x] **Step 3.6: Run resolver tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic.Create;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_GenericCreate_Resolver_GREEN_001'
```

Expected: create resolver tests pass; old `NoFallbackSuccessForCreateConvertSchedule` still passes for Convert/Schedule.

## Task 4: Add Graph Body Lowering And Fragment Invocation

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`

- [x] **Step 4.1: Accept public `kind:"create"` in TS compiler**

Update supported body kinds:

```ts
const SUPPORTED_GRAPH_BODY_STATEMENT_KINDS = new Set(['call', 'field', 'set', 'set_property', 'let', 'control', 'create']);
const SUPPORTED_GRAPH_BODY_EXPRESSION_KINDS = new Set([
  'literal',
  'field',
  'get',
  'get_property',
  'call',
  'op',
  'construct',
  'deconstruct',
  'select',
  'create',
]);
```

For a create node, preserve:

```ts
create_operation: getRequiredString(record, 'create_operation', `${path}.create_operation`),
class_path: optionalString(record, 'class_path'),
asset_path: optionalString(record, 'asset_path'),
pin_type: isRecord(record['pin_type']) ? record['pin_type'] : undefined,
key_pin_type: isRecord(record['key_pin_type']) ? record['key_pin_type'] : undefined,
value_pin_type: isRecord(record['value_pin_type']) ? record['value_pin_type'] : undefined,
```

- [x] **Step 4.2: Mirror TS lowering in Python compiler**

Update the Python supported sets and compiled node copy to preserve:

```python
node["create_operation"] = require_string(record, "create_operation", f"{path}.create_operation")
copy_optional_string(record, node, "class_path")
copy_optional_string(record, node, "asset_path")
copy_optional_object(record, node, "pin_type")
copy_optional_object(record, node, "key_pin_type")
copy_optional_object(record, node, "value_pin_type")
```

- [x] **Step 4.3: Invoke selected spawner from GraphStatementBuilder**

Add a create fragment branch that builds an ActionRequest from projected context and invokes:

```cpp
UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
	TargetGraph,
	ActionResult,
	OutFragment.SuggestedLocation,
	AdapterOptions,
	OutError);
```

If `ActionResult.SelectedSpawner` is invalid, return an error using `ActionResult.ErrorCode` and do not direct-spawn a legacy node.

- [x] **Step 4.4: Run compiler and fragment tests**

Run:

```powershell
Push-Location AgentFaceService\task-core
npm.cmd run test:node
python -m unittest discover -s python/tests -t python
Pop-Location
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.GraphStatement;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_GenericCreate_Fragment_GREEN_001'
```

Expected: Node and Python tests pass; GraphStatement tests pass with 0 failed.

## Task 5: Verification, Smoke, And Docs

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`

- [x] **Step 5.1: Run Generic create focused automation**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic.Create;BlueprintHelper.GraphWrite.GraphStatement;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_GenericCreate_FINAL_001'
```

Expected: 0 failed, 0 not run.

- [x] **Step 5.2: Run full GraphWrite regression**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_GenericCreate_Regression_001'
```

Expected: 0 failed, 0 not run.

- [x] **Step 5.3: Compile UE 5.6 target**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected: `Result: Succeeded`.

- [x] **Step 5.4: Update docs only after verification**

Add one dated note to the design doc:

```markdown
## 2026-05-24 Generic Broad Create Closure

- `Create` is resolved inside `GenericAssetStructControlActionCluster` only when `create_operation` is explicit.
- Supported first slice: `spawn_actor`, `create_widget`, `construct_object`, `make_array`, `make_map`, `make_set`; `asset_action` requires selected ActionDatabase evidence and otherwise returns `needs_more_semantic_context`.
- Struct `construct/deconstruct` remains separate and must not be counted as broad create.
```

- [x] **Step 5.5: Record manual commit suggestion without committing**

Do not run `git add`, `git commit`, or `git push`. In the final implementation report, suggest this manual commit message:

```text
新增内容：
1. 接入 GraphWrite Generic broad create 二级语义与 resolver。
2. 增加 create Graph body lowering、fragment invocation 和自动化测试。

修复内容：
1. 防止 broad create 通过 struct fallback 假成功。
```

## Self-Review Checklist

- [x] `Create` success requires explicit `create_operation`.
- [x] `construct/deconstruct` tests still prove they do not resolve through broad create.
- [x] `asset_action` does not fake success without selected spawner evidence.
- [x] TS and Python compiler behavior matches.
- [x] Full GraphWrite regression and UE 5.6 compile pass before docs claim closure.
