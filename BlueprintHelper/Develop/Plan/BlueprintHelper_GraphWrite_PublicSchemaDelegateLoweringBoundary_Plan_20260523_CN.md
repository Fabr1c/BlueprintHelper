# GraphWrite Public Schema Delegate Lowering Boundary Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Do not use subagents unless the user explicitly re-enables subagent development. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 固定 GraphWrite EventDelegate public schema / compiler lowering / C++ parser 边界：Agent-facing 输入可以继续使用 `delegate.bind` 等压缩 kind，但 lowering 后只能进入 `kind="delegate" + delegate_operation` 的内部形态。

**Architecture:** Public TaskSpec schema owns Agent-facing convenience syntax; Python compiler owns lowering from public syntax to canonical GraphWrite IR; C++ SemanticIR parser consumes only canonical internal syntax and must not accept dotted delegate public syntax as internal graph body. 该计划只固定边界和 contract，不改变 EventDelegate resolver、fragment builder、Signature ownership 或 Gap5 use-site 语义。

**Tech Stack:** TypeScript contract metadata/tests, Python TaskSpec compiler/tests, UE 5.6 C++ SemanticIR parser/tests, Unreal Automation Tests, PowerShell verification.

---

## Execution Rules

- Do not run `git add`, `git commit`, or `git push`.
- Do not introduce top-level internal statement kinds `bind`, `assign`, `unbind`, `unbind_all`, `delegate.bind`, `delegate.assign`, `delegate.unbind`, `delegate.unbind_all`, or `delegate.call`.
- Do not change Signature ownership. Handler/function/event dispatcher declarations remain owned by Signature.
- Do not change EventDelegate resolver/fragment semantics in this slice.
- Change order is fixed:
  1. `task-contract.ts` and TypeScript contract tests.
  2. Python compiler contract tests and compiler lowering guards.
  3. C++ SemanticIR parser tests and parser guard.
  4. Verification and doc sync.

## Execution Update - 2026-05-23

Status: complete.

Implementation evidence:
- TypeScript contract metadata now exposes `agent_facing_statement_kinds`, `compiler_internal_statement_kinds`, `delegate_operations`, `public_to_internal_lowering`, and `forbidden_internal_top_level_statement_kinds`.
- Python compiler keeps public delegate aliases separate from `INTERNAL_DELEGATE_STATEMENT_KIND` and rejects Agent-authored `kind="delegate"` in public TaskSpec with a deterministic diagnostic.
- C++ SemanticIR parser tests prove dotted public delegate kinds are rejected as `statement_kind_unsupported`, while canonical `kind="delegate" + delegate_operation` is accepted.
- Source contract now checks Python lowering constants and C++ parser boundary tokens.

Verification evidence:
- AgentFace `npm.cmd run build`: passed.
- AgentFace `npm.cmd run test:node`: 154 pass, 0 fail.
- AgentFace `python -m unittest discover -s python/tests -t python`: 64 tests OK.
- UE 5.6 `Build.bat TemplateEditor Win64 Development`: `Result: Succeeded`.
- `BlueprintHelper.GraphWrite.GraphSemanticIR.DelegateBoundary`: 2 succeeded, 0 failed, 0 notRun, report `Saved/Automation/GraphWrite_DelegateBoundary_SemanticIR_FINAL_001/index.json`.
- `BlueprintHelper.GraphWrite.ActionResolution.Contract`: 6 succeeded, 0 failed, 0 notRun, report `Saved/Automation/GraphWrite_DelegateBoundary_SourceContract_FINAL_001/index.json`.
- Source text guards found no production output assignment to top-level `bind/assign/unbind/unbind_all/delegate_call/delegate_clear`, and no dotted delegate public kinds in C++ SemanticIR parser sources.

## Boundary Invariants

Agent-facing public GraphBody may include:

```text
component_bound_event
delegate.bind
delegate.assign
delegate.unbind
delegate.unbind_all
delegate.call
```

Compiler-owned internal GraphBody may include:

```json
{"kind": "component_bound_event", "component": "...", "delegate": "...", "handler": "..."}
{"kind": "delegate", "delegate_operation": "bind", "target": "...", "delegate": "...", "handler": "..."}
{"kind": "delegate", "delegate_operation": "assign", "target": "...", "delegate": "...", "handler": "..."}
{"kind": "delegate", "delegate_operation": "unbind", "unbind_mode": "single", "target": "...", "delegate": "...", "handler": "..."}
{"kind": "delegate", "delegate_operation": "clear", "unbind_mode": "all", "target": "...", "delegate": "..."}
{"kind": "delegate", "delegate_operation": "call", "target": "...", "delegate": "...", "args": {}}
```

Compiler-owned internal GraphBody must not include:

```text
bind
assign
unbind
unbind_all
delegate.bind
delegate.assign
delegate.unbind
delegate.unbind_all
delegate.call
delegate_call
delegate_clear
```

## File Responsibilities

| File | Responsibility in this plan |
|---|---|
| `AgentFaceService/task-core/src/task/schema/task-contract.ts` | Publish the public vs internal EventDelegate boundary in machine-readable contract metadata. |
| `AgentFaceService/task-core/src/tests/task/task-contract-graphwrite.test.ts` | Assert public schema exposes dotted delegate syntax, while internal lowering contract exposes only canonical `delegate + delegate_operation`. |
| `AgentFaceService/task-core/src/tests/task/task-contract.test.ts` | Update whole-contract expectation only if the metadata shape changes and this test pins it. |
| `AgentFaceService/task-core/python/tests/test_graph_write_delegate_statements.py` | Prove Python compiler lowers every public delegate syntax to canonical internal form and rejects Agent-authored internal `kind="delegate"`. |
| `AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py` | Keep public aliases separate from internal canonical delegate syntax; reject forbidden Agent-facing internal/top-level shapes with deterministic diagnostics. |
| `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphSemanticIRRuntimeFactTests.cpp` | Add C++ parser contract tests for canonical delegate parsing and dotted public syntax rejection. |
| `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.cpp` | C++ statement-kind parser must accept only `delegate`, not dotted public syntax. |
| `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md` | Sync design wording if implementation reveals a mismatch. |

## Task 1: TypeScript Public Schema Contract

**Files:**
- Modify: `AgentFaceService/task-core/src/task/schema/task-contract.ts`
- Modify: `AgentFaceService/task-core/src/tests/task/task-contract-graphwrite.test.ts`
- Modify if whole-contract fixture fails: `AgentFaceService/task-core/src/tests/task/task-contract.test.ts`

- [x] **Step 1: Add failing TypeScript contract test**

Add this test inside `describe('GraphWrite TaskPlan contract metadata', ...)` in `task-contract-graphwrite.test.ts`:

```ts
  it('pins EventDelegate public schema and internal lowering boundary', () => {
    const firstSlice = TASK_PROTOCOL_CONTRACT_V1.supported_first_slice;
    const boundary = TASK_PROTOCOL_CONTRACT_V1.graph_write_taskspec_contract.event_delegate_use_site_boundary;

    assert.deepEqual(boundary.agent_facing_statement_kinds, [
      'component_bound_event',
      'delegate.bind',
      'delegate.assign',
      'delegate.unbind',
      'delegate.unbind_all',
      'delegate.call',
    ]);
    assert.deepEqual(boundary.compiler_internal_statement_kinds, [
      'component_bound_event',
      'delegate',
    ]);
    assert.deepEqual(boundary.delegate_operations, [
      'bind',
      'assign',
      'unbind',
      'clear',
      'call',
    ]);
    assert.deepEqual(boundary.public_to_internal_lowering, {
      component_bound_event: { kind: 'component_bound_event' },
      'delegate.bind': { kind: 'delegate', delegate_operation: 'bind' },
      'delegate.assign': { kind: 'delegate', delegate_operation: 'assign' },
      'delegate.unbind': { kind: 'delegate', delegate_operation: 'unbind', unbind_mode: 'single' },
      'delegate.unbind_all': { kind: 'delegate', delegate_operation: 'clear', unbind_mode: 'all' },
      'delegate.call': { kind: 'delegate', delegate_operation: 'call' },
    });
    assert.deepEqual(boundary.forbidden_internal_top_level_statement_kinds, [
      'bind',
      'assign',
      'unbind',
      'unbind_all',
      'delegate.bind',
      'delegate.assign',
      'delegate.unbind',
      'delegate.unbind_all',
      'delegate.call',
      'delegate_call',
      'delegate_clear',
    ]);
    assert.equal(firstSlice.statement_kinds.includes('delegate.bind'), true);
    assert.equal(firstSlice.statement_kinds.includes('delegate'), false);
  });
```

- [x] **Step 2: Run the TypeScript contract test and verify it fails**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
npm.cmd run build
node --test build/tests/task/task-contract-graphwrite.test.js
```

Expected before implementation:

```text
fail
compiler_internal_statement_kinds is undefined
```

- [x] **Step 3: Update `task-contract.ts` metadata**

In `TASK_PROTOCOL_CONTRACT_V1.graph_write_taskspec_contract.event_delegate_use_site_boundary`, replace the current object with:

```ts
    event_delegate_use_site_boundary: {
      note: 'GraphWrite delegate statements are use-site only. Handler declarations and signatures must already exist or be emitted by a blueprint_signature dependency before the GraphWrite body step.',
      public_shape: 'Agent-facing TaskSpec may use compact delegate.* statement kinds.',
      internal_shape: 'Compiler lowering emits only kind="component_bound_event" or kind="delegate" + delegate_operation. delegate.unbind_all lowers to delegate_operation="clear" and unbind_mode="all".',
      agent_facing_statement_kinds: [
        'component_bound_event',
        'delegate.bind',
        'delegate.assign',
        'delegate.unbind',
        'delegate.unbind_all',
        'delegate.call',
      ],
      compiler_internal_statement_kinds: [
        'component_bound_event',
        'delegate',
      ],
      delegate_operations: [
        'bind',
        'assign',
        'unbind',
        'clear',
        'call',
      ],
      public_to_internal_lowering: {
        component_bound_event: { kind: 'component_bound_event' },
        'delegate.bind': { kind: 'delegate', delegate_operation: 'bind' },
        'delegate.assign': { kind: 'delegate', delegate_operation: 'assign' },
        'delegate.unbind': { kind: 'delegate', delegate_operation: 'unbind', unbind_mode: 'single' },
        'delegate.unbind_all': { kind: 'delegate', delegate_operation: 'clear', unbind_mode: 'all' },
        'delegate.call': { kind: 'delegate', delegate_operation: 'call' },
      },
      forbidden_internal_top_level_statement_kinds: [
        'bind',
        'assign',
        'unbind',
        'unbind_all',
        'delegate.bind',
        'delegate.assign',
        'delegate.unbind',
        'delegate.unbind_all',
        'delegate.call',
        'delegate_call',
        'delegate_clear',
      ],
    },
```

- [x] **Step 4: Update whole-contract expectation if needed**

If `task-contract.test.ts` fails because it pins the full `graph_write_taskspec_contract`, add the same `event_delegate_use_site_boundary` fields to its expected object. Do not change unrelated contract sections.

- [x] **Step 5: Re-run TypeScript contract tests**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
npm.cmd run build
node --test build/tests/task/task-contract-graphwrite.test.js
node --test build/tests/task/task-contract.test.js
```

Expected:

```text
pass
fail 0
```

## Task 2: Python Compiler Contract Tests

**Files:**
- Modify: `AgentFaceService/task-core/python/tests/test_graph_write_delegate_statements.py`

- [x] **Step 1: Add failing tests for public-to-internal lowering**

Append these tests to `GraphWriteDelegateStatementCompilerTests`:

```python
    def test_all_public_delegate_kinds_lower_to_canonical_internal_delegate_shape(self):
        cases = [
            (
                {"kind": "delegate.bind", "target": "DoorSensor", "delegate": "OnDoorOpened", "handler": "HandleDoorOpened"},
                {"kind": "delegate", "delegate_operation": "bind"},
            ),
            (
                {"kind": "delegate.assign", "target": "DoorSensor", "delegate": "OnDoorOpened", "handler": "HandleDoorOpened"},
                {"kind": "delegate", "delegate_operation": "assign"},
            ),
            (
                {"kind": "delegate.unbind", "target": "DoorSensor", "delegate": "OnDoorOpened", "handler": "HandleDoorOpened"},
                {"kind": "delegate", "delegate_operation": "unbind", "unbind_mode": "single"},
            ),
            (
                {"kind": "delegate.unbind_all", "target": "DoorSensor", "delegate": "OnDoorOpened"},
                {"kind": "delegate", "delegate_operation": "clear", "unbind_mode": "all"},
            ),
            (
                {"kind": "delegate.call", "target": "DoorSensor", "delegate": "OnDoorOpened"},
                {"kind": "delegate", "delegate_operation": "call"},
            ),
        ]

        forbidden_internal_kinds = {
            "bind",
            "assign",
            "unbind",
            "unbind_all",
            "delegate.bind",
            "delegate.assign",
            "delegate.unbind",
            "delegate.unbind_all",
            "delegate.call",
            "delegate_call",
            "delegate_clear",
        }

        for public_statement, expected in cases:
            with self.subTest(public_kind=public_statement["kind"]):
                task_plan_statement = compiled_task_plan_statement(public_statement)
                bridge_statement = compiled_bridge_statement(public_statement)
                for lowered in (task_plan_statement, bridge_statement):
                    self.assertEqual(lowered["kind"], expected["kind"])
                    self.assertEqual(lowered["delegate_operation"], expected["delegate_operation"])
                    self.assertNotIn(lowered["kind"], forbidden_internal_kinds)
                    if "unbind_mode" in expected:
                        self.assertEqual(lowered["unbind_mode"], expected["unbind_mode"])

    def test_agent_authored_internal_delegate_statement_is_rejected(self):
        with self.assertRaises(TaskSpecCompileError) as ctx:
            compile_graph_write_append(make_delegate_spec({
                "kind": "delegate",
                "delegate_operation": "bind",
                "target": "DoorSensor",
                "delegate": "OnDoorOpened",
                "handler": "HandleDoorOpened",
            }), dry_run=True)

        self.assertEqual(ctx.exception.code, "unsupported_statement_kind")
        self.assertIn("delegate.bind", str(ctx.exception))
```

- [x] **Step 2: Run the focused Python test and verify the new rejection test fails**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
python -m unittest discover -s python/tests -t python -p test_graph_write_delegate_statements.py
```

Expected before compiler hardening:

```text
FAIL or ERROR in test_agent_authored_internal_delegate_statement_is_rejected
```

If the test already passes because `kind="delegate"` is currently rejected by `SUPPORTED_GRAPH_BODY_STATEMENT_KINDS`, keep the test as a regression guard and continue.

## Task 3: Python Compiler Boundary Hardening

**Files:**
- Modify: `AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py`
- Test: `AgentFaceService/task-core/python/tests/test_graph_write_delegate_statements.py`

- [x] **Step 1: Split public aliases from internal canonical constants**

Replace the delegate constants with:

```python
PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES = {
    "component_bound_event": "component_bound_event",
    "delegate.bind": "bind",
    "delegate.assign": "assign",
    "delegate.unbind": "unbind",
    "delegate.unbind_all": "clear",
    "delegate.call": "call",
}
INTERNAL_DELEGATE_STATEMENT_KIND = "delegate"
DELEGATE_STATEMENT_OPERATION_KINDS = {"bind", "assign", "unbind", "clear", "call"}
FORBIDDEN_AGENT_DELEGATE_INTERNAL_KINDS = {
    "delegate",
    "bind",
    "assign",
    "unbind",
    "unbind_all",
    "delegate_call",
    "delegate_clear",
}
SUPPORTED_GRAPH_BODY_STATEMENT_KINDS = {
    "call",
    "set",
    "set_property",
    "let",
    "control",
    *PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES.keys(),
}
```

- [x] **Step 2: Update helper names without changing lowering behavior**

Use this helper body:

```python
def _normalized_delegate_statement_kind(kind: str) -> str:
    return PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES.get(kind, kind)
```

Use this helper body:

```python
def _delegate_statement_operation(statement: Dict[str, Any]) -> Optional[str]:
    kind = statement.get("kind")
    if not isinstance(kind, str):
        return None
    if kind == INTERNAL_DELEGATE_STATEMENT_KIND:
        operation = statement.get("delegate_operation")
        if isinstance(operation, str) and operation in DELEGATE_STATEMENT_OPERATION_KINDS:
            return operation
        return None

    if kind not in PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES or kind == "component_bound_event":
        return None

    normalized_kind = _normalized_delegate_statement_kind(kind)
    if normalized_kind in DELEGATE_STATEMENT_OPERATION_KINDS:
        return normalized_kind
    return None
```

- [x] **Step 3: Add deterministic public-schema rejection for internal delegate kinds**

At the start of `_validate_supported_statements`, before the generic unsupported-kind branch, add:

```python
        if kind in FORBIDDEN_AGENT_DELEGATE_INTERNAL_KINDS:
            raise TaskSpecCompileError(
                "unsupported_statement_kind",
                "Unsupported GraphWrite statement kind.",
                [{
                    "code": "unsupported_statement_kind",
                    "path": f"{statement_path}.kind",
                    "message": "Use component_bound_event or delegate.bind/delegate.assign/delegate.unbind/delegate.unbind_all/delegate.call in Agent-facing TaskSpec. The compiler owns kind=delegate + delegate_operation lowering.",
                }],
            )
```

Then update this branch:

```python
        if kind in PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES:
            _validate_delegate_statement_shape(statement, statement_path)
```

- [x] **Step 4: Verify Python lowering contract**

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

## Task 4: C++ SemanticIR Parser Contract

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphSemanticIRRuntimeFactTests.cpp`
- Modify only if tests fail: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.cpp`

- [x] **Step 1: Add C++ parser tests**

Add helpers to `BlueprintHelperGraphSemanticIRRuntimeFactTests.cpp` inside the local utils class:

```cpp
static TSharedPtr<FJsonObject> MakeLogicSpecWithRawStatementKind(const FString& Kind, const FString& DelegateOperation = FString())
{
	TSharedPtr<FJsonObject> LogicSpec = MakeShared<FJsonObject>();
	LogicSpec->SetStringField(TEXT("schema"), TEXT("BlueprintLogicSpec.v1"));

	TSharedPtr<FJsonObject> Statement = MakeShared<FJsonObject>();
	Statement->SetStringField(TEXT("id"), TEXT("stmt_delegate_boundary"));
	Statement->SetStringField(TEXT("kind"), Kind);
	Statement->SetStringField(TEXT("target"), TEXT("DoorSensor"));
	Statement->SetStringField(TEXT("delegate"), TEXT("OnDoorOpened"));
	Statement->SetStringField(TEXT("handler"), TEXT("HandleDoorOpened"));
	if (!DelegateOperation.IsEmpty())
	{
		Statement->SetStringField(TEXT("delegate_operation"), DelegateOperation);
	}

	TArray<TSharedPtr<FJsonValue>> Statements;
	Statements.Add(MakeShared<FJsonValueObject>(Statement));
	LogicSpec->SetArrayField(TEXT("statements"), Statements);
	return LogicSpec;
}

static bool HasDiagnosticCode(const FBlueprintHelperGraphSemanticIR& IR, const FString& Code)
{
	for (const FBlueprintHelperGraphDiagnostic& Diagnostic : IR.Diagnostics)
	{
		if (Diagnostic.Code == Code)
		{
			return true;
		}
	}
	return false;
}
```

Add this test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphSemanticIRDelegateBoundary_RejectsDottedPublicKinds,
	"BlueprintHelper.GraphWrite.GraphSemanticIR.DelegateBoundary.RejectsDottedPublicKinds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphSemanticIRDelegateBoundary_RejectsDottedPublicKinds::RunTest(const FString& Parameters)
{
	const TArray<FString> PublicOnlyKinds = {
		TEXT("delegate.bind"),
		TEXT("delegate.assign"),
		TEXT("delegate.unbind"),
		TEXT("delegate.unbind_all"),
		TEXT("delegate.call")
	};

	bool bPassed = true;
	for (const FString& Kind : PublicOnlyKinds)
	{
		FBlueprintHelperGraphSemanticIR IR;
		FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(
			FBlueprintHelperGraphSemanticIRRuntimeFactTestsLocalUtils::MakeLogicSpecWithRawStatementKind(Kind),
			nullptr,
			IR);

		bPassed &= TestTrue(*FString::Printf(TEXT("%s produces unsupported kind diagnostic"), *Kind),
			FBlueprintHelperGraphSemanticIRRuntimeFactTestsLocalUtils::HasDiagnosticCode(IR, TEXT("statement_kind_unsupported")));
		if (IR.Statements.Num() > 0)
		{
			bPassed &= TestEqual(*FString::Printf(TEXT("%s remains unknown internally"), *Kind),
				IR.Statements[0]->Kind,
				EBlueprintHelperGraphStatementKind::Unknown);
		}
	}
	return bPassed;
}
```

Add this test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphSemanticIRDelegateBoundary_AcceptsCanonicalInternalDelegate,
	"BlueprintHelper.GraphWrite.GraphSemanticIR.DelegateBoundary.AcceptsCanonicalInternalDelegate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphSemanticIRDelegateBoundary_AcceptsCanonicalInternalDelegate::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphSemanticIR IR;
	const bool bBuilt = FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(
		FBlueprintHelperGraphSemanticIRRuntimeFactTestsLocalUtils::MakeLogicSpecWithRawStatementKind(TEXT("delegate"), TEXT("bind")),
		nullptr,
		IR);

	TestTrue(TEXT("canonical delegate logic spec builds"), bBuilt);
	TestEqual(TEXT("statement count"), IR.Statements.Num(), 1);
	if (IR.Statements.Num() == 1)
	{
		TestEqual(TEXT("canonical statement kind"), IR.Statements[0]->Kind, EBlueprintHelperGraphStatementKind::Delegate);
		TestEqual(TEXT("canonical delegate operation"), IR.Statements[0]->DelegateOperation, FString(TEXT("bind")));
	}
	TestFalse(TEXT("no unsupported kind diagnostic"),
		FBlueprintHelperGraphSemanticIRRuntimeFactTestsLocalUtils::HasDiagnosticCode(IR, TEXT("statement_kind_unsupported")));
	return true;
}
```

- [x] **Step 2: Run the C++ parser tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.GraphSemanticIR.DelegateBoundary;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_DelegateBoundary_SemanticIR_RED_001'
```

Expected:

```text
If current parser already rejects dotted public kinds, failed = 0.
If it accepts any dotted public kind, failed > 0 and Step 3 is required.
```

- [x] **Step 3: Confirm C++ parser hardening was already present**

In `FBlueprintHelperGraphSemanticIRUtils::ParseStatementKind`, keep only this EventDelegate parser surface:

```cpp
	if (Kind.Equals(TEXT("component_bound_event"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::ComponentBoundEvent;
	if (Kind.Equals(TEXT("delegate"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::Delegate;
```

Do not add:

```cpp
delegate.bind
delegate.assign
delegate.unbind
delegate.unbind_all
delegate.call
bind
assign
unbind
delegate_call
delegate_clear
```

- [x] **Step 4: Re-run C++ parser tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.GraphSemanticIR.DelegateBoundary;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_DelegateBoundary_SemanticIR_GREEN_001'
```

Expected:

```text
failed = 0
notRun = 0
```

## Task 5: Source Boundary Contract

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`

- [x] **Step 1: Extend source contract to include Python and C++ parser boundary**

Add a new automation test or extend the existing EventDelegate boundary contract to scan:

```text
AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py
BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.cpp
```

Required tokens in Python compiler:

```text
PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES
INTERNAL_DELEGATE_STATEMENT_KIND
FORBIDDEN_AGENT_DELEGATE_INTERNAL_KINDS
delegate_operation
```

Required C++ parser tokens:

```text
TEXT("delegate")
TEXT("component_bound_event")
statement_kind_unsupported
```

Forbidden C++ parser tokens near `ParseStatementKind`:

```text
delegate.bind
delegate.assign
delegate.unbind
delegate.unbind_all
delegate.call
delegate_call
delegate_clear
```

- [x] **Step 2: Run source contract**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Contract;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_DelegateBoundary_SourceContract_GREEN_001'
```

Expected:

```text
failed = 0
notRun = 0
```

## Task 6: Verification And Documentation Sync

**Files:**
- Modify if wording mismatch exists: `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
- Modify if wording mismatch exists: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_Gap5_EventDelegateUseSite_ImplementationPlan_20260523_CN.md`

- [x] **Step 1: Run full AgentFace verification**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
npm.cmd run build
npm.cmd run test:node
python -m unittest discover -s python/tests -t python
```

Expected:

```text
npm build exit code 0
Node fail = 0
Python OK
```

- [x] **Step 2: Run UE compile and GraphWrite boundary regression**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.GraphSemanticIR.DelegateBoundary;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_DelegateBoundary_SemanticIR_FINAL_001'
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Contract;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_DelegateBoundary_SourceContract_FINAL_001'
```

Expected:

```text
Build Result: Succeeded
DelegateBoundary failed = 0, notRun = 0
ActionResolution.Contract failed = 0, notRun = 0
```

- [x] **Step 3: Run source text guards**

Run:

```powershell
rg -n 'kind["'']?: ?["''](bind|assign|unbind|unbind_all|delegate_call|delegate_clear)["'']' AgentFaceService/task-core/src AgentFaceService/task-core/python BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement
rg -n 'delegate\.bind|delegate\.assign|delegate\.unbind|delegate\.unbind_all|delegate\.call' BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement
```

Expected:

```text
No production parser/lowering output path emits top-level bind/assign/unbind/unbind_all/delegate_call/delegate_clear.
C++ GraphStatement parser source does not accept dotted delegate public kinds.
Test files may contain forbidden strings as negative fixtures.
```

- [x] **Step 4: Sync docs only if they conflict**

If the design or Gap5 plan docs still imply C++ parser accepts `delegate.bind` directly, replace that wording with:

```markdown
Agent-facing TaskSpec may use `delegate.bind` / `delegate.assign` / `delegate.unbind` / `delegate.unbind_all` / `delegate.call`.
The Python compiler lowers these public convenience kinds into internal `kind:"delegate"` plus `delegate_operation`.
C++ SemanticIR consumes only the internal `delegate` shape and rejects dotted delegate public kinds as `statement_kind_unsupported`.
```

- [x] **Step 5: Final cleanliness check**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper
git diff --check
git status --short
```

Expected:

```text
git diff --check exit code 0
No unrelated files staged
```

## Completion Criteria

- `TASK_PROTOCOL_CONTRACT_V1` explicitly separates public delegate statement kinds from compiler-internal canonical statement kinds.
- Python compiler tests prove public `delegate.*` inputs lower to `kind="delegate" + delegate_operation`.
- Python compiler rejects Agent-authored `kind="delegate"` in public TaskSpec.
- C++ SemanticIR tests prove dotted `delegate.*` public kinds are rejected by the parser.
- C++ SemanticIR tests prove canonical `kind="delegate" + delegate_operation` is accepted.
- Source contract guards the boundary from regression.
- UE 5.6 compile, AgentFace build/node/python, C++ boundary automation, source contract, and `git diff --check` pass.

## Suggested Manual Commit Message After Implementation

```text
新增内容：
1. 增加 GraphWrite EventDelegate public schema / internal lowering 边界 contract。
2. 增加 Python compiler 与 C++ SemanticIR parser 的 delegate lowering 边界测试。

修复内容：
1. 固定 Agent-facing delegate.* 输入只能由 compiler lowering 成 kind=delegate + delegate_operation。
2. 禁止 C++ parser 接收 dotted delegate public kind 作为内部 GraphBody statement。
```
