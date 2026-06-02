# BlueprintHelper GraphWrite Connectivity Validation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 `BlueprintHelper.TaskSpec.v1 -> TaskPlan -> UE GraphWrite` 写图链路中增加连接正确性门，阻断无执行入口、无数据消费者、未进入 ownership / rollback 记录的本轮 generated graph objects。

**Architecture:** 连接校验由 UE GraphWrite runtime 的可复用 validator 承担，TS compiler 只做显然可判定的静态预检。Validator 消费真实 UE graph、generated nodes、entry roots、created links 和 ownership evidence，不读取原始 TaskSpec，也不放到 UI/CLI 本地逻辑里。

**Tech Stack:** UE 5.6 C++、GraphWrite SemanticIR、GraphWrite MutationCoordinator、Review v2 evidence、Unreal Automation Tests、AgentFaceService task-core TypeScript、BlueprintHelper CLI。

---

## Execution Status

- Status: completed on 2026-06-02.
- Debug evidence: `Debug/BlueprintHelper_GraphWriteConnectivityValidation_Implementation_20260602.md`.
- Implementation report: `BlueprintHelper/Develop/Report/BlueprintHelper_GraphWrite_ConnectivityValidation_ImplementationReport_20260602_CN.md`.
- Final E2E summary: `D:\UEProjects\Template\Saved\Automation\GraphWriteConnectivityValidationE2E_20260602_125412\summary.json`.
- Final verification:
  - `AgentFaceService/task-core`: 331 node tests passed.
  - `AgentFaceService/cli`: 54 node tests passed.
  - UE build passed with `E:\UE_5.6`.
  - Focused UE automation passed: ConnectivityValidation 8/0, ToolResultBase 11/0, TaskRuntime 38 + 4 warnings / 0 failed.
  - Final focused automation artifacts:
    - `D:\UEProjects\Template\Saved\Automation\GraphWrite_ConnectivityValidation_Final_20260602_001\index.json`
    - `D:\UEProjects\Template\Saved\Automation\GraphWrite_ToolResultBase_Final_20260602_001\index.json`
    - `D:\UEProjects\Template\Saved\Automation\GraphWrite_TaskRuntime_Final_20260602_001\index.json`

## Source Spec

- `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_ConnectivityValidation_Design_20260602_CN.md`

## Hard Rules

- 不新增 raw payload / legacy GraphWrite 旁路。
- 不把 TaskSpec 改成 agent 手写低层 `nodes/links`。
- 不在 UI、CLI、单个工具函数里实现连接判断。
- `Comment` / `Reroute` 是唯一连接白名单；它们仍必须进入 generated object / ownership / rollback 记录。
- `Layout` 不是 node 类型，也不是 GWS generated node 分类；位置/展示变更走 GraphLayout / position mutation。
- 所有没有 ExecPin 且不属于白名单的 generated node 都按 PureData 校验。
- Validator 只校验本轮 generated/owned nodes 和本轮 external boundary，不清理旧用户图孤岛。
- 失败清理由现有 service mutation lifecycle 负责；validator 只返回诊断和阻断结果。
- 执行本计划时不自动 `git add`、`git commit`、`git push`。每个任务记录变更文件和建议提交范围。

## File Structure

### New C++ Runtime Files

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Validation/BlueprintHelperGraphWriteConnectivityValidator.h`
  - Defines validator input/result contracts and a reusable `FBlueprintHelperGraphWriteConnectivityValidator`.
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Validation/BlueprintHelperGraphWriteConnectivityValidator.cpp`
  - Implements node classification, exec reachability, PureData consumption, missing-link diagnostics, and whitelist handling.
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteConnectivityValidatorTests.cpp`
  - Focused validator automation tests with transient graphs and manual pins.

### Existing C++ Runtime Files To Modify

- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphWriteContext.h`
  - Add entry-root tracking and generated node record helpers.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphWriteContext.cpp`
  - Implement entry-root registration and reset.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphWriteExecutionStats.h`
  - Add connectivity validation timing and violation count.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphWriteExecutionStats.cpp`
  - Serialize the new stats fields.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/BlueprintGraphWriteResultTypes.h`
  - Add `ConnectivityDiagnostics` / `ConnectivityViolationCount` to `FBlueprintGenerateResult`.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/Utils/GraphWritePipelineUtils.cpp`
  - Run validator in SemanticIR generation before setting success.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationCoordinator.cpp`
  - Run validator for mutation-intent generated nodes and inserted exec boundaries.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperAppendBlueprintGraphService.cpp`
  - Return `graphwrite_connectivity_failed` on preview/execute validator failure and keep current rollback path.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.cpp`
  - Preserve rollback on validator failure and expose concise violations.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.cpp`
  - Preserve `Mutation.Rollback()` for connectivity failure after merge insertions.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.cpp`
  - Keep Review v2 evidence construction independent; add ownership-completeness issue propagation if graph result reports missing generated ownership.

### Existing C++ Tests To Extend

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintGraphWriteContextTests.cpp`
  - Cover entry-root tracking reset and registration.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp`
  - Add service-level preview/execute failure cases if focused validator tests cannot exercise full result mapping.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperReplaceExternalBodyTests.cpp`
  - Add external boundary acceptance/regression case if replace external body generates a body with reachable exec/data.

### AgentFaceService Files

- Create: `AgentFaceService/task-core/src/task/compiler/task-compiler.graphwrite-connectivity-preflight.test.ts`
  - Red/green node tests for obvious static unconsumed PureData shapes.
- Create: `AgentFaceService/task-core/src/task/compiler/graphwrite-connectivity-preflight.ts`
  - Lightweight static scan for obvious unconsumed `let` / result-symbol producer misuse.
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
  - Call static preflight before producing GraphWrite TaskPlan steps.
- Modify: `AgentFaceService/task-core/src/task/service/task-spec-runner.ts`
  - Ensure `graphwrite_connectivity_failed` preview/execute issues are exposed as concise task issues.
- Modify: `AgentFaceService/cli/src/cli/output.ts`
  - Keep ordinary CLI output short: `error_code` + concise `violations`; full pin/link detail remains debug artifact / expert output.

### Scripts And Docs

- Create: `BlueprintHelper/Develop/Scripts/Run-GraphWriteConnectivityValidationE2E.ps1`
  - Runs negative blocked preview/execute cases, positive write/readback cases, and optional review reject cleanup.
- Create after implementation: `BlueprintHelper/Develop/Report/BlueprintHelper_GraphWrite_ConnectivityValidation_ImplementationReport_20260602_CN.md`
  - Required session report once code changes exist.
- Update: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_ConnectivityValidation_ImplementationPlan_20260602_CN.md`
  - Track task status and final verification evidence.
- Update or supplement: `Debug/BlueprintHelper_GraphWriteConnectivityValidation_Implementation_20260602.md`
  - Record commands, failures, artifacts, and explorer findings during execution.

## Shared Validation Contract

Use existing `FBlueprintGeneratorDiagnostic` for agent-facing violation items. Add only one small runtime contract:

```cpp
struct FBlueprintGraphWriteConnectivityValidationInput
{
	UEdGraph* TargetGraph = nullptr;
	TArray<UEdGraphNode*> GeneratedNodes;
	TSet<UEdGraphNode*> EntryRootNodes;
	int32 RequestedConnectionCount = 0;
	int32 CreatedConnectionCount = 0;
	bool bRequirePureDataReachableToExec = true;
};

struct FBlueprintGraphWriteConnectivityValidationResult
{
	bool bPassed = true;
	TArray<FBlueprintGeneratorDiagnostic> Diagnostics;
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphWriteConnectivityValidator
{
public:
	static FBlueprintGraphWriteConnectivityValidationResult Validate(
		const FBlueprintGraphWriteConnectivityValidationInput& Input);
};
```

Violation codes:

| Code | Blocking rule |
| --- | --- |
| `missing_expected_link` | Requested link count is greater than created link count. |
| `unreachable_exec_node` | Non-entry generated exec node has no incoming reachable exec link. |
| `unconsumed_pure_data_node` | Non-Comment/non-Reroute generated node without ExecPin has no outgoing data consumer. |
| `unreachable_pure_data_chain` | PureData output chain never reaches a reachable ExecNode input. |
| `invalid_expression_exec_node` | Expression context generated a node with ExecPin but no exec path. |
| `unregistered_generated_node` | Generated node is not in ownership / rollback evidence. |
| `unregistered_generated_link` | Generated link is not in evidence / rollback data. |
| `external_boundary_not_connected` | External merge boundary was not connected according to strategy. |

---

### Task 1: Add Focused Validator RED Tests

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteConnectivityValidatorTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintGraphWriteContextTests.cpp`

- [x] **Step 1: Write the failing validator tests**

Add this test file. It should fail to compile until the validator contract exists.

```cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Knot.h"
#include "Systems/ToolClusters/GraphWrite/Validation/BlueprintHelperGraphWriteConnectivityValidator.h"

namespace BlueprintHelperGraphWriteConnectivityValidatorTests
{
	static UEdGraphNode* AddNodeWithPins(
		UEdGraph* Graph,
		const FName NodeName,
		const bool bExecInput,
		const bool bExecOutput,
		const bool bDataInput,
		const bool bDataOutput)
	{
		UEdGraphNode* Node = NewObject<UEdGraphNode>(Graph, NodeName);
		Graph->AddNode(Node, true, false);

		FEdGraphPinType ExecPinType;
		ExecPinType.PinCategory = UEdGraphSchema_K2::PC_Exec;

		FEdGraphPinType BoolPinType;
		BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

		if (bExecInput)
		{
			Node->CreatePin(EGPD_Input, ExecPinType, FName(TEXT("execute")));
		}
		if (bExecOutput)
		{
			Node->CreatePin(EGPD_Output, ExecPinType, FName(TEXT("then")));
		}
		if (bDataInput)
		{
			Node->CreatePin(EGPD_Input, BoolPinType, FName(TEXT("condition")));
		}
		if (bDataOutput)
		{
			Node->CreatePin(EGPD_Output, BoolPinType, FName(TEXT("value")));
		}
		return Node;
	}

	static UEdGraphPin* FindPin(UEdGraphNode* Node, const FName PinName, const EEdGraphPinDirection Direction)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->PinName == PinName && Pin->Direction == Direction)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	static void ForceLink(UEdGraphPin* FromPin, UEdGraphPin* ToPin)
	{
		FromPin->LinkedTo.AddUnique(ToPin);
		ToPin->LinkedTo.AddUnique(FromPin);
	}

	static bool HasViolation(
		const FBlueprintGraphWriteConnectivityValidationResult& Result,
		const FString& Code)
	{
		for (const FBlueprintGeneratorDiagnostic& Diagnostic : Result.Diagnostics)
		{
			if (Diagnostic.Code == Code)
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteConnectivityValidatorRejectsExecWithoutIncomingTest,
	"BlueprintHelper.GraphWrite.ConnectivityValidation.RejectsExecWithoutIncoming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteConnectivityValidatorRejectsExecWithoutIncomingTest::RunTest(const FString&)
{
	using namespace BlueprintHelperGraphWriteConnectivityValidatorTests;

	UEdGraph* Graph = NewObject<UEdGraph>(GetTransientPackage(), FName(TEXT("BH_Connectivity_ExecMissing")));
	UEdGraphNode* Entry = AddNodeWithPins(Graph, FName(TEXT("Entry")), false, true, false, false);
	UEdGraphNode* OrphanExec = AddNodeWithPins(Graph, FName(TEXT("OrphanExec")), true, true, false, false);

	FBlueprintGraphWriteConnectivityValidationInput Input;
	Input.TargetGraph = Graph;
	Input.GeneratedNodes = {Entry, OrphanExec};
	Input.EntryRootNodes.Add(Entry);

	const FBlueprintGraphWriteConnectivityValidationResult Result =
		FBlueprintHelperGraphWriteConnectivityValidator::Validate(Input);

	TestFalse(TEXT("validation blocks orphan exec"), Result.bPassed);
	TestTrue(TEXT("reports unreachable_exec_node"), HasViolation(Result, TEXT("unreachable_exec_node")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteConnectivityValidatorRejectsPureDataWithoutConsumerTest,
	"BlueprintHelper.GraphWrite.ConnectivityValidation.RejectsPureDataWithoutConsumer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteConnectivityValidatorRejectsPureDataWithoutConsumerTest::RunTest(const FString&)
{
	using namespace BlueprintHelperGraphWriteConnectivityValidatorTests;

	UEdGraph* Graph = NewObject<UEdGraph>(GetTransientPackage(), FName(TEXT("BH_Connectivity_PureMissing")));
	UEdGraphNode* PureData = AddNodeWithPins(Graph, FName(TEXT("PureData")), false, false, false, true);

	FBlueprintGraphWriteConnectivityValidationInput Input;
	Input.TargetGraph = Graph;
	Input.GeneratedNodes = {PureData};

	const FBlueprintGraphWriteConnectivityValidationResult Result =
		FBlueprintHelperGraphWriteConnectivityValidator::Validate(Input);

	TestFalse(TEXT("validation blocks unconsumed pure data"), Result.bPassed);
	TestTrue(TEXT("reports unconsumed_pure_data_node"), HasViolation(Result, TEXT("unconsumed_pure_data_node")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteConnectivityValidatorAcceptsConsumedPureDataTest,
	"BlueprintHelper.GraphWrite.ConnectivityValidation.AcceptsConsumedPureData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteConnectivityValidatorAcceptsConsumedPureDataTest::RunTest(const FString&)
{
	using namespace BlueprintHelperGraphWriteConnectivityValidatorTests;

	UEdGraph* Graph = NewObject<UEdGraph>(GetTransientPackage(), FName(TEXT("BH_Connectivity_PureConsumed")));
	UEdGraphNode* Entry = AddNodeWithPins(Graph, FName(TEXT("Entry")), false, true, false, false);
	UEdGraphNode* ExecNode = AddNodeWithPins(Graph, FName(TEXT("ExecNode")), true, true, true, false);
	UEdGraphNode* PureData = AddNodeWithPins(Graph, FName(TEXT("PureData")), false, false, false, true);

	ForceLink(FindPin(Entry, FName(TEXT("then")), EGPD_Output), FindPin(ExecNode, FName(TEXT("execute")), EGPD_Input));
	ForceLink(FindPin(PureData, FName(TEXT("value")), EGPD_Output), FindPin(ExecNode, FName(TEXT("condition")), EGPD_Input));

	FBlueprintGraphWriteConnectivityValidationInput Input;
	Input.TargetGraph = Graph;
	Input.GeneratedNodes = {Entry, ExecNode, PureData};
	Input.EntryRootNodes.Add(Entry);

	const FBlueprintGraphWriteConnectivityValidationResult Result =
		FBlueprintHelperGraphWriteConnectivityValidator::Validate(Input);

	TestTrue(TEXT("consumed pure data passes"), Result.bPassed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteConnectivityValidatorWhitelistsCommentAndRerouteTest,
	"BlueprintHelper.GraphWrite.ConnectivityValidation.WhitelistsCommentAndReroute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteConnectivityValidatorWhitelistsCommentAndRerouteTest::RunTest(const FString&)
{
	UEdGraph* Graph = NewObject<UEdGraph>(GetTransientPackage(), FName(TEXT("BH_Connectivity_Whitelist")));
	UEdGraphNode_Comment* Comment = NewObject<UEdGraphNode_Comment>(Graph);
	Graph->AddNode(Comment, true, false);

	UK2Node_Knot* Reroute = NewObject<UK2Node_Knot>(Graph);
	Graph->AddNode(Reroute, true, false);
	Reroute->AllocateDefaultPins();

	FBlueprintGraphWriteConnectivityValidationInput Input;
	Input.TargetGraph = Graph;
	Input.GeneratedNodes = {Comment, Reroute};

	const FBlueprintGraphWriteConnectivityValidationResult Result =
		FBlueprintHelperGraphWriteConnectivityValidator::Validate(Input);

	TestTrue(TEXT("comment/reroute whitelist passes"), Result.bPassed);
	return true;
}

#endif
```

- [x] **Step 2: Extend context tests for entry roots**

Append this case to `BlueprintGraphWriteContextTests.cpp`. It should fail until Task 2 updates `FBlueprintGraphWriteContext`.

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintGraphWriteContextTracksEntryRootsTest,
	"BlueprintHelper.GraphWrite.Context.TracksEntryRoots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintGraphWriteContextTracksEntryRootsTest::RunTest(const FString&)
{
	UEdGraph* Graph = NewObject<UEdGraph>(GetTransientPackage(), FName(TEXT("BH_Context_EntryRoots")));
	UK2Node* Node = NewObject<UK2Node>(Graph);
	Graph->AddNode(Node, true, false);

	FBlueprintGraphWriteContext Context;
	Context.Initialize(Graph);
	Context.RegisterNode(TEXT("entry"), Node, true, true);

	TestEqual(TEXT("generated count"), Context.GetGeneratedNodes().Num(), 1);
	TestTrue(TEXT("entry root recorded"), Context.GetEntryRootNodes().Contains(Node));

	Context.Initialize(nullptr);
	TestEqual(TEXT("entry roots cleared"), Context.GetEntryRootNodes().Num(), 0);
	return true;
}
```

- [x] **Step 3: Run focused RED automation**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ConnectivityValidation;BlueprintHelper.GraphWrite.Context;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_ConnectivityValidation_RED_001'
```

Expected: compile or automation failure because `BlueprintHelperGraphWriteConnectivityValidator.h` and `GetEntryRootNodes()` are not implemented.

- [x] **Step 4: Record evidence**

Append to `Debug/BlueprintHelper_GraphWriteConnectivityValidation_Implementation_20260602.md`:

```markdown
## Task 1 RED

- Command: focused connectivity/context automation.
- Expected: compile or test failure before validator implementation.
- Result:
- Artifact:
```

---

### Task 2: Implement Validator Contract, Classification, And Direct Link Rules

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Validation/BlueprintHelperGraphWriteConnectivityValidator.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Validation/BlueprintHelperGraphWriteConnectivityValidator.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphWriteContext.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphWriteContext.cpp`

- [x] **Step 1: Add validator header**

Create the header with this contract:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteResultTypes.h"

class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

struct BLUEPRINTHELPER_API FBlueprintGraphWriteConnectivityValidationInput
{
	UEdGraph* TargetGraph = nullptr;
	TArray<UEdGraphNode*> GeneratedNodes;
	TSet<UEdGraphNode*> EntryRootNodes;
	int32 RequestedConnectionCount = 0;
	int32 CreatedConnectionCount = 0;
	bool bRequirePureDataReachableToExec = true;
};

struct BLUEPRINTHELPER_API FBlueprintGraphWriteConnectivityValidationResult
{
	bool bPassed = true;
	TArray<FBlueprintGeneratorDiagnostic> Diagnostics;
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphWriteConnectivityValidator
{
public:
	static FBlueprintGraphWriteConnectivityValidationResult Validate(
		const FBlueprintGraphWriteConnectivityValidationInput& Input);

private:
	static void ValidateMissingLinks(
		const FBlueprintGraphWriteConnectivityValidationInput& Input,
		FBlueprintGraphWriteConnectivityValidationResult& Result);
	static void ValidateGeneratedNodes(
		const FBlueprintGraphWriteConnectivityValidationInput& Input,
		FBlueprintGraphWriteConnectivityValidationResult& Result);
	static bool IsWhitelistNode(const UEdGraphNode* Node);
	static bool HasExecPin(const UEdGraphNode* Node);
	static bool HasIncomingExecLink(const UEdGraphNode* Node);
	static bool HasOutgoingDataLink(const UEdGraphNode* Node);
	static bool IsPureDataConsumedByReachableExec(
		const UEdGraphNode* Node,
		const TSet<const UEdGraphNode*>& ReachableExecNodes);
	static TSet<const UEdGraphNode*> CollectReachableExecNodes(
		const FBlueprintGraphWriteConnectivityValidationInput& Input);
	static bool IsExecPin(const UEdGraphPin* Pin);
	static void AddViolation(
		FBlueprintGraphWriteConnectivityValidationResult& Result,
		const FString& Code,
		const UEdGraphNode* Node,
		const FString& Message);
};
```

- [x] **Step 2: Implement direct rules**

Implement direct missing-link, exec incoming, PureData outgoing, comment, and reroute handling. Do not add anonymous namespaces.

```cpp
#include "Systems/ToolClusters/GraphWrite/Validation/BlueprintHelperGraphWriteConnectivityValidator.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphNode_Comment.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Knot.h"

FBlueprintGraphWriteConnectivityValidationResult FBlueprintHelperGraphWriteConnectivityValidator::Validate(
	const FBlueprintGraphWriteConnectivityValidationInput& Input)
{
	FBlueprintGraphWriteConnectivityValidationResult Result;
	ValidateMissingLinks(Input, Result);
	ValidateGeneratedNodes(Input, Result);
	Result.bPassed = Result.Diagnostics.Num() == 0;
	return Result;
}

void FBlueprintHelperGraphWriteConnectivityValidator::ValidateMissingLinks(
	const FBlueprintGraphWriteConnectivityValidationInput& Input,
	FBlueprintGraphWriteConnectivityValidationResult& Result)
{
	if (Input.RequestedConnectionCount > Input.CreatedConnectionCount)
	{
		AddViolation(
			Result,
			TEXT("missing_expected_link"),
			nullptr,
			FString::Printf(
				TEXT("GraphWrite requested %d links but created %d."),
				Input.RequestedConnectionCount,
				Input.CreatedConnectionCount));
	}
}

void FBlueprintHelperGraphWriteConnectivityValidator::ValidateGeneratedNodes(
	const FBlueprintGraphWriteConnectivityValidationInput& Input,
	FBlueprintGraphWriteConnectivityValidationResult& Result)
{
	const TSet<const UEdGraphNode*> ReachableExecNodes = CollectReachableExecNodes(Input);
	for (UEdGraphNode* Node : Input.GeneratedNodes)
	{
		if (!Node || IsWhitelistNode(Node))
		{
			continue;
		}

		if (HasExecPin(Node))
		{
			if (!Input.EntryRootNodes.Contains(Node) && !HasIncomingExecLink(Node))
			{
				AddViolation(Result, TEXT("unreachable_exec_node"), Node, TEXT("Generated Exec node has no incoming exec link."));
			}
			continue;
		}

		if (!HasOutgoingDataLink(Node))
		{
			AddViolation(Result, TEXT("unconsumed_pure_data_node"), Node, TEXT("Generated PureData node has no outgoing data consumer."));
			continue;
		}

		if (Input.bRequirePureDataReachableToExec && !IsPureDataConsumedByReachableExec(Node, ReachableExecNodes))
		{
			AddViolation(Result, TEXT("unreachable_pure_data_chain"), Node, TEXT("Generated PureData node is not consumed by a reachable Exec node."));
		}
	}
}
```

- [x] **Step 3: Implement helper methods**

Use pin category, direction, and direct `LinkedTo` traversal:

```cpp
bool FBlueprintHelperGraphWriteConnectivityValidator::IsWhitelistNode(const UEdGraphNode* Node)
{
	return Node && (Node->IsA<UEdGraphNode_Comment>() || Node->IsA<UK2Node_Knot>());
}

bool FBlueprintHelperGraphWriteConnectivityValidator::IsExecPin(const UEdGraphPin* Pin)
{
	return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
}

bool FBlueprintHelperGraphWriteConnectivityValidator::HasExecPin(const UEdGraphNode* Node)
{
	if (!Node)
	{
		return false;
	}
	for (const UEdGraphPin* Pin : Node->Pins)
	{
		if (IsExecPin(Pin))
		{
			return true;
		}
	}
	return false;
}

bool FBlueprintHelperGraphWriteConnectivityValidator::HasIncomingExecLink(const UEdGraphNode* Node)
{
	if (!Node)
	{
		return false;
	}
	for (const UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input && IsExecPin(Pin) && Pin->LinkedTo.Num() > 0)
		{
			return true;
		}
	}
	return false;
}

bool FBlueprintHelperGraphWriteConnectivityValidator::HasOutgoingDataLink(const UEdGraphNode* Node)
{
	if (!Node)
	{
		return false;
	}
	for (const UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Output && !IsExecPin(Pin) && Pin->LinkedTo.Num() > 0)
		{
			return true;
		}
	}
	return false;
}
```

For `CollectReachableExecNodes`, perform bounded BFS from `EntryRootNodes` through output exec pins. For `IsPureDataConsumedByReachableExec`, BFS through data links until a reachable exec node input pin is found.

- [x] **Step 4: Extend context entry-root tracking**

Modify `BlueprintGraphWriteContext.h`:

```cpp
void RegisterNode(const FString& NodeId, UK2Node* Node, bool bGenerated, bool bEntryRoot = false);
const TSet<UEdGraphNode*>& GetEntryRootNodes() const;
```

Add private member:

```cpp
TSet<UEdGraphNode*> EntryRootNodes;
```

Modify `Initialize` to clear `EntryRootNodes`. Modify `RegisterNode` to add `Node` when `bEntryRoot` is true.

- [x] **Step 5: Run focused GREEN automation**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ConnectivityValidation;BlueprintHelper.GraphWrite.Context;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_ConnectivityValidation_GREEN_001'
```

Expected: build succeeds and focused automation passes.

---

### Task 3: Connect Validator To SemanticIR Append/Replace Runtime

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/BlueprintGraphWriteResultTypes.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphWriteExecutionStats.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphWriteExecutionStats.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/Utils/GraphWritePipelineUtils.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperAppendBlueprintGraphService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.cpp`

- [x] **Step 1: Add result fields**

In `FBlueprintGenerateResult`, add:

```cpp
TArray<FBlueprintGeneratorDiagnostic> ConnectivityDiagnostics;
int32 ConnectivityViolationCount = 0;
```

In `FBlueprintGraphWriteExecutionStats`, add:

```cpp
int32 ConnectivityViolationCount = 0;
double ConnectivityValidationMs = 0.0;
```

Serialize fields as:

```cpp
Json->SetNumberField(TEXT("connectivity_violation_count"), Stats.ConnectivityViolationCount);
Json->SetNumberField(TEXT("connectivity_validation_ms"), Stats.ConnectivityValidationMs);
```

- [x] **Step 2: Register entry roots in SemanticIR pipeline**

In `GraphWritePipelineUtils.cpp`, when registering `EntryFragment`, pass `bEntryRoot=true`:

```cpp
GraphWriteContext.RegisterNode(EntryFragment.FragmentId, EntryFragment.PrimaryNode, true, true);
```

For non-entry generated fragments, keep:

```cpp
GraphWriteContext.RegisterNode(Fragment.FragmentId, Fragment.PrimaryNode, true);
```

- [x] **Step 3: Run validator before `Result.bSucceed`**

After data edges are connected and before success is assigned:

```cpp
const double ConnectivityStart = FPlatformTime::Seconds();
FBlueprintGraphWriteConnectivityValidationInput ConnectivityInput;
ConnectivityInput.TargetGraph = TargetGraph;
ConnectivityInput.GeneratedNodes = GraphWriteContext.GetGeneratedNodes();
ConnectivityInput.EntryRootNodes = GraphWriteContext.GetEntryRootNodes();
ConnectivityInput.RequestedConnectionCount = DataEdges.Num();
ConnectivityInput.CreatedConnectionCount = CreatedConnectionCount;

const FBlueprintGraphWriteConnectivityValidationResult Connectivity =
	FBlueprintHelperGraphWriteConnectivityValidator::Validate(ConnectivityInput);

Result.ConnectivityDiagnostics = Connectivity.Diagnostics;
Result.ConnectivityViolationCount = Connectivity.Diagnostics.Num();
Result.ExecutionStats.ConnectivityViolationCount = Result.ConnectivityViolationCount;
Result.ExecutionStats.ConnectivityValidationMs = GraphWriteElapsedMs(ConnectivityStart);
```

Change success condition to:

```cpp
Result.bSucceed = Result.UnresolvedNodeCount == 0
	&& GeneratedNodeCount > 0
	&& Connectivity.bPassed;
```

If `Connectivity.bPassed` is false, set:

```cpp
Result.Message = TEXT("GraphWrite connectivity validation failed.");
```

- [x] **Step 4: Map failure code in append/replace services**

When `GenerateResult.bSucceed` is false and `GenerateResult.ConnectivityViolationCount > 0`, set:

```cpp
ImportErrorCode = TEXT("graphwrite_connectivity_failed");
ImportMessage = GenerateResult.Message;
```

Attach concise diagnostics to result data under:

```json
{
  "connectivity": {
    "violations": [
      { "code": "unconsumed_pure_data_node", "node_id": "...", "message": "..." }
    ]
  }
}
```

Do not include pin dumps in normal result data.

- [x] **Step 5: Run SemanticIR focused automation**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ConnectivityValidation;BlueprintHelper.GraphWrite.ToolResultBase;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_ConnectivityValidation_SemanticIR_001'
```

Expected: focused validator tests pass; existing GraphWrite tool-result tests still pass.

---

### Task 4: Connect Validator To Patch/Merge Mutation Coordinator

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationIntent.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationCoordinator.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp`

- [x] **Step 1: Add generated-node metadata to mutation intent**

Extend `FBlueprintHelperGraphWriteMutationIntent`:

```cpp
TArray<UEdGraphNode*> GeneratedNodes;
TSet<UEdGraphNode*> EntryRootNodes;
```

For existing single inserted-node merge paths, append `InsertedNode` to `GeneratedNodes` after it is created. For branch-fork sequence insertion, also append the sequence node when `OutSequenceNode` is populated.

- [x] **Step 2: Validate coordinator result after intents run**

At the end of `ExecuteIntents`, before `Result.bSucceed` is finalized, collect intent generated nodes:

```cpp
FBlueprintGraphWriteConnectivityValidationInput ConnectivityInput;
ConnectivityInput.TargetGraph = TargetGraph;
ConnectivityInput.RequestedConnectionCount = Result.RequestedConnectionCount;
ConnectivityInput.CreatedConnectionCount = Result.CreatedConnectionCount;

for (const FBlueprintHelperGraphWriteMutationIntent& Intent : Intents)
{
	ConnectivityInput.GeneratedNodes.Append(Intent.GeneratedNodes);
	ConnectivityInput.EntryRootNodes.Append(Intent.EntryRootNodes);
	if (Intent.InsertedNode)
	{
		ConnectivityInput.GeneratedNodes.AddUnique(Intent.InsertedNode);
	}
	if (Intent.OutSequenceNode && *Intent.OutSequenceNode)
	{
		ConnectivityInput.GeneratedNodes.AddUnique(*Intent.OutSequenceNode);
	}
}
```

Run `FBlueprintHelperGraphWriteConnectivityValidator::Validate`. If it fails, set:

```cpp
Result.bSucceed = false;
Result.Message = TEXT("GraphWrite connectivity validation failed.");
Result.ConnectivityDiagnostics = Connectivity.Diagnostics;
Result.ConnectivityViolationCount = Connectivity.Diagnostics.Num();
```

- [x] **Step 3: Preserve merge rollback**

In `BlueprintHelperMergeBlueprintGraphService.cpp`, the existing `if (!bSucceeded) { Mutation.Rollback(); ... }` block must handle validator failures. Map `Result.ConnectivityViolationCount > 0` to:

```cpp
{TEXT("graphwrite_connectivity_failed"), EBlueprintHelperToolStage::Execute, OutError, false, EBlueprintHelperRollbackResult::RolledBack}
```

- [x] **Step 4: Add mutation service regression tests**

Add tests in `BlueprintHelperGraphWriteToolResultBaseTests.cpp` that assert:

- merge branch-fork generated sequence/inserted node remains connected and passes.
- a synthetic coordinator intent with a generated exec node that is not linked returns `graphwrite_connectivity_failed`.

- [x] **Step 5: Run mutation automation**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ToolResultBase;BlueprintHelper.GraphWrite.ConnectivityValidation;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_ConnectivityValidation_Mutation_001'
```

Expected: mutation tests pass and validator failures return rolled-back failure results.

---

### Task 5: Add Ownership Completeness Validation Without Replacing Review v2

**Files:**
- Create or extend: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Validation/BlueprintHelperGraphWriteOwnershipCompletenessValidator.h`
- Create or extend: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Validation/BlueprintHelperGraphWriteOwnershipCompletenessValidator.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimeClusterHubTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp`

- [x] **Step 1: Add ownership validator contract**

Use a separate validator so connectivity and Review evidence stay decoupled:

```cpp
struct FBlueprintHelperGraphWriteOwnershipValidationInput
{
	TArray<FString> GeneratedBlockRefs;
	TArray<FBlueprintHelperReviewAtomicTarget> AtomicTargets;
};

struct FBlueprintHelperGraphWriteOwnershipValidationResult
{
	bool bPassed = true;
	TArray<FBlueprintGeneratorDiagnostic> Diagnostics;
};
```

Rule:

- Every generated block ref returned by GraphWrite success must map to at least one Review atomic target.
- `Comment` / `Reroute` generated by the write path still require a target if they are part of the generated ownership scope.
- External user-authored nodes are validated only at the current boundary target, not as owned nodes.

- [x] **Step 2: Validate after `BuildReviewEvidence` constructs atomic targets**

In `FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence`, after `OutEvidence.AtomicTargets` is populated, validate `BlockRefs` against targets. If validation fails, return `false` and keep the existing task-runtime error path.

- [x] **Step 3: Add tests for missing atomic targets**

In `BlueprintHelperTaskRuntimeClusterHubTests.cpp`, add a case where `StepResult.Data.block_refs = ["EventGraph_CE_Orphan"]` and `BuildReviewEvidence` produces no matching `graph:EventGraph:block:EventGraph_CE_Orphan`. Expected: evidence build fails.

- [x] **Step 4: Run Review/TaskRuntime automation**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.TaskRuntime;BlueprintHelper.GraphWrite.ToolResultBase;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_ConnectivityValidation_Ownership_001'
```

Expected: task-runtime evidence tests pass and GraphWrite success still creates Review v2 graph-block atomic targets.

---

### Task 6: Add Task-Core Static Preflight And CLI Short Output

**Files:**
- Create: `AgentFaceService/task-core/src/task/compiler/graphwrite-connectivity-preflight.ts`
- Create: `AgentFaceService/task-core/src/task/compiler/task-compiler.graphwrite-connectivity-preflight.test.ts`
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Modify: `AgentFaceService/task-core/src/task/service/task-spec-runner.ts`
- Modify: `AgentFaceService/cli/src/cli/output.ts`

- [x] **Step 1: Write node tests for static preflight**

Create tests that compile GraphWrite specs and assert obvious unconsumed value producers are rejected before bridge preview.

```ts
import { strict as assert } from 'node:assert';
import test from 'node:test';

import { compileTaskSpecToTaskPlan } from './task-compiler.js';

function makeSpec(statements: Record<string, unknown>[]) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_graph',
    target: { asset_path: '/Game/BH_Tests/BP_StaticConnectivity', target_type: 'blueprint' },
    scope_policy: { graph_name: 'EventGraph', allow_modify_user_nodes: false },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [{
        entry_type: 'custom_event',
        name: 'GW_StaticConnectivity',
        body: { schema: 'BlueprintLogicSpec.v1', statements },
      }],
    },
    execution_policy: { dry_run_mode: 'full' },
    validation: { should_compile: false, should_save: false },
  };
}

test('graphwrite connectivity preflight rejects unused let value producer', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeSpec([{
      kind: 'let',
      name: 'UnusedScore',
      value: { kind: 'call', target: 'GetScorePercent', args: {} },
    }]) as never),
    /unconsumed_pure_data_node/,
  );
});

test('graphwrite connectivity preflight allows let consumed by later statement', () => {
  const plan = compileTaskSpecToTaskPlan(makeSpec([{
    kind: 'let',
    name: 'ScoreText',
    value: { kind: 'literal', value_type: 'string', value: 'ok' },
  }, {
    kind: 'call',
    target: 'PrintString',
    args: { InString: { kind: 'get', name: 'ScoreText' } },
  }]) as never);
  assert.equal(plan.steps.length, 1);
});
```

- [x] **Step 2: Implement static preflight**

Create a small scanner:

```ts
export interface GraphWriteConnectivityPreflightIssue {
  code: 'unconsumed_pure_data_node';
  path: string;
  message: string;
}

export function collectGraphWriteConnectivityPreflightIssues(
  statements: readonly Record<string, unknown>[],
  basePath: string,
): GraphWriteConnectivityPreflightIssue[] {
  const defined = new Map<string, string>();
  const used = new Set<string>();

  statements.forEach((statement, index) => {
    const path = `${basePath}.statements[${index}]`;
    if (statement.kind === 'let' && typeof statement.name === 'string') {
      defined.set(statement.name.toLowerCase(), path);
    }
    collectGetReferences(statement, used);
  });

  const issues: GraphWriteConnectivityPreflightIssue[] = [];
  for (const [name, path] of defined) {
    if (!used.has(name)) {
      issues.push({
        code: 'unconsumed_pure_data_node',
        path,
        message: `Generated PureData symbol '${name}' is never consumed by a later statement.`,
      });
    }
  }
  return issues;
}
```

Make `collectGetReferences` recursively inspect objects/arrays and collect `{ kind: 'get', name: string }`.

- [x] **Step 3: Wire preflight into compiler**

In `task-compiler.ts`, before making GraphWrite TaskPlan steps, call the preflight for each `entry.body.statements`, `replace.body.statements`, and external inserted body. Convert issues into `TaskSpecCompileError('taskspec_semantic_invalid', ...)`.

- [x] **Step 4: Keep CLI output short**

In `task-spec-runner.ts` and `cli/output.ts`, ensure failure code `graphwrite_connectivity_failed` is visible in ordinary output and that only concise `violations` are emitted:

```json
{
  "ok": false,
  "status": "preview_blocked",
  "error_code": "graphwrite_connectivity_failed",
  "violations": [
    {
      "code": "unconsumed_pure_data_node",
      "node_id": "stmt_find_score",
      "message": "Generated PureData node has no outgoing data consumer."
    }
  ]
}
```

- [x] **Step 5: Run task-core verification serially**

Run:

```powershell
Push-Location "D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core"
npm.cmd run build
npm.cmd run test:node
Pop-Location
```

Expected: build succeeds and node tests pass. Keep build and `test:node` serial because tests consume `build/`.

---

### Task 7: Add Real E2E Script

**Files:**
- Create: `BlueprintHelper/Develop/Scripts/Run-GraphWriteConnectivityValidationE2E.ps1`
- Modify if needed: `BlueprintHelper/Develop/Scripts/Run-GraphWriteSingleBlueprintConnectionE2E.ps1`

- [x] **Step 1: Create negative/positive E2E script**

The script should:

1. Build `AgentFaceService/task-core`.
2. Build `AgentFaceService/cli`.
3. Create a test Blueprint asset.
4. Run a negative TaskSpec with an unconsumed PureData shape and assert preview is blocked with `graphwrite_connectivity_failed` or `taskspec_semantic_invalid`.
5. Run a positive TaskSpec that creates Entry -> PrintString and consumes at least one PureData value.
6. Read back with `blueprinthelper_read_context` in `logic_flow` or `logic_json`.
7. Assert exec/data links exist.
8. If a review record is produced, use `blueprinthelper_query_review_records` and `blueprinthelper_apply_review_action` reject to verify no generated residue remains.

Use the existing CLI invocation pattern from `Run-GraphWriteSingleBlueprintConnectionE2E.ps1`:

```powershell
$Preview = & node $Cli task preview --file $SpecPath --format json --artifact-dir $ResultDir
$Execute = & node $Cli task execute --file $SpecPath --format json --artifact-dir $ResultDir
```

- [x] **Step 2: Add run summary**

Write summary to:

```text
D:\UEProjects\Template\Saved\Automation\GraphWriteConnectivityValidationE2E_<timestamp>\summary.json
```

Summary fields:

```json
{
  "schema": "BlueprintHelper.GraphWriteConnectivityValidationE2E.v1",
  "ok": true,
  "negative_preview_blocked": true,
  "negative_execute_blocked": true,
  "positive_execute_passed": true,
  "positive_readback_exec_links": 1,
  "positive_readback_data_links": 1,
  "reject_cleanup_verified": true
}
```

- [x] **Step 3: Run E2E**

Run:

```powershell
& "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\Scripts\Run-GraphWriteConnectivityValidationE2E.ps1" -PluginRoot "D:\UEProjects\Template\Plugins\BlueprintHelper"
```

Expected: script exits 0 and summary `ok` is true.

---

### Task 8: Final Verification, Report, And Audit

**Files:**
- Create: `BlueprintHelper/Develop/Report/BlueprintHelper_GraphWrite_ConnectivityValidation_ImplementationReport_20260602_CN.md`
- Update: `Debug/BlueprintHelper_GraphWriteConnectivityValidation_Implementation_20260602.md`
- Update: this plan file with final status rows.

- [x] **Step 1: Run full verification stack**

Run:

```powershell
Push-Location "D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core"
npm.cmd run build
npm.cmd run test:node
Pop-Location

& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE

& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ConnectivityValidation;BlueprintHelper.GraphWrite.ToolResultBase;BlueprintHelper.TaskRuntime;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_ConnectivityValidation_FINAL_001'

& "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\Scripts\Run-GraphWriteConnectivityValidationE2E.ps1" -PluginRoot "D:\UEProjects\Template\Plugins\BlueprintHelper"
```

Expected:

- task-core build/test pass.
- UE build passes.
- focused automation passes.
- E2E summary `ok=true`.

- [x] **Step 2: Run final read-only audit subagents**

Dispatch three read-only auditors:

- small auditor A: check validator code against design rules.
- small auditor B: check CLI/task-core output size and static preflight scope.
- medium auditor C: check service rollback / Review v2 ownership boundary.

Each auditor returns findings with file paths and line numbers. Fix all P0/P1 findings before closure.

- [x] **Step 3: Write implementation report**

Report must include:

```markdown
# BlueprintHelper GraphWrite Connectivity Validation Implementation Report

日期：2026-06-02

## 改动原因

## 改动范围

## 实施过程

## 验证结果

## 未纳入范围

## 建议提交范围
```

- [x] **Step 4: Final git hygiene**

Run:

```powershell
git status --short
git diff --check
```

Expected:

- No whitespace errors.
- Changed files are limited to this implementation and required Debug/Report docs.
- No `git add` / `git commit` / `git push` is executed by agents.

## Execution Notes For Subagents

- Use `apply_patch` for manual edits.
- Do not add anonymous namespaces in C++.
- Do not modify legacy/raw GraphWrite paths except to keep them rejected or diagnostic-only.
- Do not move workflow/async/lifecycle logic into UI widgets.
- Do not broaden `bridge call` or MCP paths; CLI/Bridge runtime remains canonical.
- Treat `E:\UE_5.6` as the engine path.
- If UE automation output is noisy, inspect `D:\UEProjects\Template\Saved\Automation\<RunName>\index.json` and `D:\UEProjects\Template\Saved\Logs\Template.log`.

## Self-Review

- Spec coverage: The plan covers generated exec reachability, PureData consumption, Comment/Reroute whitelist, Layout exclusion, preview/execute blocking, mutation rollback, ownership completeness, CLI output, task-core static preflight, and real E2E.
- Placeholder scan: No task contains unresolved placeholders. Each task names concrete files, commands, and expected outcomes.
- Type consistency: The runtime contract uses `FBlueprintGraphWriteConnectivityValidationInput`, `FBlueprintGraphWriteConnectivityValidationResult`, `FBlueprintHelperGraphWriteConnectivityValidator`, and existing `FBlueprintGeneratorDiagnostic` consistently across tasks.
