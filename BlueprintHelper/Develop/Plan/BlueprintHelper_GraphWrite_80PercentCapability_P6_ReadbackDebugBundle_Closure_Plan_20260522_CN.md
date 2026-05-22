# GraphWrite 80% Capability P6 Readback DebugBundle Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 P0-P5 的能力结果、readback、DebugBundle、测试记录和 gap 文档闭环，形成可复验的 80% 能力与 80% 正确率判断。

**Architecture:** P6 不允许把“Automation 通过”直接等同于 GraphWrite 正确。每个场景必须有 resolver evidence、spawn/link/default/readback evidence、DebugBundle 和测试记录；silent wrong graph 单独计入最高优先级缺陷。最终结论必须来自测试记录文档，而不是聊天总结。

**Tech Stack:** UE 5.6 C++、GraphWrite pipeline、DebugBundle、Automation Tests、BlueprintHelper CLI preview/execute、Markdown reporting、RunUAT BuildPlugin。

---

## Files

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWrite80PercentCapabilityTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp`
- Modify if needed: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphWriteExecutionStats.cpp`
- Modify if needed: `BlueprintHelper/Source/BlueprintHelper/Public/Shared/Debug/BlueprintHelperDebugTypes.h`
- Modify if needed: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Debug/BlueprintHelperDebugExportPolicyResolver.h`
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_TestRecord_20260522_CN.md`
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`
- Modify: `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`

## Manual Commit Policy

- P6 执行者不得自动提交。
- Final report must include manual commit command suggestions only.

## Task 1: Define Readback Assertions for Three Scenarios

- [ ] **Step 1: Add PhysicalDoor readback checklist**

In `BlueprintHelperGraphWrite80PercentCapabilityTests.cpp`, add assertions for `PhysicalDoor_InteractableOnly`:

```text
Door Blueprint exists after Setup Phase.
Root, Hinge, DoorMesh exist and are addressable.
DoorMesh starts with simulate physics false.
Light push function exists.
Force open function exists.
Both functions call physics/rotation related UE functions through resolved call evidence.
Open target rotation is approximately 177 degrees.
Graph contains closing threshold check.
Graph contains physics disable path after closed threshold.
```

- [ ] **Step 2: Add TimedAccessGate readback checklist**

Assertions:

```text
State variables exist.
Function/field/property calls have resolver evidence.
branch, sequence, select nodes have singleton provider evidence.
Exec/data links connect expected state transitions.
No builder/composer/mutation direct spawn evidence appears.
```

- [ ] **Step 3: Add EventDrivenConfigApplier readback checklist**

Assertions:

```text
Custom event exists and has event spawner evidence.
Struct make/break nodes have struct evidence or precise gap.
Delegate/bind missing evidence reports exact missing fields.
No delegate/bind success without complete component/delegate/signature evidence.
```

## Task 2: Standardize DebugBundle Evidence

- [ ] **Step 1: Ensure capability tests capture evidence**

For every P6 scenario result, capture:

```text
semantic kind
cluster kind
resolver status
selected stable id
selected spawner class
candidate count
missing evidence fields
spawned node class
pin/default/link readback summary
```

- [ ] **Step 2: Add DebugBundle failure classification**

If a scenario fails, DebugBundle must include one of:

```text
setup_failure
missing_required_evidence
candidate_threshold_exceeded
ambiguous_candidates
not_found
unsupported_intent
spawn_or_link_failure
silent_wrong_graph
```

- [ ] **Step 3: Add silent wrong graph detector**

If resolver/spawn reports success but readback proves expected nodes, pins, defaults, or links are missing, set:

```text
ErrorKind = SilentWrongGraph
bGraphWriteCorrect = false
```

## Task 3: Update Tool Result Readback Tests

- [ ] **Step 1: Extend existing readback coverage**

In `BlueprintHelperGraphWriteToolResultBaseTests.cpp`, add or extend tests so readback can locate:

```text
function call nodes by selected stable id or function reference
variable get/set nodes by field evidence
select/control nodes by singleton stable id
custom event nodes by event name
struct make/break nodes by struct type
```

- [ ] **Step 2: Verify no readback-only success**

Readback may confirm success, but it must not create success evidence by itself. Resolver/spawn evidence must exist first.

## Task 4: Calculate Final 80% Metrics

- [ ] **Step 1: Update metrics summary**

In capability metrics, calculate:

```text
GraphWrite correctness = correct GraphWrite scenario checks / executed GraphWrite scenario checks
Call correctness = correct call samples / executed call samples
Capability coverage = passed or precisely diagnosed capability items / planned capability items
Silent wrong graph count = total silent wrong graph failures
```

- [ ] **Step 2: Record final table**

Update `BlueprintHelper_GraphWrite_80PercentCapability_TestRecord_20260522_CN.md`:

```markdown
## Final P6 Summary

| 指标 | 数值 | 结论 |
|---|---:|---|
| Capability coverage | 84.0% | PASS |
| GraphWrite correctness | 82.0% | PASS |
| Call correctness | 90.0% | PASS |
| Silent wrong graph | 0 | PASS |
```

The numbers above are the required concrete format. Replace them with real computed values from Automation output; do not write estimated percentages or blank cells.

## Task 5: Sync Status and Gap Documents

- [ ] **Step 1: Update four-cluster completion table**

Modify `BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`:

```text
Only mark a cluster 完全完成 if all planned semantics in that cluster have tests, readback, DebugBundle evidence, and no open gap.
Otherwise keep 部分完成 and update the remaining gap field.
```

- [ ] **Step 2: Update gap document**

For every remaining blocker:

```text
Record exact file/entry.
Record why it blocks 80% capability or correctness.
Record why it cannot be deleted or completed in P6.
Record next required plan.
```

Remove a gap only when code, tests, readback, DebugBundle, and docs agree it is closed.

## Task 6: Verification

- [ ] **Step 1: Run full capability automation**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.Capability80;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite80_P6_Full_001'
```

Expected: Automation exits `0`. Any failing scenario must be represented in the final test record and gap document.

- [ ] **Step 2: Run GraphWrite regression group**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite80_P6_GraphWrite_Regression_001'
```

Expected: existing GraphWrite tests do not regress.

- [ ] **Step 3: Run BuildPlugin**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat' BuildPlugin -Plugin='D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\BlueprintHelper.uplugin' -Package='D:\UEProjects\Template\Saved\BlueprintHelperBuildTest_GraphWrite80_P6' -TargetPlatforms=Win64
```

Expected: `BUILD SUCCESSFUL`.

## P6 Exit Criteria

- [ ] Test record contains real computed capability/correctness values.
- [ ] GraphWrite correctness is at least 80% or remaining shortfall is recorded as gap.
- [ ] Call correctness is at least 80% or remaining shortfall is recorded as gap.
- [ ] Silent wrong graph count is 0 or explicitly documented as a blocking gap.
- [ ] Four-cluster completion status matches code/test evidence.
- [ ] Gap document has no stale completed gap.
- [ ] UE 5.6 BuildPlugin passes.
