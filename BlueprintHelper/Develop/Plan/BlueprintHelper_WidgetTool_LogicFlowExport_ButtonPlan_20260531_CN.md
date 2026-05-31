# WidgetTool LogicFlow Export Button Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 BlueprintHelper 主 WidgetTool / TaskSpecWorkbench 页面增加 `logicflow` 导出按钮，并给 `logicflow`、`logicmd`、`logicjson` 三个格式按钮添加导出内容与推荐场景 tips，按钮顺序按信息密度/压缩等级排列为 `logicflow > logicmd > logicjson`。

**Architecture:** 保留现有 UI -> Presenter -> Service 的同步导出 pipeline。Widget 只新增按钮、tooltip 和事件转发；导出格式选择继续通过 `FBlueprintHelperTaskSpecWorkbenchVisualEvent::ExportReadContext` 进入 presenter；T3D 到具体 ReadContext 格式的转换由 `FBlueprintHelperReadContextExportService` 和 `UBlueprintHelperTaskSpecWorkbenchUtils` 承担。

**Tech Stack:** Unreal Engine 5.6 C++ Slate UI、BlueprintHelper TaskSpecWorkbench presenter/service、UE Automation Tests。

---

## Constraints

1. 不执行 `git add`、`git commit`、`git push`，不删除 `.git` 内任何文件。
2. 当前工作区已有无关 dirty changes；只修改本计划列出的文件。
3. 不在 UI widget 中实现导出业务逻辑；UI 只负责按钮展示和事件转发。
4. `logic_flow` 从结构化节点/链接数据生成，不从 `logic_md` 文本反解析。
5. 不引入 timer、ActiveTimer、AsyncTask delay 或 retry loop。
6. 保持 UE 5.6 主路径，不做跨版本兼容改写。

## File Structure

- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/TaskSpecWorkbench/BlueprintHelperTaskSpecWorkbenchData.h`
  - Add `LogicFlow` to `EBlueprintHelperReadContextExportFormat`.

- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/TaskSpecWorkbench/SBlueprintHelperTaskSpecWorkbench.h`
  - Declare `OnExportLogicFlowClicked()`.

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/TaskSpecWorkbench/SBlueprintHelperTaskSpecWorkbench.cpp`
  - Add the `Export logicflow` button before `Export logicmd`.
  - Add tips to all three format buttons.
  - Add `OnExportLogicFlowClicked()` using the existing visual event pipeline.

- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/TaskSpecWorkbench/BlueprintHelperTaskSpecWorkbenchServices.h`
  - No signature change expected; only include if helper declarations need to remain visible through the service API.

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/TaskSpecWorkbench/BlueprintHelperTaskSpecWorkbenchServices.cpp`
  - Update T3D status text to mention all three formats.
  - Branch `LogicFlow` before `LogicJson` / `LogicMd` and return `LogicFlow.v1`.
  - Keep current md/json behavior unchanged.

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/TaskSpecWorkbench/Utils/BlueprintHelperTaskSpecWorkbenchUtils.h`
  - Declare `BuildLogicFlowPayload(const TSharedPtr<FJsonObject>& RawJsonRoot, TSharedRef<FJsonObject> OutPayload)`.

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/TaskSpecWorkbench/Utils/BlueprintHelperTaskSpecWorkbenchUtils.cpp`
  - Build `LogicFlow.v1` directly from raw converted nodes/links.
  - Emit `schema`, `mode`, `flow`, `stats`, and `warnings`.

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/UI/BlueprintHelperTaskSpecWorkbenchServicesTests.cpp`
  - Extend existing export-format automation test with `LogicFlow`.

---

### Task 1: Add Failing LogicFlow Export Test

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/UI/BlueprintHelperTaskSpecWorkbenchServicesTests.cpp`

- [ ] **Step 1: Extend the existing format export test**

Inside `FBlueprintHelperTaskSpecWorkbenchExportsT3DReadContextFormatsTest::RunTest`, add this block after the T3D classification assertions and before the existing `LogicJsonRequest` block:

```cpp
	FBlueprintHelperReadContextExportRequest LogicFlowRequest;
	LogicFlowRequest.SourceText = T3DText;
	LogicFlowRequest.Format = EBlueprintHelperReadContextExportFormat::LogicFlow;
	const FBlueprintHelperReadContextExportResult LogicFlowResult =
		FBlueprintHelperReadContextExportService::Export(LogicFlowRequest);

	TestTrue(TEXT("logicflow export succeeds"), LogicFlowResult.bSucceeded);
	TestTrue(TEXT("logicflow has schema"), LogicFlowResult.ExportText.Contains(TEXT("LogicFlow.v1")));
	TestTrue(TEXT("logicflow has flow field"), LogicFlowResult.ExportText.Contains(TEXT("\"flow\"")));
	TestTrue(TEXT("logicflow has stats"), LogicFlowResult.ExportText.Contains(TEXT("\"stats\"")));
	TestTrue(TEXT("logicflow status message"), LogicFlowResult.Message.Contains(TEXT("logicflow")));
```

- [ ] **Step 2: Run the focused automation test and confirm the expected initial failure**

Run from the project root if the editor/test harness is available:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun -project='D:\UEProjects\Template\Template.uproject' -runautomationtests='BlueprintHelper.UI.TaskSpecWorkbench.ExportsT3DReadContextFormats'
```

Expected before implementation: compile/test failure because `EBlueprintHelperReadContextExportFormat::LogicFlow` does not exist.

If the local automation command is not available in this environment, record that blocker and continue with compile-level verification after implementation.

---

### Task 2: Extend Export Format Routing

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/TaskSpecWorkbench/BlueprintHelperTaskSpecWorkbenchData.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/TaskSpecWorkbench/SBlueprintHelperTaskSpecWorkbench.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/TaskSpecWorkbench/SBlueprintHelperTaskSpecWorkbench.cpp`

- [ ] **Step 1: Add the enum value**

Change `EBlueprintHelperReadContextExportFormat` to:

```cpp
enum class EBlueprintHelperReadContextExportFormat : uint8
{
	LogicFlow,
	LogicMd,
	LogicJson
};
```

- [ ] **Step 2: Declare the new click handler**

Add to `SBlueprintHelperTaskSpecWorkbench` private handlers:

```cpp
	FReply OnExportLogicFlowClicked();
```

- [ ] **Step 3: Add the LogicFlow button first**

In `SBlueprintHelperTaskSpecWorkbench::Construct`, insert this slot before the `Export logicmd` slot:

```cpp
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(WorkbenchSettings.ButtonSpacing)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Export logicflow")))
				.ToolTipText(FText::FromString(TEXT("Recommended first: exports compact LogicFlow.v1 execution/data flow for quickly understanding simple entries. Not an anchor source.")))
				.OnClicked(this, &SBlueprintHelperTaskSpecWorkbench::OnExportLogicFlowClicked)
			]
```

- [ ] **Step 4: Replace md/json tips**

Use these tooltip strings for the existing two buttons:

```cpp
.ToolTipText(FText::FromString(TEXT("Exports medium-density LogicMD readable nodes and links. Recommended when logicflow is too compact for branched or larger entries.")))
```

```cpp
.ToolTipText(FText::FromString(TEXT("Exports structured LogicJson with the most detail. Recommended for precise analysis, diff, patch/merge anchors, and debug.")))
```

- [ ] **Step 5: Add the click handler implementation**

Place it before `OnExportLogicMdClicked()`:

```cpp
FReply SBlueprintHelperTaskSpecWorkbench::OnExportLogicFlowClicked()
{
	return Presenter.IsValid()
		? Presenter->HandleVisualEvent(
			FBlueprintHelperTaskSpecWorkbenchVisualEvent::ExportReadContext(
				EBlueprintHelperReadContextExportFormat::LogicFlow))
		: FReply::Handled();
}
```

---

### Task 3: Generate LogicFlow Payload in Service Boundary

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/TaskSpecWorkbench/BlueprintHelperTaskSpecWorkbenchServices.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/TaskSpecWorkbench/Utils/BlueprintHelperTaskSpecWorkbenchUtils.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/TaskSpecWorkbench/Utils/BlueprintHelperTaskSpecWorkbenchUtils.cpp`

- [ ] **Step 1: Declare the utility builder**

Add next to `BuildLogicJsonPayload`:

```cpp
	static void BuildLogicFlowPayload(const TSharedPtr<FJsonObject>& RawJsonRoot, TSharedRef<FJsonObject> OutPayload);
```

- [ ] **Step 2: Add service branch**

In `FBlueprintHelperReadContextExportService::Export`, update the T3D status message to:

```cpp
Document.StatusText = TEXT("Blueprint T3D detected. Export logicflow, logicmd, or logicjson to clipboard.");
```

Then add this branch before `LogicJson`:

```cpp
	if (Request.Format == EBlueprintHelperReadContextExportFormat::LogicFlow)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		UBlueprintHelperTaskSpecWorkbenchUtils::BuildLogicFlowPayload(RawJsonRoot, Payload);
		Result.ExportText = UBlueprintHelperTaskSpecWorkbenchUtils::SerializeJsonObject(Payload);
		Result.bSucceeded = true;
		Result.Message = TEXT("logicflow copied to clipboard.");
		return Result;
	}
```

- [ ] **Step 3: Implement structured LogicFlow builder**

Implement `BuildLogicFlowPayload` without parsing markdown. Minimal expected behavior:

```cpp
void UBlueprintHelperTaskSpecWorkbenchUtils::BuildLogicFlowPayload(
	const TSharedPtr<FJsonObject>& RawJsonRoot,
	TSharedRef<FJsonObject> OutPayload)
{
	const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
	RawJsonRoot->TryGetArrayField(TEXT("nodes"), Nodes);
	RawJsonRoot->TryGetArrayField(TEXT("links"), Links);

	// Build node id -> readable name from structured node data.
	// Build exec/data/unknown counts from structured link type/kind fields when present.
	// Use execflow when at least one exec link is present; otherwise use dataflow.
	// Flow body may be a simple deterministic node chain / node list for this workbench exporter.
}
```

Concrete output requirements:

```json
{
  "schema": "LogicFlow.v1",
  "mode": "execflow",
  "flow": "PrintString",
  "stats": {
    "nodes": 1,
    "exec_links": 0,
    "data_links": 0,
    "links": 0
  },
  "warnings": []
}
```

If there are no exec links, `mode` may be `dataflow`. If link type is missing or unrecognized, include `unknown_link` in `warnings`.

- [ ] **Step 4: Keep md/json unchanged**

Do not change `BuildLogicJsonPayload` or `BuildLogicMdFromRawJson` output shape except for code needed to share helper functions.

---

### Task 4: Run Focused Verification

**Files:**
- Modified files from Tasks 1-3 only.

- [ ] **Step 1: Run source grep checks**

```powershell
rg -n "LogicFlow|logicflow|logicmd|logicjson|ToolTipText|Export logic" BlueprintHelper\Source\BlueprintHelper\Public\UI\TaskSpecWorkbench BlueprintHelper\Source\BlueprintHelper\Private\UI\TaskSpecWorkbench BlueprintHelper\Source\BlueprintHelper\Private\Systems\TaskSpecWorkbench BlueprintHelper\Source\BlueprintHelper\Private\Tests\UI
```

Expected:
- `Export logicflow` appears before `Export logicmd` and `Export logicjson`.
- All three buttons have `ToolTipText`.
- Service has `logicflow copied to clipboard.`
- Test has `LogicFlow.v1` assertions.

- [ ] **Step 2: Run focused automation test**

Preferred:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun -project='D:\UEProjects\Template\Template.uproject' -runautomationtests='BlueprintHelper.UI.TaskSpecWorkbench.ExportsT3DReadContextFormats'
```

Expected: command exits with code `0`.

- [ ] **Step 3: Run UE compile if automation is unavailable or after code changes**

Use the repository's normal UE 5.6 compile path. If no project-specific command is available, run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload
```

Expected: command exits with code `0`.

- [ ] **Step 4: Inspect final diff**

```powershell
git diff -- BlueprintHelper/Develop/Plan/BlueprintHelper_WidgetTool_LogicFlowExport_ButtonPlan_20260531_CN.md BlueprintHelper/Source/BlueprintHelper/Public/UI/TaskSpecWorkbench/BlueprintHelperTaskSpecWorkbenchData.h BlueprintHelper/Source/BlueprintHelper/Public/UI/TaskSpecWorkbench/SBlueprintHelperTaskSpecWorkbench.h BlueprintHelper/Source/BlueprintHelper/Private/UI/TaskSpecWorkbench/SBlueprintHelperTaskSpecWorkbench.cpp BlueprintHelper/Source/BlueprintHelper/Private/Systems/TaskSpecWorkbench/BlueprintHelperTaskSpecWorkbenchServices.cpp BlueprintHelper/Source/BlueprintHelper/Private/Systems/TaskSpecWorkbench/Utils/BlueprintHelperTaskSpecWorkbenchUtils.h BlueprintHelper/Source/BlueprintHelper/Private/Systems/TaskSpecWorkbench/Utils/BlueprintHelperTaskSpecWorkbenchUtils.cpp BlueprintHelper/Source/BlueprintHelper/Private/Tests/UI/BlueprintHelperTaskSpecWorkbenchServicesTests.cpp
```

Expected: diff contains only this plan and the WidgetTool/TaskSpecWorkbench logicflow export changes.

---

## Final Manual Commit Handoff

Do not run these commands automatically. After verification, report these as the manual commit commands:

```powershell
git add BlueprintHelper/Develop/Plan/BlueprintHelper_WidgetTool_LogicFlowExport_ButtonPlan_20260531_CN.md `
  BlueprintHelper/Source/BlueprintHelper/Public/UI/TaskSpecWorkbench/BlueprintHelperTaskSpecWorkbenchData.h `
  BlueprintHelper/Source/BlueprintHelper/Public/UI/TaskSpecWorkbench/SBlueprintHelperTaskSpecWorkbench.h `
  BlueprintHelper/Source/BlueprintHelper/Private/UI/TaskSpecWorkbench/SBlueprintHelperTaskSpecWorkbench.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/TaskSpecWorkbench/BlueprintHelperTaskSpecWorkbenchServices.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/TaskSpecWorkbench/Utils/BlueprintHelperTaskSpecWorkbenchUtils.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/TaskSpecWorkbench/Utils/BlueprintHelperTaskSpecWorkbenchUtils.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/UI/BlueprintHelperTaskSpecWorkbenchServicesTests.cpp
git commit -m "新增内容：`n1. 添加 WidgetTool logicflow 导出按钮`n2. 添加 logicflow/logicmd/logicjson 格式提示`n`n变更需求：`n1. 按 flow > md > json 信息密度顺序排列导出格式按钮"
```

## Self-Review

Spec coverage:

1. 新增 logicflow 导出按钮：Task 2。
2. 三个 format 按钮 tips：Task 2。
3. 简单提示导出哪些内容并给出 flow/json 推荐：Task 2。
4. 按 flow > md > json 排序：Task 2。
5. 导出逻辑不堆在 UI：Task 3。
6. 测试/验证：Tasks 1 and 4。

Placeholder scan:

1. No `TBD`, `TODO`, or deferred implementation wording remains in required steps.
2. The only conditional text is verification fallback handling for local test command availability.

Type consistency:

1. `EBlueprintHelperReadContextExportFormat::LogicFlow` is used by UI, presenter event payload, service, and tests.
2. Output schema is `LogicFlow.v1`, matching the ReadContext LogicFlow rules document.
3. UI labels use existing workbench style: `Export logicflow`, `Export logicmd`, `Export logicjson`.
