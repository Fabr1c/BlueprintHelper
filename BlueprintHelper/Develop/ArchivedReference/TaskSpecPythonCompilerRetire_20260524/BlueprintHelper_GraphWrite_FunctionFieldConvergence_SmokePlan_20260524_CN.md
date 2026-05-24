# GraphWrite Function Field Convergence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete GraphWrite FunctionActionCluster coverage for non-call callable semantics and complete Field semantics for complex property paths, linked typed pin inference, `component_ref`, and `field_access`, then run one unified Smoke gate for items 1, 2, and 4.

**Architecture:** Keep first-level dispatch on `EBlueprintHelperSpawnerClusterKind`; do not create a parallel GraphWrite action route. Function non-call semantics are handled by reusable FunctionAction resolver boundaries that adapt to `FBlueprintHelperCallFunctionResolver`, while Field semantics are handled by reusable path and evidence services under ActionContext and FieldVariableActionResolver. Event lifecycle taxonomy stays Signature-owned; GraphWrite Smoke only verifies the Signature-plus-body boundary and delegate use-site behavior.

**Tech Stack:** UE 5.6 C++, GraphWrite SemanticIR, ActionContextPipeline, FunctionActionCluster, FieldVariableActionCluster, BlueprintSignature TaskPlan adapters, AgentFace task-core TypeScript/Python compiler tests, Unreal Automation Tests, BlueprintHelper CLI.

---

## Execution Rules

- Use `superpowers:subagent-driven-development` for implementation. Route Task 1 and Task 2 to separate workers with disjoint files, then run Task 4 Smoke in the main session.
- Do not run `git add`, `git commit`, or `git push`. Completion output must list touched files, verification evidence, and a suggested commit message only.
- Keep UE 5.6 as the compile baseline. Any non-5.6 fallback requires a narrow `#if` guard and a test that proves the UE 5.6 path is unchanged.
- Do not route new GraphWrite behavior through legacy Transaction, Review v1, parsed-node fallback, or old graph-write utility branches.
- Do not add public GraphWrite `custom_event`, `override`, `native`, or `event_dispatcher` lifecycle taxonomy. Signature owns declaration and lifecycle. GraphWrite owns body writing, flow nodes, field/function nodes, and delegate use-site nodes.
- Public AgentFace syntax can remain compact, but compiler lowering, SemanticIR, ActionContext, and ActionResolution must converge on explicit internal semantic fields.

## Current Baseline

- `FBlueprintHelperActionContextDemandCollector::ApplyDemandKinds` already maps `Call`, `Op`, `Convert`, and `Schedule` to `FunctionAction`.
- `FBlueprintHelperFunctionActionCluster::Resolve` handles `Op` through `FBlueprintHelperOperatorActionResolver`, handles `Call` through `FBlueprintHelperCallFunctionResolver`, and returns `unsupported_function_cluster_semantic` for `Convert` and `Schedule`.
- `FieldVariableActionResolver` supports `Field + field_operation=get|set + field_scope=variable|property_path`.
- `GraphSemanticIR` accepts only `field_scope=variable|property_path`.
- Field resolver requires projected owner and field evidence, but complex path decomposition, `component_ref`, `field_access`, and symbol-linked pin inference are not yet represented as complete first-class semantics.
- Event audit result: public lifecycle taxonomy remains in BlueprintSignature. The current split is `blueprint_signature.ensure_custom_event` followed by `graph_write` body ops for custom event body writing.

## Canonical Semantic Targets

FunctionActionCluster must support these internal callable cases:

| Internal kind | Function second-stage value | Resolver behavior |
|---|---|---|
| `Call` | `function_call` | Existing call resolver path |
| `Op` | `operator_function` | Existing operator resolver path |
| `Convert` | `convert_function` | Guarded call resolver path with conversion evidence |
| `Schedule` | `schedule_function` | Guarded call resolver path with schedule evidence |
| `Schedule` | `latent_or_async_function` | Guarded call resolver path with latent or async evidence |

FieldVariableActionCluster must support these internal Field cases:

| `field_operation` | `field_scope` | Resolver behavior |
|---|---|---|
| `get` | `variable` | Existing variable get |
| `set` | `variable` | Existing variable set |
| `get` | `property_path` | Property-path get with owner and path evidence |
| `set` | `property_path` | Property-path set with owner and path evidence |
| `get` | `component_ref` | Component variable get from component evidence |
| `get` | `field_access` | Independent owner/member access using typed owner evidence |
| `set` | `field_access` | Independent owner/member write using typed owner evidence |

## Event Boundary From Subagent 5.4 High Audit

The Event taxonomy conflict audit returned this decision:

- Signature owns lifecycle and identity taxonomy: `function`, `interface_function`, `custom_event`, `interface_event`, `event_dispatcher`, `override_event`, and `native_event`.
- GraphWrite owns body and flow taxonomy: `custom_event_body`, `function_body`, `event_body`, `block_implementation`, plus delegate `bind`, `assign`, `unbind`, and `call`.
- `ensure_entry(custom_event)` and `custom_event_definition` remain compiler/runtime-private lowering conveniences because the compiler already emits a Signature step before GraphWrite body writing.
- Do not add GraphWrite public `override`, `native`, or `event_dispatcher` declaration kinds in this plan.

Smoke must verify this boundary after Function and Field work:

- `append_new_owned_graph` with a custom event compiles to `blueprint_signature.ensure_custom_event` step(s), then one `graph_write` step.
- `replace_owned_graph` with `replace.scope=custom_event_definition` compiles to a Signature split and a dependent GraphWrite body step.
- GraphWrite entry types other than `custom_event` still fail at compile/runtime boundary.
- `ensure_override_event` accepts only `native_event|override_event` on the Signature path and does not route through GraphWrite/EventDelegate.
- Dispatcher declaration stays on Signature/structure path; GraphWrite delegate statements only bind, assign, unbind, or call.

## File Structure

### Function Slice

- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`
  - Add Function second-stage fields to `FBlueprintHelperActionSemanticConstraints`.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h`
  - Add the same fields to `FBlueprintHelperActionContextDemand`.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
  - Populate callable operation evidence for `Call`, `Op`, `Convert`, and `Schedule`.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp`
  - Project callable operation, graph latent allowance, and typed pin evidence into resolved context.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp`
  - Include callable operation fields in semantic constraint hash and projection.
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionSemanticActionResolver.h`
  - Declare the reusable resolver for `Convert`, `Schedule`, and latent/async FunctionAction semantics.
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionSemanticActionResolver.cpp`
  - Validate second-stage evidence and adapt valid requests to `FBlueprintHelperCallFunctionResolver`.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionActionCluster.cpp`
  - Route `Convert` and `Schedule` to `FBlueprintHelperFunctionSemanticActionResolver`.
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFunctionActionClusterTests.cpp`
  - Add FunctionAction non-call resolver tests.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`
  - Add ActionContext demand/projection tests for `Convert` and `Schedule`.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`
  - Add source-contract tests preventing FunctionAction non-call support from moving into Generic fallback.

### Field Slice

- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`
  - Add Field path role fields to `FBlueprintHelperActionSemanticConstraints`.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h`
  - Add Field path role fields to `FBlueprintHelperActionContextDemand` and extend snapshot evidence where needed.
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldPathResolution.h`
  - Declare path parser, normalized segment model, and resolved Field path evidence.
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldPathResolution.cpp`
  - Implement reusable parsing and validation for `property_path`, `component_ref`, and `field_access`.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
  - Accept and project `component_ref` and `field_access` scopes.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp`
  - Infer Field owner/type evidence from linked symbol pins and component snapshots.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp`
  - Include Field path role fields in semantic constraint hash and projection.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.cpp`
  - Resolve `component_ref` and `field_access`; keep broad missing evidence as `needs_more_semantic_context`.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
  - Accept `field_scope=component_ref|field_access` and validate complex property path input.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagBuilderUtils.cpp`
  - Emit stable fragment metadata for complex property paths and independent field access.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
  - Build Field action requests only from projected ActionContext evidence.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldVariableActionClusterTests.cpp`
  - Add resolver tests for complex path, linked pin inference, `component_ref`, and `field_access`.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`
  - Add ActionContext tests for new Field scopes and typed pin inference.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphSemanticIRRuntimeFactTests.cpp`
  - Add SemanticIR parse/validation tests for Field scope values and path diagnostics.

### Unified Smoke Slice

- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteFunctionFieldUnifiedSmokeTests.cpp`
  - Add runtime Smoke tests spanning FunctionAction, FieldVariableAction, and Event boundary contracts.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/compiler/task-compiler.test.ts`
  - Add compile-level Event boundary Smoke assertions from the audit.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/python/tests/test_graph_write_function_field_smoke.py`
  - Add Python TaskSpec compile Smoke for Function and Field syntax if public schema changes are made.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
  - Record the final Function/Field supported semantics and Event boundary decision after verification.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`
  - Update Function and Field maturity only after the unified Smoke passes.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_TestRecord_20260522_CN.md`
  - Append verification commands and results after execution.

---

## Task 1: FunctionActionCluster Non-Call Callable Semantics

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionSemanticActionResolver.h`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionSemanticActionResolver.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionActionCluster.cpp`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFunctionActionClusterTests.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`

- [ ] **Step 1.1: Add RED ActionContext tests for Function non-call kinds**

Append these tests to `BlueprintHelperActionContextPipelineTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextConvertMapsToFunctionActionTest,
	"BlueprintHelper.GraphWrite.ActionContext.SingleDemand.ConvertMapsToFunctionAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextConvertMapsToFunctionActionTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionContextDemand Demand =
		FBlueprintHelperActionContextDemandCollector::BuildSingleDemand(
			TEXT("stmt_convert_001"),
			TEXT("$.body.statements[0].value"),
			EBlueprintHelperActionSemanticKind::Convert,
			TEXT("Conv_FloatToString"),
			TEXT("FloatToString"),
			FString());

	TestEqual(TEXT("Convert cluster kind"), Demand.ClusterKind, EBlueprintHelperSpawnerClusterKind::FunctionAction);
	TestEqual(TEXT("Convert semantic kind"), Demand.SemanticKind, EBlueprintHelperActionSemanticKind::Convert);
	TestTrue(TEXT("Convert needs typed pins"), Demand.RequiredKinds.Contains(EBlueprintHelperActionContextDemandKind::TypedPins));
	TestTrue(TEXT("Convert needs target evidence"), Demand.RequiredKinds.Contains(EBlueprintHelperActionContextDemandKind::Target));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextScheduleMapsToFunctionActionTest,
	"BlueprintHelper.GraphWrite.ActionContext.SingleDemand.ScheduleMapsToFunctionAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextScheduleMapsToFunctionActionTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionContextDemand Demand =
		FBlueprintHelperActionContextDemandCollector::BuildSingleDemand(
			TEXT("stmt_schedule_001"),
			TEXT("$.body.statements[1]"),
			EBlueprintHelperActionSemanticKind::Schedule,
			TEXT("Delay"),
			TEXT("Delay"),
			FString());

	TestEqual(TEXT("Schedule cluster kind"), Demand.ClusterKind, EBlueprintHelperSpawnerClusterKind::FunctionAction);
	TestEqual(TEXT("Schedule semantic kind"), Demand.SemanticKind, EBlueprintHelperActionSemanticKind::Schedule);
	TestTrue(TEXT("Schedule needs typed pins"), Demand.RequiredKinds.Contains(EBlueprintHelperActionContextDemandKind::TypedPins));
	TestTrue(TEXT("Schedule needs target evidence"), Demand.RequiredKinds.Contains(EBlueprintHelperActionContextDemandKind::Target));
	return true;
}
```

- [ ] **Step 1.2: Run RED ActionContext tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionContext.SingleDemand.ConvertMapsToFunctionAction;BlueprintHelper.GraphWrite.ActionContext.SingleDemand.ScheduleMapsToFunctionAction;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWriteFunctionContextRed'
```

Expected before implementation:

```text
ConvertMapsToFunctionAction: PASS if current demand routing already maps Convert to FunctionAction.
ScheduleMapsToFunctionAction: PASS if current demand routing already maps Schedule to FunctionAction.
```

If both tests pass, keep them as regression coverage and continue to Step 1.3.

- [ ] **Step 1.3: Add RED FunctionAction resolver tests**

Create `BlueprintHelperFunctionActionClusterTests.cpp` with this test skeleton and local helpers:

```cpp
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

static FBlueprintHelperActionResolutionRequest MakeFunctionActionRequest(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	EBlueprintHelperActionSemanticKind Kind,
	const FString& Query,
	const FString& FunctionOperation)
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.StatementId = TEXT("function_action_test");
	Request.Semantic.Kind = Kind;
	Request.Semantic.Query = Query;
	Request.Semantic.TargetPath = Query;
	Request.Semantic.DefaultValues.Add(TEXT("function_operation"), FunctionOperation);
	Request.bAllowFuzzyUnique = true;
	Request.MaxCandidates = 5;
	return Request;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFunctionActionRejectsMissingConvertEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.ConvertRequiresEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFunctionActionRejectsMissingConvertEvidenceTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionResolutionRequest Request =
		MakeFunctionActionRequest(nullptr, nullptr, EBlueprintHelperActionSemanticKind::Convert, FString(), TEXT("convert_function"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("error"), Result.ErrorCode, FString(TEXT("needs_more_semantic_context")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFunctionActionRejectsMissingScheduleEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.ScheduleRequiresEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFunctionActionRejectsMissingScheduleEvidenceTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionResolutionRequest Request =
		MakeFunctionActionRequest(nullptr, nullptr, EBlueprintHelperActionSemanticKind::Schedule, FString(), TEXT("latent_or_async_function"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("error"), Result.ErrorCode, FString(TEXT("needs_more_semantic_context")));
	return true;
}

#endif
```

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.ConvertRequiresEvidence;BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.ScheduleRequiresEvidence;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWriteFunctionResolverRed'
```

Expected before implementation:

```text
FAIL: current FunctionActionCluster returns unsupported_function_cluster_semantic for Convert/Schedule.
```

- [ ] **Step 1.4: Add Function second-stage fields**

In `FBlueprintHelperActionSemanticConstraints`, add:

```cpp
FString FunctionOperation;
FString TransformOperation;
FString ScheduleOperation;
```

In `FBlueprintHelperActionContextDemand`, add:

```cpp
FString FunctionOperation;
FString TransformOperation;
FString ScheduleOperation;
```

Populate these fields in `BuildSingleDemand` and demand projection:

```cpp
if (Demand.SemanticKind == EBlueprintHelperActionSemanticKind::Call)
{
	Demand.FunctionOperation = TEXT("function_call");
}
else if (Demand.SemanticKind == EBlueprintHelperActionSemanticKind::Op)
{
	Demand.FunctionOperation = TEXT("operator_function");
}
else if (Demand.SemanticKind == EBlueprintHelperActionSemanticKind::Convert)
{
	Demand.FunctionOperation = TEXT("convert_function");
	Demand.TransformOperation = TEXT("convert");
}
else if (Demand.SemanticKind == EBlueprintHelperActionSemanticKind::Schedule)
{
	Demand.FunctionOperation = Demand.DefaultValues.FindRef(TEXT("function_operation"));
	if (Demand.FunctionOperation.IsEmpty())
	{
		Demand.FunctionOperation = TEXT("schedule_function");
	}
	Demand.ScheduleOperation = Demand.DefaultValues.FindRef(TEXT("schedule_operation"));
	if (Demand.ScheduleOperation.IsEmpty())
	{
		Demand.ScheduleOperation = TEXT("latent_or_async");
	}
}
```

Also include `FunctionOperation`, `TransformOperation`, and `ScheduleOperation` in `SemanticConstraintsHash`.

- [ ] **Step 1.5: Create reusable Function semantic resolver**

Create `BlueprintHelperFunctionSemanticActionResolver.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class FBlueprintHelperActionClusterContextView;

class BLUEPRINTHELPER_API FBlueprintHelperFunctionSemanticActionResolver
{
public:
	static bool IsSupportedSemanticKind(const FBlueprintHelperActionSemanticConstraints& Semantic);

	static FBlueprintHelperActionResolutionResult Resolve(
		const FBlueprintHelperActionResolutionRequest& Request,
		const FBlueprintHelperActionClusterContextView& Context);
};
```

Create `BlueprintHelperFunctionSemanticActionResolver.cpp` with these rules:

```cpp
bool FBlueprintHelperFunctionSemanticActionResolver::IsSupportedSemanticKind(
	const FBlueprintHelperActionSemanticConstraints& Semantic)
{
	if (Semantic.Kind == EBlueprintHelperActionSemanticKind::Convert)
	{
		return Semantic.FunctionOperation.Equals(TEXT("convert_function"), ESearchCase::IgnoreCase)
			|| Semantic.DefaultValues.FindRef(TEXT("function_operation")).Equals(TEXT("convert_function"), ESearchCase::IgnoreCase);
	}

	if (Semantic.Kind == EBlueprintHelperActionSemanticKind::Schedule)
	{
		const FString Operation = !Semantic.FunctionOperation.IsEmpty()
			? Semantic.FunctionOperation
			: Semantic.DefaultValues.FindRef(TEXT("function_operation"));
		return Operation.Equals(TEXT("schedule_function"), ESearchCase::IgnoreCase)
			|| Operation.Equals(TEXT("latent_or_async_function"), ESearchCase::IgnoreCase);
	}

	return false;
}
```

The `Resolve` method must:

- Return `needs_more_semantic_context` when `Query` is empty.
- Return `needs_more_semantic_context` for `Convert` when neither typed argument pins nor expected return pin evidence exists.
- Return `needs_more_semantic_context` for `Schedule` when `function_operation` is not `schedule_function` or `latent_or_async_function`.
- For `latent_or_async_function`, require projected evidence `graph_latent_allowed=true`.
- Build `FBlueprintHelperCallFunctionResolveRequest` with the same context fields used by `FBlueprintHelperFunctionActionCluster::Resolve` for `Call`.
- Map resolver result status with the same status mapping used by the existing `Call` path.
- Set `Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction`.
- Preserve selected spawner, selected function, candidate diagnostics, and selected stable id.

- [ ] **Step 1.6: Route Convert and Schedule from FunctionActionCluster**

In `BlueprintHelperFunctionActionCluster.cpp`, add the include:

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionSemanticActionResolver.h"
```

Update `Resolve` ordering:

```cpp
if (Semantic.Kind == EBlueprintHelperActionSemanticKind::Op)
{
	return FBlueprintHelperOperatorActionResolver::Resolve(Request, Context);
}

if (FBlueprintHelperFunctionSemanticActionResolver::IsSupportedSemanticKind(Semantic))
{
	return FBlueprintHelperFunctionSemanticActionResolver::Resolve(Request, Context);
}

if (Semantic.Kind != EBlueprintHelperActionSemanticKind::Call)
{
	return MakeUnsupportedIntentResult(Request);
}
```

- [ ] **Step 1.7: Add positive FunctionAction tests**

Extend `BlueprintHelperFunctionActionClusterTests.cpp` with two positive tests:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFunctionActionConvertDispatchesToCallResolverTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.ConvertDispatchesToCallResolver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFunctionActionConvertDispatchesToCallResolverTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		GetTransientPackage(),
		TEXT("BP_FunctionActionConvertDispatch"),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass());

	UEdGraph* Graph = Blueprint ? Blueprint->UbergraphPages[0] : nullptr;
	FBlueprintHelperActionResolutionRequest Request =
		MakeFunctionActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Convert, TEXT("Conv_FloatToString"), TEXT("convert_function"));
	Request.Semantic.ArgumentPinTypes.Add(TEXT("InFloat"), FBlueprintHelperCallFunctionPinType(TEXT("real"), TEXT("float"), FString()));
	Request.Semantic.ExpectedReturnPinType = FBlueprintHelperCallFunctionPinType(TEXT("string"), FString(), FString());

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestNotEqual(TEXT("not unsupported"), Result.ErrorCode, FString(TEXT("unsupported_function_cluster_semantic")));
	TestTrue(TEXT("candidate diagnostics available or resolved"), Result.IsResolved() || Result.CandidateActions.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFunctionActionLatentScheduleRequiresLatentGraphTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.LatentScheduleRequiresLatentGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFunctionActionLatentScheduleRequiresLatentGraphTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionResolutionRequest Request =
		MakeFunctionActionRequest(nullptr, nullptr, EBlueprintHelperActionSemanticKind::Schedule, TEXT("Delay"), TEXT("latent_or_async_function"));
	Request.ContextEvidence.Add(TEXT("graph_latent_allowed"), TEXT("false"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("error"), Result.ErrorCode, FString(TEXT("latent_function_not_allowed_in_graph")));
	return true;
}
```

- [ ] **Step 1.8: Add source-contract test against Generic fallback**

In `BlueprintHelperActionResolutionContractTests.cpp`, add a source contract that checks:

```cpp
bClean &= TestFalse(
	TEXT("Function convert must not route through Generic fallback"),
	GenericClusterSource.Contains(TEXT("unsupported generic fallback success for convert")));
bClean &= TestTrue(
	TEXT("FunctionActionCluster references FunctionSemanticActionResolver"),
	FunctionClusterSource.Contains(TEXT("FBlueprintHelperFunctionSemanticActionResolver")));
```

The exact source strings should assert that `FunctionActionCluster.cpp` includes `BlueprintHelperFunctionSemanticActionResolver.h` and that `GenericAssetStructControlActionCluster.cpp` does not resolve `Convert` or `Schedule` by fallback success.

- [ ] **Step 1.9: Run FunctionAction targeted tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.FunctionAction;BlueprintHelper.GraphWrite.ActionContext.SingleDemand.ConvertMapsToFunctionAction;BlueprintHelper.GraphWrite.ActionContext.SingleDemand.ScheduleMapsToFunctionAction;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWriteFunctionAction'
```

Expected:

```text
Build.bat exits 0.
FunctionAction tests pass.
No result contains unsupported_function_cluster_semantic for Convert/Schedule guarded requests.
Missing evidence requests return needs_more_semantic_context.
```

---

## Task 2: Field Complex Path, Linked Pin Inference, Component Ref, And Field Access

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldPathResolution.h`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldPathResolution.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagBuilderUtils.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldVariableActionClusterTests.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphSemanticIRRuntimeFactTests.cpp`

- [ ] **Step 2.1: Add RED Field scope parser tests**

Append tests to `BlueprintHelperGraphSemanticIRRuntimeFactTests.cpp` that parse Field statements and expressions with:

```json
{
  "kind": "field",
  "field_operation": "get",
  "field_scope": "component_ref",
  "target": "DoorMesh"
}
```

```json
{
  "kind": "field",
  "field_operation": "set",
  "field_scope": "field_access",
  "target": "DoorState",
  "property_path": "bIsClosed"
}
```

Expected before implementation:

```text
FAIL with field_scope_unsupported.
```

- [ ] **Step 2.2: Add RED Field resolver tests**

Extend `BlueprintHelperFieldVariableActionClusterTests.cpp` with four tests:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableComponentRefResolvesTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.ComponentRefResolves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableFieldAccessRequiresOwnerTypeTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.FieldAccessRequiresOwnerType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableComplexPropertyPathKeepsFullPathEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.ComplexPropertyPathKeepsFullPathEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableLinkedPinInfersExpectedReturnTypeTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.LinkedPinInfersExpectedReturnType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
```

The tests should assert:

- `component_ref` resolves only when evidence contains `component_property_name`, `component_binding_owner_class_path`, and `field_pin_category`.
- `field_access` returns `needs_more_semantic_context` when owner type evidence is absent.
- Complex `property_path=RelativeRotation.Roll` keeps `ContextEvidence["property_path"] == "RelativeRotation.Roll"` and candidate match reason includes `field_scope=property_path`.
- Linked symbol pin inference fills `ExpectedReturnPinType` or `TargetObjectPinType` before the resolver chooses a candidate.

- [ ] **Step 2.3: Extend canonical Field scopes**

In `GraphSemanticIR.cpp`, update `IsSupportedFieldScope`:

```cpp
static bool IsSupportedFieldScope(const FString& Scope)
{
	const FString Normalized = NormalizeFieldToken(Scope);
	return Normalized == TEXT("variable")
		|| Normalized == TEXT("property_path")
		|| Normalized == TEXT("component_ref")
		|| Normalized == TEXT("field_access");
}
```

In `FieldVariableActionResolver.cpp`, update `IsSupportedSemanticKind` to accept the same four scopes while keeping `field_operation=get|set`.

- [ ] **Step 2.4: Add Field path resolution service**

Create `BlueprintHelperFieldPathResolution.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

enum class EBlueprintHelperFieldPathRole : uint8
{
	Variable,
	PropertyPath,
	ComponentRef,
	FieldAccess,
	Unknown
};

struct FBlueprintHelperResolvedFieldPath
{
	EBlueprintHelperFieldPathRole Role = EBlueprintHelperFieldPathRole::Unknown;
	FString RootName;
	FString LeafName;
	FString FullPath;
	TArray<FString> Segments;
	FString OwnerClassPath;
	FBlueprintHelperCallFunctionPinType OwnerPinType;
	FBlueprintHelperCallFunctionPinType LeafPinType;
	bool bRequiresFragmentDecomposition = false;
	bool bIsValid = false;
	FString ErrorCode;
	FString Message;
};

class BLUEPRINTHELPER_API FBlueprintHelperFieldPathResolution
{
public:
	static FBlueprintHelperResolvedFieldPath Resolve(
		const FBlueprintHelperActionResolutionRequest& Request,
		const TMap<FString, FString>& Evidence);
};
```

Implement `Resolve` so that:

- `variable` role uses `TargetPath` or `Query` as both root and leaf.
- `property_path` role preserves the full path and sets `LeafName` to the final segment.
- `component_ref` role requires `component_property_name` or a non-empty target that matches a component evidence row.
- `field_access` role requires owner evidence from `field_owner_class`, `property_owner`, `target_object_type`, or `TargetObjectPinType`.
- Multi-segment paths set `bRequiresFragmentDecomposition=true`.
- Empty owner/path inputs return `bIsValid=false` with `ErrorCode=needs_more_semantic_context`.

- [ ] **Step 2.5: Project linked typed pin inference**

In `ActionContextInferenceService.cpp`, extend the Field inference branch:

```cpp
if (Demand.SemanticKind == EBlueprintHelperActionSemanticKind::Field)
{
	for (const FString& SourceSymbolId : Demand.SourceSymbolIds)
	{
		if (const FBlueprintHelperCallFunctionPinType* PinType = Snapshot.SymbolPinTypes.Find(SourceSymbolId))
		{
			Context.Semantic.TargetObjectPinType = *PinType;
			Context.Evidence.Add(TEXT("linked_source_pin_type"), PinType->ToString());
			break;
		}
	}

	for (const FString& ConsumerSymbolId : Demand.ConsumerSymbolIds)
	{
		if (const FBlueprintHelperCallFunctionPinType* PinType = Snapshot.SymbolPinTypes.Find(ConsumerSymbolId))
		{
			Context.Semantic.ExpectedReturnPinType = *PinType;
			Context.Evidence.Add(TEXT("linked_consumer_pin_type"), PinType->ToString());
			break;
		}
	}
}
```

If `FBlueprintHelperCallFunctionPinType` does not currently expose `ToString()`, add a private helper in the inference source file:

```cpp
static FString DescribeActionContextPinType(const FBlueprintHelperCallFunctionPinType& PinType)
{
	FString Result = PinType.Category;
	if (!PinType.SubCategory.IsEmpty())
	{
		Result += TEXT(".");
		Result += PinType.SubCategory;
	}
	if (!PinType.SubCategoryObjectPath.IsEmpty())
	{
		Result += TEXT(":");
		Result += PinType.SubCategoryObjectPath;
	}
	return Result;
}
```

- [ ] **Step 2.6: Resolve `component_ref` independently**

In `FieldVariableActionResolver.cpp`, use `FBlueprintHelperFieldPathResolution::Resolve` before choosing `FieldName`.

For `component_ref`:

```cpp
if (ResolvedPath.Role == EBlueprintHelperFieldPathRole::ComponentRef)
{
	FieldName = !ResolvedPath.LeafName.IsEmpty()
		? ResolvedPath.LeafName
		: GetEvidenceValue(Evidence, TEXT("component_property_name"));
}
```

The selected candidate must use:

```text
Category = field_component_ref
StableId prefix = field_component_ref:
MatchReason contains field_scope=component_ref
```

`component_ref` must not require `property_path`.

- [ ] **Step 2.7: Resolve `field_access` independently**

For `field_access`:

```cpp
if (ResolvedPath.Role == EBlueprintHelperFieldPathRole::FieldAccess)
{
	if (ResolvedPath.OwnerClassPath.IsEmpty() && !ResolvedPath.OwnerPinType.IsValid())
	{
		return MakeFieldMissingEvidenceResult(TEXT("Field access requires owner class or owner pin type evidence."));
	}
	FieldName = ResolvedPath.LeafName;
}
```

The selected candidate must use:

```text
Category = field_access
StableId prefix = field_access:
MatchReason contains field_scope=field_access
```

`field_access` must not be lowered into `property_path` internally.

- [ ] **Step 2.8: Preserve complex property path decomposition metadata**

When `bRequiresFragmentDecomposition=true`, set:

```cpp
Result.bRequiresDedicatedFragmentBuilder = true;
Result.MatchReason = TEXT("complex_property_path_requires_field_path_fragment_builder");
```

In `GraphFragmentDagBuilderUtils.cpp`, emit metadata:

```cpp
AddMetadata(Fragment, TEXT("field.path.full"), ResolvedPath.FullPath);
AddMetadata(Fragment, TEXT("field.path.root"), ResolvedPath.RootName);
AddMetadata(Fragment, TEXT("field.path.leaf"), ResolvedPath.LeafName);
AddMetadata(Fragment, TEXT("field.path.role"), TEXT("property_path"));
```

Complex property paths must compose through GraphWrite fragment builders and existing Struct/TypeStructure fragment support when struct break/make nodes are required. Do not emit parsed-node fallback plans.

- [ ] **Step 2.9: Run Field targeted tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.FieldVariable;BlueprintHelper.GraphWrite.ActionContext;BlueprintHelper.GraphWrite.GraphSemanticIR;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWriteFieldSemantics'
```

Expected:

```text
Build.bat exits 0.
FieldVariable tests pass.
component_ref resolves without property_path.
field_access fails without owner evidence and resolves with owner evidence.
Complex property_path keeps the full path and asks for dedicated fragment decomposition.
Linked symbol pin evidence is projected into action constraints.
No result contains unsupported_field_variable_cluster_semantic.
```

---

## Task 4: Unified Smoke For Items 1, 2, And Event Boundary

**Files:**
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteFunctionFieldUnifiedSmokeTests.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/compiler/task-compiler.test.ts`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/python/tests/test_graph_write_function_field_smoke.py`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_TestRecord_20260522_CN.md`

- [ ] **Step 4.1: Add unified C++ Smoke tests**

Create `BlueprintHelperGraphWriteFunctionFieldUnifiedSmokeTests.cpp` with tests named:

```text
BlueprintHelper.GraphWrite.FunctionFieldUnifiedSmoke.FunctionConvertAndSchedule
BlueprintHelper.GraphWrite.FunctionFieldUnifiedSmoke.FieldComponentRefAndFieldAccess
BlueprintHelper.GraphWrite.FunctionFieldUnifiedSmoke.EventSignatureBoundary
```

The Function Smoke must assert:

```text
Convert request reaches FunctionActionCluster.
Schedule request reaches FunctionActionCluster.
Guarded requests do not return unsupported_function_cluster_semantic.
Missing evidence returns needs_more_semantic_context.
Latent request checks graph_latent_allowed evidence.
```

The Field Smoke must assert:

```text
component_ref resolves from component evidence.
field_access is not rewritten to property_path.
complex property_path keeps full path metadata.
linked typed pins affect target or return pin constraints.
```

The Event boundary Smoke must assert with source-contract reads:

```text
EventDelegateActionCluster owns ComponentBoundEvent and Delegate.
EventDelegateActionCluster source does not contain ensure_custom_event.
EventDelegateActionCluster source does not contain ensure_override_event.
EventDelegateActionCluster source does not contain native_event declaration handling.
```

- [ ] **Step 4.2: Add TypeScript compiler Smoke for Event boundary**

In `task-compiler.test.ts`, add tests that compile:

```json
{
  "task_type": "edit_blueprint_graph",
  "behavior": {
    "graph_strategy": "append_new_owned_graph",
    "entries": [{
      "entry_type": "custom_event",
      "name": "GW_CustomEventBoundary",
      "body": {
        "schema": "BlueprintLogicSpec.v1",
        "statements": []
      }
    }]
  }
}
```

Assert the emitted plan shape:

```text
step_001.capability == blueprint_signature
step_001.operation == ensure_custom_event
step_002.capability == graph_write
step_002.depends_on contains step_001
```

Add a second test for `replace.scope=custom_event_definition` with the same Signature split assertion.

- [ ] **Step 4.3: Add Python TaskSpec Smoke for Function and Field public syntax**

Create `test_graph_write_function_field_smoke.py` only if Python public schema/lowering changes are made in Task 1 or Task 2. The tests must compile a minimal `BlueprintHelper.TaskSpec.v1` that contains:

```json
{
  "kind": "field",
  "field_operation": "set",
  "field_scope": "property_path",
  "target": "DoorMesh",
  "property_path": "RelativeRotation.Roll",
  "value": {
    "kind": "field",
    "field_operation": "get",
    "field_scope": "field_access",
    "target": "DoorState",
    "property_path": "TargetYaw"
  }
}
```

Assert the compiled GraphWrite body preserves:

```text
field_scope=property_path
property_path=RelativeRotation.Roll
nested value field_scope=field_access
nested value property_path=TargetYaw
```

- [ ] **Step 4.4: Run task-core and CLI compiler tests**

Run:

```powershell
Push-Location 'D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core'
npm.cmd run build
npm.cmd run test:node
python -m unittest discover -s python/tests -t python
Pop-Location

Push-Location 'D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\cli'
npm.cmd run build
npm.cmd run test:node
Pop-Location
```

Expected:

```text
task-core build exits 0.
task-core node tests pass.
task-core python tests pass.
cli build exits 0.
cli node tests pass.
```

- [ ] **Step 4.5: Run targeted UE Automation Smoke**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.FunctionAction;BlueprintHelper.GraphWrite.ActionResolution.FieldVariable;BlueprintHelper.GraphWrite.ActionContext;BlueprintHelper.GraphWrite.FunctionFieldUnifiedSmoke;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWriteFunctionFieldUnifiedSmoke'
```

Expected:

```text
Build.bat exits 0.
All targeted Automation tests pass.
Automation report contains no unsupported_function_cluster_semantic.
Automation report contains no unsupported_field_variable_cluster_semantic.
Automation report contains no parsed_node fallback success.
```

- [ ] **Step 4.6: Run CLI preview and execute Smoke**

Write the Smoke spec with UTF-8 without BOM:

```powershell
$SpecPath = 'D:\UEProjects\Template\Plugins\BlueprintHelper\.tmp\graphwrite_function_field_smoke.json'
$Json = @'
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "context_id": "graphwrite_function_field_smoke",
  "task_type": "edit_blueprint_graph",
  "feature_name": "GraphWriteFunctionFieldSmoke",
  "target": {
    "asset_path": "/Game/BlueprintHelper/Smoke/BP_GraphWriteFunctionFieldSmoke",
    "target_type": "blueprint"
  },
  "scope_policy": {
    "graph_name": "EG_GraphWriteFunctionFieldSmoke",
    "allow_modify_user_nodes": false
  },
  "behavior": {
    "graph_strategy": "append_new_owned_graph",
    "entries": [{
      "entry_type": "custom_event",
      "name": "RunFunctionFieldSmoke",
      "body": {
        "schema": "BlueprintLogicSpec.v1",
        "statements": [{
          "kind": "field",
          "field_operation": "set",
          "field_scope": "property_path",
          "target": "DoorMesh",
          "property_path": "RelativeRotation.Roll",
          "value": {
            "kind": "field",
            "field_operation": "get",
            "field_scope": "field_access",
            "target": "DoorState",
            "property_path": "TargetRoll"
          }
        }]
      }
    }]
  },
  "execution_policy": {
    "dry_run_mode": "full",
    "on_missing_capability": "stop_and_report"
  },
  "validation": {
    "should_compile": true,
    "should_save": false
  }
}
'@
New-Item -ItemType Directory -Force -Path (Split-Path $SpecPath) | Out-Null
[System.IO.File]::WriteAllText($SpecPath, $Json, [System.Text.UTF8Encoding]::new($false))
```

Run:

```powershell
bh.cmd task preview --file 'D:\UEProjects\Template\Plugins\BlueprintHelper\.tmp\graphwrite_function_field_smoke.json' --format full
bh.cmd task execute --file 'D:\UEProjects\Template\Plugins\BlueprintHelper\.tmp\graphwrite_function_field_smoke.json' --format full
```

Expected:

```text
Preview returns a TaskPlan with blueprint_signature before graph_write for the custom event.
Execute returns success or a concrete missing-evidence blocker tied to the Smoke fixture asset.
Result does not contain unsupported_function_cluster_semantic.
Result does not contain unsupported_field_variable_cluster_semantic.
Result does not contain legacy Transaction or Review v1 fallback tokens.
```

- [ ] **Step 4.7: Run full GraphWrite regression**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWriteFullAfterFunctionField'
```

Expected:

```text
All BlueprintHelper.GraphWrite tests pass.
Any failure outside Function/Field/Event boundary is recorded in the TestRecord with owning test name and first failing assertion.
```

- [ ] **Step 4.8: Update design and status documents**

Update:

```text
D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md
D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md
D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_TestRecord_20260522_CN.md
```

Record:

```text
FunctionActionCluster now covers Call, Op, Convert as convert_function, Schedule as schedule_function, and Schedule as latent_or_async_function when evidence permits.
FieldVariableActionCluster now covers variable, property_path, component_ref, and field_access with linked typed pin inference.
Event lifecycle taxonomy remains Signature-owned; GraphWrite body/delegate boundary is Smoke-verified.
Exact commands, report output paths, pass/fail counts, and remaining blockers are included.
```

---

## Self-Review Checklist

- [ ] Every item in user request 1 maps to Task 1.
- [ ] Every item in user request 2 maps to Task 2.
- [ ] Event taxonomy conflict from user request 3 is incorporated as a boundary decision and Smoke input.
- [ ] User request 4 maps to Task 4 unified Smoke.
- [ ] No task requires `git add`, `git commit`, or `git push`.
- [ ] No task adds legacy Transaction, Review v1, parsed-node fallback, or GraphWrite-owned Event lifecycle taxonomy.
- [ ] All new C++ classes have independent `.h/.cpp` files.
- [ ] All verification commands use UE 5.6 paths and write report output under `D:\UEProjects\Template\Saved\Automation`.

## Execution Completion Record - 2026-05-24

Status:

- Task 1 completed: `FunctionActionCluster` supports `Call`, `Op`, `Convert -> convert_function`, `Schedule -> schedule_function`, and `Schedule -> latent_or_async_function` when evidence permits.
- Task 2 completed: Field semantics support complex `property_path` metadata, linked typed pin inference, `component_ref`, and independent `field_access`.
- Task 4 completed for compiler, C++ automation, full GraphWrite regression, and CLI preview/execute boundary. Real CLI execute is blocked by the planned fixture asset not existing.

Verification evidence:

| Check | Result |
|---|---|
| `Build.bat TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload` | PASS after Function, Field, and unified smoke changes. |
| `BlueprintHelper.GraphWrite.ActionResolution.FunctionAction` | PASS: 6 succeeded, 0 failed. |
| `BlueprintHelper.GraphWrite.ActionContext.SingleDemand` | PASS: 5 succeeded, 0 failed. |
| `BlueprintHelper.GraphWrite.ActionResolution.FieldVariable` | PASS: 15 succeeded, 0 failed; latest report `Saved/Automation/GraphWriteFunctionFieldTargetedAfterPathFix/index.json`. |
| `BlueprintHelper.GraphWrite.ActionContext` | PASS: 17 succeeded, 0 failed. |
| `BlueprintHelper.GraphWrite.GraphSemanticIR` | PASS: 6 succeeded, 0 failed. |
| `BlueprintHelper.GraphWrite.FunctionFieldUnifiedSmoke` | PASS: 3 succeeded, 0 failed; latest report `Saved/Automation/GraphWriteFunctionFieldUnifiedSmokeAfterPathFix/index.json`. |
| `BlueprintHelper.GraphWrite` | PASS: 142 succeeded + 11 succeeded with warnings, 0 failed, 0 not run; latest report `Saved/Automation/GraphWriteFullAfterFunctionFieldPathFix/index.json`. |
| `AgentFaceService/task-core npm.cmd run build` | PASS. |
| `AgentFaceService/task-core npm.cmd run test:node` | PASS: 158 tests passed, 0 failed. |
| `AgentFaceService/task-core python -m unittest discover -s python/tests -t python` | PASS: 65 tests passed, 0 failed. |
| `AgentFaceService/cli npm.cmd run build` | PASS. |
| `AgentFaceService/cli npm.cmd run test:node` | PASS: 44 tests passed, 0 failed. |
| `bh.cmd task preview --file .tmp\graphwrite_function_field_smoke.json --format full` | PREVIEW_BLOCKED with valid TaskPlan shape: `blueprint_signature.ensure_custom_event` before dependent `graph_write`; blocker is `target_blueprint_not_found` for `/Game/BlueprintHelper/Smoke/BP_GraphWriteFunctionFieldSmoke`. |
| `bh.cmd task execute --file .tmp\graphwrite_function_field_smoke.json --format full` | EXECUTE_BLOCKED without writes; blocker is the same missing Smoke fixture asset. |

Self-review:

- [x] Every item in user request 1 maps to Task 1.
- [x] Every item in user request 2 maps to Task 2.
- [x] Event taxonomy conflict from user request 3 is incorporated as a boundary decision and Smoke input.
- [x] User request 4 maps to Task 4 unified Smoke.
- [x] No task requires `git add`, `git commit`, or `git push`.
- [x] No task adds legacy Transaction, Review v1, parsed-node fallback, or GraphWrite-owned Event lifecycle taxonomy.
- [x] All new C++ classes have independent `.h/.cpp` files.
- [x] All verification commands use UE 5.6 paths and write report output under `D:\UEProjects\Template\Saved\Automation`.
