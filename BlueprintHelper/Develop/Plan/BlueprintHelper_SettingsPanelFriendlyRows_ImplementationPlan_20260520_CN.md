# BlueprintHelper SettingsPanel Friendly Rows Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 BlueprintHelper Settings 页升级为中文本地化、用户友好的设置行，支持 OverlapHint，并通过事件驱动链路写回 `setting.json`。

**Architecture:** 采用 `SettingStore -> SettingsPresenter -> SettingRow -> SettingsPanel` 四层边界。`SettingStore` 只负责配置读写和路径级更新，`SettingsPresenter` 负责行模型、校验和事件协调，`SBlueprintHelperSettingRow` 只负责单行展示和输入事件转发，`SBlueprintHelperSettingsPanel` 只负责分类布局和 presenter 事件绑定。

**Tech Stack:** Unreal Engine 5.6、C++、Slate、JsonObject、BlueprintHelper `Config` / `UI/Settings` 模块、UTF-8 JSON 设置文件。

---

## Scope

本计划只实现 Settings 页的友好行、OverlapHint、本地化、事件驱动写回和最小可验证测试。

不改变 BlueprintHelper 主窗口样式，不开放 BlueprintHelper 自身 UI 布局设置，不把开发者专用配置暴露给普通用户，不新增旧字段兼容路径，不使用 timer、delay、ActiveTimer、轮询刷新或 AsyncTask 延迟作为 UI 状态同步方案。

## Editable User Settings

第一阶段只显示并允许编辑这些用户安全项：

| Dot Path | Type | UI 分组 | 中文标签 |
| --- | --- | --- | --- |
| `ui.review_panel.diff_frame_outer_padding` | number | Review 可视化 | Diff 外边距 |
| `ui.review_panel.diff_action_padding` | number | Review 可视化 | 操作按钮内边距 |
| `ui.review_panel.diff_action_spacing` | number | Review 可视化 | 操作按钮间距 |
| `ui.review_panel.surface_overlay_fill_alpha` | number | Review 可视化 | Diff 填充透明度 |
| `ui.review_panel.surface_overlay_selected_fill_alpha` | number | Review 可视化 | 选中 Diff 填充透明度 |
| `ui.review_panel.surface_geometry_padding` | number | Review 可视化 | Diff 几何外扩 |
| `ui.review_panel.debug_max_messages` | integer | Review 调试 | Debug 最大消息数 |
| `review.debug_bundle.retention` | choice | Review 调试 | DebugBundle 保留策略 |
| `debug.export_profile` | choice | 调试导出 | Debug 导出级别 |
| `debug.contains_full_settings` | boolean | 调试导出 | 导出完整设置 |
| `tool_clusters.signature.reference_context_max_results` | integer | 工具输出 | 签名引用最大结果数 |
| `tool_clusters.read_context.max_output_rows` | integer | 工具输出 | Read Context 最大行数 |
| `tool_clusters.read_context.max_output_bytes` | integer | 工具输出 | Read Context 最大字节数 |

## File Structure

Create:

- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\UI\Settings\BlueprintHelperSettingRowViewModel.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\UI\Settings\SBlueprintHelperSettingRow.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Settings\SBlueprintHelperSettingRow.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Tests\Config\BlueprintHelperSettingStoreTests.cpp`

Modify:

- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\Config\BlueprintHelperSettingStore.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\Config\BlueprintHelperSettingStore.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\UI\Settings\BlueprintHelperSettingsPresenter.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Settings\BlueprintHelperSettingsPresenter.cpp`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\UI\Settings\SBlueprintHelperSettingsPanel.h`
- `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Settings\SBlueprintHelperSettingsPanel.cpp`

## Event Flow

```text
用户修改 Row
  -> SBlueprintHelperSettingRow 发送 FBlueprintHelperSettingEditEvent
  -> SBlueprintHelperSettingsPanel 转发给 FBlueprintHelperSettingsPresenter
  -> Presenter 校验和规范化值
  -> Presenter 调用 FBlueprintHelperSettingStore::UpdateProjectSettingValue
  -> Store 更新 D:\UEProjects\Template\.blueprinthelper\setting.json
  -> Presenter 重新生成 RowViewModel 并广播 OnRowsChanged
  -> Panel 根据 presenter 状态重建分类行
```

Reset flow:

```text
用户点击重置
  -> Row 发送 DotPath reset request
  -> Panel 转发给 Presenter
  -> Presenter 调用 Store::ResetProjectSettingValue
  -> Store 删除 project override
  -> Presenter 重新读取 default/project 合并值并广播
  -> Panel 从 OnRowsChanged 刷新
```

## Task 1: Add Setting Row View Model

**Files:**

- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\UI\Settings\BlueprintHelperSettingRowViewModel.h`

- [ ] **Step 1: Create row model header**

Add:

```cpp
#pragma once

#include "CoreMinimal.h"

struct FBlueprintHelperSettingChoiceViewModel
{
	FString Value;
	FText Label;
};

enum class EBlueprintHelperSettingValueType : uint8
{
	Number,
	Integer,
	Boolean,
	String,
	Choice
};

struct FBlueprintHelperSettingRowViewModel
{
	FString DotPath;
	FText CategoryLabel;
	FText DisplayLabel;
	FText OverlapHint;
	EBlueprintHelperSettingValueType ValueType = EBlueprintHelperSettingValueType::String;
	FString CurrentValue;
	FString DefaultValue;
	FString ErrorText;
	TArray<FBlueprintHelperSettingChoiceViewModel> Choices;
	double MinValue = 0.0;
	double MaxValue = 0.0;
	bool bHasMinValue = false;
	bool bHasMaxValue = false;
	bool bModified = false;
	bool bEnabled = true;
};

struct FBlueprintHelperSettingEditEvent
{
	FString DotPath;
	FString NewValue;
};
```

## Task 2: Extend SettingStore with Dot-Path Write APIs

**Files:**

- Modify: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\Config\BlueprintHelperSettingStore.h`
- Modify: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\Config\BlueprintHelperSettingStore.cpp`
- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Tests\Config\BlueprintHelperSettingStoreTests.cpp`

- [ ] **Step 1: Add public APIs**

Add to `FBlueprintHelperSettingStore`:

```cpp
static bool UpdateProjectSettingValue(const FString& DotPath, const FString& NewValue, FString& OutError);
static bool ResetProjectSettingValue(const FString& DotPath, FString& OutError);
static bool UpdateSettingJsonText(const FString& InputJson, const FString& DotPath, const FString& NewValue, FString& OutJson, FString& OutError);
static bool RemoveSettingJsonPath(const FString& InputJson, const FString& DotPath, FString& OutJson, FString& OutError);
```

- [ ] **Step 2: Add path and value helpers**

Add file-local helpers in `BlueprintHelperSettingStore.cpp`:

```cpp
namespace
{
	static bool SplitDotPath(const FString& DotPath, TArray<FString>& OutParts, FString& OutError)
	{
		DotPath.ParseIntoArray(OutParts, TEXT("."), true);
		if (OutParts.Num() == 0)
		{
			OutError = TEXT("setting_path_empty");
			return false;
		}
		for (const FString& Part : OutParts)
		{
			if (Part.IsEmpty())
			{
				OutError = FString::Printf(TEXT("setting_path_invalid:%s"), *DotPath);
				return false;
			}
		}
		return true;
	}

	static TSharedPtr<FJsonValue> ConvertSettingStringToJsonValue(const FString& NewValue)
	{
		if (NewValue.Equals(TEXT("true"), ESearchCase::IgnoreCase))
		{
			return MakeShared<FJsonValueBoolean>(true);
		}
		if (NewValue.Equals(TEXT("false"), ESearchCase::IgnoreCase))
		{
			return MakeShared<FJsonValueBoolean>(false);
		}
		if (NewValue.IsNumeric())
		{
			double NumberValue = 0.0;
			LexTryParseString(NumberValue, *NewValue);
			return MakeShared<FJsonValueNumber>(NumberValue);
		}
		return MakeShared<FJsonValueString>(NewValue);
	}
}
```

- [ ] **Step 3: Implement pure JSON update**

Implement:

```cpp
bool FBlueprintHelperSettingStore::UpdateSettingJsonText(const FString& InputJson, const FString& DotPath, const FString& NewValue, FString& OutJson, FString& OutError)
{
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InputJson);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("setting_json_parse_failed");
		return false;
	}

	TArray<FString> Parts;
	if (!SplitDotPath(DotPath, Parts, OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Cursor = RootObject;
	for (int32 Index = 0; Index < Parts.Num() - 1; ++Index)
	{
		const FString& Part = Parts[Index];
		TSharedPtr<FJsonObject> Child = Cursor->GetObjectField(Part);
		if (!Child.IsValid())
		{
			Child = MakeShared<FJsonObject>();
			Cursor->SetObjectField(Part, Child);
		}
		Cursor = Child;
	}

	Cursor->SetField(Parts.Last(), ConvertSettingStringToJsonValue(NewValue));
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
	{
		OutError = TEXT("setting_json_serialize_failed");
		return false;
	}
	return true;
}
```

- [ ] **Step 4: Implement pure JSON remove**

Implement:

```cpp
bool FBlueprintHelperSettingStore::RemoveSettingJsonPath(const FString& InputJson, const FString& DotPath, FString& OutJson, FString& OutError)
{
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InputJson);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("setting_json_parse_failed");
		return false;
	}

	TArray<FString> Parts;
	if (!SplitDotPath(DotPath, Parts, OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Cursor = RootObject;
	for (int32 Index = 0; Index < Parts.Num() - 1; ++Index)
	{
		const TSharedPtr<FJsonObject>* Child = nullptr;
		if (!Cursor->TryGetObjectField(Parts[Index], Child) || !Child || !Child->IsValid())
		{
			OutJson = InputJson;
			return true;
		}
		Cursor = *Child;
	}

	Cursor->RemoveField(Parts.Last());
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
	{
		OutError = TEXT("setting_json_serialize_failed");
		return false;
	}
	return true;
}
```

- [ ] **Step 5: Implement disk write APIs**

Use `FBlueprintHelperProjectConfigPaths::GetProjectSettingPath()` and save with `FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM`:

```cpp
bool FBlueprintHelperSettingStore::UpdateProjectSettingValue(const FString& DotPath, const FString& NewValue, FString& OutError)
{
	const FString SettingPath = FBlueprintHelperProjectConfigPaths::GetProjectSettingPath();
	FString InputJson;
	if (!FFileHelper::LoadFileToString(InputJson, *SettingPath))
	{
		InputJson = TEXT("{}\n");
	}

	FString OutputJson;
	if (!UpdateSettingJsonText(InputJson, DotPath, NewValue, OutputJson, OutError))
	{
		return false;
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(SettingPath), true);
	if (!FFileHelper::SaveStringToFile(OutputJson, *SettingPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("setting_write_failed:%s"), *SettingPath);
		return false;
	}
	return true;
}
```

`ResetProjectSettingValue` uses `RemoveSettingJsonPath` and the same save path.

- [ ] **Step 6: Add Store test**

Create `BlueprintHelperSettingStoreTests.cpp`:

```cpp
#include "Misc/AutomationTest.h"
#include "Systems/Config/BlueprintHelperSettingStore.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueprintHelperSettingStoreUpdateJsonTest, "BlueprintHelper.Settings.Store.UpdateJsonPath", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSettingStoreUpdateJsonTest::RunTest(const FString& Parameters)
{
	FString OutJson;
	FString Error;
	TestTrue(TEXT("number path updates"), FBlueprintHelperSettingStore::UpdateSettingJsonText(TEXT("{}"), TEXT("ui.review_panel.diff_action_spacing"), TEXT("6"), OutJson, Error));
	TestTrue(TEXT("number value appears"), OutJson.Contains(TEXT("diff_action_spacing")) && OutJson.Contains(TEXT("6")));

	FString BoolJson;
	TestTrue(TEXT("bool path updates"), FBlueprintHelperSettingStore::UpdateSettingJsonText(OutJson, TEXT("debug.contains_full_settings"), TEXT("true"), BoolJson, Error));
	TestTrue(TEXT("bool value appears"), BoolJson.Contains(TEXT("contains_full_settings")) && BoolJson.Contains(TEXT("true")));

	FString RemovedJson;
	TestTrue(TEXT("path removes"), FBlueprintHelperSettingStore::RemoveSettingJsonPath(BoolJson, TEXT("debug.contains_full_settings"), RemovedJson, Error));
	TestFalse(TEXT("removed value disappears"), RemovedJson.Contains(TEXT("contains_full_settings")));
	return true;
}
```

- [ ] **Step 7: Compile**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex
```

Expected: `Succeeded`.

## Task 3: Build Presenter Row Models and Event API

**Files:**

- Modify: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\UI\Settings\BlueprintHelperSettingsPresenter.h`
- Modify: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Settings\BlueprintHelperSettingsPresenter.cpp`

- [ ] **Step 1: Add presenter row API**

Add:

```cpp
#include "UI/Settings/BlueprintHelperSettingRowViewModel.h"

DECLARE_MULTICAST_DELEGATE(FBlueprintHelperSettingsRowsChanged);

const TArray<FBlueprintHelperSettingRowViewModel>& GetRows() const;
FBlueprintHelperSettingsRowsChanged& OnRowsChanged();
void ReloadRows();
void HandleSettingValueCommitted(const FBlueprintHelperSettingEditEvent& Event);
void HandleSettingResetRequested(const FString& DotPath);
```

Add private state:

```cpp
TArray<FBlueprintHelperSettingRowViewModel> Rows;
FBlueprintHelperSettingsRowsChanged RowsChanged;
TMap<FString, FString> RowErrorsByPath;
```

- [ ] **Step 2: Create localized row helpers**

In cpp:

```cpp
#define LOCTEXT_NAMESPACE "BlueprintHelperSettingsPresenter"

static FBlueprintHelperSettingRowViewModel MakeNumberRow(const FString& DotPath, const FText& Category, const FText& Label, const FText& Hint, const FString& Current, const FString& Default, double Min, double Max)
{
	FBlueprintHelperSettingRowViewModel Row;
	Row.DotPath = DotPath;
	Row.CategoryLabel = Category;
	Row.DisplayLabel = Label;
	Row.OverlapHint = Hint;
	Row.ValueType = EBlueprintHelperSettingValueType::Number;
	Row.CurrentValue = Current;
	Row.DefaultValue = Default;
	Row.MinValue = Min;
	Row.MaxValue = Max;
	Row.bHasMinValue = true;
	Row.bHasMaxValue = true;
	Row.bModified = Current != Default;
	return Row;
}
```

Add equivalent helpers for integer, boolean and choice rows.

- [ ] **Step 3: Implement localized row list**

`ReloadRows` must create rows for every editable dot path in this plan. Category text:

```cpp
const FText ReviewVisualCategory = LOCTEXT("SettingsCategoryReviewVisual", "Review 可视化");
const FText ReviewDebugCategory = LOCTEXT("SettingsCategoryReviewDebug", "Review 调试");
const FText DebugExportCategory = LOCTEXT("SettingsCategoryDebugExport", "调试导出");
const FText ToolOutputCategory = LOCTEXT("SettingsCategoryToolOutput", "工具输出");
```

Example row:

```cpp
Rows.Add(MakeNumberRow(
	TEXT("ui.review_panel.diff_frame_outer_padding"),
	ReviewVisualCategory,
	LOCTEXT("DiffFrameOuterPaddingLabel", "Diff 外边距"),
	LOCTEXT("DiffFrameOuterPaddingHint", "调整 Diff 框与被标记区域之间的外扩距离。只影响 Review 可视化，不改变资产。"),
	CurrentValue,
	DefaultValue,
	0.0,
	64.0));
```

- [ ] **Step 4: Validate edits in presenter**

Add validation:

```cpp
bool FBlueprintHelperSettingsPresenter::ValidateRowValue(const FBlueprintHelperSettingRowViewModel& Row, const FString& NewValue, FString& OutNormalizedValue, FText& OutErrorText) const
{
	switch (Row.ValueType)
	{
	case EBlueprintHelperSettingValueType::Number:
	{
		double Parsed = 0.0;
		if (!LexTryParseString(Parsed, *NewValue))
		{
			OutErrorText = LOCTEXT("SettingErrorNumber", "请输入数字。");
			return false;
		}
		if (Row.bHasMinValue && Parsed < Row.MinValue)
		{
			OutErrorText = FText::Format(LOCTEXT("SettingErrorMin", "数值不能小于 {0}。"), FText::AsNumber(Row.MinValue));
			return false;
		}
		if (Row.bHasMaxValue && Parsed > Row.MaxValue)
		{
			OutErrorText = FText::Format(LOCTEXT("SettingErrorMax", "数值不能大于 {0}。"), FText::AsNumber(Row.MaxValue));
			return false;
		}
		OutNormalizedValue = FString::SanitizeFloat(Parsed);
		return true;
	}
	case EBlueprintHelperSettingValueType::Integer:
	{
		int32 Parsed = 0;
		if (!LexTryParseString(Parsed, *NewValue))
		{
			OutErrorText = LOCTEXT("SettingErrorInteger", "请输入整数。");
			return false;
		}
		OutNormalizedValue = LexToString(Parsed);
		return true;
	}
	case EBlueprintHelperSettingValueType::Boolean:
		OutNormalizedValue = NewValue.Equals(TEXT("true"), ESearchCase::IgnoreCase) ? TEXT("true") : TEXT("false");
		return true;
	default:
		OutNormalizedValue = NewValue;
		return true;
	}
}
```

- [ ] **Step 5: Implement event handlers**

`HandleSettingValueCommitted` validates, calls `FBlueprintHelperSettingStore::UpdateProjectSettingValue`, reloads rows, then broadcasts `RowsChanged.Broadcast()`.

`HandleSettingResetRequested` calls `FBlueprintHelperSettingStore::ResetProjectSettingValue`, reloads rows, then broadcasts.

Failure stores localized row error in `RowErrorsByPath` and still broadcasts so UI updates deterministically.

- [ ] **Step 6: Compile**

Run the Build command. Expected: `Succeeded`.

## Task 4: Implement `SBlueprintHelperSettingRow`

**Files:**

- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\UI\Settings\SBlueprintHelperSettingRow.h`
- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Settings\SBlueprintHelperSettingRow.cpp`

- [ ] **Step 1: Add row widget header**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UI/Settings/BlueprintHelperSettingRowViewModel.h"

DECLARE_DELEGATE_OneParam(FBlueprintHelperSettingValueCommitted, const FBlueprintHelperSettingEditEvent&);
DECLARE_DELEGATE_OneParam(FBlueprintHelperSettingResetRequested, const FString&);

class SBlueprintHelperSettingRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperSettingRow) {}
		SLATE_ARGUMENT(FBlueprintHelperSettingRowViewModel, Row)
		SLATE_EVENT(FBlueprintHelperSettingValueCommitted, OnValueCommitted)
		SLATE_EVENT(FBlueprintHelperSettingResetRequested, OnResetRequested)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> BuildValueWidget();
	TSharedRef<SWidget> BuildNumberWidget();
	TSharedRef<SWidget> BuildIntegerWidget();
	TSharedRef<SWidget> BuildBooleanWidget();
	TSharedRef<SWidget> BuildChoiceWidget();
	TSharedRef<SWidget> BuildStringWidget();
	void CommitValue(const FString& NewValue) const;
	FReply HandleResetClicked() const;

private:
	FBlueprintHelperSettingRowViewModel Row;
	FBlueprintHelperSettingValueCommitted OnValueCommitted;
	FBlueprintHelperSettingResetRequested OnResetRequested;
	TArray<TSharedPtr<FBlueprintHelperSettingChoiceViewModel>> ChoiceItems;
};
```

- [ ] **Step 2: Build row layout**

Use this visual hierarchy:

```text
SBorder
  SVerticalBox
    SHorizontalBox
      Label column: DisplayLabel + DotPath
      Value column: typed editor
      Hint column: "说明" text with ToolTipText(Row.OverlapHint)
      Reset column: "重置" button
    Error row: Row.ErrorText when non-empty
```

- [ ] **Step 3: Implement value editors**

Number:

```cpp
SNew(SNumericEntryBox<double>)
.Value_Lambda([this]() -> TOptional<double>
{
	double Parsed = 0.0;
	return LexTryParseString(Parsed, *Row.CurrentValue) ? TOptional<double>(Parsed) : TOptional<double>();
})
.MinValue(Row.bHasMinValue ? TOptional<double>(Row.MinValue) : TOptional<double>())
.MaxValue(Row.bHasMaxValue ? TOptional<double>(Row.MaxValue) : TOptional<double>())
.OnValueCommitted_Lambda([this](double NewValue, ETextCommit::Type)
{
	CommitValue(FString::SanitizeFloat(NewValue));
})
```

Boolean:

```cpp
SNew(SCheckBox)
.IsChecked_Lambda([this]()
{
	return Row.CurrentValue.Equals(TEXT("true"), ESearchCase::IgnoreCase) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
})
.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
{
	CommitValue(NewState == ECheckBoxState::Checked ? TEXT("true") : TEXT("false"));
})
```

String uses `SEditableTextBox`; choice uses `SComboBox<TSharedPtr<FBlueprintHelperSettingChoiceViewModel>>` and commits the stable `Value`.

- [ ] **Step 4: Implement reset**

```cpp
FReply SBlueprintHelperSettingRow::HandleResetClicked() const
{
	if (OnResetRequested.IsBound())
	{
		OnResetRequested.Execute(Row.DotPath);
	}
	return FReply::Handled();
}
```

- [ ] **Step 5: Compile**

Run the Build command. Expected: `Succeeded`.

## Task 5: Replace SettingsPanel Raw JSON View with Category Rows

**Files:**

- Modify: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\UI\Settings\SBlueprintHelperSettingsPanel.h`
- Modify: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Settings\SBlueprintHelperSettingsPanel.cpp`

- [ ] **Step 1: Add presenter and container members**

```cpp
TSharedPtr<SVerticalBox> CategoriesBox;
TSharedPtr<FBlueprintHelperSettingsPresenter> Presenter;
```

- [ ] **Step 2: Construct presenter and subscribe to events**

```cpp
Presenter = MakeShared<FBlueprintHelperSettingsPresenter>();
Presenter->OnRowsChanged().AddSP(this, &SBlueprintHelperSettingsPanel::RefreshRows);
Presenter->ReloadRows();
```

- [ ] **Step 3: Implement `RefreshRows`**

```cpp
void SBlueprintHelperSettingsPanel::RefreshRows()
{
	if (!CategoriesBox.IsValid() || !Presenter.IsValid())
	{
		return;
	}

	CategoriesBox->ClearChildren();
	TMap<FString, TArray<FBlueprintHelperSettingRowViewModel>> RowsByCategory;
	for (const FBlueprintHelperSettingRowViewModel& Row : Presenter->GetRows())
	{
		RowsByCategory.FindOrAdd(Row.CategoryLabel.ToString()).Add(Row);
	}

	for (const TPair<FString, TArray<FBlueprintHelperSettingRowViewModel>>& Pair : RowsByCategory)
	{
		CategoriesBox->AddSlot()
		.AutoHeight()
		.Padding(4.0f, 6.0f)
		[
			BuildCategorySection(FText::FromString(Pair.Key), Pair.Value)
		];
	}
}
```

- [ ] **Step 4: Forward row events only**

Panel row construction must forward events and not write files:

```cpp
SNew(SBlueprintHelperSettingRow)
.Row(Row)
.OnValueCommitted_Lambda([this](const FBlueprintHelperSettingEditEvent& Event)
{
	if (Presenter.IsValid())
	{
		Presenter->HandleSettingValueCommitted(Event);
	}
})
.OnResetRequested_Lambda([this](const FString& DotPath)
{
	if (Presenter.IsValid())
	{
		Presenter->HandleSettingResetRequested(DotPath);
	}
})
```

- [ ] **Step 5: Compile**

Run the Build command. Expected: `Succeeded`.

## Task 6: Validate Behavior

**Files:**

- Read during validation: `D:\UEProjects\Template\.blueprinthelper\setting.json`
- Read during validation: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Config\DefaultSetting.json`

- [ ] **Step 1: Final compile**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex
```

Expected: `Succeeded`.

- [ ] **Step 2: UI verification**

Open BlueprintHelper panel and switch to Settings page.

Expected:

- Settings page shows `Review 可视化`、`Review 调试`、`调试导出`、`工具输出` categories.
- Each row has Chinese label and OverlapHint tooltip.
- Editing `Diff 外边距` writes project override to `D:\UEProjects\Template\.blueprinthelper\setting.json`.
- Resetting `Diff 外边距` removes project override and falls back to `DefaultSetting.json`.
- Invalid numeric input shows localized row error and does not corrupt JSON.
- Boolean and choice edits refresh through presenter event broadcast.

## Task 7: Manual Commit Handoff

Do not run `git add`, `git commit`, or `git push` from Codex.

If all tasks pass, use this manual commit message:

```text
新增内容：
1. 添加 SettingsPanel 中文友好设置行、OverlapHint 和事件驱动写回链路
2. 添加 SettingStore 路径级写入/重置能力和配置更新测试
```

Manual commands:

```powershell
git add BlueprintHelper/Source/BlueprintHelper/Public/UI/Settings/BlueprintHelperSettingRowViewModel.h `
  BlueprintHelper/Source/BlueprintHelper/Public/UI/Settings/SBlueprintHelperSettingRow.h `
  BlueprintHelper/Source/BlueprintHelper/Private/UI/Settings/SBlueprintHelperSettingRow.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/Systems/Config/BlueprintHelperSettingStore.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/Config/BlueprintHelperSettingStore.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/UI/Settings/BlueprintHelperSettingsPresenter.h `
  BlueprintHelper/Source/BlueprintHelper/Private/UI/Settings/BlueprintHelperSettingsPresenter.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/UI/Settings/SBlueprintHelperSettingsPanel.h `
  BlueprintHelper/Source/BlueprintHelper/Private/UI/Settings/SBlueprintHelperSettingsPanel.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/Config/BlueprintHelperSettingStoreTests.cpp `
  BlueprintHelper/Develop/Plan/BlueprintHelper_SettingsPanelFriendlyRows_ImplementationPlan_20260520_CN.md

git commit -m "新增内容：添加 SettingsPanel 中文友好设置行、OverlapHint 和事件驱动写回链路"
```

## Self-Review

- Spec coverage: 覆盖中文友好行、OverlapHint、事件驱动写回、用户可编辑项限制、Store/Presenter/UI 分层、编译验证和测试。
- Placeholder scan: 没有 `TBD`、`TODO`、`implement later` 或无实现细节的泛化步骤。
- Type consistency: `FBlueprintHelperSettingRowViewModel`、`FBlueprintHelperSettingEditEvent`、`FBlueprintHelperSettingsPresenter`、`SBlueprintHelperSettingRow` 的命名保持一致。
- Architecture fit: UI 不直接写文件；Presenter 处理验证和事件；Store 处理读写；不引入 delay/polling。
