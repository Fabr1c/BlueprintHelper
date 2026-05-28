# GraphWrite Full-Surface Result Symbol Output Contract Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend `result_symbol` from a fixed set of known value-producing statement kinds to every GraphWrite statement whose resolved fragment or spawned node exposes a valid data output, without adding timer-specific behavior or legacy fallback paths.

**Architecture:** Keep the mainline as `TaskSpec GraphBody -> SemanticIR -> ActionContextPipeline -> ActionResolutionCore -> FragmentDAG -> Composer/Linker/MutationCoordinator -> UE Mutator`. Replace the current kind whitelist and fixed endpoint mapping with a reusable statement output contract resolver: SemanticIR records a graph-local deferred Temporary, ActionContext treats that Temporary as graph-local, FragmentDAG carries a deferred statement-output producer, and the generation/link phase resolves the final data pin from generated fragment `DataOutputs` or rejects with a precise diagnostic. TS performs shape validation and preserves output hints, while UE remains the authority for resolved node pins.

**Tech Stack:** UE 5.6 C++, BlueprintHelper GraphWrite SemanticIR, ActionContextPipeline, FragmentDAG, Blueprint graph generation/linking, AgentFaceService TypeScript task compiler, Unreal Automation Tests, BlueprintHelper CLI preview/execute.

---

## Current Boundary

Current implementation is already generalized past `container_action`, but it is not full-surface:

- TS `VALUE_PRODUCING_STATEMENT_KINDS` is a fixed set: `call`, `create`, `convert`, `schedule`, `container_action`.
- UE SemanticIR has the same fixed statement-kind allow list.
- FragmentDAG currently maps outputs by kind: `call -> ReturnValue`, `create/convert/schedule -> value`, `container_action -> result`.
- `call` and `schedule` require explicit output type evidence before `result_symbol` can register.

That model works for timer handles and other known producer families, but it cannot express "any GraphWrite node that actually has a data output" because TS and SemanticIR make the final decision before the UE node or fragment has been spawned.

## Design Decision

### Option A: Keep expanding the whitelist

Add more statement kinds to `VALUE_PRODUCING_STATEMENT_KINDS` and `StatementKindCanReturnGraphLocalValue`.

Trade-off:
- Simple patch.
- Still misses future action-provider, field-capability, delegate-call, control-provider, or other GraphWrite fragments that expose data outputs.
- Continues to encode endpoint names in multiple places.

Decision: reject.

### Option B: Add operation-specific result handlers

Add resolver branches for each operation family, for example schedule, field capability, delegate call, select, struct, and future action clusters.

Trade-off:
- Better than a kind-only whitelist.
- Creates repeated special cases and will drift from actual generated pins.
- Violates the "no timer special case / no operation-specific fallback" requirement.

Decision: reject.

### Option C: Add a full-surface statement output contract resolver

Introduce a shared output contract layer that can represent known static aliases and deferred generated-fragment outputs. `result_symbol` becomes a request to bind a graph-local symbol to a statement data output. The final output is selected from the generated fragment's `DataOutputs` with explicit rules.

Trade-off:
- Requires small data model changes in SemanticIR, FragmentDAG, and the graph generation/link phase.
- Provides one reusable path for current and future GraphWrite statement builders.
- Makes support evidence-based: a statement is supported only if it resolves to a data output.

Decision: implement Option C.

## Definitions

Full-surface support means:
- Any GraphWrite statement that generates a fragment with at least one valid data output can declare `result_symbol`.
- `let` is the statement adapter for expression-only value nodes (`op`, `select`, `construct`, `deconstruct`, pure `call`, `create`, `convert`, and field getter expressions). It must use the same output contract internally instead of a separate symbol-registration path.
- Field getter statements are allowed only when they are value-producing reads and declare `result_symbol`; field setter statements stay side-effect-only and cannot fabricate a result.
- If exactly one data output is available, it can be selected automatically.
- If multiple data outputs are available, the TaskSpec must provide `result_pin`.
- If no data output exists, preview/build fails with `result_symbol_no_data_output`.
- If `result_pin` does not match a data output alias, preview/build fails with `result_symbol_output_pin_not_found`.
- If multiple outputs exist and no `result_pin` is provided, preview/build fails with `result_symbol_ambiguous_output`.

Full-surface support does not mean:
- Exec-only nodes become value producers.
- `branch`, `sequence`, `return`, generic control continuation, variable set, field set, or delegate bind/assign/unbind fabricate fake data outputs.
- UI/editor-only actions are pulled into GraphWrite.
- Legacy import-node fallback is used to make unsupported nodes appear supported.
- Pure expressions become value producers through `let`. Existing `let.name` remains supported as the default symbol name, but the implementation must be backed by the same output contract as `result_symbol`.

## Contract Rules

TaskSpec fields:

```json
{
  "kind": "call",
  "target": "GetActorBounds",
  "result_symbol": "ActorOrigin",
  "result_pin": "Origin",
  "args": {
    "OnlyCollidingComponents": { "kind": "literal", "value_type": "bool", "value": true }
  }
}
```

Selection order:

1. If `result_pin` is present, match it against generated fragment `DataOutputs` and aliases case-insensitively.
2. If exactly one non-exec data output exists, select it.
3. If a canonical alias exists and maps to one generated output, select it. Canonical aliases are `ReturnValue`, `return`, `result`, and `value`.
4. If more than one candidate remains, reject with `result_symbol_ambiguous_output`.
5. If no data output exists, reject with `result_symbol_no_data_output`.

Diagnostics:

| Code | Severity | Meaning |
| --- | --- | --- |
| `result_symbol_no_data_output` | error | Statement generated no usable data output. |
| `result_symbol_ambiguous_output` | error | Statement generated multiple data outputs and no `result_pin` selected one. |
| `result_symbol_output_pin_not_found` | error | `result_pin` did not match generated output pins or aliases. |
| `result_symbol_expression_scope_invalid` | error | Attempted to use `result_symbol` on expression-only JSON without a statement wrapper, or on a statement kind that cannot produce a fragment output. |
| `result_symbol_let_alias_conflict` | error | `let` provided both `name` and `result_symbol` with different values. |
| `result_symbol_unresolved_statement_output` | error | Statement did not spawn a fragment, so no output can be resolved. |

## File Structure

Modify:
- `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
  - Preserve `result_pin`.
  - Remove TS as the final output-kind authority.
  - Keep unsafe expression rejection for `schedule` and explicitly impure `call`.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/compiler/task-compiler.graph-local-value-producer.test.ts`
  - Extend existing tests for deferred result symbols and `result_pin`.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
  - Add `ResultPinName` to statement IR.
  - Add deferred statement-output metadata to graph-local expression/symbol structures.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
  - Register deferred Temporary symbols without requiring call/schedule output type evidence.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
  - Keep Temporary get suppression and cover deferred statement-output symbols.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagBuilderUtils.h`
  - Extend `FBlueprintHelperDagDataProducer` so a producer can be static or deferred.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagBuilderUtils.cpp`
  - Register `result_symbol` as a deferred output producer instead of resolving only by kind.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp`
  - Resolve deferred data-edge sources after fragments are generated and before data links are applied.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphLocalValueProducerTests.cpp`
  - Add focused C++ tests for deferred symbols, ambiguity, explicit pin selection, and no-output rejection.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FullSurfaceResultSymbol_OutputContract_ImplementationPlan_20260527_CN.md`
  - Record implementation evidence when this plan is executed.

Create:
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementOutputContract.h`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementOutputContract.cpp`

The new utility owns only output candidate collection, alias matching, and selection diagnostics. It must not spawn nodes, inspect UI state, or call legacy import-node paths.

## Task 1: TypeScript TaskSpec Contract

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/compiler/task-compiler.graph-local-value-producer.test.ts`

- [ ] **Step 1: Add failing tests for `result_pin` preservation**

Add tests:

```ts
test('statement result_symbol preserves result_pin for UE output contract resolution', () => {
  const task = makeGraphTask({
    kind: 'call',
    target: 'GetActorBounds',
    result_symbol: 'ActorOrigin',
    result_pin: 'Origin',
    args: {
      OnlyCollidingComponents: { kind: 'literal', value_type: 'bool', value: true },
    },
  });

  const compiled = compileTaskSpec(task);
  const statement = compiled.logic.statements[0] as Record<string, unknown>;
  assert.equal(statement.result_symbol, 'ActorOrigin');
  assert.equal(statement.result_pin, 'Origin');
});
```

Expected before implementation: FAIL because `result_pin` is not preserved or validated.

- [ ] **Step 2: Add failing tests that TS no longer requires output type evidence for `call` and `schedule`**

Add tests:

```ts
test('call result_symbol can defer output validation to UE without value_type', () => {
  assert.doesNotThrow(() => compileTaskSpec(makeGraphTask({
    kind: 'call',
    target: 'Random Integer in Range',
    result_symbol: 'RandomIndex',
    args: {
      Min: { kind: 'literal', value_type: 'int', value: 0 },
      Max: { kind: 'literal', value_type: 'int', value: 3 },
    },
  })));
});

test('schedule result_symbol can defer output validation to UE without value_type', () => {
  assert.doesNotThrow(() => compileTaskSpec(makeGraphTask({
    kind: 'schedule',
    schedule_operation: 'timer_delegate_node',
    target: 'Set Timer by Event',
    result_symbol: 'LoopTimerHandle',
    args: {
      Time: { kind: 'literal', value_type: 'float', value: 0.25 },
      bLooping: { kind: 'literal', value_type: 'bool', value: true },
    },
  })));
});
```

Expected before implementation: FAIL because current TS requires explicit output evidence.

- [ ] **Step 3: Add failing tests for `let` and field getter coverage**

Add tests:

```ts
test('let uses output contract and accepts matching result_symbol alias', () => {
  assert.doesNotThrow(() => compileTaskSpec(makeGraphTask({
    kind: 'let',
    name: 'SelectedIndex',
    result_symbol: 'SelectedIndex',
    value: {
      kind: 'select',
      condition: { kind: 'literal', value_type: 'bool', value: true },
      then: { kind: 'literal', value_type: 'int', value: 1 },
      else: { kind: 'literal', value_type: 'int', value: 0 },
    },
  })));
});

test('let rejects conflicting name and result_symbol aliases', () => {
  assert.throws(
    () => compileTaskSpec(makeGraphTask({
      kind: 'let',
      name: 'SelectedIndex',
      result_symbol: 'OtherIndex',
      value: { kind: 'literal', value_type: 'int', value: 1 },
    })),
    /result_symbol_let_alias_conflict/,
  );
});

test('field getter statement can declare result_symbol', () => {
  assert.doesNotThrow(() => compileTaskSpec(makeGraphTask({
    kind: 'field',
    field_operation: 'get',
    field_scope: 'variable',
    target: 'Health',
    result_symbol: 'HealthValue',
  })));
});
```

Expected before implementation: FAIL because `let` rejects `result_symbol` and field statements only allow `set`.

- [ ] **Step 4: Keep expression safety tests**

Preserve existing assertions:

```ts
assert.throws(
  () => validateSupportedExpression({ kind: 'schedule', schedule_operation: 'latent_or_async_node' }, 'statements[0].value'),
  /impure schedule expressions require a statement result_symbol/
);
```

Expected after implementation: PASS. `schedule` and explicitly impure `call` expressions remain rejected.

- [ ] **Step 5: Implement TS shape changes**

Change TS validation to:

```ts
function validateStatementResultSymbol(record: Record<string, unknown>, path: string): void {
  if (!Object.hasOwn(record, 'result_symbol')) return;

  getRequiredString(record, 'result_symbol', `${path}.result_symbol`);
  if (Object.hasOwn(record, 'result_pin')) {
    getRequiredString(record, 'result_pin', `${path}.result_pin`);
  }

  if (record.kind === 'let' && Object.hasOwn(record, 'name')) {
    const name = getRequiredString(record, 'name', `${path}.name`);
    const resultSymbol = getRequiredString(record, 'result_symbol', `${path}.result_symbol`);
    if (name.localeCompare(resultSymbol, undefined, { sensitivity: 'accent' }) !== 0) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', 'let result_symbol must match name.', [
        {
          code: 'result_symbol_let_alias_conflict',
          path: `${path}.result_symbol`,
          message: 'let.name is the public symbol for expression values; result_symbol may only repeat the same name.',
        },
      ]);
    }
  }
}
```

Update field statement validation:

```ts
if (kind === 'field') {
  const { operation } = fieldOperationScope(statementRecord, statementPath);
  if (operation === 'get') {
    if (!Object.hasOwn(statementRecord, 'result_symbol')) {
      throw new TaskSpecCompileError('unsupported_field_operation', 'Field get statements require result_symbol.', [
        {
          code: 'field_get_statement_requires_result_symbol',
          path: `${statementPath}.result_symbol`,
          message: 'Use a field get expression inside let, or provide result_symbol for a field get statement.',
        },
      ]);
    }
    return;
  }
  if (operation !== 'set') {
    throw new TaskSpecCompileError('unsupported_field_operation', 'Field statements require field_operation=get or set.', [
      {
        code: 'unsupported_field_operation',
        path: `${statementPath}.field_operation`,
        message: 'Field statements support set, plus get when result_symbol captures the output.',
      },
    ]);
  }
}
```

Remove the TS-only hard dependency on `VALUE_PRODUCING_STATEMENT_KINDS` for final support. UE preview/build becomes the output authority.

- [ ] **Step 6: Run TS tests**

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
```

Expected:
- Build exit code 0.
- Node tests pass.

## Task 2: SemanticIR Deferred Temporary Symbol

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphLocalValueProducerTests.cpp`

- [ ] **Step 1: Add failing SemanticIR tests**

Add test cases:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLocalValueProducerDefersCallOutputContractTest,
	"BlueprintHelper.GraphWrite.GraphLocalValueProducer.DefersCallOutputContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLocalValueProducerDefersCallOutputContractTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphSemanticIR IR;
	TestTrue(TEXT("builds semantic IR"), BuildGraphLocalValueProducerLogicSpecWithoutValueType(IR));

	FBlueprintHelperGraphSymbol Symbol;
	TestTrue(TEXT("result symbol registered"), IR.TryFindSymbol(TEXT("RandomIndex"), Symbol));
	TestEqual(TEXT("symbol target kind"), Symbol.Type, FString());
	TestEqual(TEXT("source statement id recorded"), Symbol.SourceStatementId, TEXT("stmt_random"));
	return true;
}
```

Expected before implementation: FAIL because current SemanticIR emits `result_symbol_missing_output_type`.

- [ ] **Step 2: Parse and store `result_pin`**

Add to `FBlueprintHelperGraphStatementIR`:

```cpp
FString ResultPinName;
```

Parse from JSON:

```cpp
StatementObject->TryGetStringField(TEXT("result_pin"), Statement->ResultPinName);
```

- [ ] **Step 3: Register deferred Temporary**

Replace the current output-type gate with deferred registration:

```cpp
static bool StatementCanDeclareDeferredResultSymbol(const EBlueprintHelperGraphStatementKind Kind)
{
	return Kind != EBlueprintHelperGraphStatementKind::Unknown
		&& Kind != EBlueprintHelperGraphStatementKind::Return
		&& Kind != EBlueprintHelperGraphStatementKind::Branch
		&& Kind != EBlueprintHelperGraphStatementKind::Sequence;
}
```

Then:

```cpp
if (!Statement->ResultSymbolName.TrimStartAndEnd().IsEmpty())
{
	if (!StatementCanDeclareDeferredResultSymbol(Statement->Kind))
	{
		AddDiagnostic(OutIR, TEXT("result_symbol_expression_scope_invalid"), Statement->Path + TEXT(".result_symbol"), TEXT("result_symbol requires a statement that can generate a node fragment with a data output."));
	}
	else
	{
		RegisterSymbol(OutIR, Statement->ResultSymbolName, Statement->StatementId, MakeDeferredStatementOutputExpression(*Statement), Statement->Path + TEXT(".result_symbol"), ScopeStack);
	}
}
```

Do not require `value_type` for `call` or `schedule` at this layer.

- [ ] **Step 4: Move `let` onto the same contract path**

Replace the separate `let` symbol bypass with:

```cpp
if (Statement->Kind == EBlueprintHelperGraphStatementKind::Let)
{
	const FString PublicSymbolName = !Statement->ResultSymbolName.TrimStartAndEnd().IsEmpty()
		? Statement->ResultSymbolName
		: Statement->Name;
	if (!Statement->ResultSymbolName.TrimStartAndEnd().IsEmpty()
		&& !Statement->Name.TrimStartAndEnd().IsEmpty()
		&& !Statement->ResultSymbolName.Equals(Statement->Name, ESearchCase::CaseSensitive))
	{
		AddDiagnostic(OutIR, TEXT("result_symbol_let_alias_conflict"), Statement->Path + TEXT(".result_symbol"), TEXT("let result_symbol must match name."));
	}
	else
	{
		RegisterSymbol(OutIR, PublicSymbolName, Statement->StatementId, MakeDeferredStatementOutputExpression(*Statement), Statement->Path + TEXT(".name"), ScopeStack);
	}
}
```

Acceptance:
- `let.name` remains valid for existing TaskSpecs.
- `let.result_symbol` is accepted only when it matches `name`.
- The symbol uses deferred output contract metadata rather than a separate implicit producer.

- [ ] **Step 5: Preserve graph-local lookup priority**

Verify later `get` expressions still search symbol scope before variable/member resolution. A matched deferred symbol must resolve as `EBlueprintHelperGraphTargetKind::Temporary`.

- [ ] **Step 6: Run focused SemanticIR automation**

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.GraphLocalValueProducer.DefersCallOutputContract;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_ResultSymbolDeferredSemanticIR_001'
```

Expected:
- Test passes.

## Task 3: Output Contract Utility

**Files:**
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementOutputContract.h`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementOutputContract.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphLocalValueProducerTests.cpp`

- [ ] **Step 1: Add failing output-selection tests**

Add C++ tests that construct output candidates directly:

```cpp
TArray<FBlueprintHelperGraphStatementOutputCandidate> Candidates;
Candidates.Add({ TEXT("ReturnValue"), TEXT("ReturnValue"), TEXT("int") });
FBlueprintHelperGraphStatementOutputSelection Selection;
TestTrue(TEXT("single output selected"), FBlueprintHelperGraphStatementOutputContract::SelectOutput(TEXT(""), Candidates, Selection));
TestEqual(TEXT("selected pin"), Selection.PinName, TEXT("ReturnValue"));
```

Add ambiguity test:

```cpp
Candidates.Add({ TEXT("Origin"), TEXT("Origin"), TEXT("Vector") });
Candidates.Add({ TEXT("BoxExtent"), TEXT("BoxExtent"), TEXT("Vector") });
TestFalse(TEXT("ambiguous without result_pin"), FBlueprintHelperGraphStatementOutputContract::SelectOutput(TEXT(""), Candidates, Selection));
TestEqual(TEXT("diagnostic"), Selection.DiagnosticCode, TEXT("result_symbol_ambiguous_output"));
```

- [ ] **Step 2: Define utility types**

Create:

```cpp
struct FBlueprintHelperGraphStatementOutputCandidate
{
	FString PortId;
	FString PinName;
	FString Type;
	TArray<FString> Aliases;
};

struct FBlueprintHelperGraphStatementOutputSelection
{
	bool bResolved = false;
	FString PortId;
	FString PinName;
	FString Type;
	FString DiagnosticCode;
	FString DiagnosticMessage;
};
```

- [ ] **Step 3: Implement deterministic selection**

Rules:
- Normalize aliases case-insensitively.
- `result_pin` exact pin or alias wins.
- Single candidate wins.
- Canonical alias wins only if it maps to one candidate.
- Multiple candidates without `result_pin` fail.
- Empty candidates fail.

- [ ] **Step 4: Run focused utility tests**

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.GraphLocalValueProducer.OutputContract;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_ResultSymbolOutputContract_001'
```

Expected:
- Tests pass.

## Task 4: FragmentDAG Deferred Producers

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagBuilderUtils.h`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagBuilderUtils.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphLocalValueProducerTests.cpp`

- [ ] **Step 1: Extend producer state**

Add fields:

```cpp
bool bDeferredStatementOutput = false;
FString SourceStatementId;
FString ResultPinName;
```

Keep `Endpoint` for already-resolved static producers.

- [ ] **Step 2: Register deferred producer for `result_symbol`**

When a statement has `ResultSymbolName`:

```cpp
FBlueprintHelperDagDataProducer ResultProducer;
ResultProducer.bDeferredStatementOutput = true;
ResultProducer.SourceStatementId = Statement->StatementId;
ResultProducer.ResultPinName = Statement->ResultPinName;
ResultProducer.SymbolId = NormalizeSymbolKey(Statement->ResultSymbolName);
ResultProducer.Path = Statement->Path;
RegisterSymbolProducer(Statement->ResultSymbolName, ResultProducer, Statement->Path, State, SymbolScopes);
```

Remove the current kind-to-pin mapping from this path. Do not choose `ReturnValue`, `value`, or `result` before generated outputs are known.

- [ ] **Step 3: Let data edges carry deferred producers**

Update `AddDataEdge` so a deferred producer can create a data edge without a concrete source endpoint:

```cpp
if (Producer.bDeferredStatementOutput)
{
	Edge.From.FragmentId = Producer.SourceStatementId;
	Edge.From.PortId = TEXT("__deferred_statement_output__");
	Edge.From.PinName = Producer.ResultPinName;
	Edge.SymbolId = Producer.SymbolId;
}
```

The generated graph phase must resolve this before linking. If it reaches the linker unresolved, that is an error.

- [ ] **Step 4: Add DAG tests**

Create a logic spec:
- Statement 1: `call` with `result_symbol: "RandomIndex"`.
- Statement 2: `field set` with value `{ "kind": "get", "name": "RandomIndex" }`.

Expected DAG:
- One data edge from source fragment `stmt_random`.
- Source endpoint port id is `__deferred_statement_output__`.
- Symbol id is `randomindex`.

## Task 5: Generation-Time Output Resolution

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphLocalValueProducerTests.cpp`

- [ ] **Step 1: Add resolver before data links are applied**

Add a helper near existing data-edge pin resolution:

```cpp
static bool ResolveDeferredStatementOutputEdge(
	const FBlueprintHelperGraphFragmentDataEdge& DataEdge,
	const TArray<FBlueprintHelperNodeFragment>& GeneratedFragments,
	FBlueprintHelperGraphFragmentEndpointRef& OutResolvedSource,
	FBlueprintGeneratorDiagnostic& OutDiagnostic)
```

The helper:
- Finds the generated source fragment by `DataEdge.From.FragmentId`.
- Converts `Fragment.DataOutputs` and `Fragment.PinBindings` to output candidates.
- Uses `DataEdge.From.PinName` as `result_pin`.
- Calls `FBlueprintHelperGraphStatementOutputContract::SelectOutput`.
- Emits the exact diagnostic from the contract on failure.

- [ ] **Step 2: Integrate into semantic data edge connection**

Before `FindFragmentPinByKey(SourceFragment->DataOutputs, DataEdge.From.PinName)` runs, detect:

```cpp
const bool bDeferredSource = DataEdge.From.PortId.Equals(TEXT("__deferred_statement_output__"), ESearchCase::CaseSensitive);
```

If deferred:
- Resolve the source endpoint.
- Link from the resolved pin.
- Do not fall back to arbitrary first output.

- [ ] **Step 3: Add single-output success test**

Synthetic generated fragment:
- `DataOutputs` contains only `ReturnValue`.
- Deferred edge has no `result_pin`.

Expected:
- Resolver selects `ReturnValue`.
- No diagnostic.

- [ ] **Step 4: Add multiple-output explicit-pin test**

Synthetic generated fragment:
- `DataOutputs` contains `Origin` and `BoxExtent`.
- Deferred edge has `PinName = "Origin"`.

Expected:
- Resolver selects `Origin`.
- No diagnostic.

- [ ] **Step 5: Add multiple-output missing-pin rejection test**

Synthetic generated fragment:
- `DataOutputs` contains `Origin` and `BoxExtent`.
- Deferred edge has empty `PinName`.

Expected:
- Resolver rejects with `result_symbol_ambiguous_output`.
- No data connection is created.

- [ ] **Step 6: Add no-output rejection test**

Synthetic generated fragment:
- `DataOutputs` is empty.

Expected:
- Resolver rejects with `result_symbol_no_data_output`.

## Task 6: Expression Adapter, Field Getter, and Demand Semantics

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphLocalValueProducerTests.cpp`

- [ ] **Step 1: Keep `let.name` while moving it to the shared output contract**

Do not remove `let.name`. `let` remains the statement adapter for expression-only value nodes, but its producer registration must use the same deferred output contract as `result_symbol`.

Acceptance:
- `let` with pure `call`, `select`, `construct`, `deconstruct`, `create`, or `convert` expression keeps working.
- `let` with matching `result_symbol` works and registers one graph-local Temporary producer.
- `let` with conflicting `name` and `result_symbol` returns `result_symbol_let_alias_conflict`.
- Pure expression nodes are not added as new top-level statement kinds just to support result capture.

- [ ] **Step 2: Support field getter as a result-symbol statement**

Acceptance:
- `field_operation=get` and `field_scope=variable/property_path/field_access/component_ref` may appear as a statement only when `result_symbol` is present.
- Field getter statements build through the existing field capability / variable-get / break-struct fragment builders.
- Field setter statements remain side-effect-only; `field_operation=set` with `result_symbol` fails with `result_symbol_no_data_output`.
- No new FieldVariable fallback demand is created for the produced Temporary.

- [ ] **Step 3: Keep impure expression rejection**

Acceptance:
- `{ "kind": "schedule" }` as an expression remains rejected.
- `{ "kind": "call", "is_pure": false }` as an expression remains rejected.
- Pure `call` expression remains accepted.

- [ ] **Step 4: Keep Temporary demand suppression**

Acceptance:
- A later `{ "kind": "get", "name": "<result_symbol>" }` resolves to `Temporary`.
- ActionContext DemandCollector does not create a FieldVariable demand for that get.

## Task 7: Full-Surface E2E Smoke Cases

**Files:**
- Create run artifacts under `D:/UEProjects/Template/Saved/BlueprintHelper/CodexSmoke/GraphWriteFullSurfaceResultSymbol_20260527_001/`
- Modify evidence section in this plan after execution.

- [ ] **Step 1: New asset with timer handle and no explicit `value_type`**

Create a new Blueprint asset:

```powershell
bh.cmd task preview --file 'D:\UEProjects\Template\Saved\BlueprintHelper\CodexSmoke\GraphWriteFullSurfaceResultSymbol_20260527_001\01_create_asset.json' --develop --format full
bh.cmd task execute --file 'D:\UEProjects\Template\Saved\BlueprintHelper\CodexSmoke\GraphWriteFullSurfaceResultSymbol_20260527_001\01_create_asset.json' --preview-token <token> --develop --format full
```

Graph TaskSpec:
- Statement 1: `schedule` `timer_delegate_node`, `result_symbol: "LoopTimerHandle"`, no `value_type`.
- Statement 2: field set `LoopDoorTimerHandle` from `{ "kind": "get", "name": "LoopTimerHandle" }`.

Expected:
- Preview passes.
- Execute passes.
- Readback shows the timer handle output linked into the member variable set input.

- [ ] **Step 2: Multi-output statement with explicit `result_pin`**

Use a call that generates multiple data outputs and set `result_pin` to one output. Preferred smoke target:

```json
{
  "kind": "call",
  "target": "GetActorBounds",
  "result_symbol": "ActorOrigin",
  "result_pin": "Origin",
  "args": {
    "OnlyCollidingComponents": { "kind": "literal", "value_type": "bool", "value": true }
  }
}
```

Expected:
- Preview passes if the function resolves in the target Blueprint context.
- The selected output is `Origin`.
- A missing `result_pin` variant fails with `result_symbol_ambiguous_output`.

- [ ] **Step 3: No-output statement rejection**

Use a statement that generates only exec outputs, such as a variable set or delegate bind.

Expected:
- Preview fails.
- Diagnostic code is `result_symbol_no_data_output`.
- No fallback variable demand is created for the result symbol.

- [ ] **Step 4: Field getter statement smoke**

Use a field getter statement:

```json
{
  "kind": "field",
  "field_operation": "get",
  "field_scope": "variable",
  "target": "Health",
  "result_symbol": "HealthValue"
}
```

Then set another variable from `{ "kind": "get", "name": "HealthValue" }`.

Expected:
- Preview passes when `Health` exists.
- Execute passes.
- Readback shows a generated getter data output connected to the later setter.

- [ ] **Step 5: Let-backed expression smoke**

Use a `let` statement to wrap a pure expression-only node:

```json
{
  "kind": "let",
  "name": "SelectedIndex",
  "result_symbol": "SelectedIndex",
  "value": {
    "kind": "select",
    "condition": { "kind": "literal", "value_type": "bool", "value": true },
    "then": { "kind": "literal", "value_type": "int", "value": 1 },
    "else": { "kind": "literal", "value_type": "int", "value": 0 }
  }
}
```

Expected:
- Preview passes.
- A later get of `SelectedIndex` resolves as a Temporary and links from the select output.

## Task 8: Regression Gates

**Files:**
- No new source files beyond previous tasks.
- Update this plan with final evidence.

- [ ] **Step 1: Run TS gates**

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
```

Expected:
- Exit code 0.
- No failed node tests.

- [ ] **Step 2: Run UE focused gates**

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.GraphLocalValueProducer;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_FullSurfaceResultSymbol_Focused_001'
```

Expected:
- Exit code 0.
- No failed tests.

- [ ] **Step 3: Run GraphWrite full regression**

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_FullSurfaceResultSymbol_Full_001'
```

Expected:
- Exit code 0.
- No failed tests.

- [ ] **Step 4: Build plugin**

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -FromMsBuild
```

Expected:
- Result succeeded.

- [ ] **Step 5: Final workspace gates**

```powershell
git diff --check
git status --short
```

Expected:
- `git diff --check` exit code 0.
- `git status --short` contains only task-scoped files plus any pre-existing unrelated dirty files clearly called out in the final response.

## Task 9: Documentation and Manual Git Guidance

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FullSurfaceResultSymbol_OutputContract_ImplementationPlan_20260527_CN.md`

- [ ] **Step 1: Record implementation evidence**

Append or update:

```markdown
## Implementation Evidence

| Gate | Command | Result | Artifact |
| --- | --- | --- | --- |
| TS build | `npm.cmd --prefix AgentFaceService/task-core run build` | recorded after run | console |
| TS node tests | `npm.cmd --prefix AgentFaceService/task-core run test:node` | recorded after run | console |
| Focused GraphLocalValueProducer | `Automation RunTests BlueprintHelper.GraphWrite.GraphLocalValueProducer` | recorded after run | `D:\UEProjects\Template\Saved\Automation\GraphWrite_FullSurfaceResultSymbol_Focused_001\index.json` |
| Full GraphWrite | `Automation RunTests BlueprintHelper.GraphWrite` | recorded after run | `D:\UEProjects\Template\Saved\Automation\GraphWrite_FullSurfaceResultSymbol_Full_001\index.json` |
| Plugin build | `Build.bat TemplateEditor Win64 Development` | recorded after run | console |
| E2E smoke | `bh.cmd task preview/execute/context read` | recorded after run | `D:\UEProjects\Template\Saved\BlueprintHelper\CodexSmoke\GraphWriteFullSurfaceResultSymbol_20260527_001\` |
| Diff check | `git diff --check` | recorded after run | console |
```

- [ ] **Step 2: Provide manual commit suggestion only**

Do not run `git add`, `git commit`, or `git push`. Final response should include a manual command shaped like:

```powershell
git status --short
git add AgentFaceService/task-core/src/task/compiler/task-compiler.ts `
        AgentFaceService/task-core/src/task/compiler/task-compiler.graph-local-value-producer.test.ts `
        BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h `
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp `
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp `
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagBuilderUtils.h `
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagBuilderUtils.cpp `
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementOutputContract.h `
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementOutputContract.cpp `
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp `
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphLocalValueProducerTests.cpp `
        BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FullSurfaceResultSymbol_OutputContract_ImplementationPlan_20260527_CN.md
git commit -m "新增内容：GraphWrite 全面输出契约 result_symbol" `
  -m "1. 将 result_symbol 扩展为基于真实数据输出的图内值生产者语义。" `
  -m "修复内容：" `
  -m "1. 修复 result_symbol 依赖固定 statement kind 白名单的问题。" `
  -m "2. 修复 call/schedule 必须预填输出类型才能声明图内返回值的问题。" `
  -m "3. 修复多输出节点无法显式选择 result_pin 的问题。" `
  -m "变更需求：" `
  -m "1. 新增 statement output contract 作为 GraphWrite 输出端点的统一解析边界。"
```

## Self-Review

- Spec coverage: This plan covers TS shape preservation, deferred SemanticIR symbols, ActionContext demand suppression, FragmentDAG deferred producers, generation-time output selection, ambiguity diagnostics, E2E smoke, regression gates, and documentation sync.
- Architecture: The plan adds a reusable output contract boundary and keeps GraphWrite on the canonical mainline. It does not add timer-specific handling or legacy fallback.
- Scope: The plan covers statement-level graph-local value producers, field getter statements, and expression-only value nodes through the `let` adapter backed by the same output contract.
- Safety: No step fabricates a data output for exec-only statements. Unsupported or ambiguous outputs fail with explicit diagnostics.
- Git rule: The plan requires manual Git commands only and does not instruct automatic staging, commits, or pushes.
