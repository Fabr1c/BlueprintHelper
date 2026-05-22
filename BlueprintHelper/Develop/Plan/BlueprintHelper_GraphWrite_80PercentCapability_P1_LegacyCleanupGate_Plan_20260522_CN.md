# GraphWrite 80% Capability P1 Legacy Cleanup Gate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 清理或隔离仍可达 legacy GraphWrite 主链，建立禁止旧 fallback 回流的门禁。

**Architecture:** P1 只处理旧残留边界，不新增 GraphWrite 业务能力。新主链必须保持 `TaskSpec GraphBody -> SemanticIR -> ActionContextPipeline -> ActionResolutionCore -> UE NodeSpawner evidence -> shared adapter -> FragmentDAG -> Composer/Linker -> UE Mutator`；判定为旧路径后默认删除，暂不能删除的路径必须写入 gap 文档。

**Tech Stack:** UE 5.6 C++、GraphWrite SemanticIR、ActionResolutionCore、Automation Tests、PowerShell、RunUAT BuildPlugin。

---

## Files

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteLegacyMainlineContractTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFragmentBuilder.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationCoordinator.cpp`
- Modify or delete: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphJsonParser.h`
- Modify or delete: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphJsonParser.cpp`
- Modify or delete: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLinker.h`
- Modify or delete: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLinker.cpp`
- Modify or delete: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeSpawner.h`
- Modify or delete: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeSpawner.cpp`
- Modify: `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_TestRecord_20260522_CN.md`

## Manual Commit Policy

- P1 执行者不得自动运行 `git add`、`git commit`、`git push`。
- 删除旧文件前必须确认它们不再被 `.Build.cs` 或 include 路径引用。

## Task 1: Expand Forbidden Legacy Contract Tests

- [ ] **Step 1: Add forbidden token groups**

Extend `BlueprintHelperGraphWriteLegacyMainlineContractTests.cpp` with these token groups:

```cpp
static const TCHAR* ForbiddenControlFallbackTokens[] = {
	TEXT("manual_control_context"),
	TEXT("manual_control_semantic"),
	TEXT("RequireDedicatedControlBuilderBoundary")
};

static const TCHAR* ForbiddenParsedNodeMainlineTokens[] = {
	TEXT("const FParsedNode& NodeData"),
	TEXT("FParsedNode NodeData"),
	TEXT("FParsedNode BoundNodeData"),
	TEXT("parsed_node_plan_unsupported")
};

static const TCHAR* ForbiddenWideSurfaceFallbackTokens[] = {
	TEXT("CreateMergeCallFunctionNode"),
	TEXT("call_function.name"),
	TEXT("set_member_variable"),
	TEXT("make_struct"),
	TEXT("compare"),
	TEXT("ref")
};
```

The test must scan only active source files under:

```text
BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite
BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite
```

It must not fail on archived docs under `Develop/v*`.

- [ ] **Step 2: Run the contract test and capture current failures**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.LegacyMainline;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite80_P1_Legacy_Before_001'
```

Expected: failures identify every still-reachable legacy token. If the test passes before cleanup, record the pass path and continue to Task 4.

## Task 2: Remove Manual Control Fallback

- [ ] **Step 1: Delete local manual control context construction**

In `BlueprintHelperControlFragmentBuilder.cpp`, remove branches that synthesize:

```text
manual_control_context:
manual_control_semantic:
RequireDedicatedControlBuilderBoundary
```

Replacement behavior:

```text
ControlFragmentBuilder must request ActionResolutionCore through the existing ActionContext projection. If projection is missing, return a structured failure result with missing_required_evidence instead of constructing local context.
```

- [ ] **Step 2: Verify no manual control fallback token remains**

Run:

```powershell
rg -n "manual_control_context|manual_control_semantic|RequireDedicatedControlBuilderBoundary" BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFragmentBuilder.cpp
```

Expected: exit code `1`, no matches.

## Task 3: Delete or Isolate Parsed-Node Legacy Pipeline

- [ ] **Step 1: Identify active include references**

Run:

```powershell
rg -n "BlueprintGraphJsonParser|BlueprintGraphLinker|BlueprintGraphNodeSpawner|FParsedNode" BlueprintHelper/Source/BlueprintHelper
```

Expected: references are either tests documenting legacy deletion, private inactive files being deleted, or active paths requiring replacement.

- [ ] **Step 2: Delete unused private legacy files**

If a file has no active include/reference, delete it:

```text
BlueprintGraphJsonParser.h/.cpp
BlueprintGraphLinker.h/.cpp
BlueprintGraphNodeSpawner.h/.cpp
```

If a file cannot be deleted because another active source still depends on it, remove its mainline entry point and make the dependency fail fast with:

```text
unsupported_intent: legacy parsed-node pipeline is not part of GraphWrite SemanticIR mainline
```

- [ ] **Step 3: Record any non-deletable path in gap doc**

For each non-deletable path, add a row under the legacy deletion gate:

```markdown
| 旧路径 | 暂不能删除原因 | 为什么不能进入新主链路 | 删除前置条件 |
|---|---|---|---|
| `BlueprintGraphJsonParser.h/.cpp` | `BlueprintGraphMutationPlanBuilder` 仍在迁移期 include parser 类型。 | 保留 parsed-node / legacy fallback 语义。 | 将调用方迁移到 SemanticIR request builder 后删除 parser 文件。 |
```

Use concrete file names and reasons; do not write generic explanations.

## Task 4: Remove Wide-Surface Legacy Fallbacks

- [ ] **Step 1: Search active source**

Run:

```powershell
rg -n "call_function\\.name|set_member_variable|make_struct|CreateMergeCallFunctionNode|fallback|TFieldIterator" BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite
```

Expected: any active match is either an allowed diagnostic string, a supplemental candidate source explicitly marked as not sufficient for success, or a legacy residue to remove.

- [ ] **Step 2: Remove success-producing fallback behavior**

For every active fallback that can produce success without projected context/evidence:

```text
Delete the success branch.
Return missing_required_evidence, candidate_threshold_exceeded, ambiguous_candidates, not_found, or unsupported_intent according to the resolver state.
```

Allowed exception:

```text
TFieldIterator-style scanning may remain only as supplemental candidate discovery when selected success still requires ActionDatabase/ActionFilter or explicit spawner evidence. It must not be the sole success evidence.
```

## Task 5: Verification

- [ ] **Step 1: Run source-level search gate**

Run:

```powershell
rg -n "manual_control_context|manual_control_semantic|RequireDedicatedControlBuilderBoundary|CreateMergeCallFunctionNode|call_function\\.name|set_member_variable|make_struct|const FParsedNode& NodeData|FParsedNode BoundNodeData" BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite
```

Expected: no active mainline matches. Diagnostic-only matches must be documented in the gap file.

- [ ] **Step 2: Run legacy contract automation**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.LegacyMainline;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite80_P1_Legacy_After_001'
```

Expected: exit code `0`, `0 failed`.

- [ ] **Step 3: Run BuildPlugin**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat' BuildPlugin -Plugin='D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\BlueprintHelper.uplugin' -Package='D:\UEProjects\Template\Saved\BlueprintHelperBuildTest_GraphWrite80_P1' -TargetPlatforms=Win64
```

Expected: `BUILD SUCCESSFUL`.

## P1 Exit Criteria

- [ ] Legacy source search gate has no active mainline hits.
- [ ] Non-deletable legacy paths, if any, are recorded in the gap document with concrete reasons.
- [ ] Legacy contract automation passes.
- [ ] UE 5.6 BuildPlugin passes.
- [ ] Test record records P1 verification paths.
