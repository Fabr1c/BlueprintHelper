# GraphWrite 80% Capability P4 Direct Spawn Provider Generic Control Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 branch、sequence、select、return 等 canonical singleton / 唯一控制流 direct spawn 收敛到显式 evidence provider，并确保 builder/composer/mutation helper 不再私自创建控制流节点。

**Architecture:** direct spawn 是 GenericAssetStructControlActionCluster 内部的二级语义映射策略，不改变一级分发规则。所有 singleton/control-flow 节点必须返回统一 `ActionResolutionResult` 和 spawner evidence，并通过 shared spawn adapter 执行。

**Tech Stack:** UE 5.6 C++、GenericAssetStructControlActionCluster、UBlueprintNodeSpawner、ActionNodeSpawnerAdapter、GraphStatement builders、Automation Tests。

---

## Files

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSingletonControlFlowEvidenceProvider.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSingletonControlFlowEvidenceProvider.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFragmentBuilder.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSelectFragmentBuilder.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationCoordinator.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericAssetStructControlActionClusterTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteLegacyMainlineContractTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWrite80PercentCapabilityTests.cpp`
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_TestRecord_20260522_CN.md`

## Manual Commit Policy

- P4 执行者不得自动提交。
- 如果 MutationCoordinator 暂时仍必须直接创建 sequence，必须写入 gap 文档并说明删除前置条件。

## Task 1: Add Singleton Evidence Provider

- [ ] **Step 1: Create provider header**

Create `BlueprintHelperSingletonControlFlowEvidenceProvider.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class UBlueprintNodeSpawner;

enum class EBlueprintHelperSingletonControlFlowKind : uint8
{
	Branch,
	Sequence,
	Return,
	Select,
	Unknown
};

struct FBlueprintHelperSingletonControlFlowEvidence
{
	EBlueprintHelperSingletonControlFlowKind SingletonKind = EBlueprintHelperSingletonControlFlowKind::Unknown;
	TSubclassOf<UEdGraphNode> NodeClass = nullptr;
	FString StableId;
	FString Reason;
};

class BLUEPRINTHELPER_API FBlueprintHelperSingletonControlFlowEvidenceProvider
{
public:
	static bool TryResolve(
		const FBlueprintHelperActionResolutionRequest& Request,
		FBlueprintHelperSingletonControlFlowEvidence& OutEvidence);

	static FBlueprintHelperActionResolutionResult MakeResolvedResult(
		const FBlueprintHelperActionResolutionRequest& Request,
		const FBlueprintHelperSingletonControlFlowEvidence& Evidence);
};
```

- [ ] **Step 2: Implement provider**

Create implementation mapping:

```text
control + branch -> UK2Node_IfThenElse
control + sequence -> UK2Node_ExecutionSequence
control + return -> UK2Node_FunctionResult
select -> UK2Node_Select
```

Provider must set:

```text
semantic kind
singleton kind
node class path
stable id
reason
SelectedSpawner = UBlueprintNodeSpawner::Create(NodeClass)
```

It must return false for wide-surface semantics such as `call`, `get`, `set`, `bind`, `create`, `convert`, `schedule`.

## Task 2: Route Generic Resolver Through Provider

- [ ] **Step 1: Replace local direct spawn construction**

In `BlueprintHelperGenericAssetStructControlActionResolver.cpp`, replace local `UBlueprintNodeSpawner::Create(NodeClass)` singleton branches with:

```text
FBlueprintHelperSingletonControlFlowEvidenceProvider::TryResolve(...)
FBlueprintHelperSingletonControlFlowEvidenceProvider::MakeResolvedResult(...)
```

- [ ] **Step 2: Keep construct/deconstruct separate**

Do not route struct make/break through singleton control provider. Construct/deconstruct remain struct/action resolver responsibilities.

- [ ] **Step 3: Verify provider boundary**

Run:

```powershell
rg -n "UBlueprintNodeSpawner::Create\\(NodeClass\\)|UBlueprintNodeSpawner::Create\\(UK2Node_Select::StaticClass\\(\\)\\)" BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite
```

Expected: matches are only inside `BlueprintHelperSingletonControlFlowEvidenceProvider.cpp` or explicit tests checking forbidden usage.

## Task 3: Remove Direct Spawn From Builders and Mutation Coordinator

- [ ] **Step 1: Update ControlFragmentBuilder**

`BlueprintHelperControlFragmentBuilder.cpp` must:

```text
Build semantic request.
Call ActionResolutionCore / SpawnerClusterResolver.
Consume ActionResolutionResult.
Invoke shared ActionNodeSpawnerAdapter.
Never create UK2Node_* or UBlueprintNodeSpawner directly.
```

- [ ] **Step 2: Update SelectFragmentBuilder**

`BlueprintHelperSelectFragmentBuilder.cpp` must use Generic cluster evidence for select. It may configure select pins after spawn through shared lifecycle/default adapter, but it must not create `UK2Node_Select` locally.

- [ ] **Step 3: Update MutationCoordinator**

`BlueprintHelperGraphWriteMutationCoordinator.cpp` must request sequence through Generic cluster singleton provider before creating branch-fork sequence. If the provider cannot resolve sequence, return `spawn_or_link_failure` or `missing_required_evidence` with DebugBundle context.

## Task 4: Add Boundary Tests

- [ ] **Step 1: Add provider positive tests**

In `BlueprintHelperGenericAssetStructControlActionClusterTests.cpp`, add tests:

```text
Generic control branch resolves singleton evidence.
Generic control sequence resolves singleton evidence.
Generic select resolves singleton evidence.
Generic control return resolves singleton evidence when graph context supports return.
```

Each test asserts:

```text
Status == Resolved
SelectedSpawner != null
SelectedStableId contains singleton/control-flow identity
CandidateActions contains node class path
```

- [ ] **Step 2: Add wide-surface negative tests**

Provider must reject:

```text
call
get
set
bind
create
convert
schedule
```

Expected: provider returns false and no `SelectedSpawner`.

- [ ] **Step 3: Extend legacy mainline contract tests**

Add forbidden token checks so these files cannot directly call singleton direct spawn:

```text
BlueprintHelperControlFragmentBuilder.cpp
BlueprintHelperSelectFragmentBuilder.cpp
BlueprintHelperGraphWriteMutationCoordinator.cpp
BlueprintHelperGraphComposer.cpp
```

Forbidden tokens:

```text
SpawnK2Node<
NewObject<UK2Node
UBlueprintNodeSpawner::Create(UK2Node_Select::StaticClass())
UBlueprintNodeSpawner::Create(NodeClass)
UK2Node_ExecutionSequence::StaticClass()
UK2Node_IfThenElse::StaticClass()
```

Allowed file:

```text
BlueprintHelperSingletonControlFlowEvidenceProvider.cpp
```

## Task 5: Update Capability Metrics

- [ ] **Step 1: Add TimedAccessGate control evidence case**

Update P2 capability tests so `TimedAccessGate_StateMachine` records:

```text
branch resolved through singleton provider
sequence resolved through singleton provider
select resolved through singleton provider
no builder/composer/mutation direct spawn
```

- [ ] **Step 2: Update test record**

Record:

```markdown
| TimedAccessGate_StateMachine | GraphWrite | Function / Field / Control | P4 control provider verified | none | 是/否 | 是/否 | `Saved/Automation/GraphWrite80_P4_GenericControl_001/index.json` | 不需要或 gap path |
```

Use `是` only when positive tests and contract tests pass.

## Task 6: Verification

- [ ] **Step 1: Run Generic cluster tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite80_P4_GenericControl_001'
```

Expected: Generic action resolution tests pass.

- [ ] **Step 2: Run legacy contract tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.LegacyMainline;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite80_P4_LegacyContract_001'
```

Expected: no builder/composer/mutation direct singleton spawn violations.

- [ ] **Step 3: Build plugin**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat' BuildPlugin -Plugin='D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\BlueprintHelper.uplugin' -Package='D:\UEProjects\Template\Saved\BlueprintHelperBuildTest_GraphWrite80_P4' -TargetPlatforms=Win64
```

Expected: `BUILD SUCCESSFUL`.

## P4 Exit Criteria

- [ ] Singleton/control-flow evidence provider exists.
- [ ] branch/sequence/select/return resolve through provider.
- [ ] builder/composer/mutation helper cannot directly spawn singleton control nodes.
- [ ] Wide-surface semantics cannot use direct spawn fallback.
- [ ] Generic and legacy contract tests pass.

