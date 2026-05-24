# GraphWrite Generic Convert Schedule Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an explicit Generic-side path for `Convert` and `Schedule` semantics that are not FunctionAction calls, while preserving the existing FunctionAction ownership for `convert_function`, `schedule_function`, and `latent_or_async_function`.

**Architecture:** `Convert` and `Schedule` remain first-level semantic kinds, but ownership is selected by second-stage fields. If `FunctionOperation` is `convert_function`, `schedule_function`, or `latent_or_async_function`, the request stays in `FunctionActionCluster`; if `TransformOperation` or `ScheduleOperation` names a Generic operation, the request goes through `GenericAssetStructControlActionCluster` with selected spawner evidence or a deterministic missing-context diagnostic.

**Tech Stack:** UE 5.6 C++, GraphWrite SemanticIR, ActionContextPipeline, ActionResolutionCore, FunctionActionCluster, GenericAssetStructControlActionCluster, UBlueprintNodeSpawner, Unreal Automation Tests, AgentFace task-core TypeScript/Python compilers.

---

## Scope

This plan implements item 2 from the 2026-05-24 truth audit: Generic-side `convert/schedule`.

Function-side behavior that must remain unchanged:

| Semantic kind | Function operation | Owner |
|---|---|---|
| `Convert` | `convert_function` | `FunctionActionCluster` |
| `Schedule` | `schedule_function` | `FunctionActionCluster` |
| `Schedule` | `latent_or_async_function` | `FunctionActionCluster` |

Generic-side behavior introduced by this plan:

| Semantic kind | Generic operation field | Supported values | Expected result |
|---|---|---|---|
| `Convert` | `transform_operation` | `dynamic_cast`, `class_cast` | selected spawner for cast node or precise missing class evidence |
| `Convert` | `transform_operation` | `type_promotion` | selected type-promotion spawner only when UE exposes stable evidence |
| `Schedule` | `schedule_operation` | `timer_delegate_node` | selected spawner or precise `needs_more_semantic_context` requiring timer/delegate evidence |
| `Schedule` | `schedule_operation` | `latent_or_async_node` | selected spawner only when not expressible by FunctionAction and graph evidence permits latent placement |

Out of scope:

- Do not move existing FunctionAction convert/schedule success paths into Generic.
- Do not make Generic a fallback success path when second-stage evidence is absent.
- Do not use old parsed-node strings or MutationCoordinator special cases.
- Do not mark `timer` support complete if the only implementation is a normal Kismet function call; that stays FunctionAction.

## File Structure

- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`
  - Reuse existing `TransformOperation` and `ScheduleOperation`; add class/timer evidence fields only if the current constraints do not already expose them.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h`
  - Add or reuse target class, delegate, timer, and latent graph evidence.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
  - Route `Convert/Schedule` to Function or Generic based on second-stage evidence.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp`
  - Project Generic transform/schedule fields into action requests.
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformScheduleActionResolver.h`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformScheduleActionResolver.cpp`
  - Resolve Generic `Convert/Schedule` operations.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.cpp`
  - Classify Generic `Convert/Schedule` operations as candidate or missing context.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionCluster.cpp`
  - Route Generic `Convert/Schedule` to the new resolver.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericAssetStructControlActionClusterTests.cpp`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericTransformScheduleActionResolverTests.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFunctionActionClusterTests.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`

---

## Task 1: Lock Ownership Contract Tests

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFunctionActionClusterTests.cpp`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericTransformScheduleActionResolverTests.cpp`

- [x] **Step 1.1: Add Function ownership regression**

Add this assertion to the existing FunctionAction tests:

```cpp
TestEqual(TEXT("function convert stays FunctionAction"), ConvertResult.ClusterKind, EBlueprintHelperSpawnerClusterKind::FunctionAction);
TestEqual(TEXT("function schedule stays FunctionAction"), ScheduleResult.ClusterKind, EBlueprintHelperSpawnerClusterKind::FunctionAction);
TestFalse(TEXT("function convert has no generic match reason"), ConvertResult.MatchReason.Contains(TEXT("generic_transform")));
TestFalse(TEXT("function schedule has no generic match reason"), ScheduleResult.MatchReason.Contains(TEXT("generic_schedule")));
```

- [x] **Step 1.2: Add RED Generic convert missing evidence test**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericConvertRequiresGenericOperationTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Convert.RequiresGenericOperation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericConvertRequiresGenericOperationTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericActionTestBlueprint();
	UEdGraph* Graph = GetGenericActionTestGraph(Blueprint);

	FBlueprintHelperActionResolutionRequest Request =
		MakeGenericActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Convert);
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Request.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Convert;

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(Request);

	TestEqual(TEXT("missing generic convert operation status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("missing generic convert operation error"), Result.ErrorCode, FString(TEXT("needs_more_semantic_context")));
	TestFalse(TEXT("missing generic convert operation has no spawner"), Result.SelectedSpawner.IsValid());
	return true;
}
```

- [x] **Step 1.3: Add RED Generic schedule missing evidence test**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericScheduleRequiresGenericOperationTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Schedule.RequiresGenericOperation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericScheduleRequiresGenericOperationTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericActionTestBlueprint();
	UEdGraph* Graph = GetGenericActionTestGraph(Blueprint);

	FBlueprintHelperActionResolutionRequest Request =
		MakeGenericActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Schedule);
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Request.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Schedule;

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(Request);

	TestEqual(TEXT("missing generic schedule operation status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("missing generic schedule operation error"), Result.ErrorCode, FString(TEXT("needs_more_semantic_context")));
	TestFalse(TEXT("missing generic schedule operation has no spawner"), Result.SelectedSpawner.IsValid());
	return true;
}
```

- [x] **Step 1.4: Run ownership tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic.Convert;BlueprintHelper.GraphWrite.ActionResolution.Generic.Schedule;BlueprintHelper.GraphWrite.ActionResolution.FunctionAction;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_GenericConvertSchedule_RED_001'
```

Expected before implementation: Generic tests fail because Generic `Convert/Schedule` still use the old unsupported boundary; FunctionAction tests pass.

Execution note: no separate pre-implementation RED artifact was retained in this pass. The ownership assertions are covered by the final GREEN `BlueprintHelper.GraphWrite.ActionResolution` and full `BlueprintHelper.GraphWrite` automation runs recorded below.

## Task 2: Add Routing Rules In ActionContext

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp`

- [x] **Step 2.1: Route Function operations to FunctionAction**

Keep this behavior:

```cpp
if (Kind == EBlueprintHelperActionSemanticKind::Convert
	&& Demand.FunctionOperation == TEXT("convert_function"))
{
	Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
}

if (Kind == EBlueprintHelperActionSemanticKind::Schedule
	&& (Demand.FunctionOperation == TEXT("schedule_function")
		|| Demand.FunctionOperation == TEXT("latent_or_async_function")))
{
	Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
}
```

- [x] **Step 2.2: Route Generic operations to Generic cluster**

Add:

```cpp
if (Kind == EBlueprintHelperActionSemanticKind::Convert
	&& !Demand.TransformOperation.IsEmpty()
	&& Demand.FunctionOperation.IsEmpty())
{
	Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Demand.SemanticFamily = EBlueprintHelperActionSemanticFamily::Convert;
	Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::TypedPins);
}

if (Kind == EBlueprintHelperActionSemanticKind::Schedule
	&& !Demand.ScheduleOperation.IsEmpty()
	&& Demand.FunctionOperation.IsEmpty())
{
	Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Demand.SemanticFamily = EBlueprintHelperActionSemanticFamily::Schedule;
	Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::Target);
}
```

- [x] **Step 2.3: Reject ambiguous second-stage ownership**

If both Function and Generic operation fields are present, return invalid context:

```cpp
Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
Result.ErrorCode = TEXT("ambiguous_convert_schedule_owner");
Result.Message = TEXT("Convert/Schedule must provide either FunctionOperation or Generic transform/schedule operation, not both.");
```

- [x] **Step 2.4: Run ActionContext routing tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionContext;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_GenericConvertSchedule_Context_GREEN_001'
```

Expected: existing Function convert/schedule context tests remain green; new Generic routing tests pass after they are added.

## Task 3: Implement Generic Transform/Schedule Resolver

**Files:**
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformScheduleActionResolver.h`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformScheduleActionResolver.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionCluster.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.cpp`

- [x] **Step 3.1: Add resolver interface**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class FBlueprintHelperActionClusterContextView;

class BLUEPRINTHELPER_API FBlueprintHelperGenericTransformScheduleActionResolver
{
public:
	static FBlueprintHelperActionResolutionResult ResolveConvert(
		const FBlueprintHelperActionResolutionRequest& Request,
		const FBlueprintHelperActionClusterContextView& Context);

	static FBlueprintHelperActionResolutionResult ResolveSchedule(
		const FBlueprintHelperActionResolutionRequest& Request,
		const FBlueprintHelperActionClusterContextView& Context);
};
```

- [x] **Step 3.2: Implement Generic convert operation gate**

```cpp
static bool IsSupportedGenericTransformOperation(const FString& Operation)
{
	const FString Normalized = Operation.TrimStartAndEnd().ToLower();
	return Normalized == TEXT("dynamic_cast")
		|| Normalized == TEXT("class_cast")
		|| Normalized == TEXT("type_promotion");
}
```

For missing operation:

```cpp
Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
Result.ErrorCode = TEXT("needs_more_semantic_context");
Result.Message = TEXT("Generic Convert requires transform_operation.");
```

- [x] **Step 3.3: Implement cast node resolution**

For `dynamic_cast` and `class_cast`, create a selected spawner for `UK2Node_DynamicCast` and expose target class evidence:

```cpp
UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(UK2Node_DynamicCast::StaticClass());
Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
Result.SelectedSpawner = Spawner;
Result.SelectedStableId = FString::Printf(TEXT("generic_transform:%s:%s"), *Operation, *TargetClassPath);
Result.NodeClass = TEXT("/Script/BlueprintGraph.K2Node_DynamicCast");
Result.MatchReason = FString::Printf(TEXT("generic_transform operation=%s target=%s"), *Operation, *TargetClassPath);
```

If target class evidence is missing:

```cpp
Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
Result.ErrorCode = TEXT("needs_more_semantic_context");
Result.Message = TEXT("Generic cast requires target class evidence.");
```

- [x] **Step 3.4: Keep type promotion honest**

If UE type-promotion evidence cannot provide a selected spawner, return:

```cpp
Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
Result.ErrorCode = TEXT("needs_more_semantic_context");
Result.Message = TEXT("type_promotion requires projected type-promotion spawner evidence.");
```

Do not reuse `FBlueprintHelperOperatorActionResolver` unless the request is explicitly reclassified as FunctionAction/Operator by ActionContext.

- [x] **Step 3.5: Implement Generic schedule operation gate**

```cpp
static bool IsSupportedGenericScheduleOperation(const FString& Operation)
{
	const FString Normalized = Operation.TrimStartAndEnd().ToLower();
	return Normalized == TEXT("timer_delegate_node")
		|| Normalized == TEXT("latent_or_async_node");
}
```

Missing operation returns `needs_more_semantic_context`. Unsupported operation returns `unsupported_generic_schedule_operation`.

- [x] **Step 3.6: Route from Generic cluster**

Add before the provider boundary fallback:

```cpp
if (Context.GetSemantic().Kind == EBlueprintHelperActionSemanticKind::Convert)
{
	return FBlueprintHelperGenericTransformScheduleActionResolver::ResolveConvert(Request, Context);
}

if (Context.GetSemantic().Kind == EBlueprintHelperActionSemanticKind::Schedule)
{
	return FBlueprintHelperGenericTransformScheduleActionResolver::ResolveSchedule(Request, Context);
}
```

- [x] **Step 3.7: Run resolver tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic.Convert;BlueprintHelper.GraphWrite.ActionResolution.Generic.Schedule;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_GenericConvertSchedule_Resolver_GREEN_001'
```

Expected: Generic missing-context tests pass; concrete cast test passes if target class evidence is supplied; schedule tests pass with either selected spawner or deterministic missing-context diagnostic.

## Task 4: Compiler Lowering And Contract Updates

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`

- [x] **Step 4.1: Preserve Generic transform/schedule fields in TS**

For `kind:"convert"` and `kind:"schedule"` body nodes, preserve:

```ts
transform_operation: optionalString(record, 'transform_operation'),
schedule_operation: optionalString(record, 'schedule_operation'),
function_operation: optionalString(record, 'function_operation'),
target_class_path: optionalString(record, 'target_class_path'),
graph_latent_allowed: typeof record['graph_latent_allowed'] === 'boolean' ? record['graph_latent_allowed'] : undefined,
```

- [x] **Step 4.2: Mirror lowering in Python**

```python
copy_optional_string(record, node, "transform_operation")
copy_optional_string(record, node, "schedule_operation")
copy_optional_string(record, node, "function_operation")
copy_optional_string(record, node, "target_class_path")
copy_optional_bool(record, node, "graph_latent_allowed")
```

- [x] **Step 4.3: Update source contract**

The old contract that forbids all `Convert/Schedule` tokens in Generic must be narrowed to forbid only Function-owned tokens:

```cpp
const TArray<FString> ForbiddenFunctionOwnedTokens = {
	TEXT("convert_function"),
	TEXT("schedule_function"),
	TEXT("latent_or_async_function"),
	TEXT("FBlueprintHelperCallFunctionResolver::Resolve")
};
```

Add positive contract tokens for the Generic resolver:

```cpp
TestTrue(TEXT("Generic Convert/Schedule resolver is present"),
	GenericClusterSource.Contains(TEXT("FBlueprintHelperGenericTransformScheduleActionResolver")));
```

- [x] **Step 4.4: Run compiler and contract tests**

Run:

```powershell
Push-Location AgentFaceService\task-core
npm.cmd run test:node
python -m unittest discover -s python/tests -t python
Pop-Location
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Contract;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_GenericConvertSchedule_Contract_GREEN_001'
```

Expected: Node tests pass, Python tests pass, contract automation has 0 failed.

## Task 5: Verification And Documentation

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`

- [x] **Step 5.1: Run focused automation**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic.Convert;BlueprintHelper.GraphWrite.ActionResolution.Generic.Schedule;BlueprintHelper.GraphWrite.ActionResolution.FunctionAction;BlueprintHelper.GraphWrite.ActionResolution.Contract;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_GenericConvertSchedule_FINAL_001'
```

Expected: 0 failed, 0 not run.

- [x] **Step 5.2: Run full GraphWrite regression**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_GenericConvertSchedule_Regression_001'
```

Expected: 0 failed, 0 not run.

- [x] **Step 5.3: Compile UE 5.6 target**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected: `Result: Succeeded`.

- [x] **Step 5.4: Update docs after tests**

Add this dated design note:

```markdown
## 2026-05-24 Generic Convert/Schedule Ownership Closure

- Function-owned `convert_function`, `schedule_function`, and `latent_or_async_function` remain in `FunctionActionCluster`.
- Generic-owned `dynamic_cast`, `class_cast`, `type_promotion`, `timer_delegate_node`, and `latent_or_async_node` require explicit second-stage evidence and do not fallback through FunctionAction.
- Generic schedule support is only considered complete for operations that expose selected spawner evidence or deterministic missing-context diagnostics; normal Kismet timer calls remain FunctionAction.
```

- [x] **Step 5.5: Record manual commit suggestion without committing**

Do not run `git add`, `git commit`, or `git push`. In the final implementation report, suggest:

```text
新增内容：
1. 接入 Generic-side Convert/Schedule 二级语义边界。
2. 增加 Function/Generic ownership contract 与自动化测试。

修复内容：
1. 防止 Generic Convert/Schedule 通过 Function 或 struct fallback 假成功。
```

## Execution Result - 2026-05-24

Implemented:

- Added first-level `Convert` / `Schedule` SemanticIR acceptance and statement/expression compiler lowering in UE, TypeScript, and Python.
- Preserved `function_operation`, `transform_operation`, `schedule_operation`, `target_class_path`, and `graph_latent_allowed` through AgentFace lowering and ActionContext projection.
- Kept function-owned `convert_function`, `schedule_function`, and `latent_or_async_function` in `FunctionActionCluster`.
- Routed explicit Generic `dynamic_cast`, `class_cast`, `type_promotion`, `timer_delegate_node`, and `latent_or_async_node` operations to `GenericAssetStructControlActionCluster`.
- Added `FBlueprintHelperGenericTransformScheduleActionResolver`; `dynamic_cast` / `class_cast` can select cast-node spawner evidence, while type-promotion and schedule operations return deterministic `needs_more_semantic_context` until projected spawner evidence exists.
- Added ambiguous Function+Generic ownership rejection via `ambiguous_convert_schedule_owner`.

Verification:

- `npm.cmd run build`: PASS.
- `npm.cmd run test:node`: PASS, 164 tests.
- `npm.cmd run test:python`: PASS, 71 tests.
- UE 5.6 `Build.bat TemplateEditor Win64 Development`: PASS, `Result: Succeeded`.
- `BlueprintHelper.GraphWrite.ActionResolution`: PASS after fixing source-hygiene failure around direct `UBlueprintNodeSpawner::Create(NodeClass)` token.
- `BlueprintHelper.GraphWrite.ActionContext`: PASS.
- Full `BlueprintHelper.GraphWrite`: PASS, `Saved/Automation/GraphWrite_GenericConvertSchedule_Final_20260524_001/index.json` reports 155 succeeded, 11 succeeded with warnings, 0 failed, 0 not run.
- Final `npm.cmd run test`: PASS.

## Self-Review Checklist

- [x] Function convert/schedule tests still pass and still report `FunctionAction`.
- [x] Generic convert/schedule tests require explicit second-stage evidence.
- [x] Generic resolver never calls `FBlueprintHelperCallFunctionResolver::Resolve`.
- [x] TS and Python compiler lowering preserve the same fields.
- [x] Documentation distinguishes Function-owned schedule from Generic-owned schedule.
