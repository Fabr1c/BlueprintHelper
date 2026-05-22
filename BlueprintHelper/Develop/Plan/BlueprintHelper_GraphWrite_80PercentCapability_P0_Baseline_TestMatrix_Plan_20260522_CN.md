# GraphWrite 80% Capability P0 Baseline Test Matrix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 固化 80% 能力/正确率的基线口径、三复杂需求测试矩阵、错误分类、测试记录模板和 legacy 删除门禁。

**Architecture:** P0 不实现 GraphWrite 新能力，只建立后续 P1-P6 的验收框架。所有指标必须区分 Setup Phase、GraphWrite Phase、call correctness、GraphWrite correctness、silent wrong graph 和 gap update。文档与契约测试是门禁，防止后续阶段重新把旧 fallback 或无 evidence 成功路径写回主链路。

**Tech Stack:** BlueprintHelper Markdown docs、UE 5.6 Automation Tests、GraphWrite plan docs、PowerShell、RunUAT BuildPlugin。

---

## Files

- Create: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_TestRecord_20260522_CN.md`
- Modify: `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWrite80PercentPlanContractTests.cpp`
- Read only: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_Roadmap_20260522_CN.md`
- Read only: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`

## Manual Commit Policy

- 本仓库任务执行者不得自动运行 `git add`、`git commit`、`git push`。
- P0 完成后只输出手动提交建议，且提交范围只包含本计划实际修改文件。

## Task 1: Create Test Record Document

- [ ] **Step 1: Write the initial test record**

Create `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_TestRecord_20260522_CN.md` with these sections:

```markdown
# BlueprintHelper GraphWrite 80% 能力与正确率测试记录

日期：2026-05-22

## 1. 计分口径

| 指标 | 口径 |
|---|---|
| Setup Phase | 资产创建、组件准备、保存/打开/校验资产；失败记录为 setup_failure，不计入 GraphWrite 正确率。 |
| GraphWrite Phase | 从基础 Blueprint 可定位后开始，记录 context、resolver、spawn、default、link、readback、DebugBundle。 |
| Call 正确 | callable owner、函数名、参数、pin 绑定、执行顺序均符合需求。 |
| GraphWrite 正确 | 节点类型、pin/default、exec/data link、readback、DebugBundle、编译/preview 语义均符合需求。 |
| Silent wrong graph | 表面成功但 readback、编译或 preview 证明语义错误；单独计入最高优先级缺陷。 |

## 2. 测试矩阵

| 测试 | Phase | 能力项 | 当前状态 | 错误分类 | Call 正确 | GraphWrite 正确 | Evidence / DebugBundle | Gap 更新 |
|---|---|---|---|---|---|---|---|---|
| PhysicalDoor_InteractableOnly | Setup | 创建 Actor Blueprint 和 Root/Hinge/DoorMesh | 未运行 | not_run | N/A | N/A | 未生成 | 不需要 |
| PhysicalDoor_InteractableOnly | GraphWrite | 物理门内部逻辑 | 未运行 | not_run | 未统计 | 未统计 | 未生成 | 未判断 |
| TimedAccessGate_StateMachine | GraphWrite | Function / Field / Control | 未运行 | not_run | 未统计 | 未统计 | 未生成 | 未判断 |
| EventDrivenConfigApplier | GraphWrite | Struct / Event / Delegate | 未运行 | not_run | 未统计 | 未统计 | 未生成 | 未判断 |

## 3. 错误分类

| 分类 | 说明 |
|---|---|
| setup_failure | 资产、组件、保存、打开、fixture 准备失败。 |
| missing_required_evidence | 缺 callable、owner、field owner、property path、binding object、delegate signature、target type 等关键 evidence。 |
| candidate_threshold_exceeded | ActionDatabase / filter 后候选数量超过安全阈值。 |
| ambiguous_candidates | 候选数量未超阈值，但多个候选不能安全区分。 |
| not_found | 上下文足够但无合法候选。 |
| unsupported_intent | 当前架构尚未实现该语义。 |
| spawn_or_link_failure | resolver 成功后 spawn、默认值、pin binding、link 或 readback 失败。 |
| silent_wrong_graph | 表面成功但 graph 语义错误。 |
| not_run | 该项尚未执行。 |

## 4. 当前汇总

| 指标 | 数值 | 说明 |
|---|---:|---|
| 已运行 GraphWrite 场景数 | 0 | P0 只建立基线。 |
| GraphWrite 正确场景数 | 0 | P0 不计入正确率。 |
| Call 正确样本数 | 0 | P0 不计入 call 正确率。 |
| Silent wrong graph 数 | 0 | P0 尚未执行测试。 |
```

- [ ] **Step 2: Verify the document has no blank result cells**

Run:

```powershell
Select-String -Path 'BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_TestRecord_20260522_CN.md' -Pattern '\|\s*\|'
```

Expected: no matches. Empty cells are not allowed; use `未运行`, `未统计`, `未生成`, `未判断`, or `N/A`.

## Task 2: Add Legacy Deletion Rule to Gap Document

- [ ] **Step 1: Add a short policy section**

Modify `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md` near the current remaining gap summary:

```markdown
## Legacy 删除门禁

后续 GraphWrite 80% 能力推进采用 legacy 严格模式：只要代码路径保留旧语义、旧 fallback、旧模型、旧 transaction/review 解释，默认视为 legacy，不能复用进新主链路。

判定为旧路径后默认直接删除。若暂时不能删除，必须在本 gap 文档中保留一条明确记录，包含：

1. 旧路径文件与入口。
2. 当前仍不能删除的具体原因。
3. 它为什么不能进入新主链路。
4. 删除前置条件。
```

- [ ] **Step 2: Verify the policy is discoverable**

Run:

```powershell
Select-String -Path 'BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md' -Pattern 'Legacy 删除门禁|默认直接删除|不能进入新主链路'
```

Expected: all three patterns are found.

## Task 3: Add Plan Contract Automation

- [ ] **Step 1: Create source-level document contract test**

Create `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWrite80PercentPlanContractTests.cpp`:

```cpp
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace BlueprintHelperGraphWrite80PercentPlanContractTests
{
	static FString ProjectRoot()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	}

	static bool LoadProjectFile(const FString& RelativePath, FString& OutText)
	{
		return FFileHelper::LoadFileToString(OutText, *(ProjectRoot() / RelativePath));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWrite80PercentPlanDocsContractTest,
	"BlueprintHelper.GraphWrite.Capability80.P0.PlanDocsContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWrite80PercentPlanDocsContractTest::RunTest(const FString& Parameters)
{
	FString Roadmap;
	TestTrue(TEXT("roadmap loads"),
		BlueprintHelperGraphWrite80PercentPlanContractTests::LoadProjectFile(
			TEXT("Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_Roadmap_20260522_CN.md"),
			Roadmap));
	TestTrue(TEXT("roadmap defines legacy strict mode"), Roadmap.Contains(TEXT("Legacy 严格模式")));
	TestTrue(TEXT("roadmap defines direct spawn boundary"), Roadmap.Contains(TEXT("Direct spawn 边界")));
	TestTrue(TEXT("roadmap defines NeedsMoreSemanticContext distinction"), Roadmap.Contains(TEXT("必要上下文缺失不等于候选超过阈值")));

	FString TestRecord;
	TestTrue(TEXT("test record loads"),
		BlueprintHelperGraphWrite80PercentPlanContractTests::LoadProjectFile(
			TEXT("Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_TestRecord_20260522_CN.md"),
			TestRecord));
	TestTrue(TEXT("test record includes physical door"), TestRecord.Contains(TEXT("PhysicalDoor_InteractableOnly")));
	TestTrue(TEXT("test record includes synthetic function field control"), TestRecord.Contains(TEXT("TimedAccessGate_StateMachine")));
	TestTrue(TEXT("test record includes synthetic struct event delegate"), TestRecord.Contains(TEXT("EventDrivenConfigApplier")));
	TestTrue(TEXT("test record initializes as not run"), TestRecord.Contains(TEXT("not_run")));

	FString GapDoc;
	TestTrue(TEXT("gap doc loads"),
		BlueprintHelperGraphWrite80PercentPlanContractTests::LoadProjectFile(
			TEXT("Plugins/BlueprintHelper/BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md"),
			GapDoc));
	TestTrue(TEXT("gap doc records legacy deletion gate"), GapDoc.Contains(TEXT("Legacy 删除门禁")));
	TestTrue(TEXT("gap doc requires delete or gap reason"), GapDoc.Contains(TEXT("默认直接删除")));

	return true;
}
```

- [ ] **Step 2: Run targeted automation**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.Capability80.P0.PlanDocsContract;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite80_P0_PlanDocs_001'
```

Expected: exit code `0`, report contains `1 succeeded`, `0 failed`.

## Task 4: Build Plugin

- [ ] **Step 1: Run UE 5.6 BuildPlugin**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat' BuildPlugin -Plugin='D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\BlueprintHelper.uplugin' -Package='D:\UEProjects\Template\Saved\BlueprintHelperBuildTest_GraphWrite80_P0' -TargetPlatforms=Win64
```

Expected: `BUILD SUCCESSFUL`.

- [ ] **Step 2: Update test record with verification evidence**

Modify `BlueprintHelper_GraphWrite_80PercentCapability_TestRecord_20260522_CN.md` section `当前汇总` with automation/build evidence paths:

```markdown
| P0 文档契约 Automation | PASS | `Saved/Automation/GraphWrite80_P0_PlanDocs_001/index.json` |
| UE 5.6 BuildPlugin | PASS | `Saved/BlueprintHelperBuildTest_GraphWrite80_P0` |
```

## P0 Exit Criteria

- [ ] Test record document exists and has no empty result cells.
- [ ] Gap document contains the legacy deletion gate.
- [ ] P0 plan contract automation passes.
- [ ] UE 5.6 BuildPlugin passes.
- [ ] No automatic git staging, commit, or push was performed.

