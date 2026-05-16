---
name: blueprint-helper
description: Use when a user request requires accessing Unreal Engine Blueprint assets through BlueprintHelper, including reading, inspecting, creating, or modifying Blueprint-related UE editor assets.
---

# BlueprintHelper Skill

## Main Agent Role

You are the user-facing planning and decision agent for Blueprint work.

Your job is to understand the user's gameplay or editor intent, identify the target Blueprint asset and scope, protect the user's existing assets, and decide what needs to be delegated. You own the conversation, clarification questions, final explanation, and user-facing tradeoffs.

Prefer delegating BlueprintHelper CLI execution to a SideAgent. If the current Claude environment cannot dispatch a SideAgent but the Main Agent can run the required BlueprintHelper CLI command, execute the same single-command SideAgent contract locally and report that it used `main_agent_direct_fallback`.

Return `tool_unavailable` only when the required BlueprintHelper CLI command is not installed or callable. Do not describe this as write permission failure.

Use this Skill when the user asks to:

- read, inspect, summarize, create, or modify a Blueprint;
- edit Blueprint graphs, variables, components, class settings, interfaces, UMG, DataAssets, DataTables, or object properties through the UE Editor;
- compile, save, open, diagnose, or validate UE editor assets as part of BlueprintHelper work.

Do not use BlueprintHelper tools for C++, TypeScript, Python, JSON, docs, tests, config, `AGENTS.md`, or memory files. Use normal repository tools for those.

## Entry Rule

The supported Agent-facing entry for ordinary BlueprintHelper reads and writes is the BlueprintHelper CLI. Use the global BlueprintHelper MCP lifecycle commands only for opening or closing Unreal Editor from an Agent workflow; do not route ordinary read/write tools through MCP.

For complex JSON input, copy a template from `BlueprintHelper/Resources/AgentGuide/Templates/`, edit the copy, then call the CLI with `--file`. Direct tool-name TaskSpec calls use wrapper templates with root field `task_spec`; grouped `task preview` / `task execute` calls use bare `BlueprintHelper.TaskSpec.v1` files.

## Main Agent Flow

1. Read `references/08_User_Preferences.md` and `references/00_Agent_Onboarding_Index_20260504.md`.
2. Convert the user's request into intent, target, scope, and safety constraints.
3. If the target asset, target graph, or create-vs-modify strategy is unclear, ask the user before any tool delegation.
4. If BlueprintHelper access is required, first confirm the required CLI command is available. Read-only commands such as `bh blueprinthelper_read_context` do not require a write session.
5. Send a concise execution package to a SideAgent and tell it to read `references/09_SideAgent_Tool_Execution.md`. If SideAgent dispatch is unavailable but the tool is callable by the Main Agent, execute that one tool locally under the same contract.
6. Review the translated result, then decide whether to continue, ask the user for confirmation, or report the outcome.

The Main Agent owns context reuse. Keep a running summary of SideAgent returns, decide whether a follow-up can be answered from existing evidence, and only dispatch a new SideAgent when a specific missing tool result is needed.

The SideAgent is an execution and translation worker, not the conversation owner. Do not pass the full conversation or full `SKILL.md`; pass only the execution package and the reference paths it must read.

## Main Agent Context Ledger

Before dispatching a follow-up SideAgent:

- check the accumulated SideAgent results for the same asset, target, view, and evidence;
- answer directly if the existing translated result is enough;
- if more data is needed, identify the exact missing field or validation result;
- delegate one atomic BlueprintHelper tool call for that missing data, not a broad re-analysis of the same function or graph.

## SideAgent Delegation Package

When delegating, use semantic fields instead of dumping rules:

```yaml
user_goal: "<what the user wants in gameplay/editor terms>"
main_agent_decision: "<why this requires BlueprintHelper tool access>"
operation_mode: "create_new | modify_existing | inspect_only | validate_only"
target_asset_path: "<UE asset path, or unknown>"
target_graph: "<graph/function/event/widget scope, or unknown>"
safety_constraints:
  allow_modify_user_nodes: false
  require_preview: true
  require_write_session_if_disabled: true
  write_session_scope: "running Editor/Bridge, usable by delegated SideAgents within approved scope and lifetime"
read_strategy:
  avoid_full_logic_md_when_graph_size_unknown: true
  large_graph_node_threshold: 80
  large_graph_policy: "estimate size first, then read summary or block-scoped slices"
tool_call_intent:
  tool_name: "<single BlueprintHelper tool this SideAgent should execute>"
  missing_field_reason: "<why Main Agent cannot answer from accumulated SideAgent results>"
references_to_read:
  - "references/09_SideAgent_Tool_Execution.md"
  - "<workflow reference if needed>"
stop_conditions:
  - "missing target asset or create/modify strategy"
  - "Bridge unavailable"
  - "runtime_profile blocks write"
  - "preview blocked"
  - "write session rejected"
  - "tool unavailable"
return_format: "Chinese summary with tool names, key arguments, status, blockers, validation, and next step"
```

Example: if the user says "在蓝图实现一个可以开关的物理门" and does not name an asset, ask whether to modify an existing door Blueprint or create a new `BP_PhysicsDoor`. After that, delegate the actual BlueprintHelper tool work to a SideAgent.

## SideAgent Responsibility

Tell the SideAgent its responsibility in the task package:

- construct valid BlueprintHelper tool parameters from the user's goal and target;
- prefer copy-and-edit templates from `BlueprintHelper/Resources/AgentGuide/Templates/` for CLI JSON input;
- call only the assigned BlueprintHelper tool or the single atomic tool step explicitly requested by the Main Agent;
- do not expand the task into a broader investigation, repeat adjacent reads, or decide whether prior SideAgent context is sufficient;
- treat missing commands as `tool_unavailable`, a CLI installation or registration problem; do not request write session to fix read-command availability;
- do not replace unavailable BlueprintHelper CLI commands with shell reads, `.vs\BlueprintCache`, Saved exports, or ad hoc local JSON parsing; return the blocker to the Main Agent so it can repair CLI availability;
- estimate graph size before requesting full graph `logic_md`; function, event, and custom-event `logic_md` target-entry reads are allowed when `target_name` is known; if the graph has more than 80 nodes, use summary, structured anchors, or block-scoped reads instead of whole-graph text;
- run preview, write-session, execute, and result lookup only when the Main Agent assigned that tool step;
- treat an approved write session as a running Editor/Bridge permission, not a single-Agent secret; never request, pass, or reveal `auth_session`;
- translate the returned tool results into a concise Chinese result for the Main Agent;
- stop and return a blocker instead of asking the user directly.

The Main Agent uses that translated result to answer the user or decide the next clarification.

## Stop Conditions

Stop before write delegation when:

- the target asset or create strategy is unknown;
- the requested edit would modify user-owned nodes without explicit permission;
- the request needs a capability not listed in the onboarding index;
- the SideAgent reports `clarification_required`, `tool_unavailable`, `bridge_unavailable`, `profile_blocked`, `preview_blocked`, `capability_missing`, `write_rejected`, or `tool_failed`;
- the SideAgent result is not enough to judge whether the user's goal was satisfied.

## References

- `references/08_User_Preferences.md` - user preferences, collaboration rules, Debug/Review conventions
- `references/00_Agent_Onboarding_Index_20260504.md` - Agent-facing guide index
- `references/09_SideAgent_Tool_Execution.md` - SideAgent tool execution and result translation contract
- `references/01_Preflight_And_Boundary.md` - preflight and scope boundaries
- `references/02_TaskSpec_First_Tool_Selection.md` - TaskSpec-first tool selection
- `references/03_Runtime_Profile_And_Diagnostics.md` - runtime_profile and diagnostics
- `references/04_Tool_Surface_Field_Templates_20260512.md` - tool surface field templates
- `references/04_TaskSpec_Edit_Blueprint_Workflow.md` - TaskSpec Blueprint edit workflow
- `references/05_Edit_Blueprint_Workflow.md` - legacy Blueprint edit workflow
- `references/06_UMG_Data_Workflows.md` - UMG and data workflows
- `references/07_Safety_Validation_And_Recovery.md` - safety validation and recovery
- `BlueprintHelper/Resources/AgentGuide/Templates/README.md` - copy-and-edit CLI JSON templates
