# GraphWrite 80% Capability P2 Test Metrics Collection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立 3 个复杂需求的稳定执行与指标采集框架，即使场景尚未全部通过，也能产出可复验的错误率、call 正确率和 DebugBundle 记录。

**Architecture:** P2 建立测试 harness 和 metrics model，不修 resolver 正确率。Setup Phase 与 GraphWrite Phase 分开记录，PhysicalDoor 的资产/组件准备失败不污染 GraphWrite 正确率。所有测试记录必须能回写到 `BlueprintHelper_GraphWrite_80PercentCapability_TestRecord_20260522_CN.md`。

**Tech Stack:** UE 5.6 Automation Tests、BlueprintHelper C++、GraphWrite pipeline、DebugBundle、Markdown test record、PowerShell。

---

## Files

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGraphWriteCapabilityMetrics.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGraphWriteCapabilityMetrics.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWrite80PercentCapabilityTests.cpp`
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_TestRecord_20260522_CN.md`
- Read only: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_Roadmap_20260522_CN.md`

## Manual Commit Policy

- P2 执行者不得自动提交。
- 每次运行测试后只更新测试记录文档中的实际结果与 evidence path。

## Task 1: Add Metrics DTO

- [ ] **Step 1: Create public metrics header**

Create `BlueprintHelperGraphWriteCapabilityMetrics.h`:

```cpp
#pragma once

#include "CoreMinimal.h"

enum class EBlueprintHelperGraphWriteCapabilityErrorKind : uint8
{
	None,
	SetupFailure,
	MissingRequiredEvidence,
	CandidateThresholdExceeded,
	AmbiguousCandidates,
	NotFound,
	UnsupportedIntent,
	SpawnOrLinkFailure,
	SilentWrongGraph,
	NotRun
};

struct FBlueprintHelperGraphWriteCapabilityCaseResult
{
	FString CaseName;
	FString Phase;
	FString Capability;
	EBlueprintHelperGraphWriteCapabilityErrorKind ErrorKind = EBlueprintHelperGraphWriteCapabilityErrorKind::NotRun;
	bool bCallCorrect = false;
	bool bGraphWriteCorrect = false;
	FString EvidenceSummary;
	FString DebugBundlePath;
	FString GapUpdate;
};

struct FBlueprintHelperGraphWriteCapabilitySummary
{
	int32 GraphWriteCasesRun = 0;
	int32 GraphWriteCasesCorrect = 0;
	int32 CallSamplesRun = 0;
	int32 CallSamplesCorrect = 0;
	int32 SilentWrongGraphCount = 0;

	double GraphWriteCorrectRate() const;
	double CallCorrectRate() const;
};

class FBlueprintHelperGraphWriteCapabilityMetrics
{
public:
	static const TCHAR* ErrorKindToString(EBlueprintHelperGraphWriteCapabilityErrorKind Kind);
	static FBlueprintHelperGraphWriteCapabilitySummary Summarize(const TArray<FBlueprintHelperGraphWriteCapabilityCaseResult>& Results);
	static FString ToMarkdownRow(const FBlueprintHelperGraphWriteCapabilityCaseResult& Result);
};
```

- [ ] **Step 2: Implement metrics logic**

Create `BlueprintHelperGraphWriteCapabilityMetrics.cpp`:

```cpp
#include "Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGraphWriteCapabilityMetrics.h"

double FBlueprintHelperGraphWriteCapabilitySummary::GraphWriteCorrectRate() const
{
	return GraphWriteCasesRun > 0 ? static_cast<double>(GraphWriteCasesCorrect) / static_cast<double>(GraphWriteCasesRun) : 0.0;
}

double FBlueprintHelperGraphWriteCapabilitySummary::CallCorrectRate() const
{
	return CallSamplesRun > 0 ? static_cast<double>(CallSamplesCorrect) / static_cast<double>(CallSamplesRun) : 0.0;
}

const TCHAR* FBlueprintHelperGraphWriteCapabilityMetrics::ErrorKindToString(const EBlueprintHelperGraphWriteCapabilityErrorKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperGraphWriteCapabilityErrorKind::None: return TEXT("none");
	case EBlueprintHelperGraphWriteCapabilityErrorKind::SetupFailure: return TEXT("setup_failure");
	case EBlueprintHelperGraphWriteCapabilityErrorKind::MissingRequiredEvidence: return TEXT("missing_required_evidence");
	case EBlueprintHelperGraphWriteCapabilityErrorKind::CandidateThresholdExceeded: return TEXT("candidate_threshold_exceeded");
	case EBlueprintHelperGraphWriteCapabilityErrorKind::AmbiguousCandidates: return TEXT("ambiguous_candidates");
	case EBlueprintHelperGraphWriteCapabilityErrorKind::NotFound: return TEXT("not_found");
	case EBlueprintHelperGraphWriteCapabilityErrorKind::UnsupportedIntent: return TEXT("unsupported_intent");
	case EBlueprintHelperGraphWriteCapabilityErrorKind::SpawnOrLinkFailure: return TEXT("spawn_or_link_failure");
	case EBlueprintHelperGraphWriteCapabilityErrorKind::SilentWrongGraph: return TEXT("silent_wrong_graph");
	case EBlueprintHelperGraphWriteCapabilityErrorKind::NotRun: return TEXT("not_run");
	default: return TEXT("unknown");
	}
}

FBlueprintHelperGraphWriteCapabilitySummary FBlueprintHelperGraphWriteCapabilityMetrics::Summarize(
	const TArray<FBlueprintHelperGraphWriteCapabilityCaseResult>& Results)
{
	FBlueprintHelperGraphWriteCapabilitySummary Summary;
	for (const FBlueprintHelperGraphWriteCapabilityCaseResult& Result : Results)
	{
		if (Result.Phase.Equals(TEXT("GraphWrite"), ESearchCase::IgnoreCase))
		{
			++Summary.GraphWriteCasesRun;
			if (Result.bGraphWriteCorrect)
			{
				++Summary.GraphWriteCasesCorrect;
			}
		}
		if (Result.bCallCorrect || Result.ErrorKind != EBlueprintHelperGraphWriteCapabilityErrorKind::NotRun)
		{
			++Summary.CallSamplesRun;
			if (Result.bCallCorrect)
			{
				++Summary.CallSamplesCorrect;
			}
		}
		if (Result.ErrorKind == EBlueprintHelperGraphWriteCapabilityErrorKind::SilentWrongGraph)
		{
			++Summary.SilentWrongGraphCount;
		}
	}
	return Summary;
}

FString FBlueprintHelperGraphWriteCapabilityMetrics::ToMarkdownRow(
	const FBlueprintHelperGraphWriteCapabilityCaseResult& Result)
{
	return FString::Printf(
		TEXT("| %s | %s | %s | 运行完成 | %s | %s | %s | %s | %s |"),
		*Result.CaseName,
		*Result.Phase,
		*Result.Capability,
		ErrorKindToString(Result.ErrorKind),
		Result.bCallCorrect ? TEXT("是") : TEXT("否"),
		Result.bGraphWriteCorrect ? TEXT("是") : TEXT("否"),
		Result.DebugBundlePath.IsEmpty() ? TEXT("未生成") : *Result.DebugBundlePath,
		Result.GapUpdate.IsEmpty() ? TEXT("不需要") : *Result.GapUpdate);
}
```

- [ ] **Step 3: Verify compile references**

Run:

```powershell
rg -n "BlueprintHelperGraphWriteCapabilityMetrics" BlueprintHelper/Source/BlueprintHelper
```

Expected: the new `.h/.cpp` and tests reference the type.

## Task 2: Add Metrics Unit Tests

- [ ] **Step 1: Create automation test shell**

Create `BlueprintHelperGraphWrite80PercentCapabilityTests.cpp` with a first metrics test:

```cpp
#include "Misc/AutomationTest.h"
#include "Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGraphWriteCapabilityMetrics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteCapabilityMetricsSummaryTest,
	"BlueprintHelper.GraphWrite.Capability80.P2.MetricsSummary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteCapabilityMetricsSummaryTest::RunTest(const FString& Parameters)
{
	TArray<FBlueprintHelperGraphWriteCapabilityCaseResult> Results;
	Results.Add({TEXT("PhysicalDoor_InteractableOnly"), TEXT("Setup"), TEXT("创建资产和组件"), EBlueprintHelperGraphWriteCapabilityErrorKind::None, false, false, TEXT("setup ok"), TEXT(""), TEXT("")});
	Results.Add({TEXT("PhysicalDoor_InteractableOnly"), TEXT("GraphWrite"), TEXT("物理门内部逻辑"), EBlueprintHelperGraphWriteCapabilityErrorKind::None, true, true, TEXT("graph ok"), TEXT("Saved/Automation/GraphWrite80_Door/index.json"), TEXT("")});
	Results.Add({TEXT("TimedAccessGate_StateMachine"), TEXT("GraphWrite"), TEXT("Function / Field / Control"), EBlueprintHelperGraphWriteCapabilityErrorKind::AmbiguousCandidates, false, false, TEXT("ambiguous call"), TEXT("Saved/Automation/GraphWrite80_Gate/index.json"), TEXT("Function ambiguity")});

	const FBlueprintHelperGraphWriteCapabilitySummary Summary = FBlueprintHelperGraphWriteCapabilityMetrics::Summarize(Results);
	TestEqual(TEXT("GraphWrite cases run"), Summary.GraphWriteCasesRun, 2);
	TestEqual(TEXT("GraphWrite cases correct"), Summary.GraphWriteCasesCorrect, 1);
	TestEqual(TEXT("Call samples run"), Summary.CallSamplesRun, 2);
	TestEqual(TEXT("Call samples correct"), Summary.CallSamplesCorrect, 1);
	TestEqual(TEXT("Silent wrong graphs"), Summary.SilentWrongGraphCount, 0);
	TestTrue(TEXT("Markdown row uses error kind"), FBlueprintHelperGraphWriteCapabilityMetrics::ToMarkdownRow(Results[2]).Contains(TEXT("ambiguous_candidates")));
	return true;
}
```

- [ ] **Step 2: Run metrics test**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.Capability80.P2.MetricsSummary;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite80_P2_Metrics_001'
```

Expected: `1 succeeded`, `0 failed`.

## Task 3: Add Three Scenario Harness Entries

- [ ] **Step 1: Add three scenario tests as controlled not-run/diagnostic producers**

In `BlueprintHelperGraphWrite80PercentCapabilityTests.cpp`, add:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteCapabilityScenarioRegistrationTest,
	"BlueprintHelper.GraphWrite.Capability80.P2.ScenarioRegistration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteCapabilityScenarioRegistrationTest::RunTest(const FString& Parameters)
{
	TArray<FBlueprintHelperGraphWriteCapabilityCaseResult> Results;
	Results.Add({TEXT("PhysicalDoor_InteractableOnly"), TEXT("Setup"), TEXT("创建资产和组件"), EBlueprintHelperGraphWriteCapabilityErrorKind::NotRun, false, false, TEXT("registered"), TEXT(""), TEXT("")});
	Results.Add({TEXT("PhysicalDoor_InteractableOnly"), TEXT("GraphWrite"), TEXT("物理门内部逻辑"), EBlueprintHelperGraphWriteCapabilityErrorKind::NotRun, false, false, TEXT("registered"), TEXT(""), TEXT("")});
	Results.Add({TEXT("TimedAccessGate_StateMachine"), TEXT("GraphWrite"), TEXT("Function / Field / Control"), EBlueprintHelperGraphWriteCapabilityErrorKind::NotRun, false, false, TEXT("registered"), TEXT(""), TEXT("")});
	Results.Add({TEXT("EventDrivenConfigApplier"), TEXT("GraphWrite"), TEXT("Struct / Event / Delegate"), EBlueprintHelperGraphWriteCapabilityErrorKind::NotRun, false, false, TEXT("registered"), TEXT(""), TEXT("")});

	TestEqual(TEXT("scenario count"), Results.Num(), 4);
	for (const FBlueprintHelperGraphWriteCapabilityCaseResult& Result : Results)
	{
		TestFalse(TEXT("case name is set"), Result.CaseName.IsEmpty());
		TestFalse(TEXT("phase is set"), Result.Phase.IsEmpty());
		TestFalse(TEXT("capability is set"), Result.Capability.IsEmpty());
	}
	return true;
}
```

- [ ] **Step 2: Run scenario registration test**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.Capability80.P2.ScenarioRegistration;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite80_P2_Scenarios_001'
```

Expected: `1 succeeded`, `0 failed`.

## Task 4: Update Test Record

- [ ] **Step 1: Replace P0 not-run summary with P2 harness evidence**

Update the test record summary:

```markdown
| P2 MetricsSummary Automation | PASS | `Saved/Automation/GraphWrite80_P2_Metrics_001/index.json` |
| P2 ScenarioRegistration Automation | PASS | `Saved/Automation/GraphWrite80_P2_Scenarios_001/index.json` |
```

- [ ] **Step 2: Keep scenario rows registered but not counted as passed**

Scenario rows remain `not_run` until P3-P6 implement actual GraphWrite execution. P2 must not inflate correctness.

## Task 5: Build Plugin

- [ ] **Step 1: Run BuildPlugin**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat' BuildPlugin -Plugin='D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\BlueprintHelper.uplugin' -Package='D:\UEProjects\Template\Saved\BlueprintHelperBuildTest_GraphWrite80_P2' -TargetPlatforms=Win64
```

Expected: `BUILD SUCCESSFUL`.

## P2 Exit Criteria

- [ ] Metrics DTO compiles.
- [ ] Metrics summary automation passes.
- [ ] Scenario registration automation passes.
- [ ] Test record includes P2 evidence without claiming scenario correctness.
- [ ] UE 5.6 BuildPlugin passes.

