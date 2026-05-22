# GraphWrite 80% Capability P5 Struct Event Delegate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 补齐 struct make/break、custom event、component-bound event、delegate bind/assign 的 evidence 链路，并让缺上下文时返回可执行的细分诊断。

**Architecture:** Struct 属于 GenericAssetStructControlActionCluster 的 value/struct action resolution；custom event 属于 EventDelegateActionCluster 的已支持路径；component-bound event 和 delegate bind/assign 只有在 component、binding object、delegate signature evidence 完整时才能进入 UE spawner family。缺 evidence 必须返回 `NeedsMoreSemanticContext`，不能退回旧 fallback 或泛化 unsupported。

**Tech Stack:** UE 5.6 C++、GenericAssetStructControlActionCluster、EventDelegateActionCluster、ActionContextPipeline、UBlueprintEventNodeSpawner、delegate spawner families、Automation Tests。

---

## Files

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericAssetStructControlActionClusterTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteLegacyMainlineContractTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWrite80PercentCapabilityTests.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateActionClusterTests.cpp`
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_TestRecord_20260522_CN.md`
- Modify if unresolved: `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`

## Manual Commit Policy

- P5 执行者不得自动提交。
- If a delegate spawner family cannot be invoked safely, record the exact reason in the gap document.

## Task 1: Complete Struct Make/Break Evidence

- [ ] **Step 1: Add struct tests**

In `BlueprintHelperGenericAssetStructControlActionClusterTests.cpp`, add tests:

```text
construct vector-like struct with explicit target type
deconstruct vector-like struct with explicit target type
construct missing target type -> missing_required_evidence
deconstruct unsupported struct -> not_found or unsupported_intent
```

Positive tests assert:

```text
Status == Resolved
SelectedSpawner != null
SelectedStableId contains struct identity
CandidateActions include StructType or function/action identity
```

- [ ] **Step 2: Harden provider boundary**

In `BlueprintHelperGenericActionProviderBoundary.cpp`:

```text
Construct/Deconstruct require target type evidence or a valid NodeSpawnerCandidate semantic.
Create/Convert/Schedule remain unsupported or needs-more-context until their own evidence contract exists.
Unsupported paths must not return success through direct struct fallback.
```

- [ ] **Step 3: Implement missing evidence diagnostics**

In `BlueprintHelperGenericAssetStructControlActionResolver.cpp`, map struct failures:

```text
missing target type -> missing_required_evidence
candidate list too broad -> candidate_threshold_exceeded
multiple make/break candidates -> ambiguous_candidates
no make/break candidate -> not_found
```

## Task 2: Add EventDelegate Tests

- [ ] **Step 1: Create EventDelegate test file**

Create `BlueprintHelperEventDelegateActionClusterTests.cpp` with tests:

```text
custom event with event name -> Resolved
custom event missing event name -> missing_required_evidence
component_bound_event missing component -> missing_required_evidence
component_bound_event missing delegate signature -> missing_required_evidence
bind missing binding object -> missing_required_evidence
bind missing delegate signature -> missing_required_evidence
```

Positive custom event assertions:

```text
Status == Resolved
SelectedSpawner != null
SelectedStableId contains event name
CandidateActions include UBlueprintEventNodeSpawner evidence
```

Negative delegate assertions:

```text
Status != Resolved
Diagnostic reason uses missing_required_evidence
Diagnostic includes one of component_missing, binding_object_missing, delegate_signature_missing
```

- [ ] **Step 2: Run EventDelegate tests before implementation**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.EventDelegate;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite80_P5_EventDelegate_Before_001'
```

Expected: new negative tests fail until diagnostics are implemented; record failures in the task notes.

## Task 3: Project Event/Delegate Context

- [ ] **Step 1: Extend demand collector**

In `BlueprintHelperActionContextDemandCollector.cpp`, ensure these semantics request binding evidence:

```text
ComponentBoundEvent
Bind
Assign
Unbind
DelegateCall
DelegateClear
```

Required evidence:

```text
component path or object reference
binding object path
delegate name
delegate signature
target graph context
```

- [ ] **Step 2: Extend inference service**

In `BlueprintHelperActionContextInferenceService.cpp`, project available TaskSpec/SemanticIR fields into evidence keys:

```text
component_path
binding_object_path
delegate_name
delegate_signature
target_graph
```

If a key cannot be inferred, leave it absent; do not fabricate defaults.

## Task 4: Implement EventDelegate Diagnostics and Success Paths

- [ ] **Step 1: Custom event remains success path**

Keep current custom event behavior:

```text
event + event_name -> UBlueprintEventNodeSpawner::Create(...)
```

Add explicit missing event name diagnostic if no event name exists.

- [ ] **Step 2: Component-bound and bind missing evidence diagnostics**

In `BlueprintHelperEventDelegateActionCluster.cpp`, for component-bound/bind semantics:

```text
If component evidence is missing, return NeedsMoreSemanticContext with component_missing.
If binding object is missing, return NeedsMoreSemanticContext with binding_object_missing.
If delegate signature is missing, return NeedsMoreSemanticContext with delegate_signature_missing.
```

- [ ] **Step 3: Add spawner success only when evidence is complete**

When evidence is complete, route to the correct UE spawner family. If a UE spawner cannot be invoked because the current API shape is unresolved, return `unsupported_intent` and write the exact missing API boundary in the gap document.

Do not declare `ComponentBoundEvent` or `Bind` fully complete until there is a positive test with `SelectedSpawner != null`.

## Task 5: Update Capability Scenario B

- [ ] **Step 1: Add EventDrivenConfigApplier metrics**

Update P2 capability tests with:

```text
custom event success
make/break struct success or precise missing evidence
delegate/bind missing evidence diagnostics
no legacy fallback success
```

- [ ] **Step 2: Update test record**

Record the actual P5 status for `EventDrivenConfigApplier`:

```markdown
| EventDrivenConfigApplier | GraphWrite | Struct / Event / Delegate | P5 executed | none | 是 | 是 | `Saved/Automation/GraphWrite80_P5_EventStruct_001/index.json` | 不需要 |
| EventDrivenConfigApplier | GraphWrite | Struct / Event / Delegate | P5 executed | missing_required_evidence | 否 | 否 | `Saved/Automation/GraphWrite80_P5_EventDelegate_001/index.json` | 不需要，诊断符合预期 |
| EventDrivenConfigApplier | GraphWrite | Struct / Event / Delegate | P5 executed | unsupported_intent | 否 | 否 | `Saved/Automation/GraphWrite80_P5_EventDelegate_001/index.json` | `BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md` |
```

Use exactly one row per executed result. Pick the `none` row only when the scenario is fully correct; pick the diagnostic rows when the run proves a controlled failure.

## Task 6: Verification

- [ ] **Step 1: Run Generic struct tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite80_P5_GenericStruct_001'
```

Expected: struct tests pass or unresolved positive delegate paths are recorded as gap.

- [ ] **Step 2: Run EventDelegate tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.EventDelegate;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite80_P5_EventDelegate_001'
```

Expected: custom event positive and missing evidence diagnostics pass. Component-bound/bind positive paths pass only if UE spawner evidence is complete; otherwise gap doc records exact blocker.

- [ ] **Step 3: Run BuildPlugin**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat' BuildPlugin -Plugin='D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\BlueprintHelper.uplugin' -Package='D:\UEProjects\Template\Saved\BlueprintHelperBuildTest_GraphWrite80_P5' -TargetPlatforms=Win64
```

Expected: `BUILD SUCCESSFUL`.

## P5 Exit Criteria

- [ ] Struct make/break has explicit evidence and diagnostics.
- [ ] Custom event remains resolved through event spawner evidence.
- [ ] Component-bound/bind missing evidence returns precise `NeedsMoreSemanticContext`.
- [ ] Complete delegate evidence either resolves through UE spawner family or records an exact gap.
- [ ] EventDrivenConfigApplier metrics are updated.
