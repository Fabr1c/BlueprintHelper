# Snapshot IR Design Spike

> **Context:** Part of Task S0 (Wave 0) in the BridgePayload Object-First plan.
> **Status:** Design spike only -- no code implementation.
> **Target Phase:** Phase 6 (post object-first protocol shipping).
> **Constraint:** Snapshot IR must NOT block Tasks C1 through M3.

## 1. Snapshot IR Structs

These structs form the Intermediate Representation (IR) that decouples UE runtime graph data
from the serializers that produce RawJson, LogicJson, and LogicMarkdown output.

```cpp
// Single pin on a node
struct FBlueprintHelperPinSnapshot
{
    FString PinName;           // e.g. "Exec", "Target"
    FString Direction;         // "EGPD_Input" / "EGPD_Output"
    FString PinType;           // e.g. "exec", "boolean", "object", "integer"
    FString DefaultValue;      // literal default, empty if none
};

// Single node in a graph
struct FBlueprintHelperNodeSnapshot
{
    FString NodeType;          // e.g. "K2Node_CallFunction", "K2Node_VariableGet"
    FString Guid;              // unique node identifier
    FVector2D Position;        // node position on the graph canvas
    TArray<FBlueprintHelperPinSnapshot> Pins;
};

// Single connection (link) between two pins
struct FBlueprintHelperLinkSnapshot
{
    FString SourcePinGuid;     // GUID of the source pin (or pin name if GUID unavailable)
    FString TargetPinGuid;     // GUID of the target pin
};

// A single UEdGraph
struct FBlueprintHelperGraphSnapshot
{
    FString GraphName;         // e.g. "EventGraph", "MyFuncGraph"
    TArray<FBlueprintHelperNodeSnapshot> Nodes;
    TArray<FBlueprintHelperLinkSnapshot> Links;
};

// The full Blueprint asset
struct FBlueprintHelperBlueprintSnapshot
{
    FString BlueprintPath;     // e.g. "/Game/BP/BP_Test.BP_Test"
    TArray<FBlueprintHelperGraphSnapshot> Graphs;
};
```

### Design notes

- All fields are plain data (strings, enums-as-strings, vectors, arrays of the structs above).
- No UE reflection types (`UEdGraph*`, `UEdGraphNode*`, `UEdGraphPin*`) appear in the IR.
- The IR bridges `UEdGraph` (UE runtime) and serializer input (pure data).

## 2. Serializer Boundaries

In the target state, each serializer consumes Snapshot IR and produces its output without
touching UE reflection directly:

```
                    ┌──────────────────────┐
                    │  UEdGraph / UBlueprint│  (UE runtime)
                    └──────────┬───────────┘
                               │
                    ┌──────────▼───────────┐
                    │  Snapshot IR         │  (plain data structs)
                    │  (FBlueprintHelper   │
                    │   *Snapshot)         │
                    └────┬─────┬─────┬─────┘
                         │     │     │
              ┌──────────┘     │     └──────────┐
              ▼                ▼                 ▼
     ┌────────────────┐ ┌────────────┐ ┌─────────────────┐
     │  RawJson       │ │ LogicJson  │ │ LogicMarkdown   │
     │  serializer    │ │ serializer │ │ serializer      │
     └────────────────┘ └────────────┘ └─────────────────┘
```

- **RawJson serializer**: `FBlueprintHelperGraphSnapshot` -> `FJsonObject` (`nodes`, `links`, `version`, `schema`).
- **LogicJson serializer**: `FBlueprintHelperGraphSnapshot` -> Logic graph JSON structure.
- **LogicMarkdown serializer**: `FBlueprintHelperGraphSnapshot` -> Markdown text.

### Current state

All three serializers currently read `UEdGraph` / `UBlueprint` directly, bypassing the IR.

### Future state (Phase 6+)

Each serializer receives `const FBlueprintHelperGraphSnapshot&` (or
`const FBlueprintHelperBlueprintSnapshot&` for multi-graph) and performs a pure data
transform, making it unit-testable without loading assets.

## 3. Blocking Constraint

**Snapshot IR does NOT block Tasks C1 through M3.** The object-first protocol ships with
direct `UEdGraph` -> `FJsonObject` conversion (C1). The IR is a subsequent refactoring
layer that sits between `UEdGraph` and the serializers.

- C0-C4, M0-M3 all complete without any Snapshot IR dependency.
- The IR header and converter (`UEdGraph` -> IR) are introduced in Phase 6.
- The serializer refactors (IR -> output) happen after the IR converter exists, one
  serializer at a time.

## 4. First Implementation Target

**Graph-only snapshot after object-first protocol ships.**

The initial implementation will:

1. Build a single converter: `UEdGraph` -> `FBlueprintHelperGraphSnapshot` (populates
   nodes, pins, links from UE reflection).
2. Make `ConvertGraphToJsonObject()` consume `FBlueprintHelperGraphSnapshot` instead of
   `UEdGraph` directly.
3. Add unit tests that construct `FBlueprintHelperGraphSnapshot` by hand (no editor
   dependency) and verify the output `FJsonObject` shape.

Multi-Blueprint (`FBlueprintHelperBlueprintSnapshot`) and cross-graph scenarios are
deferred until the single-graph path is stable.

## 5. Rationale

Why introduce an IR at all?

1. **Decouple serializers from UE reflection.** Currently, RawJson, LogicJson, and
   LogicMarkdown each walk `UEdGraph` nodes/pins independently. A bug fix or feature
   addition (e.g., including `Pins`->`bHidden`) must be repeated in three places. With IR,
   the walk happens once in the IR converter, and all three serializers get the fix for
   free.

2. **Make serializers pure data transforms.** `FBlueprintHelperGraphSnapshot` contains
   no UE pointers. Unit tests can feed arbitrary graph shapes without launching an editor
   or loading a `uasset`.

3. **Enable cross-serializer consistency checks.** Because all three outputs derive from
   the same IR, integration tests can verify that RawJson node count == LogicJson node
   count == Markdown node count for the same snapshot.

4. **Future-proof for new serializers.** A future output format (e.g., Mermaid diagram,
   DOT graph) only needs to consume IR -- no new `UEdGraph` traversal code.
