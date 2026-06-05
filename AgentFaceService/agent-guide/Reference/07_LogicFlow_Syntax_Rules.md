# 07 - LogicFlow Syntax Rules

This document records LogicFlow read-back rules only. It does not contain
payload examples, grammar examples, or copyable template content. Concrete read
request shapes belong to CLI-discovered template files; concrete read payloads
belong to command results and artifacts.

## Payload Rules

1. LogicFlow payloads identify themselves as the compact logic-flow read result.
2. Mode is either execution-flow oriented or data-flow oriented.
3. The compact flow body is the only human-readable logic body in this payload.
4. Statistics are kept in the payload statistics object only.
5. Warnings are reserved for compression risk, degradation, ambiguity,
   truncation, unknown nodes, or unknown links.
6. Default logic-flow output does not contain write anchors.
7. Anchors are expert/debug-only and are not part of default LogicFlow output.

## Rendering Rules

1. Newlines separate root flow lines.
2. Two spaces form one indentation level.
3. Execution links are represented with the execution-flow delimiter.
4. Connected inputs, emitted outputs, branch labels, promoted values, relative
   references, or orphan summaries are rendering syntax only.
5. Node names, graph names, pin names, branch labels, variable names, and widget
   names are display names.
6. Display names must not be used as write anchors.
7. If a display name contains reserved syntax and cannot be parsed safely, the
   read must degrade to a structured logic-json result.
8. If traversal detects a cycle, emit an explicit cycle marker instead of
   pretending the flow is acyclic.

## Mode Rules

1. Use execution-flow mode when a stable exec-pin chain exists.
2. Use data-flow mode when the graph has no meaningful exec-pin chain, when exec
   only submits a result, or when pure data dependencies are the main structure.
3. Execution-flow may inline data expressions inside node inputs.
4. Pure expression depth, shared values, and long inline expressions should be
   promoted for readability.
5. Macro and collapsed graph bodies are not expanded by default.
6. Timeline, latent, async, loop, delegate, and branch structures must preserve
   their distinct execution exits when known.
7. Unknown callback or execution semantics must not be guessed.

## Degradation Rules

1. Use logic-md when a function, event, or custom event is readable but too large
   or branched for compact logic-flow.
2. Use logic-json for full Blueprint reads, full graph reads, patch/merge
   anchors, block ids, node refs, pin refs, link refs, raw layout, GUIDs, or
   debug inspection.
3. Unknown link semantics must degrade a logic-flow request to logic-json.
4. Unknown nodes preserve known names and pins but must add warning facts.
5. Empty readable logic, ambiguous flow, macro-boundary ambiguity, unknown nodes,
   and unknown links must be reported through warnings.
6. Do not use logic-flow as a write anchor source.
