# 05 - UE Blueprint Write Architecture Rules

This reference constrains how Agents choose between Blueprint edits, C++ source work, and data-driven configuration before they dispatch write work.

## 1. MainAgent Architecture Gate

Before any write dispatch, the Main Agent must classify the request:

```text
small Blueprint orchestration -> BlueprintExplorer evidence -> TaskWorker wiring
complex logic or computation -> SourcecodeWorker source implementation -> source verification -> TaskWorker wiring
data/config content -> C++ DataAsset/config contract when needed -> Blueprint/DataAsset configuration
source-only support -> SourcecodeWorker source implementation and verification, no UE asset write unless requested
```

The Main Agent owns this decision. BlueprintExplorer, SourcecodeExplorer, SourcecodeWorker, and TaskWorker execute bounded packages; they do not change the architecture classification on their own except by reporting a blocker such as `architecture_gate_mismatch`.

## 2. Blueprint Node Budget

- BlueprintHelper is a Blueprint assistance tool, not a reason to move heavy implementation into Blueprint.
- Simple business behavior may stay in Blueprint when it is light control flow and normally less than 25 nodes per function/event/macro.
- Complex gameplay logic, heavy computation, repeated algorithms, or graph work likely to require 30+ nodes belongs in C++.
- The node budget is per function, event, or macro, not the total node count of an entire Blueprint asset.

## 3. C++ And Blueprint Contract

When source work is required:

- Put reusable logic in C++ services, components, subsystems, function libraries, interfaces, or other existing project architecture boundaries.
- Expose Blueprint extension points through `BlueprintImplementableEvent`, `BlueprintNativeEvent`, or overridable interfaces.
- Blueprint overrides should connect authored gameplay responses, presentation, or simple orchestration to the C++ contract.
- Do not hardcode content values in C++ when designers or Blueprint assets should configure them.
- Do not add unnamed namespace blocks in C++.

## 4. Data-Driven Content

- Use `UDataAsset`, config structs, data tables, or explicit Blueprint config variables for tunable content.
- Blueprint assets should expose variables according to the asset semantics.
- Private variables should remain private unless the workflow explicitly requires designer configuration.
- DataAsset and config changes must be reflected in readback evidence before reporting success.

## 5. SourcecodeWorker Handoff

Dispatch `sourcecode-worker` when the architecture gate requires source work before Blueprint wiring.

The package must include:

- the architecture decision and reason;
- allowed and forbidden source write paths;
- specific source tasks;
- expected Blueprint extension points or DataAsset/config contracts;
- focused source verification commands.

SourcecodeWorker must return changed files, verification results, blockers, and the next MainAgent step. After source verification succeeds, the Main Agent may dispatch TaskWorker to wire Blueprint assets through TaskSpec preview, execute, and readback.

## 6. Stop Conditions

Stop and report when:

- Blueprint-only implementation would exceed the complexity budget;
- required C++/DataAsset/interface source work is outside the allowed write scope;
- source verification fails and the failure is not safely repairable in the current task;
- Blueprint evidence, screenshots, preview, execute, or readback disagree;
- a required BlueprintHelper capability is missing.
