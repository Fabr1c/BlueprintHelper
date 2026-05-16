# CallFunction TaskSpec Action Resolver Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 鎵╁ `call_function` 鐨勮В鏋愯兘鍔涳紝璁?TaskSpec 涓殑鍑芥暟鍚嶅彲浠ユ寜 UE 缂栬緫鍣ㄥ彲瑙佺殑鍑芥暟鍊欓€夎繘琛屽尮閰嶏紝鍚屾椂涓嶆妸 UE 鍙抽敭鑿滃崟銆乤ction database銆乶ode spawner 鎴栫紪杈戝櫒閫夋嫨鐘舵€佹毚闇茬粰鏅€?Agent銆?
**Architecture:** Agent 浠嶅彧鎻愪氦 `BlueprintHelper.TaskSpec.v1`锛宍call_function` 浠嶅彧浣跨敤 `name` 鍜?`args`銆侻CP/Python compiler 缁х画鐢熸垚璇箟 TaskPlan锛孶E Task Runtime / GraphWrite 鍦?preview 鍜?execute 闃舵璋冪敤鍐呴儴 `CallFunction` resolver銆俁esolver 鍙€熺敤 UE action menu 鐨勫€欓€夋瀯寤哄拰鍥惧吋瀹规€ц繃婊わ紝浣嗚緭鍑虹殑鏄ǔ瀹氬嚱鏁拌韩浠藉拰鍙璁¤瘖鏂紝涓嶈緭鍑虹紪杈戝櫒鑿滃崟鎿嶄綔鏂规硶銆?
**Tech Stack:** Unreal Engine 5.6 Editor C++, BlueprintGraph/Kismet action APIs, BlueprintHelper GraphWrite / TaskRuntime, TypeScript TaskSpec compiler tests, UE Automation tests.

---

## Status

**2026-05-13 source backwrite**

- Task 1 source is integrated: resolver DTOs, qualified query parsing, conservative callable-function candidate universe, deterministic ranking, and dedicated resolver automation tests now exist in source.
- Task 2 source is integrated: raw `call_function.name` is preserved for UE-side resolution, `TextToBlueprintGenerator` exposes `ResolveFunctionForGraph`, `CallFunctionNodeHandler` routes spawning through the resolver, legacy `FindFunctionByName()` is retained as fallback only, and graph-generation resolver tests were added.
- Task 3 source is integrated for the currently supported Merge contract: Merge now resolves inserted function/custom-event calls through the resolver, keeps merge-level `inserted_logic_not_found: call_function resolve failed: ...` error shaping, and has targeted read-back/source coverage for display name, owner-qualified name, and explicit member-prefix blocking.
- Task 4 is integrated on 2026-05-17: TaskRuntime now pre-resolves GraphWrite `call_function` statements for preview/execute, blocks ambiguous/not found/member-prefix calls before write execution, emits compact candidate summaries, and records resolved call identity in runtime result data plus TaskRunJournal metadata.
- Task 5 is integrated against the active workspace layout: `AgentFaceService/task-core` preserves raw owner-qualified names and its Node tests/build were rerun successfully.
- Task 6 is verified for the current automated scope on 2026-05-13: `AgentFaceService/task-core` build and `test:node` passed; UE `Build.bat` passed; `BlueprintHelper.GraphWrite.CallFunctionResolver` passed 8/8; `BlueprintHelper.GraphWrite.TaskRuntime.CallFunction` passed 3/3 with no automation errors; `BlueprintHelper.TaskRuntime` passed 11/11.

---

## 0. 杈圭晫鏀舵暃

### 0.1 Agent-facing 涓嶅彉椤?
- 涓嶆柊澧炴櫘閫?Agent 鍙 CLI 涔嬪鐨勭洿杈惧懡浠ら潰銆?- 涓嶆柊澧?`right_click`銆乣action_menu`銆乣node_spawner`銆乣perform_action`銆乣selected_objects`銆乣context_target_mask`銆乣bindings`銆乣node_class` 绛?TaskSpec 瀛楁銆?- 绗竴鐗堜笉淇敼 `BlueprintLogicStatementSchema` 鐨?Agent-facing 褰㈢姸銆?- 鏅€?TaskSpec 浠嶅啓锛?
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

- `args` 浠嶅彧琛ㄨ揪鍑芥暟鍙傛暟锛屼笉鍏佽鎶?MCP tool args銆丅ridge payload銆乁E action payload 娣峰叆杩欓噷銆?- 褰撳嚱鏁颁笉鍞竴鏃讹紝preview 杩斿洖 blocked銆侫gent 鍙兘淇敼 TaskSpec 鐨?`name` 涓烘洿鍏蜂綋鐨勫嚱鏁板悕锛屼笉鑳藉洖閫€鍒板簳灞傚伐鍏枫€?
### 0.2 鍏佽鐨?`name` 杈撳叆

绗竴鐗堝厑璁?resolver 鍐呴儴璇嗗埆杩欎簺瀛楃涓插舰鎬侊細

| 杈撳叆褰㈡€?| 渚嬪瓙 | 瑙ｆ瀽绛栫暐 |
|---|---|---|
| 鍘熺敓鍑芥暟鍚?| `PrintString` | 绮剧‘浼樺厛锛屽敮涓€鍚庡彲鎵ц |
| DisplayName | `Print String` | 鍙湪鍊欓€夊敮涓€鏃跺彲鎵ц |
| Owner-qualified | `/Script/Engine.KismetSystemLibrary:PrintString` | 绮剧‘鍖归厤 owner class + native function |
| Script dot-qualified | `/Script/Engine.KismetSystemLibrary.PrintString` | 绮剧‘鍖归厤 owner class + native function |
| 鐭?owner-qualified | `KismetSystemLibrary.PrintString` | 浠呭綋 owner class 鍞竴鏃跺彲鎵ц |

鏄庣‘闃绘柇锛?
- `DoorMesh.AddAngularImpulseInDegrees` 杩欑被 component/member 鍓嶇紑鍦ㄧ涓€鐗堜笉鑷姩瑙ｉ噴涓哄綋鍓嶇紪杈戝櫒閫変腑缁勪欢锛屼篃涓嶉潤榛橀檷绾т负鍏ㄥ眬鍑芥暟銆俻review 杩斿洖 `explicit_member_call_not_supported`锛屾彁绀洪渶瑕佸悗缁崟鐙璁?component member call銆?- 绌哄瓧绗︿覆銆佽嚜鐒惰瑷€鍙ュ瓙銆侀渶瑕佸垱寤烘柊鍑芥暟鐨勫悕瀛楃洿鎺?blocked銆?
### 0.3 UE 鍐呴儴鍙敤浣嗕笉鏆撮湶鐨勮兘鍔?
Resolver 鍐呴儴鍙娇鐢細

- `FBlueprintActionContext`
- `FBlueprintActionMenuUtils::MakeContextMenu`
- `FBlueprintActionMenuBuilder`
- `FBlueprintActionMenuUtils::ExtractNodeTemplateFromAction`
- `UEdGraphSchema_K2::CanFunctionBeUsedInGraph`
- `UBlueprintFunctionNodeSpawner`

Resolver 鍐呴儴绂佹浣跨敤锛?
- 褰撳墠 Content Browser 閫夋嫨銆?- 褰撳墠 Level 閫変腑 Actor銆?- 褰撳墠 Blueprint Editor 閫変腑 component/property銆?- 褰撳墠榧犳爣鎷栧嚭鐨?`FromPin`銆?- `SGraphActionMenu` UI widget 鎴栦换浣曢渶瑕佺敤鎴蜂氦浜掔殑鑿滃崟瀹炰緥銆?- 浠?focused editor tab 鎺ㄥ鐩爣鍥俱€?
绗竴鐗?action context 鍙厑璁革細

```text
Blueprints = [TaskSpec target blueprint]
Graphs = [TaskSpec target graph]
Pins = []
SelectedObjects = []
ContextTargetMask = TARGET_Blueprint | TARGET_BlueprintLibraries
bIsContextSensitive = true
```

### 0.4 鑷姩閫夋嫨闂ㄧ

Resolver 蹇呴』鎸変互涓嬮『搴忚В鏋愶細

1. Owner-qualified exact match銆?2. 褰撳墠 Blueprint / parent / library action 涓殑 native name exact match銆?3. DisplayName exact match銆?4. 缂栬緫鍣ㄦ悳绱㈡枃鏈€欓€夊彧鐢ㄤ簬寤鸿鍒楄〃锛涘彧鏈夊€欓€夐泦鍚堟渶缁堝敮涓€涓旂ǔ瀹氳韩浠藉敮涓€鏃舵墠鍙墽琛屻€?
姝т箟瑙勫垯锛?
- 澶氫釜 owner class 涓嬪嚭鐜板悓鍚嶅嚱鏁版椂 blocked銆?- native name 鍜?DisplayName 鍛戒腑涓嶅悓鍑芥暟鏃?blocked銆?- fuzzy 鍊欓€夊垎鏁扮浉杩戞椂 blocked銆?- preview 鍜?execute 涓ゆ瑙ｆ瀽鍑虹殑 stable id 涓嶄竴鑷存椂 blocked銆?
绋冲畾韬唤鏍煎紡锛?
```text
owner_class_path + ":" + native_function_name
```

渚嬪瓙锛?
```text
/Script/Engine.KismetSystemLibrary:PrintString
```

### 0.5 Preview / Execute 鍚堝悓

Preview 琛屼负锛?
- 瀵规瘡涓?`call_function` statement 瑙ｆ瀽鍊欓€夈€?- 鍞竴鏃跺湪 preview summary/debug 涓褰?normalized summary锛歚query`銆乣stable_id`銆乣owner_class`銆乣native_name`銆乣display_name`銆乣node_class`銆?- 姝т箟鏃惰繑鍥?`preview_blocked`锛岄敊璇爜 `ambiguous_function_call`锛屽垪鍑烘渶澶?8 涓€欓€夋憳瑕併€?- 涓嶅瓨鍦ㄦ椂杩斿洖 `preview_blocked`锛岄敊璇爜 `function_call_not_found`锛屽垪鍑烘渶澶?8 涓缓璁€欓€夈€?
Execute 琛屼负锛?
- execute 鍓嶉噸鏂拌В鏋愩€?- 鑻?TaskPlan 鎴?preview summary 涓湁 expected stable id锛屽垯蹇呴』鍖归厤銆?- 鑻ヨВ鏋愮粨鏋滃彉鎴愭涔夈€佷笉瀛樺湪鎴?stable id 鍙樺寲锛屽仠姝㈠啓鍏ャ€?- 鐪熷疄鍐欏叆鍚庝粛璧扮幇鏈?compile/save/review/debug/transaction 娴佺▼銆?
---

## 1. 鏂囦欢缁撴瀯

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
- `BlueprintHelper/Resources/AgentGuide/Reference/04_Tool_Surface_Field_Templates_20260512.md`
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

- [x] **Step 1: Define resolver DTOs**

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

- [x] **Step 2: Implement query parsing**

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

- [x] **Step 3: Build UE action candidates without editor UI state**

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

- [x] **Step 4: Implement deterministic ranking**

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

- [x] **Step 5: Add resolver automation tests**

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

- [x] **Step 1: Stop stripping qualified function names before resolver**

Change `FunctionNameForGenerator()` so `call` nodes preserve the raw trimmed TaskSpec value:

```cpp
static FString FunctionNameForGenerator(const FString& InFunction)
{
	return InFunction.TrimStartAndEnd();
}
```

Reason: `/Script/Engine.KismetSystemLibrary:PrintString` must reach the resolver intact. Stripping owner data before UE-side resolution removes the only deterministic disambiguation input available to TaskSpec.

- [x] **Step 2: Add context-aware resolve helper to TextToBlueprintGenerator**

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

- [x] **Step 3: Update CallFunctionNodeHandler**

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

- [x] **Step 4: Keep legacy direct resolver as fallback only for expert/internal paths**

Do not delete `FindFunctionByName()` in this slice. Mark it as legacy fallback in a comment near the function:

```cpp
// Legacy fallback for older internal node handlers. TaskSpec call_function should use
// ResolveFunctionForGraph so graph compatibility and ambiguity checks run before spawning.
```

- [x] **Step 5: Add graph generation tests**

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

- [x] **Step 1: Replace local merge function resolution**

`ResolveMergeCallableFunction()` currently checks skeleton/generated/parent class and then falls back to global `FindFunctionByName()`. Replace this with the new resolver using `Context.Blueprint`, `Context.Graph`, and `Request.InsertedFunctionName`.

Expected behavior:

```text
PrintString -> resolves if unique in graph/library context
Print String -> resolves if unique
/Script/Engine.KismetSystemLibrary:PrintString -> resolves exactly
DoorMesh.AddAngularImpulseInDegrees -> blocks with explicit_member_call_not_supported
```

- [x] **Step 2: Keep merge error codes stable**

If resolver returns not found or ambiguous, preserve merge-level category:

```text
inserted_logic_not_found: call_function resolve failed: <resolver message>
```

This keeps existing callers from treating resolver diagnostics as a new low-level capability.

- [x] **Step 3: Add merge read-back tests**

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

- [x] **Step 1: Resolve during dry-run**

When TaskRuntime lowers a `call_function` statement in dry-run, resolve the function before constructing node JSON. A blocked resolver result must produce preview blocked and no write.

Error shape:

```text
code = ambiguous_function_call | function_call_not_found | explicit_member_call_not_supported
stage = DryRun
path = write.ops[<opIndex>].body.statements[<statementIndex>].name
```

- [x] **Step 2: Include compact candidate summaries**

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

- [x] **Step 3: Store resolved identity in internal runtime facts**

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

- [x] **Step 4: Add dry-run diagnostics tests**

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

### 2026-05-17 implementation backwrite

- Implemented in `BlueprintHelperTaskRuntimeService.cpp`, resolver candidate serialization, Append dry-run issue JSON, and GraphStatement explicit-member fallback behavior. `TextToBlueprintGenerator.cpp` did not require a Task 4 change because the covered path is TaskRuntime preview/execute pre-resolution plus GraphWrite compact diagnostics.
- Preview blocked diagnostics now use `dry_run.result = blocked`, `can_execute = false`, error `stage = dry_run`, and paths such as `write.ops[0].body.statements[0].name` / `.target`.
- Compact candidate groups now use `query` and candidate objects limited to stable/display/owner/native identity fields; no node spawner/action/binding/debug bundle details are emitted.
- Successful preview and execute attach `runtime_facts.resolved_call_functions[]`; execute journals carry the same resolved identity facts. `display_name` is locale-dependent, so tests assert stable/native identity and non-empty display text.
- Added and passed: `BlueprintHelper.GraphWrite.CallFunctionResolver.PreviewBlocksAmbiguousFunction`, `BlueprintHelper.GraphWrite.CallFunctionResolver.PreviewReportsCandidateSummaries`, and `BlueprintHelper.GraphWrite.CallFunctionResolver.ExecuteRevalidatesStableId`.
- Verification:
  - `Build.bat TemplateEditor Win64 Development ... UBT-Task4-20260517-rerun.log`: passed.
  - `Automation RunTests BlueprintHelper.GraphWrite.CallFunctionResolver`: 11 total, 0 failed, 1 EOS offline warning, report `Saved/Automation/Task4_CallFunction_20260517_002/index.json`.
  - `Automation RunTests BlueprintHelper.GraphWrite.TaskRuntime.CallFunction`: 3 total, 0 failed, 1 EOS offline warning, report `Saved/Automation/Task4_TaskRuntime_CallFunction_20260517_001/index.json`.
  - `Automation RunTests BlueprintHelper.RuntimeDiagnostics.Debug`: 9 total, 0 failed, report `Saved/Automation/Task4_RuntimeDiagnostics_Debug_20260517_001/index.json`.

---

## 6. Task 5: Keep TaskSpec Compiler And Docs TaskSpec-first

**Files:**

- Validate: `ClaudePlugin/mcp/src/task/compiler/task-compiler.ts`
- Validate: `ClaudePlugin/mcp/src/task/schema/task-schemas.ts`
- Modify: `BlueprintHelper/Resources/AgentGuide/Workflows/05_Edit_Blueprint_Workflow.md`
- Modify: `BlueprintHelper/Resources/AgentGuide/Reference/04_Tool_Surface_Field_Templates_20260512.md`
- Modify: `BlueprintHelper/Resources/Docs/TaskSpec_TaskPlan_Contract_20260504.md`
- Test: `ClaudePlugin/mcp/src/tests/task/task-p1-schema.test.ts`
- Test: `ClaudePlugin/mcp/src/tests/task/task-contract.test.ts`

- [x] **Step 1: Verify compiler preserves raw name**

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

- [x] **Step 2: Keep schema passthrough but document narrower semantics**

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

- [x] **Step 3: Add forbidden example to docs**

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

- [x] **Step 4: Run MCP tests**

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

- [x] **Step 4: Run MCP task tests**

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

