# BlueprintHelper Settings Panel User Rows Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 BlueprintHelper Setting 页从只读 JSON 查看器升级为中文用户友好配置行，只开放用户可编辑项，并用事件驱动链路写回 `setting.json`。

**Architecture:** UI 只绘制行、显示中文文案、转发输入事件；`FBlueprintHelperSettingsPresenter` 负责行 ViewModel、校验、事件处理和 ViewChanged 广播；`FBlueprintHelperSettingStore` 负责 JSON 路径读写、dot-path 更新和保存。所有数据变化必须走 `SettingRow UI -> Presenter -> SettingStore -> Presenter ViewChanged -> Panel Refresh`，禁止 Widget 直接写 JSON。

**Tech Stack:** UE 5.6 C++、Slate、Json、BlueprintHelper `Systems/Config`、BlueprintHelper UI Presenter pattern、UTF-8 no BOM JSON。

---

## 0. Scope and Non-Goals

### In Scope

- Setting 页绘制用户友好的中文配置行。
- 每行附带 OverlapHint：行右侧显示提示入口，Hover 时展示中文说明。
- 只绘制 `settings_visibility.user_editable` 中允许用户编辑的配置。
- 用户改值后通过 Presenter 事件写回项目级 `.blueprinthelper/setting.json`。
- 写回后通过 Presenter 的 ViewChanged 事件刷新 Panel。
- 支持 number、integer、boolean、string 四种基础值。
- 对 number/integer 做最小范围校验，避免非法值破坏配置。
- 保留 JSON 原文只读折叠区或 Debug 区，作为开发排查入口。

### Out of Scope

- 不替换当前所有硬编码调用点实际读取 settings；本计划只实现可编辑 Setting 页和事件驱动写回。
- 不开放 BlueprintHelper 外壳 UI，例如主窗口 tab、通知、TaskSpec、Layout 页自身布局。
- 不引入 ActiveTimer、延迟刷新、轮询重试。
- 不引入完整 UE Localization pipeline；本轮使用 `LOCTEXT` 写中文文本，后续可被正式本地化采集。
- 不提交 git；按仓库规则最终只输出建议提交命令。

## 1. File Structure Map

### Config Data / Store

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Config/BlueprintHelperSettingTypes.h`
  - 定义 Setting value type、row category、row view model、row update request/result。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Config/BlueprintHelperSettingStore.h`
  - 增加 dot-path 更新、项目 setting 保存、project setting 读取辅助接口。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Config/BlueprintHelperSettingStore.cpp`
  - 实现 JSON dot-path 更新和保存到 `.blueprinthelper/setting.json`。

### Presenter

- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Settings/BlueprintHelperSettingsPresenter.h`
  - 增加 ViewChanged event sink、row view model、`HandleRowValueChanged()`、`HandleResetRowToDefault()`。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Settings/BlueprintHelperSettingsPresenter.cpp`
  - 生成中文行定义，处理输入事件，调用 Store 写回，广播 ViewChanged。

### UI

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Settings/SBlueprintHelperSettingRow.h`
  - 单行 Slate widget 类。
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Settings/SBlueprintHelperSettingRow.cpp`
  - 绘制中文名称、值输入控件、重置按钮、OverlapHint。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Settings/SBlueprintHelperSettingsPanel.h`
  - 保存 row widget 容器、处理 Presenter ViewChanged。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Settings/SBlueprintHelperSettingsPanel.cpp`
  - 从只读 JSON 主界面改为分类行列表 + Debug JSON 折叠区。

### Config Files

- Modify: `BlueprintHelper/Config/DefaultSetting.json`
  - 确认 `settings_visibility.user_editable` 只保留用户可编辑行。
- Modify: `D:/UEProjects/Template/.blueprinthelper/setting.json`
  - 同步默认项目配置。

### Tests / Validation

- Modify or create test in: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/RuntimeDiagnostics/BlueprintHelperSafetyTests.cpp`
  - 添加 SettingStore dot-path 写回测试。
- Compile command:
  - `& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReloadFromIDE`

## 2. User Editable Row Set

Only these keys are shown as normal user-editable rows:

| Key | 中文名称 | Type | Range | Hint |
|---|---|---|---|---|
| `ui.review_panel.diff_frame_outer_padding` | Diff 框外边距 | number | 0-20 | 调整 Diff 框与内容之间的外边距，只影响 Review 可视化。 |
| `ui.review_panel.diff_action_padding` | Diff 按钮内边距 | number | 0-20 | 调整 Accept/Reject 按钮区域的内边距。 |
| `ui.review_panel.diff_action_spacing` | Diff 按钮间距 | margin | 0-20 each | 调整 Accept/Reject 按钮之间的间距。 |
| `ui.review_panel.surface_overlay_fill_alpha` | Diff 覆盖透明度 | number | 0-1 | 调整未选中 Diff 覆盖层透明度。 |
| `ui.review_panel.surface_overlay_selected_fill_alpha` | 选中 Diff 覆盖透明度 | number | 0-1 | 调整选中 Diff 覆盖层透明度。 |
| `ui.review_panel.surface_geometry_padding` | Diff 绘制外扩 | vector2 | 0-50 each | 调整 Diff 框绘制时的几何外扩。 |
| `ui.review_panel.debug_max_messages` | Debug 最大消息数 | integer | 20-2000 | 控制 Review Debug 面板保留的消息数量。 |
| `review.debug_bundle.retention` | DebugBundle 保留策略 | string enum | `standard`, `keep_all`, `minimal` | 控制 Review DebugBundle 的保留策略。 |
| `debug.export_profile` | Debug 导出级别 | string enum | `standard`, `minimal`, `full` | 控制 Debug 导出信息量。 |
| `debug.contains_full_settings` | Debug 包含完整设置 | bool | true/false | 开启后 DebugBundle 可包含完整设置，可能暴露本地配置。 |
| `tool_clusters.signature.reference_context_max_results` | 签名引用最大结果数 | integer | 1-500 | 控制签名引用查询最多返回多少条。 |
| `tool_clusters.read_context.max_output_rows` | ReadContext 最大行数 | integer | 0-10000 | 控制读取上下文最多输出多少行，0 表示不限制。 |
| `tool_clusters.read_context.max_output_bytes` | ReadContext 最大字节数 | integer | 0-10485760 | 控制读取上下文最大输出大小，0 表示不限制。 |

`developer_only` keys are not displayed in normal mode.

## 3. Task Breakdown

### Task 1: Add setting row data types

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Config/BlueprintHelperSettingTypes.h`

- [ ] **Step 1: Create row type definitions**

Create `BlueprintHelperSettingTypes.h`:

```cpp
// BlueprintHelper settings row data types.

#pragma once

#include "CoreMinimal.h"

enum class EBlueprintHelperSettingValueType : uint8
{
	Number,
	Integer,
	Boolean,
	String,
	Margin,
	Vector2
};

enum class EBlueprintHelperSettingCategory : uint8
{
	ReviewVisual,
	ReviewDebug,
	ReadContext
};

struct FBlueprintHelperSettingValue
{
	FString StringValue;
	double NumberValue = 0.0;
	int32 IntegerValue = 0;
	bool bBoolValue = false;
	TArray<double> NumberArray;

	static FBlueprintHelperSettingValue FromNumber(double InValue);
	static FBlueprintHelperSettingValue FromInteger(int32 InValue);
	static FBlueprintHelperSettingValue FromBoolean(bool bInValue);
	static FBlueprintHelperSettingValue FromString(const FString& InValue);
	static FBlueprintHelperSettingValue FromNumberArray(const TArray<double>& InValue);
};

struct FBlueprintHelperSettingRowViewModel
{
	FString Key;
	FString DisplayName;
	FString HintText;
	FString ValueText;
	FString ErrorText;
	EBlueprintHelperSettingCategory Category = EBlueprintHelperSettingCategory::ReviewVisual;
	EBlueprintHelperSettingValueType ValueType = EBlueprintHelperSettingValueType::Number;
	double MinNumber = 0.0;
	double MaxNumber = 1.0;
	TArray<FString> EnumOptions;
	bool bUserEditable = true;
	bool bDirty = false;
};

struct FBlueprintHelperSettingUpdateRequest
{
	FString Key;
	EBlueprintHelperSettingValueType ValueType = EBlueprintHelperSettingValueType::Number;
	FBlueprintHelperSettingValue Value;
};

struct FBlueprintHelperSettingUpdateResult
{
	bool bSucceeded = false;
	FString StatusText;
	FString ErrorText;
};
```

- [ ] **Step 2: Add inline factory definitions**

Append this code to the same header below the structs:

```cpp
inline FBlueprintHelperSettingValue FBlueprintHelperSettingValue::FromNumber(double InValue)
{
	FBlueprintHelperSettingValue Value;
	Value.NumberValue = InValue;
	Value.StringValue = FString::SanitizeFloat(InValue);
	return Value;
}

inline FBlueprintHelperSettingValue FBlueprintHelperSettingValue::FromInteger(int32 InValue)
{
	FBlueprintHelperSettingValue Value;
	Value.IntegerValue = InValue;
	Value.NumberValue = static_cast<double>(InValue);
	Value.StringValue = FString::FromInt(InValue);
	return Value;
}

inline FBlueprintHelperSettingValue FBlueprintHelperSettingValue::FromBoolean(bool bInValue)
{
	FBlueprintHelperSettingValue Value;
	Value.bBoolValue = bInValue;
	Value.StringValue = bInValue ? TEXT("true") : TEXT("false");
	return Value;
}

inline FBlueprintHelperSettingValue FBlueprintHelperSettingValue::FromString(const FString& InValue)
{
	FBlueprintHelperSettingValue Value;
	Value.StringValue = InValue;
	return Value;
}

inline FBlueprintHelperSettingValue FBlueprintHelperSettingValue::FromNumberArray(const TArray<double>& InValue)
{
	FBlueprintHelperSettingValue Value;
	Value.NumberArray = InValue;
	TArray<FString> Parts;
	for (double Number : InValue)
	{
		Parts.Add(FString::SanitizeFloat(Number));
	}
	Value.StringValue = FString::Join(Parts, TEXT(", "));
	return Value;
}
```

- [ ] **Step 3: Commit**

Do not execute git automatically. Use this message later:

```text
新增内容：
1. 添加 Settings 行数据模型
```

### Task 2: Add SettingStore dot-path update and save support

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Config/BlueprintHelperSettingStore.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Config/BlueprintHelperSettingStore.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/RuntimeDiagnostics/BlueprintHelperSafetyTests.cpp`

- [ ] **Step 1: Extend SettingStore header**

Add include:

```cpp
#include "Systems/Config/BlueprintHelperSettingTypes.h"
```

Add public methods to `FBlueprintHelperSettingStore`:

```cpp
static FBlueprintHelperSettingUpdateResult UpdateProjectSettingValue(
	const FBlueprintHelperSettingUpdateRequest& Request);
static bool TryReadProjectSettingValue(
	const FString& Key,
	EBlueprintHelperSettingValueType ValueType,
	FBlueprintHelperSettingValue& OutValue);
```

Add private helpers:

```cpp
static TSharedPtr<FJsonObject> LoadProjectSettingObject(FString& OutError);
static bool SaveProjectSettingObject(const TSharedRef<FJsonObject>& Root, FString& OutError);
static bool SetJsonValueByDotPath(
	const TSharedRef<FJsonObject>& Root,
	const FString& Key,
	const FBlueprintHelperSettingUpdateRequest& Request,
	FString& OutError);
static bool TryGetJsonValueByDotPath(
	const TSharedPtr<FJsonObject>& Root,
	const FString& Key,
	EBlueprintHelperSettingValueType ValueType,
	FBlueprintHelperSettingValue& OutValue);
```

- [ ] **Step 2: Implement load/save helpers**

Add to `BlueprintHelperSettingStore.cpp`:

```cpp
TSharedPtr<FJsonObject> FBlueprintHelperSettingStore::LoadProjectSettingObject(FString& OutError)
{
	OutError.Reset();
	FString Path;
	if (!EnsureProjectSetting(Path, OutError))
	{
		return nullptr;
	}

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *Path))
	{
		OutError = FString::Printf(TEXT("无法读取设置文件: %s"), *Path);
		return nullptr;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = FString::Printf(TEXT("设置文件不是有效 JSON: %s"), *Path);
		return nullptr;
	}
	return Root;
}

bool FBlueprintHelperSettingStore::SaveProjectSettingObject(
	const TSharedRef<FJsonObject>& Root,
	FString& OutError)
{
	OutError.Reset();
	FString JsonText;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		OutError = TEXT("设置 JSON 序列化失败");
		return false;
	}

	const FString Path = FBlueprintHelperProjectConfigPaths::GetProjectSettingPath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (!FFileHelper::SaveStringToFile(JsonText, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("无法写入设置文件: %s"), *Path);
		return false;
	}
	return true;
}
```

- [ ] **Step 3: Implement dot-path set**

Add to `BlueprintHelperSettingStore.cpp`:

```cpp
bool FBlueprintHelperSettingStore::SetJsonValueByDotPath(
	const TSharedRef<FJsonObject>& Root,
	const FString& Key,
	const FBlueprintHelperSettingUpdateRequest& Request,
	FString& OutError)
{
	TArray<FString> Segments;
	Key.ParseIntoArray(Segments, TEXT("."), true);
	if (Segments.Num() == 0)
	{
		OutError = TEXT("设置 Key 为空");
		return false;
	}

	TSharedPtr<FJsonObject> Current = Root;
	for (int32 Index = 0; Index < Segments.Num() - 1; ++Index)
	{
		const FString& Segment = Segments[Index];
		const TSharedPtr<FJsonObject>* Child = nullptr;
		if (!Current->TryGetObjectField(Segment, Child) || !Child || !Child->IsValid())
		{
			TSharedPtr<FJsonObject> NewObject = MakeShared<FJsonObject>();
			Current->SetObjectField(Segment, NewObject);
			Current = NewObject;
		}
		else
		{
			Current = *Child;
		}
	}

	const FString& Leaf = Segments.Last();
	switch (Request.ValueType)
	{
	case EBlueprintHelperSettingValueType::Number:
		Current->SetNumberField(Leaf, Request.Value.NumberValue);
		return true;
	case EBlueprintHelperSettingValueType::Integer:
		Current->SetNumberField(Leaf, Request.Value.IntegerValue);
		return true;
	case EBlueprintHelperSettingValueType::Boolean:
		Current->SetBoolField(Leaf, Request.Value.bBoolValue);
		return true;
	case EBlueprintHelperSettingValueType::String:
		Current->SetStringField(Leaf, Request.Value.StringValue);
		return true;
	case EBlueprintHelperSettingValueType::Margin:
	case EBlueprintHelperSettingValueType::Vector2:
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		for (double Number : Request.Value.NumberArray)
		{
			Values.Add(MakeShared<FJsonValueNumber>(Number));
		}
		Current->SetArrayField(Leaf, Values);
		return true;
	}
	default:
		OutError = TEXT("不支持的设置值类型");
		return false;
	}
}
```

- [ ] **Step 4: Implement update method**

Add:

```cpp
FBlueprintHelperSettingUpdateResult FBlueprintHelperSettingStore::UpdateProjectSettingValue(
	const FBlueprintHelperSettingUpdateRequest& Request)
{
	FBlueprintHelperSettingUpdateResult Result;
	FString Error;
	TSharedPtr<FJsonObject> Root = LoadProjectSettingObject(Error);
	if (!Root.IsValid())
	{
		Result.ErrorText = Error;
		Result.StatusText = Error;
		return Result;
	}

	if (!SetJsonValueByDotPath(Root.ToSharedRef(), Request.Key, Request, Error))
	{
		Result.ErrorText = Error;
		Result.StatusText = Error;
		return Result;
	}

	if (!SaveProjectSettingObject(Root.ToSharedRef(), Error))
	{
		Result.ErrorText = Error;
		Result.StatusText = Error;
		return Result;
	}

	Result.bSucceeded = true;
	Result.StatusText = FString::Printf(TEXT("已保存设置: %s"), *Request.Key);
	return Result;
}
```

- [ ] **Step 5: Add read helper**

Implement `TryGetJsonValueByDotPath()` and `TryReadProjectSettingValue()`:

```cpp
bool FBlueprintHelperSettingStore::TryGetJsonValueByDotPath(
	const TSharedPtr<FJsonObject>& Root,
	const FString& Key,
	EBlueprintHelperSettingValueType ValueType,
	FBlueprintHelperSettingValue& OutValue)
{
	TArray<FString> Segments;
	Key.ParseIntoArray(Segments, TEXT("."), true);
	if (!Root.IsValid() || Segments.Num() == 0)
	{
		return false;
	}

	TSharedPtr<FJsonObject> Current = Root;
	for (int32 Index = 0; Index < Segments.Num() - 1; ++Index)
	{
		const TSharedPtr<FJsonObject>* Child = nullptr;
		if (!Current->TryGetObjectField(Segments[Index], Child) || !Child || !Child->IsValid())
		{
			return false;
		}
		Current = *Child;
	}

	const FString& Leaf = Segments.Last();
	if (ValueType == EBlueprintHelperSettingValueType::Boolean)
	{
		bool bValue = false;
		if (Current->TryGetBoolField(Leaf, bValue))
		{
			OutValue = FBlueprintHelperSettingValue::FromBoolean(bValue);
			return true;
		}
		return false;
	}

	if (ValueType == EBlueprintHelperSettingValueType::String)
	{
		FString Value;
		if (Current->TryGetStringField(Leaf, Value))
		{
			OutValue = FBlueprintHelperSettingValue::FromString(Value);
			return true;
		}
		return false;
	}

	if (ValueType == EBlueprintHelperSettingValueType::Number
		|| ValueType == EBlueprintHelperSettingValueType::Integer)
	{
		double Number = 0.0;
		if (Current->TryGetNumberField(Leaf, Number))
		{
			OutValue = ValueType == EBlueprintHelperSettingValueType::Integer
				? FBlueprintHelperSettingValue::FromInteger(static_cast<int32>(Number))
				: FBlueprintHelperSettingValue::FromNumber(Number);
			return true;
		}
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	if (Current->TryGetArrayField(Leaf, Array) && Array)
	{
		TArray<double> Numbers;
		for (const TSharedPtr<FJsonValue>& JsonValue : *Array)
		{
			Numbers.Add(JsonValue.IsValid() ? JsonValue->AsNumber() : 0.0);
		}
		OutValue = FBlueprintHelperSettingValue::FromNumberArray(Numbers);
		return true;
	}
	return false;
}

bool FBlueprintHelperSettingStore::TryReadProjectSettingValue(
	const FString& Key,
	EBlueprintHelperSettingValueType ValueType,
	FBlueprintHelperSettingValue& OutValue)
{
	FString Error;
	const TSharedPtr<FJsonObject> Root = LoadProjectSettingObject(Error);
	return TryGetJsonValueByDotPath(Root, Key, ValueType, OutValue);
}
```

- [ ] **Step 6: Add store write test**

Add a compact static behavior test in `BlueprintHelperSafetyTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSettingStoreDotPathUpdateTest,
	"BlueprintHelper.Settings.Store.DotPathUpdate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSettingStoreDotPathUpdateTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperSettingUpdateRequest Request;
	Request.Key = TEXT("ui.review_panel.diff_frame_outer_padding");
	Request.ValueType = EBlueprintHelperSettingValueType::Number;
	Request.Value = FBlueprintHelperSettingValue::FromNumber(3.0);

	const FBlueprintHelperSettingUpdateResult Result =
		FBlueprintHelperSettingStore::UpdateProjectSettingValue(Request);
	TestTrue(TEXT("setting update succeeds"), Result.bSucceeded);

	FBlueprintHelperSettingValue ReadBack;
	TestTrue(TEXT("setting can be read back"),
		FBlueprintHelperSettingStore::TryReadProjectSettingValue(
			Request.Key,
			Request.ValueType,
			ReadBack));
	TestEqual(TEXT("setting value matches"), ReadBack.NumberValue, 3.0);
	return true;
}
```

Add includes:

```cpp
#include "Systems/Config/BlueprintHelperSettingStore.h"
#include "Systems/Config/BlueprintHelperSettingTypes.h"
```

- [ ] **Step 7: Compile**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReloadFromIDE
```

Expected: `Result: Succeeded`.

### Task 3: Extend SettingsPresenter with row ViewModels and event-driven writeback

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Settings/BlueprintHelperSettingsPresenter.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Settings/BlueprintHelperSettingsPresenter.cpp`

- [ ] **Step 1: Update presenter header**

Add include:

```cpp
#include "Systems/Config/BlueprintHelperSettingTypes.h"
```

Add event type and methods:

```cpp
struct FBlueprintHelperSettingsPresenterEvent
{
	bool bViewChanged = false;
	bool bShowStatus = false;
	FString StatusText;
};

class BLUEPRINTHELPER_API FBlueprintHelperSettingsPresenter
{
public:
	using FPresenterEventSink = TFunction<void(const FBlueprintHelperSettingsPresenterEvent&)>;

	void SetEventSink(FPresenterEventSink InEventSink);
	const TArray<FBlueprintHelperSettingRowViewModel>& GetRows() const;
	FBlueprintHelperSettingUpdateResult HandleRowValueChanged(
		const FBlueprintHelperSettingUpdateRequest& Request);
	FBlueprintHelperSettingUpdateResult HandleResetRowToDefault(const FString& Key);
```

Keep existing `Reload()`, `EnsureProjectSetting()`, `GetView()`. Add private members:

```cpp
private:
	void RebuildRows();
	void EmitViewChanged(const FString& StatusText);
	static TArray<FBlueprintHelperSettingRowViewModel> MakeDefaultRowDefinitions();
	static bool ValidateRequest(
		const FBlueprintHelperSettingRowViewModel& Row,
		const FBlueprintHelperSettingUpdateRequest& Request,
		FString& OutError);

	TArray<FBlueprintHelperSettingRowViewModel> Rows;
	FPresenterEventSink EventSink;
```

- [ ] **Step 2: Add row definitions with Chinese text**

Implement in cpp:

```cpp
#define LOCTEXT_NAMESPACE "BlueprintHelperSettingsPresenter"

TArray<FBlueprintHelperSettingRowViewModel> FBlueprintHelperSettingsPresenter::MakeDefaultRowDefinitions()
{
	TArray<FBlueprintHelperSettingRowViewModel> Result;
	auto AddNumber = [&Result](
		const FString& Key,
		EBlueprintHelperSettingCategory Category,
		const TCHAR* DisplayName,
		const TCHAR* Hint,
		double MinValue,
		double MaxValue)
	{
		FBlueprintHelperSettingRowViewModel Row;
		Row.Key = Key;
		Row.Category = Category;
		Row.DisplayName = DisplayName;
		Row.HintText = Hint;
		Row.ValueType = EBlueprintHelperSettingValueType::Number;
		Row.MinNumber = MinValue;
		Row.MaxNumber = MaxValue;
		Result.Add(Row);
	};

	AddNumber(TEXT("ui.review_panel.diff_frame_outer_padding"),
		EBlueprintHelperSettingCategory::ReviewVisual,
		TEXT("Diff 框外边距"),
		TEXT("调整 Diff 框与内容之间的外边距，只影响 Review 可视化。"),
		0.0,
		20.0);
	AddNumber(TEXT("ui.review_panel.diff_action_padding"),
		EBlueprintHelperSettingCategory::ReviewVisual,
		TEXT("Diff 按钮内边距"),
		TEXT("调整 Accept/Reject 按钮区域的内边距。"),
		0.0,
		20.0);
	AddNumber(TEXT("ui.review_panel.surface_overlay_fill_alpha"),
		EBlueprintHelperSettingCategory::ReviewVisual,
		TEXT("Diff 覆盖透明度"),
		TEXT("调整未选中 Diff 覆盖层透明度。"),
		0.0,
		1.0);
	AddNumber(TEXT("ui.review_panel.surface_overlay_selected_fill_alpha"),
		EBlueprintHelperSettingCategory::ReviewVisual,
		TEXT("选中 Diff 覆盖透明度"),
		TEXT("调整选中 Diff 覆盖层透明度。"),
		0.0,
		1.0);

	FBlueprintHelperSettingRowViewModel DebugMessages;
	DebugMessages.Key = TEXT("ui.review_panel.debug_max_messages");
	DebugMessages.Category = EBlueprintHelperSettingCategory::ReviewDebug;
	DebugMessages.DisplayName = TEXT("Debug 最大消息数");
	DebugMessages.HintText = TEXT("控制 Review Debug 面板保留的消息数量。");
	DebugMessages.ValueType = EBlueprintHelperSettingValueType::Integer;
	DebugMessages.MinNumber = 20.0;
	DebugMessages.MaxNumber = 2000.0;
	Result.Add(DebugMessages);

	FBlueprintHelperSettingRowViewModel Retention;
	Retention.Key = TEXT("review.debug_bundle.retention");
	Retention.Category = EBlueprintHelperSettingCategory::ReviewDebug;
	Retention.DisplayName = TEXT("DebugBundle 保留策略");
	Retention.HintText = TEXT("控制 Review DebugBundle 的保留策略。");
	Retention.ValueType = EBlueprintHelperSettingValueType::String;
	Retention.EnumOptions = { TEXT("standard"), TEXT("keep_all"), TEXT("minimal") };
	Result.Add(Retention);

	FBlueprintHelperSettingRowViewModel FullSettings;
	FullSettings.Key = TEXT("debug.contains_full_settings");
	FullSettings.Category = EBlueprintHelperSettingCategory::ReviewDebug;
	FullSettings.DisplayName = TEXT("Debug 包含完整设置");
	FullSettings.HintText = TEXT("开启后 DebugBundle 可包含完整设置，可能暴露本地配置。");
	FullSettings.ValueType = EBlueprintHelperSettingValueType::Boolean;
	Result.Add(FullSettings);

	FBlueprintHelperSettingRowViewModel SignatureLimit;
	SignatureLimit.Key = TEXT("tool_clusters.signature.reference_context_max_results");
	SignatureLimit.Category = EBlueprintHelperSettingCategory::ReadContext;
	SignatureLimit.DisplayName = TEXT("签名引用最大结果数");
	SignatureLimit.HintText = TEXT("控制签名引用查询最多返回多少条。");
	SignatureLimit.ValueType = EBlueprintHelperSettingValueType::Integer;
	SignatureLimit.MinNumber = 1.0;
	SignatureLimit.MaxNumber = 500.0;
	Result.Add(SignatureLimit);

	FBlueprintHelperSettingRowViewModel MaxRows;
	MaxRows.Key = TEXT("tool_clusters.read_context.max_output_rows");
	MaxRows.Category = EBlueprintHelperSettingCategory::ReadContext;
	MaxRows.DisplayName = TEXT("ReadContext 最大行数");
	MaxRows.HintText = TEXT("控制读取上下文最多输出多少行，0 表示不限制。");
	MaxRows.ValueType = EBlueprintHelperSettingValueType::Integer;
	MaxRows.MinNumber = 0.0;
	MaxRows.MaxNumber = 10000.0;
	Result.Add(MaxRows);

	FBlueprintHelperSettingRowViewModel MaxBytes;
	MaxBytes.Key = TEXT("tool_clusters.read_context.max_output_bytes");
	MaxBytes.Category = EBlueprintHelperSettingCategory::ReadContext;
	MaxBytes.DisplayName = TEXT("ReadContext 最大字节数");
	MaxBytes.HintText = TEXT("控制读取上下文最大输出大小，0 表示不限制。");
	MaxBytes.ValueType = EBlueprintHelperSettingValueType::Integer;
	MaxBytes.MinNumber = 0.0;
	MaxBytes.MaxNumber = 10485760.0;
	Result.Add(MaxBytes);

	return Result;
}

#undef LOCTEXT_NAMESPACE
```

- [ ] **Step 3: Rebuild row values from Store**

Implement:

```cpp
void FBlueprintHelperSettingsPresenter::RebuildRows()
{
	Rows = MakeDefaultRowDefinitions();
	for (FBlueprintHelperSettingRowViewModel& Row : Rows)
	{
		FBlueprintHelperSettingValue Value;
		if (FBlueprintHelperSettingStore::TryReadProjectSettingValue(Row.Key, Row.ValueType, Value))
		{
			Row.ValueText = Value.StringValue;
		}
		else
		{
			Row.ValueText = TEXT("");
			Row.ErrorText = TEXT("未找到设置值");
		}
	}
}
```

Call `RebuildRows()` at the end of `Reload()` and `EnsureProjectSetting()`.

- [ ] **Step 4: Implement validation and writeback**

Add:

```cpp
bool FBlueprintHelperSettingsPresenter::ValidateRequest(
	const FBlueprintHelperSettingRowViewModel& Row,
	const FBlueprintHelperSettingUpdateRequest& Request,
	FString& OutError)
{
	OutError.Reset();
	if (Row.ValueType != Request.ValueType)
	{
		OutError = TEXT("设置类型不匹配");
		return false;
	}
	if (Request.ValueType == EBlueprintHelperSettingValueType::Number)
	{
		if (Request.Value.NumberValue < Row.MinNumber || Request.Value.NumberValue > Row.MaxNumber)
		{
			OutError = FString::Printf(TEXT("值必须在 %.2f 到 %.2f 之间"), Row.MinNumber, Row.MaxNumber);
			return false;
		}
	}
	if (Request.ValueType == EBlueprintHelperSettingValueType::Integer)
	{
		if (Request.Value.IntegerValue < static_cast<int32>(Row.MinNumber)
			|| Request.Value.IntegerValue > static_cast<int32>(Row.MaxNumber))
		{
			OutError = FString::Printf(TEXT("值必须在 %d 到 %d 之间"),
				static_cast<int32>(Row.MinNumber),
				static_cast<int32>(Row.MaxNumber));
			return false;
		}
	}
	if (Request.ValueType == EBlueprintHelperSettingValueType::String && Row.EnumOptions.Num() > 0)
	{
		if (!Row.EnumOptions.Contains(Request.Value.StringValue))
		{
			OutError = TEXT("不支持的选项值");
			return false;
		}
	}
	return true;
}
```

Add:

```cpp
FBlueprintHelperSettingUpdateResult FBlueprintHelperSettingsPresenter::HandleRowValueChanged(
	const FBlueprintHelperSettingUpdateRequest& Request)
{
	const FBlueprintHelperSettingRowViewModel* Row = Rows.FindByPredicate(
		[&Request](const FBlueprintHelperSettingRowViewModel& Candidate)
		{
			return Candidate.Key == Request.Key;
		});
	if (!Row)
	{
		FBlueprintHelperSettingUpdateResult Result;
		Result.ErrorText = TEXT("未知设置项");
		Result.StatusText = Result.ErrorText;
		return Result;
	}

	FString Error;
	if (!ValidateRequest(*Row, Request, Error))
	{
		FBlueprintHelperSettingUpdateResult Result;
		Result.ErrorText = Error;
		Result.StatusText = Error;
		return Result;
	}

	FBlueprintHelperSettingUpdateResult Result =
		FBlueprintHelperSettingStore::UpdateProjectSettingValue(Request);
	Reload();
	EmitViewChanged(Result.StatusText);
	return Result;
}
```

- [ ] **Step 5: Add event sink**

Add:

```cpp
void FBlueprintHelperSettingsPresenter::SetEventSink(FPresenterEventSink InEventSink)
{
	EventSink = MoveTemp(InEventSink);
}

const TArray<FBlueprintHelperSettingRowViewModel>& FBlueprintHelperSettingsPresenter::GetRows() const
{
	return Rows;
}

void FBlueprintHelperSettingsPresenter::EmitViewChanged(const FString& StatusText)
{
	if (EventSink)
	{
		FBlueprintHelperSettingsPresenterEvent Event;
		Event.bViewChanged = true;
		Event.bShowStatus = !StatusText.IsEmpty();
		Event.StatusText = StatusText;
		EventSink(Event);
	}
}
```

- [ ] **Step 6: Compile**

Run the UE build command. Expected: `Result: Succeeded`.

### Task 4: Implement user-friendly setting row widget with OverlapHint

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Settings/SBlueprintHelperSettingRow.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Settings/SBlueprintHelperSettingRow.cpp`

- [ ] **Step 1: Create row header**

Create:

```cpp
// BlueprintHelper single settings row.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Config/BlueprintHelperSettingTypes.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class SCheckBox;
class STextBlock;

DECLARE_DELEGATE_OneParam(FBlueprintHelperSettingRowValueCommitted, const FBlueprintHelperSettingUpdateRequest&);

class SBlueprintHelperSettingRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperSettingRow)
	{
	}
	SLATE_ARGUMENT(FBlueprintHelperSettingRowViewModel, Row)
	SLATE_EVENT(FBlueprintHelperSettingRowValueCommitted, OnValueCommitted)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void Refresh(const FBlueprintHelperSettingRowViewModel& InRow);

private:
	TSharedRef<SWidget> BuildValueWidget();
	void CommitTextValue(const FText& Text, ETextCommit::Type CommitType);
	void CommitBooleanValue(ECheckBoxState State);
	FText GetHintToolTipText() const;

	FBlueprintHelperSettingRowViewModel Row;
	FBlueprintHelperSettingRowValueCommitted OnValueCommitted;
	TSharedPtr<STextBlock> NameTextBlock;
	TSharedPtr<STextBlock> KeyTextBlock;
	TSharedPtr<STextBlock> ErrorTextBlock;
	TSharedPtr<SEditableTextBox> ValueTextBox;
	TSharedPtr<SCheckBox> ValueCheckBox;
};
```

- [ ] **Step 2: Implement row layout**

Create cpp:

```cpp
// BlueprintHelper single settings row implementation.

#include "UI/Settings/SBlueprintHelperSettingRow.h"

#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "BlueprintHelperSettingRow"

void SBlueprintHelperSettingRow::Construct(const FArguments& InArgs)
{
	Row = InArgs._Row;
	OnValueCommitted = InArgs._OnValueCommitted;

	ChildSlot
	[
		SNew(SBorder)
		.Padding(FMargin(8.0f, 5.0f))
		.ToolTipText(this, &SBlueprintHelperSettingRow::GetHintToolTipText)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.38f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(NameTextBlock, STextBlock)
					.Text(FText::FromString(Row.DisplayName))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(KeyTextBlock, STextBlock)
					.Text(FText::FromString(Row.Key))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.42f)
			[
				BuildValueWidget()
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.14f)
			.Padding(8.0f, 0.0f)
			[
				SAssignNew(ErrorTextBlock, STextBlock)
				.Text(FText::FromString(Row.ErrorText))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(28.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("OverlapHintMark", "说明"))
					.ToolTipText(this, &SBlueprintHelperSettingRow::GetHintToolTipText)
				]
			]
		]
	];
}
```

- [ ] **Step 3: Implement value widget**

Add:

```cpp
TSharedRef<SWidget> SBlueprintHelperSettingRow::BuildValueWidget()
{
	if (Row.ValueType == EBlueprintHelperSettingValueType::Boolean)
	{
		return SAssignNew(ValueCheckBox, SCheckBox)
			.IsChecked(Row.ValueText.Equals(TEXT("true"), ESearchCase::IgnoreCase)
				? ECheckBoxState::Checked
				: ECheckBoxState::Unchecked)
			.OnCheckStateChanged(this, &SBlueprintHelperSettingRow::CommitBooleanValue);
	}

	return SAssignNew(ValueTextBox, SEditableTextBox)
		.Text(FText::FromString(Row.ValueText))
		.OnTextCommitted(this, &SBlueprintHelperSettingRow::CommitTextValue);
}
```

- [ ] **Step 4: Implement commit conversion**

Add:

```cpp
void SBlueprintHelperSettingRow::CommitTextValue(const FText& Text, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnCleared || !OnValueCommitted.IsBound())
	{
		return;
	}

	const FString RawValue = Text.ToString().TrimStartAndEnd();
	FBlueprintHelperSettingUpdateRequest Request;
	Request.Key = Row.Key;
	Request.ValueType = Row.ValueType;

	if (Row.ValueType == EBlueprintHelperSettingValueType::Integer)
	{
		Request.Value = FBlueprintHelperSettingValue::FromInteger(FCString::Atoi(*RawValue));
	}
	else if (Row.ValueType == EBlueprintHelperSettingValueType::Number)
	{
		Request.Value = FBlueprintHelperSettingValue::FromNumber(FCString::Atod(*RawValue));
	}
	else if (Row.ValueType == EBlueprintHelperSettingValueType::Margin
		|| Row.ValueType == EBlueprintHelperSettingValueType::Vector2)
	{
		TArray<FString> Parts;
		RawValue.ParseIntoArray(Parts, TEXT(","), true);
		TArray<double> Numbers;
		for (const FString& Part : Parts)
		{
			Numbers.Add(FCString::Atod(*Part.TrimStartAndEnd()));
		}
		Request.Value = FBlueprintHelperSettingValue::FromNumberArray(Numbers);
	}
	else
	{
		Request.Value = FBlueprintHelperSettingValue::FromString(RawValue);
	}

	OnValueCommitted.Execute(Request);
}

void SBlueprintHelperSettingRow::CommitBooleanValue(ECheckBoxState State)
{
	if (!OnValueCommitted.IsBound())
	{
		return;
	}

	FBlueprintHelperSettingUpdateRequest Request;
	Request.Key = Row.Key;
	Request.ValueType = Row.ValueType;
	Request.Value = FBlueprintHelperSettingValue::FromBoolean(State == ECheckBoxState::Checked);
	OnValueCommitted.Execute(Request);
}
```

- [ ] **Step 5: Implement refresh and hint**

Add:

```cpp
void SBlueprintHelperSettingRow::Refresh(const FBlueprintHelperSettingRowViewModel& InRow)
{
	Row = InRow;
	if (NameTextBlock.IsValid())
	{
		NameTextBlock->SetText(FText::FromString(Row.DisplayName));
	}
	if (KeyTextBlock.IsValid())
	{
		KeyTextBlock->SetText(FText::FromString(Row.Key));
	}
	if (ErrorTextBlock.IsValid())
	{
		ErrorTextBlock->SetText(FText::FromString(Row.ErrorText));
	}
	if (ValueTextBox.IsValid())
	{
		ValueTextBox->SetText(FText::FromString(Row.ValueText));
	}
	if (ValueCheckBox.IsValid())
	{
		ValueCheckBox->SetIsChecked(Row.ValueText.Equals(TEXT("true"), ESearchCase::IgnoreCase)
			? ECheckBoxState::Checked
			: ECheckBoxState::Unchecked);
	}
}

FText SBlueprintHelperSettingRow::GetHintToolTipText() const
{
	return FText::FromString(Row.HintText);
}

#undef LOCTEXT_NAMESPACE
```

- [ ] **Step 6: Compile**

Run the UE build command. Expected: `Result: Succeeded`.

### Task 5: Replace SettingsPanel JSON view with categorized row UI

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Settings/SBlueprintHelperSettingsPanel.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Settings/SBlueprintHelperSettingsPanel.cpp`

- [ ] **Step 1: Update panel header**

Add forward declarations:

```cpp
class SBlueprintHelperSettingRow;
class SVerticalBox;
struct FBlueprintHelperSettingsPresenterEvent;
```

Add methods and fields:

```cpp
void HandlePresenterEvent(const FBlueprintHelperSettingsPresenterEvent& Event);
void RebuildRows();
void AddCategoryRows(
	const FText& CategoryTitle,
	EBlueprintHelperSettingCategory Category,
	TSharedRef<SVerticalBox> TargetBox);
void HandleRowCommitted(const FBlueprintHelperSettingUpdateRequest& Request);

TSharedPtr<SVerticalBox> RowsBox;
TMap<FString, TSharedPtr<SBlueprintHelperSettingRow>> RowWidgetsByKey;
```

- [ ] **Step 2: Set Presenter event sink**

In `Construct()` after Presenter creation:

```cpp
Presenter->SetEventSink([this](const FBlueprintHelperSettingsPresenterEvent& Event)
{
	HandlePresenterEvent(Event);
});
```

- [ ] **Step 3: Replace main body with rows**

Replace the JSON-first body with:

```cpp
SAssignNew(RowsBox, SVerticalBox)
```

inside a `SScrollBox`, and keep JSON text box below under a `Debug JSON` label:

```cpp
+ SVerticalBox::Slot()
.FillHeight(0.72f)
[
	SNew(SScrollBox)
	+ SScrollBox::Slot()
	[
		SAssignNew(RowsBox, SVerticalBox)
	]
]
+ SVerticalBox::Slot()
.FillHeight(0.28f)
.Padding(0.0f, 8.0f, 0.0f, 0.0f)
[
	SNew(SVerticalBox)
	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("调试 JSON（只读）")))
	]
	+ SVerticalBox::Slot()
	.FillHeight(1.0f)
	[
		SAssignNew(SettingJsonTextBox, SMultiLineEditableTextBox)
		.IsReadOnly(true)
	]
]
```

- [ ] **Step 4: Implement RebuildRows**

Add:

```cpp
void SBlueprintHelperSettingsPanel::RebuildRows()
{
	if (!RowsBox.IsValid() || !Presenter.IsValid())
	{
		return;
	}

	RowsBox->ClearChildren();
	RowWidgetsByKey.Reset();

	AddCategoryRows(FText::FromString(TEXT("Review 可视化")), EBlueprintHelperSettingCategory::ReviewVisual, RowsBox.ToSharedRef());
	AddCategoryRows(FText::FromString(TEXT("Review Debug")), EBlueprintHelperSettingCategory::ReviewDebug, RowsBox.ToSharedRef());
	AddCategoryRows(FText::FromString(TEXT("Read Context 裁切")), EBlueprintHelperSettingCategory::ReadContext, RowsBox.ToSharedRef());
}
```

- [ ] **Step 5: Implement AddCategoryRows**

Add:

```cpp
void SBlueprintHelperSettingsPanel::AddCategoryRows(
	const FText& CategoryTitle,
	EBlueprintHelperSettingCategory Category,
	TSharedRef<SVerticalBox> TargetBox)
{
	TargetBox->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 10.0f, 0.0f, 4.0f)
	[
		SNew(STextBlock)
		.Text(CategoryTitle)
	];

	for (const FBlueprintHelperSettingRowViewModel& Row : Presenter->GetRows())
	{
		if (Row.Category != Category || !Row.bUserEditable)
		{
			continue;
		}

		TSharedPtr<SBlueprintHelperSettingRow> RowWidget;
		TargetBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 2.0f)
		[
			SAssignNew(RowWidget, SBlueprintHelperSettingRow)
			.Row(Row)
			.OnValueCommitted(FBlueprintHelperSettingRowValueCommitted::CreateSP(
				this,
				&SBlueprintHelperSettingsPanel::HandleRowCommitted))
		];
		RowWidgetsByKey.Add(Row.Key, RowWidget);
	}
}
```

- [ ] **Step 6: Implement event handlers**

Add:

```cpp
void SBlueprintHelperSettingsPanel::HandlePresenterEvent(
	const FBlueprintHelperSettingsPresenterEvent& Event)
{
	if (Event.bViewChanged)
	{
		RefreshView();
		RebuildRows();
	}
	if (Event.bShowStatus && StatusTextBlock.IsValid())
	{
		StatusTextBlock->SetText(FText::FromString(Event.StatusText));
	}
}

void SBlueprintHelperSettingsPanel::HandleRowCommitted(
	const FBlueprintHelperSettingUpdateRequest& Request)
{
	if (!Presenter.IsValid())
	{
		return;
	}

	const FBlueprintHelperSettingUpdateResult Result =
		Presenter->HandleRowValueChanged(Request);
	if (!Result.bSucceeded && StatusTextBlock.IsValid())
	{
		StatusTextBlock->SetText(FText::FromString(Result.ErrorText));
	}
}
```

- [ ] **Step 7: Call RebuildRows in RefreshView**

At the end of `RefreshView()`:

```cpp
RebuildRows();
```

Guard against recursion by only calling `RebuildRows()` from `RefreshView()` and not from row refresh loops.

- [ ] **Step 8: Compile**

Run the UE build command. Expected: `Result: Succeeded`.

### Task 6: Add reset-to-default behavior

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Settings/SBlueprintHelperSettingRow.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Settings/SBlueprintHelperSettingRow.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Settings/BlueprintHelperSettingsPresenter.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Settings/BlueprintHelperSettingsPresenter.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Settings/SBlueprintHelperSettingsPanel.cpp`

- [ ] **Step 1: Add reset delegate**

In row header:

```cpp
DECLARE_DELEGATE_OneParam(FBlueprintHelperSettingRowResetRequested, const FString&);
```

Add `SLATE_EVENT(FBlueprintHelperSettingRowResetRequested, OnResetRequested)` and a field:

```cpp
FBlueprintHelperSettingRowResetRequested OnResetRequested;
```

- [ ] **Step 2: Add reset button**

Add a row slot before hint:

```cpp
+ SHorizontalBox::Slot()
.AutoWidth()
.Padding(6.0f, 0.0f)
[
	SNew(SButton)
	.Text(LOCTEXT("ResetButton", "重置"))
	.ToolTipText(LOCTEXT("ResetButtonHint", "恢复这个设置项的默认值"))
	.OnClicked_Lambda([this]()
	{
		if (OnResetRequested.IsBound())
		{
			OnResetRequested.Execute(Row.Key);
		}
		return FReply::Handled();
	})
]
```

- [ ] **Step 3: Presenter reset implementation**

Implement `HandleResetRowToDefault()` by reading the same key from default setting file, then saving it to project setting:

```cpp
FBlueprintHelperSettingUpdateResult FBlueprintHelperSettingsPresenter::HandleResetRowToDefault(
	const FString& Key)
{
	const FBlueprintHelperSettingRowViewModel* Row = Rows.FindByPredicate(
		[&Key](const FBlueprintHelperSettingRowViewModel& Candidate)
		{
			return Candidate.Key == Key;
		});
	if (!Row)
	{
		FBlueprintHelperSettingUpdateResult Result;
		Result.ErrorText = TEXT("未知设置项");
		Result.StatusText = Result.ErrorText;
		return Result;
	}

	FBlueprintHelperSettingValue DefaultValue;
	if (!FBlueprintHelperSettingStore::TryReadDefaultSettingValue(Key, Row->ValueType, DefaultValue))
	{
		FBlueprintHelperSettingUpdateResult Result;
		Result.ErrorText = TEXT("默认设置中未找到该项");
		Result.StatusText = Result.ErrorText;
		return Result;
	}

	FBlueprintHelperSettingUpdateRequest Request;
	Request.Key = Key;
	Request.ValueType = Row->ValueType;
	Request.Value = DefaultValue;
	return HandleRowValueChanged(Request);
}
```

If `TryReadDefaultSettingValue()` does not exist yet, add it to Store using the same logic as project read but loading `GetDefaultSettingPath()`.

- [ ] **Step 4: Wire panel reset event**

In `AddCategoryRows()`:

```cpp
.OnResetRequested(FBlueprintHelperSettingRowResetRequested::CreateSP(
	this,
	&SBlueprintHelperSettingsPanel::HandleRowResetRequested))
```

Add:

```cpp
void SBlueprintHelperSettingsPanel::HandleRowResetRequested(const FString& Key)
{
	if (!Presenter.IsValid())
	{
		return;
	}
	Presenter->HandleResetRowToDefault(Key);
}
```

- [ ] **Step 5: Compile**

Run the UE build command. Expected: `Result: Succeeded`.

### Task 7: Settings visibility safety check

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/RuntimeDiagnostics/BlueprintHelperSafetyTests.cpp`

- [ ] **Step 1: Add static visibility test**

Add a test that loads project `setting.json` and verifies no BlueprintHelper outer-shell UI is in `settings_visibility.user_editable`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSettingsVisibilityExcludesShellUiTest,
	"BlueprintHelper.Settings.Visibility.ExcludesShellUi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSettingsVisibilityExcludesShellUiTest::RunTest(const FString& Parameters)
{
	FString JsonText;
	TestTrue(TEXT("project setting exists"),
		FFileHelper::LoadFileToString(
			JsonText,
			*FBlueprintHelperProjectConfigPaths::GetProjectSettingPath()));

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	TestTrue(TEXT("project setting parses"),
		FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());

	const TSharedPtr<FJsonObject>* Visibility = nullptr;
	TestTrue(TEXT("visibility object exists"),
		Root->TryGetObjectField(TEXT("settings_visibility"), Visibility) && Visibility && Visibility->IsValid());

	const TArray<TSharedPtr<FJsonValue>>* UserEditable = nullptr;
	TestTrue(TEXT("user_editable exists"),
		(*Visibility)->TryGetArrayField(TEXT("user_editable"), UserEditable) && UserEditable);

	for (const TSharedPtr<FJsonValue>& Value : *UserEditable)
	{
		const FString Key = Value.IsValid() ? Value->AsString() : FString();
		TestFalse(TEXT("main window shell is not user editable"), Key.StartsWith(TEXT("ui.main_window")));
		TestFalse(TEXT("notifications are not user editable"), Key.StartsWith(TEXT("ui.notifications")));
		TestFalse(TEXT("task spec workbench is not user editable"), Key.StartsWith(TEXT("ui.task_spec_workbench")));
		TestFalse(TEXT("layout editor is not user editable"), Key.StartsWith(TEXT("ui.layout_rule_editor")));
	}
	return true;
}
```

Add includes if missing:

```cpp
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Systems/Config/BlueprintHelperProjectConfigPaths.h"
```

- [ ] **Step 2: Compile**

Run the UE build command. Expected: `Result: Succeeded`.

### Task 8: Final validation and documentation update

**Files:**
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_SettingsPanel_UserRows_ImplementationPlan_20260520_CN.md`

- [ ] **Step 1: Run compile**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReloadFromIDE
```

Expected: `Result: Succeeded`.

- [ ] **Step 2: Update this plan status**

Append:

```markdown
## 2026-05-20 Execution Status

- [x] Settings row data model implemented.
- [x] SettingStore dot-path update and save implemented.
- [x] SettingsPresenter event-driven row writeback implemented.
- [x] User-friendly Chinese setting rows implemented.
- [x] OverlapHint tooltip implemented.
- [x] Project `setting.json` writeback implemented.
- [x] Compile passed.

Distance from expectation:

1. Settings values are written back to JSON, but existing systems do not yet consume every setting at runtime.
2. Developer-only keys remain in JSON for manual developer tuning but are not normal user-editable rows.
```

- [ ] **Step 3: Provide manual commit message**

Do not execute git automatically. Use:

```text
新增内容：
1. 添加中文用户友好 Settings 行和事件驱动写回
2. 添加 SettingStore dot-path 更新能力
```

## 4. Acceptance Criteria

- Setting 页显示中文用户友好行，而不是只显示整段 JSON。
- 每个可编辑行有中文名称、值控件、重置按钮、OverlapHint。
- 鼠标覆盖 hint 能显示中文说明。
- 用户修改值后由 Presenter 调用 Store 写回 `.blueprinthelper/setting.json`。
- 写回后 Presenter 广播 ViewChanged，Panel 通过事件刷新，不使用 timer/retry/delay。
- BlueprintHelper 本身外壳 UI 配置不显示为用户可编辑行。
- UE 5.6 编译通过。

## 5. Execution Notes

- 不要让 `SBlueprintHelperSettingsPanel` 或 `SBlueprintHelperSettingRow` 直接调用 `FFileHelper::SaveStringToFile`。
- 不要让 Row 持有 `FBlueprintHelperSettingStore`。
- 不要开放 `ui.main_window`、`ui.notifications`、`ui.layout_rule_editor`、`ui.task_spec_workbench` 给普通用户编辑。
- 不要新增 ActiveTimer、延迟刷新或轮询。
- 所有新增 C++ 类保持独立 `.h/.cpp`。
