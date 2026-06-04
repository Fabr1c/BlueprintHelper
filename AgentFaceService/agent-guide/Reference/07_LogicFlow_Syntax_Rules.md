# 07 - LogicFlow Syntax Rules

## 1. Payload

`LogicFlow.v1` payload:

```json
{
  "schema": "LogicFlow.v1",
  "mode": "execflow | dataflow",
  "flow": "string",
  "stats": {
    "nodes": 0,
    "exec_links": 0,
    "data_links": 0,
    "orphans": 0
  },
  "warnings": []
}
```

Rules:

1. `payload.schema` is `LogicFlow.v1`.
2. `payload.mode` is `execflow` or `dataflow`.
3. `payload.flow` is the only compact logic body.
4. `payload.stats` is the only statistics object.
5. `payload.warnings` contains only compression risk, degradation, ambiguity, truncation, unknown node, or unknown link warnings.
6. Default `logic_flow` output does not contain write anchors.
7. `anchors` are expert/debug-only and are not part of default `LogicFlow.v1`.

## 2. Modes

Rules:

1. Use `execflow` when a stable ExecPin chain exists.
2. Use `dataflow` when the graph has no meaningful ExecPin chain, when ExecPin only submits a result, or when pure data dependencies are the main graph structure.
3. `execflow` may inline data expressions inside node inputs.
4. Switch to `dataflow` only when data dependencies are the main readable structure.
5. Timeline, async, and callback structures may expose nested flow blocks.
6. Macro and collapsed graph bodies are not expanded by default.

## 3. Text Grammar

Grammar:

```text
FlowBody        := GraphBlock+ | DataflowBlock | FlowLine+
GraphBlock      := "flow " GraphName ":" NewLine IndentedFlowBody
DataflowBlock   := "dataflow:" NewLine DataLine+
FlowLine        := ExecLine | BranchLine | DataLine | OrphanLine
ExecLine        := NodeExpr (" -> " NodeExprOrCycle)*
BranchLine      := Indent ExecPinName " -> " ExecLine
DataLine        := Indent DataName " = " DataExpr
OrphanLine      := "orphans: " OrphanItem (", " OrphanItem)*
NodeExpr        := NodeName InputList? OutputList?
InputList       := "[" InputItem (", " InputItem)* "]"
InputItem       := InputName "=" DataExpr
OutputList      := "(" OutputName ("," OutputName)* ")"
DataExpr        := Literal | Ref | PromotedRef | NodeExpr | FunctionExpr | FieldExpr
Ref             := "&" | "&%" SignedInteger | "&." PinName | "&%" SignedInteger "." PinName
PromotedRef     := "$p" Integer
NodeExprOrCycle := NodeExpr | "<cycle:" NodeRef ">"
```

Rules:

1. Newlines separate root flow lines.
2. Two spaces form one indentation level.
3. `->` is reserved for execution links.
4. `[]` is reserved for emitted inputs.
5. `()` is reserved for emitted outputs.
6. `, ` separates input and orphan items.
7. `,` separates output pins inside `()`.
8. `=` separates input names from values and dataflow names from expressions.
9. Node names, graph names, pin names, and branch labels are display names.
10. Display names must not be used as write anchors.
11. If a display name contains a reserved delimiter and cannot be parsed safely, the read must degrade to `logic_json`.
12. If a graph name is emitted, the graph body is rendered as `flow GraphName:` with the body indented by two spaces.
13. If traversal detects a cycle, emit `<cycle:NodeRef>` at the repeated edge.

## 4. Node Form

Syntax:

```text
NodeName[InputName=InputValue, ...](OutputName, ...)
```

Rules:

1. Use `NodeName` for the display node name.
2. Use `[]` for connected input pins and non-default literal input values.
3. Use `()` for output pins consumed by later nodes or exposed by an entry node.
4. Omit `[]` when there are no emitted inputs.
5. Omit `()` when there are no emitted outputs.
6. Omit both `[]` and `()` when only the node name is needed.
7. Input order follows pin order when stable.
8. Output order follows pin order when stable.
9. If pin order is unavailable, use exported order.
10. Duplicate pin names must be disambiguated with a short suffix or index.

Examples:

```text
BeginPlay
EventTick(DeltaSeconds)
SetActorLocation[NewLocation=TargetLocation]
SpawnActor[Class=EnemyClass, Transform=SpawnTransform](ReturnValue)
```

## 5. Execflow

Syntax:

```text
A -> B -> C
```

Rules:

1. `->` represents an execution link.
2. One root execution entry may be rendered as one line when it has no branches.
3. Multiple execution entries are rendered as separate root lines.
4. Execution node order follows reachable ExecPin traversal.
5. Unknown execution order must not be guessed.
6. Disconnected non-comment nodes are emitted in `orphans:`.
7. Execution roots prefer event, function, and custom event entry nodes.
8. If no entry node exists, execution roots use nodes with outgoing exec links and no incoming exec links.
9. If no root can be proven, the first exported node may be used with a warning.
10. Multiple outgoing exec links are sorted by known pin order, then pin name, then target node.

Example:

```text
EventBeginPlay -> InitState -> SetReady[Value=true]
```

## 6. Relative References

Syntax:

```text
&
&%N
&.PinName
&%N.PinName
```

Rules:

1. `&` references the previous execution node.
2. `&%-1` references the node two positions before the current execution node.
3. `&%1` references the next execution node.
4. `&.PinName` references an output pin on the previous execution node.
5. `&%N.PinName` references an output pin on the relative execution node at offset `N`.
6. Relative references may only target nodes in the same local execution line or branch.
7. Use promoted `$pN` values instead of relative references when the producer is outside the local readable scope.

Example:

```text
EventAxis(Axis_X,Axis_Y) -> DoMove[Right=&.Axis_X, Forward=&.Axis_Y]
```

## 7. Pure Data Expressions

Rules:

1. Nodes without ExecPins are pure data nodes.
2. Pure data nodes are inlined into the input that consumes them.
3. A pure data node that is not consumed is an orphan unless it is a comment, reroute, or other whitelist node.
4. Single-output pure nodes may omit the output pin when inlined.
5. Multi-output pure nodes must keep the output pin reference.
6. Pure expression depth above 3 should be promoted to `$pN`.
7. Shared pure expression values used by two or more consumers must be promoted to `$pN`.
8. Long inline expressions that reduce readability should be promoted to `$pN`.

Examples:

```text
SetActorLocation[NewLocation=+[GetActorLocation[], *(GetVelocity[], DeltaSeconds)]]
$p0 = GetPlayerCharacter[PlayerIndex=0]
BeginPlay -> SetOwner[NewOwner=$p0] -> AttachToActor[ParentActor=$p0]
BreakHitResult[Hit].Location
```

## 8. Dataflow

Syntax:

```text
dataflow:
  $p0 = Expression
  $p1 = Expression
  ResultName = $pN
```

Rules:

1. `dataflow:` starts a pure data graph body.
2. `$pN` declares a promoted intermediate value.
3. `$pN` numbering starts at `$p0` and increases by dependency order.
4. A `$pN` value must be declared before it is used.
5. Final named outputs are written as `OutputName = Value`.
6. Cyclic or ambiguous data dependencies must degrade to `logic_json`.

Example:

```text
dataflow:
  $p0 = GetActorLocation[]
  $p1 = GetVelocity[]
  $p2 = *[$p1, DeltaSeconds]
  ReturnValue = +[$p0, $p2]
```

## 9. Branches

Syntax:

```text
BranchNode[Input=Value]
  PinName -> Node
```

Rules:

1. Multi-output execution nodes must render each execution output as a branch.
2. Branch labels use execution pin display names.
3. Branch bodies use two-space indentation.
4. Branch bodies use the same `NodeName[inputs](outputs)` syntax.
5. Branch depth above 3 should add a warning.
6. More than 8 branch outputs should add a warning.

Examples:

```text
EventInteract -> Branch[Condition=IsValid[Object=Target]]
  True -> UseActor[Target=Target]
  False -> PrintString[InString="No target"]

BeginPlay -> Sequence
  Then0 -> InitHUD
  Then1 -> BindInput
  Then2 -> LoadSave

InputAction -> SwitchOnEnum[Selection=Mode]
  Idle -> StartIdle
  Combat -> StartCombat
  Default -> StartFallback
```

## 10. Loops

Rules:

1. Loop nodes must preserve `LoopBody`.
2. Loop nodes must preserve `Completed`.
3. Loop body parameters are exposed in the branch label when available.
4. Loop body expressions may reference loop body parameters with `&.PinName`.

Examples:

```text
ForEachLoop[Array=Enemies]
  LoopBody(ArrayElement,ArrayIndex) -> ApplyDamage[DamagedActor=&.ArrayElement]
  Completed -> PrintString[InString="Done"]

ForLoop[FirstIndex=0, LastIndex=Count]
  LoopBody(Index) -> ProcessIndex[Index=&.Index]
  Completed -> Finish
```

## 11. Latent, Timeline, And Async

Rules:

1. Latent nodes must preserve completion execution pins such as `Completed` or `Finished`.
2. Async nodes must preserve delegate-like execution outputs.
3. Timeline nodes must preserve execution outputs such as `Update`, `Finished`, `Reverse`, or custom track callbacks when known.
4. Timeline value outputs may be emitted as node outputs when they are consumed.
5. Complex Timeline tracks should degrade to `logic_md` or `logic_json`.
6. Unknown callback semantics must not be guessed.

Example:

```text
BeginPlay -> Delay[Duration=1.0]
  Completed -> SpawnActor[Class=EnemyClass](ReturnValue)
```

## 12. Variables, Properties, And Structs

Rules:

1. `Get Variable` is rendered as the variable name when used as a value.
2. `Set Variable` is rendered as `Set VariableName[Value=...]`.
3. Member or component calls may render as `Target.FunctionName[...]`.
4. `Target=Self` is omitted by default.
5. `Break Struct` multi-output fields are rendered as `.FieldName`.
6. `Make Struct` is rendered as a value expression.
7. Default literal input values are omitted.
8. Non-default literal input values are emitted.

Examples:

```text
Branch[Condition=IsDead]
EventDamage(Damage) -> Set Health[Value=Clamp[-(Health, &.Damage), 0, MaxHealth]]
SetActorLocation[NewLocation=MakeVector[X=0, Y=0, Z=BreakHitResult[Hit].Location.Z]]
Mesh.SetVisibility[NewVisibility=false]
```

## 13. Cast, Spawn, And Calls

Rules:

1. Cast nodes must preserve success and failure execution outputs when both exist.
2. Spawn nodes must expose `ReturnValue` when it is consumed.
3. Function calls use `FunctionName[inputs](outputs)`.
4. Macro and collapsed graph calls use the same boundary node form.
5. Macro and collapsed graph internals are not expanded.
6. Ambiguous macro or collapsed graph execution outputs add `macro_boundary_ambiguous`.

Examples:

```text
ActorBeginOverlap(OtherActor) -> CastToBP_Enemy[Object=&.OtherActor](AsBP_Enemy)
  Success -> ApplyDamage[DamagedActor=&.AsBP_Enemy]
  CastFailed -> PrintString[InString="Not enemy"]

SpawnActor[Class=EnemyClass, Transform=SpawnTransform](ReturnValue) -> SetOwner[Target=&.ReturnValue, NewOwner=Self]
```

## 14. Delegates And Events

Rules:

1. Delegate binding must preserve the bound event or function name.
2. Custom events as entries use normal entry syntax.
3. Event dispatcher calls preserve `Broadcast` semantics.
4. Multiple bound targets are emitted as separate lines.
5. Delegate relationships must not be compressed into ordinary function calls when doing so loses event semantics.

Examples:

```text
BeginPlay -> BindEvent[Event=OnHealthChanged, CustomEvent=HandleHealthChanged]
SetHealth -> Broadcast OnHealthChanged[NewHealth=Health]
```

## 15. Comments, Reroutes, And Orphans

Rules:

1. Reroute and knot nodes are folded by default.
2. Comment nodes are not part of execution flow.
3. Comment orphans are merged by count.
4. Non-comment orphans are emitted by name.
5. More than 5 non-comment orphans may be compressed by count with a warning.
6. Whitelist nodes do not trigger connectivity failure.

Syntax:

```text
orphans: Name xCount, Name xCount
```

Example:

```text
orphans: Comment x2, PrintString x1
```

## 16. Warnings

Rules:

1. `unknown_node` means a node is present but its semantic kind is not known.
2. `unknown_link` means a link is present but its execution/data type is not known.
3. `ambiguous_flow` means a readable flow order cannot be proven.
4. `macro_boundary_ambiguous` means a macro or collapsed graph boundary has ambiguous execution exits.
5. `empty_logic` means the selected target has no readable logic nodes.
6. `logic_flow_degraded_unknown_link` means a requested `logic_flow` response returned `logic_json` because at least one link was unknown.

## 17. Unknown And Degradation

Rules:

1. Unknown nodes preserve node names.
2. Unknown nodes preserve known input pins.
3. Unknown nodes preserve known output pins.
4. Unknown execution semantics must not be guessed.
5. Unknown link semantics must degrade the `logic_flow` request to `logic_json`.
6. Degraded payloads use `payload.schema=LogicJson.v1`.
7. Degraded payloads include `requested_format=logic_flow`.
8. Degraded payloads include warning `logic_flow_degraded_unknown_link`.
9. Unknown node warnings use `unknown_node`.
10. Unknown link warnings use `unknown_link`.
11. Ambiguous flow warnings use `ambiguous_flow`.
12. Empty readable logic warnings use `empty_logic`.

Example:

```text
UnknownNode[Input=A](Output)
```

## 18. Escalation

Rules:

1. Use `logic_md` when a function, event, or custom event is readable but too large or branched for `logic_flow`.
2. Use `logic_json` for full Blueprint reads.
3. Use `logic_json` for full graph reads.
4. Use `logic_json` for patch or merge anchors.
5. Use `logic_json` for `block_id`, `node_ref`, `pin_ref`, or `link_ref`.
6. Use `logic_json` for raw node, pin, link, GUID, layout, or debug inspection.
7. Use `logic_json` when `logic_flow` warnings contain unknown control flow or ambiguous flow.
8. Use `logic_json` when `logic_flow` degrades.
9. Do not use `logic_flow` as a write anchor source.
