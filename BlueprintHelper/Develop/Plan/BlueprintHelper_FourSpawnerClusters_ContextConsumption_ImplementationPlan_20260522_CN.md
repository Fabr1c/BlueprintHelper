# Four Spawner Clusters Stable ActionContext Consumption Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make FunctionActionCluster, FieldVariableActionCluster, EventDelegateActionCluster, and GenericAssetStructControlActionCluster consume one stable ActionContextPipeline projection instead of rebuilding or guessing context locally.

**Architecture:** `ActionContextPipeline` remains the only context construction path: demand collection, GameThread snapshot, worker-safe inference, bundle projection, then `ActionResolutionCore`. The four clusters consume a read-only projected request/context view and only perform UE ActionDatabase / ActionFilter / NodeSpawner-family candidate resolution. `execute` must not offer or perform an implicit re-preview path; missing or stale context must return explicit diagnostics.

**Tech Stack:** Unreal Engine 5.6 C++, BlueprintHelper GraphWrite, ActionResolutionCore, ActionContextPipeline DTOs, UE ActionDatabase / BlueprintActionFilter / UBlueprintNodeSpawner, BlueprintHelper Automation Tests, BlueprintHelper CLI preview/execute.

---

## Non-negotiable rules

1. Do not add legacy fallback, old AgentFace compatibility, old node handler routes, or parsed-node mutation fallback.
2. Do not provide an option named or behaving as "allow execute to re-preview". The path is unique: preview resolves, execute validates and consumes available context/evidence, stale context returns a diagnostic.
3. Do not let clusters construct `ContextDemand`, `Snapshot`, `Inference`, `Bundle`, or `ActionContextScope` internally.
4. Do not let clusters read settings directly for action-resolution defaults. Settings are consumed before dispatch through `ActionResolutionCore` / settings resolver.
5. Do not cache spawned nodes, `UObject*`, `UEdGraph*`, `UEdGraphPin*`, or `UBlueprintNodeSpawner*` across phases.
6. UE object reads remain inside the GameThread snapshot/context boundary. Worker-safe inference only consumes DTO data.
7. The result of this plan is stable context consumption. Preview-to-execute evidence reuse is prepared by exposing stable statement/context identity, but the full evidence cache implementation is a separate follow-up.

---

## File structure

### Create

- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h`
  - Read-only, request-scoped context facade consumed by all four clusters.
  - Owns no `UObject*`; references the projected request and exposes validation helpers.

- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.cpp`
  - Implements completeness checks, stable identity helpers, and diagnostic helpers.

- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFourClusterContextConsumptionTests.cpp`
  - Source-contract and behavior-contract tests for the four-cluster context consumption boundary.

### Modify

- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.cpp`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionActionCluster.h`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionActionCluster.cpp`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionCluster.h`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionCluster.cpp`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.h`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionCluster.h`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionCluster.cpp`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.cpp`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOperatorActionResolver.cpp`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.cpp`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_FourSpawnerClusters_ContextConsumption_ImplementationPlan_20260522_CN.md`

---

## Task 1: Add cluster context consumption contract tests

**Files:**
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFourClusterContextConsumptionTests.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`

- [x] **Step 1: Create source-contract tests**

Create tests that scan the four cluster/resolver files and fail when they include or instantiate ActionContext construction classes. The forbidden tokens are:

```text
BlueprintHelperActionContextDemandCollector
BlueprintHelperActionContextSnapshotBuilder
BlueprintHelperActionContextInferenceService
BlueprintHelperActionContextBundleProjector
BlueprintHelperActionContextBuildService
FBlueprintHelperActionContextScope::Build
BuildSync(
BuildAsyncFromSnapshot(
```

Test target files:

```text
Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionActionCluster.cpp
Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionCluster.cpp
Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp
Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionCluster.cpp
Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.cpp
Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOperatorActionResolver.cpp
Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.cpp
```

- [x] **Step 2: Add positive contract checks**

The same test file must assert each cluster `.cpp` contains:

```text
FBlueprintHelperActionClusterContextView
```

This makes it impossible for a future cluster to bypass the context-view boundary without breaking tests.

- [x] **Step 3: Add no execute re-preview source contract**

Scan these files:

```text
Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.cpp
Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.cpp
Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp
```

Forbidden tokens:

```text
allow_execute_repreview
execute_repreview
repreview_on_execute
RunPreviewFromExecute
PreviewFallback
```

- [x] **Step 4: Extend existing contract tests**

In `BlueprintHelperActionResolutionContractTests.cpp`, extend source checks so active code cannot return:

```text
UnsupportedClusterMigration
migration_pending
unsupported_cluster_migration
legacy_fallback
```

Allowed exception: tests may mention those strings only as forbidden-token assertions.

---

## Task 2: Introduce the read-only cluster context view

**Files:**
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`

- [x] **Step 1: Add the class**

Implement `FBlueprintHelperActionClusterContextView` as a small read-only facade over `FBlueprintHelperActionResolutionRequest`.

Required public API:

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperActionClusterContextView
{
public:
	explicit FBlueprintHelperActionClusterContextView(const FBlueprintHelperActionResolutionRequest& InRequest);

	const FBlueprintHelperActionResolutionRequest& GetRequest() const;
	EBlueprintHelperSpawnerClusterKind GetClusterKind() const;
	const FBlueprintHelperActionSemanticConstraints& GetSemantic() const;

	const FString& GetStatementId() const;
	const FString& GetProjectedContextHash() const;
	const FString& GetSemanticConstraintsHash() const;

	bool HasGraphContext() const;
	bool HasSemanticKind() const;
	bool HasStableIdentity() const;
	bool IsCompleteForCluster(EBlueprintHelperSpawnerClusterKind ExpectedCluster, FString& OutCode, FString& OutMessage) const;
};
```

- [x] **Step 2: Add missing request identity fields only if absent**

`FBlueprintHelperActionResolutionRequest` must expose:

```cpp
FString StatementId;
FString ProjectedContextHash;
FString SemanticConstraintsHash;
```

Do not duplicate fields if equivalent fields already exist; map the view to the existing field names instead.

- [x] **Step 3: Enforce completeness semantics**

`IsCompleteForCluster` must return false for:

```text
invalid_cluster_dispatch
action_context_graph_missing
semantic_kind_missing
action_context_identity_missing
```

It must not repair or infer missing values.

---

## Task 3: Route ActionResolutionCore dispatch through the context view

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.cpp`
- Modify all four cluster headers/cpp files.

- [x] **Step 1: Update cluster signatures**

Each cluster must use this shape:

```cpp
static FBlueprintHelperActionResolutionResult Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context);
```

- [x] **Step 2: Validate context before cluster dispatch**

`ActionResolutionCore` constructs one `FBlueprintHelperActionClusterContextView` from the already-projected request and validates the expected cluster before calling that cluster.

If validation fails, return the existing equivalent of:

```text
NeedsMoreSemanticContext(code, message)
```

If the cluster kind is unknown, return:

```text
InvalidRequest("unknown_spawner_cluster", "Unknown spawner cluster kind.")
```

- [x] **Step 3: Keep settings consumption in core**

Default `MaxCandidates`, search policy, and ambiguity policy remain resolved before dispatch. Clusters must not call settings resolvers directly.

---

## Task 4: Populate projected identity from BundleProjector

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`

- [x] **Step 1: Ensure projected context carries stable identity**

Projection must provide:

```text
StatementId
SemanticConstraintsHash
ProjectedContextHash
```

Hash inputs must be stable strings such as asset path, graph name, semantic kind/query/target/operator, expected type, argument types, target object type, and settings revision. Do not hash pointer addresses.

- [x] **Step 2: Add projection assertions**

Add tests asserting projected action requests for at least one function/action statement and one field-variable statement have non-empty identity:

```cpp
TestFalse(TEXT("Projected request has StatementId"), ProjectedRequest.StatementId.IsEmpty());
TestFalse(TEXT("Projected request has SemanticConstraintsHash"), ProjectedRequest.SemanticConstraintsHash.IsEmpty());
TestFalse(TEXT("Projected request has ProjectedContextHash"), ProjectedRequest.ProjectedContextHash.IsEmpty());
```

---

## Task 5: Migrate FunctionActionCluster

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionActionCluster.h`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionActionCluster.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOperatorActionResolver.cpp`

- [ ] **Step 1: Consume only projected context**

Function cluster reads:

```text
Semantic.Kind
Semantic.Query
Semantic.Operator
Target object type
Argument pin types
Expected return type
Graph context from projected request
```

- [ ] **Step 2: Map semantics**

```text
call -> Function/action query plus argument and target constraints
op -> Operator token plus operand pin types and expected return
convert_function -> source type and target type constraints
schedule_function -> timer/schedule semantic constraints plus callable metadata
latent_or_async_function -> latent/async metadata plus world context hints
```

- [ ] **Step 3: Normalize missing context**

Missing required operator/call context returns:

```text
NeedsMoreSemanticContext: operator_context_missing
NeedsMoreSemanticContext: function_context_missing
```

No global `FindFunctionByName` fallback is allowed.

---

## Task 6: Migrate FieldVariableActionCluster

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionCluster.h`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionCluster.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.cpp`

- [ ] **Step 1: Consume only projected context**

Field cluster reads:

```text
Semantic.Kind
Semantic.Query
Semantic.Target
Semantic.PropertyPath
Projected field symbols
Target object type
Expected pin type
Read/write intent
Graph context from projected request
```

- [ ] **Step 2: Remove local guessing**

Remove active code that scans Blueprint variables/components/local parameters independently of SnapshotBuilder output, guesses target object locally, or falls back from property path to arbitrary function call.

- [ ] **Step 3: Normalize missing context**

Return:

```text
field_context_missing
field_target_missing
field_write_type_missing
field_action_unresolvable
```

---

## Task 7: Migrate EventDelegateActionCluster

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.h`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp`

- [ ] **Step 1: Consume only projected context**

Event cluster reads:

```text
Semantic.Kind
Semantic.Query
Binding object path
Selected object path
Component name/type
Delegate signature hints
Graph context
Candidate metadata from UE ActionDatabase
```

- [ ] **Step 2: Use UE NodeSpawner families**

Allowed spawner families:

```text
UBlueprintEventNodeSpawner
UBlueprintBoundEventNodeSpawner
UAnimNotifyEventNodeSpawner
UBlueprintDelegateNodeSpawner
UBlueprintBoundNodeSpawner
```

- [ ] **Step 3: Normalize missing context**

Return:

```text
event_context_missing
binding_object_missing
delegate_signature_missing
event_action_unresolvable
```

---

## Task 8: Migrate GenericAssetStructControlActionCluster

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionCluster.h`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionCluster.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.cpp`

- [ ] **Step 1: Consume only projected context**

Generic cluster reads:

```text
Semantic.Kind
Semantic.Query
TypeName
Expected return type
Argument pin types
Struct metadata hints
Enum metadata hints
Control-flow legality hints
Graph context
```

- [ ] **Step 2: Preserve generic native make/break**

The native `HasNativeMake` / `HasNativeBreak` support remains generic. It consumes projected type/struct metadata and must not introduce Vector-only hardcoding.

- [ ] **Step 3: Separate single-node and multi-node semantics**

Single-node cases use UE NodeSpawner evidence where expressible. Multi-node DAG cases may use dedicated FragmentBuilder only when UE ActionDatabase cannot represent the semantic operation, and they must still consume the same projected context.

---

## Task 9: Remove active local rebuild and migration paths

**Files:**
- Modify all ActionResolution cluster/resolver files listed above.
- Modify `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`

- [ ] **Step 1: Remove active forbidden paths**

Within `ActionResolution` production files, active code must not contain:

```text
UnsupportedClusterMigration
migration_pending
unsupported_cluster_migration
legacy_fallback
FindFunctionByName
BuildSync(
BuildAsyncFromSnapshot(
FBlueprintHelperActionContextScope::Build
```

Allowed exceptions:

```text
BlueprintHelperActionContextBuildService.cpp owns BuildSync / BuildAsyncFromSnapshot.
BlueprintHelperActionContextScope.cpp owns scope construction.
Tests may mention forbidden tokens only as forbidden-token assertions.
```

- [ ] **Step 2: Replace removed paths with explicit diagnostics**

Use:

```text
InvalidRequest
NeedsMoreSemanticContext
ActionUnresolvable
AmbiguousCandidates
```

---

## Task 10: Compile and run full graphwrite smoke

**Files:**
- Use existing smoke: `D:/UEProjects/Template/Saved/BlueprintHelper/CodexSmoke/ActionContextPipeline_20260522_025331/full_graph_20260522_025331.json`
- Modify this plan document with real progress.

- [x] **Step 1: Compile**

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected:

```text
Result: Succeeded
```

- [x] **Step 2: Start editor with global MCP**

Use `mcp__blueprint_helper__blueprint_open_editor` with:

```text
project_file = D:\UEProjects\Template\Template.uproject
wait_timeout_ms = 120000
```

Expected:

```text
EDITOR_BRIDGE_AVAILABLE
```

- [x] **Step 3: Run preview**

```powershell
bh.cmd task preview --file 'D:\UEProjects\Template\Saved\BlueprintHelper\CodexSmoke\ActionContextPipeline_20260522_025331\full_graph_20260522_025331.json' --fields status,summary,error_code,message,artifacts.full_result
```

Expected:

```json
{
  "status": "preview_passed",
  "summary": {
    "warnings": 0,
    "errors": 0,
    "modified": false
  }
}
```

- [x] **Step 4: Run execute**

```powershell
bh.cmd task execute --file 'D:\UEProjects\Template\Saved\BlueprintHelper\CodexSmoke\ActionContextPipeline_20260522_025331\full_graph_20260522_025331.json' --fields status,summary,error_code,message,artifacts.full_result
```

Expected:

```json
{
  "status": "executed",
  "summary": {
    "warnings": 0,
    "errors": 0,
    "modified": true
  }
}
```

- [x] **Step 5: Diagnose through full_result if needed**

If preview or execute fails, read `artifacts.full_result` and classify the failure as:

```text
context_missing
context_hash_missing
cluster_context_consumption_gap
action_unresolvable
ambiguous_candidates
ue_compile_error
```

Write the specific gap into this plan before fixing it.

---

## Task 11: Update architecture docs

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_FourSpawnerClusters_ContextConsumption_ImplementationPlan_20260522_CN.md`

- [ ] **Step 1: Add stable context consumption rule**

Add:

```text
Four Spawner-Oriented clusters must consume ActionContext through the projected request/context view emitted by ActionContextPipeline. Clusters may query UE ActionDatabase / ActionFilter / NodeSpawner with the projected graph context, but they must not build demand, snapshot, inference, bundle, scope, settings defaults, or target/type inference locally.
```

- [ ] **Step 2: Add execute path uniqueness rule**

Add:

```text
Execute does not re-preview. When preview-time context or future evidence is stale/unresolvable, execute returns explicit diagnostics and stops. It must not silently rerun preview or choose another action path.
```

- [ ] **Step 3: Record verification evidence**

After successful compile and CLI smoke, append:

```markdown
## Progress Update 2026-05-22

- [x] Four clusters consume `FBlueprintHelperActionClusterContextView`.
- [x] Clusters no longer build ActionContextPipeline locally.
- [x] Full graphwrite preview passed.
- [x] Full graphwrite execute passed.

距离期望差距：
- [x] 无。

阻塞内容：
- 无。
```

If any check fails, do not mark it complete. Write the precise remaining gap.

---

## Progress Update 2026-05-22

- [x] Added `FBlueprintHelperActionClusterContextView` and routed `ActionResolutionCore` dispatch through the projected request/context view.
- [x] Added stable identity fields on `FBlueprintHelperActionResolutionRequest`: `StatementId`, `ProjectedContextHash`, and `SemanticConstraintsHash`.
- [x] Added `ContextEvidence` projection from `ResolvedActionContextBundle` so clusters can consume worker-inferred evidence without rebuilding context locally.
- [x] Updated the four cluster entrypoints to receive `FBlueprintHelperActionClusterContextView`.
- [x] Removed the active `call` stable-id fast path that used direct `FindFunctionByName` and produced function candidates without a valid UE `NodeSpawner`.
- [x] Updated `FieldVariableActionResolver` to consume projected `field_name` evidence instead of scanning `Blueprint->NewVariables` to choose candidates.
- [x] Routed branch/return control boundary checks through the existing `ActionContextScope` instead of constructing an identity-free request in the control builder.
- [x] Compile passed with `Build.bat TemplateEditor Win64 Development`.
- [x] Full graphwrite preview passed for `D:/UEProjects/Template/Saved/BlueprintHelper/CodexSmoke/ActionContextPipeline_20260522_025331/full_graph_20260522_025331.json`.
- [x] Full graphwrite execute passed for the same TaskSpec.

距离期望差距：
- [ ] `EventDelegateActionCluster` still only exposes explicit missing-context / unsupported diagnostics; full UE event/delegate spawner-family consumption is not implemented in this pass.
- [ ] `GenericAssetStructControlActionResolver` still contains direct struct spawner creation for make/break fallback. This is currently a UE-unrepresentable escape hatch, but it still needs a follow-up pass to either replace it with ActionDatabase evidence or document the narrow exception at resolver level.
- [ ] `FunctionResolution` still builds a diagnostic candidate universe beyond ActionDatabase. It no longer resolves candidates without a valid `NodeSpawner`, but the diagnostic-only direct scan should be reviewed in the next cleanup pass.
- [ ] CLI does not expose a standalone `compile_blueprint_asset` top-level command in this session. The full TaskSpec preview/execute passed, but independent blueprint compile result could not be pulled through CLI without using developer-only MCP commands.

阻塞内容：
- Standalone blueprint compile verification is blocked by CLI command exposure, not by the ActionContext changes.

---

## Self-review

Spec coverage:

1. Four clusters stable context consumption is covered by Tasks 2-8.
2. No execute re-preview path is covered by Tasks 1, 3, 9, and 11.
3. No legacy fallback / no migration pending is covered by Tasks 1 and 9.
4. Preview-to-execute evidence cache is intentionally not implemented here; this plan only prepares stable statement/context identity needed by that follow-up.
5. Compile and CLI preview/execute validation are covered by Task 10.

Placeholder scan:

1. This plan contains no placeholder implementation steps.
2. Conditional wording is limited to avoiding duplicate fields if equivalent fields already exist.
3. All failure cases require explicit diagnostics rather than silent fallback.

Type consistency:

1. `FBlueprintHelperActionClusterContextView` is the single new class name used across tasks.
2. `StatementId`, `ProjectedContextHash`, and `SemanticConstraintsHash` are the stable identity field names used consistently.
3. `InvalidRequest` and `NeedsMoreSemanticContext` are the required diagnostic categories for invalid or incomplete context.

---

## Manual commit guidance

Do not run git commands automatically. After implementation and validation, the user should commit only files changed by this plan.

Suggested commit message shape:

```text
新增内容：
1. 引入四簇统一 ActionContext 消费边界

变更需求：
1. 四个 Spawner-Oriented Cluster 改为消费稳定 projected context
2. execute 阶段保持唯一路径，不提供重新 preview 分支
```

---

## Progress Update 2026-05-22 - set_property/op/generic/event boundary pass

- [x] `set_property` graph-body FragmentDAG emission now resolves through `FieldVariableActionCluster` and invokes the selected UE `NodeSpawner` through `FBlueprintHelperActionNodeSpawnerAdapter`.
- [x] `FieldVariableActionCluster` now allows `get_property` / `set_property` to enter the same field-variable resolver path as `get` / `set`; simple property access uses `Target` / `PropertyPath` to shrink the field search scope instead of creating a separate ad-hoc handler.
- [x] `op` now consumes operator intent from `Semantic.Query` first, then projected `ContextEvidence.operator_token/operator/op/op_name`; missing operator context returns explicit `operator_context_missing`.
- [x] `op` remains a UE type-promotion path through `FTypePromotion::GetOperatorSpawner`; it is not represented as a pseudo `call`.
- [x] Generic construct/deconstruct direct MakeStruct/BreakStruct creation is now labeled as a dedicated `GenericAssetStructControl` boundary in candidate evidence and result messages.
- [x] `EventDelegateActionCluster` now resolves custom `event` semantics through `UBlueprintEventNodeSpawner` when an event name is projected.
- [x] `EventDelegateActionCluster` now returns specific diagnostics for missing `component_bound_event` and `bind` projected evidence instead of the previous generic `needs_more_semantic_context`.
- [x] Compile passed with `Build.bat TemplateEditor Win64 Development`.
- [x] Full graphwrite preview passed for `D:/UEProjects/Template/Saved/BlueprintHelper/CodexSmoke/ActionContextPipeline_20260522_025331/full_graph_20260522_025331.json`.
- [x] Full graphwrite execute passed for the same TaskSpec.

距离期望差距：

- [ ] `set_property` is migrated for simple graph-body field/property writes. Complex object-path or struct-member writes still need a composed DAG path once the schema carries enough typed target/member evidence.
- [ ] `EventDelegateActionCluster` still does not fully resolve `component_bound_event` or `bind`; the ActionContextPipeline must first project component name/class, delegate owner/property, and signature evidence.
- [ ] Generic direct MakeStruct/BreakStruct remains a deliberate dedicated boundary because UE ActionDatabase cannot always express the operation through FunctionAction. It is now explicitly marked, but should stay narrow and covered by tests.
- [ ] The full graphwrite smoke covers the current P0-P3 graphwrite path, but not the missing component-bound/bind delegate evidence path.

阻塞内容：

- `component_bound_event` / `bind` completion is blocked on projected delegate/component evidence fields, not on `EventDelegateActionCluster` dispatch itself.