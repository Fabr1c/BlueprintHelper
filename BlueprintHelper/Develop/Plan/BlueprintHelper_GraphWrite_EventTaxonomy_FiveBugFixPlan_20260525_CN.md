# GraphWrite Event Taxonomy Five Bug Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Repository rule overrides the generic skill template: do not run `git add`, `git commit`, or `git push`; each task ends with a manual checkpoint note only.

**Goal:** 修复 EVTAX-001 到 EVTAX-005 的 5 个 event taxonomy / evidence 边界 bug，并保持 BlueprintSignature、GraphWrite、EventDelegate、Review evidence 的职责边界清晰。

**Architecture:** BlueprintSignature 继续拥有 custom/override/native event declaration 与 handler declaration/signature lifecycle；GraphWrite 只消费 Signature 产出的 event reference / declaration evidence 并写 body/use-site；EventDelegate 只消费 ActionContext projection 后的 delegate/component-bound use-site evidence。共享语义通过私有 GraphWrite event reference utility 和 ActionContext projection 传递，不新增硬编码分支、不新增 GraphWrite declaration kind、不把 EventDelegate 变成 handler 查找器。

**Tech Stack:** Unreal Engine 5.6 C++ plugin, BlueprintHelper GraphWrite SemanticIR, ActionContext pipeline, Automation Tests, PowerShell verification.

## Execution Status

- Status: completed in this implementation pass.
- Scope: EVTAX-001 through EVTAX-005 implemented without adding GraphWrite event declaration ownership or EventDelegate handler scan fallback.
- Verification:
  - `Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload` passed.
  - Focused automation passed for `BlueprintHelper.GraphWrite.EventTaxonomy.Replace`, `BlueprintHelper.GraphWrite.ToolResultBase.ReplaceGraphScopeEntrySelectorUnsupported`, `BlueprintHelper.GraphWrite.ToolResultBase.ReplaceBlockedDryRunErrorEnvelope`, `BlueprintHelper.GraphWrite.EventTaxonomy`, `BlueprintHelper.GraphWrite.ActionContext.EventDelegate`, `BlueprintHelper.GraphWrite.ActionResolution.EventDelegate`, `BlueprintHelper.GraphWrite.ActionResolution.Contract.EventDelegateNoCustomEventScan`, `BlueprintHelper.GraphWrite.LegacyMainline.NoPublicParsedNodeGraphWriteApi`, and `BlueprintHelper.GraphWrite.LegacyMainline.ActiveGraphWriteSourceLegacyTokenGate`.
  - Source scans passed for EventDelegate custom-event/Ubergraph scan removal, Replace service entry node type routing removal, GraphWrite entry parsed-node residue removal, and legacy event declaration token absence.
  - Final read-only subagent review returned PASS for the latest diff.

---

## Non-Negotiable Architecture Gates

- 不新增 `EBlueprintHelperGraphStatementKind::CustomEvent` / `Event` declaration kind；custom/override/native declaration 仍归 BlueprintSignature。
- 不新增 EventDelegate 对 `ensure_custom_event`、`ensure_override_event`、`native_event` 的 ownership。
- 不在 GraphWrite/EventDelegate resolver 内扫描资产来修复缺失 Signature evidence。
- 不按事件名硬编码 `ReceiveBeginPlay`、`ReceiveTick` 等 native 映射；native/override taxonomy 只能来自 BlueprintSignature evidence。
- 不通过 private parsed-node creation fast path 解决本批问题；新写入路径继续走 SemanticIR -> ActionContext -> ActionResolution -> FragmentDAG -> Mutation。

## File Structure

- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphEventReferenceUtils.h`
  - 私有共享 event reference / taxonomy / dependency evidence utility。
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphEventReferenceUtils.cpp`
  - 解析 `logic_spec.entry` 和 metadata，写入 `event_taxonomy`、`source_cluster`、`signature_evidence_id`。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
  - 增加 `FBlueprintHelperGraphEntryIR`，不增加新的 statement kind。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
  - 解析 `logic_spec.entry` 到 `SemanticIR.Entry`。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDagBuilder.cpp`
  - 从 `SemanticIR.Entry` 把 event reference metadata 投影到 FragmentDAG。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp`
  - custom event entry 必须有 Signature evidence；dry-run temporary entry 也只能在 evidence 存在时创建。
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceEntryResolver.h`
  - Replace scope-specific entry resolver。
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceEntryResolver.cpp`
  - `custom_event_body` 只匹配 `UK2Node_CustomEvent`，`event_body` 只匹配 `UK2Node_Event`，`function_body` 只匹配 `UK2Node_FunctionEntry`。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.cpp`
  - 移除本地模糊 `NodeMatchesEntryName()`，改用 scoped resolver。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.h`
  - 给 review scope 增加可序列化 `EventTaxonomy` 字段，UI scope kind 仍保持 generic `Event`。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.cpp`
  - 输出 `event_taxonomy`。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentEvidenceUtils.cpp`
  - 读取新主字段 `event_taxonomy`，旧 `{ event_name, event, custom_event_name }` 只作为 event name 兼容读取。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h`
  - 为 handler projection 增加 `HandlerFunctionPath`、`HandlerSourceCluster`、`SignatureEvidenceId`。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
  - 从 statement/expression `context_evidence` 复制 handler dependency evidence。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp`
  - 投影 `handler_function_path`、`handler_source_cluster`、`signature_evidence_id` 到 EventDelegate request evidence。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h`
  - 增加 handler function path / source cluster / signature evidence fields。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.cpp`
  - 删除 `UK2Node_CustomEvent` fallback scan，只按 projected `handler_function_path` 解析。
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphEventTaxonomyTests.cpp`
  - 新增 5 个 bug 的 focused automation tests。
- Modify Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`
  - 覆盖 Signature handler evidence -> ActionContext projection。
- Modify Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateActionClusterTests.cpp`
  - 更新 positive helper，使 handler evidence 携带 `handler_function_path`。
- Modify Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`
  - 增加禁止 EventDelegate resolver 扫描 `UK2Node_CustomEvent` 的 source contract。

---

### Task 1: BUG-001 custom_event declaration evidence gate

**Bug:** `logic_spec.entry.kind=custom_event` 可被 GraphWrite 当作 entry 使用，但缺少强制 Signature declaration evidence gate，容易看起来像 GraphWrite 创建/拥有 custom event declaration。

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphEventReferenceUtils.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphEventReferenceUtils.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphEventTaxonomyTests.cpp`

- [ ] **Step 1: Write failing tests for custom_event without Signature evidence**

Add these tests to `BlueprintHelperGraphEventTaxonomyTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphEventTaxonomyCustomEventRequiresSignatureEvidenceTest,
	"BlueprintHelper.GraphWrite.EventTaxonomy.CustomEvent.RequiresSignatureEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphEventTaxonomyCustomEventRequiresSignatureEvidenceTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"JSON({
		"schema": "BlueprintLogicSpec.v2",
		"entry": {
			"kind": "custom_event",
			"name": "HandleOpened",
			"event_taxonomy": "custom_event",
			"source_cluster": "BlueprintSignature"
		},
		"statements": [
			{ "id": "stmt_print", "kind": "call", "target": "PrintString" }
		]
	})JSON");

	TSharedPtr<FJsonObject> LogicSpec;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("json parses"), FJsonSerializer::Deserialize(Reader, LogicSpec));

	FBlueprintHelperGraphSemanticIR SemanticIR;
	FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(LogicSpec, SemanticIR);

	TestEqual(TEXT("entry kind"), SemanticIR.Entry.Kind, FString(TEXT("custom_event")));
	TestEqual(TEXT("entry taxonomy"), SemanticIR.Entry.EventTaxonomy, FString(TEXT("custom_event")));
	TestTrue(TEXT("missing signature evidence is diagnosed"), SemanticIR.Diagnostics.ContainsByPredicate(
		[](const FBlueprintHelperGraphSemanticDiagnostic& Diagnostic)
		{
			return Diagnostic.Code == TEXT("custom_event_signature_evidence_missing");
		}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphEventTaxonomyCustomEventAcceptsSignatureEvidenceTest,
	"BlueprintHelper.GraphWrite.EventTaxonomy.CustomEvent.AcceptsSignatureEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphEventTaxonomyCustomEventAcceptsSignatureEvidenceTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"JSON({
		"schema": "BlueprintLogicSpec.v2",
		"entry": {
			"kind": "custom_event",
			"name": "HandleOpened",
			"event_taxonomy": "custom_event",
			"source_cluster": "BlueprintSignature",
			"signature_evidence_id": "signature:custom_event:HandleOpened"
		},
		"statements": [
			{ "id": "stmt_print", "kind": "call", "target": "PrintString" }
		]
	})JSON");

	TSharedPtr<FJsonObject> LogicSpec;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("json parses"), FJsonSerializer::Deserialize(Reader, LogicSpec));

	FBlueprintHelperGraphSemanticIR SemanticIR;
	FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(LogicSpec, SemanticIR);

	TestEqual(TEXT("entry name"), SemanticIR.Entry.Name, FString(TEXT("HandleOpened")));
	TestEqual(TEXT("entry signature evidence"), SemanticIR.Entry.SignatureEvidenceId, FString(TEXT("signature:custom_event:HandleOpened")));
	TestFalse(TEXT("no signature evidence missing diagnostic"), SemanticIR.Diagnostics.ContainsByPredicate(
		[](const FBlueprintHelperGraphSemanticDiagnostic& Diagnostic)
		{
			return Diagnostic.Code == TEXT("custom_event_signature_evidence_missing");
		}));
	return true;
}
```

- [ ] **Step 2: Run the new tests and verify they fail**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -unattended -nop4 -nosplash -nullrhi -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.EventTaxonomy.CustomEvent;Quit"
```

Expected before implementation: compile failure for `SemanticIR.Entry` or test failure because no `custom_event_signature_evidence_missing` diagnostic exists.

- [ ] **Step 3: Add private event reference utility**

Create `BlueprintHelperGraphEventReferenceUtils.h` with this exact shape:

```cpp
#pragma once

#include "CoreMinimal.h"

class FJsonObject;

enum class EBlueprintHelperGraphEventTaxonomy : uint8
{
	Unknown,
	CustomEvent,
	NativeEvent,
	OverrideEvent
};

struct FBlueprintHelperGraphEventReference
{
	FString Kind;
	FString Name;
	FString GraphName;
	EBlueprintHelperGraphEventTaxonomy Taxonomy = EBlueprintHelperGraphEventTaxonomy::Unknown;
	FString SourceCluster;
	FString SignatureEvidenceId;
	TMap<FString, FString> Metadata;

	bool HasSignatureEvidence() const
	{
		return !SourceCluster.TrimStartAndEnd().IsEmpty()
			&& !SignatureEvidenceId.TrimStartAndEnd().IsEmpty();
	}
};

class FBlueprintHelperGraphEventReferenceUtils
{
public:
	static FString TaxonomyToString(EBlueprintHelperGraphEventTaxonomy Taxonomy);
	static EBlueprintHelperGraphEventTaxonomy ParseTaxonomy(const FString& Value);
	static bool TryReadEntryReference(const TSharedPtr<FJsonObject>& EntryObject, FBlueprintHelperGraphEventReference& OutReference);
	static void WriteMetadata(const FBlueprintHelperGraphEventReference& Reference, TMap<FString, FString>& OutMetadata);
	static bool IsSignatureOwnedTaxonomy(EBlueprintHelperGraphEventTaxonomy Taxonomy);
};
```

Implement `TryReadEntryReference()` so it reads only data fields from `entry`: `kind`, `name`, `graph`, `event_taxonomy`, `source_cluster`, `signature_evidence_id`, plus `context_evidence`. It must not inspect Blueprint assets or infer native event names.

- [ ] **Step 4: Parse entry into SemanticIR without adding a statement kind**

Add this struct to `BlueprintHelperGraphSemanticIR.h`:

```cpp
struct BLUEPRINTHELPER_API FBlueprintHelperGraphEntryIR
{
	FString Kind;
	FString Name;
	FString GraphName;
	FString EventTaxonomy;
	FString SourceCluster;
	FString SignatureEvidenceId;
	TMap<FString, FString> ContextEvidence;

	bool IsEmpty() const
	{
		return Kind.TrimStartAndEnd().IsEmpty()
			&& Name.TrimStartAndEnd().IsEmpty();
	}
};
```

Add `FBlueprintHelperGraphEntryIR Entry;` to `FBlueprintHelperGraphSemanticIR`.

In `BuildFromLogicSpec()`, before parsing statements, read `logic_spec.entry` and add this diagnostic for custom event entries:

```cpp
if (OutIR.Entry.Kind.Equals(TEXT("custom_event"), ESearchCase::IgnoreCase)
	&& OutIR.Entry.SignatureEvidenceId.TrimStartAndEnd().IsEmpty())
{
	FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(
		OutIR,
		TEXT("custom_event_signature_evidence_missing"),
		TEXT("$.entry.signature_evidence_id"),
		TEXT("custom_event entry requires BlueprintSignature signature_evidence_id; GraphWrite only writes the body/use-site."));
}
```

- [ ] **Step 5: Enforce the same gate in the generation pipeline**

Replace local `HasSignatureDependencyEntryFact()` usage in `BlueprintGraphGenerationPipeline.cpp` with `FBlueprintHelperGraphEventReferenceUtils::TryReadEntryReference()`. The entry path must only create the dry-run temporary custom event node when `EntryRef.Taxonomy == CustomEvent` and `EntryRef.HasSignatureEvidence()` is true:

```cpp
FBlueprintHelperGraphEventReference EntryRef;
FBlueprintHelperGraphEventReferenceUtils::TryReadEntryReference(*EntryObject, EntryRef);
if (IsDryRunPayload(JsonObject)
	&& EntryRef.Taxonomy == EBlueprintHelperGraphEventTaxonomy::CustomEvent
	&& EntryRef.HasSignatureEvidence())
{
	EntryNode = CreateDryRunSignatureDependencyCustomEventNode(TargetGraph, EntryRef.Name);
}
else
{
	AddSemanticUnresolved(
		OutUnresolvedNodes,
		EntryId,
		TEXT("custom_event entry requires BlueprintSignature signature_evidence_id before GraphWrite can write the body/use-site."));
}
```

- [ ] **Step 6: Run focused verification**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -unattended -nop4 -nosplash -nullrhi -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.EventTaxonomy.CustomEvent;Quit"
```

Expected after implementation: both custom event tests pass.

Manual checkpoint message:

```text
修复内容：
1. 为 GraphWrite custom_event entry 增加 Signature evidence gate
```

---

### Task 2: BUG-002 override/native event taxonomy reference preservation

**Bug:** `UK2Node_Event` / event body evidence 会退化成 generic `Event`，GraphWrite 无法区分 `native_event` 与 `override_event`；但 declaration ownership 不能迁出 BlueprintSignature。

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphEventReferenceUtils.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDagBuilder.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphEventTaxonomyTests.cpp`

- [ ] **Step 1: Write failing tests for native/override taxonomy metadata**

Add:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphEventTaxonomyOverrideEntryPreservesReferenceTest,
	"BlueprintHelper.GraphWrite.EventTaxonomy.OverrideEntry.PreservesReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphEventTaxonomyOverrideEntryPreservesReferenceTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"JSON({
		"schema": "BlueprintLogicSpec.v2",
		"entry": {
			"kind": "event",
			"name": "ReceiveBeginPlay",
			"event_taxonomy": "override_event",
			"source_cluster": "BlueprintSignature",
			"signature_evidence_id": "signature:override_event:ReceiveBeginPlay"
		},
		"statements": [
			{ "id": "stmt_print", "kind": "call", "target": "PrintString" }
		]
	})JSON");

	TSharedPtr<FJsonObject> LogicSpec;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("json parses"), FJsonSerializer::Deserialize(Reader, LogicSpec));

	FBlueprintHelperGraphSemanticIR SemanticIR;
	FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(LogicSpec, SemanticIR);
	FBlueprintHelperGraphFragmentDag Dag;
	FBlueprintHelperGraphFragmentDagBuilder::BuildFromSemanticIR(SemanticIR, Dag);

	TestEqual(TEXT("event name metadata"), Dag.Metadata.FindRef(TEXT("event_name")), FString(TEXT("ReceiveBeginPlay")));
	TestEqual(TEXT("event taxonomy metadata"), Dag.Metadata.FindRef(TEXT("event_taxonomy")), FString(TEXT("override_event")));
	TestEqual(TEXT("source cluster metadata"), Dag.Metadata.FindRef(TEXT("source_cluster")), FString(TEXT("BlueprintSignature")));
	TestEqual(TEXT("signature evidence id metadata"), Dag.Metadata.FindRef(TEXT("signature_evidence_id")), FString(TEXT("signature:override_event:ReceiveBeginPlay")));
	return true;
}
```

- [ ] **Step 2: Run the taxonomy test and verify it fails**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -unattended -nop4 -nosplash -nullrhi -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.EventTaxonomy.OverrideEntry;Quit"
```

Expected before implementation: `Dag.Metadata` lacks `event_taxonomy` / `signature_evidence_id`.

- [ ] **Step 3: Project entry reference into FragmentDAG metadata**

In `BlueprintHelperGraphFragmentDagBuilder.cpp`, after base metadata is added, write:

```cpp
if (!SemanticIR.Entry.IsEmpty())
{
	FBlueprintHelperGraphEventReference EntryRef;
	EntryRef.Kind = SemanticIR.Entry.Kind;
	EntryRef.Name = SemanticIR.Entry.Name;
	EntryRef.GraphName = SemanticIR.Entry.GraphName;
	EntryRef.Taxonomy = FBlueprintHelperGraphEventReferenceUtils::ParseTaxonomy(SemanticIR.Entry.EventTaxonomy);
	EntryRef.SourceCluster = SemanticIR.Entry.SourceCluster;
	EntryRef.SignatureEvidenceId = SemanticIR.Entry.SignatureEvidenceId;
	FBlueprintHelperGraphEventReferenceUtils::WriteMetadata(EntryRef, OutDag.Metadata);
}
```

Do not infer taxonomy from `ReceiveBeginPlay`. If `event_taxonomy` is missing, write no taxonomy and let the diagnostic/test catch the missing evidence.

- [ ] **Step 4: Add native/override evidence validation**

In SemanticIR entry parsing, require `source_cluster` and `signature_evidence_id` for `native_event` and `override_event`:

```cpp
const bool bSignatureOwnedEvent =
	OutIR.Entry.EventTaxonomy.Equals(TEXT("native_event"), ESearchCase::IgnoreCase)
	|| OutIR.Entry.EventTaxonomy.Equals(TEXT("override_event"), ESearchCase::IgnoreCase);
if (bSignatureOwnedEvent && OutIR.Entry.SignatureEvidenceId.TrimStartAndEnd().IsEmpty())
{
	FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(
		OutIR,
		TEXT("event_signature_evidence_missing"),
		TEXT("$.entry.signature_evidence_id"),
		TEXT("native/override event entry requires BlueprintSignature signature_evidence_id; GraphWrite only writes the body/use-site."));
}
```

- [ ] **Step 5: Run focused verification**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -unattended -nop4 -nosplash -nullrhi -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.EventTaxonomy.OverrideEntry;Quit"
```

Expected after implementation: override entry metadata test passes and no GraphWrite declaration kind was added.

Manual checkpoint message:

```text
修复内容：
1. 保留 native/override event 的 Signature-owned taxonomy evidence
```

---

### Task 3: BUG-003 Replace entry resolver scope split

**Bug:** Replace path 的 entry matching 同时接受 `UK2Node_CustomEvent`、`UK2Node_Event`、`UK2Node_FunctionEntry` 和 node title/name fallback，导致 `custom_event_body` / `event_body` / `graph` 语义边界偏弱。

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceEntryResolver.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceEntryResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphEventTaxonomyTests.cpp`

- [ ] **Step 1: Write failing resolver tests**

Add source-level tests that construct a small transient graph with one `UK2Node_CustomEvent`, one `UK2Node_Event`, and one `UK2Node_FunctionEntry`, then assert scoped matching:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphEventTaxonomyReplaceScopeResolverTest,
	"BlueprintHelper.GraphWrite.EventTaxonomy.Replace.ScopeResolver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphEventTaxonomyReplaceScopeResolverTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReplaceEntryResolveRequest CustomRequest;
	CustomRequest.Scope = EBlueprintHelperReplaceScope::CustomEventBody;
	CustomRequest.EntryName = TEXT("HandleOpened");
	TestTrue(TEXT("custom_event_body accepts custom event class"),
		FBlueprintHelperReplaceEntryResolver::MatchesEntryClass(CustomRequest, UK2Node_CustomEvent::StaticClass()));
	TestFalse(TEXT("custom_event_body rejects native event class"),
		FBlueprintHelperReplaceEntryResolver::MatchesEntryClass(CustomRequest, UK2Node_Event::StaticClass()));
	TestFalse(TEXT("custom_event_body rejects function entry class"),
		FBlueprintHelperReplaceEntryResolver::MatchesEntryClass(CustomRequest, UK2Node_FunctionEntry::StaticClass()));

	FBlueprintHelperReplaceEntryResolveRequest EventRequest;
	EventRequest.Scope = EBlueprintHelperReplaceScope::EventBody;
	EventRequest.EntryName = TEXT("ReceiveBeginPlay");
	TestTrue(TEXT("event_body accepts event class"),
		FBlueprintHelperReplaceEntryResolver::MatchesEntryClass(EventRequest, UK2Node_Event::StaticClass()));
	TestFalse(TEXT("event_body rejects custom event class"),
		FBlueprintHelperReplaceEntryResolver::MatchesEntryClass(EventRequest, UK2Node_CustomEvent::StaticClass()));

	FBlueprintHelperReplaceEntryResolveRequest GraphRequest;
	GraphRequest.Scope = EBlueprintHelperReplaceScope::Graph;
	GraphRequest.EntryName = TEXT("ReceiveBeginPlay");
	TestFalse(TEXT("graph scope does not claim entry body nodes"),
		FBlueprintHelperReplaceEntryResolver::MatchesEntryClass(GraphRequest, UK2Node_Event::StaticClass()));
	return true;
}
```

- [ ] **Step 2: Run resolver tests and verify they fail**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -unattended -nop4 -nosplash -nullrhi -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.EventTaxonomy.Replace.ScopeResolver;Quit"
```

Expected before implementation: compile failure for `FBlueprintHelperReplaceEntryResolver`.

- [ ] **Step 3: Create scoped resolver**

Implement the public test seam and production matcher:

```cpp
struct FBlueprintHelperReplaceEntryResolveRequest
{
	EBlueprintHelperReplaceScope Scope = EBlueprintHelperReplaceScope::Graph;
	FString EntryName;
	FString EventTaxonomy;
	FString SignatureEvidenceId;
};

class FBlueprintHelperReplaceEntryResolver
{
public:
	static bool MatchesEntryClass(const FBlueprintHelperReplaceEntryResolveRequest& Request, const UClass* NodeClass);
	static bool NodeMatchesEntry(const FBlueprintHelperReplaceEntryResolveRequest& Request, UEdGraphNode* Node);
};
```

Core behavior:

```cpp
bool FBlueprintHelperReplaceEntryResolver::MatchesEntryClass(
	const FBlueprintHelperReplaceEntryResolveRequest& Request,
	const UClass* NodeClass)
{
	if (!NodeClass)
	{
		return false;
	}
	if (Request.Scope == EBlueprintHelperReplaceScope::CustomEventBody)
	{
		return NodeClass->IsChildOf(UK2Node_CustomEvent::StaticClass());
	}
	if (Request.Scope == EBlueprintHelperReplaceScope::EventBody)
	{
		return NodeClass->IsChildOf(UK2Node_Event::StaticClass());
	}
	if (Request.Scope == EBlueprintHelperReplaceScope::FunctionBody)
	{
		return NodeClass->IsChildOf(UK2Node_FunctionEntry::StaticClass());
	}
	return false;
}
```

`NodeMatchesEntry()` must use node-type-specific name extraction only:
- `UK2Node_CustomEvent::CustomFunctionName`
- `UK2Node_Event::GetFunctionName()` and `EventReference.GetMemberName()`
- `UK2Node_FunctionEntry::FunctionReference.GetMemberName()` and `CustomGeneratedFunctionName`

It must not compare `Node->GetName()` or `GetNodeTitle()` for event/function body scopes.

- [ ] **Step 4: Replace service uses scoped resolver**

In `BlueprintHelperReplaceBlueprintGraphService.cpp`, replace calls to local `NodeMatchesEntryName()` with:

```cpp
FBlueprintHelperReplaceEntryResolveRequest EntryResolveRequest;
EntryResolveRequest.Scope = Request.Scope;
EntryResolveRequest.EntryName = Request.EntryName;
EntryResolveRequest.EventTaxonomy = Request.EventTaxonomy;
EntryResolveRequest.SignatureEvidenceId = Request.SignatureEvidenceId;

if (FBlueprintHelperReplaceEntryResolver::NodeMatchesEntry(EntryResolveRequest, Node))
{
	...
}
```

For `Graph` scope, preserve broad graph-level replacement by deleting non-entry body nodes only when no `selector.entry_name` is present. If `selector.entry_name` is present with `replace_scope=graph`, return preflight conflict `graph_scope_entry_selector_unsupported`.

- [ ] **Step 5: Run focused verification**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -unattended -nop4 -nosplash -nullrhi -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.EventTaxonomy.Replace;Quit"
```

Expected after implementation: scoped resolver tests pass; existing replace lowering tests still pass.

Manual checkpoint message:

```text
修复内容：
1. 拆分 Replace entry resolver，避免 custom_event_body/event_body/graph 共用模糊匹配
```

---

### Task 4: BUG-004 Review evidence preserves event_taxonomy while keeping generic Event scope

**Bug:** Review evidence 把 custom/native/override event 都折叠成 generic `Event`，UI grouping 可以继续 generic，但 metadata 必须保留 taxonomy。

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentEvidenceUtils.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphEventTaxonomyTests.cpp`

- [ ] **Step 1: Write failing review evidence test**

Add:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphEventTaxonomyReviewEvidencePreservesTaxonomyTest,
	"BlueprintHelper.GraphWrite.EventTaxonomy.ReviewEvidence.PreservesTaxonomy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphEventTaxonomyReviewEvidencePreservesTaxonomyTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphFragmentDag Dag;
	Dag.Schema = TEXT("BlueprintHelperGraphFragmentDag.v1");
	Dag.Metadata.Add(TEXT("review_scope_kind"), TEXT("event"));
	Dag.Metadata.Add(TEXT("event_name"), TEXT("ReceiveBeginPlay"));
	Dag.Metadata.Add(TEXT("event_taxonomy"), TEXT("override_event"));
	Dag.Metadata.Add(TEXT("source_cluster"), TEXT("BlueprintSignature"));

	FBlueprintHelperGraphFragmentRef Fragment;
	Fragment.FragmentId = TEXT("stmt_0");
	Fragment.SourceStatementId = TEXT("stmt_0");
	Fragment.Path = TEXT("$.statements[0]");
	Fragment.Kind = TEXT("call");
	Dag.Fragments.Add(Fragment);

	const FBlueprintHelperGraphFragmentEvidenceBundle Bundle =
		FBlueprintHelperGraphFragmentEvidenceBuilder::BuildFromDag(Dag);

	TestEqual(TEXT("one review scope"), Bundle.ReviewScopes.Num(), 1);
	if (Bundle.ReviewScopes.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperGraphFragmentEvidenceReviewScope& Scope = Bundle.ReviewScopes[0];
	TestEqual(TEXT("scope stays generic event"), Scope.ScopeKind, EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Event);
	TestEqual(TEXT("event name"), Scope.EventName, FString(TEXT("ReceiveBeginPlay")));
	TestEqual(TEXT("taxonomy field"), Scope.EventTaxonomy, FString(TEXT("override_event")));
	TestEqual(TEXT("taxonomy metadata"), Scope.Metadata.FindRef(TEXT("event_taxonomy")), FString(TEXT("override_event")));
	return true;
}
```

- [ ] **Step 2: Run evidence test and verify it fails**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -unattended -nop4 -nosplash -nullrhi -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.EventTaxonomy.ReviewEvidence;Quit"
```

Expected before implementation: compile failure for `Scope.EventTaxonomy`.

- [ ] **Step 3: Add EventTaxonomy to review scope serialization**

In `BlueprintHelperGraphFragmentEvidence.h`:

```cpp
FString EventTaxonomy;
```

Place it next to `EventName`.

In `ToJson()`:

```cpp
if (!EventTaxonomy.IsEmpty())
{
	Json->SetStringField(TEXT("event_taxonomy"), EventTaxonomy);
}
```

- [ ] **Step 4: Populate taxonomy from metadata**

In `BlueprintHelperGraphFragmentEvidenceUtils.cpp`, after setting `Scope.EventName`:

```cpp
Scope.EventTaxonomy = ReadFirstMetadata(Dag.Metadata, { TEXT("event_taxonomy") });
```

Also set it in `MakeReviewScope()` when the hint did not already provide it:

```cpp
if (Scope.EventTaxonomy.IsEmpty())
{
	Scope.EventTaxonomy = ReadFirstMetadata(Dag.Metadata, { TEXT("event_taxonomy") });
}
```

Do not add `custom_event` / `native_event` / `override_event` as new `ReviewScopeKind` values.

- [ ] **Step 5: Run focused verification**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -unattended -nop4 -nosplash -nullrhi -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.EventTaxonomy.ReviewEvidence;Quit"
```

Expected after implementation: review scope remains `Event`, JSON contains `event_taxonomy`.

Manual checkpoint message:

```text
修复内容：
1. Review evidence 保留 event_taxonomy metadata
```

---

### Task 5: BUG-005 EventDelegate handler fallback scan removal and Signature/ActionContext split

**Bug:** EventDelegate resolver 在 handler function 缺失时扫描 `UK2Node_CustomEvent`，把 handler declaration 查找混入 EventDelegate use-site evidence reader。

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateActionClusterTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`

- [ ] **Step 1: Write failing ActionContext projection test**

In `BlueprintHelperActionContextPipelineTests.cpp`, extend `FBlueprintHelperActionContextDelegateProjectionTest`:

```cpp
Demand.HandlerName = TEXT("HandleDoorStateChanged");
Demand.HandlerFunctionPath = TEXT("/Game/Test/BP_Door.BP_Door_C:HandleDoorStateChanged");
Demand.HandlerSourceCluster = TEXT("BlueprintSignature");
Demand.SignatureEvidenceId = TEXT("signature:custom_event:HandleDoorStateChanged");

...

TestEqual(TEXT("handler function path"), Context.Evidence.FindRef(TEXT("handler_function_path")), FString(TEXT("/Game/Test/BP_Door.BP_Door_C:HandleDoorStateChanged")));
TestEqual(TEXT("handler source cluster"), Context.Evidence.FindRef(TEXT("handler_source_cluster")), FString(TEXT("BlueprintSignature")));
TestEqual(TEXT("signature evidence id"), Context.Evidence.FindRef(TEXT("signature_evidence_id")), FString(TEXT("signature:custom_event:HandleDoorStateChanged")));
```

- [ ] **Step 2: Write failing EventDelegate missing handler path test**

In `BlueprintHelperEventDelegateActionClusterTests.cpp`, update `AddHandlerEvidence()` so positive tests pass only when a real function path is projected:

```cpp
static void AddHandlerEvidence(
	FBlueprintHelperActionResolutionRequest& Request,
	UClass* HandlerScopeClass,
	const TCHAR* HandlerName)
{
	Request.ContextEvidence.Add(TEXT("handler_name"), HandlerName);
	Request.ContextEvidence.Add(TEXT("handler_scope_class_path"), HandlerScopeClass ? HandlerScopeClass->GetPathName() : TEXT(""));
	if (HandlerScopeClass)
	{
		if (UFunction* HandlerFunction = HandlerScopeClass->FindFunctionByName(FName(HandlerName)))
		{
			Request.ContextEvidence.Add(TEXT("handler_function_path"), HandlerFunction->GetPathName());
			Request.ContextEvidence.Add(TEXT("handler_source_cluster"), TEXT("BlueprintSignature"));
			Request.ContextEvidence.Add(TEXT("signature_evidence_id"), FString::Printf(TEXT("signature:handler:%s"), HandlerName));
		}
	}
}
```

Add a negative test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateBindMissingHandlerFunctionPathTest,
	"BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.Delegate.BindMissingHandlerFunctionPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateBindMissingHandlerFunctionPathTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	FMulticastDelegateProperty* DelegateProperty =
		FindRequiredDelegateProperty(*this, UPrimitiveComponent::StaticClass(), TEXT("OnComponentBeginOverlap"));
	if (!Blueprint || !Graph || !DelegateProperty)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request =
		MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Delegate, DelegateProperty->GetName());
	Request.ContextEvidence.Add(TEXT("binding_object_path"), TEXT("CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("delegate_operation"), TEXT("bind"));
	AddDelegateEvidence(Request, DelegateProperty);
	Request.ContextEvidence.Add(TEXT("handler_name"), TEXT("K2_DestroyActor"));
	Request.ContextEvidence.Add(TEXT("handler_scope_class_path"), AActor::StaticClass()->GetPathName());

	AssertMissingEvidenceDiagnostic(
		*this,
		TEXT("delegate bind missing handler function path"),
		FBlueprintHelperActionResolutionCore::Resolve(Request),
		TEXT("handler_function_path_missing"));
	return true;
}
```

- [ ] **Step 3: Write source contract forbidding EventDelegate custom event scan**

In `BlueprintHelperActionResolutionContractTests.cpp`, add:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionResolutionEventDelegateNoCustomEventScanContractTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.EventDelegateNoCustomEventScan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionResolutionEventDelegateNoCustomEventScanContractTest::RunTest(const FString& Parameters)
{
	const FString SourcePath = BuildGraphWritePrivateSourcePath(
		TEXT("ActionResolution"),
		TEXT("BlueprintHelperEventDelegateUseSiteEvidence.cpp"));

	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *SourcePath))
	{
		AddError(FString::Printf(TEXT("EventDelegate evidence source could not be read: %s"), *SourcePath));
		return false;
	}

	bool bClean = true;
	const TArray<FString> ForbiddenTokens = {
		TEXT("#include \"K2Node_CustomEvent.h\""),
		TEXT("UK2Node_CustomEvent"),
		TEXT("UbergraphPages")
	};
	for (const FString& Token : ForbiddenTokens)
	{
		if (Text.Contains(Token))
		{
			AddError(FString::Printf(TEXT("EventDelegate resolver must consume projected handler evidence, not scan custom events; forbidden token '%s' found."), *Token));
			bClean = false;
		}
	}
	return bClean;
}
```

- [ ] **Step 4: Run the failing tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -unattended -nop4 -nosplash -nullrhi -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ActionContext.EventDelegate.DelegateProjectsEvidence;BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.Delegate.BindMissingHandlerFunctionPath;BlueprintHelper.GraphWrite.ActionResolution.Contract.EventDelegateNoCustomEventScan;Quit"
```

Expected before implementation: projection fields missing and source contract fails on `UK2Node_CustomEvent`.

- [ ] **Step 5: Add handler evidence fields to ActionContext**

In `FBlueprintHelperActionContextDemand`:

```cpp
FString HandlerFunctionPath;
FString HandlerSourceCluster;
FString SignatureEvidenceId;
```

In demand collection, copy from `Statement.ContextEvidence` for EventDelegate statements:

```cpp
static FString EvidenceValue(const TMap<FString, FString>& Evidence, const FString& Key)
{
	if (const FString* Value = Evidence.Find(Key))
	{
		return Value->TrimStartAndEnd();
	}
	return FString();
}

if (InOutDemand.HandlerFunctionPath.IsEmpty())
{
	InOutDemand.HandlerFunctionPath = EvidenceValue(Statement.ContextEvidence, TEXT("handler_function_path"));
}
if (InOutDemand.HandlerSourceCluster.IsEmpty())
{
	InOutDemand.HandlerSourceCluster = EvidenceValue(Statement.ContextEvidence, TEXT("handler_source_cluster"));
}
if (InOutDemand.SignatureEvidenceId.IsEmpty())
{
	InOutDemand.SignatureEvidenceId = EvidenceValue(Statement.ContextEvidence, TEXT("signature_evidence_id"));
}
```

In `BuildContext()`, project them:

```cpp
BlueprintHelperActionContextInference::AddEvidenceIfPresent(Context, TEXT("handler_function_path"), Demand.HandlerFunctionPath);
BlueprintHelperActionContextInference::AddEvidenceIfPresent(Context, TEXT("handler_source_cluster"), Demand.HandlerSourceCluster);
BlueprintHelperActionContextInference::AddEvidenceIfPresent(Context, TEXT("signature_evidence_id"), Demand.SignatureEvidenceId);
```

- [ ] **Step 6: Remove EventDelegate custom event fallback scan**

In `BlueprintHelperEventDelegateUseSiteEvidence.h`, add:

```cpp
FString HandlerFunctionPath;
FString HandlerSourceCluster;
FString SignatureEvidenceId;
```

In `TryRead()`, read required handler fields for handler-requiring operations:

```cpp
OutEvidence.HandlerFunctionPath = EvidenceValue(Request, TEXT("handler_function_path"));
OutEvidence.HandlerSourceCluster = EvidenceValue(Request, TEXT("handler_source_cluster"));
OutEvidence.SignatureEvidenceId = EvidenceValue(Request, TEXT("signature_evidence_id"));
```

Before `ResolveHandlerFunction()`:

```cpp
if (RequiresResolvedHandler(SemanticKind, OutEvidence.DelegateOperation))
{
	if (OutEvidence.HandlerFunctionPath.IsEmpty())
	{
		return Missing(TEXT("handler_function_path_missing"), TEXT("EventDelegate resolution requires projected ContextEvidence.handler_function_path from BlueprintSignature."), OutMissingDetail, OutMessage);
	}
	if (OutEvidence.HandlerSourceCluster.IsEmpty())
	{
		return Missing(TEXT("handler_source_cluster_missing"), TEXT("EventDelegate resolution requires projected ContextEvidence.handler_source_cluster."), OutMissingDetail, OutMessage);
	}
	if (OutEvidence.SignatureEvidenceId.IsEmpty())
	{
		return Missing(TEXT("signature_evidence_id_missing"), TEXT("EventDelegate resolution requires projected ContextEvidence.signature_evidence_id."), OutMissingDetail, OutMessage);
	}
}
```

Rewrite `ResolveHandlerFunction()` to resolve only by `handler_function_path`:

```cpp
static bool ResolveHandlerFunction(
	FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
	FString& OutMissingDetail,
	FString& OutMessage)
{
	UFunction* HandlerFunction = FindObject<UFunction>(nullptr, *Evidence.HandlerFunctionPath.TrimStartAndEnd());
	if (!HandlerFunction)
	{
		return Missing(
			TEXT("handler_function_unresolved"),
			FString::Printf(TEXT("Could not resolve projected handler function '%s'."), *Evidence.HandlerFunctionPath),
			OutMissingDetail,
			OutMessage);
	}

	if (!Evidence.HandlerName.IsEmpty()
		&& !HandlerFunction->GetName().Equals(Evidence.HandlerName, ESearchCase::IgnoreCase))
	{
		return Missing(
			TEXT("handler_function_name_mismatch"),
			FString::Printf(TEXT("Projected handler function '%s' does not match handler_name '%s'."), *HandlerFunction->GetName(), *Evidence.HandlerName),
			OutMissingDetail,
			OutMessage);
	}

	Evidence.HandlerFunction = HandlerFunction;
	return true;
}
```

Remove `#include "K2Node_CustomEvent.h"` from this file.

- [ ] **Step 7: Run focused verification**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -unattended -nop4 -nosplash -nullrhi -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ActionContext.EventDelegate;BlueprintHelper.GraphWrite.ActionResolution.EventDelegate;BlueprintHelper.GraphWrite.ActionResolution.Contract.EventDelegateNoCustomEventScan;Quit"
```

Expected after implementation: EventDelegate positive tests pass with handler function path, missing path test fails deterministically, source contract passes.

Manual checkpoint message:

```text
修复内容：
1. 移除 EventDelegate handler custom event fallback scan
2. 通过 BlueprintSignature evidence 和 ActionContext projection 传递 handler reference
```

---

## Unified Verification

- [ ] **Step 1: Run focused automation suite**

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -unattended -nop4 -nosplash -nullrhi -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.EventTaxonomy;BlueprintHelper.GraphWrite.ActionContext.EventDelegate;BlueprintHelper.GraphWrite.ActionResolution.EventDelegate;BlueprintHelper.GraphWrite.ActionResolution.Contract.EventDelegateNoCustomEventScan;Quit"
```

Expected: all listed tests pass.

- [ ] **Step 2: Run legacy/public surface guards**

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -unattended -nop4 -nosplash -nullrhi -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.LegacyMainline.NoPublicParsedNodeGraphWriteApi;BlueprintHelper.GraphWrite.LegacyMainline.ActiveGraphWriteSourceLegacyTokenGate;Quit"
```

Expected: no public parsed DTO resurrection and no parsed-node mainline fallback.

- [ ] **Step 3: Build plugin/editor target**

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

Expected: build succeeds.

- [ ] **Step 4: Source contract scans**

```powershell
rg -n "UK2Node_CustomEvent|UbergraphPages" BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperEventDelegateUseSiteEvidence.cpp
rg -n "custom_event_declaration|override_event_declaration|native_event_declaration|ensure_custom_event|ensure_override_event" BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\ActionResolution BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\GraphStatement
rg -n "EBlueprintHelperGraphStatementKind::CustomEvent|EBlueprintHelperGraphStatementKind::Event" BlueprintHelper\Source\BlueprintHelper
```

Expected:
- first command has no matches;
- second command has no new GraphWrite/EventDelegate ownership matches, excluding tests/docs;
- third command has no matches.

- [ ] **Step 5: Diff hygiene**

```powershell
git diff --check
git status --short
```

Expected: no whitespace errors; status contains only files changed for this plan plus any pre-existing unrelated files.

## Self-Review Checklist

- [ ] EVTAX-001 has a deterministic Signature evidence gate for `custom_event`.
- [ ] EVTAX-002 preserves `native_event` / `override_event` taxonomy without GraphWrite declaration ownership.
- [ ] EVTAX-003 splits Replace entry resolution by scope and node class.
- [ ] EVTAX-004 keeps Review scope generic `Event` while serializing `event_taxonomy`.
- [ ] EVTAX-005 removes EventDelegate custom event scan and consumes only projected handler evidence.
- [ ] No hardcoded UE event-name taxonomy inference was introduced.
- [ ] No new public parsed DTO or parsed-node write path was introduced.
- [ ] No `git add`, `git commit`, or `git push` was run by the agent.

## Suggested Manual Commit Message After Full Execution

```text
修复内容：
1. 修复 GraphWrite event taxonomy evidence 边界
2. 拆分 Replace event entry resolver
3. 移除 EventDelegate handler fallback scan
```
