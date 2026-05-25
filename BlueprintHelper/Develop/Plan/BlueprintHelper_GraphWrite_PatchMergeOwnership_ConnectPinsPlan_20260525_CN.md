# GraphWrite Patch/Merge Ownership + ConnectPins Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 收敛审计项 1 + 5：明确 Patch/Merge graph mutation ownership，并修复 `Patch ConnectPins`，使 pin link 变更只通过统一 mutation intent/coordinator 执行。

**Architecture:** Patch/Merge service 只负责解析 payload、resolve anchor、构造 `FBlueprintHelperGraphWriteMutationIntent`；`FBlueprintHelperGraphWriteMutationCoordinator` 是 pin link mutation 的唯一执行 owner。`Patch ConnectPins` 不新增独立 mutator，不引入 `ApplyConnectPins` / `TryCreateConnection` / `BreakLinkTo` 分支。

**Tech Stack:** UE 5.6 C++、BlueprintHelper GraphWrite service、Automation Tests、`Build.bat`、`UnrealEditor-Cmd.exe Automation RunTests`。

---

## Scope

本计划只处理：

- 审计项 1：Merge/Patch ownership boundary。
- 审计项 5：Patch ConnectPins。

本计划不处理：

- 审计项 2：Merge callable convergence。
- 审计项 3：EventDelegate taxonomy / handler scan。
- 任何 GraphStatement / EventDelegate / FunctionActionCluster 行为改造。

## File Structure

允许修改：

- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationCoordinator.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationIntent.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteLegacyMainlineContractTests.cpp`
- `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_PatchMergeOwnership_ConnectPinsPlan_20260525_CN.md`

禁止修改：

- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.cpp` 中 callable 创建逻辑；该问题归第二份计划。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/*`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp`

## Ownership Decision

最终边界：

- `FBlueprintHelperPatchBlueprintGraphService` owns payload parsing, target resolving, preflight, transaction, and result envelope.
- `FBlueprintHelperGraphWriteMutationCoordinator` owns pin/default/link graph mutation execution.
- `FBlueprintHelperGraphWriteMutationIntent` owns the structured mutation contract between Patch/Merge services and the coordinator.
- Patch service must not directly call schema link APIs.

Current known problem:

`BlueprintHelperPatchBlueprintGraphService.cpp` currently has a broken `ConnectPins` branch: it does not read source endpoint fields from `Request.PatchPayload`, then attempts to resolve empty `FromNodeRef` / `FromPinRef`.

---

## Task 1: RED Contract Tests for Ownership Boundary

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteLegacyMainlineContractTests.cpp`

- [ ] **Step 1: Add/strengthen ownership token checks**

In `FBlueprintHelperMergePatchUsesMutationCoordinatorContractTest`, keep the existing forbidden tokens and add a required-token check that Patch/Merge use mutation intents and the coordinator.

Add this helper near the existing source assertion helpers if one does not already exist:

```cpp
static bool AssertSourceContainsTokens(
	FAutomationTestBase& Test,
	const FString& RelativePath,
	const TArray<FString>& RequiredTokens)
{
	FString Source;
	if (!LoadSource(Test, RelativePath, Source))
	{
		return false;
	}

	bool bClean = true;
	for (const FString& Token : RequiredTokens)
	{
		if (!Source.Contains(Token))
		{
			Test.AddError(FString::Printf(TEXT("%s must contain current ownership token: %s"), *RelativePath, *Token));
			bClean = false;
		}
	}
	return bClean;
}
```

Then update the test body:

```cpp
bClean &= AssertSourceContainsTokens(
	*this,
	TEXT("Private/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.cpp"),
	{
		TEXT("EBlueprintHelperGraphWriteMutationIntentKind::ConnectPins"),
		TEXT("EBlueprintHelperGraphWriteMutationIntentKind::DisconnectPins"),
		TEXT("EBlueprintHelperGraphWriteMutationIntentKind::ReplacePinConnection"),
		TEXT("FBlueprintHelperGraphWriteMutationCoordinator::ExecuteIntents")
	});
bClean &= AssertSourceContainsTokens(
	*this,
	TEXT("Private/Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.cpp"),
	{
		TEXT("FBlueprintHelperGraphWriteMutationCoordinator::ExecuteIntents"),
		TEXT("EBlueprintHelperGraphWriteMutationIntentKind::AppendSemanticBody"),
		TEXT("EBlueprintHelperGraphWriteMutationIntentKind::InsertSemanticBodyBetweenPins"),
		TEXT("EBlueprintHelperGraphWriteMutationIntentKind::BranchForkSemanticBody")
	});
```

- [ ] **Step 2: Run RED contract test**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.LegacyMainline.MergePatchUsesMutationCoordinator; Quit' -log
```

Expected:

- Build passes.
- Contract test may already pass if required tokens exist.
- If it fails, failure must point to missing coordinator/intents, not to EventDelegate or callable changes.

---

## Task 2: RED Functional Tests for Patch ConnectPins

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp`

- [ ] **Step 1: Add helper to build connect_pins payload**

Add a local helper next to existing graph write payload helpers:

```cpp
static TSharedPtr<FJsonObject> MakePatchConnectPinsPayload(
	const FString& AssetPath,
	const FString& GraphName,
	const FString& SourceNodeRef,
	const FString& SourcePinRef,
	const FString& TargetNodeRef,
	const FString& TargetPinRef,
	bool bDryRun)
{
	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();

	TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), AssetPath);
	Target->SetStringField(TEXT("graph"), GraphName);
	Target->SetStringField(TEXT("patch_scope"), TEXT("pin"));
	Payload->SetObjectField(TEXT("target"), Target);

	Payload->SetStringField(TEXT("patch_type"), TEXT("connect_pins"));
	Payload->SetBoolField(TEXT("dry_run"), bDryRun);

	TSharedPtr<FJsonObject> PatchedRef = MakeShared<FJsonObject>();
	PatchedRef->SetStringField(TEXT("node_ref"), TargetNodeRef);
	PatchedRef->SetStringField(TEXT("pin_ref"), TargetPinRef);
	Payload->SetObjectField(TEXT("patched_ref"), PatchedRef);

	TSharedPtr<FJsonObject> Patch = MakeShared<FJsonObject>();
	Patch->SetStringField(TEXT("source_node_ref"), SourceNodeRef);
	Patch->SetStringField(TEXT("source_pin_ref"), SourcePinRef);
	Payload->SetObjectField(TEXT("patch"), Patch);

	return Payload;
}
```

- [ ] **Step 2: Add a dry-run preflight success test**

Add an automation test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchConnectPinsDryRunResolvesEndpointsTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchConnectPinsDryRunResolvesEndpoints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchConnectPinsDryRunResolvesEndpointsTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchConnectPinsDryRun"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Source = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("PatchConnectSource"));
	UK2Node_CustomEvent* Target = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("PatchConnectTarget"));
	TestNotNull(TEXT("source node exists"), Source);
	TestNotNull(TEXT("target node exists"), Target);
	if (!Source || !Target)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchConnectPinsPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			Source->GetName(),
			TEXT("then"),
			Target->GetName(),
			TEXT("execute"),
			true));

	TestTrue(TEXT("connect_pins dry-run succeeds"), Result.bOk);
	TestEqual(TEXT("operation is patch_blueprint_graph"), Result.Operation, FString(TEXT("patch_blueprint_graph")));
	return true;
}
```

- [ ] **Step 3: Add an execute success test**

Add an automation test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchConnectPinsExecutesViaCoordinatorTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchConnectPinsExecutesViaCoordinator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchConnectPinsExecutesViaCoordinatorTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchConnectPinsExecute"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Source = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("PatchConnectExecSource"));
	UK2Node_CustomEvent* Target = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("PatchConnectExecTarget"));
	UEdGraphPin* SourceThen = Source ? Source->FindPin(TEXT("then")) : nullptr;
	UEdGraphPin* TargetExec = Target ? Target->FindPin(TEXT("execute")) : nullptr;
	TestNotNull(TEXT("source then pin exists"), SourceThen);
	TestNotNull(TEXT("target execute pin exists"), TargetExec);
	if (!SourceThen || !TargetExec)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchConnectPinsPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			Source->GetName(),
			TEXT("then"),
			Target->GetName(),
			TEXT("execute"),
			false));

	TestTrue(TEXT("connect_pins execute succeeds"), Result.bOk);
	TestEqual(TEXT("source then has one linked pin"), SourceThen->LinkedTo.Num(), 1);
	TestTrue(TEXT("source then links to target execute"), SourceThen->LinkedTo.Contains(TargetExec));
	return true;
}
```

- [ ] **Step 4: Run RED functional tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ToolResultBase.PatchConnectPins; Quit' -log
```

Expected RED:

- Dry-run should pass if preflight resolves endpoints.
- Execute likely fails before implementation with `Unable to resolve source node:` because source endpoint fields are not read from `patch`.

---

## Task 3: Implement Source Endpoint Parsing for ConnectPins

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.cpp`

- [ ] **Step 1: Add source endpoint fields to `FPatchRequest`**

In `FPatchRequest`, add explicit fields:

```cpp
FString SourceNodeRef;
FString SourcePinRef;
FString SourceNodePath;
FString SourcePinPath;
```

- [ ] **Step 2: Parse source endpoint from `patch`**

In `ParseRequest`, inside the existing `if (Payload->TryGetObjectField(TEXT("patch"), PatchObj)...)` block, add:

```cpp
(*PatchObj)->TryGetStringField(TEXT("source_node_ref"), Req.SourceNodeRef);
(*PatchObj)->TryGetStringField(TEXT("source_pin_ref"), Req.SourcePinRef);
(*PatchObj)->TryGetStringField(TEXT("source_node_path"), Req.SourceNodePath);
(*PatchObj)->TryGetStringField(TEXT("source_pin_path"), Req.SourcePinPath);
```

- [ ] **Step 3: Add a private endpoint resolver helper**

Add this private method declaration:

```cpp
bool ResolvePatchSourcePin(
	UEdGraph* Graph,
	const FPatchRequest& Request,
	UEdGraphPin*& OutPin,
	FString& OutError) const;
```

Add this implementation:

```cpp
bool FBlueprintHelperPatchBlueprintGraphService::ResolvePatchSourcePin(
	UEdGraph* Graph,
	const FPatchRequest& Request,
	UEdGraphPin*& OutPin,
	FString& OutError) const
{
	OutPin = nullptr;
	if (!Graph)
	{
		OutError = TEXT("target_graph_invalid");
		return false;
	}
	if (Request.SourceNodeRef.IsEmpty() && Request.SourceNodePath.IsEmpty())
	{
		OutError = TEXT("connect_pins requires patch.source_node_ref or patch.source_node_path.");
		return false;
	}
	if (Request.SourcePinRef.IsEmpty() && Request.SourcePinPath.IsEmpty())
	{
		OutError = TEXT("connect_pins requires patch.source_pin_ref or patch.source_pin_path.");
		return false;
	}

	FBlueprintHelperPatchResolveError ResolveError;
	UEdGraphNode* SourceNode = nullptr;
	if (!PathService.ResolveNode(Graph, Request.SourceNodeRef, Request.SourceNodePath, SourceNode, ResolveError))
	{
		OutError = ResolveError.Message.IsEmpty()
			? FString::Printf(TEXT("Unable to resolve source node: %s"), *Request.SourceNodeRef)
			: ResolveError.Message;
		return false;
	}

	if (!PathService.ResolvePin(Graph, SourceNode, Request.SourcePinRef, Request.SourcePinPath, OutPin, ResolveError))
	{
		OutError = ResolveError.Message.IsEmpty()
			? FString::Printf(TEXT("Unable to resolve source pin: %s"), *Request.SourcePinRef)
			: ResolveError.Message;
		return false;
	}

	return true;
}
```

- [ ] **Step 4: Use helper in `ApplyPatch` ConnectPins branch**

Replace the current local `FromNodeRef` / `FromPinRef` code in the `ConnectPins` case with:

```cpp
UEdGraphPin* FromPin = nullptr;
if (!ResolvePatchSourcePin(Graph, Request, FromPin, OutError))
{
	return false;
}

FBlueprintHelperGraphWriteMutationIntent Intent;
Intent.Kind = EBlueprintHelperGraphWriteMutationIntentKind::ConnectPins;
Intent.IntentId = TEXT("patch_connect_pins");
Intent.Source.Pin = FromPin;
Intent.Target.Pin = Target.Pin;
return ExecuteMutationIntent(Graph, Intent, bOutChanged, OutError);
```

- [ ] **Step 5: Run PatchConnectPins tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ToolResultBase.PatchConnectPins; Quit' -log
```

Expected:

- Both PatchConnectPins tests pass.

---

## Task 4: Ensure Mutation Coordinator Remains the Execution Owner

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationCoordinator.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationIntent.h`
- Modify only if tests reveal missing diagnostics.

- [ ] **Step 1: Check `ExecuteIntents` diagnostics for pin link cases**

Inspect `ExecuteIntents` cases:

```cpp
case EBlueprintHelperGraphWriteMutationIntentKind::ConnectPins:
case EBlueprintHelperGraphWriteMutationIntentKind::DisconnectPins:
case EBlueprintHelperGraphWriteMutationIntentKind::ReplacePinConnection:
```

Do not add Patch-specific branches. If error messages are empty, make coordinator helpers produce stable messages:

```cpp
if (!SourcePin || !TargetPin)
{
	OutError = TEXT("pin_endpoint_invalid");
	return false;
}
```

- [ ] **Step 2: Add only coordinator-level missing endpoint tests if needed**

If a null endpoint currently crashes or returns an empty message, add tests in the smallest existing mutation coordinator test file:

- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintGraphMutationPlanExecutorTests.cpp`, or
- a new focused test file only if there is no coordinator-level test seam.

Expected assertion:

```cpp
TestFalse(TEXT("connect with missing source fails"), Result.bSucceed);
TestEqual(TEXT("stable error message"), Result.Message, FString(TEXT("pin_endpoint_invalid")));
```

- [ ] **Step 3: Run coordinator-related tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite; Quit' -log
```

Expected:

- No regression in GraphWrite automation tests.

---

## Task 5: Update Plan Status and Run Final Verification

**Files:**

- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_PatchMergeOwnership_ConnectPinsPlan_20260525_CN.md`

- [ ] **Step 1: Add execution result section**

Append:

```markdown
## Execution Result

- Patch ConnectPins source endpoint parsing implemented.
- Patch/Merge direct pin mutation remains forbidden by source contract.
- Patch ConnectPins uses `FBlueprintHelperGraphWriteMutationCoordinator::ExecuteIntents`.
- No Merge callable or EventDelegate behavior was changed in this batch.
```

- [ ] **Step 2: Run final verification**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ToolResultBase.PatchConnectPins; Quit' -log
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.LegacyMainline.MergePatchUsesMutationCoordinator; Quit' -log
git diff --check
```

Expected:

- Build exit code `0`.
- Both automation runs exit code `0`.
- `git diff --check` exit code `0`; Windows LF/CRLF warnings are acceptable if no whitespace error is reported.

- [ ] **Step 3: Checkpoint without git commit**

Do not run `git add`, `git commit`, or `git push`.

Report changed files and suggested manual commit message:

```text
修复内容：
1. 修复 Patch ConnectPins source endpoint 解析并统一走 GraphWrite mutation coordinator
2. 增强 Patch/Merge mutation ownership contract 测试
```

## Self-Review Checklist

- [ ] Plan has no EventDelegate implementation task.
- [ ] Plan has no Merge callable convergence task.
- [ ] Patch service remains adapter/preflight/result owner only.
- [ ] Mutation coordinator remains pin mutation execution owner.
- [ ] Tests prove ConnectPins dry-run and execute paths.

## Execution Result

- Patch ConnectPins source endpoint parsing implemented via `patch.source_node_ref` / `patch.source_pin_ref` with `source_node_path` / `source_pin_path` support.
- Patch dry-run / preflight now validates ConnectPins source endpoint without mutating the graph.
- Patch ConnectPins execute path builds `EBlueprintHelperGraphWriteMutationIntentKind::ConnectPins` and routes execution through `FBlueprintHelperGraphWriteMutationCoordinator::ExecuteIntents`.
- Patch/Merge source contract tests now assert coordinator and mutation-intent ownership tokens while keeping direct schema link mutation forbidden.
- No Merge callable convergence, EventDelegate taxonomy, or GraphStatement behavior changes were included in this batch.

## Verification Result

- `Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload`: passed.
- `Automation RunTests BlueprintHelper.GraphWrite.ToolResultBase.PatchConnectPins`: passed, 6 tests.
- `Automation RunTests BlueprintHelper.GraphWrite.LegacyMainline.MergePatchUsesMutationCoordinator`: passed, 1 test.
- `git diff --check`: passed; only LF/CRLF warnings were emitted.
