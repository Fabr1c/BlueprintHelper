# GraphWrite Merge Callable 收敛修复计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Do not run `git add`, `git commit`, or `git push`; report changed files and suggested manual commit scope only.

**Goal:** 关闭架构偏离项 2：`merge_blueprint_graph` 的 inserted callable 创建不再维护本地 resolver/spawner/lifecycle 分支，而是收敛到 GraphStatement callable fragment builder 或其薄服务包装。Merge service 只负责 merge anchor、semantic body insertion intent 和结果 envelope。

**Architecture:** callable 节点创建属于 GraphStatement/ActionResolution 主线能力。Merge service 不应拥有独立的 `ResolveMergeCallableFunction`、`CreateMergeCallableNode`、direct spawner adapter 调用或 callable node lifecycle。新增一个 private merge callable fragment service 作为适配层，把 Merge 的 inserted logic 请求转换成 `FBlueprintHelperGraphFragmentBuildRequest`，再调用 `FBlueprintHelperGraphStatementBuilder::BuildCallFunctionFragment`。

**Tech Stack:** Unreal Engine 5.6 C++ module, AutomationSpec tests, GraphWrite Merge service, GraphStatement builder, ActionResolution resolver.

---

## Scope

本计划只处理：

- 审计项 2：Merge callable 创建收敛。

本计划不处理：

- 审计项 1/5：Merge/Patch mutation ownership 与 Patch ConnectPins。
- 审计项 3：EventDelegate/custom/native taxonomy。
- Merge 插入连接 ownership 的新增行为，除非为保持现有 tests 通过必须触及。

---

## Files

新增：

- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperMergeCallableFragmentService.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperMergeCallableFragmentService.cpp`

修改：

- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteLegacyMainlineContractTests.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp`

可能修改：

- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuildRequest.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`

Actual additional files touched during execution:

- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentSpawnCoordinator.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentSpawnCoordinator.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/FunctionResolution/Utils/BlueprintHelperCallFunctionResolverUtils.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteBlockScopedAnchorTests.cpp`

Reason:

- Dry-run validation needed a shared GraphStatement resolve-only seam so preview and execute both validate through projected ActionContext/ActionResolution while preview still does not spawn nodes.
- Compiled Blueprint custom events needed shared FunctionResolution spawner evidence for graph-compatible manually enumerated `UFunction` candidates.

仅当 builder 现有 public/private 接口不足以返回 primary node 或 fragment metadata 时修改这些 GraphStatement 文件。

---

## Implementation Steps

### 1. RED: 增加 Merge callable source contract

- [ ] 在 `BlueprintHelperGraphWriteLegacyMainlineContractTests.cpp` 增加 contract，禁止 Merge service 保留本地 callable resolver/spawner token：

```cpp
const TArray<FString> ForbiddenMergeCallableTokens = {
    TEXT("ResolveMergeCallableFunction"),
    TEXT("CreateMergeCallableNode"),
    TEXT("UBlueprintFunctionNodeSpawner::Create"),
    TEXT("FBlueprintHelperActionNodeSpawnerAdapter::InvokeNodeSpawner"),
    TEXT("#include \"BlueprintGraphWriteFacade.h\""),
    TEXT("#include \"BlueprintHelperCallFunctionResolver.h\""),
    TEXT("#include \"BlueprintHelperActionNodeSpawnerAdapter.h\"")
};
```

- [ ] Contract 只扫描 `BlueprintHelperMergeBlueprintGraphService.cpp`，不扫描新的 private service；新的 service 允许依赖 GraphStatement builder，但不允许直接调用 spawner adapter。
- [ ] 跑 contract，先确认当前代码失败，形成 RED。

### 2. RED: 锁住 Merge callable 行为

- [ ] 在 `BlueprintHelperGraphWriteToolResultBaseTests.cpp` 增加或扩展 Merge focused tests，覆盖：
  - `inserted_logic.kind = "function_call"` 能创建 callable fragment 并插入到 anchor 后。
  - `inserted_logic.kind = "custom_event_call"` 走同一 callable fragment service，而不是 Merge 本地 node spawner。
  - `inserted_logic.kind = "owned_block_call"` 在 `allow_compile_before_call` 满足时仍能创建调用节点。
  - 失败 query 返回稳定错误，不能伪造 success。
  - dry-run 不创建节点，但能通过同一 resolution/build request 发现 ambiguous 或 missing callable。

- [ ] 测试应通过工具或 task runtime 入口调用 `merge_blueprint_graph`，不要直接调用新 service。

### 3. 新增 private callable fragment service

- [ ] 新建 `BlueprintHelperMergeCallableFragmentService.h/.cpp`，只放在 GraphWrite private implementation 边界。

建议接口：

```cpp
struct FBlueprintHelperMergeCallableFragmentRequest
{
    UBlueprint* Blueprint = nullptr;
    UEdGraph* Graph = nullptr;
    FString Query;
    FString FragmentId;
    FString SourceStatementId;
    FString SearchMode;
    FString AmbiguityPolicy;
    TArray<FString> CategoryPriority;
    FVector2D Location = FVector2D::ZeroVector;
};

struct FBlueprintHelperMergeCallableFragmentResult
{
    bool bOk = false;
    FString Code;
    FString Message;
    FBlueprintHelperNodeFragment Fragment;
    UK2Node* PrimaryNode = nullptr;
};

class FBlueprintHelperMergeCallableFragmentService
{
public:
    static FBlueprintHelperMergeCallableFragmentResult BuildCallableFragment(
        const FBlueprintHelperMergeCallableFragmentRequest& Request);
};
```

- [ ] `BuildCallableFragment` 内部构造 `FBlueprintHelperGraphFragmentBuildRequest`：

```cpp
FBlueprintHelperGraphFragmentBuildRequest BuildRequest;
BuildRequest.Blueprint = Request.Blueprint;
BuildRequest.Graph = Request.Graph;
BuildRequest.Query = Request.Query;
BuildRequest.Location = Request.Location;
BuildRequest.SearchMode = Request.SearchMode;
BuildRequest.AmbiguityPolicy = Request.AmbiguityPolicy;
BuildRequest.CategoryPriority = Request.CategoryPriority;
```

- [ ] 调用 `FBlueprintHelperGraphStatementBuilder::BuildCallFunctionFragment(BuildRequest, OutError)`。
- [ ] 从返回 fragment 中提取 primary node；若 builder 当前没有足够 metadata，优先在 GraphStatement 层补明确 result 字段，而不是在 Merge service 重新搜索 graph。
- [ ] service 不直接 include 或调用 `BlueprintHelperActionNodeSpawnerAdapter`。

### 4. 替换 Merge service 的本地 callable 创建

- [ ] 删除匿名 namespace 中的本地 helper：
  - `ResolveMergeCallableFunction`
  - `CreateMergeCallableNode`
  - direct `UFunction*` overload
  - 仅服务本地 spawner 的 include

- [ ] `ResolveInsertedLogic` 的 `FunctionCall` 分支改为：

```cpp
FBlueprintHelperMergeCallableFragmentRequest Request;
Request.Blueprint = Context.Blueprint;
Request.Graph = Context.Graph;
Request.Query = InsertedLogic.FunctionReference;
Request.Location = ComputeInsertionLocation(Context);
Request.SearchMode = InsertedLogic.SearchMode;
Request.AmbiguityPolicy = InsertedLogic.AmbiguityPolicy;
Request.CategoryPriority = InsertedLogic.CategoryPriority;

const FBlueprintHelperMergeCallableFragmentResult Result =
    FBlueprintHelperMergeCallableFragmentService::BuildCallableFragment(Request);
```

- [ ] `CustomEventCall` 分支也转换成 callable query 请求，不再调用本地 spawner。
- [ ] `OwnedBlockCall` 只保留 owned block existence/compile gate 逻辑；创建调用节点仍走 callable fragment service。
- [ ] `Context.InsertedNode`、`Context.InsertedRef`、owned metadata 继续由 Merge service 组装，但 callable 节点 lifecycle 不由 Merge service 本地创建。

### 5. 保留 Merge insertion mutation 边界

- [ ] 确认 Merge service 仍只通过 `FBlueprintHelperGraphWriteMutationIntent` 表达插入位置：
  - `AppendSemanticBody`
  - `InsertSemanticBodyBetweenPins`
  - `BranchForkSemanticBody`
- [ ] 不在本计划中重写 Patch ConnectPins；若测试暴露连接问题，把失败记录到 1+5 计划，不在本计划夹带修复。

### 6. GREEN: 跑 focused tests

- [ ] 运行 Merge callable contract：

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.LegacyMainline.NoLocalMergeCallableSpawner;Quit" -unattended -nop4 -nosplash
```

- [ ] 运行 Merge callable 行为测试：

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ToolResultBase.MergeCallableConvergence;Quit" -unattended -nop4 -nosplash
```

### 7. REFACTOR: 源码残留检查

- [ ] 确认 Merge service 不再包含本地 resolver/spawner token：

```powershell
rg -n "ResolveMergeCallableFunction|CreateMergeCallableNode|UBlueprintFunctionNodeSpawner::Create|FBlueprintHelperActionNodeSpawnerAdapter::InvokeNodeSpawner|BlueprintGraphWriteFacade|BlueprintHelperCallFunctionResolver|BlueprintHelperActionNodeSpawnerAdapter" BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\BlueprintHelperMergeBlueprintGraphService.cpp
```

预期无命中。

- [ ] 确认新 service 只依赖 GraphStatement builder 主线：

```powershell
rg -n "BuildCallFunctionFragment|FBlueprintHelperGraphFragmentBuildRequest|ActionNodeSpawnerAdapter|UBlueprintFunctionNodeSpawner" BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\BlueprintHelperMergeCallableFragmentService.*
```

预期命中 `BuildCallFunctionFragment` / `FBlueprintHelperGraphFragmentBuildRequest`，不命中 direct spawner token。

### 8. Build 与最终校验

- [ ] 编译：

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

- [ ] 格式检查：

```powershell
git diff --check
```

- [ ] 输出 changed files、测试命令结果、未提交说明、建议 commit message。不要执行 git stage/commit/push。

---

## Acceptance Criteria

- `BlueprintHelperMergeBlueprintGraphService.cpp` 不再包含本地 callable resolver/spawner/lifecycle helper。
- `merge_blueprint_graph` 的 `function_call`、`custom_event_call`、`owned_block_call` 都通过 shared GraphStatement callable fragment builder 创建节点。
- Merge service 仍保留 anchor、inserted logic context、mutation intent 和 result envelope 责任。
- focused tests、build、`git diff --check` 全部通过。

---

## Execution Result

- Merge service callable creation was converged into `FBlueprintHelperMergeCallableFragmentService`.
- `function_call`, `custom_event_call`, and `owned_block_call` now build callable nodes through `FBlueprintHelperGraphStatementBuilder::BuildCallFunctionFragment`.
- Merge dry-run now validates callable feasibility through `FBlueprintHelperGraphStatementBuilder::ValidateCallFunctionFragment`, which projects ActionContext and resolves ActionResolution without spawning nodes.
- Merge service keeps anchor, inserted context, ownership metadata, mutation intent, and result-envelope responsibility, but no longer owns local callable resolver/spawner/adapter branches.
- FunctionResolution now creates shared `UBlueprintFunctionNodeSpawner` evidence for graph-compatible manually enumerated `UFunction` candidates, which covers compiled Blueprint custom events and owned block calls without reintroducing a Merge-local spawner path.
- Source contract `BlueprintHelper.GraphWrite.LegacyMainline.NoLocalMergeCallableSpawner` was added to keep forbidden callable tokens out of `BlueprintHelperMergeBlueprintGraphService.cpp` and to keep direct spawner/adapter tokens out of `BlueprintHelperMergeCallableFragmentService.cpp`.
- Block-scoped merge tests now cover custom event call, custom event dry-run no-preview-node residue, owned block call, uncompiled owned block call with compile gate, and blocked dry-run no-preview-node residue.

## Verification Result

- `Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload`: passed.
- `Automation RunTests BlueprintHelper.GraphWrite.BlockScopedAnchors.Merge`: passed, 10 tests.
- `Automation RunTests BlueprintHelper.GraphWrite.LegacyMainline.NoLocalMergeCallableSpawner`: passed, 1 test.
- Static forbidden-token check on `BlueprintHelperMergeBlueprintGraphService.cpp`: passed, no matches.
- Static service-mainline check on `BlueprintHelperMergeCallableFragmentService.*`: only `BuildCallFunctionFragment` / `ValidateCallFunctionFragment` / `FBlueprintHelperGraphFragmentBuildRequest` matched; no direct spawner/adapter/resolver token matched.
- `git diff --check`: passed; only LF/CRLF warnings were emitted.

## Suggested Manual Commit Message

变更需求：
1. 将 GraphWrite Merge callable 创建收敛到 GraphStatement callable fragment builder。

修复内容：
1. 移除 Merge service 本地 callable resolver/spawner 分支，避免与 FunctionActionCluster 主线漂移。

新增内容：
1. 增加 Merge callable 收敛 source contract 和端到端行为测试。
