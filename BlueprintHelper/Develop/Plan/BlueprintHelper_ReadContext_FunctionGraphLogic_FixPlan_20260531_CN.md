# ReadContext FunctionGraph Logic Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix `blueprinthelper_read_context` function-target logic reads so `target_type=function` returns the target function body's real logic without using `target_type=graph` as a semantic workaround.

**Architecture:** Keep `function` as a first-class ReadContext target. Preserve the exporter rule that raw graph export hides `UK2Node_FunctionEntry` / `UK2Node_FunctionResult` from importable `nodes[]`, and repair the read formatter contract by synthesizing function-entry metadata only inside the non-importable logic read payload when a function graph matches the requested function but has no exported entry node. The fix stays in the reusable LogicGroupBuilder boundary and does not add caller-side special cases.

**Tech Stack:** UE 5.6, BlueprintHelper C++, LogicJson/LogicMd/LogicFlow read pipeline, AgentFace task-core, Unreal Automation Tests, PowerShell.

---

## Execution Result

- Status: implemented.
- RED evidence: `Automation RunTests BlueprintHelper.ObjectFirst.Logic.FunctionTargetUsesExportedFunctionGraphWithoutEntry` failed before production fix with `Payload.Graph=""`, no entry, and `Nodes=0`; report path `D:\UEProjects\Template\Saved\Automation\ReadContext_FunctionGraph_RED_20260531_002\index.json`.
- Build evidence: `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload` succeeded, including the final stricter formatter assertions.
- Boundary RED evidence: after adding `__function_entry__ -> body -> __function_result__` links to the exported-shape fixture, `Automation RunTests BlueprintHelper.ObjectFirst.Logic.FunctionTargetUsesExportedFunctionGraphWithoutEntry` failed because only the body node was returned; report path `D:\UEProjects\Template\Saved\Automation\ReadContext_FunctionGraph_Boundary_RED_20260531_001\index.json`.
- Boundary GREEN evidence: `Automation RunTests BlueprintHelper.ObjectFirst.Logic.FunctionTargetUsesExportedFunctionGraphWithoutEntry` passed, 1 succeeded / 0 failed; report path `D:\UEProjects\Template\Saved\Automation\ReadContext_FunctionGraph_Boundary_GREEN_20260531_002\index.json`.
- Formatter GREEN evidence: `Automation RunTests BlueprintHelper.Read.LogicSnapshotFormatter` passed, 2 succeeded / 0 failed; latest report path `D:\UEProjects\Template\Saved\Automation\ReadContext_FunctionGraph_Formatter_GREEN_20260531_002\index.json`.
- LogicFlow GREEN evidence: `npm run build` succeeded, targeted import `node -e "await import('./build/tool-surface/bridge/read-context/read-context-handler.test.js')"` passed 5 tests / 0 failed, and `npm run test:node` passed 282 tests / 0 failed in `AgentFaceService/task-core`.
- Focused GREEN evidence: `Automation RunTests BlueprintHelper.ObjectFirst.Logic.FunctionTarget` passed, 3 succeeded / 0 failed; report path `D:\UEProjects\Template\Saved\Automation\ReadContext_FunctionGraph_FunctionTarget_GREEN_20260531_002\index.json`.
- Regression evidence: `Automation RunTests BlueprintHelper.ObjectFirst.Logic` passed, 13 succeeded / 0 failed; latest report path `D:\UEProjects\Template\Saved\Automation\ReadContext_FunctionGraph_ObjectFirstLogic_GREEN_20260531_003\index.json`.
- Implementation note: `target_type=function` remains the only semantic function read path. The fix synthesizes function entry/result boundary metadata only inside non-importable LogicJson/LogicMd payloads when the requested function name matches the exported function graph and no `FunctionEntry` node exists in `nodes[]`; graph-level links can now bind `__function_entry__` and `__function_result__` endpoints.
- Review cleanup note: the old explicit `K2Node_FunctionEntry` fixture no longer carries synthetic boundary links; those links stay isolated to the exported-shape fixture that matches real FunctionGraph output.

---

## Files

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/BlueprintHelperObjectFirstLogicTests.cpp`
  - Add a red test for the real exporter shape: function graph body nodes exist, but `K2Node_FunctionEntry` is absent from `nodes[]`.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Read/BlueprintHelperLogicReadSnapshotFormatterTests.cpp`
  - Add formatter coverage for the generated `logic_json` boundary node refs/kinds/links and the `logic_md` boundary execution text.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicGroupBuilder.cpp`
  - Add a focused function-target fallback inside `BuildTargetEntry` when `Scope == TargetFunction`, graph name equals target function name, and no exported entry node exists.
  - The fallback must set `Payload.Entry` as a synthetic function entry and return the graph's body nodes.
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_ReadContext_FunctionGraphLogic_Audit_20260531_CN.md`
  - Replace the old workaround-oriented recommendation with the implemented semantic fix and verification evidence.
- Modify: `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-handler.test.ts`
  - Add `logic_flow` coverage for function-target LogicJson with synthetic `__function_entry__` and `__function_result__` boundaries, both at payload conversion and `read_context` handler level.

## Non-goals

- Do not route normal agents to `target_type=graph` for function reads.
- Do not change AgentFace schema or payload mapping for this bug.
- Do not make raw graph export importable by adding real `FunctionEntry` / `FunctionResult` nodes back into `nodes[]`.
- Do not execute `git add`, `git commit`, or `git push`.

---

### Task 1: Add the failing function-target test

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/BlueprintHelperObjectFirstLogicTests.cpp`

- [ ] **Step 1: Add a raw JSON fixture matching the real exporter shape**

Add this helper next to `MakeFunctionInFunctionGraphRawJsonObject()`:

```cpp
static TSharedPtr<FJsonObject> MakeExportedFunctionGraphWithoutEntryRawJsonObject()
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("version"), TEXT("2.2"));
    Root->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.JsonToBlueprint"));

    TArray<TSharedPtr<FJsonValue>> EventGraphNodes;
    EventGraphNodes.Add(MakeShared<FJsonValueObject>(
        MakeLogicTestEventNode(TEXT("event_graph_entry"), TEXT("K2Node_CustomEvent"), TEXT("EventGraphDoor"), TEXT("EventGraphDoor"))));

    TArray<TSharedPtr<FJsonValue>> FunctionNodes;
    TSharedRef<FJsonObject> SetRelativeRotationNode = MakeShared<FJsonObject>();
    SetRelativeRotationNode->SetStringField(TEXT("id"), TEXT("set_relative_rotation"));
    SetRelativeRotationNode->SetStringField(TEXT("type"), TEXT("K2Node_CallFunction"));
    SetRelativeRotationNode->SetStringField(TEXT("name"), TEXT("SetRelativeRotation"));
    SetRelativeRotationNode->SetStringField(TEXT("function_name"), TEXT("SetRelativeRotation"));
    FunctionNodes.Add(MakeShared<FJsonValueObject>(SetRelativeRotationNode));

    TArray<TSharedPtr<FJsonValue>> Graphs;
    Graphs.Add(MakeShared<FJsonValueObject>(MakeLogicTestGraph(TEXT("EventGraph"), EventGraphNodes)));
    Graphs.Add(MakeShared<FJsonValueObject>(MakeLogicTestGraph(TEXT("AddMazeRelativeRotation"), FunctionNodes)));
    Root->SetArrayField(TEXT("graphs"), Graphs);

    return Root;
}
```

- [ ] **Step 2: Add the red automation test**

Add this test after `FObjectFirstLogic_FunctionTargetUsesFunctionGraph`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FObjectFirstLogic_FunctionTargetUsesExportedFunctionGraphWithoutEntry,
    "BlueprintHelper.ObjectFirst.Logic.FunctionTargetUsesExportedFunctionGraphWithoutEntry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstLogic_FunctionTargetUsesExportedFunctionGraphWithoutEntry::RunTest(const FString& Parameters)
{
    FBlueprintHelperLogicGroupBuilder Builder;
    const FBlueprintHelperLogicJsonPayload Payload = Builder.BuildTargetEntry(
        FBlueprintHelperObjectFirstLogicTestsLocalUtils::MakeExportedFunctionGraphWithoutEntryRawJsonObject(),
        TEXT("/Game/Gameplay/Maze/BP_Maze"),
        TEXT(""),
        TEXT("AddMazeRelativeRotation"),
        EBlueprintHelperLogicScope::TargetFunction);

    TestEqual(TEXT("function target resolves exported function graph"), Payload.Graph, FString(TEXT("AddMazeRelativeRotation")));
    TestEqual(TEXT("function target records function name from target"), Payload.Function, FString(TEXT("AddMazeRelativeRotation")));
    TestTrue(TEXT("function target synthesizes entry metadata"), Payload.Entry.IsSet());
    if (Payload.Entry.IsSet())
    {
        TestEqual(TEXT("synthetic function entry uses target name"), Payload.Entry->Name, FString(TEXT("AddMazeRelativeRotation")));
        TestEqual(TEXT("synthetic function entry kind is function"), Payload.Entry->Kind, EBlueprintHelperLogicNodeKind::FunctionEntry);
        TestEqual(TEXT("synthetic function entry ref is stable"), Payload.Entry->NodeRef, FString(TEXT("__function_entry__")));
    }
    TestEqual(TEXT("function target returns exported body nodes"), Payload.Nodes.Num(), 1);
    if (Payload.Nodes.Num() == 1)
    {
        TestEqual(TEXT("function target includes function body node"), Payload.Nodes[0].Name, FString(TEXT("SetRelativeRotation")));
        TestEqual(TEXT("function target body node keeps raw node ref"), Payload.Nodes[0].NodeRef, FString(TEXT("nodes[0]")));
    }

    return true;
}
```

- [ ] **Step 3: Run the red test**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.ObjectFirst.Logic.FunctionTargetUsesExportedFunctionGraphWithoutEntry;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\ReadContext_FunctionGraph_RED_20260531_001'
```

Expected: FAIL before implementation because `Payload.Entry` is not set and `Payload.Nodes.Num()` is `0`.

---

### Task 2: Implement function-entry metadata synthesis in LogicGroupBuilder

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicGroupBuilder.cpp`

- [ ] **Step 1: Add focused local helpers**

Add these helpers inside `FBlueprintHelperLogicGroupBuilderLocalUtils`, near the existing path/ref helpers:

```cpp
static const TCHAR* SyntheticFunctionEntryNodeRef()
{
    return TEXT("__function_entry__");
}

static bool DoesGraphNameMatchTargetFunction(
    const FString& EffectiveGraphName,
    const FString& TargetName)
{
    return !EffectiveGraphName.IsEmpty()
        && !TargetName.IsEmpty()
        && EffectiveGraphName.Equals(TargetName, ESearchCase::IgnoreCase);
}

static FBlueprintHelperLogicEntry MakeSyntheticFunctionEntry(
    const FString& EffectiveGraphName,
    const FString& TargetName)
{
    FBlueprintHelperLogicEntry Entry;
    Entry.Kind = EBlueprintHelperLogicNodeKind::FunctionEntry;
    Entry.Name = TargetName;
    Entry.NodeRef = SyntheticFunctionEntryNodeRef();
    Entry.NodePath = MakeGraphNodePath(EffectiveGraphName, Entry.NodeRef);
    return Entry;
}
```

- [ ] **Step 2: Update `BuildTargetEntry` graph matching**

Change the `TryBuildFromGraph` lambda captures from:

```cpp
auto TryBuildFromGraph = [this, &Payload, &MatchesScope, &MatchesTargetName](
    const TSharedPtr<FJsonObject>& GraphObj,
    const FString& EffectiveGraphName) -> bool
```

to:

```cpp
auto TryBuildFromGraph = [this, &Payload, &MatchesScope, &MatchesTargetName, Scope, &TargetName](
    const TSharedPtr<FJsonObject>& GraphObj,
    const FString& EffectiveGraphName) -> bool
```

- [ ] **Step 3: Synthesize entry metadata only for matching exported function graphs**

Replace the current `EntryIndex == INDEX_NONE` early return:

```cpp
if (EntryIndex == INDEX_NONE)
{
    return false;
}
```

with:

```cpp
const bool bCanSynthesizeFunctionEntry =
    Scope == EBlueprintHelperLogicScope::TargetFunction
    && FBlueprintHelperLogicGroupBuilderLocalUtils::DoesGraphNameMatchTargetFunction(EffectiveGraphName, TargetName)
    && NodesArray->Num() > 0;

if (EntryIndex == INDEX_NONE && !bCanSynthesizeFunctionEntry)
{
    return false;
}
```

Then replace the entry reset block:

```cpp
Payload.Graph = EffectiveGraphName;
Payload.Entry.Reset();
Payload.Nodes.Reset();
Payload.Groups.Reset();
```

with:

```cpp
Payload.Graph = EffectiveGraphName;
Payload.Entry.Reset();
Payload.Nodes.Reset();
Payload.Groups.Reset();

if (EntryIndex == INDEX_NONE && bCanSynthesizeFunctionEntry)
{
    Payload.Entry = FBlueprintHelperLogicGroupBuilderLocalUtils::MakeSyntheticFunctionEntry(
        EffectiveGraphName,
        TargetName);
}
```

Keep the existing `i == EntryIndex` branch so raw JSON fixtures that explicitly contain `K2Node_FunctionEntry` keep their previous behavior.

- [ ] **Step 4: Run the focused green test**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.ObjectFirst.Logic.FunctionTargetUsesExportedFunctionGraphWithoutEntry;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\ReadContext_FunctionGraph_GREEN_20260531_001'
```

Expected: PASS, 1 succeeded, 0 failed.

---

### Task 3: Run regression around existing logic target behavior

**Files:**
- No production file changes.

- [ ] **Step 1: Run existing object-first logic suite**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.ObjectFirst.Logic;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\ReadContext_FunctionGraph_ObjectFirstLogic_20260531_001'
```

Expected: PASS for the suite. Existing function-entry fixture test must still pass.

- [ ] **Step 2: Run plugin build if automation cannot compile changed tests**

Run only if the automation command fails before test execution due to build/module load problems:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat' BuildPlugin -Plugin='D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\BlueprintHelper.uplugin' -Package='D:\UEProjects\Template\Saved\BlueprintHelperBuildTest_ReadContextFunctionGraph_20260531_001' -TargetPlatforms=Win64 -StrictIncludes
```

Expected: `Result: Succeeded`.

---

### Task 4: Update audit document and final checks

**Files:**
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_ReadContext_FunctionGraphLogic_Audit_20260531_CN.md`

- [ ] **Step 1: Update the audit conclusion**

Change the conclusion so it says the runtime fix keeps `target_type=function` as the main path and no longer recommends `target_type=graph` as the agent-facing workaround after this implementation.

Use this wording:

```markdown
本次修复后，`target_type=function` 仍然是 function body read 的唯一语义入口。UE logic read formatter 会在目标 function graph 与请求的 function name 匹配、但 exporter 没有输出 `FunctionEntry` 节点时，合成只属于 `LogicJson/LogicMd` 非 importable payload 的 function entry metadata，然后返回该 function graph 的 body nodes。普通 Agent 不需要也不应该退回 `target_type=graph`。
```

- [ ] **Step 2: Validate no trailing whitespace**

Run:

```powershell
$paths = @(
  'BlueprintHelper\Source\BlueprintHelper\Private\Tests\BlueprintHelperObjectFirstLogicTests.cpp',
  'BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\Logic\BlueprintHelperLogicGroupBuilder.cpp',
  'BlueprintHelper\Develop\Plan\BlueprintHelper_ReadContext_FunctionGraphLogic_Audit_20260531_CN.md',
  'BlueprintHelper\Develop\Plan\BlueprintHelper_ReadContext_FunctionGraphLogic_FixPlan_20260531_CN.md'
)
$bad = foreach ($path in $paths) { Get-Content -Path $path | Select-String -Pattern '[ \t]+$' | ForEach-Object { "$path:$($_.LineNumber)" } }
if ($bad) { $bad; exit 1 }
```

Expected: no output, exit code `0`.

- [ ] **Step 3: Review changed file set**

Run:

```powershell
git status --short -- BlueprintHelper/Source/BlueprintHelper/Private/Tests/BlueprintHelperObjectFirstLogicTests.cpp BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicGroupBuilder.cpp BlueprintHelper/Develop/Plan/BlueprintHelper_ReadContext_FunctionGraphLogic_Audit_20260531_CN.md BlueprintHelper/Develop/Plan/BlueprintHelper_ReadContext_FunctionGraphLogic_FixPlan_20260531_CN.md
```

Expected: only the four planned paths are listed for this task.

---

## Self-review

- Spec coverage: The plan fixes the root issue in the function-target read path, keeps `target_type=function`, and does not introduce graph-target fallback semantics.
- Placeholder scan: No `TBD`, no generic "add tests", no unspecified file paths.
- Type consistency: The plan uses existing `FBlueprintHelperLogicGroupBuilder`, `FBlueprintHelperLogicJsonPayload`, `FBlueprintHelperLogicEntry`, and `EBlueprintHelperLogicScope::TargetFunction` types.
- Git rule: The plan does not ask workers to stage, commit, or push. Final response should include a suggested commit message and manual commands only.
