# BlueprintHelper GraphWrite Semantic Mainline Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 移除 GraphWrite 主路径中的旧 `FParsedNode` 桥接、硬编码 Kind 分发、placeholder DAG、过宽候选扫描和不完整簇声明，使 GraphWrite 回到 `SemanticIR -> ActionContext -> ActionResolution -> FragmentBuilder registry -> FragmentDAG -> Composer/Mutator` 的统一架构。

**Architecture:** Pipeline 只负责上下文构建、identity 分配、registry lookup 和结果编排；语义到 fragment 的创建由可注册 builder/provider 消费 statement/action IR request。Action 候选必须先消费 projected context 收缩候选域，宽扫描只能作为显式诊断模式，不能作为默认成功路径。

**Tech Stack:** UE 5.6, BlueprintHelper C++, GraphWrite SemanticIR, ActionContextPipeline, ActionResolutionCore, FragmentDAG, BlueprintHelper CLI preview/execute, Unreal Automation/Runtime smoke.

---

## 硬性约束

- 不保留旧 Agent / 旧字段 / 旧工具 fallback。
- 不新增 `FParsedNode` 主路径消费者。
- 不新增基于 `StatementKind == ...` 的 pipeline 级硬编码分发。
- 不让 placeholder 成为可解析 expression 的默认路径。
- 不把 `TObjectIterator` 全局扫描作为普通成功路径。
- 不把簇声明为支持某能力，实际却只返回缺上下文。
- 所有硬编码策略项需要进入 Settings 或明确标为不可配置的架构常量。
- 任务完成必须同步本文档 checkbox；未验证项不得标记完成。

---

## Files and Responsibilities

| 文件 | 职责 |
| --- | --- |
| `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h` | 保留 statement/expression 语义结构；新增 request DTO 时只放纯数据结构。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSemanticFragmentRequest.h` | 新增 statement/action fragment request 数据模型，替代主路径 `FParsedNode` 桥接。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp` | 移除 semantic statement 到 `FParsedNode` 的映射；只做 context、identity、registry lookup。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSemanticFragmentBuilderRegistry.h` | 新增 SemanticKind 到 builder provider 的 registry 接口。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSemanticFragmentBuilderRegistry.cpp` | 注册 FieldVariable / Function / Generic / EventDelegate 簇 builder。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp` | 从主路径 builder 中移除硬编码 if/case；保留低层 node mutation helper 时必须只由 registry builder 调用。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagBuilderUtils.cpp` | 可解析 expression 直接进入真实 fragment/evidence；placeholder 只用于明确不可解析诊断。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperEventDelegateActionCluster.cpp` | 让 bind / component_bound_event 与实际 evidence 能力一致。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp` | 收集 component/delegate/signature/type hints 等簇所需上下文。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperGenericAssetStructControlActionResolver.cpp` | 收缩 struct/control/select 候选域；显式诊断模式才允许宽搜索。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Function/BlueprintHelperCallFunctionResolverUtils.cpp` | FunctionAction 候选域先由 projected context 构建，再进入 UE action/filter。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Settings/*` | 如已有 Settings runtime consumption，扩展 diagnostic wide scan 开关；没有则接入现有 setting service。 |
| `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_SemanticMainlineCleanup_ImplementationPlan_20260522_CN.md` | 本计划和执行进度记录。 |

---

## Task 1: 新增 Semantic Fragment Request，移除 `FParsedNode` 主路径桥接

**Files:**

| 类型 | 路径 |
| --- | --- |
| Create | `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSemanticFragmentRequest.h` |
| Modify | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp` |
| Modify | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp` |
| Test | `BlueprintHelper/Source/BlueprintHelper/Private/Tests/BlueprintHelperGraphWriteTests.cpp` 或现有 GraphWrite automation test 文件 |

- [ ] **Step 1: 写失败测试，证明 semantic statement 不再生成 `FParsedNode`**

测试目标：给 `call` / `set_property` / `branch` statement 跑 preview，断言 debug evidence 或 internal diagnostic 不出现 `parsed_node_bridge` / `FParsedNode` 主路径标记。

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSemanticRequestNoParsedNodeBridgeTest,
	"BlueprintHelper.GraphWrite.SemanticRequest.NoParsedNodeBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSemanticRequestNoParsedNodeBridgeTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphSemanticStatement Statement;
	Statement.Kind = EBlueprintHelperGraphStatementKind::Call;
	Statement.Id = TEXT("Call_PrintString");
	Statement.CallReference.FunctionName = TEXT("PrintString");

	FBlueprintHelperSemanticFragmentRequest Request =
		FBlueprintHelperSemanticFragmentRequest::FromStatement(Statement);

	TestEqual(TEXT("Request keeps semantic statement kind"), Request.StatementKind, EBlueprintHelperGraphStatementKind::Call);
	TestFalse(TEXT("Request must not be backed by FParsedNode"), Request.DebugSource.Contains(TEXT("FParsedNode")));
	return true;
}
```

- [ ] **Step 2: 新增 request DTO**

新增纯数据结构，只承载 semantic statement/expression、action context key、node identity、layout intent、review/debug evidence hint。

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"

struct FBlueprintHelperSemanticFragmentRequest
{
	FString RequestId;
	EBlueprintHelperGraphStatementKind StatementKind = EBlueprintHelperGraphStatementKind::Unknown;
	FBlueprintHelperGraphSemanticStatement Statement;
	FString GraphName;
	FString ActionContextKey;
	FString DebugSource;

	static FBlueprintHelperSemanticFragmentRequest FromStatement(
		const FBlueprintHelperGraphSemanticStatement& InStatement,
		const FString& InGraphName = FString(),
		const FString& InActionContextKey = FString())
	{
		FBlueprintHelperSemanticFragmentRequest Request;
		Request.RequestId = InStatement.Id;
		Request.StatementKind = InStatement.Kind;
		Request.Statement = InStatement;
		Request.GraphName = InGraphName;
		Request.ActionContextKey = InActionContextKey;
		Request.DebugSource = TEXT("semantic_statement");
		return Request;
	}
};
```

- [ ] **Step 3: Pipeline 改为构造 request**

在 `SpawnSemanticStatementFragment` 附近删除 semantic statement 到 `FParsedNode` 的映射代码，改为：

```cpp
const FBlueprintHelperSemanticFragmentRequest Request =
	FBlueprintHelperSemanticFragmentRequest::FromStatement(Statement, TargetGraph ? TargetGraph->GetName() : FString(), ActionContextKey);

FBlueprintHelperGraphFragmentBuildResult FragmentResult;
if (!SemanticFragmentBuilderRegistry.Build(Request, BuildContext, FragmentResult))
{
	return FBlueprintHelperGraphWriteResult::Failure(
		TEXT("semantic_fragment_builder_not_found"),
		FString::Printf(TEXT("No semantic fragment builder registered for statement kind: %s"), *LexToString(Statement.Kind)));
}
```

- [ ] **Step 4: 删除主路径 `FParsedNode` builder 入参**

`BlueprintHelperGraphStatementBuilder.cpp` 中保留底层低级 helper 时，入口函数改为接收 `FBlueprintHelperSemanticFragmentRequest` 或具体簇 request。旧 `FParsedNode` 入口如果仍被 graph-json legacy 调用，应从 graphwrite 主路径断开，并标为不可调用或删除。

- [ ] **Step 5: 编译并运行最小测试**

Run:

```powershell
# 使用项目现有 UE 5.6 编译入口；执行者需要按当前工作区真实命令替换，不得在未编译时勾选本步骤
```

Expected:

```text
BlueprintHelper module compiles.
BlueprintHelper.GraphWrite.SemanticRequest.NoParsedNodeBridge passes.
```

---

## Task 2: 建立 SemanticKind -> FragmentBuilder Registry，移除 pipeline 级 Kind 分发

**Files:**

| 类型 | 路径 |
| --- | --- |
| Create | `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSemanticFragmentBuilderRegistry.h` |
| Create | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSemanticFragmentBuilderRegistry.cpp` |
| Modify | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp` |
| Modify | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp` |

- [ ] **Step 1: 写失败测试，检索硬编码分发**

检查目标：`BlueprintGraphGenerationPipeline.cpp` 不应出现 `Statement.Kind == EBlueprintHelperGraphStatementKind::Call` 这类业务分支；允许日志和诊断中的 `LexToString`。

```powershell
rg "Statement\\.Kind\\s*==|switch\\s*\\([^\\)]*Statement\\.Kind|case\\s+EBlueprintHelperGraphStatementKind" BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline
```

Expected before fix:

```text
Finds hard-coded semantic kind dispatch.
```

Expected after fix:

```text
No matches in Pipeline mainline.
```

- [ ] **Step 2: 新增 registry 接口**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSemanticFragmentRequest.h"

struct FBlueprintHelperSemanticFragmentBuildContext;
struct FBlueprintHelperGraphFragmentBuildResult;

class IBlueprintHelperSemanticFragmentBuilder
{
public:
	virtual ~IBlueprintHelperSemanticFragmentBuilder() = default;
	virtual bool CanBuild(const FBlueprintHelperSemanticFragmentRequest& Request) const = 0;
	virtual bool Build(
		const FBlueprintHelperSemanticFragmentRequest& Request,
		const FBlueprintHelperSemanticFragmentBuildContext& Context,
		FBlueprintHelperGraphFragmentBuildResult& OutResult) const = 0;
};

class FBlueprintHelperSemanticFragmentBuilderRegistry
{
public:
	void Register(TSharedRef<IBlueprintHelperSemanticFragmentBuilder> Builder);
	bool Build(
		const FBlueprintHelperSemanticFragmentRequest& Request,
		const FBlueprintHelperSemanticFragmentBuildContext& Context,
		FBlueprintHelperGraphFragmentBuildResult& OutResult) const;

private:
	TArray<TSharedRef<IBlueprintHelperSemanticFragmentBuilder>> Builders;
};
```

- [ ] **Step 3: 注册四大簇 builder**

注册顺序必须按语义专属度排序：

| 顺序 | Builder | 覆盖 |
| --- | --- | --- |
| 1 | FieldVariableActionFragmentBuilder | get / set / get_property / set_property |
| 2 | FunctionActionFragmentBuilder | call / op / convert_function / schedule_function / latent_or_async_function |
| 3 | GenericAssetStructControlFragmentBuilder | construct / deconstruct / select / control |
| 4 | EventDelegateActionFragmentBuilder | custom_event / bind / component_bound_event / delegate |

- [ ] **Step 4: Pipeline 只做 registry lookup**

Pipeline 的 statement spawn 主路径只允许：

```cpp
FBlueprintHelperSemanticFragmentRequest Request = FBlueprintHelperSemanticFragmentRequest::FromStatement(Statement, GraphName, ActionContextKey);
FBlueprintHelperGraphFragmentBuildResult BuildResult;
const bool bBuilt = SemanticFragmentBuilderRegistry.Build(Request, SemanticBuildContext, BuildResult);
```

- [ ] **Step 5: 移除 builder 内业务大分支**

`BlueprintHelperGraphStatementBuilder.cpp` 若仍保留 `Call / Set / SetProperty / Branch / Return / Construct / Deconstruct / Select` 大分支，应迁移到各簇 builder；该文件只保留共享 adapter 或低层 node helper。

---

## Task 3: DAG 层让可解析 expression 直接生成真实 fragment

**Files:**

| 类型 | 路径 |
| --- | --- |
| Modify | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagBuilderUtils.cpp` |
| Modify | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSemanticFragmentBuilderRegistry.cpp` |

- [ ] **Step 1: 写失败测试，验证 Call / Op / Construct / Deconstruct 不再 placeholder**

测试输入：`op(">")`、`construct(Vector)`、`deconstruct(Vector)`、`call(PrintString)` expression。

Expected after fix:

```text
FragmentDAG node kind is function_action/generic_action, not placeholder_expression.
Debug evidence contains resolver/provider id.
```

- [ ] **Step 2: 修改 expression dispatch 规则**

规则：

| Expression | 处理 |
| --- | --- |
| literal | literal fragment |
| ref | symbol/data edge fragment |
| call | FunctionAction builder |
| op | FunctionAction builder with operator constraints |
| construct | Generic builder |
| deconstruct | Generic builder |
| select | Generic builder |
| unknown / malformed | placeholder diagnostic fragment |

- [ ] **Step 3: placeholder 降级为诊断**

`BuildPlaceholderExpression` 只允许在 `InvalidRequest` 或 `NeedsMoreSemanticContext` 时生成，且必须包含：

```json
{
  "status": "needs_more_semantic_context",
  "expression_kind": "op",
  "reason": "missing_left_value_type"
}
```

---

## Task 4: EventDelegateActionCluster 补齐真实成功路径或收窄能力声明

**Files:**

| 类型 | 路径 |
| --- | --- |
| Modify | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperEventDelegateActionCluster.cpp` |
| Modify | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp` |
| Modify | `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionTypes.h` |

- [ ] **Step 1: 写失败测试，证明声明能力必须有成功路径**

输入：

```json
{
  "kind": "bind",
  "target": "RP_Trigger",
  "delegate": "OnComponentBeginOverlap",
  "handler": "OnTriggerBeginOverlap"
}
```

Expected after fix:

```text
Preview returns one UBlueprintBoundEventNodeSpawner or UBlueprintDelegateNodeSpawner evidence.
Execute creates a bound event/delegate node.
```

- [ ] **Step 2: ActionContextPipeline 增加 evidence**

需要投影：

| Evidence | 来源 |
| --- | --- |
| component name/type | Blueprint component tree |
| delegate property/function | component class reflection |
| existing handler signature | Blueprint skeleton/generated class |
| graph context | TaskSpec graph target + resolved UEdGraph |
| pin compatibility | delegate signature params |

- [ ] **Step 3: 接入 UE spawner**

优先顺序：

| 输入语义 | UE spawner |
| --- | --- |
| custom event | existing custom event spawner path |
| component_bound_event | `UBlueprintBoundEventNodeSpawner` |
| bind delegate | `UBlueprintDelegateNodeSpawner` when available |
| unsupported delegate | `NeedsMoreSemanticContext` with candidate delegates |

- [ ] **Step 4: 如果上下文不足，返回候选而不是假声明成功**

返回格式：

```json
{
  "status": "needs_more_semantic_context",
  "candidate_delegates": [
    {
      "target": "RP_Trigger",
      "delegate": "OnComponentBeginOverlap",
      "signature": "Actor, PrimitiveComponent, int32, bool, HitResult"
    }
  ]
}
```

---

## Task 5: Generic resolver 候选域由 projected context 收缩

**Files:**

| 类型 | 路径 |
| --- | --- |
| Modify | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperGenericAssetStructControlActionResolver.cpp` |
| Modify | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp` |
| Modify | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Settings/*` |

- [ ] **Step 1: 写失败测试，禁止默认 `TObjectIterator` 宽扫描**

Run:

```powershell
rg "TObjectIterator<\\s*(UScriptStruct|UFunction)" BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Clusters/BlueprintHelperGenericAssetStructControlActionResolver.cpp
```

Expected after fix:

```text
Only appears inside an explicit diagnostic-wide-scan helper guarded by setting or debug mode.
```

- [ ] **Step 2: 构建 context-bounded struct candidate domain**

候选域来源：

| 来源 | 用途 |
| --- | --- |
| explicit type path | 精确 struct |
| connected pin type | construct/deconstruct/select 类型 |
| target asset class/module | 限制用户项目 / engine module |
| known Blueprint variables | struct type hints |
| previous resolver evidence | preview -> execute stable evidence |

- [ ] **Step 3: 宽搜索降级为诊断模式**

默认 preview 不进行全局 `TObjectIterator`。当 setting `graph_write.action_resolution.enable_diagnostic_wide_scan` 为 true 时，允许返回低置信候选，但不能自动 execute。

```json
{
  "status": "needs_more_semantic_context",
  "confidence": "low",
  "diagnostic_wide_scan_used": true,
  "candidate_structs": []
}
```

---

## Task 6: FunctionAction 候选构建先消费 projected context

**Files:**

| 类型 | 路径 |
| --- | --- |
| Modify | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Function/BlueprintHelperCallFunctionResolverUtils.cpp` |
| Modify | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp` |
| Modify | `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionTypes.h` |

- [ ] **Step 1: 写失败测试，验证 BuildCandidateUniverse 不再默认枚举全 class/function library**

输入：`op(">")` 且左右值连接到 float pin。

Expected after fix:

```text
Candidate domain starts from typed pin/operator projected context.
Returned candidate is typed numeric greater function/spawner evidence.
No global library scan flag in evidence.
```

- [ ] **Step 2: 候选域构建顺序**

| 顺序 | 来源 | 说明 |
| --- | --- | --- |
| 1 | target object/class | 最强约束 |
| 2 | typed pins / connected values | op/call 参数约束 |
| 3 | graph schema/context | pure/latent/world context 约束 |
| 4 | action database/filter | UE 侧 action 过滤 |
| 5 | diagnostic wide scan | 仅诊断，不自动成功 |

- [ ] **Step 3: 返回低置信诊断而不是默默选错**

当候选仍多于 1 个：

```json
{
  "status": "needs_more_semantic_context",
  "candidate_functions": [
    {
      "display_name": "Greater",
      "owner": "UKismetMathLibrary",
      "pin_signature": "float,float -> bool",
      "confidence": "medium"
    }
  ]
}
```

---

## Task 7: Full GraphWrite preview/execute 验证与文档同步

**Files:**

| 类型 | 路径 |
| --- | --- |
| Modify | `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_SemanticMainlineCleanup_ImplementationPlan_20260522_CN.md` |
| Modify | `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_LegacyMainlineCleanup_ImplementationPlan_20260522_CN.md` |
| Test | Full graphwrite TaskSpec fixture |

- [ ] **Step 1: 运行 preview**

Expected:

```text
Preview succeeds for call/get/set/get_property/set_property/op/construct/deconstruct/select/control/custom_event.
Preview returns needs_more_semantic_context only for intentionally ambiguous bind/delegate cases.
No UnsupportedClusterMigration.
No *_migration_pending.
No FParsedNode bridge evidence on mainline.
```

- [ ] **Step 2: 运行 execute**

Expected:

```text
Execute succeeds for non-ambiguous fixture.
Created Blueprint compiles.
DebugBundle shows ActionContextPipeline evidence consumed by each cluster.
```

- [ ] **Step 3: 同步文档状态**

规则：

| 情况 | 文档标记 |
| --- | --- |
| 编译通过且 fixture 通过 | `- [x]` |
| 编译通过但缺专用 fixture | `- [o]`，写明缺口 |
| 编译失败或 execute 失败 | `- [ ]`，写明 full_result/error |

---

## Completion Checklist

- [ ] `FParsedNode` 不再是 semantic statement 主路径内部模型。
- [ ] Pipeline 不再按 statement kind 硬编码分发业务 builder。
- [ ] 可解析 expression 不再默认 placeholder。
- [ ] EventDelegateActionCluster 的声明能力和实际成功路径一致。
- [ ] Generic resolver 默认候选域由 projected context 收缩。
- [ ] FunctionAction 候选域先消费 target/typed pin/graph context。
- [ ] 宽搜索只作为显式诊断模式，不自动 execute。
- [ ] 编译通过。
- [ ] Full graphwrite preview/execute 通过或记录明确缺口。

---

## 2026-05-22 Closure Sync

- [x] `FParsedNode` is no longer a semantic statement mainline model; parsed graph DTOs are private pipeline-only.
- [x] Fragment construction is routed through request / registry boundaries.
- [x] Resolvable expressions (`call`, `op`, `construct`, `deconstruct`) build real fragments before any placeholder diagnostic path.
- [x] EventDelegate declared capability matches implemented success paths; `ComponentBoundEvent` / `Bind` are not claimed as complete.
- [x] Generic and FunctionAction candidate domains are context-bounded; target resolver source contains no `TObjectIterator<UClass/UScriptStruct/UFunction>`.
- [x] GraphStatementBuilder no longer rebuilds `ActionContextScope`; missing scope fails with `action_context_scope_required`.
- [x] AgentFace public control flow is `kind:"control"` + `control`; lowered TaskPlan body uses compiler-owned `sequence` / `branch` / `return`.
- [x] UE 5.6 compile passed after the final boundary changes.
- [x] Task-core `npm.cmd run build` passed; `npm.cmd run test:node` passed 150/150; Python compiler/runtime tests passed 47/47.
- [x] Local TaskSpecRunner Bridge preview passed for public `kind:"control"` input with lowered `BlueprintLogicSpec.v2` body and no `statement_kind_unsupported`.

## Suggested Manual Commit Message After Completion

```text
变更需求：
1. 收敛 GraphWrite 主路径到 SemanticIR request、ActionContext、ActionResolution 和 FragmentBuilder registry。
2. 移除主流 Pipeline 的 FParsedNode 语义桥接和 statement kind 硬编码分发。

修复内容：
1. 修复可解析 expression 被降级为 placeholder 的 DAG 语义偏差。
2. 修复 EventDelegateActionCluster 声明能力和真实能力不一致的问题。
3. 收紧 Generic 和 FunctionAction 候选域，避免默认全局扫描误匹配。
```

```powershell
git add BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSemanticFragmentRequest.h `
  BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSemanticFragmentBuilderRegistry.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite `
  BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_SemanticMainlineCleanup_ImplementationPlan_20260522_CN.md `
  BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_LegacyMainlineCleanup_ImplementationPlan_20260522_CN.md
git commit -m "变更需求：收敛 GraphWrite 语义主路径" -m "1. 收敛 GraphWrite 主路径到 SemanticIR request、ActionContext、ActionResolution 和 FragmentBuilder registry。" -m "2. 移除主流 Pipeline 的 FParsedNode 语义桥接和 statement kind 硬编码分发。" -m "修复内容：" -m "1. 修复可解析 expression 被降级为 placeholder 的 DAG 语义偏差。" -m "2. 修复 EventDelegateActionCluster 声明能力和真实能力不一致的问题。" -m "3. 收紧 Generic 和 FunctionAction 候选域，避免默认全局扫描误匹配。"
```
