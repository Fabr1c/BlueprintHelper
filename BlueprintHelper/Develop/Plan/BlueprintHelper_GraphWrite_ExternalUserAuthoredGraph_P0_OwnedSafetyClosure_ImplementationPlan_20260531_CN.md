# GraphWrite External User-Authored Graph P0 Owned Safety Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在开放 external graph 写入前封口现有 owned-only 主路径：runtime 强制消费 ownership constraints，移除 legacy anchor fallback，修复 append 多 entry ownership 覆盖，并禁止 replace owned path 隐式接管 user-authored entry。

**Architecture:** 新增集中式 `FBlueprintHelperGraphWriteDomainPolicy`。owned resolver 只接受 metadata-backed `block_id`。Append、Replace、Patch、Merge 都通过同一 domain policy 进入服务，不允许 service 本地各自解释 ownership。

**Tech Stack:** TypeScript、Zod、UE 5.6 C++、BlueprintHelper Task Runtime、GraphWrite Automation Tests。
---

## Scope

完成后：

- existing owned GraphWrite 仍可工作；
- `allow_modify_user_nodes=true` 继续被拒绝；
- runtime 不再忽略 `constraints.ownership_scope`；
- owned resolver 不再接受 comment fallback 或无 `block_id` path fallback；
- external 写入仍然不可执行。

## Task 1: RED TypeScript Contract Tests

**Files:**

- Modify: `AgentFaceService/task-core/src/task/schema/task-schemas.op-coverage-extension.test.ts`
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.replace.test.ts`
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.create.test.ts`

- [ ] **Step 1: Add compiler rejection cases**

```ts
assertCompileError(
  makeGraphWriteTaskSpec({ scope_policy: { graph_name: 'EventGraph', allow_modify_user_nodes: true } }),
  'unsupported_scope_policy',
);
```

Add cases proving owned TaskPlan steps lower with:

```ts
constraints: {
  allow_modify_user_nodes: false,
  ownership_scope: 'blueprinthelper_owned',
}
```

- [ ] **Step 2: Run RED**

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
```

Expected: new assertions expose any TaskPlan shape that omits owned constraints.

## Task 2: Add Central Domain Policy

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Policy/BlueprintHelperGraphWriteDomainPolicy.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Policy/BlueprintHelperGraphWriteDomainPolicy.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp`

- [ ] **Step 1: Define domain types**

```cpp
enum class EBlueprintHelperGraphWriteTargetDomain : uint8
{
	BlueprintHelperOwned,
	ExternalUserAuthored
};

struct FBlueprintHelperGraphWriteDomainPolicyRequest
{
	EBlueprintHelperGraphWriteTargetDomain Domain = EBlueprintHelperGraphWriteTargetDomain::BlueprintHelperOwned;
	FString Strategy;
	FString OwnershipScope;
	bool bAllowModifyUserNodes = false;
	TArray<FString> AllowedExternalMutations;
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphWriteDomainPolicy
{
public:
	static bool ValidateOwnedRequest(const FBlueprintHelperGraphWriteDomainPolicyRequest& Request, FString& OutError);
	static bool ValidateExternalRequest(const FBlueprintHelperGraphWriteDomainPolicyRequest& Request, FString& OutError);
};
```

- [ ] **Step 2: Enforce owned runtime constraints**

In `TryBuildGraphWriteIrPayload`, parse and validate:

```cpp
constraints.ownership_scope == "blueprinthelper_owned"
constraints.allow_modify_user_nodes == false
write.strategy == "owned_graph_edit"
```

Missing or mismatched constraints return `unsupported_scope_policy`.

- [ ] **Step 3: Add UE contract tests**

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperGraphWritePlanCacheTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteLegacyMainlineContractTests.cpp`

Add tests:

- `BlueprintHelper.TaskRuntime.GraphWrite.RejectsMissingOwnershipScope`
- `BlueprintHelper.TaskRuntime.GraphWrite.RejectsAllowModifyUserNodes`
- `BlueprintHelper.GraphWrite.LegacyMainline.OwnedDomainPolicyCentralized`

## Task 3: Remove Legacy Owned Anchor Fallbacks

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnedBlockAnchorResolver.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnedBlockAnchorResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteBlockScopedResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteBlockScopedAnchorTests.cpp`

- [ ] **Step 1: Add RED tests**

Replace legacy success expectations with rejection:

```cpp
TestFalse(TEXT("comment fallback is rejected"), Resolver.ResolveNode(Graph, Request, OutNode, Error));
TestEqual(TEXT("stable error"), Error, FString(TEXT("owned_block_metadata_required")));
```

Add no-`block_id` fallback rejection for node and link resolution.

- [ ] **Step 2: Extract owned resolver**

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperOwnedBlockAnchorResolver
{
public:
	bool ResolveNode(UEdGraph* Graph, const FBlueprintHelperOwnedBlockAnchor& Anchor, UEdGraphNode*& OutNode, FString& OutError) const;
	bool ResolveLink(UEdGraph* Graph, const FBlueprintHelperOwnedBlockLinkAnchor& Anchor, UEdGraphPin*& OutFrom, UEdGraphPin*& OutTo, FString& OutError) const;
};
```

Delete `NodeCommentMentionsBlockId`. Do not preserve compatibility aliases.

## Task 4: Fix Owned Mutation Ownership Boundaries

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperAppendBlueprintGraphService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp`

- [ ] **Step 1: Add append partition regression**

Create two appended entries and assert each generated node keeps exactly its own `BlueprintHelperBlockId`.

- [ ] **Step 2: Partition append nodes before metadata write**

```cpp
for (const FBlueprintHelperAppendEntryResult& EntryResult : EntryResults)
{
	for (UEdGraphNode* Node : EntryResult.CreatedNodes)
	{
		OwnershipService.MarkOwned(Node, EntryResult.BlockId);
	}
}
```

Do not apply every block id to one shared created-node array.

- [ ] **Step 3: Reject implicit external adoption in replace**

For owned replace scopes, require metadata-backed owned target. If entry is user-authored, return:

```cpp
owned_replace_target_not_blueprinthelper_owned
```

Do not mark that entry owned as a fallback.

## Task 5: Verification

- [ ] **Step 1: Run focused UE suites separately**

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.BlockScopedAnchor;Quit' -TestExit='Automation Test Queue Empty'
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ToolResultBase;Quit' -TestExit='Automation Test Queue Empty'
```

- [ ] **Step 2: Run common gate**

Use the master plan common verification gate.

## Manual Commit Checkpoint

Workers stop after reporting evidence. Suggested commit message:

```text
修复内容：
1. 封口 GraphWrite owned-only domain policy 并移除 legacy anchor fallback
2. 修复 append 多 entry ownership 覆盖和 replace 隐式接管 user-authored entry
```

