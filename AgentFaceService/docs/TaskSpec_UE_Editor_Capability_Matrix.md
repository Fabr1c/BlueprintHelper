# TaskSpec UE Editor Capability Matrix

This document records capability boundaries only. It does not list concrete
TaskSpec fields, behavior payloads, wrapper shapes, adapter args, or copyable
templates.

Agents must use current CLI discovery/index output to find the matching template
file for exact request fields.

## Scope

In scope:

- UE editor asset mutations that are reachable through
  `BlueprintHelper.TaskSpec.v1`.
- Preview/execute behavior through AgentFace task-core, Bridge, UE TaskRuntime,
  and runtime clusters.
- Capability-family boundaries and unsupported domains.

Out of scope:

- Direct Bridge command authoring.
- Editor lifecycle open/close/modal-dismiss/close-dialog, which belongs to the global MCP lifecycle tools.
- Read, diagnostics, runtime profile, Review query, and DebugBundle workflows.
- Agent-authored TaskPlan or raw runtime adapter payloads.

## Supported Capability Families

| Capability family | Agent intent | Runtime ownership |
|---|---|---|
| Asset creation | Ensure supported UE assets exist. | Asset factory / create-asset runtime path. |
| Blueprint components | Add, configure, reuse, or remove Blueprint components. | Blueprint component runtime cluster. |
| Blueprint class settings | Manage interfaces, class defaults, and parent changes. | Class-settings runtime cluster. |
| Blueprint signatures | Manage functions, custom/interface events, dispatchers, override/native events, and guarded removal. | Signature service and runtime cluster. |
| Blueprint variables | Manage member variables, defaults, and local variables where supported. | Variable runtime cluster. |
| Blueprint graph body | Append, replace, patch, or merge supported BlueprintHelper-owned graph logic; external graph mutation remains policy-gated. | GraphWrite runtime cluster. |
| UMG widget edits | Create, update, or remove supported Widget Blueprint tree/property entries. | UMG runtime cluster. |
| DataTable edits | Add, update, or delete supported rows. | DataTable runtime cluster. |
| Object property edits | Set reflected UObject/DataAsset properties where supported. | Object-property runtime cluster. |
| Composite Blueprint feature authoring | Compile multiple supported semantic edits into one TaskPlan. | task-core decomposition plus existing runtime clusters. |

Concrete fields, enum values, strategy names, and payload structure are template
owned. Do not duplicate them here.

## GraphWrite Boundary

GraphWrite is Agent-facing through TaskSpec semantics only. Runtime adapter
operations are lowering targets and must not be authored by ordinary Agents.

GraphWrite reads and writes must preserve ownership boundaries:

- BlueprintHelper-owned content can be edited only through supported owned
  anchors returned by the read workflow and selected template.
- User-authored graph content is read-only unless a current template and preview
  policy explicitly permits a narrow external mutation.
- Display labels, raw graph array indexes, ad hoc JSON paths, and GUID-first
  selectors are not ordinary write anchors.

Field, OpCoverage, GenericOps, ContainerAction, Schedule, and EventDelegate
support are implementation capability areas. Their exact statement facts and
payload shapes are owned by task-core, runtime code, tests, and CLI-discovered
templates.

## Unsupported Agent Paths

- Direct lifecycle commands as TaskSpec writes.
- Direct GraphWrite, Component, Widget, DataTable, or ObjectProperty Bridge
  payloads.
- Global editor undo/redo as ordinary recovery.
- Details-panel or Slate UI simulation as write evidence.
- Creating new unsupported semantic kinds by copying old examples from docs.

## Verification Policy

Capability claims require all relevant layers to stay aligned:

- CLI discovery and templates.
- task-core schema/compiler tests.
- UE runtime build and focused automation.
- Real editor E2E validation for editor mutations.
- Read-back verification that proves the requested semantic result.
