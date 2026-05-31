#include "UI/Settings/SBlueprintHelperSettingRow.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "BlueprintHelperSettingRow"

void SBlueprintHelperSettingRow::Construct(const FArguments& InArgs)
{
	Row = InArgs._Row;
	OnValueCommitted = InArgs._OnValueCommitted;
	OnResetRequested = InArgs._OnResetRequested;

	ChoiceItems.Reset();
	for (const FBlueprintHelperSettingChoiceViewModel& Choice : Row.Choices)
	{
		ChoiceItems.Add(MakeShared<FBlueprintHelperSettingChoiceViewModel>(Choice));
	}

	ChildSlot
	[
		SNew(SBorder)
		.Padding(6.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.38f)
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(Row.DisplayLabel)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(Row.DotPath))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.52f, 0.52f, 0.52f, 1.0f)))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(Row.AccessStatusText + TEXT(" | ") + Row.ConsumerStatusText))
						.ColorAndOpacity(FSlateColor(Row.bRuntimeConsumed
							? FLinearColor(0.42f, 0.72f, 0.50f, 1.0f)
							: FLinearColor(0.90f, 0.65f, 0.30f, 1.0f)))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.42f)
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					BuildValueWidget()
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("OverlapHintLabel", "说明"))
					.ToolTipText(Row.OverlapHint)
					.ColorAndOpacity(FSlateColor(FLinearColor(0.70f, 0.78f, 0.90f, 1.0f)))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.Text(LOCTEXT("ResetButton", "重置"))
					.IsEnabled(Row.bEnabled && Row.bModified)
					.ToolTipText(LOCTEXT("ResetButtonHint", "移除项目覆盖值，恢复默认设置。"))
					.OnClicked(this, &SBlueprintHelperSettingRow::HandleResetClicked)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Row.ErrorText))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.25f, 0.20f, 1.0f)))
				.Visibility(Row.ErrorText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
			]
		]
	];
}

TSharedRef<SWidget> SBlueprintHelperSettingRow::BuildValueWidget()
{
	switch (Row.ValueType)
	{
	case EBlueprintHelperSettingValueType::Number:
		return BuildNumberWidget();
	case EBlueprintHelperSettingValueType::Integer:
		return BuildIntegerWidget();
	case EBlueprintHelperSettingValueType::Boolean:
		return BuildBooleanWidget();
	case EBlueprintHelperSettingValueType::Choice:
		return BuildChoiceWidget();
	case EBlueprintHelperSettingValueType::Vector2:
		return BuildTextValueWidget(LOCTEXT("Vector2FormatHint", "格式：X,Y"));
	case EBlueprintHelperSettingValueType::Margin:
		return BuildTextValueWidget(LOCTEXT("MarginFormatHint", "格式：左,上,右,下"));
	case EBlueprintHelperSettingValueType::ColorArray:
		return BuildTextValueWidget(LOCTEXT("ColorArrayFormatHint", "格式：[R,G,B,A]"));
	case EBlueprintHelperSettingValueType::String:
	default:
		return BuildStringWidget();
	}
}

TSharedRef<SWidget> SBlueprintHelperSettingRow::BuildNumberWidget()
{
	return SNew(SNumericEntryBox<double>)
		.IsEnabled(Row.bEnabled)
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
		});
}

TSharedRef<SWidget> SBlueprintHelperSettingRow::BuildIntegerWidget()
{
	return SNew(SNumericEntryBox<int32>)
		.IsEnabled(Row.bEnabled)
		.Value_Lambda([this]() -> TOptional<int32>
		{
			int32 Parsed = 0;
			return LexTryParseString(Parsed, *Row.CurrentValue) ? TOptional<int32>(Parsed) : TOptional<int32>();
		})
		.MinValue(Row.bHasMinValue ? TOptional<int32>(static_cast<int32>(Row.MinValue)) : TOptional<int32>())
		.MaxValue(Row.bHasMaxValue ? TOptional<int32>(static_cast<int32>(Row.MaxValue)) : TOptional<int32>())
		.OnValueCommitted_Lambda([this](int32 NewValue, ETextCommit::Type)
		{
			CommitValue(LexToString(NewValue));
		});
}

TSharedRef<SWidget> SBlueprintHelperSettingRow::BuildBooleanWidget()
{
	return SNew(SCheckBox)
		.IsEnabled(Row.bEnabled)
		.IsChecked_Lambda([this]()
		{
			return Row.CurrentValue.Equals(TEXT("true"), ESearchCase::IgnoreCase) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
		{
			CommitValue(NewState == ECheckBoxState::Checked ? TEXT("true") : TEXT("false"));
		});
}

TSharedRef<SWidget> SBlueprintHelperSettingRow::BuildChoiceWidget()
{
	return SNew(SComboBox<TSharedPtr<FBlueprintHelperSettingChoiceViewModel>>)
		.IsEnabled(Row.bEnabled)
		.OptionsSource(&ChoiceItems)
		.InitiallySelectedItem(FindCurrentChoice())
		.OnGenerateWidget_Lambda([](TSharedPtr<FBlueprintHelperSettingChoiceViewModel> Choice)
		{
			return SNew(STextBlock)
				.Text(Choice.IsValid() ? Choice->Label : FText::GetEmpty());
		})
		.OnSelectionChanged_Lambda([this](TSharedPtr<FBlueprintHelperSettingChoiceViewModel> Choice, ESelectInfo::Type)
		{
			if (Choice.IsValid())
			{
				CommitValue(Choice->Value);
			}
		})
		[
			SNew(STextBlock)
			.Text_Lambda([this]()
			{
				const TSharedPtr<FBlueprintHelperSettingChoiceViewModel> Choice = FindCurrentChoice();
				return Choice.IsValid() ? Choice->Label : FText::FromString(Row.CurrentValue);
			})
		];
}

TSharedRef<SWidget> SBlueprintHelperSettingRow::BuildStringWidget()
{
	return BuildTextValueWidget(FText::GetEmpty());
}

TSharedRef<SWidget> SBlueprintHelperSettingRow::BuildTextValueWidget(const FText& HintText)
{
	return SNew(SEditableTextBox)
		.IsEnabled(Row.bEnabled)
		.Text(FText::FromString(Row.CurrentValue))
		.HintText(HintText)
		.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type)
		{
			CommitValue(NewText.ToString());
		});
}

void SBlueprintHelperSettingRow::CommitValue(const FString& NewValue) const
{
	if (OnValueCommitted.IsBound())
	{
		FBlueprintHelperSettingEditEvent Event;
		Event.DotPath = Row.DotPath;
		Event.NewValue = NewValue;
		OnValueCommitted.Execute(Event);
	}
}

FReply SBlueprintHelperSettingRow::HandleResetClicked() const
{
	if (OnResetRequested.IsBound())
	{
		OnResetRequested.Execute(Row.DotPath);
	}
	return FReply::Handled();
}

TSharedPtr<FBlueprintHelperSettingChoiceViewModel> SBlueprintHelperSettingRow::FindCurrentChoice() const
{
	for (const TSharedPtr<FBlueprintHelperSettingChoiceViewModel>& Choice : ChoiceItems)
	{
		if (Choice.IsValid() && Choice->Value == Row.CurrentValue)
		{
			return Choice;
		}
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
