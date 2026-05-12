# CallFunction TaskSpec Action Resolver Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 扩宽 `call_function` 的解析能力，让 TaskSpec 中的函数名可以按 UE 编辑器可见的函数候选进行匹配，同时不把 UE 右键菜单、action database、node spawner 或编辑器选择状态暴露给普通 Agent。

**Architecture:** Agent 仍只提交 `BlueprintHelper.TaskSpec.v1`，`call_function` 仍只使用 `name` 和 `args`。MCP/Python compiler 继续生成语义 TaskPlan，UE Task Runtime / GraphWrite 在 preview 和 execute 阶段调用内部 `CallFunction` resolver。Resolver 可借用 UE action menu 的候选构建和图兼容性过滤，但输出的是稳定函数身份和可审计诊断，不输出编辑器菜单操作方法。

**Tech Stack:** Unreal Engine 5.6 Editor C++, BlueprintGraph/Kismet action APIs, BlueprintHelper GraphWrite / TaskRuntime, TypeScript TaskSpec compiler tests, UE Automation tests.

---

## 0. 边界收敛

### 0.1 Agent-facing 不变项

- 不新增普通 Agent 可见 MCP 工具。
- 不新增 `right_click`、`action_menu`、`node_spawner`、`perform_action`、`selected_objects`、`context_target_mask`、`bindings`、`node_class` 等 TaskSpec 字段。
- 第一版不修改 `BlueprintLogicStatementSchema` 的 Agent-facing 形状。
- 普通 TaskSpec 仍写：

```json
{
  "kind": "call_function",
  "name": "Print String",
  "args": {
    "InString": {
      "kind": "literal",
      "value_type": "string",
      "value": "message"
    }
  }
}
```

- `args` 仍只表达函数参数，不允许把 MCP tool args、Bridge payload、UE action payload 混入这里。
- 当函数不唯一时，preview 返回 blocked。Agent 只能修改 TaskSpec 的 `name` 为更具体的函数名，不能回退到底层工具。

### 0.2 允许的 `name` 输入

第一版允许 resolver 内部识别这些字符串形态：

| 输入形态 | 例子 | 解析策略 |
|---|---|---|
| 原生函数名 | `PrintString` | 精确优先，唯一后可执行 |
| DisplayName | `Print String` | 只在候选唯一时可执行 |
| Owner-qualified | `/Script/Engine.KismetSystemLibrary:PrintString` | 精确匹配 owner class + native function |
| Script dot-qualified | `/Script/Engine.KismetSystemLibrary.PrintString` | 精确匹配 owner class + native function |
| 短 owner-qualified | `KismetSystemLibrary.PrintString` | 仅当 owner class 唯一时可执行 |

明确阻断：

- `DoorMesh.AddAngularImpulseInDegrees` 这类 component/member 前缀在第一版不自动解释为当前编辑器选中组件，也不静默降级为全局函数。preview 返回 `explicit_member_call_not_supported`，提示需要后续单独设计 component member call。
- 空字符串、自然语言句子、需要创建新函数的名字直接 blocked。

### 0.3 UE 内部可用但不暴露的能力

Resolver 内部可使用：

- `FBlueprintActionContext`
- `FBlueprintActionMenuUtils::MakeContextMenu`
- `FBlueprintActionMenuBuilder`
- `FBlueprintActionMenuUtils::ExtractNodeTemplateFromAction`
- `UEdGraphSchema_K2::CanFunctionBeUsedInGraph`
- `UBlueprintFunctionNodeSpawner`

Resolver 内部禁止使用：

- 当前 Content Browser 选择。
- 当前 Level 选中 Actor。
- 当前 Blueprint Editor 选中 component/property。
- 当前鼠标拖出的 `FromPin`。
- `SGraphActionMenu` UI widget 或任何需要用户交互的菜单实例。
- 从 focused editor tab 推导目标图。

第一版 action context 只允许：

```text
Blueprints = [TaskSpec target blueprint]
Graphs = [TaskSpec target graph]
Pins = []
SelectedObjects = []
ContextTargetMask = TARGET_Blueprint | TARGET_BlueprintLibraries
bIsContextSensitive = true
```

### 0.4 自动选择门禁

Resolver 必须按以下顺序解析：

1. Owner-qualified exact match。
2. 当前 Blueprint / parent / library action 中的 native name exact match。
3. DisplayName exact match。
4. 编辑器搜索文本候选只用于建议列表；只有候选集合最终唯一且稳定身份唯一时才可执行。

歧义规则：

- 多个 owner class 下出现同名函数时 blocked。
- native name 和 DisplayName 命中不同函数时 blocked。
- fuzzy 候选分数相近时 blocked。
- preview 和 execute 两次解析出的 stable id 不一致时 blocked。

稳定身份格式：

```text
owner_class_path + ":" + native_function_name
```

例子：

```text
/Script/Engine.KismetSystemLibrary:PrintString
```

### 0.5 Preview / Execute 合同

Preview 行为：

- 对每个 `call_function` statement 解析候选。
- 唯一时在 preview summary/debug 中记录 normalized summary：`query`、`stable_id`、`owner_class`、`native_name`、`display_name`、`node_class`。
- 歧义时返回 `preview_blocked`，错误码 `ambiguous_function_call`，列出最多 8 个候选摘要。
- 不存在时返回 `preview_blocked`，错误码 `function_call_not_found`，列出最多 8 个建议候选。

Execute 行为：

- execute 前重新解析。
- 若 TaskPlan 或 preview summary 中有 expected stable id，则必须匹配。
- 若解析结果变成歧义、不存在或 stable id 变化，停止写入。
- 真实写入后仍走现有 compile/save/review/debug/transaction 流程。

---

## 1. 文件结构

### Create

- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperCallFunctionResolverTests.cpp`

### Modify

- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/TextToBlueprintGenerator.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/TextToBlueprintGenerator.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/NodeHandlers/CallFunctionNodeHandler.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Shared/Services/BlueprintHelperAgentImportService.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp`
- `BlueprintHelper/Resources/AgentGuide/Workflows/05_Edit_Blueprint_Workflow.md`
- `BlueprintHelper/Resources/AgentGuide/Reference/04_MCP_Field_Templates_20260507.md`
- `BlueprintHelper/Resources/Docs/TaskSpec_TaskPlan_Contract_20260504.md`

### Validate

- `ClaudePlugin/mcp/src/task/compiler/task-compiler.ts`
- `ClaudePlugin/mcp/src/task/schema/task-schemas.ts`
- `ClaudePlugin/mcp/src/tests/task/task-p1-schema.test.ts`
- `ClaudePlugin/mcp/src/tests/task/task-contract.test.ts`

TypeScript files should remain unchanged unless a regression proves the compiler is rewriting `call_function.name`. First slice should preserve raw `name`.

---

## 2. Task 1: Add Internal CallFunction Resolver

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperCallFunctionResolverTests.cpp`

- [ ] **Step 1: Define resolver DTOs**

Create the header with this public contract:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Math/Vector2D.h"

class UBlueprint;
class UEdGraph;
class UFunction;
class UK2Node;
class UK2Node_CallFunction;

enum class EBlueprintHelperCallFunctionResolveStatus : uint8
{
	Resolved,
	Ambiguous,
	NotFound,
	Blocked
};

struct FBlueprintHelperCallFunctionCandidate
{
	FString StableId;
	FString OwnerClassPath;
	FString NativeFunctionName;
	FString DisplayName;
	FString Category;
	FString NodeClassPath;
	FString MatchReason;
	int32 Score = 0;
	bool bGraphCompatible = false;
	TWeakObjectPtr<UFunction> Function;
	TSubclassOf<UK2Node_CallFunction> NodeClass;
};

struct FBlueprintHelperCallFunctionResolveRequest
{
	UBlueprint* Blueprint = nullptr;
	UEdGraph* Graph = nullptr;
	FString Query;
	TArray<FString> ArgumentNames;
	bool bAllowFuzzyUnique = true;
	int32 MaxCandidates = 8;
};

struct FBlueprintHelperCallFunctionResolveResult
{
	EBlueprintHelperCallFunctionResolveStatus Status = EBlueprintHelperCallFunctionResolveStatus::NotFound;
	FString ErrorCode;
	FString Message;
	FBlueprintHelperCallFunctionCandidate Selected;
	TArray<FBlueprintHelperCallFunctionCandidate> Candidates;

	bool IsResolved() const
	{
		return Status == EBlueprintHelperCallFunctionResolveStatus::Resolved && Selected.Function.IsValid();
	}
};

class BLUEPRINTHELPER_API FBlueprintHelperCallFunctionResolver
{
public:
	static FBlueprintHelperCallFunctionResolveResult Resolve(const FBlueprintHelperCallFunctionResolveRequest& Request);
	static FString MakeStableId(const UFunction* Function);
	static bool TryParseQualifiedQuery(const FString& Query, FString& OutOwner, FString& OutFunction);
	static UK2Node* SpawnResolvedNode(UEdGraph* Graph, const FBlueprintHelperCallFunctionCandidate& Candidate, const FVector2D& Location, FString& OutError);
};
```

- [ ] **Step 2: Implement query parsing**

Rules:

```text
/Script/Engine.KismetSystemLibrary:PrintString -> owner=/Script/Engine.KismetSystemLibrary, function=PrintString
/Script/Engine.KismetSystemLibrary.PrintString -> owner=/Script/Engine.KismetSystemLibrary, function=PrintString
KismetSystemLibrary.PrintString -> owner=KismetSystemLibrary, function=PrintString
DoorMesh.AddAngularImpulseInDegrees -> blocked if DoorMesh is not an owner class candidate
```

Expected blocked error for explicit component/member prefix:

```text
ErrorCode = explicit_member_call_not_supported
Message = call_function.name uses an explicit member prefix; first slice supports graph/self/library calls only.
```

- [ ] **Step 3: Build UE action candidates without editor UI state**

Implementation outline:

```cpp
FBlueprintActionContext Context;
Context.Blueprints.Add(Request.Blueprint);
Context.Graphs.Add(Request.Graph);

FBlueprintActionMenuBuilder Builder;
FBlueprintActionMenuUtils::MakeContextMenu(
	Context,
	true,
	EContextTargetFlags::TARGET_Blueprint | EContextTargetFlags::TARGET_BlueprintLibraries,
	Builder);
Builder.RebuildActionList();

for (int32 Index = 0; Index < Builder.GetNumActions(); ++Index)
{
	TSharedPtr<FEdGraphSchemaAction> Action = Builder.GetSchemaAction(Index);
	const UK2Node* Template = FBlueprintActionMenuUtils::ExtractNodeTemplateFromAction(Action);
	const UK2Node_CallFunction* CallTemplate = Cast<UK2Node_CallFunction>(Template);
	if (!CallTemplate)
	{
		continue;
	}
	const UFunction* Function = CallTemplate->GetTargetFunction();
	if (!Function)
	{
		continue;
	}
	// Build stable candidate. Do not expose Action or Spawner to Agent-facing results.
}
```

If `ExtractNodeTemplateFromAction` is insufficient for some entries, keep a conservative fallback that scans callable `UFunction` objects and validates them through `UEdGraphSchema_K2::CanFunctionBeUsedInGraph`. The fallback must not skip graph compatibility checks.

- [ ] **Step 4: Implement deterministic ranking**

Ranking must produce deterministic order:

```text
1000 owner-qualified exact native
900 native exact
850 display exact
700 full search text exact token
500 all query tokens contained in full search text
0 otherwise
```

Tie-break order:

```text
OwnerClassPath ascending
NativeFunctionName ascending
DisplayName ascending
```

Execution can only select a candidate when:

```text
top score >= 850 and no second candidate has the same stable id conflict
or top score >= 700 and it is the only compatible candidate above 0
```

- [ ] **Step 5: Add resolver automation tests**

Add tests under `BlueprintHelper.GraphWrite.CallFunctionResolver`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCallFunctionResolverPrintStringNativeTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.PrintStringNativeNameResolves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCallFunctionResolverPrintStringDisplayTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.PrintStringDisplayNameResolves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCallFunctionResolverQualifiedTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.QualifiedNameResolvesStableId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCallFunctionResolverAmbiguousTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.AmbiguousShortNameBlocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCallFunctionResolverMemberPrefixTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.MemberPrefixBlocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
```

Assertions:

```cpp
TestEqual(TEXT("status"), Result.Status, EBlueprintHelperCallFunctionResolveStatus::Resolved);
TestEqual(TEXT("stable id"), Result.Selected.StableId, FString(TEXT("/Script/Engine.KismetSystemLibrary:PrintString")));
TestTrue(TEXT("function valid"), Result.Selected.Function.IsValid());
TestTrue(TEXT("graph compatible"), Result.Selected.bGraphCompatible);
```

---

## 3. Task 2: Preserve TaskSpec `name` And Route Spawning Through Resolver

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Shared/Services/BlueprintHelperAgentImportService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/NodeHandlers/CallFunctionNodeHandler.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/TextToBlueprintGenerator.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/TextToBlueprintGenerator.h`

- [ ] **Step 1: Stop stripping qualified function names before resolver**

Change `FunctionNameForGenerator()` so `call` nodes preserve the raw trimmed TaskSpec value:

```cpp
static FString FunctionNameForGenerator(const FString& InFunction)
{
	return InFunction.TrimStartAndEnd();
}
```

Reason: `/Script/Engine.KismetSystemLibrary:PrintString` must reach the resolver intact. Stripping owner data before UE-side resolution removes the only deterministic disambiguation input available to TaskSpec.

- [ ] **Step 2: Add context-aware resolve helper to TextToBlueprintGenerator**

Add a wrapper:

```cpp
static FBlueprintHelperCallFunctionResolveResult ResolveFunctionForGraph(
	UEdGraph* TargetGraph,
	const FString& FunctionQuery,
	const TMap<FString, FString>& DefaultValues);
```

Implementation:

```cpp
UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
FBlueprintHelperCallFunctionResolveRequest Request;
Request.Blueprint = Blueprint;
Request.Graph = TargetGraph;
Request.Query = FunctionQuery;
DefaultValues.GetKeys(Request.ArgumentNames);
return FBlueprintHelperCallFunctionResolver::Resolve(Request);
```

- [ ] **Step 3: Update CallFunctionNodeHandler**

Replace direct `FindFunctionByName()` use:

```cpp
const FBlueprintHelperCallFunctionResolveResult ResolveResult =
	TextToBlueprintGenerator::ResolveFunctionForGraph(TargetGraph, NodeData.FunctionName, NodeData.DefaultValues);

if (!ResolveResult.IsResolved())
{
	OutError = ResolveResult.Message.IsEmpty()
		? FString::Printf(TEXT("call_function resolve failed: %s"), *NodeData.FunctionName)
		: ResolveResult.Message;
	return nullptr;
}

UK2Node* SpawnedNode = FBlueprintHelperCallFunctionResolver::SpawnResolvedNode(
	TargetGraph,
	ResolveResult.Selected,
	FVector2D(NodeData.X, NodeData.Y),
	OutError);

if (SpawnedNode)
{
	TextToBlueprintGenerator::ApplyDefaultValues(SpawnedNode, NodeData.DefaultValues, NodeData.Id);
}
return SpawnedNode;
```

- [ ] **Step 4: Keep legacy direct resolver as fallback only for expert/internal paths**

Do not delete `FindFunctionByName()` in this slice. Mark it as legacy fallback in a comment near the function:

```cpp
// Legacy fallback for older internal node handlers. TaskSpec call_function should use
// ResolveFunctionForGraph so graph compatibility and ambiguity checks run before spawning.
```

- [ ] **Step 5: Add graph generation tests**

Extend or add automation tests that generate a graph from TaskSpec-import style nodes:

```text
BlueprintHelper.GraphWrite.CallFunctionResolver.GeneratorDisplayNameSpawnsPrintString
BlueprintHelper.GraphWrite.CallFunctionResolver.GeneratorQualifiedNameSpawnsPrintString
BlueprintHelper.GraphWrite.CallFunctionResolver.GeneratorAmbiguousNameDoesNotSpawn
```

Each test must inspect the generated node:

```cpp
UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(GeneratedNode);
TestNotNull(TEXT("call node"), CallNode);
TestEqual(TEXT("target function"), CallNode->GetFunctionName(), FName(TEXT("PrintString")));
```

---

## 4. Task 3: Apply Resolver To Merge Function Calls

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteBlockScopedAnchorTests.cpp`

- [ ] **Step 1: Replace local merge function resolution**

`ResolveMergeCallableFunction()` currently checks skeleton/generated/parent class and then falls back to global `FindFunctionByName()`. Replace this with the new resolver using `Context.Blueprint`, `Context.Graph`, and `Request.InsertedFunctionName`.

Expected behavior:

```text
PrintString -> resolves if unique in graph/library context
Print String -> resolves if unique
/Script/Engine.KismetSystemLibrary:PrintString -> resolves exactly
DoorMesh.AddAngularImpulseInDegrees -> blocks with explicit_member_call_not_supported
```

- [ ] **Step 2: Keep merge error codes stable**

If resolver returns not found or ambiguous, preserve merge-level category:

```text
inserted_logic_not_found: call_function resolve failed: <resolver message>
```

This keeps existing callers from treating resolver diagnostics as a new low-level capability.

- [ ] **Step 3: Add merge read-back tests**

Add targeted coverage:

```text
BlueprintHelper.GraphWrite.BlockScopedAnchors.MergeInsertFlowDisplayNameFunctionCall
BlueprintHelper.GraphWrite.BlockScopedAnchors.MergeInsertFlowQualifiedFunctionCall
BlueprintHelper.GraphWrite.BlockScopedAnchors.MergeMemberPrefixBlocks
```

Assertions:

```cpp
TestTrue(TEXT("merge preview blocks member prefix"), PreviewResult.Status == EBlueprintHelperToolStatus::PreviewBlocked);
TestTrue(TEXT("message has resolver code"), ErrorMessage.Contains(TEXT("explicit_member_call_not_supported")));
```

---

## 5. Task 4: Preview Diagnostics And Journal Identity

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/TextToBlueprintGenerator.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/RuntimeDiagnostics/BlueprintHelperDebugCaseTests.cpp`

- [ ] **Step 1: Resolve during dry-run**

When TaskRuntime lowers a `call_function` statement in dry-run, resolve the function before constructing node JSON. A blocked resolver result must produce preview blocked and no write.

Error shape:

```text
code = ambiguous_function_call | function_call_not_found | explicit_member_call_not_supported
stage = DryRun
path = write.ops[<opIndex>].body.statements[<statementIndex>].name
```

- [ ] **Step 2: Include compact candidate summaries**

For ambiguous/not found responses, include compact diagnostics:

```json
{
  "query": "Print",
  "candidates": [
    {
      "stable_id": "/Script/Engine.KismetSystemLibrary:PrintString",
      "display_name": "Print String",
      "owner_class": "/Script/Engine.KismetSystemLibrary"
    }
  ]
}
```

Do not include `FEdGraphSchemaAction`, `UBlueprintNodeSpawner`, menu section names, binding objects, selected object payloads, local DebugBundle paths, or raw source content.

- [ ] **Step 3: Store resolved identity in internal runtime facts**

On successful preview and execute, record:

```json
{
  "resolved_call_functions": [
    {
      "statement_path": "write.ops[0].body.statements[0]",
      "query": "Print String",
      "stable_id": "/Script/Engine.KismetSystemLibrary:PrintString",
      "native_name": "PrintString",
      "display_name": "Print String"
    }
  ]
}
```

This is runtime/debug metadata. It is not a new Agent-authored TaskSpec field.

- [ ] **Step 4: Add dry-run diagnostics tests**

Add tests:

```text
BlueprintHelper.GraphWrite.CallFunctionResolver.PreviewBlocksAmbiguousFunction
BlueprintHelper.GraphWrite.CallFunctionResolver.PreviewReportsCandidateSummaries
BlueprintHelper.GraphWrite.CallFunctionResolver.ExecuteRevalidatesStableId
```

Expected preview blocked assertion:

```cpp
TestEqual(TEXT("error code"), Result.Error.Code, FString(TEXT("ambiguous_function_call")));
TestTrue(TEXT("candidate summary"), Result.DebugSummary.Contains(TEXT("stable_id")));
TestFalse(TEXT("no node spawner leak"), Result.DebugSummary.Contains(TEXT("UBlueprintFunctionNodeSpawner")));
```

---

## 6. Task 5: Keep TaskSpec Compiler And Docs TaskSpec-first

**Files:**

- Validate: `ClaudePlugin/mcp/src/task/compiler/task-compiler.ts`
- Validate: `ClaudePlugin/mcp/src/task/schema/task-schemas.ts`
- Modify: `BlueprintHelper/Resources/AgentGuide/Workflows/05_Edit_Blueprint_Workflow.md`
- Modify: `BlueprintHelper/Resources/AgentGuide/Reference/04_MCP_Field_Templates_20260507.md`
- Modify: `BlueprintHelper/Resources/Docs/TaskSpec_TaskPlan_Contract_20260504.md`
- Test: `ClaudePlugin/mcp/src/tests/task/task-p1-schema.test.ts`
- Test: `ClaudePlugin/mcp/src/tests/task/task-contract.test.ts`

- [ ] **Step 1: Verify compiler preserves raw name**

Add or update a test:

```ts
assert.deepEqual(compiledNode, {
  id: 'entry_stmt_1',
  kind: 'call',
  function: '/Script/Engine.KismetSystemLibrary:PrintString',
  inputs: {
    InString: 'message',
  },
});
```

Expected result: TypeScript compiler does not parse, strip, or validate UE owner qualifiers. UE resolver owns function identity.

- [ ] **Step 2: Keep schema passthrough but document narrower semantics**

No schema addition is required for first slice. The docs should say:

```text
call_function.name may be a native function name, a Blueprint display name, or an owner-qualified native name.
Preview resolves the function against the target Blueprint graph. If the name is ambiguous, change name to an owner-qualified native name and preview again.
```

Do not mention:

```text
right-click menu
action database
node spawner
PerformAction
selected objects
context target mask
```

- [ ] **Step 3: Add forbidden example to docs**

Document this as blocked:

```json
{
  "kind": "call_function",
  "name": "DoorMesh.AddAngularImpulseInDegrees",
  "args": {}
}
```

Required wording:

```text
Explicit component/member calls are not part of the first CallFunction resolver slice. Preview blocks them instead of interpreting editor selection state.
```

- [ ] **Step 4: Run MCP tests**

Command:

```powershell
Push-Location ClaudePlugin\mcp
npm run build
npm run test:node
Pop-Location
```

Expected:

```text
TypeScript build succeeds.
Node task contract/schema tests pass.
```

---

## 7. Task 6: Verification

**Files:**

- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperCallFunctionResolverTests.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteBlockScopedAnchorTests.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/RuntimeDiagnostics/BlueprintHelperDebugCaseTests.cpp`

- [ ] **Step 1: Build plugin**

Command:

```powershell
& 'F:/UE_5.6/Engine/Build/BatchFiles/RunUAT.bat' BuildPlugin `
  -Plugin='G:/UnrealPractise/MrStone/Plugins/BlueprintHelper/BlueprintHelper/BlueprintHelper.uplugin' `
  -Package='G:/UnrealPractise/MrStone/Plugins/BlueprintHelper/PluginOut/BlueprintHelper_CallFunctionResolver' `
  -TargetPlatforms=Win64 `
  -Rocket
```

Expected:

```text
BUILD SUCCESSFUL
```

- [ ] **Step 2: Run resolver automation**

Command:

```powershell
& 'F:/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' `
  'G:/UnrealPractise/MrStone/MrStone.uproject' `
  -Unattended -NullRHI -NoSplash -NoSound `
  -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.CallFunctionResolver; Quit' `
  -TestExit='Automation Test Queue Empty' `
  -ReportOutputPath='G:/UnrealPractise/MrStone/Saved/Automation/CallFunctionResolver'
```

Expected:

```text
0 failed
PrintStringNativeNameResolves passed
PrintStringDisplayNameResolves passed
QualifiedNameResolvesStableId passed
AmbiguousShortNameBlocks passed
MemberPrefixBlocks passed
```

- [ ] **Step 3: Run GraphWrite affected groups**

Command:

```powershell
& 'F:/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' `
  'G:/UnrealPractise/MrStone/MrStone.uproject' `
  -Unattended -NullRHI -NoSplash -NoSound `
  -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite; Quit' `
  -TestExit='Automation Test Queue Empty' `
  -ReportOutputPath='G:/UnrealPractise/MrStone/Saved/Automation/GraphWrite_CallFunctionResolver'
```

Expected:

```text
0 failed
Existing append/replace/patch/merge tests still pass.
New resolver tests pass.
```

- [ ] **Step 4: Run MCP task tests**

Command:

```powershell
Push-Location 'G:/UnrealPractise/MrStone/Plugins/BlueprintHelper/ClaudePlugin/mcp'
npm run test:node
Pop-Location
```

Expected:

```text
task schema and contract tests pass.
call_function.name remains raw.
```

---

## 8. Acceptance Criteria

- `call_function.name = "Print String"` can resolve to `/Script/Engine.KismetSystemLibrary:PrintString` when unique in target graph context.
- `/Script/Engine.KismetSystemLibrary:PrintString` resolves deterministically and records that stable id.
- Ambiguous names block at preview with candidate summaries.
- Explicit member/component prefixes block at preview and never consult current editor selection.
- No new Agent-facing MCP tool is added.
- No new Agent-authored TaskSpec field is required.
- AgentGuide describes only TaskSpec behavior, not UE right-click/action menu mechanics.
- Existing `call_function` native-name behavior remains compatible.
- Execute revalidates function identity before writing nodes.
- Debug/preview summaries do not leak node spawners, raw UE action objects, selected object payloads, raw source content, or local DebugBundle artifact paths.

## 9. Deferred Scope

- Pin-drag context search.
- Component member calls such as `DoorMesh.AddAngularImpulseInDegrees`.
- Level actor selected-object function calls.
- Content Browser selected asset actions.
- Latent call placement policy beyond existing `CanFunctionBeUsedInGraph` compatibility.
- Auto-creating missing functions or events.
- Agent-facing function search tool.

