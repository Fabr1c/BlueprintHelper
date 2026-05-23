# GraphWrite Gap5 EventDelegate Use-Site Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close Gap 5 first-stage EventDelegate use-site support for `component_bound_event`, `delegate.bind`, `delegate.assign`, `delegate.unbind`, `delegate.unbind_all`, and `delegate.call` without moving declaration/signature ownership out of Signature.

**Architecture:** Signature owns declarations and signatures; GraphWrite/EventDelegate consumes projected evidence and writes use-site graph nodes only. The implementation must preserve `TaskSpec -> SemanticIR -> ActionContextPipeline -> ActionResolutionCore -> NodeSpawner evidence -> shared adapter -> FragmentDAG/Composer` for wide-surface delegate semantics, while allowing only explicit use-site node composition such as `Bind Event to ...` plus `Create Event` reference nodes.

**Tech Stack:** UE 5.6 C++, BlueprintGraph `UBlueprintBoundEventNodeSpawner` / `UBlueprintDelegateNodeSpawner`, BlueprintHelper ActionContext pipeline, AgentFace TypeScript/Zod contract docs, Python TaskSpec compiler, Unreal Automation Tests, PowerShell verification.

---

## Execution Rules

- Do not run `git add`, `git commit`, or `git push`; provide manual commands only after completion.
- Do not assign this whole plan to `codex5.3spark`. Use `codex5.3spark` only for one small source-contract or doc-audit slice at a time.
- Use fresh workers per task when using subagent-driven execution. Each task below is intended to be independently reviewable.
- Do not close Gap 5 until all six use-site semantics have positive resolver evidence, focused execution/readback coverage, missing-evidence diagnostics, UE 5.6 compile, and doc sync.
- Do not add compatibility paths for legacy parsed-node delegate implementations.

## Fixed Boundary

Signature owns declaration and signature mutation:

- `ensure_function`
- `ensure_custom_event`
- `ensure_event_dispatcher`
- `ensure_override_event`
- signature pins, mismatch policy, migration, and removal

GraphWrite/EventDelegate owns existing-declaration use-site graph writing:

- component-bound event node placement
- delegate bind/assign/unbind/call/clear node placement
- `Create Event` delegate-reference nodes
- graph links and body content around those use sites

Handler rule:

- If the handler implementation already exists, GraphWrite may reference it through projected evidence.
- If the handler implementation does not exist, a prior Signature dependency step must create it before GraphWrite runs.
- The `Bind Event to ...` / `Assign ...` node and the `Create Event` node are GraphWrite use-site nodes.
- The function or event selected by `Create Event` remains Signature-owned declaration/signature state.

AgentFace rule:

- `delegate.unbind` and `delegate.unbind_all` are explicit and separate.
- Missing callback evidence for `delegate.unbind` must not silently downgrade to `delegate.unbind_all`.

## Canonical Use-Site Kind Mapping

Agent-facing GraphBody kinds:

| AgentFace statement kind | Internal SemanticIR statement kind | Action semantic | UE node family |
|---|---|---|---|
| `component_bound_event` | `component_bound_event` | `ComponentBoundEvent` | `UBlueprintBoundEventNodeSpawner` + `UK2Node_ComponentBoundEvent` |
| `delegate.bind` | `bind` | `Bind` | `UBlueprintDelegateNodeSpawner` + `UK2Node_AddDelegate` |
| `delegate.assign` | `assign` | `Assign` | `UBlueprintDelegateNodeSpawner` + `UK2Node_AssignDelegate` |
| `delegate.unbind` | `unbind` | `Unbind` | `UBlueprintDelegateNodeSpawner` + `UK2Node_RemoveDelegate` |
| `delegate.unbind_all` | `delegate_clear` | `DelegateClear` | `UBlueprintDelegateNodeSpawner` + `UK2Node_ClearDelegate` |
| `delegate.call` | `delegate_call` | `DelegateCall` | `UBlueprintDelegateNodeSpawner` + `UK2Node_CallDelegate` |

The Python compiler may normalize dotted AgentFace kinds to internal SemanticIR kinds, but C++ GraphStatement parsing should only consume the internal kinds listed above.

## Evidence Keys

Required projected evidence keys:

```text
delegate_name
delegate_owner_class_path
delegate_property_name
delegate_property_path
delegate_signature
delegate_signature_function_path
```

Additional keys by use site:

```text
component_bound_event:
  component_path
  component_binding_field_path

delegate.bind:
  binding_object_path
  handler_name
  handler_scope_class_path

delegate.assign:
  binding_object_path
  handler_name
  handler_scope_class_path

delegate.unbind:
  binding_object_path
  handler_name
  handler_scope_class_path
  unbind_mode=single

delegate.unbind_all:
  binding_object_path
  unbind_mode=all

delegate.call:
  binding_object_path
  argument_type_count
```

`delegate_signature` may be a friendly type token for diagnostics. `delegate_signature_function_path` is the stable verification key for pin/signature correctness. Resolver success must require a unique `FMulticastDelegateProperty` reconstructed from `delegate_owner_class_path` + `delegate_property_name` and verified against `delegate_property_path`.

## File Structure

Create:

- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h`
  - Private evidence DTO and exact projected-evidence validation helpers.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.cpp`
  - Rebuild exact delegate/component/handler evidence from projected keys without broad asset scanning.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.h`
  - Public builder entry used by `BlueprintHelperGraphFragmentBuilderRegistry`.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.cpp`
  - Invoke selected EventDelegate spawners through the shared adapter and compose `Create Event` reference nodes where required.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/python/tests/test_graph_write_delegate_statements.py`
  - Python compiler tests for AgentFace dotted delegate kinds and unbind/unbind_all distinction.

Modify:

- `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py`
  - Validate delegate statement shapes and normalize dotted AgentFace kinds to internal SemanticIR kinds.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/schema/task-contract.ts`
  - Document the first-stage GraphWrite delegate statement kinds.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/schema/task-schemas.ts`
  - Keep schema permissive but add contract-facing comments or exported kind catalog if existing pattern supports it.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`
  - Add `Assign`, `Unbind`, `DelegateCall`, `DelegateClear` semantic kinds.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.cpp`
  - Add string mappings for the new semantic kinds.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h`
  - Add delegate snapshot fields required by projected evidence.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
  - Map new EventDelegate semantics into `EventDelegateAction` demand and collect handler/unbind evidence.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextSnapshotBuilder.cpp`
  - Capture exact delegate/component binding fields on the game thread.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp`
  - Project delegate evidence keys into `ContextEvidence`.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp`
  - Resolve each use-site semantic to correct UE spawner family.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h`
  - Add optional binding set support for bound event spawners.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.cpp`
  - Pass binding sets into `UBlueprintNodeSpawner::Invoke`.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
  - Add internal statement kinds.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.cpp`
  - Parse internal statement kinds.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuilderRegistry.cpp`
  - Route EventDelegate statements to the new fragment builder.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateActionClusterTests.cpp`
  - Replace unsupported-boundary positives with selected-spawner positives.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`
  - Add delegate evidence projection tests.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWrite80PercentCapabilityTests.cpp`
  - Update the Gap5 capability row after positive readback passes.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`
  - Mark Gap 5 closed only after all verification gates pass.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
  - Sync any final wording discovered during implementation.

Do not modify:

- Signature service ownership logic except for test fixture setup that calls existing Signature APIs.
- Legacy parsed-node mutation pipeline.
- Review v1 / transaction fallback code.

---

### Task 1: AgentFace Delegate Statement Contract And Compiler Normalization

**Files:**

- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/python/tests/test_graph_write_delegate_statements.py`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/schema/task-contract.ts`

- [ ] **Step 1: Write failing Python compiler tests**

Create `test_graph_write_delegate_statements.py` with:

```python
import unittest

from blueprinthelper_task.compiler.graph_write_append import compile_graph_write_append
from blueprinthelper_task.shared.errors import TaskSpecCompileError


def make_spec(statement):
    return {
        "schema": "BlueprintHelper.TaskSpec.v1",
        "task_type": "edit_blueprint_graph",
        "target": {"asset_path": "/Game/Test/BP_Door", "target_type": "blueprint"},
        "scope_policy": {"graph_name": "EventGraph", "allow_modify_user_nodes": False},
        "behavior": {
            "graph_strategy": "append_new_owned_graph",
            "entries": [{
                "entry_type": "custom_event",
                "name": "SetupDoorDelegateBindings",
                "body": {"schema": "BlueprintLogicSpec.v2", "statements": [statement]},
            }],
        },
        "validation": {"should_compile": False, "should_save": False},
    }


class GraphWriteDelegateStatementCompilerTests(unittest.TestCase):
    def compile_nodes_for(self, statement):
        result = compile_graph_write_append(make_spec(statement), dry_run=True)
        graph_step = result["task_plan"]["steps"][-1]
        return graph_step["write"]["ops"][0]["body"]["statements"]

    def test_delegate_bind_normalizes_to_internal_bind(self):
        nodes = self.compile_nodes_for({
            "kind": "delegate.bind",
            "target": "DoorDispatcher",
            "handler": "HandleDoorDispatcher",
            "delegate": "OnDoorStateChanged",
        })
        self.assertEqual(nodes[0]["kind"], "bind")
        self.assertEqual(nodes[0]["handler"], "HandleDoorDispatcher")

    def test_delegate_unbind_requires_handler_and_stays_single(self):
        nodes = self.compile_nodes_for({
            "kind": "delegate.unbind",
            "target": "DoorDispatcher",
            "handler": "HandleDoorDispatcher",
            "delegate": "OnDoorStateChanged",
        })
        self.assertEqual(nodes[0]["kind"], "unbind")
        self.assertEqual(nodes[0]["unbind_mode"], "single")
        self.assertEqual(nodes[0]["handler"], "HandleDoorDispatcher")

    def test_delegate_unbind_missing_handler_is_rejected(self):
        with self.assertRaises(TaskSpecCompileError) as ctx:
            compile_graph_write_append(make_spec({
                "kind": "delegate.unbind",
                "target": "DoorDispatcher",
                "delegate": "OnDoorStateChanged",
            }), dry_run=True)
        self.assertEqual(ctx.exception.code, "taskspec_semantic_invalid")
        self.assertIn("handler", str(ctx.exception))

    def test_delegate_unbind_all_normalizes_to_delegate_clear(self):
        nodes = self.compile_nodes_for({
            "kind": "delegate.unbind_all",
            "target": "DoorDispatcher",
            "delegate": "OnDoorStateChanged",
        })
        self.assertEqual(nodes[0]["kind"], "delegate_clear")
        self.assertEqual(nodes[0]["unbind_mode"], "all")
        self.assertNotIn("handler", nodes[0])

    def test_component_bound_event_accepts_component_delegate_and_handler(self):
        nodes = self.compile_nodes_for({
            "kind": "component_bound_event",
            "component": "DoorTrigger",
            "delegate": "OnComponentBeginOverlap",
            "handler": "HandleDoorTriggerOverlap",
        })
        self.assertEqual(nodes[0]["kind"], "component_bound_event")
        self.assertEqual(nodes[0]["component"], "DoorTrigger")
        self.assertEqual(nodes[0]["delegate"], "OnComponentBeginOverlap")
        self.assertEqual(nodes[0]["handler"], "HandleDoorTriggerOverlap")


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the red tests**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
python -m unittest discover -s python/tests -t python
```

Expected before implementation:

```text
FAILED
unsupported_statement_kind
```

- [ ] **Step 3: Add canonical delegate kind mapping**

In `graph_write_append.py`, replace:

```python
SUPPORTED_GRAPH_BODY_STATEMENT_KINDS = {"call", "set", "set_property", "let", "control"}
```

with:

```python
DELEGATE_STATEMENT_KIND_ALIASES = {
    "component_bound_event": "component_bound_event",
    "delegate.bind": "bind",
    "delegate.assign": "assign",
    "delegate.unbind": "unbind",
    "delegate.unbind_all": "delegate_clear",
    "delegate.call": "delegate_call",
}

SUPPORTED_GRAPH_BODY_STATEMENT_KINDS = {
    "call",
    "set",
    "set_property",
    "let",
    "control",
    *DELEGATE_STATEMENT_KIND_ALIASES.keys(),
}
```

Add:

```python
def _normalized_delegate_statement_kind(kind: str) -> str:
    return DELEGATE_STATEMENT_KIND_ALIASES.get(kind, kind)
```

- [ ] **Step 4: Validate delegate statement shapes**

In `_validate_supported_statements`, after the existing `elif kind == "control"` block, add:

```python
        elif kind in DELEGATE_STATEMENT_KIND_ALIASES:
            _validate_delegate_statement(statement, statement_path)
```

Add:

```python
def _validate_delegate_statement(statement: Dict[str, Any], path: str) -> None:
    kind = _required_string(statement, "kind", f"{path}.kind")
    if kind == "component_bound_event":
        _required_string(statement, "component", f"{path}.component")
        _required_string(statement, "delegate", f"{path}.delegate")
        _required_string(statement, "handler", f"{path}.handler")
        return

    _required_string(statement, "target", f"{path}.target")
    _required_string(statement, "delegate", f"{path}.delegate")

    if kind in {"delegate.bind", "delegate.assign", "delegate.unbind"}:
        _required_string(statement, "handler", f"{path}.handler")
    elif kind == "delegate.unbind_all":
        if "handler" in statement:
            raise TaskSpecCompileError(
                "taskspec_semantic_invalid",
                "delegate.unbind_all must not provide handler.",
                [{
                    "code": "delegate_unbind_all_handler_forbidden",
                    "path": f"{path}.handler",
                    "message": "Use delegate.unbind for a specific handler or delegate.unbind_all without handler.",
                }],
            )
    elif kind == "delegate.call":
        _validate_expression_map(statement.get("args"), f"{path}.args")
```

- [ ] **Step 5: Normalize delegate nodes in compiler output**

In `_clone_logic_statement`, add this branch before returning the cloned statement:

```python
    if isinstance(out.get("kind"), str) and out["kind"] in DELEGATE_STATEMENT_KIND_ALIASES:
        original_kind = out["kind"]
        out["kind"] = _normalized_delegate_statement_kind(original_kind)
        if original_kind == "delegate.unbind":
            out["unbind_mode"] = "single"
        elif original_kind == "delegate.unbind_all":
            out["unbind_mode"] = "all"
```

Apply the same normalization in `_clone_logic_statement_with_compiled_ids`, because that function copies GraphBody statements into `BlueprintLogicSpec.v2` plan `body.statements`.

- [ ] **Step 6: Update contract documentation**

In `task-contract.ts`, update `supported_first_slice.statement_kinds` to include:

```ts
'component_bound_event',
'delegate.bind',
'delegate.assign',
'delegate.unbind',
'delegate.unbind_all',
'delegate.call',
```

Add this note under `graph_write_taskspec_contract`:

```ts
event_delegate_use_site_boundary:
  'GraphWrite delegate statements are use-site only. Handler declarations and signatures must already exist or be emitted by a blueprint_signature dependency step.'
```

- [ ] **Step 7: Run compiler tests**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
python -m unittest discover -s python/tests -t python
```

Expected:

```text
OK
```

- [ ] **Step 8: Audit completion**

Run:

```powershell
rg -n "delegate\\.unbind_all|delegate\\.unbind|delegate_clear|DELEGATE_STATEMENT_KIND_ALIASES" AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py AgentFaceService/task-core/python/tests/test_graph_write_delegate_statements.py AgentFaceService/task-core/src/task/schema/task-contract.ts
```

Expected:

```text
compiler has alias table
tests distinguish delegate.unbind from delegate.unbind_all
contract lists all first-stage delegate statement kinds
```

---

### Task 2: UE SemanticIR And ActionContext Delegate Evidence

**Files:**

- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextSnapshotBuilder.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`

- [ ] **Step 1: Add red ActionContext tests**

Append tests to `BlueprintHelperActionContextPipelineTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextDelegateBindProjectionTest,
	"BlueprintHelper.GraphWrite.ActionContext.EventDelegate.BindProjectsEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextDelegateBindProjectionTest::RunTest(const FString&)
{
	FBlueprintHelperActionContextDemand Demand;
	Demand.StatementId = TEXT("stmt_delegate_bind");
	Demand.SourcePath = TEXT("$.statements[0]");
	Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	Demand.SemanticKind = EBlueprintHelperActionSemanticKind::Bind;
	Demand.Query = TEXT("OnDoorStateChanged");
	Demand.TargetPath = TEXT("DoorDispatcher");
	Demand.BindingObjectPath = TEXT("self");
	Demand.DelegateName = TEXT("OnDoorStateChanged");
	Demand.DelegateSignature = TEXT("FDoorStateChangedSignature");
	Demand.ArgumentNames = { TEXT("NewState") };
	Demand.DefaultValues.Add(TEXT("handler_name"), TEXT("HandleDoorStateChanged"));

	FBlueprintHelperActionContextSnapshot Snapshot;
	Snapshot.Graph.GraphName = TEXT("EventGraph");

	const FBlueprintHelperResolvedActionContext Context =
		FBlueprintHelperActionContextInferenceService::BuildContextForTest(Snapshot, Demand);

	TestEqual(TEXT("cluster"), Context.ClusterKind, EBlueprintHelperSpawnerClusterKind::EventDelegateAction);
	TestEqual(TEXT("semantic"), Context.Semantic.Kind, EBlueprintHelperActionSemanticKind::Bind);
	TestEqual(TEXT("delegate_name"), Context.Evidence.FindRef(TEXT("delegate_name")), FString(TEXT("OnDoorStateChanged")));
	TestEqual(TEXT("binding object"), Context.Evidence.FindRef(TEXT("binding_object_path")), FString(TEXT("self")));
	TestEqual(TEXT("handler"), Context.Evidence.FindRef(TEXT("handler_name")), FString(TEXT("HandleDoorStateChanged")));
	return true;
}
```

Expose this test-only wrapper in `BlueprintHelperActionContextInferenceService.h`:

```cpp
#if WITH_DEV_AUTOMATION_TESTS
public:
	static FBlueprintHelperResolvedActionContext BuildContextForTest(
		const FBlueprintHelperActionContextSnapshot& Snapshot,
		const FBlueprintHelperActionContextDemand& Demand);
#endif
```

Implement it in `BlueprintHelperActionContextInferenceService.cpp`:

```cpp
#if WITH_DEV_AUTOMATION_TESTS
FBlueprintHelperResolvedActionContext FBlueprintHelperActionContextInferenceService::BuildContextForTest(
	const FBlueprintHelperActionContextSnapshot& Snapshot,
	const FBlueprintHelperActionContextDemand& Demand)
{
	return BuildContext(Snapshot, Demand);
}
#endif
```

- [ ] **Step 2: Run red compile**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected before implementation:

```text
error: 'Bind' exists, but new projection fields or test wrapper are missing
```

- [ ] **Step 3: Add action semantic enum values**

In `BlueprintHelperActionResolutionCore.h`, extend `EBlueprintHelperActionSemanticKind` after `Bind`:

```cpp
	Assign,
	Unbind,
	DelegateCall,
	DelegateClear,
```

In `SemanticKindToString`, add:

```cpp
	case EBlueprintHelperActionSemanticKind::Assign: return TEXT("assign");
	case EBlueprintHelperActionSemanticKind::Unbind: return TEXT("unbind");
	case EBlueprintHelperActionSemanticKind::DelegateCall: return TEXT("delegate_call");
	case EBlueprintHelperActionSemanticKind::DelegateClear: return TEXT("delegate_clear");
```

- [ ] **Step 4: Add internal GraphStatement kinds**

In `BlueprintHelperGraphSemanticIR.h`, extend `EBlueprintHelperGraphStatementKind` after `Return`:

```cpp
	ComponentBoundEvent,
	Bind,
	Assign,
	Unbind,
	DelegateCall,
	DelegateClear
```

In `ParseStatementKind`, add:

```cpp
	if (Kind.Equals(TEXT("component_bound_event"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::ComponentBoundEvent;
	if (Kind.Equals(TEXT("bind"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::Bind;
	if (Kind.Equals(TEXT("assign"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::Assign;
	if (Kind.Equals(TEXT("unbind"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::Unbind;
	if (Kind.Equals(TEXT("delegate_call"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::DelegateCall;
	if (Kind.Equals(TEXT("delegate_clear"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::DelegateClear;
```

- [ ] **Step 5: Map new statement kinds into ActionContext demands**

In `FBlueprintHelperActionContextDemandCollector::ToActionSemanticKind(EBlueprintHelperGraphStatementKind Kind)`, add:

```cpp
	case EBlueprintHelperGraphStatementKind::ComponentBoundEvent:
		return EBlueprintHelperActionSemanticKind::ComponentBoundEvent;
	case EBlueprintHelperGraphStatementKind::Bind:
		return EBlueprintHelperActionSemanticKind::Bind;
	case EBlueprintHelperGraphStatementKind::Assign:
		return EBlueprintHelperActionSemanticKind::Assign;
	case EBlueprintHelperGraphStatementKind::Unbind:
		return EBlueprintHelperActionSemanticKind::Unbind;
	case EBlueprintHelperGraphStatementKind::DelegateCall:
		return EBlueprintHelperActionSemanticKind::DelegateCall;
	case EBlueprintHelperGraphStatementKind::DelegateClear:
		return EBlueprintHelperActionSemanticKind::DelegateClear;
```

Update `IsEventDelegateSemantic`:

```cpp
	return SemanticKind == EBlueprintHelperActionSemanticKind::Event
		|| SemanticKind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent
		|| SemanticKind == EBlueprintHelperActionSemanticKind::Bind
		|| SemanticKind == EBlueprintHelperActionSemanticKind::Assign
		|| SemanticKind == EBlueprintHelperActionSemanticKind::Unbind
		|| SemanticKind == EBlueprintHelperActionSemanticKind::DelegateCall
		|| SemanticKind == EBlueprintHelperActionSemanticKind::DelegateClear;
```

Update `ApplyDemandKinds` so all six new use-site semantics select `EventDelegateAction` and require `Binding` + `Target`.

- [ ] **Step 6: Collect handler and unbind evidence from statements**

In `ApplyEventDelegateStatementEvidence`, interpret fields:

```cpp
// Statement.Name or Statement.Property carries delegate name.
// Statement.Target carries binding object or delegate target.
// Statement.Value literal may carry handler_name when compiler encodes it.
```

Add deterministic evidence extraction:

```cpp
if (InOutDemand.DelegateName.IsEmpty())
{
	InOutDemand.DelegateName = FirstNonEmpty(Statement.Property, Statement.Name, Statement.ResolvedTarget.Member);
}
if (InOutDemand.BindingObjectPath.IsEmpty())
{
	InOutDemand.BindingObjectPath = FirstNonEmpty(Statement.Target, Statement.ResolvedTarget.Raw);
}
if (InOutDemand.ComponentPath.IsEmpty())
{
	InOutDemand.ComponentPath = ResolveComponentPathFromTarget(Statement.ResolvedTarget);
}
if (Statement.Value.IsValid() && Statement.Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
{
	InOutDemand.DefaultValues.Add(TEXT("handler_name"), Statement.Value->LiteralValue);
}
```

Add explicit statement fields in `FBlueprintHelperGraphStatementIR`:

```cpp
	FString HandlerName;
	FString UnbindMode;
```

Parse them in `FBlueprintHelperGraphSemanticIRBuilder::ParseStatement`:

```cpp
	StatementObject->TryGetStringField(TEXT("handler"), Statement->HandlerName);
	StatementObject->TryGetStringField(TEXT("unbind_mode"), Statement->UnbindMode);
```

Use those fields in `ApplyEventDelegateStatementEvidence`:

```cpp
if (!Statement.HandlerName.TrimStartAndEnd().IsEmpty())
{
	InOutDemand.DefaultValues.Add(TEXT("handler_name"), Statement.HandlerName.TrimStartAndEnd());
}
if (!Statement.UnbindMode.TrimStartAndEnd().IsEmpty())
{
	InOutDemand.DefaultValues.Add(TEXT("unbind_mode"), Statement.UnbindMode.TrimStartAndEnd());
}
```

- [ ] **Step 7: Extend snapshot and inference fields**

In `BlueprintHelperActionContextTypes.h`, add to `FBlueprintHelperActionContextFieldSnapshot`:

```cpp
	FString FieldPath;
	bool bMulticastDelegate = false;
	bool bBlueprintAssignable = false;
	bool bBlueprintCallable = false;
	FString DelegateSignatureFunctionPath;
```

In `CaptureFields`, also capture multicast delegate properties from `Blueprint->SkeletonGeneratedClass` and component binding fields. For each `FMulticastDelegateProperty`, set:

```cpp
Field.Name = DelegateProperty->GetName();
Field.OwnerClassPath = DelegateProperty->GetOwnerClass()->GetPathName();
Field.FieldPath = DelegateProperty->GetPathName();
Field.bMulticastDelegate = true;
Field.bBlueprintAssignable = DelegateProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable);
Field.bBlueprintCallable = DelegateProperty->HasAnyPropertyFlags(CPF_BlueprintCallable);
Field.DelegateSignatureFunctionPath = DelegateProperty->SignatureFunction
	? DelegateProperty->SignatureFunction->GetPathName()
	: FString();
```

In `BuildContext`, when a matching delegate field is found, add:

```cpp
Context.Evidence.Add(TEXT("delegate_owner_class_path"), Field->OwnerClassPath);
Context.Evidence.Add(TEXT("delegate_property_name"), Field->Name);
Context.Evidence.Add(TEXT("delegate_property_path"), Field->FieldPath);
Context.Evidence.Add(TEXT("delegate_signature_function_path"), Field->DelegateSignatureFunctionPath);
```

For component-bound events, also project:

```cpp
Context.Evidence.Add(TEXT("component_binding_field_path"), ComponentField->FieldPath);
```

- [ ] **Step 8: Run ActionContext tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionContext.EventDelegate;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_ActionContext_GREEN_001'
```

Expected:

```text
failed = 0
notRun = 0
```

- [ ] **Step 9: Audit completion**

Run:

```powershell
rg -n "Assign|Unbind|DelegateCall|DelegateClear|delegate_owner_class_path|delegate_signature_function_path|component_binding_field_path" BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite
```

Expected:

```text
new semantic kinds map through ActionContext
delegate evidence is projected by SnapshotBuilder/InferenceService
no EventDelegate resolver code reads TaskSpec or scans GraphBody directly
```

---

### Task 3: EventDelegate Resolver Positive Spawner Evidence

**Files:**

- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateActionClusterTests.cpp`

- [ ] **Step 1: Replace unsupported-boundary tests with red positive tests**

In `BlueprintHelperEventDelegateActionClusterTests.cpp`, replace `AssertUnsupportedCompleteDelegateBoundary` with:

```cpp
static bool AssertResolvedDelegateSpawner(
	FAutomationTestBase& Test,
	const FString& Label,
	const FBlueprintHelperActionResolutionResult& Result,
	const FString& ExpectedStableIdPrefix,
	const FString& ExpectedNodeClass)
{
	bool bPassed = true;
	bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s status"), *Label), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	bPassed &= Test.TestNotNull(*FString::Printf(TEXT("%s selected spawner"), *Label), Result.SelectedSpawner.Get());
	bPassed &= Test.TestTrue(*FString::Printf(TEXT("%s stable id"), *Label), Result.SelectedStableId.StartsWith(ExpectedStableIdPrefix));
	bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s candidate count"), *Label), Result.CandidateActions.Num(), 1);
	if (Result.CandidateActions.Num() == 1)
	{
		bPassed &= Test.TestTrue(*FString::Printf(TEXT("%s node class"), *Label), Result.CandidateActions[0].NodeClassPath.Contains(ExpectedNodeClass));
		bPassed &= Test.TestTrue(*FString::Printf(TEXT("%s from action evidence"), *Label), Result.CandidateActions[0].bFromActionDatabase);
	}
	return bPassed;
}
```

Add one complete-evidence test per semantic:

```cpp
AssertResolvedDelegateSpawner(*this, TEXT("component bound event"), Resolve(ComponentBound), TEXT("component_bound_event:"), TEXT("K2Node_ComponentBoundEvent"));
AssertResolvedDelegateSpawner(*this, TEXT("delegate bind"), Resolve(Bind), TEXT("delegate_bind:"), TEXT("K2Node_AddDelegate"));
AssertResolvedDelegateSpawner(*this, TEXT("delegate assign"), Resolve(Assign), TEXT("delegate_assign:"), TEXT("K2Node_AssignDelegate"));
AssertResolvedDelegateSpawner(*this, TEXT("delegate unbind"), Resolve(Unbind), TEXT("delegate_unbind:"), TEXT("K2Node_RemoveDelegate"));
AssertResolvedDelegateSpawner(*this, TEXT("delegate call"), Resolve(DelegateCall), TEXT("delegate_call:"), TEXT("K2Node_CallDelegate"));
AssertResolvedDelegateSpawner(*this, TEXT("delegate clear"), Resolve(DelegateClear), TEXT("delegate_clear:"), TEXT("K2Node_ClearDelegate"));
```

- [ ] **Step 2: Run red resolver tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.EventDelegate;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_EventDelegateResolver_RED_001'
```

Expected before implementation:

```text
failed > 0
unsupported_intent or unsupported_event_delegate_cluster_semantic
```

- [ ] **Step 3: Add exact evidence helper**

In `BlueprintHelperEventDelegateUseSiteEvidence.h`, define:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class UBlueprint;
class UEdGraph;
class UFunction;
class UClass;
class FMulticastDelegateProperty;
class FObjectProperty;

struct FBlueprintHelperEventDelegateUseSiteEvidence
{
	EBlueprintHelperActionSemanticKind SemanticKind = EBlueprintHelperActionSemanticKind::Unknown;
	FString DelegateName;
	FString DelegateOwnerClassPath;
	FString DelegatePropertyName;
	FString DelegatePropertyPath;
	FString DelegateSignature;
	FString DelegateSignatureFunctionPath;
	FString ComponentPath;
	FString ComponentBindingFieldPath;
	FString BindingObjectPath;
	FString HandlerName;
	FString HandlerScopeClassPath;
	FString UnbindMode;
	FMulticastDelegateProperty* DelegateProperty = nullptr;
	FObjectProperty* ComponentBindingProperty = nullptr;
	UFunction* HandlerFunction = nullptr;
};

class FBlueprintHelperEventDelegateUseSiteEvidenceReader
{
public:
	static bool TryRead(
		const FBlueprintHelperActionResolutionRequest& Request,
		const EBlueprintHelperActionSemanticKind SemanticKind,
		FBlueprintHelperEventDelegateUseSiteEvidence& OutEvidence,
		FString& OutMissingDetail,
		FString& OutMessage);
};
```

In `.cpp`, implement `TryRead` so it:

```text
1. Reads only Request.ContextEvidence and Request.Semantic.
2. Requires delegate_name, delegate_owner_class_path, delegate_property_name, delegate_property_path, and delegate_signature.
3. Uses FindObject<UClass>(nullptr, *delegate_owner_class_path) and FindFProperty<FMulticastDelegateProperty>(OwnerClass, *delegate_property_name).
4. Verifies DelegateProperty->GetPathName() == delegate_property_path.
5. For component_bound_event, requires component_path and component_binding_field_path, then resolves an exact FObjectProperty.
6. For bind/assign/unbind, requires handler_name and resolves UFunction by handler_scope_class_path + handler_name.
7. For delegate_clear, requires unbind_mode=all and forbids handler_name.
8. Returns missing_required_evidence with the existing detailed labels when evidence is absent.
```

- [ ] **Step 4: Resolve correct UE spawner family**

In `BlueprintHelperEventDelegateActionCluster.cpp`, include:

```cpp
#include "BlueprintBoundEventNodeSpawner.h"
#include "BlueprintDelegateNodeSpawner.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_AssignDelegate.h"
#include "K2Node_CallDelegate.h"
#include "K2Node_ClearDelegate.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_RemoveDelegate.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h"
```

Map semantic to node class:

```cpp
static TSubclassOf<UEdGraphNode> DelegateNodeClassForSemantic(EBlueprintHelperActionSemanticKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperActionSemanticKind::Bind:
		return UK2Node_AddDelegate::StaticClass();
	case EBlueprintHelperActionSemanticKind::Assign:
		return UK2Node_AssignDelegate::StaticClass();
	case EBlueprintHelperActionSemanticKind::Unbind:
		return UK2Node_RemoveDelegate::StaticClass();
	case EBlueprintHelperActionSemanticKind::DelegateCall:
		return UK2Node_CallDelegate::StaticClass();
	case EBlueprintHelperActionSemanticKind::DelegateClear:
		return UK2Node_ClearDelegate::StaticClass();
	default:
		return nullptr;
	}
}
```

For `ComponentBoundEvent`:

```cpp
UBlueprintBoundEventNodeSpawner* Spawner =
	UBlueprintBoundEventNodeSpawner::Create(
		UK2Node_ComponentBoundEvent::StaticClass(),
		Evidence.DelegateProperty);
```

For delegate use-site nodes:

```cpp
UBlueprintDelegateNodeSpawner* Spawner =
	UBlueprintDelegateNodeSpawner::Create(
		Cast<UClass>(DelegateNodeClassForSemantic(SemanticKind)),
		Evidence.DelegateProperty);
```

Use a helper that returns the exact delegate-node subclass type:

```cpp
static TSubclassOf<UK2Node_BaseMCDelegate> DelegateNodeClassForSemantic(EBlueprintHelperActionSemanticKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperActionSemanticKind::Bind:
		return UK2Node_AddDelegate::StaticClass();
	case EBlueprintHelperActionSemanticKind::Assign:
		return UK2Node_AssignDelegate::StaticClass();
	case EBlueprintHelperActionSemanticKind::Unbind:
		return UK2Node_RemoveDelegate::StaticClass();
	case EBlueprintHelperActionSemanticKind::DelegateCall:
		return UK2Node_CallDelegate::StaticClass();
	case EBlueprintHelperActionSemanticKind::DelegateClear:
		return UK2Node_ClearDelegate::StaticClass();
	default:
		return nullptr;
	}
}
```

- [ ] **Step 5: Add stable candidate evidence**

Candidate evidence must use:

```text
StableId:
  component_bound_event:<delegate_property_path>:<component_binding_field_path>
  delegate_bind:<delegate_property_path>:<handler_name>
  delegate_assign:<delegate_property_path>:<handler_name>
  delegate_unbind:<delegate_property_path>:<handler_name>
  delegate_call:<delegate_property_path>
  delegate_clear:<delegate_property_path>

MatchReason:
  ue_bound_event_node_spawner
  ue_delegate_node_spawner
```

Each resolved result must include:

```cpp
Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
Result.SelectedSpawner = Spawner;
Result.SelectedStableId = StableId;
Result.CandidateActions.Add(Candidate);
```

- [ ] **Step 6: Own all first-stage semantic kinds**

Update `OwnsSemanticKind`:

```cpp
	case EBlueprintHelperActionSemanticKind::Event:
	case EBlueprintHelperActionSemanticKind::ComponentBoundEvent:
	case EBlueprintHelperActionSemanticKind::Bind:
	case EBlueprintHelperActionSemanticKind::Assign:
	case EBlueprintHelperActionSemanticKind::Unbind:
	case EBlueprintHelperActionSemanticKind::DelegateCall:
	case EBlueprintHelperActionSemanticKind::DelegateClear:
		return true;
```

- [ ] **Step 7: Run resolver tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.EventDelegate;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_EventDelegateResolver_GREEN_001'
```

Expected:

```text
failed = 0
notRun = 0
```

- [ ] **Step 8: Audit no Signature ownership leakage**

Run:

```powershell
rg -n "ensure_function|ensure_custom_event|ensure_event_dispatcher|ensure_override_event|SignatureService|BlueprintSignature" BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.cpp
```

Expected:

```text
no matches
```

---

### Task 4: Shared Adapter Binding Support And EventDelegate Fragment Builder

**Files:**

- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.cpp`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.h`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuilderRegistry.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateActionClusterTests.cpp`

- [ ] **Step 1: Add binding-set support to shared adapter**

In adapter header, include:

```cpp
#include "BlueprintNodeBinder.h"
```

Add to `FBlueprintHelperActionNodeSpawnOptions`:

```cpp
	IBlueprintNodeBinder::FBindingSet Bindings;
```

In adapter cpp, replace:

```cpp
IBlueprintNodeBinder::FBindingSet Bindings;
UEdGraphNode* SpawnedNode = NodeSpawner->Invoke(TargetGraph, Bindings, Location);
```

with:

```cpp
UEdGraphNode* SpawnedNode = NodeSpawner->Invoke(TargetGraph, Options.Bindings, Location);
```

Existing callers pass an empty set and keep current behavior.

- [ ] **Step 2: Add EventDelegate fragment builder header**

Create `BlueprintHelperEventDelegateFragmentBuilder.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

class FBlueprintHelperActionContextScope;
class UEdGraph;
struct FBlueprintHelperGraphStatementIR;

class BLUEPRINTHELPER_API FBlueprintHelperEventDelegateFragmentBuilder
{
public:
	static bool BuildStatement(
		UEdGraph* TargetGraph,
		const FBlueprintHelperActionContextScope* ActionContextScope,
		const FBlueprintHelperGraphStatementIR& Statement,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError);
};
```

- [ ] **Step 3: Implement use-site builder**

The builder must:

```text
1. Build a projected request from ActionContextScope using the statement id.
2. Call FBlueprintGraphWriteFacade::ResolveActionForGraph.
3. Invoke ActionResult.SelectedSpawner through FBlueprintHelperActionNodeSpawnerAdapter.
4. For component_bound_event, pass component FObjectProperty in SpawnOptions.Bindings.
5. For bind/assign/unbind, spawn a UK2Node_CreateDelegate reference node and set its function to handler_name.
6. Link Create Event delegate output to the delegate input pin on add/assign/remove nodes.
7. For delegate_clear and delegate_call, do not create handler nodes.
8. Populate FBlueprintHelperNodeFragment pins, ownership tags, and review targets.
```

Create Event reference node:

```cpp
UBlueprintNodeSpawner* CreateEventSpawner =
	UBlueprintNodeSpawner::Create(UK2Node_CreateDelegate::StaticClass());
FBlueprintHelperActionNodeSpawnOptions CreateEventOptions;
CreateEventOptions.NodeId = Statement.StatementId + TEXT("_create_event");
CreateEventOptions.NodeConfigurationHook =
	[HandlerName](UK2Node& SpawnedNode, const FBlueprintHelperActionNodeSpawnContext&, FString& OutError)
	{
		UK2Node_CreateDelegate* CreateDelegate = Cast<UK2Node_CreateDelegate>(&SpawnedNode);
		if (!CreateDelegate)
		{
			OutError = TEXT("create_event_reference_spawn_failed");
			return false;
		}
		CreateDelegate->SetFunction(FName(*HandlerName));
		return true;
	};
```

Do not create or modify handler functions here.

- [ ] **Step 4: Route EventDelegate statements from registry**

In `BlueprintHelperGraphFragmentBuilderRegistry.cpp`, include:

```cpp
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.h"
```

Before the final unsupported statement error, add:

```cpp
	if (Statement.Kind == EBlueprintHelperGraphStatementKind::ComponentBoundEvent
		|| Statement.Kind == EBlueprintHelperGraphStatementKind::Bind
		|| Statement.Kind == EBlueprintHelperGraphStatementKind::Assign
		|| Statement.Kind == EBlueprintHelperGraphStatementKind::Unbind
		|| Statement.Kind == EBlueprintHelperGraphStatementKind::DelegateCall
		|| Statement.Kind == EBlueprintHelperGraphStatementKind::DelegateClear)
	{
		return FBlueprintHelperEventDelegateFragmentBuilder::BuildStatement(
			TargetGraph,
			ActionContextScope,
			Statement,
			OutFragment,
			OutError);
	}
```

- [ ] **Step 5: Add focused fragment/readback tests**

In `BlueprintHelperEventDelegateActionClusterTests.cpp`, add execution tests that assert spawned node classes:

```text
component_bound_event -> UK2Node_ComponentBoundEvent
delegate.bind -> UK2Node_AddDelegate + UK2Node_CreateDelegate
delegate.assign -> UK2Node_AssignDelegate + UK2Node_CreateDelegate
delegate.unbind -> UK2Node_RemoveDelegate + UK2Node_CreateDelegate
delegate.unbind_all -> UK2Node_ClearDelegate
delegate.call -> UK2Node_CallDelegate
```

Each test must assert:

```cpp
TestTrue(TEXT("GraphWrite does not create handler declaration"), !ContainsNewFunctionNamed(Blueprint, TEXT("HandleDoorStateChanged")));
TestTrue(TEXT("node came from selected spawner evidence"), Fragment.OwnershipTags.Contains(TEXT("selected_stable_id")));
```

The handler function must be created in the fixture before GraphWrite by calling existing Signature test setup or by creating a native test function on a test class. The GraphWrite fragment builder must only reference it.

- [ ] **Step 6: Run focused fragment tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.EventDelegate;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_EventDelegateFragment_GREEN_001'
```

Expected:

```text
failed = 0
notRun = 0
```

- [ ] **Step 7: Audit adapter boundary**

Run:

```powershell
rg -n "NodeSpawner->Invoke\\(|Options\\.Bindings|IBlueprintNodeBinder::FBindingSet" BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.cpp BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.cpp
```

Expected:

```text
shared adapter owns Invoke
EventDelegate builder supplies bindings through options
no direct NodeSpawner->Invoke call outside shared adapter
```

---

### Task 5: TaskSpec Execution, Readback, And Physical Door Validation

**Files:**

- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWrite80PercentCapabilityTests.cpp`
- Add focused automation test file only if the existing EventDelegate test file becomes too large:
  `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateUseSiteExecutionTests.cpp`

- [ ] **Step 1: Build a small EventDelegate专项 asset scenario**

Create a test Blueprint fixture with:

```text
Actor Blueprint:
  Components:
    DoorTrigger: BoxComponent
  Event Dispatcher:
    OnDoorStateChanged(bool bIsOpen)
  Existing handler function:
    HandleDoorStateChanged(bool bIsOpen)
```

Setup may use existing Signature capability to create `OnDoorStateChanged` and `HandleDoorStateChanged`. Errors during asset/component/signature setup are setup errors and must not be counted as GraphWrite correctness errors.

- [ ] **Step 2: Execute all six use-site semantics**

Use a TaskSpec body containing:

```json
{
  "schema": "BlueprintLogicSpec.v2",
  "statements": [
    {
      "kind": "component_bound_event",
      "component": "DoorTrigger",
      "delegate": "OnComponentBeginOverlap",
      "handler": "HandleDoorTriggerOverlap"
    },
    {
      "kind": "delegate.bind",
      "target": "self",
      "delegate": "OnDoorStateChanged",
      "handler": "HandleDoorStateChanged"
    },
    {
      "kind": "delegate.assign",
      "target": "self",
      "delegate": "OnDoorStateChanged",
      "handler": "HandleDoorStateChanged"
    },
    {
      "kind": "delegate.unbind",
      "target": "self",
      "delegate": "OnDoorStateChanged",
      "handler": "HandleDoorStateChanged"
    },
    {
      "kind": "delegate.unbind_all",
      "target": "self",
      "delegate": "OnDoorStateChanged"
    },
    {
      "kind": "delegate.call",
      "target": "self",
      "delegate": "OnDoorStateChanged",
      "args": {
        "bIsOpen": {"kind": "literal", "value_type": "bool", "value": true}
      }
    }
  ]
}
```

Readback must confirm:

```text
K2Node_ComponentBoundEvent exists for DoorTrigger.OnComponentBeginOverlap
K2Node_AddDelegate exists for OnDoorStateChanged
K2Node_AssignDelegate exists for OnDoorStateChanged
K2Node_RemoveDelegate exists for OnDoorStateChanged
K2Node_ClearDelegate exists for OnDoorStateChanged
K2Node_CallDelegate exists for OnDoorStateChanged
UK2Node_CreateDelegate exists only for bind/assign/unbind and references HandleDoorStateChanged
```

- [ ] **Step 3: Add physical-door complex validation row**

Use the existing physical door requirement as the complex GraphWrite scenario:

```text
1. Door closed/static state has physics simulation disabled.
2. Door has light push and force open functions.
3. Door rotates around the hinge to 177 degrees.
4. Door enables physics after opening and responds to collision.
5. Door disables physics again when closed.
6. Delegate use-site validation is limited to door-owned interaction hooks; no player interaction expansion.
```

This row must include only GraphWrite errors after setup succeeds. Asset creation, component creation, variable creation, and signature creation failures remain setup errors.

- [ ] **Step 4: Update capability metrics row**

In `BlueprintHelperGraphWrite80PercentCapabilityTests.cpp`, replace the current unsupported Gap5 row:

```text
Complete component/delegate/signature projected evidence returns unsupported_intent
```

with a positive row:

```text
Complete component/delegate/signature/handler projected evidence resolves and spawns component-bound event plus delegate bind/assign/unbind/call/clear use-site nodes; no Signature ensure_* behavior is called by GraphWrite.
```

Set:

```cpp
ErrorKind = EBlueprintHelperGraphWriteCapabilityErrorKind::None
bGraphWriteCorrect = true
ResolverStatus = TEXT("resolved")
SelectedSpawnerClass = TEXT("UBlueprintBoundEventNodeSpawner|UBlueprintDelegateNodeSpawner")
SpawnedNodeClass = TEXT("K2Node_ComponentBoundEvent|K2Node_AddDelegate|K2Node_AssignDelegate|K2Node_RemoveDelegate|K2Node_CallDelegate|K2Node_ClearDelegate|K2Node_CreateDelegate")
bHasSpawnEvidence = true
bReadbackComplete = true
```

- [ ] **Step 5: Run EventDelegate use-site execution tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.EventDelegate.UseSite;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_EventDelegateUseSite_GREEN_001'
```

Expected:

```text
failed = 0
notRun = 0
```

- [ ] **Step 6: Run capability metrics tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.Capability80;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_Capability80_GREEN_001'
```

Expected:

```text
failed = 0
notRun = 0
GraphWrite correctness does not decrease
Silent wrong graph count remains 0
```

---

### Task 6: Source Contracts, Regression, Compile, And Docs

**Files:**

- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md` only if implementation reveals a wording mismatch.

- [ ] **Step 1: Add no-Signature-boundary source contract**

Add a source contract test that scans:

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
```

Expected failure message:

```text
GraphWrite/EventDelegate must not own declaration or signature mutation.
```

- [ ] **Step 2: Run focused source contracts**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Contract;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_SourceContract_GREEN_001'
```

Expected:

```text
failed = 0
notRun = 0
```

- [ ] **Step 3: Run full GraphWrite regression**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_Regression_GREEN_001'
```

Expected:

```text
failed = 0
notRun = 0
```

- [ ] **Step 4: Run UE 5.6 compile**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected:

```text
BUILD SUCCESSFUL
```

- [ ] **Step 5: Run AgentFace tests**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
npm.cmd run build
npm.cmd run test:node
python -m unittest discover -s python/tests -t python
```

Expected:

```text
npm build succeeds
node tests pass
python unittest OK
```

- [ ] **Step 6: Run whitespace check**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper
git diff --check
```

Expected:

```text
exit code 0
```

- [ ] **Step 7: Sync Gap5 closure docs**

In `BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`, mark Gap5:

```markdown
状态：已关闭（仅限 EventDelegate first-stage use-site positive spawner support）

Closure scope - 2026-05-23:
- `component_bound_event`, `bind`, `assign`, `unbind`, `delegate_call`, and `delegate_clear` resolve with complete projected evidence.
- `delegate.unbind` and `delegate.unbind_all` remain explicit and do not silently downgrade.
- GraphWrite/EventDelegate writes use-site nodes only and does not call or duplicate Signature `ensure_*` behavior.
- Handler declarations/signatures remain Signature-owned; GraphWrite only references existing handler evidence.

Closure evidence:
- `D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_ActionContext_GREEN_001\index.json`, 0 failed.
- `D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_EventDelegateResolver_GREEN_001\index.json`, 0 failed.
- `D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_EventDelegateFragment_GREEN_001\index.json`, 0 failed.
- `D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_EventDelegateUseSite_GREEN_001\index.json`, 0 failed.
- `D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_Capability80_GREEN_001\index.json`, 0 failed.
- `D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_SourceContract_GREEN_001\index.json`, 0 failed.
- `D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_Regression_GREEN_001\index.json`, 0 failed / 0 not run.
- UE 5.6 compile, `BUILD SUCCESSFUL`.
- AgentFace build/node/python tests pass.
- `git diff --check`, exit code 0.
```

Keep any remaining broad GraphWrite gaps outside this Gap5 closure.

- [ ] **Step 8: Audit docs for overclaim**

Run:

```powershell
rg -n "Gap 5|EventDelegate|Signature|ensure_|完全完成|partial|open|closed" BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md
```

Expected:

```text
Gap5 closure scope says first-stage use-site positive spawner support
Signature remains declaration/signature owner
Four-cluster completion status does not mark unrelated Generic create/convert/schedule as done
```

---

### Task 7: Independent Review

**Files:**

- Review all modified files from Tasks 1-6.

- [ ] **Step 1: Request code review**

Use a review worker with this prompt:

```text
Review GraphWrite Gap5 EventDelegate use-site implementation only. Do not edit files.

Check:
1. Signature owns declarations/signatures; GraphWrite does not call or duplicate ensure_*.
2. component_bound_event, delegate.bind, delegate.assign, delegate.unbind, delegate.unbind_all, delegate.call all have complete-evidence positive paths.
3. Missing component/binding/delegate/signature/handler evidence still returns deterministic diagnostics.
4. delegate.unbind never falls back to unbind_all when handler evidence is missing.
5. EventDelegate resolver consumes projected ContextEvidence and does not rebuild TaskSpec, scan GraphBody, or repair missing context.
6. Bound event spawner receives a non-empty binding set through the shared adapter.
7. Create Event nodes are use-site references only and never create handler declarations.
8. Automation evidence has failed=0 and notRun=0 for Gap5 source contract, resolver, fragment, execution, capability, regression, and compile.
9. Gap/design/four-cluster docs do not overclaim broader GraphWrite completion.

Return findings first with file and line references. If no findings, say no blocking issues and list residual risk.
```

- [ ] **Step 2: Fix blocking review findings**

For each blocking finding, make the smallest change that restores the stated boundary. Re-run the narrowest failed verification plus:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
```

- [ ] **Step 3: Final completion gate**

Completion requires:

```text
AgentFace Python tests pass
AgentFace build/node tests pass
ActionContext EventDelegate tests pass
EventDelegate resolver tests pass
EventDelegate fragment/readback tests pass
EventDelegate use-site execution tests pass
Capability80 tests pass
Source contract tests pass
Full BlueprintHelper.GraphWrite regression passes
UE 5.6 compile passes
git diff --check passes
Independent review has no blocking findings
Gap/design/progress docs are synchronized
```

## Non-Goals

- Do not create Function / Custom Event / Event Dispatcher / Override Event declarations in GraphWrite.
- Do not modify handler signatures from EventDelegate.
- Do not auto-create missing handlers in GraphWrite.
- Do not silently convert `delegate.unbind` into `delegate.unbind_all`.
- Do not use parsed-node delegate fallback.
- Do not make delegate actions a shortcut through `call`.
- Do not close broad Generic `create` / `convert` / `schedule` gaps as part of Gap5.

## Suggested Manual Commit Message After Implementation

```text
新增内容：
1. 增加 EventDelegate use-site 正向 spawner、fragment 和执行验收
2. 增加 delegate.unbind/unbind_all AgentFace 语义区分与编译测试

变更需求：
1. 明确 GraphWrite/EventDelegate 只消费 projected evidence，不拥有 Signature 声明和签名
2. 关闭 Gap5 第一阶段 EventDelegate use-site spawner 边界
```

Manual commit commands after implementation:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper
git add -- `
  AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py `
  AgentFaceService/task-core/python/tests/test_graph_write_delegate_statements.py `
  AgentFaceService/task-core/src/task/schema/task-contract.ts `
  AgentFaceService/task-core/src/task/schema/task-schemas.ts `
  BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextSnapshotBuilder.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuilderRegistry.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateActionClusterTests.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWrite80PercentCapabilityTests.cpp `
  BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md `
  BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md `
  BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md `
  BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_Gap5_EventDelegateUseSite_ImplementationPlan_20260523_CN.md
git commit -m "变更需求：关闭 GraphWrite Gap5 EventDelegate use-site 边界"
```
