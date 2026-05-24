# GraphWrite Further Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 关闭 2026-05-24 质量审计后仍未证明闭合的 GraphWrite gap：call fragment 的 `semantic_kind` ownership tag 合约，以及 `context_evidence` 从 TS compiler 到 UE build request 的真实链路测试。

**Architecture:** 已完成的 `asset_action`、`type_promotion`、Function shared coordinator 实现不重新设计。本计划只修正 shared coordinator 调用侧的 fragment contract 回退，并补齐 `context_evidence` 在 compiler output、Bridge payload、SemanticIR、FragmentBuildRequest、ActionRequest append 点的验证。四簇 smoke 只在 Editor/Bridge 生命周期正确启动后复跑，Bridge 未启动一类 harness 问题不得写入 bug 文档。

**Tech Stack:** UE 5.6 C++ editor automation, BlueprintHelper GraphWrite ActionResolution/GraphStatement, AgentFace task-core TypeScript compiler tests, BlueprintHelper CLI four-cluster smoke.

---

## Scope

已由当前自动化证明的内容不再重复实现：

- Generic `asset_action` 已接入真实 `FBlueprintActionDatabase` spawner evidence。
- Generic `type_promotion` 已通过 `FTypePromotion::GetOperatorSpawner` 和 typed pin evidence 解析。
- Function `call` / `action-provider` fragment 已通过 `FBlueprintHelperActionFragmentSpawnCoordinator` 收敛 resolve/spawn/pin/metadata 生命周期。

本计划只处理两个真实剩余项：

- `BuildCallFunctionFragment` 通过 shared coordinator 后将 `bAppendSemanticKindOwnershipTag` 置为 `false`，导致 call fragment 不再携带 `OwnershipTags["semantic_kind"] == "call"`。
- `context_evidence` 已在 TS/C++ 中接入，但测试还需要证明它实际经过 compiler lowering 和 UE SemanticIR/BuildRequest 链路，而不只是源代码字符串扫描。

## Execution Rules

- 不执行 `git add`、`git commit`、`git push`。
- 不修改 `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_FourClusterE2ESmoke_Bugs_20260524_CN.md`，除非 smoke 发现新的插件代码缺陷。Editor/Bridge 未启动、`ECONNREFUSED 127.0.0.1:54321` 只记录为 harness detail。
- 不引入 legacy fallback、旧 Agent 字段兼容、`FindFunctionByName` fallback 或 synthetic spawner success。
- 修复应保持 shared coordinator 边界，不把 lifecycle 逻辑挪回 `BlueprintHelperGraphStatementBuilder.cpp`。

## Subagent Distribution

- Task 1: 小型代码修改，使用 `5.4 high` worker。
- Task 2: 中型测试补强和少量 contract 断言，使用 `5.4 high` worker。
- Task 3: smoke 复验和文档同步，使用 `5.4 high` worker。
- 全部任务完成后：派发一个大型只读审计，使用 `5.4 high` reviewer，检查 semantic contract、context evidence 链路、no fake success、bug 文档污染和 smoke 证据。

## File Structure

### Modify

- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
  - 将 call fragment coordinator request 的 `bAppendSemanticKindOwnershipTag` 恢复为 `true`。
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteFunctionFieldUnifiedSmokeTests.cpp`
  - 增加 runtime smoke，直接构建 call fragment 并断言 `OwnershipTags["semantic_kind"] == "call"`。
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`
  - 强化 source contract：call/action-provider 仍委托 coordinator，同时必须请求 semantic ownership tag；call/create/action-provider 必须 append request `ContextEvidence`。
- `AgentFaceService/task-core/src/task/compiler/task-compiler.convert-schedule.test.ts`
  - 增加 compiler + append bridge payload 的 `context_evidence` 保真测试，覆盖 statement 和 nested expression。
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphSemanticIRRuntimeFactTests.cpp`
  - 增加 UE 侧 runtime fact，验证 JSON `context_evidence` 进入 SemanticIR 和 `FBlueprintHelperGraphFragmentBuildRequest`。
- `BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/SmokeRecord_20260524_CN.md`
  - smoke 复跑后更新结果、命令、artifact 和是否仍为 harness detail。
- `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
  - 修复和 smoke 通过后同步能力状态。
- `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`
  - 修复和 smoke 通过后关闭或缩小对应 gap。

---

## Task 1: Restore Call Fragment `semantic_kind` Ownership Tag

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteFunctionFieldUnifiedSmokeTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`

- [ ] **Step 1: Add runtime test includes**

In `BlueprintHelperGraphWriteFunctionFieldUnifiedSmokeTests.cpp`, extend the include block:

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextScope.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"
```

- [ ] **Step 2: Add a reusable ActionContextScope helper**

Add this helper inside the anonymous namespace in `BlueprintHelperGraphWriteFunctionFieldUnifiedSmokeTests.cpp`:

```cpp
static bool BuildUnifiedSmokeActionContextScopeForStatement(
	FAutomationTestBase& Test,
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperActionContextScope& OutScope,
	FString& OutError)
{
	TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>> Statements;
	Statements.Add(MakeShared<FBlueprintHelperGraphStatementIR>(Statement));

	const TArray<FBlueprintHelperActionContextDemand> Demands =
		FBlueprintHelperActionContextDemandCollector::CollectFromStatements(Statements);
	Test.TestTrue(TEXT("action context demands exist"), Demands.Num() > 0);
	if (Demands.Num() == 0)
	{
		OutError = TEXT("no_action_context_demands");
		return false;
	}

	return FBlueprintHelperActionContextScope::Build(
		Blueprint,
		Graph,
		Demands,
		FBlueprintHelperActionContextScope::MakeRevision(
			Blueprint,
			Graph,
			TEXT("function_field_unified_smoke"),
			TEXT("semantic_kind_contract")),
		OutScope,
		OutError);
}
```

- [ ] **Step 3: Add the failing runtime smoke**

Append this test before `#endif` in `BlueprintHelperGraphWriteFunctionFieldUnifiedSmokeTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteUnifiedSmokeCallFragmentSemanticKindOwnershipTest,
	"BlueprintHelper.GraphWrite.FunctionFieldUnifiedSmoke.CallFragmentSemanticKindOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteUnifiedSmokeCallFragmentSemanticKindOwnershipTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeUnifiedSmokeBlueprint();
	UEdGraph* Graph = GetUnifiedSmokeGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	FBlueprintHelperGraphStatementIR Statement;
	Statement.StatementId = TEXT("stmt_call_semantic_kind_contract");
	Statement.Path = TEXT("$.statements[0]");
	Statement.Kind = EBlueprintHelperGraphStatementKind::Call;
	Statement.PatternName = TEXT("call");
	Statement.Target = TEXT("PrintString");
	Statement.ResolvedCallFunctionStableId = TEXT("/Script/Engine.KismetSystemLibrary:PrintString");
	Statement.SearchMode = TEXT("contextual");
	Statement.AmbiguityPolicy = TEXT("fail_on_ambiguity");

	FBlueprintHelperActionContextScope ActionContextScope;
	FString ScopeError;
	if (!BuildUnifiedSmokeActionContextScopeForStatement(
		*this,
		Blueprint,
		Graph,
		Statement,
		ActionContextScope,
		ScopeError))
	{
		AddError(FString::Printf(TEXT("action context scope build failed: %s"), *ScopeError));
		return false;
	}

	FBlueprintHelperGraphFragmentBuildRequest Request =
		FBlueprintHelperGraphFragmentBuildRequest::FromStatement(Statement);
	Request.DefaultValues.Add(TEXT("InString"), TEXT("semantic kind contract"));

	FBlueprintHelperNodeFragment Fragment;
	FString BuildError;
	const bool bBuilt = FBlueprintHelperGraphStatementBuilder::BuildCallFunctionFragment(
		Graph,
		Request,
		Fragment,
		BuildError,
		nullptr,
		&ActionContextScope);

	bool bPassed = true;
	bPassed &= TestTrue(TEXT("call fragment builds"), bBuilt);
	if (!bBuilt)
	{
		AddError(FString::Printf(TEXT("call fragment build failed: %s"), *BuildError));
		return false;
	}
	bPassed &= TestNotNull(TEXT("call primary node"), Fragment.PrimaryNode);
	bPassed &= TestEqual(
		TEXT("call ownership semantic kind"),
		Fragment.OwnershipTags.FindRef(TEXT("semantic_kind")),
		FString(TEXT("call")));
	return bPassed;
}
```

- [ ] **Step 4: Run the red test**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.FunctionFieldUnifiedSmoke.CallFragmentSemanticKindOwnership;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_CallSemanticKind_RED_001'
```

Expected before implementation: the test builds the call fragment but fails the ownership assertion because `semantic_kind` is empty.

- [ ] **Step 5: Restore the coordinator request flag**

In `BlueprintHelperGraphStatementBuilder.cpp`, change only the call fragment coordinator request:

```cpp
	CoordinatorRequest.bAppendSemanticKindOwnershipTag = true;
```

Do not move resolve/spawn/pin/metadata logic back into `BuildCallFunctionFragment`.

- [ ] **Step 6: Strengthen the source contract**

In `FBlueprintHelperActionResolutionFunctionFragmentLifecycleCoordinatorContractTest`, after the existing per-slice checks, add:

```cpp
	bClean &= TestTrue(
		TEXT("BuildCallFunctionFragment preserves semantic ownership tag through coordinator"),
		CallFragmentSlice.Contains(TEXT("CoordinatorRequest.bAppendSemanticKindOwnershipTag = true;")));
	bClean &= TestTrue(
		TEXT("BuildActionProviderFragment preserves semantic ownership tag through coordinator"),
		ActionProviderSlice.Contains(TEXT("CoordinatorRequest.bAppendSemanticKindOwnershipTag = true;")));
```

- [ ] **Step 7: Run focused green automation**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.FunctionFieldUnifiedSmoke.CallFragmentSemanticKindOwnership+BlueprintHelper.GraphWrite.ActionResolution.Contract.FunctionFragmentLifecycleCoordinator;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_CallSemanticKind_GREEN_001'
```

Expected: both tests pass.

---

## Task 2: Prove `context_evidence` Through Compiler and UE Request Path

**Files:**
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.convert-schedule.test.ts`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphSemanticIRRuntimeFactTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`

- [ ] **Step 1: Add compiler tests for statement and expression evidence**

In `task-compiler.convert-schedule.test.ts`, append:

```ts
test('convert and schedule preserve context_evidence through compiler outputs', () => {
  const convertInput = {
    kind: 'convert',
    function_operation: 'convert_function',
    transform_operation: 'type_promotion',
    context_evidence: {
      type_promotion_stable_id: 'type_promotion:Add:int:real',
      type_promotion_operator: 'Add',
      type_promotion_source_pin_type: 'int',
      type_promotion_target_pin_type: 'real',
      type_promotion_result_pin_type: 'real',
    },
    args: {
      value: { kind: 'literal', value_type: 'number', value: 7 },
    },
  };

  for (const statement of [compileTaskPlanStatement(convertInput), compileBridgeStatement(convertInput)]) {
    assert.equal(statement.kind, 'convert');
    assert.equal(statement.transform_operation, 'type_promotion');
    assert.deepEqual(statement.context_evidence, convertInput.context_evidence);
  }

  const nestedScheduleInput = {
    kind: 'call',
    target: 'PrintString',
    args: {
      value: {
        kind: 'schedule',
        function_operation: 'schedule_function',
        schedule_operation: 'latent_or_async_node',
        context_evidence: {
          graph_latent_allowed: 'false',
          schedule_operation: 'latent_or_async_node',
        },
        args: {
          delay: { kind: 'literal', value_type: 'number', value: 0.25 },
        },
      },
    },
  };

  for (const statement of [compileTaskPlanStatement(nestedScheduleInput), compileBridgeStatement(nestedScheduleInput)]) {
    const args = statement.args as Record<string, Record<string, unknown>>;
    const value = args.value;
    assert.equal(value.kind, 'schedule');
    assert.equal(value.schedule_operation, 'latent_or_async_node');
    assert.deepEqual(value.context_evidence, {
      graph_latent_allowed: 'false',
      schedule_operation: 'latent_or_async_node',
    });
  }
});
```

- [ ] **Step 2: Run the TypeScript red/green test target**

Run:

```powershell
npm.cmd --prefix D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core run test
```

Expected after current implementation: pass. If it fails, fix only the missing `copyContextEvidence(...)` call for the exact failing lowering path.

- [ ] **Step 3: Add UE runtime fact include**

In `BlueprintHelperGraphSemanticIRRuntimeFactTests.cpp`, add:

```cpp
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuildRequest.h"
```

- [ ] **Step 4: Add a JSON fixture helper for context evidence**

Inside `FBlueprintHelperGraphSemanticIRRuntimeFactTestsLocalUtils`, add:

```cpp
	static TSharedRef<FJsonObject> MakeLogicSpecWithContextEvidence()
	{
		TSharedRef<FJsonObject> LogicSpec = MakeShared<FJsonObject>();
		LogicSpec->SetStringField(TEXT("schema"), TEXT("BlueprintLogicSpec.v2"));

		TSharedRef<FJsonObject> StatementEvidence = MakeShared<FJsonObject>();
		StatementEvidence->SetStringField(TEXT("type_promotion_stable_id"), TEXT("type_promotion:Add:int:real"));
		StatementEvidence->SetStringField(TEXT("type_promotion_operator"), TEXT("Add"));
		StatementEvidence->SetStringField(TEXT("type_promotion_source_pin_type"), TEXT("int"));
		StatementEvidence->SetStringField(TEXT("type_promotion_target_pin_type"), TEXT("real"));
		StatementEvidence->SetStringField(TEXT("type_promotion_result_pin_type"), TEXT("real"));

		TSharedRef<FJsonObject> ExpressionEvidence = MakeShared<FJsonObject>();
		ExpressionEvidence->SetStringField(TEXT("graph_latent_allowed"), TEXT("false"));
		ExpressionEvidence->SetStringField(TEXT("schedule_operation"), TEXT("latent_or_async_node"));

		TSharedRef<FJsonObject> Delay = MakeShared<FJsonObject>();
		Delay->SetStringField(TEXT("kind"), TEXT("literal"));
		Delay->SetStringField(TEXT("type"), TEXT("real"));
		Delay->SetNumberField(TEXT("value"), 0.25);

		TSharedRef<FJsonObject> ScheduleArgs = MakeShared<FJsonObject>();
		ScheduleArgs->SetObjectField(TEXT("delay"), Delay);

		TSharedRef<FJsonObject> ScheduleExpression = MakeShared<FJsonObject>();
		ScheduleExpression->SetStringField(TEXT("id"), TEXT("expr_schedule_context"));
		ScheduleExpression->SetStringField(TEXT("kind"), TEXT("schedule"));
		ScheduleExpression->SetStringField(TEXT("target"), TEXT("Delay"));
		ScheduleExpression->SetStringField(TEXT("function_operation"), TEXT("schedule_function"));
		ScheduleExpression->SetStringField(TEXT("schedule_operation"), TEXT("latent_or_async_node"));
		ScheduleExpression->SetObjectField(TEXT("context_evidence"), ExpressionEvidence);
		ScheduleExpression->SetObjectField(TEXT("args"), ScheduleArgs);

		TSharedRef<FJsonObject> Args = MakeShared<FJsonObject>();
		Args->SetObjectField(TEXT("value"), ScheduleExpression);

		TSharedRef<FJsonObject> Statement = MakeShared<FJsonObject>();
		Statement->SetStringField(TEXT("id"), TEXT("stmt_context_evidence"));
		Statement->SetStringField(TEXT("kind"), TEXT("convert"));
		Statement->SetStringField(TEXT("target"), TEXT("Add"));
		Statement->SetStringField(TEXT("function_operation"), TEXT("convert_function"));
		Statement->SetStringField(TEXT("transform_operation"), TEXT("type_promotion"));
		Statement->SetObjectField(TEXT("context_evidence"), StatementEvidence);
		Statement->SetObjectField(TEXT("args"), Args);

		TArray<TSharedPtr<FJsonValue>> Statements;
		Statements.Add(MakeShared<FJsonValueObject>(Statement));
		LogicSpec->SetArrayField(TEXT("statements"), Statements);
		return LogicSpec;
	}
```

- [ ] **Step 5: Add the UE runtime fact**

Append after the resolved stable id runtime facts:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphSemanticIRRuntimeFact_ContextEvidenceSurvivesBuildRequest,
	"BlueprintHelper.GraphWrite.GraphSemanticIR.RuntimeFact.ContextEvidenceSurvivesBuildRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperGraphSemanticIRRuntimeFact_ContextEvidenceSurvivesBuildRequest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphSemanticIR IR;
	const bool bBuilt = FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(
		FBlueprintHelperGraphSemanticIRRuntimeFactTestsLocalUtils::MakeLogicSpecWithContextEvidence(),
		IR);

	TestTrue(TEXT("logic spec builds"), bBuilt);
	TestEqual(TEXT("one statement parsed"), IR.Statements.Num(), 1);
	if (IR.Statements.Num() != 1 || !IR.Statements[0].IsValid())
	{
		return false;
	}

	const FBlueprintHelperGraphStatementIR& Statement = *IR.Statements[0];
	bool bPassed = true;
	bPassed &= TestEqual(
		TEXT("statement evidence operator"),
		Statement.ContextEvidence.FindRef(TEXT("type_promotion_operator")),
		FString(TEXT("Add")));
	bPassed &= TestEqual(
		TEXT("statement evidence target pin type"),
		Statement.ContextEvidence.FindRef(TEXT("type_promotion_target_pin_type")),
		FString(TEXT("real")));

	const FBlueprintHelperGraphFragmentBuildRequest StatementRequest =
		FBlueprintHelperGraphFragmentBuildRequest::FromStatement(Statement);
	bPassed &= TestEqual(
		TEXT("build request evidence stable id"),
		StatementRequest.ContextEvidence.FindRef(TEXT("type_promotion_stable_id")),
		FString(TEXT("type_promotion:Add:int:real")));

	const TSharedPtr<FBlueprintHelperGraphExpressionIR>* ExpressionPtr = Statement.Args.Find(TEXT("value"));
	bPassed &= TestTrue(TEXT("nested expression exists"), ExpressionPtr && ExpressionPtr->IsValid());
	if (ExpressionPtr && ExpressionPtr->IsValid())
	{
		const FBlueprintHelperGraphExpressionIR& Expression = *ExpressionPtr->Get();
		bPassed &= TestEqual(
			TEXT("expression evidence latent flag"),
			Expression.ContextEvidence.FindRef(TEXT("graph_latent_allowed")),
			FString(TEXT("false")));
		const FBlueprintHelperGraphFragmentBuildRequest ExpressionRequest =
			FBlueprintHelperGraphFragmentBuildRequest::FromExpression(Expression);
		bPassed &= TestEqual(
			TEXT("expression build request evidence schedule operation"),
			ExpressionRequest.ContextEvidence.FindRef(TEXT("schedule_operation")),
			FString(TEXT("latent_or_async_node")));
	}

	return bPassed;
}
```

- [ ] **Step 6: Strengthen ActionRequest append source contract**

In `FBlueprintHelperActionResolutionFunctionFragmentLifecycleCoordinatorContractTest`, extract `BuildCreateFragment` as an additional source slice:

```cpp
	FString CreateFragmentSlice;
	if (!TryExtractSourceSlice(
		GraphStatementBuilderSource,
		TEXT("BuildCreateFragment("),
		TEXT("BuildActionProviderFragment("),
		CreateFragmentSlice))
	{
		AddError(TEXT("Could not extract BuildCreateFragment source slice."));
		bClean = false;
	}
```

After the slice extraction succeeds, add:

```cpp
	bClean &= TestTrue(
		TEXT("BuildCallFunctionFragment appends context evidence to action request"),
		CallFragmentSlice.Contains(TEXT("ActionRequest.ContextEvidence.Append(BoundRequest.ContextEvidence);")));
	bClean &= TestTrue(
		TEXT("BuildCreateFragment appends context evidence to action request"),
		CreateFragmentSlice.Contains(TEXT("ActionRequest.ContextEvidence.Append(Request.ContextEvidence);")));
	bClean &= TestTrue(
		TEXT("BuildActionProviderFragment appends context evidence to action request"),
		ActionProviderSlice.Contains(TEXT("ActionRequest.ContextEvidence.Append(Request.ContextEvidence);")));
```

- [ ] **Step 7: Run focused UE automation**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.GraphSemanticIR.RuntimeFact.ContextEvidenceSurvivesBuildRequest+BlueprintHelper.GraphWrite.ActionResolution.Contract.FunctionFragmentLifecycleCoordinator;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_ContextEvidence_GREEN_001'
```

Expected: both tests pass.

---

## Task 3: Rebuild, Rerun Four-Cluster Smoke, and Sync Docs

**Files:**
- Modify: `BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/SmokeRecord_20260524_CN.md`
- Modify: `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
- Modify: `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`

- [ ] **Step 1: Rebuild TS packages**

Run:

```powershell
npm.cmd --prefix D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core run test
npm.cmd --prefix D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\cli run build
```

Expected:

- task-core reports all suites passing.
- cli build exits `0`.

- [ ] **Step 2: Build UE 5.6 target**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload
```

Expected: `Result: Succeeded`.

- [ ] **Step 3: Start Editor/Bridge through lifecycle MCP**

Use the global lifecycle tool:

```json
{
  "project_file": "D:\\UEProjects\\Template\\Template.uproject",
  "wait_timeout_ms": 180000
}
```

Expected: the BlueprintHelper Bridge is available on `127.0.0.1:54321` before smoke starts.

- [ ] **Step 4: Run four-cluster smoke**

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\PlanArtifacts\GraphWriteFourClusterE2ESmoke_20260524\run_four_cluster_smoke.ps1"
```

Expected:

- `00_create_smoke_actor.json` through `04_generic_graph.json` preview and execute pass.
- `05_generic_expected_diagnostics.json` preview blocks with one of the expected diagnostic tokens: `needs_more_semantic_context`, `type_promotion`, `timer_delegate_node`, `schedule_operation`, `spawner evidence`.
- `BlueprintHelper.GraphWrite` automation inside the smoke script passes.
- UE build inside the smoke script succeeds.

- [ ] **Step 5: Update smoke record**

In `SmokeRecord_20260524_CN.md`, replace the current `PASS WITH HARNESS DETAIL` conclusion only after the smoke script exits `0` with this plain-text section:

```text
Status: PASS

Rerun command:

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\PlanArtifacts\GraphWriteFourClusterE2ESmoke_20260524\run_four_cluster_smoke.ps1"

Result root: `BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/Results`

Validation:

- positive four-cluster preview/execute specs passed
- expected Generic diagnostics remained blocked
- `BlueprintHelper.GraphWrite` automation passed
- UE 5.6 build passed
```

If the smoke still fails with `bridge_unavailable` or `ECONNREFUSED 127.0.0.1:54321`, keep the status as `PASS WITH HARNESS DETAIL` or `BLOCKED BY HARNESS`, record the lifecycle detail in `SmokeRecord_20260524_CN.md`, and do not touch the bug document.

- [ ] **Step 6: Sync design and gap docs**

Update `BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md` and `BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md` with only these facts:

- call/action-provider fragment lifecycle remains coordinator-owned
- call fragment `semantic_kind` ownership tag is restored
- `context_evidence` is proven through TS compiler output, append bridge payload, UE SemanticIR, and FragmentBuildRequest
- four-cluster smoke status and artifact path

- [ ] **Step 7: Verify bug doc is not polluted**

Run:

```powershell
git diff -- BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_FourClusterE2ESmoke_Bugs_20260524_CN.md
```

Expected: no diff unless Task 3 found a real plugin defect that is independent of editor/Bridge lifecycle.

---

## Task 4: Final Read-Only Audit

**Files:**
- Read only: GraphWrite ActionResolution/GraphStatement C++ files, task-core compiler files, four-cluster smoke artifacts, design/gap docs.

- [ ] **Step 1: Dispatch final read-only reviewer**

Ask the reviewer to check:

- `asset_action` still never returns synthetic success.
- `type_promotion` still uses `FTypePromotion::GetOperatorSpawner` and projected typed pin evidence.
- Function call/action-provider resolve/spawn lifecycle remains inside `FBlueprintHelperActionFragmentSpawnCoordinator`.
- call/action-provider fragment ownership tags include `semantic_kind`.
- `context_evidence` is verified by executable tests, not only source token scans.
- four-cluster smoke result is current and bug doc contains no harness-only issue.

- [ ] **Step 2: Run final focused verification set locally**

Run:

```powershell
npm.cmd --prefix D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core run test
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Contract+BlueprintHelper.GraphWrite.FunctionFieldUnifiedSmoke+BlueprintHelper.GraphWrite.GraphSemanticIR.RuntimeFact.ContextEvidenceSurvivesBuildRequest;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_FurtherFix_Final_001'
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload
```

Expected:

- task-core tests pass
- focused UE automation has `failed=0`
- UE build reports `Result: Succeeded`

## Completion Criteria

The plan is complete only when all are true:

- `CallFragmentSemanticKindOwnership` passes.
- `FunctionFragmentLifecycleCoordinator` contract checks coordinator ownership, semantic tag preservation, and context evidence append points.
- task-core compiler tests assert `context_evidence` through task plan and append bridge payload.
- UE runtime fact asserts `context_evidence` through SemanticIR and `FBlueprintHelperGraphFragmentBuildRequest`.
- four-cluster smoke either passes with Bridge running or is honestly recorded as a harness lifecycle blocker without touching the bug doc.
- final read-only audit returns approved or all findings are resolved.

## Manual Commit Guidance

Do not commit inside the execution agent. After review, the user can commit only this task's files with:

```powershell
git add BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteFunctionFieldUnifiedSmokeTests.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphSemanticIRRuntimeFactTests.cpp `
  AgentFaceService/task-core/src/task/compiler/task-compiler.convert-schedule.test.ts `
  BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FurtherFixPlan_20260524_CN.md `
  BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/SmokeRecord_20260524_CN.md `
  BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md `
  BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md
git commit -m "修复内容：`n1. 恢复 GraphWrite call fragment semantic_kind 合约`n2. 补齐 context_evidence 编译到 UE 请求链路验证"
```
