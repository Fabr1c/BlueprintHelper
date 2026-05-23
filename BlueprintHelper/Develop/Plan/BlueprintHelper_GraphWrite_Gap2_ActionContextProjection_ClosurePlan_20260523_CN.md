# GraphWrite Gap2 ActionContext Projection Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close Gap 2 by removing GraphStatementBuilder-owned local action-context demand construction and local semantic-to-cluster projection.

**Architecture:** GraphStatementBuilder may express statement/expression semantics, but ActionContext owns demand construction and cluster projection. `FBlueprintHelperActionContextDemandCollector` remains the single boundary that maps `EBlueprintHelperActionSemanticKind` to `EBlueprintHelperSpawnerClusterKind`, and `FBlueprintHelperActionContextScope` / bundle projection remains the only route for building `FBlueprintHelperActionResolutionRequest`.

**Tech Stack:** UE 5.6 C++, Unreal Automation Tests, BlueprintHelper GraphWrite ActionContext pipeline, PowerShell verification commands.

---

## Execution Rules

- Do not run `git add`, `git commit`, or `git push`; provide manual commands only after completion.
- Treat `AGENT.md` as unrelated local dirt unless the user explicitly scopes it into this task.
- Use code migration model `5.5high` for implementation workers.
- Use `5.4-mini-0xhigh` for medium-range audits after each task.
- `5.3spark` is acceptable only for small source-contract spot checks.
- Close the gap only after source contracts, focused automation, full GraphWrite regression, UE 5.6 compile, and docs all agree.

## Current Gap

`BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md` records Gap 2 as open because:

- `BlueprintHelperGraphStatementBuilder.cpp` still contains `BuildSingleActionContextDemand`.
- `BlueprintHelperGraphStatementBuilder.cpp` still contains `ResolveSpawnerClusterForSemanticKind`.
- Builder-owned fallback statement ids currently include cluster strings, which couples Builder to projection policy.

Current desired chain:

```text
GraphStatement / SemanticIR
-> ActionContextDemandCollector
-> ActionContextScope / BundleProjector
-> ActionResolutionCore
-> UE NodeSpawner evidence
-> shared adapter
-> FragmentDAG / Builder
```

## File Structure

Modify:

- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h`
  - Expose a single-demand construction API owned by the ActionContext boundary.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
  - Reuse existing `BuildDemand` / `ApplyDemandKinds` so semantic-to-cluster mapping stays in one place.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
  - Remove `BuildSingleActionContextDemand`.
  - Remove `ResolveSpawnerClusterForSemanticKind`.
  - Project requests through ActionContext demand collector and scope, without local cluster mapping.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`
  - Strengthen the source contract so Gap 2 cannot regress.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`
  - Add direct unit coverage for the single-demand ActionContext API.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`
  - Mark Gap 2 closed only after verification passes.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`
  - Sync architecture progress without overclaiming Gap 5.

Do not modify:

- `D:/UEProjects/Template/Plugins/BlueprintHelper/AGENT.md`
- EventDelegate component-bound/bind implementation files, except if a verification compile issue requires a narrow include-only fix.

---

### Task 1: Red Source Contract For Gap 2

**Files:**

- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`

- [ ] **Step 1: Extend the existing GraphStatement projection contract**

In `FBlueprintHelperActionResolutionGraphStatementProjectionContractTest::RunTest`, extend `ForbiddenTokens` to include the current Gap 2 tokens:

```cpp
const TArray<FString> ForbiddenTokens = {
	BuildForbiddenActionResolutionToken(TEXT("ActionRequest."), TEXT("ClusterKind =")),
	BuildForbiddenActionResolutionToken(TEXT("ActionRequest."), TEXT("Semantic =")),
	TEXT("BuildSingleActionContextDemand("),
	TEXT("ResolveSpawnerClusterForSemanticKind("),
	TEXT("Demand.ClusterKind ="),
	TEXT("Demand.SemanticKind ="),
	TEXT("ContextDemands.Add(BuildSingleActionContextDemand(")
};
```

Keep the error text specific:

```cpp
AddError(FString::Printf(
	TEXT("GraphStatementBuilder must not own local ActionContext demand construction or semantic-to-cluster projection; forbidden token '%s' found in %s"),
	*Token,
	*GraphStatementBuilderPath));
```

- [ ] **Step 2: Compile the red contract**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected:

```text
BUILD SUCCESSFUL
```

- [ ] **Step 3: Run the red contract**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Contract.GraphStatementUsesActionContextProjection;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap2_SourceContract_RED_001'
```

Expected result before implementation:

```text
Failed: GraphStatementBuilder must not own local ActionContext demand construction or semantic-to-cluster projection
```

- [ ] **Step 3: Audit completion and deviation**

Audit:

```powershell
Get-Content 'D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap2_SourceContract_RED_001\index.json' -Raw | ConvertFrom-Json | Select-Object succeeded,failed,notRun
```

Expected:

```text
failed = 1
```

If the contract passes before implementation, stop and inspect the source scan, because the red test is not proving Gap 2.

---

### Task 2: Add ActionContext-Owned Single-Demand API

**Files:**

- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`

- [ ] **Step 1: Add focused unit tests first**

Append these tests to `BlueprintHelperActionContextPipelineTests.cpp` near the existing demand collector tests:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextSingleDemandMapsSetPropertyTest,
	"BlueprintHelper.GraphWrite.ActionContext.SingleDemand.SetPropertyMapsToFieldVariable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextSingleDemandMapsSetPropertyTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperActionContextDemand Demand =
		FBlueprintHelperActionContextDemandCollector::BuildSingleDemand(
			TEXT("stmt_set_property"),
			TEXT("$.statements[0]"),
			EBlueprintHelperActionSemanticKind::SetProperty,
			TEXT("DoorMesh.RelativeRotation"),
			TEXT("DoorMesh.RelativeRotation"),
			TEXT("DoorMesh.RelativeRotation"),
			TEXT("rotator"),
			FString(),
			FString(),
			{},
			{ TEXT("value") });

	TestEqual(TEXT("set_property cluster"), Demand.ClusterKind, EBlueprintHelperSpawnerClusterKind::FieldVariableAction);
	TestEqual(TEXT("set_property semantic"), Demand.SemanticKind, EBlueprintHelperActionSemanticKind::SetProperty);
	TestEqual(TEXT("set_property query uses property path"), Demand.Query, FString(TEXT("DoorMesh.RelativeRotation")));
	TestTrue(TEXT("set_property requires typed pins"), Demand.RequiredKinds.Contains(EBlueprintHelperActionContextDemandKind::TypedPins));
	TestTrue(TEXT("set_property requires target"), Demand.RequiredKinds.Contains(EBlueprintHelperActionContextDemandKind::Target));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextSingleDemandMapsSelectTest,
	"BlueprintHelper.GraphWrite.ActionContext.SingleDemand.SelectMapsToGenericCluster",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextSingleDemandMapsSelectTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperActionContextDemand Demand =
		FBlueprintHelperActionContextDemandCollector::BuildSingleDemand(
			TEXT("expr_select"),
			TEXT("$.statements[0].value"),
			EBlueprintHelperActionSemanticKind::Select,
			TEXT("select"),
			FString(),
			FString(),
			TEXT("bool"),
			FString(),
			FString(),
			{},
			{ TEXT("condition"), TEXT("then"), TEXT("else") });

	TestEqual(TEXT("select cluster"), Demand.ClusterKind, EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction);
	TestEqual(TEXT("select semantic"), Demand.SemanticKind, EBlueprintHelperActionSemanticKind::Select);
	TestEqual(TEXT("select query"), Demand.Query, FString(TEXT("select")));
	TestTrue(TEXT("select requires typed pins"), Demand.RequiredKinds.Contains(EBlueprintHelperActionContextDemandKind::TypedPins));
	return true;
}
```

- [ ] **Step 2: Run tests to verify compile failure or missing method**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected result before implementation:

```text
error: 'BuildSingleDemand' is not a member of 'FBlueprintHelperActionContextDemandCollector'
```

- [ ] **Step 3: Add the public collector method**

In `BlueprintHelperActionContextDemandCollector.h`, add this public method after `CollectFromStatements`:

```cpp
	static FBlueprintHelperActionContextDemand BuildSingleDemand(
		const FString& StableId,
		const FString& SourcePath,
		EBlueprintHelperActionSemanticKind SemanticKind,
		const FString& Query,
		const FString& TargetPath,
		const FString& PropertyPath,
		const FString& TypeName,
		const FString& SearchMode,
		const FString& AmbiguityPolicy,
		const TArray<FString>& CategoryPriority,
		const TArray<FString>& ArgumentNames);
```

Keep the existing private `BuildDemand` declaration unchanged.

- [ ] **Step 4: Implement the method by delegating to existing collector logic**

In `BlueprintHelperActionContextDemandCollector.cpp`, add this implementation before `BuildDemand`:

```cpp
FBlueprintHelperActionContextDemand FBlueprintHelperActionContextDemandCollector::BuildSingleDemand(
	const FString& StableId,
	const FString& SourcePath,
	EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query,
	const FString& TargetPath,
	const FString& PropertyPath,
	const FString& TypeName,
	const FString& SearchMode,
	const FString& AmbiguityPolicy,
	const TArray<FString>& CategoryPriority,
	const TArray<FString>& ArgumentNames)
{
	return BuildDemand(
		StableId,
		SourcePath,
		SemanticKind,
		Query,
		TargetPath,
		PropertyPath,
		TypeName,
		SearchMode,
		AmbiguityPolicy,
		CategoryPriority,
		ArgumentNames);
}
```

This keeps `ApplyDemandKinds` as the only semantic-to-cluster mapping authority.

- [ ] **Step 5: Run the new collector tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionContext.SingleDemand;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap2_SingleDemand_GREEN_001'
```

Expected:

```text
succeeded = 2
failed = 0
```

- [ ] **Step 6: Audit completion and deviation**

Audit that `ApplyDemandKinds` remains the only mapping site:

```powershell
rg -n "EBlueprintHelperSpawnerClusterKind::FunctionAction|EBlueprintHelperSpawnerClusterKind::FieldVariableAction|EBlueprintHelperSpawnerClusterKind::EventDelegateAction|EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction" BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp
```

Expected:

```text
Cluster enum mappings remain in BlueprintHelperActionContextDemandCollector.cpp.
GraphStatementBuilder.cpp has no semantic-to-cluster switch.
```

---

### Task 3: Migrate GraphStatementBuilder Off Local Demand And Cluster Projection

**Files:**

- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`

- [ ] **Step 1: Add the collector include**

Add:

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h"
```

Keep:

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextScope.h"
```

- [ ] **Step 2: Replace the local statement-id helper**

Replace the current `MakeActionContextStatementId` signature that accepts `EBlueprintHelperSpawnerClusterKind` with this semantic-only version:

```cpp
static FString MakeActionContextStatementId(
	const FString& PreferredStatementId,
	const EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query,
	const FString& TargetPath,
	const FString& TypeName)
{
	const FString TrimmedStatementId = PreferredStatementId.TrimStartAndEnd();
	if (!TrimmedStatementId.IsEmpty())
	{
		return TrimmedStatementId;
	}

	return FString::Printf(
		TEXT("%s:%s:%s:%s"),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(SemanticKind),
		*Query,
		*TargetPath,
		*TypeName);
}
```

This removes cluster identity from fallback id generation.

- [ ] **Step 3: Delete the local demand builder**

Remove the entire function:

```cpp
static FBlueprintHelperActionContextDemand BuildSingleActionContextDemand(...)
```

No replacement function with that name is allowed in `GraphStatementBuilder.cpp`.

- [ ] **Step 4: Update projected request helper to call the collector**

Replace `TryBuildProjectedActionRequestFromContext` with:

```cpp
static bool TryBuildProjectedActionRequestFromContext(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FString& StatementId,
	const EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query,
	const FString& TargetPath,
	const FString& PropertyPath,
	const FString& TypeName,
	const FString& SearchMode,
	const FString& AmbiguityPolicy,
	const TArray<FString>& CategoryPriority,
	const TArray<FString>& ArgumentNames,
	FBlueprintHelperActionResolutionRequest& OutRequest,
	FString& OutError)
{
	OutRequest = FBlueprintHelperActionResolutionRequest();

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	const FBlueprintHelperActionContextDemand ContextDemand =
		FBlueprintHelperActionContextDemandCollector::BuildSingleDemand(
			MakeActionContextStatementId(
				StatementId,
				SemanticKind,
				Query,
				TargetPath,
				TypeName),
			StatementId,
			SemanticKind,
			Query,
			TargetPath,
			PropertyPath,
			TypeName,
			SearchMode,
			AmbiguityPolicy,
			CategoryPriority,
			ArgumentNames);

	if (ActionContextScope)
	{
		return ActionContextScope->TryBuildRequest(
			ContextDemand.StatementId,
			Blueprint,
			TargetGraph,
			OutRequest,
			OutError);
	}

	OutError = FString::Printf(
		TEXT("action_context_scope_required: %s"),
		*ContextDemand.StatementId);
	return false;
}
```

Important review points:

- `ContextDemand.ClusterKind` is never read in Builder.
- Builder does not assign `Demand.ClusterKind`.
- Builder does not assign `Demand.SemanticKind`.
- Builder uses `ActionContextScope->TryBuildRequest` as the projection route.

- [ ] **Step 5: Remove the local semantic-to-cluster switch**

Delete the entire function:

```cpp
static EBlueprintHelperSpawnerClusterKind ResolveSpawnerClusterForSemanticKind(EBlueprintHelperActionSemanticKind Kind)
```

No replacement switch over semantic kind is allowed in `GraphStatementBuilder.cpp`.

- [ ] **Step 6: Update `RequireResolvedActionProvider` signatures**

Change the helper signature from:

```cpp
static bool RequireResolvedActionProvider(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FString& StatementId,
	EBlueprintHelperSpawnerClusterKind ClusterKind,
	EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query,
	const FString& TargetPath,
	const FString& TypeName,
	FBlueprintHelperActionResolutionResult* OutResult,
	FString& OutError)
```

to:

```cpp
static bool RequireResolvedActionProvider(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FString& StatementId,
	EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query,
	const FString& TargetPath,
	const FString& PropertyPath,
	const FString& TypeName,
	const FString& SearchMode,
	const FString& AmbiguityPolicy,
	const TArray<FString>& CategoryPriority,
	FBlueprintHelperActionResolutionResult* OutResult,
	FString& OutError)
```

Inside it, call:

```cpp
const TArray<FString> ArgumentNames;
if (!TryBuildProjectedActionRequestFromContext(
	TargetGraph,
	ActionContextScope,
	StatementId,
	SemanticKind,
	Query,
	TargetPath,
	PropertyPath,
	TypeName,
	SearchMode,
	AmbiguityPolicy,
	CategoryPriority,
	ArgumentNames,
	ActionRequest,
	OutError))
{
	return false;
}
```

Keep the overload without `OutResult`, but update it to pass through the new arguments.

- [ ] **Step 7: Update call sites**

Use these replacements:

```cpp
// BuildCallFunctionFragment
TryBuildProjectedActionRequestFromContext(
	TargetGraph,
	ActionContextScope,
	BoundRequest.ActionContextStatementId.IsEmpty() ? BoundRequest.FragmentId : BoundRequest.ActionContextStatementId,
	EBlueprintHelperActionSemanticKind::Call,
	MakeCallFunctionResolveQuery(BoundRequest),
	ExplicitTargetObjectName,
	FString(),
	BoundRequest.ExpectedReturnType,
	BoundRequest.SearchMode,
	BoundRequest.AmbiguityPolicy,
	BoundRequest.CategoryPriority,
	ArgumentNames,
	ActionRequest,
	OutError)
```

```cpp
// BuildVariableSetFragment
RequireResolvedActionProvider(
	TargetGraph,
	ActionContextScope,
	Request.ActionContextStatementId.IsEmpty() ? Request.FragmentId : Request.ActionContextStatementId,
	EBlueprintHelperActionSemanticKind::Set,
	Request.Target,
	Request.Target,
	FString(),
	Request.ExpectedReturnType,
	FString(),
	FString(),
	{},
	&ActionResult,
	OutError)
```

```cpp
// BuildSetPropertyFragment
RequireResolvedActionProvider(
	TargetGraph,
	ActionContextScope,
	Request.ActionContextStatementId.IsEmpty() ? Request.FragmentId : Request.ActionContextStatementId,
	EBlueprintHelperActionSemanticKind::SetProperty,
	Request.Target,
	Request.Target,
	Request.Target,
	Request.ExpectedReturnType,
	FString(),
	FString(),
	{},
	&ActionResult,
	OutError)
```

```cpp
// Get expression
RequireResolvedActionProvider(
	TargetGraph,
	ActionContextScope,
	MakeExpressionActionContextStatementId(Expression),
	EBlueprintHelperActionSemanticKind::Get,
	VariableName,
	VariableName,
	FString(),
	Expression.Type,
	Expression.SearchMode,
	Expression.AmbiguityPolicy,
	Expression.CategoryPriority,
	&ActionResult,
	OutError)
```

For `ResolveActionProviderForExpression`, remove `ResolveSpawnerClusterForSemanticKind(SemanticKind)` and pass the semantic fields directly into the updated projected request helper.

- [ ] **Step 8: Compile**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected:

```text
BUILD SUCCESSFUL
```

- [ ] **Step 9: Audit completion and deviation**

Run:

```powershell
rg -n "BuildSingleActionContextDemand|ResolveSpawnerClusterForSemanticKind|Demand\\.ClusterKind =|Demand\\.SemanticKind =|ActionRequest\\.ClusterKind =|ActionRequest\\.Semantic =" BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp
```

Expected:

```text
no matches
```

If `FBlueprintHelperActionContextDemand` remains as a local const return value from `BuildSingleDemand`, that is acceptable only if review confirms Builder does not assign its fields and does not read `ClusterKind`.

---

### Task 4: Focused Automation And Regression

**Files:**

- No source modifications unless a focused failure identifies a direct Task 1-3 defect.

- [ ] **Step 1: Run Gap 2 source contract**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Contract.GraphStatementUsesActionContextProjection;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap2_SourceContract_GREEN_001'
```

Expected:

```text
failed = 0
notRun = 0
```

- [ ] **Step 2: Run single-demand collector tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionContext.SingleDemand;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap2_SingleDemand_GREEN_002'
```

Expected:

```text
failed = 0
notRun = 0
```

- [ ] **Step 3: Run existing ActionContext pipeline tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionContext;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap2_ActionContext_GREEN_001'
```

Expected:

```text
failed = 0
```

- [ ] **Step 4: Run GraphWrite regression**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap2_Regression_001'
```

Expected:

```text
failed = 0
notRun = 0
```

Warnings may remain if they match the known non-failing warning profile. New errors or failed tests block closure.

- [ ] **Step 5: Run UE 5.6 compile**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected:

```text
BUILD SUCCESSFUL
```

- [ ] **Step 6: Run whitespace check**

Run:

```powershell
git diff --check
```

Expected:

```text
exit code 0
```

Line-ending warnings are informational if `git diff --check` exits 0.

- [ ] **Step 7: Audit completion and deviation**

Audit:

```powershell
$reports = @(
  'D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap2_SourceContract_GREEN_001\index.json',
  'D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap2_SingleDemand_GREEN_002\index.json',
  'D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap2_ActionContext_GREEN_001\index.json',
  'D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap2_Regression_001\index.json'
)
foreach ($path in $reports) {
  $json = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
  [pscustomobject]@{
    Report = Split-Path (Split-Path $path -Parent) -Leaf
    Succeeded = $json.succeeded
    SucceededWithWarnings = $json.succeededWithWarnings
    Failed = $json.failed
    NotRun = $json.notRun
  }
}
```

Expected:

```text
all Failed values are 0
all NotRun values are 0
```

---

### Task 5: Update Gap And Progress Docs

**Files:**

- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`

- [ ] **Step 1: Mark Gap 2 closed with narrow scope**

Replace the Gap 2 status paragraph with:

```markdown
状态：已关闭（仅限 GraphStatementBuilder 本地 demand / semantic-cluster projection 职责收敛）

Closure scope - 2026-05-23:
- `BlueprintHelperGraphStatementBuilder.cpp` no longer defines `BuildSingleActionContextDemand`.
- `BlueprintHelperGraphStatementBuilder.cpp` no longer defines or calls `ResolveSpawnerClusterForSemanticKind`.
- Single-demand construction is owned by `FBlueprintHelperActionContextDemandCollector::BuildSingleDemand`.
- Semantic kind -> cluster kind mapping remains inside `FBlueprintHelperActionContextDemandCollector::ApplyDemandKinds`.
- GraphStatementBuilder still consumes `FBlueprintHelperActionContextScope::TryBuildRequest`; request projection remains owned by ActionContext bundle projection.
```

Add closure evidence:

```markdown
Closure evidence:
- `D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap2_SourceContract_GREEN_001\index.json`, 0 failed.
- `D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap2_SingleDemand_GREEN_002\index.json`, 0 failed.
- `D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap2_ActionContext_GREEN_001\index.json`, 0 failed.
- `D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap2_Regression_001\index.json`, 0 failed / 0 not run.
- UE 5.6 `Build.bat TemplateEditor Win64 Development`, `BUILD SUCCESSFUL`.
- `git diff --check`, exit code 0.
```

Keep Gap 5 open.

- [ ] **Step 2: Update four-cluster status**

In `BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`, update the residual architecture gap sentence:

```markdown
Gap 2/3/4 closed for their documented scopes; Gap 5 remains open for EventDelegate component-bound/bind positive spawner-family support. Broad Generic create/convert/schedule semantics remain outside the singleton direct-spawn closure.
```

Do not mark any cluster as `完全完成` unless the cluster has no remaining open semantic, context, evidence, test, or architecture gap.

- [ ] **Step 3: Audit docs for overclaim**

Run:

```powershell
rg -n "Gap 5|component-bound|bind|create/convert/schedule|完全完成|Gap 2/3/4 closed" BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md
```

Expected:

```text
Gap 5 remains open.
No cluster is marked 完全完成.
Broad create/convert/schedule remains outside Gap 2 closure.
```

---

### Task 6: Final Review And Closure Gate

**Files:**

- Review all files modified by Tasks 1-5.

- [ ] **Step 1: Request medium-range audit**

Use a `5.4-mini-0xhigh` audit worker with this prompt:

```text
Audit Gap 2 closure only. Do not edit files.

Confirm:
1. BlueprintHelperGraphStatementBuilder.cpp no longer owns BuildSingleActionContextDemand.
2. BlueprintHelperGraphStatementBuilder.cpp no longer owns ResolveSpawnerClusterForSemanticKind or any semantic-to-cluster switch.
3. Single-demand construction is inside ActionContextDemandCollector.
4. Builder does not assign ActionRequest.ClusterKind or ActionRequest.Semantic.
5. Builder still projects requests through FBlueprintHelperActionContextScope::TryBuildRequest.
6. Gap/FourCluster docs close only Gap 2 scope and keep Gap 5 open.
7. Verification evidence exists and has failed=0/notRun=0 where required.

Return PASS or FAIL. If FAIL, list file, line, blocker, and correction.
```

- [ ] **Step 2: Fix any audit blocker**

For each FAIL item, make the smallest code or doc change that restores the stated architecture boundary. Re-run the focused test that proves the fix.

- [ ] **Step 3: Final local source scan**

Run:

```powershell
rg -n "BuildSingleActionContextDemand|ResolveSpawnerClusterForSemanticKind|ActionRequest\\.ClusterKind =|ActionRequest\\.Semantic =|Demand\\.ClusterKind =|Demand\\.SemanticKind =" BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp
```

Expected:

```text
no matches
```

- [ ] **Step 4: Record completion**

The task is complete only when:

```text
Gap 2 source contract PASS
ActionContext focused tests PASS
GraphWrite regression PASS
UE 5.6 compile PASS
git diff --check PASS
medium audit PASS
Gap docs updated without closing Gap 5
```

## Non-Goals

- Do not implement Gap 5 component-bound/bind positive spawner support.
- Do not change EventDelegate support status.
- Do not create direct UE node spawn paths in Builder, Composer, or MutationCoordinator.
- Do not weaken singleton direct-spawn provider boundaries from Gap 3/4.
- Do not add compatibility paths for old parsed-node mainline semantics.

## Suggested Manual Commit Message After Implementation

```text
变更需求：
1. 收敛 Gap 2，将 GraphStatementBuilder 本地 demand 和 semantic-cluster 投影职责移交给 ActionContext 边界

新增内容：
1. 增加 ActionContext single-demand collector API 与 Gap 2 source contract 覆盖

修复内容：
1. 移除 GraphStatementBuilder 内部 BuildSingleActionContextDemand 和 ResolveSpawnerClusterForSemanticKind 局部分发逻辑
```

Manual commit commands after implementation:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper
git add -- `
  BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp `
  BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md `
  BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md `
  BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_Gap2_ActionContextProjection_ClosurePlan_20260523_CN.md
git commit -m "变更需求：收敛 GraphWrite Gap 2 ActionContext 投影边界"
```
