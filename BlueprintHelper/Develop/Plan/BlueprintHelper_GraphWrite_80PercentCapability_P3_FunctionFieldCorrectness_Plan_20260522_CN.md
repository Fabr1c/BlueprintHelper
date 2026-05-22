# GraphWrite 80% Capability P3 Function Field Correctness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 优先提升 `FunctionActionCluster` 与 `FieldVariableActionCluster` 的 resolver 正确率，支撑 PhysicalDoor 和 TimedAccessGate 的 call / variable / property 主路径。

**Architecture:** wide-surface semantics 必须最大化消费 projected context 和 UE ActionDatabase / ActionFilter / NodeSpawner evidence。`TFieldIterator` 等扫描只能作为补充候选来源，不能作为 selected success 的唯一证据。field/property 必须使用显式 owner、field name、property path、pin/type evidence；缺关键 evidence 返回 `NeedsMoreSemanticContext`。

**Tech Stack:** UE 5.6 C++、FunctionActionCluster、FieldVariableActionCluster、ActionContextPipeline、Automation Tests、BlueprintHelper CLI smoke。

---

## Files

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperCallFunctionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperCallFunctionResolverUtils.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperCallFunctionResolverTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldVariableActionClusterTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWrite80PercentCapabilityTests.cpp`
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_TestRecord_20260522_CN.md`
- Modify if unresolved gap remains: `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`

## Manual Commit Policy

- P3 执行者不得自动提交。
- 如果旧扫描路径不能删除，必须在 gap 文档写明它只作为 supplemental candidate，不能单独成功。

## Task 1: Lock Function Candidate Evidence

- [ ] **Step 1: Add a failing contract test for supplemental scan**

In `BlueprintHelperCallFunctionResolverTests.cpp`, add a test named:

```text
BlueprintHelper.GraphWrite.ActionResolution.CallFunction.SupplementalScanCannotSelectWithoutSpawnerEvidence
```

Test behavior:

```text
Given a call request whose only possible match comes from TFieldIterator supplemental scan
When ActionDatabase / ActionFilter candidate evidence is absent
Then resolver must not return Resolved with SelectedSpawner
And diagnostic reason must be missing_required_evidence, not_found, or ambiguous_candidates
```

- [ ] **Step 2: Implement resolver guard**

In `BlueprintHelperCallFunctionResolver.cpp` or `BlueprintHelperCallFunctionResolverUtils.cpp`, ensure selected success requires:

```text
SelectedSpawner != null
SelectedStableId is not empty
CandidateActions contains at least one UE spawner/action evidence record
Supplemental scan evidence is marked supplemental
```

Do not remove supplemental scan if it is still needed for owner/function metadata enrichment; remove only success-producing behavior without spawner evidence.

- [ ] **Step 3: Run call resolver tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.CallFunction;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite80_P3_CallResolver_001'
```

Expected: call resolver group passes.

## Task 2: Add PhysicalDoor Call Correctness Cases

- [ ] **Step 1: Add resolver tests for door calls**

Add tests covering these callable families:

```text
DoorMesh.SetSimulatePhysics(false)
DoorMesh.SetSimulatePhysics(true)
DoorMesh.AddImpulse(...)
Hinge/DoorMesh.SetRelativeRotation(...)
GetRelativeRotation or equivalent angle readback call
```

Each test must assert:

```text
Status == Resolved
SelectedSpawner != null
SelectedStableId contains function/action identity
CandidateActions.Num() > 0
Diagnostic does not contain legacy fallback success
```

- [ ] **Step 2: Add ambiguity test**

Add one ambiguous call request with a short query such as `Set` and insufficient owner/type context.

Expected:

```text
Status != Resolved
Reason == candidate_threshold_exceeded or ambiguous_candidates
Top candidates are present
```

## Task 3: Harden Field / Property Evidence

- [ ] **Step 1: Add field evidence tests**

In `BlueprintHelperFieldVariableActionClusterTests.cpp`, add tests for:

```text
get DoorOpenAngle
set bIsClosed
get_property DoorMesh.RelativeRotation
set_property DoorMesh.SimulatePhysics
```

Each positive test must provide owner and field/property evidence through the projected request/context model, not only `Semantic.Query`.

- [ ] **Step 2: Add missing evidence tests**

Add negative tests:

```text
get_property without owner -> missing_required_evidence
set_property without property_path -> missing_required_evidence
get with only broad query and no projected field evidence -> missing_required_evidence or ambiguous_candidates
```

- [ ] **Step 3: Implement resolver constraints**

In `BlueprintHelperFieldVariableActionResolver.cpp`:

```text
Do not resolve get_property or set_property from Semantic.Query alone.
Require owner evidence for property access.
Require property_path or field metadata for property access.
Allow field get/set query fallback only when projected context proves a unique field owner and field name.
```

## Task 4: Update Capability Scenario Metrics

- [ ] **Step 1: Update PhysicalDoor GraphWrite metrics**

In `BlueprintHelperGraphWrite80PercentCapabilityTests.cpp`, add a P3 diagnostic case for PhysicalDoor call/field coverage:

```text
CaseName = PhysicalDoor_InteractableOnly
Phase = GraphWrite
Capability = Function / Field call correctness
Expected call samples = SetSimulatePhysics, SetRelativeRotation, AddImpulse, get/set state variables
```

Mark it correct only if resolver tests pass and no silent wrong graph is found.

- [ ] **Step 2: Update TimedAccessGate metrics**

Add a P3 diagnostic case:

```text
CaseName = TimedAccessGate_StateMachine
Phase = GraphWrite
Capability = Function / Field / property resolver correctness
```

Mark partial failures with `ambiguous_candidates`, `missing_required_evidence`, or `spawn_or_link_failure` precisely.

## Task 5: Verification

- [ ] **Step 1: Run Function / Field automation**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.CallFunction;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite80_P3_Function_001'
```

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.FieldVariable;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite80_P3_Field_001'
```

Expected: both targeted groups pass.

- [ ] **Step 2: Run capability metrics automation**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.Capability80.P2;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite80_P3_Capability_001'
```

Expected: metrics tests pass and record P3 cases.

- [ ] **Step 3: Build plugin**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat' BuildPlugin -Plugin='D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\BlueprintHelper.uplugin' -Package='D:\UEProjects\Template\Saved\BlueprintHelperBuildTest_GraphWrite80_P3' -TargetPlatforms=Win64
```

Expected: `BUILD SUCCESSFUL`.

## P3 Exit Criteria

- [ ] Function success cannot be selected from supplemental scan alone.
- [ ] Field/property missing evidence produces precise diagnostics.
- [ ] PhysicalDoor and TimedAccessGate call/field samples are represented in capability metrics.
- [ ] Function / Field automation passes.
- [ ] UE 5.6 BuildPlugin passes.

