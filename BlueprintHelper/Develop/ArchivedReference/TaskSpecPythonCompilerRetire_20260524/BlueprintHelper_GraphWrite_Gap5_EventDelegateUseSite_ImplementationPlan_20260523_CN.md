# GraphWrite Gap5 EventDelegate Use-Site Implementation Plan

> **Execution note:** The original plan supported subagent execution, but the current implementation was completed single-threaded after the user explicitly requested abandoning subagent development. Steps use checkbox (`- [ ]`) syntax for tracking historical work items; the closure status below is authoritative for this execution.

**Goal:** Close Gap 5 first-stage EventDelegate use-site support for `component_bound_event`, `delegate.bind`, `delegate.assign`, `delegate.unbind`, `delegate.unbind_all`, and `delegate.call` without moving declaration/signature ownership out of Signature.

**Architecture:** `EventDelegateActionCluster` remains the first-level ActionResolution cluster. C++ Action semantic must use only two first-stage meanings for this area: `ComponentBoundEvent` and `Delegate`; `bind / assign / unbind / call / clear` are second-level delegate operations carried by SemanticIR / ActionContext evidence, not separate top-level Action semantic enum values. Signature owns declarations and signatures; GraphWrite/EventDelegate consumes existing projected evidence and writes use-site graph nodes only.

**Tech Stack:** UE 5.6 C++, BlueprintGraph `UBlueprintBoundEventNodeSpawner` / `UBlueprintDelegateNodeSpawner`, manual `UK2Node_AssignDelegate` factory for assign, BlueprintHelper ActionContext pipeline, AgentFace TypeScript task contract, Python TaskSpec compiler, Unreal Automation Tests, PowerShell verification.

---

## Execution Rules

- Do not run `git add`, `git commit`, or `git push`; provide manual commands only after completion.
- Do not assign this whole plan to `codex5.3spark`. Current execution is single-threaded.
- Do not use subagents for the remaining review/docs/verification unless the user explicitly re-enables that workflow.
- Do not close Gap 5 until all six use-site semantics have positive resolver evidence, focused readback coverage, deterministic missing-evidence diagnostics, UE 5.6 compile, AgentFace tests, source contracts, and doc sync.
- Do not preserve legacy parsed-node delegate fallback paths.
- Do not introduce `Assign`, `Unbind`, `DelegateCall`, or `DelegateClear` as top-level `EBlueprintHelperActionSemanticKind` values.

## Execution Update - 2026-05-23

Status: closed for EventDelegate first-stage use-site positive spawner support.

Closed scope:
- `component_bound_event` resolves through `ComponentBoundEvent` and component/delegate projected evidence.
- `delegate.bind`, `delegate.assign`, `delegate.unbind`, `delegate.unbind_all`, and `delegate.call` resolve through first-stage `Delegate` plus second-stage `delegate_operation`.
- `delegate.unbind` and `delegate.unbind_all` remain explicit; `unbind` does not fall back to clear/unbind-all when handler evidence is missing.
- GraphWrite/EventDelegate writes use-site nodes only and does not call or duplicate Signature `ensure_*` behavior.
- Handler declarations/signatures remain Signature-owned; GraphWrite only references existing handler evidence.

Verification evidence:
- `BlueprintHelper.GraphWrite.ActionResolution.EventDelegate`: 10 succeeded, 0 failed, 0 not run.
- `BlueprintHelper.GraphWrite.GraphStatement.EventDelegate`: 6 succeeded, 0 failed, 0 not run.
- `BlueprintHelper.GraphWrite.ActionContext`: 13 succeeded, 0 failed, 0 not run.
- `BlueprintHelper.GraphWrite.ActionResolution.Contract`: 5 succeeded, 0 failed, 0 not run.
- `BlueprintHelper.GraphWrite.LegacyMainline`: 8 succeeded, 0 failed, 0 not run.
- `BlueprintHelper.GraphWrite.Capability80`: 5 succeeded, 0 failed, 0 not run.
- `BlueprintHelper.GraphWrite`: 122 succeeded, 10 succeeded with warnings, 0 failed, 0 not run.
- AgentFace Python tests: 57 OK.
- AgentFace Node tests: 150 pass, 0 fail.
- UE 5.6 compile: `Build.bat TemplateEditor Win64 Development` succeeded.

Assign exception:
- `delegate.assign` is not implemented through a normal `UBlueprintDelegateNodeSpawner` selected-spawner path because UE's AssignDelegate spawner can auto-create a Signature-owned `UK2Node_CustomEvent`.
- The resolver returns `ue_delegate_manual_assign_factory` evidence with no `SelectedSpawner`; the fragment builder manually constructs `UK2Node_AssignDelegate` without `PostPlacedNewNode` and links an existing-handler `Create Event` reference.

## Fixed Boundary

Signature owns declaration and signature mutation:

- `ensure_function`
- `ensure_custom_event`
- `ensure_event_dispatcher`
- `ensure_override_event`
- handler function graph/signature creation
- event dispatcher declaration/signature creation
- mismatch policy, migration, and removal

GraphWrite/EventDelegate owns existing-declaration use-site graph writing:

- component-bound event use-site node placement
- delegate bind/assign/unbind/call/clear use-site node placement
- `Create Event` delegate-reference nodes for bind/assign/unbind when handler evidence exists
- graph links and body content around those use sites

Handler rule:

- If the handler exists, GraphWrite may reference it through projected evidence.
- If the handler does not exist, a prior Signature dependency step must create it before GraphWrite runs.
- GraphWrite must not secretly create, rename, or alter handler declarations/signatures.
- `Bind Event to ...` / `Assign ...` nodes are GraphWrite use-site nodes.
- The function or event selected by `Create Event` remains Signature-owned declaration/signature state.

## Canonical Two-Level Mapping

Agent-facing GraphBody kinds:

| AgentFace statement kind | Internal SemanticIR statement kind | First-stage Action semantic | Second-stage operation | UE node family |
|---|---|---|---|---|
| `component_bound_event` | `component_bound_event` | `ComponentBoundEvent` | empty | `UBlueprintBoundEventNodeSpawner` + `UK2Node_ComponentBoundEvent` |
| `delegate.bind` | `delegate` | `Delegate` | `bind` | `UBlueprintDelegateNodeSpawner` + `UK2Node_AddDelegate` + `UK2Node_CreateDelegate` |
| `delegate.assign` | `delegate` | `Delegate` | `assign` | `ue_delegate_manual_assign_factory` + `UK2Node_AssignDelegate` + `UK2Node_CreateDelegate` |
| `delegate.unbind` | `delegate` | `Delegate` | `unbind` | `UBlueprintDelegateNodeSpawner` + `UK2Node_RemoveDelegate` + `UK2Node_CreateDelegate` |
| `delegate.unbind_all` | `delegate` | `Delegate` | `clear` | `UBlueprintDelegateNodeSpawner` + `UK2Node_ClearDelegate` |
| `delegate.call` | `delegate` | `Delegate` | `call` | `UBlueprintDelegateNodeSpawner` + `UK2Node_CallDelegate` |

The Python compiler may accept dotted AgentFace kinds, but the C++ SemanticIR should consume only:

```text
component_bound_event
delegate
```

with a required `delegate_operation` field for `delegate`.

## General Semantic Taxonomy Rule

This Gap5 plan uses the following broader taxonomy rule, which should also guide later GraphWrite convergence:

```text
First-stage Action semantic:
  Decides evidence shape, resolver family, candidate search strategy, and node-generation paradigm inside the selected SpawnerCluster.

Second-stage operation:
  Describes the concrete action inside one evidence/resolver family.
```

Therefore, a semantic should stay first-stage only when it changes one of these boundaries:

- required projected evidence structure
- owning resolver / spawner family
- ActionDatabase or singleton-evidence strategy
- fragment composition paradigm
- correctness-critical diagnostics and ambiguity policy

If multiple operations share the same evidence family and resolver strategy, they should be represented as one first-stage semantic plus a second-stage `*_operation` field.

Planned taxonomy convergence after Gap5:

| Current kinds | Target first-stage semantic | Second-stage fields | Migration risk |
|---|---|---|---|
| `get`, `set`, `get_property`, `set_property` | `Field` | `field_operation=get/set`, `field_scope=variable/property_path` | Low |
| `construct`, `deconstruct` | `Struct` or `TypeStructure` | `type_operation=construct/deconstruct` | Medium |
| `convert` | `TypeTransform` or `Callable` depending on resolver evidence | `transform_operation=cast/convert/promote` | Medium |
| `call`, `op` | possibly `Callable` | `callable_operation=function/operator` | High because `callfunction` is the wide-surface search core |
| `select` | likely `GenericExpression` or singleton evidence under Generic cluster | `expression_operation=select` | Medium; verify whether UE Select is always canonical singleton |
| `event` | `Event` | `event_operation=custom_event/override/native` | High because Signature ownership boundary is strict |
| `create` | split after evidence audit | `create_operation=spawn_actor/create_widget/construct_object/asset_action` | High because current name is too broad |
| `schedule` | `AsyncFlow` or `Callable` depending on timer/latent evidence | `schedule_operation=timer/latent/async` | High because lifecycle and latent behavior matter |

Gap5 scope is intentionally limited to `EventDelegate -> ComponentBoundEvent / Delegate + delegate_operation`. It must not attempt to migrate Field, Struct, Callable, Event, Create, Select, or Schedule in the same implementation slice.

## Evidence Keys

Common delegate evidence:

```text
delegate_operation
delegate_name
delegate_owner_class_path
delegate_property_name
delegate_property_path
delegate_signature
delegate_signature_function_path
target_graph
```

Additional evidence by use site:

```text
component_bound_event:
  component_path
  component_binding_owner_class_path
  component_binding_field_path
  handler_name
  handler_scope_class_path

delegate bind / assign / unbind:
  binding_object_path
  handler_name
  handler_scope_class_path
  unbind_mode = single only for unbind

delegate clear:
  binding_object_path
  unbind_mode = all
  no handler_name

delegate call:
  binding_object_path
  argument_type_count
  default_value_count when literal args exist
```

Missing required evidence must return deterministic diagnostics. `delegate.unbind` must never downgrade into `delegate.clear` when `handler_name` is missing.

## File Responsibilities

- `AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py`: validate dotted delegate statements, normalize them to `kind=delegate` plus `delegate_operation`, and preserve explicit `unbind` vs `clear`.
- `AgentFaceService/task-core/python/tests/test_graph_write_delegate_statements.py`: Python compiler coverage for the two-level model.
- `AgentFaceService/task-core/src/task/schema/task-contract.ts`: task contract lists agent-facing kinds and documents use-site-only boundary.
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`: top-level semantic enum owns `ComponentBoundEvent` and `Delegate` only for this family.
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`: SemanticIR owns `ComponentBoundEvent` and `Delegate` statement kinds plus `DelegateOperation`.
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/*`: ActionContext projects delegate operation and existing declaration/signature evidence.
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.*`: exact evidence reader for EventDelegate use sites.
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp`: maps `ComponentBoundEvent` and `Delegate + delegate_operation` to UE spawner families.
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h` and `.cpp`: shared spawner adapter accepts UE binding sets.
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.h` and private `.cpp`: builds use-site fragments from resolved spawner evidence.
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuilderRegistry.cpp`: routes only `ComponentBoundEvent` and `Delegate` statements to the EventDelegate fragment builder.
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/*`: source contracts, resolver tests, fragment/readback tests, capability row update.
- `BlueprintHelper/Develop/Gap/*`, `BlueprintHelper/Develop/Plan/*`, `BlueprintHelper/Develop/Design/*`: documentation sync only after verification.

---

## Task 0: Converge Existing Partial Edits To The Two-Level Model

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.cpp`
- Modify: any Gap5 files touched by a prior dispersed-semantic attempt

- [ ] **Step 1: Audit dispersed top-level semantic tokens**

Run:

```powershell
rg -n "Assign|Unbind|DelegateCall|DelegateClear|delegate_call|delegate_clear|delegate_operation" BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite
```

Expected before cleanup may include old partial edits. Expected after cleanup:

```text
No EBlueprintHelperActionSemanticKind::Assign
No EBlueprintHelperActionSemanticKind::Unbind
No EBlueprintHelperActionSemanticKind::DelegateCall
No EBlueprintHelperActionSemanticKind::DelegateClear
delegate_operation appears in SemanticIR/ActionContext/EventDelegate code
```

- [ ] **Step 2: Keep only two EventDelegate first-stage semantic values**

In `BlueprintHelperActionResolutionCore.h`, the EventDelegate part of `EBlueprintHelperActionSemanticKind` must be:

```cpp
	Event,
	ComponentBoundEvent,
	Delegate,
```

Do not add:

```cpp
	Assign,
	Unbind,
	DelegateCall,
	DelegateClear,
```

- [ ] **Step 3: Map semantic strings**

In `BlueprintHelperActionResolutionCore.cpp`, keep:

```cpp
	case EBlueprintHelperActionSemanticKind::ComponentBoundEvent: return TEXT("component_bound_event");
	case EBlueprintHelperActionSemanticKind::Delegate: return TEXT("delegate");
```

Do not add separate string mappings for `assign`, `unbind`, `delegate_call`, or `delegate_clear`.

- [ ] **Step 4: Keep only two node-backed EventDelegate statement kinds**

In `BlueprintHelperGraphSemanticIR.h`, add only:

```cpp
	ComponentBoundEvent,
	Delegate
```

to `EBlueprintHelperGraphStatementKind`.

In `FBlueprintHelperGraphStatementIR`, add:

```cpp
	FString ComponentName;
	FString DelegateName;
	FString DelegateOperation;
	FString HandlerName;
	FString UnbindMode;
```

- [ ] **Step 5: Parse only the canonical internal kinds**

In `BlueprintHelperGraphSemanticIRUtils.cpp`, parse:

```cpp
	if (Kind.Equals(TEXT("component_bound_event"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::ComponentBoundEvent;
	if (Kind.Equals(TEXT("delegate"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::Delegate;
```

Do not parse `bind`, `assign`, `unbind`, `delegate_call`, or `delegate_clear` as statement kinds.

## Task 1: AgentFace Compiler Normalizes Delegate Statements To `delegate + delegate_operation`

**Files:**

- Modify: `AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py`
- Modify: `AgentFaceService/task-core/src/task/schema/task-contract.ts`
- Test: `AgentFaceService/task-core/python/tests/test_graph_write_delegate_statements.py`

- [ ] **Step 1: Add or update compiler tests first**

`test_graph_write_delegate_statements.py` must assert this compiler output shape:

```python
def test_delegate_bind_normalizes_to_delegate_operation(self):
    compiled = compile_body_statement({
        "kind": "delegate.bind",
        "target": "self",
        "delegate": "OnDoorStateChanged",
        "handler": "HandleDoorStateChanged",
    })
    self.assertEqual(compiled["kind"], "delegate")
    self.assertEqual(compiled["delegate_operation"], "bind")
    self.assertEqual(compiled["target"], "self")
    self.assertEqual(compiled["delegate"], "OnDoorStateChanged")
    self.assertEqual(compiled["handler"], "HandleDoorStateChanged")
```

Also cover:

```python
delegate.assign -> kind=delegate, delegate_operation=assign, handler required
delegate.unbind -> kind=delegate, delegate_operation=unbind, handler required, unbind_mode=single
delegate.unbind_all -> kind=delegate, delegate_operation=clear, handler forbidden, unbind_mode=all
delegate.call -> kind=delegate, delegate_operation=call, args compiled
component_bound_event -> kind=component_bound_event, no delegate_operation
```

- [ ] **Step 2: Normalize dotted kinds**

In `graph_write_append.py`, use:

```python
DELEGATE_STATEMENT_KIND_ALIASES = {
    "delegate.bind": "bind",
    "delegate.assign": "assign",
    "delegate.unbind": "unbind",
    "delegate.unbind_all": "clear",
    "delegate.call": "call",
}

SUPPORTED_GRAPH_BODY_STATEMENT_KINDS = {
    "call",
    "set",
    "set_property",
    "let",
    "control",
    "component_bound_event",
    *DELEGATE_STATEMENT_KIND_ALIASES.keys(),
}
```

The normalized output for dotted delegate statements must be:

```python
out["kind"] = "delegate"
out["delegate_operation"] = DELEGATE_STATEMENT_KIND_ALIASES[original_kind]
```

- [ ] **Step 3: Validate shapes**

Validation rules:

```text
component_bound_event requires component, delegate, handler.
delegate.bind requires target, delegate, handler.
delegate.assign requires target, delegate, handler.
delegate.unbind requires target, delegate, handler.
delegate.unbind_all requires target, delegate and forbids handler.
delegate.call requires target, delegate and validates args.
```

Error for `delegate.unbind_all` with handler must use a deterministic code:

```text
delegate_clear_handler_forbidden
```

- [ ] **Step 4: Update contract documentation**

In `task-contract.ts`, keep agent-facing statement kinds:

```ts
'component_bound_event',
'delegate.bind',
'delegate.assign',
'delegate.unbind',
'delegate.unbind_all',
'delegate.call',
```

Add a boundary note:

```ts
event_delegate_use_site_boundary: {
  note: 'GraphWrite delegate statements are use-site only. Handler declarations and signatures must already exist or be emitted by a blueprint_signature dependency before the GraphWrite body step.',
  internal_shape: 'delegate.* statements compile to kind=delegate plus delegate_operation.',
}
```

- [ ] **Step 5: Verify AgentFace**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
python -m unittest discover -s python/tests -t python -p test_graph_write_delegate_statements.py
python -m unittest discover -s python/tests -t python
```

Expected:

```text
OK
```

## Task 2: SemanticIR And ActionContext Carry Delegate Operation Evidence

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextSnapshotBuilder.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`

- [ ] **Step 1: Add focused ActionContext tests**

Add a test that builds a `Delegate` statement:

```cpp
TSharedPtr<FBlueprintHelperGraphStatementIR> Statement = MakeShared<FBlueprintHelperGraphStatementIR>();
Statement->StatementId = TEXT("stmt_delegate_unbind");
Statement->Kind = EBlueprintHelperGraphStatementKind::Delegate;
Statement->Target = TEXT("self");
Statement->DelegateName = TEXT("OnDoorStateChanged");
Statement->DelegateOperation = TEXT("unbind");
Statement->HandlerName = TEXT("HandleDoorStateChanged");
Statement->UnbindMode = TEXT("single");
```

Expected demand:

```cpp
Demands[0].SemanticKind == EBlueprintHelperActionSemanticKind::Delegate
Demands[0].DelegateOperation == TEXT("unbind")
Demands[0].BindingObjectPath == TEXT("self")
Demands[0].HandlerName == TEXT("HandleDoorStateChanged")
Demands[0].UnbindMode == TEXT("single")
```

- [ ] **Step 2: Parse SemanticIR fields**

In `ParseStatement`, read:

```cpp
StatementObject->TryGetStringField(TEXT("component"), Statement->ComponentName);
StatementObject->TryGetStringField(TEXT("delegate"), Statement->DelegateName);
StatementObject->TryGetStringField(TEXT("delegate_operation"), Statement->DelegateOperation);
StatementObject->TryGetStringField(TEXT("handler"), Statement->HandlerName);
StatementObject->TryGetStringField(TEXT("unbind_mode"), Statement->UnbindMode);
```

For `Delegate`, require non-empty `delegate_operation`. Valid operations:

```text
bind
assign
unbind
call
clear
```

- [ ] **Step 3: Map statement kind to top-level action semantic**

In `FBlueprintHelperActionContextDemandCollector::ToActionSemanticKind`:

```cpp
case EBlueprintHelperGraphStatementKind::ComponentBoundEvent:
	return EBlueprintHelperActionSemanticKind::ComponentBoundEvent;
case EBlueprintHelperGraphStatementKind::Delegate:
	return EBlueprintHelperActionSemanticKind::Delegate;
```

- [ ] **Step 4: Project demand fields**

Add fields to `FBlueprintHelperActionContextDemand`:

```cpp
FString DelegateOperation;
FString HandlerName;
FString UnbindMode;
```

`ApplyEventDelegateStatementEvidence` must populate:

```cpp
InOutDemand.DelegateOperation = Statement.DelegateOperation.TrimStartAndEnd();
InOutDemand.DelegateName = FirstNonEmpty(Statement.DelegateName, Statement.Property, Statement.Name);
InOutDemand.BindingObjectPath = Statement.Target;
InOutDemand.ComponentPath = FirstNonEmpty(Statement.ComponentName, ResolveComponentPathFromTarget(Statement.ResolvedTarget));
InOutDemand.HandlerName = Statement.HandlerName.TrimStartAndEnd();
InOutDemand.UnbindMode = Statement.UnbindMode.TrimStartAndEnd();
```

- [ ] **Step 5: Snapshot existing component/delegate evidence**

`FBlueprintHelperActionContextFieldSnapshot` must include:

```cpp
FString FieldPath;
bool bMulticastDelegate = false;
bool bBlueprintAssignable = false;
bool bBlueprintCallable = false;
FString DelegateSignatureFunctionPath;
```

`CaptureFields` must capture:

```text
FObjectProperty component fields from SkeletonGeneratedClass/GeneratedClass.
FMulticastDelegateProperty fields from SkeletonGeneratedClass/GeneratedClass.
Delegate SignatureFunction path.
```

- [ ] **Step 6: Inference projects exact evidence**

`BuildContext` must add:

```cpp
delegate_operation
handler_name
handler_scope_class_path
unbind_mode
delegate_owner_class_path
delegate_property_name
delegate_property_path
delegate_signature_function_path
component_binding_owner_class_path
component_binding_field_path
```

Do not use UObject access in `BlueprintHelperActionContextInferenceService.cpp`; it must remain DTO-only.

- [ ] **Step 7: Verify ActionContext**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionContext.EventDelegate;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_ActionContext_GREEN_001'
```

Expected:

```text
BUILD SUCCESSFUL
failed = 0
notRun = 0
```

## Task 3: EventDelegate Resolver Uses `Delegate + delegate_operation`

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateActionClusterTests.cpp`

- [ ] **Step 1: Add red positive resolver tests**

Each positive test request must use:

```cpp
Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Delegate;
Request.ContextEvidence.Add(TEXT("delegate_operation"), TEXT("bind")); // or assign/unbind/call/clear
```

Expected resolved node class by operation:

```text
bind -> K2Node_AddDelegate
assign -> K2Node_AssignDelegate
unbind -> K2Node_RemoveDelegate
call -> K2Node_CallDelegate
clear -> K2Node_ClearDelegate
```

`component_bound_event` keeps:

```cpp
Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::ComponentBoundEvent;
```

Expected:

```text
component_bound_event -> K2Node_ComponentBoundEvent
```

- [ ] **Step 2: Implement evidence reader**

`BlueprintHelperEventDelegateUseSiteEvidence.*` reads only `Request.ContextEvidence` and `Request.Semantic`.

It must require:

```text
delegate_name
delegate_owner_class_path
delegate_property_name
delegate_property_path
delegate_signature
delegate_operation for Delegate semantic
```

For `Delegate + clear`:

```text
requires unbind_mode=all
forbids handler_name
```

For `Delegate + unbind`:

```text
requires handler_name
requires handler_scope_class_path
requires unbind_mode=single
must not fall back to clear
```

- [ ] **Step 3: Map operation to spawner node class**

In `BlueprintHelperEventDelegateActionCluster.cpp`:

```cpp
static TSubclassOf<UK2Node_BaseMCDelegate> DelegateNodeClassForOperation(const FString& Operation)
{
	if (Operation.Equals(TEXT("bind"), ESearchCase::IgnoreCase)) return UK2Node_AddDelegate::StaticClass();
	if (Operation.Equals(TEXT("assign"), ESearchCase::IgnoreCase)) return UK2Node_AssignDelegate::StaticClass();
	if (Operation.Equals(TEXT("unbind"), ESearchCase::IgnoreCase)) return UK2Node_RemoveDelegate::StaticClass();
	if (Operation.Equals(TEXT("call"), ESearchCase::IgnoreCase)) return UK2Node_CallDelegate::StaticClass();
	if (Operation.Equals(TEXT("clear"), ESearchCase::IgnoreCase)) return UK2Node_ClearDelegate::StaticClass();
	return nullptr;
}
```

Resolver behavior:

```cpp
ComponentBoundEvent -> UBlueprintBoundEventNodeSpawner::Create(UK2Node_ComponentBoundEvent::StaticClass(), Evidence.DelegateProperty)
Delegate -> UBlueprintDelegateNodeSpawner::Create(DelegateNodeClassForOperation(Evidence.DelegateOperation), Evidence.DelegateProperty)
```

- [ ] **Step 4: Stable ID format**

Use stable IDs:

```text
component_bound_event:<delegate_property_path>:<component_binding_field_path>
delegate:<operation>:<delegate_property_path>
delegate:<operation>:<delegate_property_path>:<handler_name> for bind/assign/unbind
```

Candidate `MatchReason`:

```text
ue_bound_event_node_spawner
ue_delegate_node_spawner
```

- [ ] **Step 5: Verify resolver and source boundary**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.EventDelegate;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_EventDelegateResolver_GREEN_001'
rg -n "ensure_function|ensure_custom_event|ensure_event_dispatcher|ensure_override_event|SignatureService|BlueprintSignature" BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.cpp
```

Expected:

```text
failed = 0
notRun = 0
rg returns no matches
```

## Task 4: Fragment Builder Builds Use-Site Nodes Through Shared Adapter

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuilderRegistry.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateActionClusterTests.cpp`

- [ ] **Step 1: Adapter accepts binding sets**

Add to `FBlueprintHelperActionNodeSpawnOptions`:

```cpp
IBlueprintNodeBinder::FBindingSet Bindings;
```

Adapter invocation must use:

```cpp
UEdGraphNode* SpawnedNode = NodeSpawner->Invoke(TargetGraph, Options.Bindings, Location);
```

- [ ] **Step 2: Route only two statement kinds**

`BlueprintHelperGraphFragmentBuilderRegistry.cpp` routes:

```cpp
if (Statement.Kind == EBlueprintHelperGraphStatementKind::ComponentBoundEvent
	|| Statement.Kind == EBlueprintHelperGraphStatementKind::Delegate)
{
	return FBlueprintHelperEventDelegateFragmentBuilder::BuildStatement(...);
}
```

Do not route `Bind`, `Assign`, `Unbind`, `DelegateCall`, or `DelegateClear` statement kinds.

- [ ] **Step 3: Build component-bound event**

For `ComponentBoundEvent`:

```text
Build projected request from ActionContextScope.
Resolve action through FBlueprintGraphWriteFacade::ResolveActionForGraph.
Pass component FObjectProperty as Options.Bindings.
Invoke selected UBlueprintBoundEventNodeSpawner through shared adapter.
Populate primary fragment metadata and pin bindings.
```

- [ ] **Step 4: Build delegate operations**

For `Delegate + bind/assign/unbind`:

```text
Spawn selected delegate node through shared adapter.
Spawn UK2Node_CreateDelegate through shared adapter.
Set CreateDelegate.SelectedFunctionName via SetFunction(handler_name).
Connect CreateDelegate delegate output to the delegate input pin on the primary node.
Do not create or modify handler functions.
```

For `Delegate + call/clear`:

```text
Spawn only the selected delegate node.
Do not spawn CreateDelegate.
```

- [ ] **Step 5: Verify fragment/readback**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.EventDelegate;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_EventDelegateFragment_GREEN_001'
rg -n "NodeSpawner->Invoke\\(|Options\\.Bindings|IBlueprintNodeBinder::FBindingSet" BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.cpp BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.cpp
```

Expected:

```text
failed = 0
notRun = 0
shared adapter owns Invoke
EventDelegate builder supplies bindings through options
```

## Task 5: Physical Door Capability Row And Use-Site Readback

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWrite80PercentCapabilityTests.cpp`
- Add only if needed: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateUseSiteExecutionTests.cpp`

- [x] **Step 1: Keep setup errors separated**

The capability record keeps physical-door setup as a separate row from GraphWrite correctness:

```text
Setup row:
  Create Actor Blueprint and Root/Hinge/DoorMesh
GraphWrite row:
  Physical door internal logic readback
```

Asset/component/signature creation failures are setup errors, not GraphWrite correctness errors.

- [x] **Step 2: Cover all six use-site statements**

This implementation did not add a separate `BlueprintHelper.GraphWrite.EventDelegate.UseSite` automation entry. The six use-site statements are covered by the focused resolver and fragment/readback suites:

```text
component_bound_event -> UK2Node_ComponentBoundEvent
delegate.bind -> UK2Node_AddDelegate + UK2Node_CreateDelegate
delegate.assign -> UK2Node_AssignDelegate + UK2Node_CreateDelegate, no auto-created UK2Node_CustomEvent
delegate.unbind -> UK2Node_RemoveDelegate + UK2Node_CreateDelegate
delegate.unbind_all -> delegate_operation=clear / unbind_mode=all -> UK2Node_ClearDelegate
delegate.call -> UK2Node_CallDelegate
```

Readback must confirm:

```text
UK2Node_ComponentBoundEvent exists for CollisionComponent.OnComponentBeginOverlap.
UK2Node_AddDelegate exists for OnComponentBeginOverlap.
UK2Node_AssignDelegate exists for OnComponentBeginOverlap.
UK2Node_RemoveDelegate exists for OnComponentBeginOverlap.
UK2Node_ClearDelegate exists for OnComponentBeginOverlap.
UK2Node_CallDelegate exists for OnComponentBeginOverlap.
UK2Node_CreateDelegate exists only for bind/assign/unbind and references the existing handler evidence.
```

- [x] **Step 3: Add capability rows**

Capability row must represent the agreed complex scenario:

```text
Door closed/static state has physics simulation disabled.
Door has light push and force open functions.
Door rotates around the hinge to 177 degrees.
Door enables physics after opening and responds to collision.
Door disables physics again when closed.
Delegate use-site validation is limited to door-owned interaction hooks; no player interaction expansion.
```

Update Gap5 row from unsupported to positive:

```text
Complete component/delegate/signature/handler projected evidence resolves and spawns component-bound event plus delegate bind/assign/unbind/call/clear use-site nodes; no Signature ensure_* behavior is called by GraphWrite.
```

- [x] **Step 4: Verify readback and capability**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.EventDelegate;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_EventDelegateResolver_GREEN_002'
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.GraphStatement.EventDelegate;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_EventDelegateFragment_GREEN_002'
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.Capability80;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_Capability80_GREEN_001'
```

Actual:

```text
failed = 0
notRun = 0
GraphWrite correctness does not decrease
Silent wrong graph count remains 0
```

## Task 6: Source Contracts, Regression, Compile, And Docs

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`
- Modify: `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`
- Modify only if wording mismatch exists: `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`

- [x] **Step 1: Add source contract**

Scan:

```text
BlueprintHelperEventDelegateActionCluster.cpp
BlueprintHelperEventDelegateUseSiteEvidence.cpp
BlueprintHelperEventDelegateFragmentBuilder.cpp
```

Forbidden tokens:

```text
ensure_function
ensure_custom_event
ensure_event_dispatcher
ensure_override_event
BlueprintSignatureService
SignatureTaskPlanAdapter
EBlueprintHelperActionSemanticKind::Assign
EBlueprintHelperActionSemanticKind::Unbind
EBlueprintHelperActionSemanticKind::DelegateCall
EBlueprintHelperActionSemanticKind::DelegateClear
EBlueprintHelperGraphStatementKind::Assign
EBlueprintHelperGraphStatementKind::Unbind
EBlueprintHelperGraphStatementKind::DelegateCall
EBlueprintHelperGraphStatementKind::DelegateClear
```

Failure message:

```text
GraphWrite/EventDelegate must not own declaration/signature mutation and must keep delegate suboperations as second-level semantics.
```

- [x] **Step 2: Run full verification**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Contract;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_SourceContract_GREEN_001'
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_Regression_FINAL_001'
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
npm.cmd run build
npm.cmd run test:node
python -m unittest discover -s python/tests -t python
cd D:\UEProjects\Template\Plugins\BlueprintHelper
git diff --check
```

Expected:

```text
source contract failed = 0, notRun = 0
GraphWrite regression failed = 0, notRun = 0
BUILD SUCCESSFUL
AgentFace build/node/python tests pass
git diff --check exit code 0
```

- [x] **Step 3: Sync docs without overclaim**

Gap5 closure wording must say:

```markdown
Status: closed for EventDelegate first-stage use-site positive spawner support.

Closure scope - 2026-05-23:
- `component_bound_event` resolves through `ComponentBoundEvent`.
- `delegate.bind`, `delegate.assign`, `delegate.unbind`, `delegate.unbind_all`, and `delegate.call` resolve through first-stage `Delegate` plus second-stage `delegate_operation`.
- `delegate.unbind` and `delegate.unbind_all` remain explicit and do not silently downgrade.
- GraphWrite/EventDelegate writes use-site nodes only and does not call or duplicate Signature `ensure_*` behavior.
- Handler declarations/signatures remain Signature-owned; GraphWrite only references existing handler evidence.
```

Do not mark unrelated Generic `create` / `convert` / `schedule` work complete.

## Task 7: Single-Thread Review Gate

**Files:** review all modified files from Tasks 0-6.

- [x] **Step 1: Replace independent subagent review with single-thread review**

The user explicitly requested abandoning subagent development. For this execution, the independent review checkpoint is replaced by single-thread source review plus source contracts and full regression. Do not request a subagent review unless the user re-enables it.

Reference review checklist:

```text
Check:
1. EventDelegate first-stage semantics are ComponentBoundEvent and Delegate only.
2. bind/assign/unbind/call/clear are carried as delegate_operation, not top-level action semantic enum values.
3. Signature owns declarations/signatures; GraphWrite does not call or duplicate ensure_*.
4. component_bound_event and delegate_operation=bind/assign/unbind/call/clear all have complete-evidence positive paths.
5. Missing component/binding/delegate/signature/handler evidence returns deterministic diagnostics.
6. delegate_operation=unbind never falls back to clear when handler evidence is missing.
7. EventDelegate resolver consumes projected ContextEvidence and does not rebuild TaskSpec, scan GraphBody, or repair missing context.
8. Bound event spawner receives a non-empty binding set through the shared adapter.
9. Create Event nodes are use-site references only and never create handler declarations.
10. Automation evidence has failed=0 and notRun=0 for Gap5 source contract, resolver, fragment, capability, regression, and compile.
11. Gap/design/four-cluster docs do not overclaim broader GraphWrite completion.
```

- [x] **Step 2: Fix blocking findings**

Blocking findings from the single-thread review were handled in code/tests/docs before closure:
- stale AgentFace TypeScript contract expectations were updated to include `component_bound_event` and dotted delegate statement kinds.
- old Gap5 status wording in docs was updated.
- stale `SelectedSpawner != null` wording was narrowed to exclude the intentional `delegate.assign` manual factory.

- [x] **Step 3: Final completion gate**

Completion requires:

```text
No top-level Assign/Unbind/DelegateCall/DelegateClear action semantics.
AgentFace Python tests pass.
AgentFace build/node tests pass.
ActionContext EventDelegate tests pass.
EventDelegate resolver tests pass.
EventDelegate fragment/readback tests pass.
EventDelegate use-site resolver/fragment/capability readback tests pass.
Capability80 tests pass.
Source contract tests pass.
Full BlueprintHelper.GraphWrite regression passes.
UE 5.6 compile passes.
git diff --check passes.
Single-thread review/source contract has no blocking findings.
Gap/design/progress docs are synchronized.
```

## Non-Goals

- Do not create Function / Custom Event / Event Dispatcher / Override Event declarations in GraphWrite.
- Do not modify handler signatures from EventDelegate.
- Do not auto-create missing handlers in GraphWrite.
- Do not silently convert `delegate.unbind` into `delegate.unbind_all` / `clear`.
- Do not use parsed-node delegate fallback.
- Do not make delegate actions a shortcut through `call`.
- Do not add top-level `Assign`, `Unbind`, `DelegateCall`, or `DelegateClear` action semantics.
- Do not close broad Generic `create` / `convert` / `schedule` gaps as part of Gap5.

## Suggested Manual Commit Message After Implementation

```text
新增内容：
1. 增加 EventDelegate use-site 正向 spawner、fragment 和执行验收。
2. 增加 delegate 二级 operation 的 AgentFace 编译与测试。

变更需求：
1. 明确 EventDelegate 一级语义为 ComponentBoundEvent / Delegate，bind/assign/unbind/call/clear 作为二级 delegate_operation。
2. 明确 GraphWrite/EventDelegate 只消费 projected evidence，不拥有 Signature 声明和签名。
3. 关闭 Gap5 第一阶段 EventDelegate use-site spawner 边界。
```

Manual commit commands after implementation:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper
git add -- <only files modified by the final Gap5 implementation>
git commit -m "<use the commit message above>"
```
