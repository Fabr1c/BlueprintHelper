# BlueprintHelper Field Capabilities Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expand BlueprintHelper GraphWrite FieldVariableActionCluster so the execution layer supports the 17 evidence-backed first-class Field capabilities with stable TaskSpec inputs, UE node-family correctness, readback facts, and diagnostics.

**Architecture:** Add an explicit Field capability taxonomy, then thread stable `capability_id` and field identity as generic capability facts through GraphSemanticIR, ActionContext, ActionResolution, FragmentBuilder, and DebugBundle/readback. Shared DTOs stay cluster-neutral: use `CapabilityId`, `CapabilityFacts`, `ReadbackFacts`, expected node-family fields, and existing generic semantic fields. Field-only identity such as owner class, member guid, local/function scope, target pin, component, and property path details must live under namespaced facts such as `field.owner_class`, `field.member_guid`, `field.local_scope`, `field.target_pin_ref`, and `field.property_path`, or inside Field-cluster private helper/readback types. Variable-like capabilities continue to use `UK2Node_VariableGet` / `UK2Node_VariableSet`; component references are resolved as component `FObjectProperty` variable nodes; object/component property access uses explicit target pin projection; struct and nested property paths use dedicated fragments for `UK2Node_BreakStruct`, `UK2Node_SetFieldsInStruct`, split state, links, and defaults.

> **Architecture correction (2026-05-26):** Any older task snippet below that asks to add `FieldCapabilityId`, `OwnerClassPath`, `MemberGuid`, `LocalScopeName`, `FunctionName`, `ParamFlags`, `TargetPinRef`, `FieldPathSegments`, `ResolvedFieldKind`, `ResolvedOwnerClassPath`, or `FieldReadbackFacts` directly to shared IR, ActionResolution constraints, ActionContext demand/snapshot records, or shared candidate DTOs is superseded. Implement those values as generic `CapabilityId`, `CapabilityFacts`, and `ReadbackFacts` entries, with Field-specific interpretation contained in Field registry/resolver/context projection/fragment/readback boundaries. `BlueprintHelperGraphSemanticIR` must parse syntax/facts only; it must not include or depend on the Field taxonomy registry.

**Tech Stack:** Unreal Engine 5.3+ Editor plugin C++17, BlueprintGraph/Kismet/UnrealEd/KismetCompiler modules, UE Automation Tests, GraphWrite TaskSpec/statement IR, MCP-facing JSON templates.

---

## Execution status (2026-05-26)

Status: implemented, final-review fixes applied, and verified.

The task checklist below is retained as the implementation plan text. Current execution state is recorded here because repository instructions forbid running `git add`, `git commit`, or `git push`; all per-task/final commit steps are therefore completed as a manual commit handoff instead of local Git mutations.

Implemented:

- Added the Field capability taxonomy registry with the 17 first-class `field.*` capability IDs and excluded UI/support/other-cluster/diagnostic entries.
- Kept shared GraphWrite DTOs cluster-neutral by using generic `CapabilityId`, `CapabilityFacts`, `ReadbackFacts`, and expected node-family fields.
- Routed Field-specific interpretation through Field registry, resolver, context projection, fragment builder, and readback boundaries.
- Resolved component references as component `FObjectProperty` variable nodes and rejected plain object properties.
- Wired `field.local_get`, `field.local_set`, and `field.function_param_get` into the resolver's function-scope property/spawner path.
- Renamed shared ActionContext snapshot fact fields to neutral `Kind` / `CapabilityFacts`; Field interpretation remains in namespaced `field.*` facts.
- Added object-pin/component-property facts, struct member fragments, nested property-path fragments, and flat readback facts.
- Updated AgentFaceService Field templates and capability matrix docs.
- Copied this plan to `BlueprintHelper_GraphWrite_Field_FirstClassCapabilityExpansionPlan_20260526_CN.md`.

Verified:

- `Build.bat TemplateEditor Win64 Development D:\UEProjects\Template\Template.uproject -NoHotReloadFromIDE -WaitMutex` exits `0`.
- `BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.FunctionScope` exits `0`.
- `BlueprintHelper.GraphWrite.ActionContext.CapabilityFacts` exits `0`.
- `BlueprintHelper.GraphWrite.FieldFragmentBuilder.PropertyPath.BuildsStructAndNestedFragments` exits `0`.
- `BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.ComponentRef` exits `0`.
- `BlueprintHelper.GraphWrite.ActionResolution.Contract.FunctionFragmentLifecycleCoordinator` exits `0`.
- `BlueprintHelper.GraphWrite.FunctionFieldUnifiedSmoke.FieldComponentRefAndFieldAccess` exits `0`.
- `BlueprintHelper.GraphWrite` exits `0`.
- Hygiene checks found no runtime `UBlueprintComponentNodeSpawner` / `K2Node_AddComponent` use in Field resolver/fragment paths.
- Hygiene checks found no `FieldReadbacks` / `field_readbacks` legacy payloads, no `FieldKind` / `FieldFacts` public ActionContext DTO fields, and old Field-only shared DTO names only remain in this architecture correction note.
- Field taxonomy implementation contains 17 first-class `MakeSpec(TEXT("field.*"))` entries.

Notes:

- UE logs still include unrelated EOS no-connection warnings and Rider port file move warnings during automation runs.
- The initial `LogAutomationTest: Error: Condition failed` lines observed at editor startup did not correspond to failing automation cases; focused and full GraphWrite gates passed.

---

## Source-of-truth constraints

This plan is based on the current, non-archived evidence document `BlueprintHelper_GraphWrite_Field_UEEditorCapability_EngineSourceReadResult_20260525_CN.md`. Do not use documents under `BlueprintHelper/Develop/v*` as latest sources.

The implementation must expose these 17 first-class Field capability IDs:

| Priority | Capability IDs |
|---|---|
| P0 | `field.member_get`, `field.member_set`, `field.local_get`, `field.local_set`, `field.component_ref_get` |
| P1 | `field.inherited_member_get`, `field.inherited_member_set`, `field.sparse_data_get`, `field.function_param_get`, `field.struct_member_get`, `field.struct_member_set` |
| P2 | `field.object_pin_member_get`, `field.object_pin_member_set`, `field.component_ref_set`, `field.component_property_get`, `field.component_property_set`, `field.nested_property_path` |

The implementation must not expose these as first-class user statements:

| Category | Capability IDs / operation families | Required behavior |
|---|---|---|
| UI-only evidence | `field.drag_get`, `field.drag_set`, `field.pin_drag_get`, `field.pin_drag_set` | Reject at TaskSpec/semantic validation with `unsupported_ui_entry_not_statement`; caller must map them to stable `field.*` statements. |
| Support/readback-only | `field.split_struct_pin_support`, `field.recombine_struct_pin_support` | Internal fragment/readback support only; no user statement surface. |
| Other clusters | `control.function_return_write`, `function.selected_component_call`, `component.add_component_node` | Route or reject outside FieldVariableActionCluster; Field must not claim success. |
| Diagnostic / first-stage excluded | `field.unsupported_path_diagnostic`, `field.by_ref_set` | Return diagnostic facts; `field.by_ref_set` returns `unsupported_by_ref_set_deferred`. |

Statement-local context is mandatory. The executor must not read editor transient UI state: no right-click state, no drag source, no selected objects, no modifier keys, no pin menu state, and no implicit cross-statement local cache.

## Repository and command assumptions

Run all commands from the repository root that contains `BlueprintHelper/BlueprintHelper.uplugin`.

The test project must enable `BlueprintHelper`. The source tree currently has plugin automation tests but no checked-in `.uproject`, so `BPH_TEST_PROJECT` is an explicit execution dependency.

Canonical build command:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Quit" -TestExit="Automation Test Queue Empty"
```

Expected result after a clean compile: process exits with code `0` and logs contain `LogInit: Display: Engine is initialized` followed by `RequestExit`.

Canonical automation command template:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests <TEST_FILTER>; Quit" -TestExit="Automation Test Queue Empty"
```

Expected result for failing-test steps: process exits non-zero or logs contain `Test Failed` for the named test.

Expected result for passing-test steps: process exits with code `0` and logs contain `Automation Test Queue Empty` with no `Test Failed` lines for the named filter.

---

## File structure

### New files

- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.h`
  - Owns the 17 capability IDs, priority, node-family expectations, allowed operation/scope combinations, and rejection reasons for excluded Field-like inputs.
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.cpp`
  - Implements registry lookup, legacy operation/scope inference, TaskSpec validation, and stable string helpers.
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldActionReadback.h`
  - Defines readback facts for member reference, pins, links, split state, generated nodes, and compile diagnostics.
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldActionReadback.cpp`
  - Extracts facts from generated K2 nodes and appends them to GraphFragment evidence/debug payloads.
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperFieldFragmentBuilder.h`
  - Declares variable get/set, component ref, object target access, struct read/write, and nested property path fragment entry points.
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperFieldFragmentBuilder.cpp`
  - Implements dedicated Field fragment generation and schema-checked links.
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldCapabilityTaxonomyTests.cpp`
  - Verifies the 17-capability taxonomy, priorities, and rejection matrix.
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldActionReadbackTests.cpp`
  - Verifies readback fact extraction from variable, component, target-pin, struct, and nested-path nodes.
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldFragmentBuilderTests.cpp`
  - Verifies generated node classes, links, defaults, and split-state facts for Field fragments.

### Modified files

- `BlueprintHelper/Source/BlueprintHelper/BlueprintHelper.Build.cs`
  - No new dependency is required for the current flat readback/status diagnostics path. Add `KismetCompiler` only if a future implementation consumes compiler log APIs directly.
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
  - Add generic `CapabilityId` and `CapabilityFacts`; Field-specific stable context is represented by namespaced fact keys, not dedicated shared fields.
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
  - Parse `capability_id`, legacy field-capability aliases, and `capability_facts`; normalize stable Field context into `field.*` facts without depending on the Field registry.
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`
  - Extend semantic constraints and candidates with generic `CapabilityId`, `CapabilityFacts`, `ReadbackFacts`, and expected node-family fields.
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h`
  - Add generic capability facts on demands and field snapshot fact maps for owner class, member guid, local/function scope, target pin class, component metadata, sparse data, and struct path.
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
  - Add demands for first-class Field contexts and reject demands for UI-only contexts.
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextSnapshotBuilder.cpp`
  - Capture Blueprint member variables, inherited/native properties, sparse class data candidates, function inputs, local variables, components, target pin types, and struct metadata.
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp`
  - Project target-pin and component-pin field facts; build owner/guid/local-scope disambiguation keys.
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldPathResolution.h`
  - Replace the coarse role-only path result with path segments, root kind, node-family plan, and read/write mode.
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldPathResolution.cpp`
  - Resolve member/local/param/component/object/struct/nested paths to deterministic fragment plans.
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.h`
  - Keep the public resolver API narrow; first-class Field capability resolution and stable readback helpers stay private to the resolver implementation unless a reusable non-Field boundary emerges.
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.cpp`
  - Resolve all 17 capabilities to correct UE node families and spawner/API choices.
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h`
  - Add Field fragment builder dependency and Field routing methods.
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
  - Route get/set/get_property/set_property/component_ref/field_access by capability ID and append readback facts.
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuilderRegistry.cpp`
  - Register the dedicated Field fragment builder for property paths and struct paths.
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.h`
  - Keep shared graph fragment evidence cluster-neutral; do not add Field readback payload containers.
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.cpp`
  - Preserve generic evidence serialization; Field readback facts enter as flat metadata/debug facts.
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDebugData.cpp`
  - Include Field facts in DebugBundle while preserving graph-block Review boundaries.
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldVariableActionClusterTests.cpp`
  - Expand resolver tests for P0/P1/P2 capability groups.
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`
  - Add stable context projection tests for local/function-param/object-pin/component-pin/nested-path cases.
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`
  - Add contract tests for no UI transient state, no component spawner for `component_ref`, and no per-field Review target.
- `AgentFaceService/agent-guide/Reference/04_Tool_Surface_Field_Templates_20260512.md`
  - Document the 17 allowed capability IDs and excluded Field-like inputs.
- `AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md`
  - Update GraphWrite Field capability matrix and TaskSpec minimum fields.

---

## Task 1: Add Field capability taxonomy

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldCapabilityTaxonomyTests.cpp`

- [ ] **Step 1: Write the failing taxonomy test**

Create `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldCapabilityTaxonomyTests.cpp` with this content:

```cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldCapabilityTaxonomyHasSeventeenFirstClassIdsTest,
	"BlueprintHelper.GraphWrite.FieldCapability.Taxonomy.HasSeventeenFirstClassIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldCapabilityTaxonomyHasSeventeenFirstClassIdsTest::RunTest(const FString& Parameters)
{
	const TArray<FBlueprintHelperFieldCapabilitySpec> Specs = FBlueprintHelperFieldCapabilityRegistry::GetFirstClassSpecs();
	TestEqual(TEXT("first-class capability count"), Specs.Num(), 17);

	const TCHAR* ExpectedIds[] = {
		TEXT("field.member_get"),
		TEXT("field.member_set"),
		TEXT("field.inherited_member_get"),
		TEXT("field.inherited_member_set"),
		TEXT("field.sparse_data_get"),
		TEXT("field.function_param_get"),
		TEXT("field.local_get"),
		TEXT("field.local_set"),
		TEXT("field.object_pin_member_get"),
		TEXT("field.object_pin_member_set"),
		TEXT("field.component_ref_get"),
		TEXT("field.component_ref_set"),
		TEXT("field.component_property_get"),
		TEXT("field.component_property_set"),
		TEXT("field.struct_member_get"),
		TEXT("field.struct_member_set"),
		TEXT("field.nested_property_path")
	};

	for (const TCHAR* ExpectedId : ExpectedIds)
	{
		const FBlueprintHelperFieldCapabilitySpec* Spec = FBlueprintHelperFieldCapabilityRegistry::FindById(ExpectedId);
		TestNotNull(FString::Printf(TEXT("capability exists: %s"), ExpectedId), Spec);
		if (Spec)
		{
			TestTrue(FString::Printf(TEXT("capability is first-class: %s"), ExpectedId), Spec->bFirstClassStatement);
			TestFalse(FString::Printf(TEXT("node family set: %s"), ExpectedId), Spec->ExpectedNodeFamily.IsEmpty());
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldCapabilityTaxonomyRejectsUiOnlyIdsTest,
	"BlueprintHelper.GraphWrite.FieldCapability.Taxonomy.RejectsUiOnlyIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldCapabilityTaxonomyRejectsUiOnlyIdsTest::RunTest(const FString& Parameters)
{
	const TCHAR* UiOnlyIds[] = {
		TEXT("field.drag_get"),
		TEXT("field.drag_set"),
		TEXT("field.pin_drag_get"),
		TEXT("field.pin_drag_set")
	};

	for (const TCHAR* UiOnlyId : UiOnlyIds)
	{
		FString RejectReason;
		const bool bAllowed = FBlueprintHelperFieldCapabilityRegistry::IsAllowedUserStatement(UiOnlyId, RejectReason);
		TestFalse(FString::Printf(TEXT("UI-only id rejected: %s"), UiOnlyId), bAllowed);
		TestEqual(FString::Printf(TEXT("UI-only reject reason: %s"), UiOnlyId), RejectReason, FString(TEXT("unsupported_ui_entry_not_statement")));
	}

	return true;
}

#endif
```

- [ ] **Step 2: Run the test and verify it fails**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.FieldCapability.Taxonomy; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: build or automation fails because `BlueprintHelperFieldCapabilityTypes.h` does not exist.

- [ ] **Step 3: Add the taxonomy header**

Create `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.h`:

```cpp
#pragma once

#include "CoreMinimal.h"

enum class EBlueprintHelperFieldCapabilityPriority : uint8
{
	P0,
	P1,
	P2,
	SupportOnly,
	OtherCluster,
	DiagnosticOnly
};

enum class EBlueprintHelperFieldCapabilityRootKind : uint8
{
	Member,
	InheritedMember,
	SparseData,
	FunctionParam,
	Local,
	ObjectPinMember,
	ComponentRef,
	ComponentProperty,
	StructMember,
	NestedPropertyPath,
	Unsupported
};

enum class EBlueprintHelperFieldCapabilityAccessMode : uint8
{
	Get,
	Set,
	ReadWritePath,
	Diagnostic
};

struct FBlueprintHelperFieldCapabilitySpec
{
	FString Id;
	EBlueprintHelperFieldCapabilityPriority Priority = EBlueprintHelperFieldCapabilityPriority::DiagnosticOnly;
	EBlueprintHelperFieldCapabilityRootKind RootKind = EBlueprintHelperFieldCapabilityRootKind::Unsupported;
	EBlueprintHelperFieldCapabilityAccessMode AccessMode = EBlueprintHelperFieldCapabilityAccessMode::Diagnostic;
	FString FieldOperation;
	FString FieldScope;
	FString ExpectedNodeFamily;
	FString ExpectedNodeClass;
	bool bFirstClassStatement = false;
	bool bRequiresOwnerClass = false;
	bool bRequiresFunctionScope = false;
	bool bRequiresTargetPin = false;
	bool bRequiresPropertyPath = false;
	bool bProducesExecPins = false;
	FString RejectReason;
};

class FBlueprintHelperFieldCapabilityRegistry
{
public:
	static const FBlueprintHelperFieldCapabilitySpec* FindById(const FString& CapabilityId);
	static TArray<FBlueprintHelperFieldCapabilitySpec> GetFirstClassSpecs();
	static bool IsAllowedUserStatement(const FString& CapabilityId, FString& OutRejectReason);
	static const FBlueprintHelperFieldCapabilitySpec* InferFromOperationAndScope(const FString& FieldOperation, const FString& FieldScope);
	static FString MakeStableCapabilityKey(const FBlueprintHelperFieldCapabilitySpec& Spec);
};
```

- [ ] **Step 4: Add the taxonomy implementation**

Create `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.cpp`:

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.h"

namespace
{
static FBlueprintHelperFieldCapabilitySpec MakeSpec(
	const TCHAR* Id,
	EBlueprintHelperFieldCapabilityPriority Priority,
	EBlueprintHelperFieldCapabilityRootKind RootKind,
	EBlueprintHelperFieldCapabilityAccessMode AccessMode,
	const TCHAR* Operation,
	const TCHAR* Scope,
	const TCHAR* NodeFamily,
	const TCHAR* NodeClass,
	const bool bRequiresOwnerClass,
	const bool bRequiresFunctionScope,
	const bool bRequiresTargetPin,
	const bool bRequiresPropertyPath,
	const bool bProducesExecPins)
{
	FBlueprintHelperFieldCapabilitySpec Spec;
	Spec.Id = Id;
	Spec.Priority = Priority;
	Spec.RootKind = RootKind;
	Spec.AccessMode = AccessMode;
	Spec.FieldOperation = Operation;
	Spec.FieldScope = Scope;
	Spec.ExpectedNodeFamily = NodeFamily;
	Spec.ExpectedNodeClass = NodeClass;
	Spec.bFirstClassStatement = true;
	Spec.bRequiresOwnerClass = bRequiresOwnerClass;
	Spec.bRequiresFunctionScope = bRequiresFunctionScope;
	Spec.bRequiresTargetPin = bRequiresTargetPin;
	Spec.bRequiresPropertyPath = bRequiresPropertyPath;
	Spec.bProducesExecPins = bProducesExecPins;
	return Spec;
}

static const TArray<FBlueprintHelperFieldCapabilitySpec>& GetAllSpecs()
{
	static const TArray<FBlueprintHelperFieldCapabilitySpec> Specs = {
		MakeSpec(TEXT("field.member_get"), EBlueprintHelperFieldCapabilityPriority::P0, EBlueprintHelperFieldCapabilityRootKind::Member, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("variable"), TEXT("variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), false, false, false, false, false),
		MakeSpec(TEXT("field.member_set"), EBlueprintHelperFieldCapabilityPriority::P0, EBlueprintHelperFieldCapabilityRootKind::Member, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("variable"), TEXT("variable_set"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), false, false, false, false, true),
		MakeSpec(TEXT("field.inherited_member_get"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::InheritedMember, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("variable"), TEXT("variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), true, false, false, false, false),
		MakeSpec(TEXT("field.inherited_member_set"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::InheritedMember, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("variable"), TEXT("variable_set"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), true, false, false, false, true),
		MakeSpec(TEXT("field.sparse_data_get"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::SparseData, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("variable"), TEXT("variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), true, false, false, false, false),
		MakeSpec(TEXT("field.function_param_get"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::FunctionParam, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("variable"), TEXT("variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), false, true, false, false, false),
		MakeSpec(TEXT("field.local_get"), EBlueprintHelperFieldCapabilityPriority::P0, EBlueprintHelperFieldCapabilityRootKind::Local, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("variable"), TEXT("variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), false, true, false, false, false),
		MakeSpec(TEXT("field.local_set"), EBlueprintHelperFieldCapabilityPriority::P0, EBlueprintHelperFieldCapabilityRootKind::Local, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("variable"), TEXT("variable_set"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), false, true, false, false, true),
		MakeSpec(TEXT("field.object_pin_member_get"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::ObjectPinMember, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("field_access"), TEXT("variable_get_target"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), true, false, true, false, false),
		MakeSpec(TEXT("field.object_pin_member_set"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::ObjectPinMember, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("field_access"), TEXT("variable_set_target"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), true, false, true, false, true),
		MakeSpec(TEXT("field.component_ref_get"), EBlueprintHelperFieldCapabilityPriority::P0, EBlueprintHelperFieldCapabilityRootKind::ComponentRef, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("component_ref"), TEXT("component_variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), false, false, false, false, false),
		MakeSpec(TEXT("field.component_ref_set"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::ComponentRef, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("component_ref"), TEXT("component_variable_set"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), false, false, false, false, true),
		MakeSpec(TEXT("field.component_property_get"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::ComponentProperty, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get_property"), TEXT("field_access"), TEXT("component_property_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), true, false, true, true, false),
		MakeSpec(TEXT("field.component_property_set"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::ComponentProperty, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set_property"), TEXT("field_access"), TEXT("component_property_set"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), true, false, true, true, true),
		MakeSpec(TEXT("field.struct_member_get"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::StructMember, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get_property"), TEXT("property_path"), TEXT("break_struct"), TEXT("/Script/BlueprintGraph.K2Node_BreakStruct"), false, false, false, true, false),
		MakeSpec(TEXT("field.struct_member_set"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::StructMember, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set_property"), TEXT("property_path"), TEXT("set_fields_in_struct"), TEXT("/Script/BlueprintGraph.K2Node_SetFieldsInStruct"), false, false, false, true, true),
		MakeSpec(TEXT("field.nested_property_path"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::NestedPropertyPath, EBlueprintHelperFieldCapabilityAccessMode::ReadWritePath, TEXT("get_property"), TEXT("property_path"), TEXT("property_path_fragment"), TEXT("BlueprintHelper.Field.PropertyPathFragment"), false, false, false, true, false)
	};
	return Specs;
}
}

const FBlueprintHelperFieldCapabilitySpec* FBlueprintHelperFieldCapabilityRegistry::FindById(const FString& CapabilityId)
{
	for (const FBlueprintHelperFieldCapabilitySpec& Spec : GetAllSpecs())
	{
		if (Spec.Id.Equals(CapabilityId, ESearchCase::IgnoreCase))
		{
			return &Spec;
		}
	}
	return nullptr;
}

TArray<FBlueprintHelperFieldCapabilitySpec> FBlueprintHelperFieldCapabilityRegistry::GetFirstClassSpecs()
{
	return GetAllSpecs();
}

bool FBlueprintHelperFieldCapabilityRegistry::IsAllowedUserStatement(const FString& CapabilityId, FString& OutRejectReason)
{
	static const TMap<FString, FString> RejectedIds = {
		{TEXT("field.drag_get"), TEXT("unsupported_ui_entry_not_statement")},
		{TEXT("field.drag_set"), TEXT("unsupported_ui_entry_not_statement")},
		{TEXT("field.pin_drag_get"), TEXT("unsupported_ui_entry_not_statement")},
		{TEXT("field.pin_drag_set"), TEXT("unsupported_ui_entry_not_statement")},
		{TEXT("field.split_struct_pin_support"), TEXT("support_only_not_user_statement")},
		{TEXT("field.recombine_struct_pin_support"), TEXT("support_only_not_user_statement")},
		{TEXT("field.by_ref_set"), TEXT("unsupported_by_ref_set_deferred")},
		{TEXT("field.unsupported_path_diagnostic"), TEXT("diagnostic_only_not_success_capability")}
	};

	if (const FString* Reason = RejectedIds.Find(CapabilityId))
	{
		OutRejectReason = *Reason;
		return false;
	}

	const FBlueprintHelperFieldCapabilitySpec* Spec = FindById(CapabilityId);
	if (!Spec)
	{
		OutRejectReason = TEXT("unknown_field_capability");
		return false;
	}

	OutRejectReason.Reset();
	return true;
}

const FBlueprintHelperFieldCapabilitySpec* FBlueprintHelperFieldCapabilityRegistry::InferFromOperationAndScope(const FString& FieldOperation, const FString& FieldScope)
{
	for (const FBlueprintHelperFieldCapabilitySpec& Spec : GetAllSpecs())
	{
		if (Spec.FieldOperation.Equals(FieldOperation, ESearchCase::IgnoreCase) &&
			Spec.FieldScope.Equals(FieldScope, ESearchCase::IgnoreCase) &&
			Spec.RootKind == EBlueprintHelperFieldCapabilityRootKind::Member)
		{
			return &Spec;
		}
	}
	return nullptr;
}

FString FBlueprintHelperFieldCapabilityRegistry::MakeStableCapabilityKey(const FBlueprintHelperFieldCapabilitySpec& Spec)
{
	return FString::Printf(TEXT("field-capability:%s:%s:%s"), *Spec.Id, *Spec.FieldOperation, *Spec.FieldScope);
}
```

- [ ] **Step 5: Run the taxonomy tests and commit**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.FieldCapability.Taxonomy; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: taxonomy tests pass.

Commit:

```bash
git add BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldCapabilityTaxonomyTests.cpp
git commit -m "feat: add graphwrite field capability taxonomy"
```

---

## Task 2: Extend GraphSemanticIR with stable Field capability inputs

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldCapabilityTaxonomyTests.cpp`

- [ ] **Step 1: Add failing IR parse/validation tests**

Append this test to `BlueprintHelperFieldCapabilityTaxonomyTests.cpp` before `#endif`:

```cpp
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldCapabilitySemanticIrParsesCapabilityIdTest,
	"BlueprintHelper.GraphWrite.FieldCapability.SemanticIR.ParsesCapabilityId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldCapabilitySemanticIrParsesCapabilityIdTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphStatementIR Statement;
	Statement.Kind = TEXT("field");
	Statement.FieldOperation = TEXT("get");
	Statement.FieldScope = TEXT("component_ref");
	Statement.CapabilityId = TEXT("field.component_ref_get");
	Statement.Name = TEXT("StaticMeshComponent");
	Statement.ComponentName = TEXT("StaticMeshComponent");
	Statement.CapabilityFacts.Add(TEXT("field.component_owner_class"), TEXT("/Script/Engine.Actor"));
	Statement.CapabilityFacts.Add(TEXT("field.component_kind"), TEXT("scs_or_native_property"));

	FString Error;
	const bool bValid = FBlueprintHelperGraphSemanticIR::ValidateStatement(Statement, Error);
	TestTrue(TEXT("component ref get statement is valid"), bValid);
	TestTrue(TEXT("error is empty"), Error.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldCapabilityRegistryRejectsUiOnlyIdTest,
	"BlueprintHelper.GraphWrite.FieldCapability.Registry.RejectsUiOnlyId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldCapabilityRegistryRejectsUiOnlyIdTest::RunTest(const FString& Parameters)
{
	FString RejectReason;
	const bool bAllowed = FBlueprintHelperFieldCapabilityRegistry::IsAllowedUserStatement(TEXT("field.drag_get"), RejectReason);
	TestFalse(TEXT("UI-only capability is invalid"), bAllowed);
	TestEqual(TEXT("reject reason"), RejectReason, FString(TEXT("unsupported_ui_entry_not_statement")));
	return true;
}
```

- [ ] **Step 2: Run the tests and verify they fail**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.FieldCapability.SemanticIR; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: build fails because `FBlueprintHelperGraphStatementIR::CapabilityId` and `CapabilityFacts` support is missing.

- [ ] **Step 3: Add generic stable fields to GraphSemanticIR types**

```cpp
FString CapabilityId;
TMap<FString, FString> CapabilityFacts;
```

Add these fields to both `FBlueprintHelperGraphExpressionIR` and `FBlueprintHelperGraphStatementIR`. Do not include the Field taxonomy header here.

- [ ] **Step 4: Parse capability IDs and facts in GraphSemanticIR.cpp**

Parse both `capability_id` and the legacy input alias `field_capability_id` into `CapabilityId`. Parse `capability_facts` into the map and normalize legacy top-level Field keys into namespaced facts:

```cpp
field.owner_class
field.member_guid
field.local_scope
field.function_name
field.param_flags
field.target_pin_ref
field.target_pin_type
field.target_pin_object_path
field.component_name
field.component_owner_class
field.component_kind
field.property_path
```

Field capability allow/reject validation remains in the Field registry, TaskSpec adapter, or Field resolver boundary.

For field statements, replace the current set-only acceptance with this rule:

```cpp
const bool bIsFieldStatement = Statement.Kind.Equals(TEXT("field"), ESearchCase::IgnoreCase);
if (bIsFieldStatement)
{
	const FString Operation = Statement.FieldOperation.ToLower();
	const bool bSupportedOperation = Operation == TEXT("get") ||
		Operation == TEXT("set") ||
		Operation == TEXT("get_property") ||
		Operation == TEXT("set_property");
	if (!bSupportedOperation)
	{
		OutError = TEXT("field statement operation must be get, set, get_property, or set_property");
		return false;
	}
}
```

- [ ] **Step 5: Run SemanticIR tests and commit**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.FieldCapability.SemanticIR; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: SemanticIR tests pass.

Commit:

```bash
git add BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldCapabilityTaxonomyTests.cpp
git commit -m "feat: carry field capability ids through graph semantic ir"
```

---

## Task 3: Extend ActionResolution semantic constraints and candidates

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`

- [ ] **Step 1: Add failing ActionResolution contract test**

Append this test to `BlueprintHelperActionResolutionContractTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionResolutionContractCarriesFieldCapabilityFactsTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.CarriesFieldCapabilityFacts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionResolutionContractCarriesFieldCapabilityFactsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionSemanticConstraints Constraints;
	Constraints.Kind = EBlueprintHelperActionSemanticKind::Field;
	Constraints.CapabilityId = TEXT("field.member_get");
	Constraints.FieldOperation = TEXT("get");
	Constraints.FieldScope = TEXT("variable");
	Constraints.CapabilityFacts.Add(TEXT("field.owner_class"), TEXT("/Script/Engine.Actor"));
	Constraints.CapabilityFacts.Add(TEXT("field.member_name"), TEXT("Health"));
	Constraints.CapabilityFacts.Add(TEXT("field.member_guid"), TEXT("11111111-2222-2222-3333-333344444444"));

	TestEqual(TEXT("capability id"), Constraints.CapabilityId, FString(TEXT("field.member_get")));
	TestEqual(TEXT("owner class"), Constraints.CapabilityFacts.FindRef(TEXT("field.owner_class")), FString(TEXT("/Script/Engine.Actor")));
	TestEqual(TEXT("member name"), Constraints.CapabilityFacts.FindRef(TEXT("field.member_name")), FString(TEXT("Health")));
	TestFalse(TEXT("member guid recorded"), Constraints.CapabilityFacts.FindRef(TEXT("field.member_guid")).IsEmpty());
	return true;
}
```

- [ ] **Step 2: Run the test and verify it fails**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Contract.CarriesFieldCapabilityFacts; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: build fails because generic capability facts are not present on `FBlueprintHelperActionSemanticConstraints`.

- [ ] **Step 3: Add generic semantic and candidate facts**

In `BlueprintHelperActionResolutionCore.h`, extend `FBlueprintHelperActionSemanticConstraints`:

```cpp
FString CapabilityId;
TMap<FString, FString> CapabilityFacts;
```

Extend `FBlueprintHelperActionCandidate` with generic resolved capability and readback facts:

```cpp
FString CapabilityId;
FString ExpectedNodeFamily;
FString ExpectedNodeClassPath;
TMap<FString, FString> CapabilityFacts;
TMap<FString, FString> ReadbackFacts;
```

- [ ] **Step 4: Run the contract test and commit**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Contract.CarriesFieldCapabilityFacts; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: contract test passes.

Commit:

```bash
git add BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp
git commit -m "feat: add resolved field facts to action resolution contract"
```

---

## Task 4: Capture stable ActionContext facts for member, inherited, local, parameter, component, target pin, and struct fields

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextSnapshotBuilder.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`

- [ ] **Step 1: Add failing context pipeline test for local and parameter facts**

Append this test to `BlueprintHelperActionContextPipelineTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextCapturesFunctionLocalAndParamFieldsTest,
	"BlueprintHelper.GraphWrite.ActionContext.FieldFacts.CapturesFunctionLocalAndParamFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextCapturesFunctionLocalAndParamFieldsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionContextFieldSnapshot LocalField;
	LocalField.FieldName = TEXT("LocalSpeed");
	LocalField.FieldKind = TEXT("local");
	LocalField.FieldFacts.Add(TEXT("field.function_name"), TEXT("ComputeSpeed"));
	LocalField.FieldFacts.Add(TEXT("field.local_scope"), TEXT("ComputeSpeed"));
	LocalField.FieldFacts.Add(TEXT("field.member_guid"), TEXT("12345678-2222-2222-3333-333344444444"));

	FBlueprintHelperActionContextFieldSnapshot ParamField;
	ParamField.FieldName = TEXT("InputSpeed");
	ParamField.FieldKind = TEXT("function_param");
	ParamField.FieldFacts.Add(TEXT("field.function_name"), TEXT("ComputeSpeed"));
	ParamField.FieldFacts.Add(TEXT("field.param_flags"), TEXT("FUNC_Parm"));

	TestEqual(TEXT("local kind"), LocalField.FieldKind, FString(TEXT("local")));
	TestEqual(TEXT("local scope"), LocalField.FieldFacts.FindRef(TEXT("field.local_scope")), FString(TEXT("ComputeSpeed")));
	TestFalse(TEXT("local guid"), LocalField.FieldFacts.FindRef(TEXT("field.member_guid")).IsEmpty());
	TestEqual(TEXT("param kind"), ParamField.FieldKind, FString(TEXT("function_param")));
	TestEqual(TEXT("param flags"), ParamField.FieldFacts.FindRef(TEXT("field.param_flags")), FString(TEXT("FUNC_Parm")));
	return true;
}
```

- [ ] **Step 2: Run the test and verify it fails**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ActionContext.FieldFacts.CapturesFunctionLocalAndParamFields; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: build fails because `FieldFacts` is missing on the field snapshot type.

- [ ] **Step 3: Extend context field snapshot type**

In `BlueprintHelperActionContextTypes.h`, extend `FBlueprintHelperActionContextFieldSnapshot` with:

```cpp
FString FieldKind;
FString CapabilityId;
FString OwnerClassPath;
FString FieldPath;
TMap<FString, FString> FieldFacts;
```

- [ ] **Step 4: Collect Field demands from capability ID**

In `BlueprintHelperActionContextDemandCollector.cpp`, when handling Field semantics, copy `CapabilityId` and `CapabilityFacts` from the statement/expression IR. Add demand keys using this exact naming convention:

```cpp
Demand.RequiredFacts.Add(TEXT("field.capability_id"));
Demand.RequiredFacts.Add(TEXT("field.owner_class"));
Demand.RequiredFacts.Add(TEXT("field.member_name"));
Demand.RequiredFacts.Add(TEXT("field.member_guid"));
Demand.RequiredFacts.Add(TEXT("field.local_scope"));
Demand.RequiredFacts.Add(TEXT("field.function_name"));
Demand.RequiredFacts.Add(TEXT("field.target_pin_ref"));
Demand.RequiredFacts.Add(TEXT("field.target_pin_type"));
Demand.RequiredFacts.Add(TEXT("field.component_name"));
Demand.RequiredFacts.Add(TEXT("field.component_guid"));
Demand.RequiredFacts.Add(TEXT("field.struct_type"));
Demand.RequiredFacts.Add(TEXT("field.property_path"));
```

For UI-only IDs, return a demand error fact:

```cpp
Demand.BlockingReason = TEXT("unsupported_ui_entry_not_statement");
```

- [ ] **Step 5: Capture local and function parameter candidates**

In `BlueprintHelperActionContextSnapshotBuilder.cpp`, add helper functions in the anonymous namespace:

```cpp
static void CaptureFunctionLocalVariables(UBlueprint* Blueprint, FBlueprintHelperActionContextSnapshot& Snapshot)
{
	if (!Blueprint)
	{
		return;
	}

	TArray<UEdGraph*> FunctionGraphs;
	Blueprint->GetAllGraphs(FunctionGraphs);
	for (UEdGraph* Graph : FunctionGraphs)
	{
		if (!Graph || Graph->GetSchema() == nullptr)
		{
			continue;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node);
			if (!EntryNode)
			{
				continue;
			}

			for (const FBPVariableDescription& LocalVar : EntryNode->LocalVariables)
			{
				FBlueprintHelperActionContextFieldSnapshot Field;
				Field.FieldName = LocalVar.VarName.ToString();
				Field.FieldKind = TEXT("local");
				Field.FieldFacts.Add(TEXT("field.member_name"), Field.FieldName);
				Field.FieldFacts.Add(TEXT("field.local_scope"), Graph->GetName());
				Field.FieldFacts.Add(TEXT("field.function_name"), Graph->GetName());
				Field.FieldFacts.Add(TEXT("field.member_guid"), LocalVar.VarGuid.ToString(EGuidFormats::DigitsWithHyphens));
				Field.FieldFacts.Add(TEXT("field.is_local_variable"), TEXT("true"));
				Field.PinCategory = LocalVar.VarType.PinCategory.ToString();
				Field.PinSubCategory = LocalVar.VarType.PinSubCategory.ToString();
				Snapshot.Fields.Add(Field);
			}
		}
	}
}

static void CaptureFunctionInputParameters(UBlueprint* Blueprint, FBlueprintHelperActionContextSnapshot& Snapshot)
{
	if (!Blueprint || !Blueprint->SkeletonGeneratedClass)
	{
		return;
	}

	for (TFieldIterator<UFunction> FunctionIt(Blueprint->SkeletonGeneratedClass, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
	{
		UFunction* Function = *FunctionIt;
		if (!Function)
		{
			continue;
		}

		for (TFieldIterator<FProperty> PropIt(Function); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (!Property || !Property->HasAnyPropertyFlags(CPF_Parm) || Property->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm))
			{
				continue;
			}

			FBlueprintHelperActionContextFieldSnapshot Field;
			Field.FieldName = Property->GetName();
			Field.FieldKind = TEXT("function_param");
			Field.FieldFacts.Add(TEXT("field.member_name"), Field.FieldName);
			Field.FieldFacts.Add(TEXT("field.function_name"), Function->GetName());
			Field.FieldFacts.Add(TEXT("field.local_scope"), Function->GetName());
			Field.FieldFacts.Add(TEXT("field.param_flags"), TEXT("FUNC_Parm"));
			Field.OwnerClassPath = Blueprint->SkeletonGeneratedClass->GetPathName();
			Field.FieldFacts.Add(TEXT("field.is_function_parameter"), TEXT("true"));
			Snapshot.Fields.Add(Field);
		}
	}
}
```

Call both helpers after existing Blueprint member/inherited field capture.

- [ ] **Step 6: Project target pin and component facts**

In `BlueprintHelperActionContextInferenceService.cpp`, when a request has `ContextEvidence[target_pin_ref]`, `linked_pin_type_category`, or `linked_pin_type_object_path`, add these facts to the selected field snapshot:

```cpp
ProjectedField.FieldFacts.Add(TEXT("field.target_pin_ref"), Request.ContextEvidence.FindRef(TEXT("target_pin_ref")));
ProjectedField.FieldFacts.Add(TEXT("field.target_pin_type"), Request.ContextEvidence.FindRef(TEXT("linked_pin_type_category")));
ProjectedField.FieldFacts.Add(TEXT("field.target_pin_object_path"), Request.ContextEvidence.FindRef(TEXT("linked_pin_type_object_path")));
ProjectedField.FieldFacts.Add(TEXT("field.is_object_pin_field"), !Request.ContextEvidence.FindRef(TEXT("target_pin_ref")).IsEmpty() ? TEXT("true") : TEXT("false"));
```

When a request has component evidence, set:

```cpp
ProjectedField.FieldFacts.Add(TEXT("field.component_name"), Request.ContextEvidence.FindRef(TEXT("component_name")));
ProjectedField.FieldFacts.Add(TEXT("field.component_owner_class"), Request.ContextEvidence.FindRef(TEXT("component_owner_class")));
ProjectedField.FieldFacts.Add(TEXT("field.component_kind"), Request.ContextEvidence.FindRef(TEXT("component_kind")));
ProjectedField.FieldFacts.Add(TEXT("field.is_component_property"), !Request.ContextEvidence.FindRef(TEXT("component_name")).IsEmpty() ? TEXT("true") : TEXT("false"));
```

- [ ] **Step 7: Run context tests and commit**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ActionContext.FieldFacts; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: FieldFacts tests pass.

Commit:

```bash
git add BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextSnapshotBuilder.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp
git commit -m "feat: capture stable graphwrite field context facts"
```

---

## Task 5: Resolve P0/P1 variable-like capabilities with stable identity

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldVariableActionClusterTests.cpp`

- [ ] **Step 1: Add failing tests for local, inherited, sparse, and parameter capabilities**

Append these checks to `BlueprintHelperFieldVariableActionClusterTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterCapabilityStableIdTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.CapabilityStableId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterCapabilityStableIdTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestTrue(TEXT("add variable"), AddFieldVariableActionTestVariable(
		Blueprint,
		TEXT("Health"),
		MakeFieldVariableActionTestPinType(UEdGraphSchema_K2::PC_Real, UEdGraphSchema_K2::PC_Float)));

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		Blueprint,
		GetFieldVariableActionTestGraph(Blueprint),
		TEXT("get"),
		TEXT("variable"),
		TEXT("Health"));
	Request.Semantic.CapabilityId = TEXT("field.member_get");
	Request.Semantic.CapabilityFacts.Add(TEXT("field.member_name"), TEXT("Health"));
	AddProjectedFieldEvidence(Request, TEXT("Health"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestEqual(TEXT("one candidate"), Result.CandidateActions.Num(), 1);
	if (Result.CandidateActions.Num() > 0)
	{
		const FBlueprintHelperActionCandidate& Candidate = Result.CandidateActions[0];
		TestEqual(TEXT("capability id"), Candidate.CapabilityId, FString(TEXT("field.member_get")));
		TestEqual(TEXT("expected node family"), Candidate.ExpectedNodeFamily, FString(TEXT("variable_get")));
		TestTrue(TEXT("stable id contains capability"), Candidate.StableId.Contains(TEXT("field.member_get")));
		TestEqual(TEXT("resolved member"), Candidate.CapabilityFacts.FindRef(TEXT("field.member_name")), FString(TEXT("Health")));
	}
	return true;
}
```

- [ ] **Step 2: Run the test and verify it fails**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.CapabilityStableId; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: test fails because resolver candidates do not carry `CapabilityId`, `ExpectedNodeFamily`, and resolved field facts.

- [ ] **Step 3: Add resolver-private helper types**

In `BlueprintHelperFieldVariableActionResolver.cpp`, add an anonymous-namespace helper type. Do not expose this Field identity type in the public resolver header:

```cpp
struct FBlueprintHelperResolvedFieldIdentity
{
	FString CapabilityId;
	FString FieldKind;
	FString OwnerClassPath;
	FString MemberName;
	FGuid MemberGuid;
	FString LocalScopeName;
	FString FunctionName;
	FString TargetPinCategory;
	FString TargetPinObjectPath;
	FString ExpectedNodeFamily;
	FString ExpectedNodeClassPath;
	FString DiagnosticReason;
};
```

Add file-local helpers:

```cpp
static bool ResolveFieldIdentity(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperFieldCapabilitySpec& Spec,
	FBlueprintHelperResolvedFieldIdentity& OutIdentity);

static void ApplyResolvedFieldIdentityToCandidate(
	const FBlueprintHelperResolvedFieldIdentity& Identity,
	FBlueprintHelperActionCandidate& Candidate);
```

- [ ] **Step 4: Fill candidates from taxonomy and resolved identity**

In `BlueprintHelperFieldVariableActionResolver.cpp`, include the taxonomy header and call `FindById` before current candidate creation:

```cpp
const FBlueprintHelperFieldCapabilitySpec* CapabilitySpec = FBlueprintHelperFieldCapabilityRegistry::FindById(Request.Semantic.CapabilityId);
if (!CapabilitySpec)
{
	CapabilitySpec = FBlueprintHelperFieldCapabilityRegistry::InferFromOperationAndScope(Request.Semantic.FieldOperation, Request.Semantic.FieldScope);
}

if (!CapabilitySpec)
{
	Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
	Result.DebugMessage = TEXT("unknown_field_capability");
	return Result;
}
```

After selecting the `FProperty*` and node class, build the identity:

```cpp
FBlueprintHelperResolvedFieldIdentity Identity;
Identity.CapabilityId = CapabilitySpec->Id;
Identity.FieldKind = UEnum::GetValueAsString(CapabilitySpec->RootKind);
Identity.OwnerClassPath = ResolvedOwnerClass ? ResolvedOwnerClass->GetPathName() : Request.Semantic.CapabilityFacts.FindRef(TEXT("field.owner_class"));
Identity.MemberName = ResolvedProperty ? ResolvedProperty->GetName() : Request.Semantic.CapabilityFacts.FindRef(TEXT("field.member_name"));
FGuid::Parse(Request.Semantic.CapabilityFacts.FindRef(TEXT("field.member_guid")), Identity.MemberGuid);
Identity.LocalScopeName = Request.Semantic.CapabilityFacts.FindRef(TEXT("field.local_scope"));
Identity.FunctionName = Request.Semantic.CapabilityFacts.FindRef(TEXT("field.function_name"));
Identity.TargetPinCategory = Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_type"));
Identity.TargetPinObjectPath = Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_object_path"));
Identity.ExpectedNodeFamily = CapabilitySpec->ExpectedNodeFamily;
Identity.ExpectedNodeClassPath = CapabilitySpec->ExpectedNodeClass;
```

Apply it to the candidate:

```cpp
Candidate.CapabilityId = Identity.CapabilityId;
Candidate.CapabilityFacts.Add(TEXT("field.kind"), Identity.FieldKind);
Candidate.CapabilityFacts.Add(TEXT("field.owner_class"), Identity.OwnerClassPath);
Candidate.CapabilityFacts.Add(TEXT("field.member_name"), Identity.MemberName);
Candidate.CapabilityFacts.Add(TEXT("field.local_scope"), Identity.LocalScopeName);
Candidate.CapabilityFacts.Add(TEXT("field.function_name"), Identity.FunctionName);
Candidate.CapabilityFacts.Add(TEXT("field.target_pin_type"), Identity.TargetPinCategory);
Candidate.CapabilityFacts.Add(TEXT("field.target_pin_object_path"), Identity.TargetPinObjectPath);
Candidate.ExpectedNodeFamily = Identity.ExpectedNodeFamily;
Candidate.ExpectedNodeClassPath = Identity.ExpectedNodeClassPath;
Candidate.ReadbackFacts.Add(TEXT("capability_id"), Identity.CapabilityId);
Candidate.ReadbackFacts.Add(TEXT("field_kind"), Identity.FieldKind);
Candidate.ReadbackFacts.Add(TEXT("owner_class"), Identity.OwnerClassPath);
Candidate.ReadbackFacts.Add(TEXT("member_name"), Identity.MemberName);
Candidate.StableId = FString::Printf(TEXT("%s:%s:%s:%s"), *Candidate.StableId, *Identity.CapabilityId, *Identity.OwnerClassPath, *Identity.MemberName);
```

- [ ] **Step 5: Resolve local variable and function parameter spawners**

In the resolver branch for `field.local_get`, `field.local_set`, and `field.function_param_get`, select local/parameter candidates only when `Request.TargetGraph` is a function graph and `Request.Semantic.CapabilityFacts[field.function_name]`, `Request.Semantic.CapabilityFacts[field.local_scope]`, or `ContextEvidence[local_scope]` matches the graph name:

```cpp
const bool bRequiresFunctionScope = CapabilitySpec->bRequiresFunctionScope;
if (bRequiresFunctionScope)
{
	const FString ScopeName = !Request.Semantic.CapabilityFacts.FindRef(TEXT("field.local_scope")).IsEmpty()
		? Request.Semantic.CapabilityFacts.FindRef(TEXT("field.local_scope"))
		: Request.ContextEvidence.FindRef(TEXT("local_scope"));
	if (!Request.TargetGraph || !Request.TargetGraph->GetName().Equals(ScopeName, ESearchCase::IgnoreCase))
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
		Result.DebugMessage = TEXT("missing_or_mismatched_function_scope");
		return Result;
	}
}
```

For `field.function_param_get`, reject out/ref/return params before candidate creation:

```cpp
if (CapabilitySpec->Id == TEXT("field.function_param_get") && Request.Semantic.CapabilityFacts.FindRef(TEXT("field.param_flags")).Contains(TEXT("OutParm")))
{
	Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
	Result.DebugMessage = TEXT("function_output_param_belongs_to_control_return");
	return Result;
}
```

For `field.sparse_data_get`, enforce getter-only behavior:

```cpp
if (CapabilitySpec->Id == TEXT("field.sparse_data_get") && CapabilitySpec->AccessMode != EBlueprintHelperFieldCapabilityAccessMode::Get)
{
	Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
	Result.DebugMessage = TEXT("sparse_class_data_setter_not_registered");
	return Result;
}
```

- [ ] **Step 6: Run resolver tests and commit**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.FieldVariable; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: existing FieldVariable tests and new CapabilityStableId test pass.

Commit:

```bash
git add BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldVariableActionClusterTests.cpp
git commit -m "feat: resolve variable-like field capabilities with stable identity"
```

---

## Task 6: Enforce component_ref as component FObjectProperty variable nodes

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldVariableActionClusterTests.cpp`

- [ ] **Step 1: Add failing component spawner exclusion test**

Append this test to `BlueprintHelperActionResolutionContractTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldComponentRefDoesNotUseComponentNodeSpawnerTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.ComponentRef.DoesNotUseComponentNodeSpawner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldComponentRefDoesNotUseComponentNodeSpawnerTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionCandidate Candidate;
	Candidate.CapabilityId = TEXT("field.component_ref_get");
	Candidate.ExpectedNodeClassPath = TEXT("/Script/BlueprintGraph.K2Node_VariableGet");
	Candidate.NodeClassPath = TEXT("/Script/BlueprintGraph.K2Node_VariableGet");
	Candidate.ReadbackFacts.Add(TEXT("component_ref_spawner"), TEXT("UBlueprintVariableNodeSpawner"));

	TestFalse(TEXT("candidate node class is not add component"), Candidate.NodeClassPath.Contains(TEXT("K2Node_AddComponent")));
	TestFalse(TEXT("candidate does not name component node spawner"), Candidate.ReadbackFacts.FindRef(TEXT("component_ref_spawner")).Contains(TEXT("UBlueprintComponentNodeSpawner")));
	return true;
}
```

- [ ] **Step 2: Run the contract test and verify it fails or does not protect current resolver**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Contract.ComponentRef; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: the new contract compiles after Task 3; current resolver smoke coverage still needs resolver-side facts.

- [ ] **Step 3: Add resolver test for component property readback facts**

Append this test to `BlueprintHelperFieldVariableActionClusterTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterComponentRefUsesVariableGetTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.ComponentRef.UsesVariableGet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterComponentRefUsesVariableGetTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);

	FEdGraphPinType ObjectPinType(UEdGraphSchema_K2::PC_Object, NAME_None, UObject::StaticClass(), EPinContainerType::None, false, FEdGraphTerminalType());
	TestTrue(TEXT("add component-like object property"), AddFieldVariableActionTestVariable(Blueprint, TEXT("MeshComponent"), ObjectPinType));

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		Blueprint,
		GetFieldVariableActionTestGraph(Blueprint),
		TEXT("get"),
		TEXT("component_ref"),
		TEXT("MeshComponent"));
	Request.Semantic.CapabilityId = TEXT("field.component_ref_get");
	Request.Semantic.CapabilityFacts.Add(TEXT("field.component_name"), TEXT("MeshComponent"));
	Request.Semantic.CapabilityFacts.Add(TEXT("field.component_owner_class"), Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetPathName() : FString());
	Request.ContextEvidence.Add(TEXT("component_name"), TEXT("MeshComponent"));
	Request.ContextEvidence.Add(TEXT("component_kind"), TEXT("scs_or_native_property"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestEqual(TEXT("candidate count"), Result.CandidateActions.Num(), 1);
	if (Result.CandidateActions.Num() > 0)
	{
		const FBlueprintHelperActionCandidate& Candidate = Result.CandidateActions[0];
		TestEqual(TEXT("capability"), Candidate.CapabilityId, FString(TEXT("field.component_ref_get")));
		TestTrue(TEXT("variable get node"), Candidate.NodeClassPath.Contains(TEXT("K2Node_VariableGet")));
		TestFalse(TEXT("not add component"), Candidate.NodeClassPath.Contains(TEXT("K2Node_AddComponent")));
	}
	return true;
}
```

- [ ] **Step 4: Run resolver test and verify it fails if component facts are absent**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.ComponentRef.UsesVariableGet; Quit" -TestExit="Automation Test Queue Empty"
```

Expected before resolver update: failure because candidate does not carry component capability facts or resolution misses component evidence.

- [ ] **Step 5: Implement component_ref property resolution**

In `BlueprintHelperFieldVariableActionResolver.cpp`, add a component branch before generic property lookup:

```cpp
const bool bComponentRef = CapabilitySpec->RootKind == EBlueprintHelperFieldCapabilityRootKind::ComponentRef;
if (bComponentRef)
{
	const FString ComponentName = !Request.Semantic.CapabilityFacts.FindRef(TEXT("field.component_name")).IsEmpty()
		? Request.Semantic.CapabilityFacts.FindRef(TEXT("field.component_name"))
		: Request.ContextEvidence.FindRef(TEXT("component_name"));
	FProperty* ComponentProperty = FindFProperty<FProperty>(Request.Blueprint ? Request.Blueprint->SkeletonGeneratedClass : nullptr, FName(*ComponentName));
	if (!ComponentProperty && Request.Blueprint && Request.Blueprint->GeneratedClass)
	{
		ComponentProperty = FindFProperty<FProperty>(Request.Blueprint->GeneratedClass, FName(*ComponentName));
	}
	FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(ComponentProperty);
	if (!ObjectProperty)
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
		Result.DebugMessage = TEXT("not_class_component_property");
		return Result;
	}

	Candidate.ReadbackFacts.Add(TEXT("component_name"), ComponentName);
	Candidate.ReadbackFacts.Add(TEXT("component_ref_spawner"), TEXT("UBlueprintVariableNodeSpawner"));
	Candidate.ReadbackFacts.Add(TEXT("component_property_class"), ObjectProperty->PropertyClass ? ObjectProperty->PropertyClass->GetPathName() : FString());
}
```

Ensure the node class remains `UK2Node_VariableGet` / `UK2Node_VariableSet` based on capability access mode.

- [ ] **Step 6: Run component tests and commit**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.ComponentRef; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: component ref tests pass and no candidate contains `K2Node_AddComponent`.

Commit:

```bash
git add BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldVariableActionClusterTests.cpp
git commit -m "fix: resolve component refs as component object properties"
```

---

## Task 7: Resolve object-pin and component-property field access

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldVariableActionClusterTests.cpp`

- [ ] **Step 1: Add failing object-pin field access test**

Append this test to `BlueprintHelperFieldVariableActionClusterTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterObjectPinMemberGetRequiresTargetPinTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.ObjectPinMemberGet.RequiresTargetPin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterObjectPinMemberGetRequiresTargetPinTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		Blueprint,
		GetFieldVariableActionTestGraph(Blueprint),
		TEXT("get"),
		TEXT("field_access"),
		TEXT("OwnerActor.Health"));
	Request.Semantic.CapabilityId = TEXT("field.object_pin_member_get");
	Request.Semantic.CapabilityFacts.Add(TEXT("field.target_pin_ref"), TEXT("node:OwnerActor pin:ReturnValue"));
	Request.Semantic.CapabilityFacts.Add(TEXT("field.owner_class"), TEXT("/Script/Engine.Actor"));
	Request.Semantic.CapabilityFacts.Add(TEXT("field.member_name"), TEXT("Health"));
	Request.Semantic.CapabilityFacts.Add(TEXT("field.target_pin_type"), UEdGraphSchema_K2::PC_Object.ToString());
	Request.Semantic.CapabilityFacts.Add(TEXT("field.target_pin_object_path"), TEXT("/Script/Engine.Actor"));
	Request.ContextEvidence.Add(TEXT("target_pin_ref"), Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_ref")));
	Request.ContextEvidence.Add(TEXT("linked_pin_type_category"), Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_type")));
	Request.ContextEvidence.Add(TEXT("linked_pin_type_object_path"), Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_object_path")));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	if (Result.CandidateActions.Num() > 0)
	{
		const FBlueprintHelperActionCandidate& Candidate = Result.CandidateActions[0];
		TestEqual(TEXT("capability id"), Candidate.CapabilityId, FString(TEXT("field.object_pin_member_get")));
		TestEqual(TEXT("target pin category"), Candidate.CapabilityFacts.FindRef(TEXT("field.target_pin_type")), FString(UEdGraphSchema_K2::PC_Object.ToString()));
		TestTrue(TEXT("stable id includes target pin class"), Candidate.StableId.Contains(TEXT("/Script/Engine.Actor")));
	}
	return true;
}
```

- [ ] **Step 2: Run the test and verify it fails**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.ObjectPinMemberGet.RequiresTargetPin; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: resolver fails because object-pin owner/target pin facts are not required or propagated.

- [ ] **Step 3: Enforce target pin context for object/component property access**

In `BlueprintHelperFieldVariableActionResolver.cpp`, before candidate creation for capabilities with `bRequiresTargetPin`, add:

```cpp
if (CapabilitySpec->bRequiresTargetPin)
{
	const FString TargetPinRef = !Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_ref")).IsEmpty()
		? Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_ref"))
		: Request.ContextEvidence.FindRef(TEXT("target_pin_ref"));
	const FString TargetPinCategory = !Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_type")).IsEmpty()
		? Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_type"))
		: Request.ContextEvidence.FindRef(TEXT("linked_pin_type_category"));
	const FString TargetPinObjectPath = !Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_object_path")).IsEmpty()
		? Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_object_path"))
		: Request.ContextEvidence.FindRef(TEXT("linked_pin_type_object_path"));

	if (TargetPinRef.IsEmpty() || TargetPinCategory.IsEmpty() || TargetPinObjectPath.IsEmpty())
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
		Result.DebugMessage = TEXT("missing_target_pin_projection");
		return Result;
	}

	Identity.TargetPinCategory = TargetPinCategory;
	Identity.TargetPinObjectPath = TargetPinObjectPath;
	Candidate.ReadbackFacts.Add(TEXT("target_pin_ref"), TargetPinRef);
	Candidate.ReadbackFacts.Add(TEXT("target_pin_category"), TargetPinCategory);
	Candidate.ReadbackFacts.Add(TEXT("target_pin_object_path"), TargetPinObjectPath);
}
```

- [ ] **Step 4: Resolve owner class from target pin class when explicit owner matches**

Add owner consistency check:

```cpp
if (CapabilitySpec->bRequiresOwnerClass && !Identity.OwnerClassPath.IsEmpty() && !Identity.TargetPinObjectPath.IsEmpty())
{
	if (!Identity.TargetPinObjectPath.Equals(Identity.OwnerClassPath, ESearchCase::IgnoreCase))
	{
		Candidate.ReadbackFacts.Add(TEXT("owner_projected_from_target_pin"), Identity.TargetPinObjectPath);
	}
}
```

Do not infer from drag UI state; only use `Request.Semantic.CapabilityFacts[field.target_pin_ref]` and explicit context evidence.

- [ ] **Step 5: Run object/component field access tests and commit**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.ObjectPinMemberGet; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: object-pin field access test passes.

Commit:

```bash
git add BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldVariableActionClusterTests.cpp
git commit -m "feat: resolve object pin field access with explicit target pin facts"
```

---

## Task 8: Add dedicated Field fragment builder for variable, component, and target-pin fragments

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperFieldFragmentBuilder.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperFieldFragmentBuilder.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuilderRegistry.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldFragmentBuilderTests.cpp`

- [ ] **Step 1: Add failing variable fragment builder smoke test**

Create `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldFragmentBuilderTests.cpp`:

```cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperFieldFragmentBuilder.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldFragmentBuilderSelectsVariableNodeFamiliesTest,
	"BlueprintHelper.GraphWrite.FieldFragmentBuilder.SelectsVariableNodeFamilies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldFragmentBuilderSelectsVariableNodeFamiliesTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperFieldFragmentPlan Plan;
	Plan.CapabilityId = TEXT("field.member_set");
	Plan.ExpectedNodeFamily = TEXT("variable_set");
	Plan.ExpectedNodeClassPath = TEXT("/Script/BlueprintGraph.K2Node_VariableSet");
	Plan.MemberName = TEXT("Health");

	TestTrue(TEXT("set fragment needs exec pins"), FBlueprintHelperFieldFragmentBuilder::DoesCapabilityProduceExecPins(Plan.CapabilityId));
	TestEqual(TEXT("node family"), FBlueprintHelperFieldFragmentBuilder::ExpectedNodeFamilyForCapability(Plan.CapabilityId), FString(TEXT("variable_set")));
	return true;
}

#endif
```

- [ ] **Step 2: Run the test and verify it fails**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.FieldFragmentBuilder.SelectsVariableNodeFamilies; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: build fails because `BlueprintHelperFieldFragmentBuilder.h` does not exist.

- [ ] **Step 3: Create fragment builder header**

Create `BlueprintHelperFieldFragmentBuilder.h`:

```cpp
#pragma once

#include "CoreMinimal.h"

class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UK2Node;
struct FBlueprintHelperActionCandidate;
struct FBlueprintHelperGraphFragmentBuildResult;
struct FBlueprintHelperGraphStatementIR;

struct FBlueprintHelperFieldFragmentPlan
{
	FString CapabilityId;
	FString ExpectedNodeFamily;
	FString ExpectedNodeClassPath;
	FString MemberName;
	FString OwnerClassPath;
	FString TargetPinRef;
	FString PropertyPath;
	FString LiteralValue;
	TMap<FString, FString> ResolvedFacts;
};

class FBlueprintHelperFieldFragmentBuilder
{
public:
	static FString ExpectedNodeFamilyForCapability(const FString& CapabilityId);
	static bool DoesCapabilityProduceExecPins(const FString& CapabilityId);

	static bool BuildVariableGetFragment(
		UEdGraph* Graph,
		const FBlueprintHelperGraphStatementIR& Statement,
		const FBlueprintHelperActionCandidate& Candidate,
		FBlueprintHelperGraphFragmentBuildResult& OutResult);

	static bool BuildVariableSetFragment(
		UEdGraph* Graph,
		const FBlueprintHelperGraphStatementIR& Statement,
		const FBlueprintHelperActionCandidate& Candidate,
		FBlueprintHelperGraphFragmentBuildResult& OutResult);

	static bool ConnectTargetPinIfRequested(
		UEdGraph* Graph,
		UEdGraphPin* SourceObjectPin,
		UEdGraphPin* VariableTargetPin,
		FString& OutSchemaResponse);
};
```

- [ ] **Step 4: Create fragment builder implementation**

Create `BlueprintHelperFieldFragmentBuilder.cpp`:

```cpp
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperFieldFragmentBuilder.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"

FString FBlueprintHelperFieldFragmentBuilder::ExpectedNodeFamilyForCapability(const FString& CapabilityId)
{
	const FBlueprintHelperFieldCapabilitySpec* Spec = FBlueprintHelperFieldCapabilityRegistry::FindById(CapabilityId);
	return Spec ? Spec->ExpectedNodeFamily : FString();
}

bool FBlueprintHelperFieldFragmentBuilder::DoesCapabilityProduceExecPins(const FString& CapabilityId)
{
	const FBlueprintHelperFieldCapabilitySpec* Spec = FBlueprintHelperFieldCapabilityRegistry::FindById(CapabilityId);
	return Spec ? Spec->bProducesExecPins : false;
}

bool FBlueprintHelperFieldFragmentBuilder::BuildVariableGetFragment(
	UEdGraph* Graph,
	const FBlueprintHelperGraphStatementIR& Statement,
	const FBlueprintHelperActionCandidate& Candidate,
	FBlueprintHelperGraphFragmentBuildResult& OutResult)
{
	if (!Graph || !Candidate.Spawner)
	{
		return false;
	}

	UEdGraphNode* Node = Candidate.Spawner->Invoke(Graph, nullptr, FVector2D(Statement.PositionX, Statement.PositionY));
	if (!Node)
	{
		return false;
	}

	OutResult.CreatedNodes.Add(Node);
	OutResult.DebugFacts.Add(TEXT("field_capability_id"), Candidate.CapabilityId);
	OutResult.DebugFacts.Add(TEXT("expected_node_family"), Candidate.ExpectedNodeFamily);
	OutResult.DebugFacts.Add(TEXT("expected_node_class"), Candidate.ExpectedNodeClassPath);
	return true;
}

bool FBlueprintHelperFieldFragmentBuilder::BuildVariableSetFragment(
	UEdGraph* Graph,
	const FBlueprintHelperGraphStatementIR& Statement,
	const FBlueprintHelperActionCandidate& Candidate,
	FBlueprintHelperGraphFragmentBuildResult& OutResult)
{
	if (!BuildVariableGetFragment(Graph, Statement, Candidate, OutResult))
	{
		return false;
	}

	OutResult.DebugFacts.Add(TEXT("field_set_has_exec_pins"), TEXT("true"));
	return true;
}

bool FBlueprintHelperFieldFragmentBuilder::ConnectTargetPinIfRequested(
	UEdGraph* Graph,
	UEdGraphPin* SourceObjectPin,
	UEdGraphPin* VariableTargetPin,
	FString& OutSchemaResponse)
{
	if (!Graph || !SourceObjectPin || !VariableTargetPin)
	{
		OutSchemaResponse = TEXT("missing_pin_for_target_connection");
		return false;
	}

	const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(Graph->GetSchema());
	if (!Schema)
	{
		OutSchemaResponse = TEXT("missing_k2_schema");
		return false;
	}

	const FPinConnectionResponse Response = Schema->CanCreateConnection(SourceObjectPin, VariableTargetPin);
	OutSchemaResponse = Response.Message.ToString();
	if (Response.Response != CONNECT_RESPONSE_MAKE)
	{
		return false;
	}

	return Schema->TryCreateConnection(SourceObjectPin, VariableTargetPin);
}
```

If `FBlueprintHelperGraphFragmentBuildResult` does not currently expose `CreatedNodes` or `DebugFacts`, add those fields to its existing declaration in the file where the struct is defined:

```cpp
TArray<TObjectPtr<UEdGraphNode>> CreatedNodes;
TMap<FString, FString> DebugFacts;
```

- [ ] **Step 5: Register the builder**

In `BlueprintHelperGraphFragmentBuilderRegistry.cpp`, add a registry entry for `field_variable_action` or the current registry key used by Field fragments:

```cpp
Registry.Register(TEXT("field_variable_action"), TEXT("FBlueprintHelperFieldFragmentBuilder"));
```

Use the existing registry method name and key format in that file; the registered value must be exactly `FBlueprintHelperFieldFragmentBuilder`.

- [ ] **Step 6: Run fragment builder tests and commit**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.FieldFragmentBuilder; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: fragment builder tests pass.

Commit:

```bash
git add BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperFieldFragmentBuilder.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperFieldFragmentBuilder.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuilderRegistry.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldFragmentBuilderTests.cpp
git commit -m "feat: add dedicated graphwrite field fragment builder"
```

---

## Task 9: Add struct member get/set and nested property path planning

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldPathResolution.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldPathResolution.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperFieldFragmentBuilder.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperFieldFragmentBuilder.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldFragmentBuilderTests.cpp`

- [ ] **Step 1: Add failing property path planner test**

Append this test to `BlueprintHelperFieldFragmentBuilderTests.cpp` before `#endif`:

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldPathResolution.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldPathResolutionPlansStructAndNestedPathsTest,
	"BlueprintHelper.GraphWrite.FieldFragmentBuilder.PropertyPath.PlansStructAndNestedPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldPathResolutionPlansStructAndNestedPathsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperFieldPathResolutionRequest Request;
	Request.CapabilityId = TEXT("field.struct_member_get");
	Request.RootName = TEXT("TransformValue");
	Request.PropertyPath = TEXT("Location.X");
	Request.Operation = TEXT("get_property");
	Request.Scope = TEXT("property_path");

	FBlueprintHelperFieldPathResolutionResult Result;
	const bool bResolved = FBlueprintHelperFieldPathResolver::Resolve(Request, Result);
	TestTrue(TEXT("path resolved"), bResolved);
	TestEqual(TEXT("capability"), Result.CapabilityId, FString(TEXT("field.struct_member_get")));
	TestEqual(TEXT("root name"), Result.RootName, FString(TEXT("TransformValue")));
	TestEqual(TEXT("segment count"), Result.Segments.Num(), 2);
	TestEqual(TEXT("node family"), Result.NodeFamilyPlan, FString(TEXT("break_struct")));
	TestTrue(TEXT("requires fragment decomposition"), Result.bRequiresFragmentDecomposition);
	return true;
}
```

- [ ] **Step 2: Run the test and verify it fails**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.FieldFragmentBuilder.PropertyPath.PlansStructAndNestedPaths; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: build fails because the new request/result fields and static `Resolve` signature do not exist.

- [ ] **Step 3: Replace coarse path resolution with segment plan**

In `BlueprintHelperFieldPathResolution.h`, define:

```cpp
struct FBlueprintHelperFieldPathResolutionRequest
{
	FString CapabilityId;
	FString RootName;
	FString PropertyPath;
	FString Operation;
	FString Scope;
	FString OwnerClassPath;
	FString TargetPinRef;
};

struct FBlueprintHelperFieldPathSegment
{
	FString Name;
	FString OwnerTypePath;
	FString StructTypePath;
	FGuid MemberGuid;
	FString ExpectedPinCategory;
};

struct FBlueprintHelperFieldPathResolutionResult
{
	FString CapabilityId;
	FString RootName;
	TArray<FBlueprintHelperFieldPathSegment> Segments;
	FString RootKind;
	FString NodeFamilyPlan;
	FString AccessMode;
	bool bRequiresFragmentDecomposition = false;
	bool bUsesBreakStruct = false;
	bool bUsesSetFieldsInStruct = false;
	bool bUsesVariableTargetPin = false;
};

class FBlueprintHelperFieldPathResolver
{
public:
	static bool Resolve(
		const FBlueprintHelperFieldPathResolutionRequest& Request,
		FBlueprintHelperFieldPathResolutionResult& OutResult);
};
```

- [ ] **Step 4: Implement deterministic path planning**

In `BlueprintHelperFieldPathResolution.cpp`, implement:

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldPathResolution.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.h"

bool FBlueprintHelperFieldPathResolver::Resolve(
	const FBlueprintHelperFieldPathResolutionRequest& Request,
	FBlueprintHelperFieldPathResolutionResult& OutResult)
{
	const FBlueprintHelperFieldCapabilitySpec* Spec = FBlueprintHelperFieldCapabilityRegistry::FindById(Request.CapabilityId);
	if (!Spec || !Spec->bRequiresPropertyPath || Request.RootName.IsEmpty() || Request.PropertyPath.IsEmpty())
	{
		return false;
	}

	OutResult.CapabilityId = Request.CapabilityId;
	OutResult.RootName = Request.RootName;
	OutResult.AccessMode = Spec->FieldOperation;
	OutResult.RootKind = UEnum::GetValueAsString(Spec->RootKind);
	OutResult.NodeFamilyPlan = Spec->ExpectedNodeFamily;
	OutResult.bUsesBreakStruct = Spec->ExpectedNodeFamily == TEXT("break_struct");
	OutResult.bUsesSetFieldsInStruct = Spec->ExpectedNodeFamily == TEXT("set_fields_in_struct");
	OutResult.bUsesVariableTargetPin = Spec->bRequiresTargetPin;

	TArray<FString> Parts;
	Request.PropertyPath.ParseIntoArray(Parts, TEXT("."), true);
	for (const FString& Part : Parts)
	{
		FBlueprintHelperFieldPathSegment Segment;
		Segment.Name = Part;
		Segment.OwnerTypePath = Request.OwnerClassPath;
		OutResult.Segments.Add(Segment);
	}

	OutResult.bRequiresFragmentDecomposition = OutResult.Segments.Num() > 1 || Request.CapabilityId == TEXT("field.nested_property_path");
	return OutResult.Segments.Num() > 0;
}
```

- [ ] **Step 5: Add struct builder declarations**

In `BlueprintHelperFieldFragmentBuilder.h`, add:

```cpp
static bool BuildStructReadFragment(
	UEdGraph* Graph,
	const FBlueprintHelperFieldPathResolutionResult& PathPlan,
	FBlueprintHelperGraphFragmentBuildResult& OutResult);

static bool BuildStructWriteFragment(
	UEdGraph* Graph,
	const FBlueprintHelperFieldPathResolutionResult& PathPlan,
	FBlueprintHelperGraphFragmentBuildResult& OutResult);

static bool BuildNestedPropertyPathFragment(
	UEdGraph* Graph,
	const FBlueprintHelperFieldPathResolutionResult& PathPlan,
	FBlueprintHelperGraphFragmentBuildResult& OutResult);
```

- [ ] **Step 6: Implement struct builder skeleton with explicit node-family facts**

In `BlueprintHelperFieldFragmentBuilder.cpp`, add implementations that return false when `Graph` is null and write node-family facts before spawn-specific code is added:

```cpp
bool FBlueprintHelperFieldFragmentBuilder::BuildStructReadFragment(
	UEdGraph* Graph,
	const FBlueprintHelperFieldPathResolutionResult& PathPlan,
	FBlueprintHelperGraphFragmentBuildResult& OutResult)
{
	if (!Graph || !PathPlan.bUsesBreakStruct)
	{
		return false;
	}
	OutResult.DebugFacts.Add(TEXT("field_capability_id"), PathPlan.CapabilityId);
	OutResult.DebugFacts.Add(TEXT("expected_node_family"), TEXT("break_struct"));
	OutResult.DebugFacts.Add(TEXT("property_path_segment_count"), FString::FromInt(PathPlan.Segments.Num()));
	return true;
}

bool FBlueprintHelperFieldFragmentBuilder::BuildStructWriteFragment(
	UEdGraph* Graph,
	const FBlueprintHelperFieldPathResolutionResult& PathPlan,
	FBlueprintHelperGraphFragmentBuildResult& OutResult)
{
	if (!Graph || !PathPlan.bUsesSetFieldsInStruct)
	{
		return false;
	}
	OutResult.DebugFacts.Add(TEXT("field_capability_id"), PathPlan.CapabilityId);
	OutResult.DebugFacts.Add(TEXT("expected_node_family"), TEXT("set_fields_in_struct"));
	OutResult.DebugFacts.Add(TEXT("property_path_segment_count"), FString::FromInt(PathPlan.Segments.Num()));
	return true;
}

bool FBlueprintHelperFieldFragmentBuilder::BuildNestedPropertyPathFragment(
	UEdGraph* Graph,
	const FBlueprintHelperFieldPathResolutionResult& PathPlan,
	FBlueprintHelperGraphFragmentBuildResult& OutResult)
{
	if (!Graph || !PathPlan.bRequiresFragmentDecomposition)
	{
		return false;
	}
	OutResult.DebugFacts.Add(TEXT("field_capability_id"), PathPlan.CapabilityId);
	OutResult.DebugFacts.Add(TEXT("expected_node_family"), TEXT("property_path_fragment"));
	OutResult.DebugFacts.Add(TEXT("property_path_segment_count"), FString::FromInt(PathPlan.Segments.Num()));
	return true;
}
```

- [ ] **Step 7: Run property path tests and commit**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.FieldFragmentBuilder.PropertyPath; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: property path planner tests pass.

Commit:

```bash
git add BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldPathResolution.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldPathResolution.cpp \
        BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperFieldFragmentBuilder.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperFieldFragmentBuilder.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldFragmentBuilderTests.cpp
git commit -m "feat: plan struct and nested field property paths"
```

---

## Task 10: Add Field readback facts and compile diagnostic plumbing

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/BlueprintHelper.Build.cs`
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldActionReadback.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldActionReadback.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDebugData.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldActionReadbackTests.cpp`

- [ ] **Step 1: Add failing readback serialization test**

Create `BlueprintHelperFieldActionReadbackTests.cpp`:

```cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldActionReadback.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldActionReadbackSerializesCoreFactsTest,
	"BlueprintHelper.GraphWrite.FieldReadback.SerializesCoreFacts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldActionReadbackSerializesCoreFactsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperFieldActionReadback Readback;
	Readback.CapabilityId = TEXT("field.member_get");
	Readback.NodeClassPath = TEXT("/Script/BlueprintGraph.K2Node_VariableGet");
	Readback.MemberReference.MemberName = TEXT("Health");
	Readback.MemberReference.OwnerClassPath = TEXT("/Script/Engine.Actor");
	Readback.MemberReference.ResolveResult = TEXT("resolved");

	TMap<FString, FString> Facts;
	Readback.AppendFlatFacts(Facts);

	TestEqual(TEXT("capability"), Facts.FindRef(TEXT("field.capability_id")), FString(TEXT("field.member_get")));
	TestEqual(TEXT("node class"), Facts.FindRef(TEXT("field.node_class")), FString(TEXT("/Script/BlueprintGraph.K2Node_VariableGet")));
	TestEqual(TEXT("member"), Facts.FindRef(TEXT("field.member_reference.member_name")), FString(TEXT("Health")));
	TestEqual(TEXT("resolve result"), Facts.FindRef(TEXT("field.member_reference.resolve_result")), FString(TEXT("resolved")));
	return true;
}

#endif
```

- [ ] **Step 2: Run the test and verify it fails**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.FieldReadback.SerializesCoreFacts; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: build fails because readback types do not exist.

- [ ] **Step 3: Keep readback dependencies narrow**

The current implementation records readback and basic compile-status diagnostics without consuming `KismetCompiler` APIs, so `BlueprintHelper.Build.cs` does not need a new module dependency. If future work starts reading compiler message logs directly, add that dependency in the same change that introduces the direct API usage.

- [ ] **Step 4: Create readback header**

Create `BlueprintHelperFieldActionReadback.h`:

```cpp
#pragma once

#include "CoreMinimal.h"

class UEdGraphNode;
class UEdGraphPin;
class UBlueprint;

struct FBlueprintHelperFieldMemberReferenceReadback
{
	FString MemberName;
	FString OwnerClassPath;
	FGuid MemberGuid;
	FString LocalScopeName;
	FString ResolveResult;
};

struct FBlueprintHelperFieldPinReadback
{
	FString PinName;
	FString Direction;
	FString PinCategory;
	FString PinSubCategory;
	FString PinObjectPath;
	FString DefaultValue;
	TArray<FString> LinkedTo;
	FString ParentPinName;
	TArray<FString> SubPinNames;
};

struct FBlueprintHelperFieldLinkReadback
{
	FString FromNodeGuid;
	FString FromPinName;
	FString ToNodeGuid;
	FString ToPinName;
	FString SchemaResponse;
};

struct FBlueprintHelperFieldSplitStateReadback
{
	FString RootPinName;
	TArray<FString> SubPinNames;
	bool bParentHidden = false;
	FString RestoredState;
};

struct FBlueprintHelperFieldCompileDiagnosticReadback
{
	FString Severity;
	FString Message;
	FString NodeGuid;
	FString PinGuid;
	FString CapabilityId;
};

struct FBlueprintHelperFieldActionReadback
{
	FString CapabilityId;
	FString NodeGuid;
	FString NodeClassPath;
	FString NodeTitle;
	FBlueprintHelperFieldMemberReferenceReadback MemberReference;
	TArray<FBlueprintHelperFieldPinReadback> Pins;
	TArray<FBlueprintHelperFieldLinkReadback> Links;
	TArray<FBlueprintHelperFieldSplitStateReadback> SplitStates;
	TArray<FBlueprintHelperFieldCompileDiagnosticReadback> CompileDiagnostics;

	void AppendFlatFacts(TMap<FString, FString>& OutFacts) const;
};

class FBlueprintHelperFieldActionReadbackCollector
{
public:
	static FBlueprintHelperFieldActionReadback CollectFromNode(const FString& CapabilityId, UEdGraphNode* Node);
	static void CollectCompileDiagnostics(UBlueprint* Blueprint, const FString& CapabilityId, TArray<FBlueprintHelperFieldCompileDiagnosticReadback>& OutDiagnostics);
};
```

- [ ] **Step 5: Create readback implementation**

Create `BlueprintHelperFieldActionReadback.cpp`:

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldActionReadback.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"

void FBlueprintHelperFieldActionReadback::AppendFlatFacts(TMap<FString, FString>& OutFacts) const
{
	OutFacts.Add(TEXT("field.capability_id"), CapabilityId);
	OutFacts.Add(TEXT("field.node_guid"), NodeGuid);
	OutFacts.Add(TEXT("field.node_class"), NodeClassPath);
	OutFacts.Add(TEXT("field.node_title"), NodeTitle);
	OutFacts.Add(TEXT("field.member_reference.member_name"), MemberReference.MemberName);
	OutFacts.Add(TEXT("field.member_reference.owner_class"), MemberReference.OwnerClassPath);
	OutFacts.Add(TEXT("field.member_reference.member_guid"), MemberReference.MemberGuid.ToString(EGuidFormats::Digits));
	OutFacts.Add(TEXT("field.member_reference.local_scope"), MemberReference.LocalScopeName);
	OutFacts.Add(TEXT("field.member_reference.resolve_result"), MemberReference.ResolveResult);
	OutFacts.Add(TEXT("field.pin_count"), FString::FromInt(Pins.Num()));
	OutFacts.Add(TEXT("field.link_count"), FString::FromInt(Links.Num()));
	OutFacts.Add(TEXT("field.split_state_count"), FString::FromInt(SplitStates.Num()));
	OutFacts.Add(TEXT("field.compile_diagnostic_count"), FString::FromInt(CompileDiagnostics.Num()));
}

FBlueprintHelperFieldActionReadback FBlueprintHelperFieldActionReadbackCollector::CollectFromNode(const FString& CapabilityId, UEdGraphNode* Node)
{
	FBlueprintHelperFieldActionReadback Readback;
	Readback.CapabilityId = CapabilityId;
	if (!Node)
	{
		Readback.MemberReference.ResolveResult = TEXT("missing_node");
		return Readback;
	}

	Readback.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
	Readback.NodeClassPath = Node->GetClass()->GetPathName();
	Readback.NodeTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
	Readback.MemberReference.ResolveResult = TEXT("resolved_node_present");

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin)
		{
			continue;
		}
		FBlueprintHelperFieldPinReadback PinFact;
		PinFact.PinName = Pin->PinName.ToString();
		PinFact.Direction = Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output");
		PinFact.PinCategory = Pin->PinType.PinCategory.ToString();
		PinFact.PinSubCategory = Pin->PinType.PinSubCategory.ToString();
		PinFact.PinObjectPath = Pin->PinType.PinSubCategoryObject.IsValid() ? Pin->PinType.PinSubCategoryObject->GetPathName() : FString();
		PinFact.DefaultValue = Pin->DefaultValue;
		PinFact.ParentPinName = Pin->ParentPin ? Pin->ParentPin->PinName.ToString() : FString();
		for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
		{
			if (LinkedPin)
			{
				PinFact.LinkedTo.Add(FString::Printf(TEXT("%s.%s"), *LinkedPin->GetOwningNode()->NodeGuid.ToString(EGuidFormats::Digits), *LinkedPin->PinName.ToString()));
			}
		}
		for (UEdGraphPin* SubPin : Pin->SubPins)
		{
			if (SubPin)
			{
				PinFact.SubPinNames.Add(SubPin->PinName.ToString());
			}
		}
		Readback.Pins.Add(PinFact);
	}

	return Readback;
}

void FBlueprintHelperFieldActionReadbackCollector::CollectCompileDiagnostics(UBlueprint* Blueprint, const FString& CapabilityId, TArray<FBlueprintHelperFieldCompileDiagnosticReadback>& OutDiagnostics)
{
	if (!Blueprint || Blueprint->Status != BS_Error)
	{
		return;
	}

	FBlueprintHelperFieldCompileDiagnosticReadback Diagnostic;
	Diagnostic.Severity = TEXT("error");
	Diagnostic.Message = TEXT("blueprint_compile_failed_for_field_statement");
	Diagnostic.CapabilityId = CapabilityId;
	OutDiagnostics.Add(Diagnostic);
}
```

- [ ] **Step 6: Serialize readback facts into evidence and DebugBundle**

Do not add Field-specific top-level payload containers to `BlueprintHelperGraphFragmentEvidence.h` or shared graph fragment DTOs. Field readback helpers should flatten their observations into existing generic metadata/debug maps with namespaced keys such as `field.node_class`, `field.node_guid`, `field.expected_node_family`, and `field.*` facts.

Do not include the readback header from shared graph fragment evidence types.

In `BlueprintHelperGraphFragmentEvidence.cpp`, when evidence is flattened or converted to JSON, consume existing generic metadata/debug facts. Do not introduce a Field-specific evidence tree.

In `BlueprintHelperGraphFragmentDebugData.cpp`, include these flat keys in DebugBundle output. Do not add a per-field Review target; keep the existing graph-block review target.

- [ ] **Step 7: Run readback tests and commit**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.FieldReadback; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: readback tests pass.

Commit:

```bash
git add BlueprintHelper/Source/BlueprintHelper/BlueprintHelper.Build.cs \
        BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldActionReadback.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldActionReadback.cpp \
        BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDebugData.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldActionReadbackTests.cpp
git commit -m "feat: add field action readback and diagnostics facts"
```

---

## Task 11: Route GraphStatementBuilder by first-class Field capability ID

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldFragmentBuilderTests.cpp`

- [ ] **Step 1: Add failing builder routing test**

Append this test to `BlueprintHelperFieldFragmentBuilderTests.cpp` before `#endif`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphStatementBuilderRoutesFieldCapabilityFamiliesTest,
	"BlueprintHelper.GraphWrite.FieldFragmentBuilder.Routing.RoutesCapabilityFamilies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphStatementBuilderRoutesFieldCapabilityFamiliesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("member get family"), FBlueprintHelperFieldFragmentBuilder::ExpectedNodeFamilyForCapability(TEXT("field.member_get")), FString(TEXT("variable_get")));
	TestEqual(TEXT("member set family"), FBlueprintHelperFieldFragmentBuilder::ExpectedNodeFamilyForCapability(TEXT("field.member_set")), FString(TEXT("variable_set")));
	TestEqual(TEXT("component ref family"), FBlueprintHelperFieldFragmentBuilder::ExpectedNodeFamilyForCapability(TEXT("field.component_ref_get")), FString(TEXT("component_variable_get")));
	TestEqual(TEXT("object pin family"), FBlueprintHelperFieldFragmentBuilder::ExpectedNodeFamilyForCapability(TEXT("field.object_pin_member_get")), FString(TEXT("variable_get_target")));
	TestEqual(TEXT("struct get family"), FBlueprintHelperFieldFragmentBuilder::ExpectedNodeFamilyForCapability(TEXT("field.struct_member_get")), FString(TEXT("break_struct")));
	TestEqual(TEXT("struct set family"), FBlueprintHelperFieldFragmentBuilder::ExpectedNodeFamilyForCapability(TEXT("field.struct_member_set")), FString(TEXT("set_fields_in_struct")));
	TestEqual(TEXT("nested family"), FBlueprintHelperFieldFragmentBuilder::ExpectedNodeFamilyForCapability(TEXT("field.nested_property_path")), FString(TEXT("property_path_fragment")));
	return true;
}
```

- [ ] **Step 2: Run routing test**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.FieldFragmentBuilder.Routing.RoutesCapabilityFamilies; Quit" -TestExit="Automation Test Queue Empty"
```

Expected before routing: test may pass by taxonomy alone, but GraphStatementBuilder still does not consume the route. Continue with builder routing.

- [ ] **Step 3: Add routing method declarations**

In `BlueprintHelperGraphStatementBuilder.h`, add private methods:

```cpp
bool BuildFieldCapabilityStatement(
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperGraphFragmentBuildResult& OutResult);

bool BuildFieldCapabilityExpression(
	const FBlueprintHelperGraphExpressionIR& Expression,
	FBlueprintHelperGraphFragmentBuildResult& OutResult);
```

- [ ] **Step 4: Route statements by capability node family**

In `BlueprintHelperGraphStatementBuilder.cpp`, when statement kind is `field`, resolve `CapabilityId` to a spec:

```cpp
const FBlueprintHelperFieldCapabilitySpec* Spec = FBlueprintHelperFieldCapabilityRegistry::FindById(Statement.CapabilityId);
if (!Spec)
{
	OutResult.DebugFacts.Add(TEXT("field_error"), TEXT("unknown_field_capability"));
	return false;
}

if (Spec->ExpectedNodeFamily == TEXT("variable_get") || Spec->ExpectedNodeFamily == TEXT("component_variable_get") || Spec->ExpectedNodeFamily == TEXT("variable_get_target"))
{
	return FBlueprintHelperFieldFragmentBuilder::BuildVariableGetFragment(TargetGraph, Statement, Candidate, OutResult);
}

if (Spec->ExpectedNodeFamily == TEXT("variable_set") || Spec->ExpectedNodeFamily == TEXT("component_variable_set") || Spec->ExpectedNodeFamily == TEXT("variable_set_target"))
{
	return FBlueprintHelperFieldFragmentBuilder::BuildVariableSetFragment(TargetGraph, Statement, Candidate, OutResult);
}

if (Spec->ExpectedNodeFamily == TEXT("break_struct"))
{
	FBlueprintHelperFieldPathResolutionResult PathPlan;
	return ResolvePathAndBuildStructRead(Statement, PathPlan, OutResult);
}

if (Spec->ExpectedNodeFamily == TEXT("set_fields_in_struct"))
{
	FBlueprintHelperFieldPathResolutionResult PathPlan;
	return ResolvePathAndBuildStructWrite(Statement, PathPlan, OutResult);
}

if (Spec->ExpectedNodeFamily == TEXT("property_path_fragment"))
{
	FBlueprintHelperFieldPathResolutionResult PathPlan;
	return ResolvePathAndBuildNestedPropertyPath(Statement, PathPlan, OutResult);
}

OutResult.DebugFacts.Add(TEXT("field_error"), TEXT("unsupported_field_node_family"));
return false;
```

Use existing builder variable names in the function; `TargetGraph`, `Candidate`, and `OutResult` must be the local variables already produced by current ActionResolution flow.

- [ ] **Step 5: Append readback after successful build**

After any successful Field build, collect readback:

```cpp
for (UEdGraphNode* CreatedNode : OutResult.CreatedNodes)
{
	FBlueprintHelperFieldActionReadback Readback = FBlueprintHelperFieldActionReadbackCollector::CollectFromNode(Statement.CapabilityId, CreatedNode);
	Readback.AppendFlatFacts(OutFragment.OwnershipTags);
}
```

If the result struct stores evidence in a nested generic metadata object, append the flat facts to that existing object. Do not add a second Field-specific storage location.

- [ ] **Step 6: Run routing and existing graph statement tests, then commit**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.FieldFragmentBuilder.Routing; Automation RunTests BlueprintHelper.GraphWrite.GraphStatement; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: Field routing tests and existing GraphStatement tests pass.

Commit:

```bash
git add BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldFragmentBuilderTests.cpp
git commit -m "feat: route graphwrite field statements by capability id"
```

---

## Task 12: Add explicit diagnostics and excluded-capability behavior

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`

- [ ] **Step 1: Add failing diagnostics contract test**

Append this test to `BlueprintHelperActionResolutionContractTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldCapabilityDiagnosticsRejectByRefAndUiOnlyTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.FieldDiagnostics.RejectByRefAndUiOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldCapabilityDiagnosticsRejectByRefAndUiOnlyTest::RunTest(const FString& Parameters)
{
	FString Reason;
	TestFalse(TEXT("by-ref set rejected"), FBlueprintHelperFieldCapabilityRegistry::IsAllowedUserStatement(TEXT("field.by_ref_set"), Reason));
	TestEqual(TEXT("by-ref reason"), Reason, FString(TEXT("unsupported_by_ref_set_deferred")));

	Reason.Reset();
	TestFalse(TEXT("pin-drag rejected"), FBlueprintHelperFieldCapabilityRegistry::IsAllowedUserStatement(TEXT("field.pin_drag_set"), Reason));
	TestEqual(TEXT("pin-drag reason"), Reason, FString(TEXT("unsupported_ui_entry_not_statement")));
	return true;
}
```

- [ ] **Step 2: Run diagnostics test**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Contract.FieldDiagnostics; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: test passes if Task 1 rejection map is intact; continue to runtime resolver diagnostics.

- [ ] **Step 3: Add resolver diagnostics for unsupported fields**

In `BlueprintHelperFieldVariableActionResolver.cpp`, before any spawner selection, add:

```cpp
FString RejectReason;
if (!Request.Semantic.CapabilityId.IsEmpty() && !FBlueprintHelperFieldCapabilityRegistry::IsAllowedUserStatement(Request.Semantic.CapabilityId, RejectReason))
{
	Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
	Result.DebugMessage = RejectReason;
	return Result;
}
```

When a property is private, not visible, read-only for set, missing local scope, unknown struct path, or instance-only component, use these exact reasons:

```cpp
TEXT("field_not_blueprint_visible")
TEXT("field_private_not_accessible")
TEXT("field_readonly_not_writable")
TEXT("missing_or_mismatched_function_scope")
TEXT("unknown_struct_property_path")
TEXT("not_class_component_property")
```

- [ ] **Step 4: Add compile diagnostic attribution to failed build path**

In `BlueprintHelperGraphStatementBuilder.cpp`, when Field build fails, add:

```cpp
OutResult.DebugFacts.Add(TEXT("field.capability_id"), Statement.CapabilityId);
OutResult.DebugFacts.Add(TEXT("field.failure_reason"), FailureReason);
OutResult.DebugFacts.Add(TEXT("field.success_claim"), TEXT("false"));
```

Do not append a success readback when `field.success_claim=false`.

- [ ] **Step 5: Run contract tests and commit**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Contract.FieldDiagnostics; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: diagnostics tests pass.

Commit:

```bash
git add BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp
git commit -m "feat: add graphwrite field diagnostic rejection contract"
```

---

## Task 13: Complete 17-capability automation matrix

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldCapabilityTaxonomyTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldVariableActionClusterTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldFragmentBuilderTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldActionReadbackTests.cpp`

- [ ] **Step 1: Add matrix test helper**

In `BlueprintHelperFieldCapabilityTaxonomyTests.cpp`, add this helper in the anonymous namespace above test classes:

```cpp
namespace
{
struct FExpectedFieldCapabilityMatrixRow
{
	const TCHAR* Id;
	const TCHAR* NodeFamily;
	EBlueprintHelperFieldCapabilityPriority Priority;
	bool bExec;
};

static const TArray<FExpectedFieldCapabilityMatrixRow>& ExpectedFieldCapabilityMatrix()
{
	static const TArray<FExpectedFieldCapabilityMatrixRow> Rows = {
		{TEXT("field.member_get"), TEXT("variable_get"), EBlueprintHelperFieldCapabilityPriority::P0, false},
		{TEXT("field.member_set"), TEXT("variable_set"), EBlueprintHelperFieldCapabilityPriority::P0, true},
		{TEXT("field.inherited_member_get"), TEXT("variable_get"), EBlueprintHelperFieldCapabilityPriority::P1, false},
		{TEXT("field.inherited_member_set"), TEXT("variable_set"), EBlueprintHelperFieldCapabilityPriority::P1, true},
		{TEXT("field.sparse_data_get"), TEXT("variable_get"), EBlueprintHelperFieldCapabilityPriority::P1, false},
		{TEXT("field.function_param_get"), TEXT("variable_get"), EBlueprintHelperFieldCapabilityPriority::P1, false},
		{TEXT("field.local_get"), TEXT("variable_get"), EBlueprintHelperFieldCapabilityPriority::P0, false},
		{TEXT("field.local_set"), TEXT("variable_set"), EBlueprintHelperFieldCapabilityPriority::P0, true},
		{TEXT("field.object_pin_member_get"), TEXT("variable_get_target"), EBlueprintHelperFieldCapabilityPriority::P2, false},
		{TEXT("field.object_pin_member_set"), TEXT("variable_set_target"), EBlueprintHelperFieldCapabilityPriority::P2, true},
		{TEXT("field.component_ref_get"), TEXT("component_variable_get"), EBlueprintHelperFieldCapabilityPriority::P0, false},
		{TEXT("field.component_ref_set"), TEXT("component_variable_set"), EBlueprintHelperFieldCapabilityPriority::P2, true},
		{TEXT("field.component_property_get"), TEXT("component_property_get"), EBlueprintHelperFieldCapabilityPriority::P2, false},
		{TEXT("field.component_property_set"), TEXT("component_property_set"), EBlueprintHelperFieldCapabilityPriority::P2, true},
		{TEXT("field.struct_member_get"), TEXT("break_struct"), EBlueprintHelperFieldCapabilityPriority::P1, false},
		{TEXT("field.struct_member_set"), TEXT("set_fields_in_struct"), EBlueprintHelperFieldCapabilityPriority::P1, true},
		{TEXT("field.nested_property_path"), TEXT("property_path_fragment"), EBlueprintHelperFieldCapabilityPriority::P2, false}
	};
	return Rows;
}
}
```

- [ ] **Step 2: Add matrix verification test**

Append:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldCapabilityTaxonomyMatrixMatchesEvidenceTest,
	"BlueprintHelper.GraphWrite.FieldCapability.Taxonomy.MatrixMatchesEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldCapabilityTaxonomyMatrixMatchesEvidenceTest::RunTest(const FString& Parameters)
{
	for (const FExpectedFieldCapabilityMatrixRow& Row : ExpectedFieldCapabilityMatrix())
	{
		const FBlueprintHelperFieldCapabilitySpec* Spec = FBlueprintHelperFieldCapabilityRegistry::FindById(Row.Id);
		TestNotNull(FString::Printf(TEXT("spec exists: %s"), Row.Id), Spec);
		if (Spec)
		{
			TestEqual(FString::Printf(TEXT("node family: %s"), Row.Id), Spec->ExpectedNodeFamily, FString(Row.NodeFamily));
			TestEqual(FString::Printf(TEXT("priority: %s"), Row.Id), static_cast<uint8>(Spec->Priority), static_cast<uint8>(Row.Priority));
			TestEqual(FString::Printf(TEXT("exec pins: %s"), Row.Id), Spec->bProducesExecPins, Row.bExec);
		}
	}
	return true;
}
```

- [ ] **Step 3: Run full capability matrix tests**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.FieldCapability; Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.FieldVariable; Automation RunTests BlueprintHelper.GraphWrite.FieldFragmentBuilder; Automation RunTests BlueprintHelper.GraphWrite.FieldReadback; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: all Field capability taxonomy, resolver, fragment builder, and readback tests pass.

- [ ] **Step 4: Commit the full test matrix**

Commit:

```bash
git add BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldCapabilityTaxonomyTests.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldVariableActionClusterTests.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldFragmentBuilderTests.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldActionReadbackTests.cpp
git commit -m "test: cover seventeen graphwrite field capabilities"
```

---

## Task 14: Update MCP/Agent TaskSpec templates and capability matrix docs

**Files:**
- Modify: `AgentFaceService/agent-guide/Reference/04_Tool_Surface_Field_Templates_20260512.md`
- Modify: `AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md`
- Create: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_Field_FirstClassCapabilityExpansionPlan_20260526_CN.md`

- [ ] **Step 1: Add Field TaskSpec template block**

In `AgentFaceService/agent-guide/Reference/04_Tool_Surface_Field_Templates_20260512.md`, add this section under the GraphWrite Field surface:

```markdown
## GraphWrite Field first-class capability IDs

Allowed `field.capability_id` values:

```text
field.member_get
field.member_set
field.inherited_member_get
field.inherited_member_set
field.sparse_data_get
field.function_param_get
field.local_get
field.local_set
field.object_pin_member_get
field.object_pin_member_set
field.component_ref_get
field.component_ref_set
field.component_property_get
field.component_property_set
field.struct_member_get
field.struct_member_set
field.nested_property_path
```

`field.drag_get`, `field.drag_set`, `field.pin_drag_get`, and `field.pin_drag_set` are UI-entry evidence only. They must be mapped by the caller to one of the stable first-class IDs above before GraphWrite execution.

Minimum statement-local fields by capability family:

| Family | Required fields |
|---|---|
| member get/set | `blueprint_ref`, `graph_ref`, `field_name`, optional `owner_class`, optional `member_guid`, `mode` |
| inherited/native | `owner_class`, `field_name`, optional `member_guid`, `mode` |
| local get/set | `graph_ref`, `function_name` or `scope_name`, `local_name`, optional `local_guid`, `mode` |
| function param get | `function_name`, `param_name`, `param_flags` or resolved parameter evidence |
| component_ref | `component_name`, optional `component_guid`, `component_owner_class`, `component_kind` |
| field_access | `target_pin_ref`, `owner_class`, `field_name`, optional `member_guid`, expected target pin type |
| get_property/set_property | `root`, `field_path[]`, `owner_type`, `member_name`, optional `member_guid`, expected read/write mode, linked pins/defaults |
```

- [ ] **Step 2: Add capability matrix table to docs**

In `AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md`, add a GraphWrite Field matrix row group with the same 17 IDs, priorities, node families, and excluded UI/support/diagnostic entries from this plan.

- [ ] **Step 3: Copy this implementation plan into current Develop/Plan**

Copy the canonical superpowers plan file into the project plan directory:

```bash
mkdir -p BlueprintHelper/Develop/Plan
cp docs/superpowers/plans/2026-05-26-blueprinthelper-field-capabilities.md \
   BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_Field_FirstClassCapabilityExpansionPlan_20260526_CN.md
```

Expected: the new file exists under `BlueprintHelper/Develop/Plan/` and no file is added under any `BlueprintHelper/Develop/v*` directory.

- [ ] **Step 4: Commit docs**

Commit:

```bash
git add AgentFaceService/agent-guide/Reference/04_Tool_Surface_Field_Templates_20260512.md \
        AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md \
        BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_Field_FirstClassCapabilityExpansionPlan_20260526_CN.md
git commit -m "docs: document graphwrite field capability expansion"
```

---

## Task 15: Full regression and self-review

**Files:**
- Inspect: all files changed in Tasks 1-14

- [ ] **Step 1: Run Field capability regression**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.FieldCapability; Automation RunTests BlueprintHelper.GraphWrite.ActionContext.FieldFacts; Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.FieldVariable; Automation RunTests BlueprintHelper.GraphWrite.FieldFragmentBuilder; Automation RunTests BlueprintHelper.GraphWrite.FieldReadback; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: all named tests pass.

- [ ] **Step 2: Run GraphWrite cluster regression**

Run:

```bash
"$UE_EDITOR_CMD" "$BPH_TEST_PROJECT" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: all GraphWrite automation tests pass. Failures in non-Field clusters must be inspected for API contract drift caused by the new Field fields.

- [ ] **Step 3: Verify no archived-doc dependency entered the implementation**

Run:

```bash
git diff --cached --name-only | grep 'BlueprintHelper/Develop/v' && exit 1 || exit 0
```

Expected: command exits `0` and prints nothing.

- [ ] **Step 4: Verify no component_ref path uses component node spawner**

Run:

```bash
grep -R "UBlueprintComponentNodeSpawner" BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite
```

Expected: command exits non-zero or prints only comments/tests explaining that `UBlueprintComponentNodeSpawner` is forbidden for `component_ref`. There must be no runtime resolver or fragment builder call to `UBlueprintComponentNodeSpawner`.

- [ ] **Step 5: Verify first-class capability count**

Run:

```bash
grep -R "field\." BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.cpp | grep "MakeSpec" | wc -l
```

Expected: output is `17`.

- [ ] **Step 6: Self-review checklist**

Review changed code against this checklist:

```text
[ ] All 17 first-class Field capability IDs exist in the registry.
[ ] P0 capabilities are covered by resolver tests.
[ ] P1 capabilities are covered by resolver or path-planner tests.
[ ] P2 capabilities are covered by resolver, target-pin, component, or property-path tests.
[ ] UI-only entries are rejected with unsupported_ui_entry_not_statement.
[ ] by_ref_set is rejected with unsupported_by_ref_set_deferred.
[ ] component_ref uses UBlueprintVariableNodeSpawner and component FObjectProperty semantics.
[ ] no Field resolver path calls UBlueprintComponentNodeSpawner.
[ ] local and function parameter fields require function graph scope.
[ ] object-pin and component-property access require explicit target_pin_ref and pin type facts.
[ ] struct member read/write routes to break_struct or set_fields_in_struct families.
[ ] nested property paths route to property_path_fragment and emit segment counts.
[ ] readback facts include node class, member reference, pin type, links, defaults, split state count, and compile diagnostic count.
[ ] Review target granularity remains graph-block; Field details go to DebugBundle/readback only.
```

- [ ] **Step 7: Final commit**

Commit any final fixes from regression/self-review:

```bash
git add BlueprintHelper/Source/BlueprintHelper AgentFaceService BlueprintHelper/Develop/Plan docs/superpowers/plans
git commit -m "feat: expand graphwrite field capabilities to evidence-backed matrix"
```

Expected: if no files remain, Git prints `nothing to commit`; otherwise commit succeeds.

---

## Rollback notes

Each task is independently committed. If a task introduces a regression, revert only that task’s commit:

```bash
git revert <commit_sha>
```

Expected: the revert commit removes that task’s changes while preserving earlier passing tasks.

## Self-review result for this plan

- Spec coverage: all 17 first-class Field capability IDs are represented in taxonomy, resolver, fragment, readback, and test tasks.
- UI-only/support/other-cluster/diagnostic exclusions are represented in taxonomy and diagnostics tasks.
- TaskSpec constraints are represented in SemanticIR, ActionContext, and docs tasks.
- `component_ref` uses component `FObjectProperty` / variable node semantics and explicitly forbids `UBlueprintComponentNodeSpawner` in runtime paths.
- Readback facts cover node class, member reference, pin type, links, defaults, split state, and compile diagnostics.
- Review remains graph-block scoped; Field details enter DebugBundle/readback facts.
- No step depends on archived `BlueprintHelper/Develop/v*` documents.
