// This Project Is Made By Fabric


#include "SHelperMainWidget.h"

#include "BlueprintHelper.h"
#include "BlueprintTextConverter.h"
#include "Services/BlueprintHelperGraphResolver.h"
#include "Services/BlueprintHelperImportService.h"
#include "HAL/PlatformApplicationMisc.h"
#include "ScopedTransaction.h"
#include "SlateOptMacros.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SHelperMainWidget::Construct(const FArguments& InArgs)
{
	ImportService = InArgs._ImportService;
	GraphResolverPtr = InArgs._GraphResolver;

	// 初始化所有函数库
	FunctionSource = TextToBlueprintGenerator::GetAllBlueprintFunctions();
	FilteredFunctionSource = FunctionSource;

	ChildSlot
	[
		SNew(SVerticalBox)
		// 顶部操作区
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("从蓝图文本/剪贴板转换为JSON")))
				.ToolTipText(FText::FromString(TEXT("优先读取主文本区中的 Blueprint T3D 文本；若主文本区为空或不是 T3D，则读取当前剪贴板，并转换为插件可回放 JSON。")))
				.OnClicked(this, &SHelperMainWidget::OnParseClipboardClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("复制JSON")))
				.ToolTipText(FText::FromString(TEXT("将主文本区内容复制回剪贴板。")))
				.OnClicked(this, &SHelperMainWidget::OnCopyResultClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("复制JSON规则")))
				.ToolTipText(FText::FromString(TEXT("将 Json -> 蓝图规则 Markdown 复制到剪贴板，方便外部大模型按插件协议输出 JSON。")))
				.OnClicked(this, &SHelperMainWidget::OnCopyJsonRuleClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("从JSON生成蓝图")))
				.ToolTipText(FText::FromString(TEXT("读取主文本区 JSON 内容并生成蓝图节点到当前激活图表。")))
				.OnClicked(this, &SHelperMainWidget::OnGenerateFromTextClicked)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(SSeparator)
		]
		// 中间主区域
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(8.0f, 0.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			+ SSplitter::Slot()
			.Value(0.3f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("未匹配节点")))
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SAssignNew(UnresolvedListView, SListView<TSharedPtr<FUnresolvedNodeItem>>)
					.ListItemsSource(&UnresolvedSource)
					.OnGenerateRow(this, &SHelperMainWidget::GenerateUnresolvedRow)
					.OnSelectionChanged(this, &SHelperMainWidget::OnUnresolvedSelectionChanged)
				]
			]
			+ SSplitter::Slot()
			.Value(0.7f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(0.6f)
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SAssignNew(MainTextBox, SMultiLineEditableTextBox)
					.HintText(FText::FromString(TEXT("这里用于粘贴 Blueprint T3D 文本，或显示/编辑插件约束 JSON。\n建议链路：复制蓝图节点 -> 点击“从蓝图文本/剪贴板转换为JSON” -> 必要时交给外部模型加工 -> 点击“从JSON生成蓝图”。")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					SAssignNew(FunctionSearchBox, SSearchBox)
					.HintText(FText::FromString(TEXT("搜索引擎函数...")))
					.OnTextChanged(this, &SHelperMainWidget::OnSearchTextChanged)
				]
				+ SVerticalBox::Slot()
				.FillHeight(0.4f)
				[
					SAssignNew(FunctionListView, SListView<TSharedPtr<FEngineFunctionItem>>)
					.ListItemsSource(&FilteredFunctionSource)
					.OnGenerateRow(this, &SHelperMainWidget::GenerateFunctionRow)
					.OnMouseButtonDoubleClick(this, &SHelperMainWidget::OnFunctionDoubleClicked)
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(SSeparator)
		]

		// 底部状态栏
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f)
		[
			SAssignNew(StatusTextBlock, STextBlock)
			.Text(FText::FromString(TEXT("就绪。")))
		]
	];
}

void SHelperMainWidget::OnSearchTextChanged(const FText& InFilterText)
{
	const FString FilterString = InFilterText.ToString().TrimStartAndEnd();
	FilteredFunctionSource.Empty();

	for (const TSharedPtr<FEngineFunctionItem>& Item : FunctionSource)
	{
		if (!Item.IsValid())
		{
			continue;
		}

		const bool bMatched = FilterString.IsEmpty()
			|| Item->FunctionName.Contains(FilterString, ESearchCase::IgnoreCase)
			|| Item->NativeFunctionName.Contains(FilterString, ESearchCase::IgnoreCase)
			|| Item->Category.Contains(FilterString, ESearchCase::IgnoreCase);
		if (bMatched)
		{
			FilteredFunctionSource.Add(Item);
		}
	}

	if (FunctionListView.IsValid())
	{
		FunctionListView->RequestListRefresh();
	}
}

void SHelperMainWidget::OnFunctionDoubleClicked(TSharedPtr<FEngineFunctionItem> Item)
{
	if (!Item.IsValid() || !Item->FunctionPtr)
	{
		SetStatusMessage(TEXT("请选择有效的引擎函数进行映射。"));
		return;
	}

	GenerateMappedNodeForCurrentSelection(Item->FunctionPtr);
}

TSharedRef<ITableRow> SHelperMainWidget::GenerateUnresolvedRow(TSharedPtr<FUnresolvedNodeItem> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	const FString DisplayText = Item.IsValid() ? Item->DisplayText : TEXT("Invalid Unresolved Node");
	return SNew(STableRow<TSharedPtr<FUnresolvedNodeItem>>, OwnerTable)
	[
		SNew(STextBlock)
		.Text(FText::FromString(DisplayText))
	];
}

TSharedRef<ITableRow> SHelperMainWidget::GenerateFunctionRow(TSharedPtr<FEngineFunctionItem> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	const FString DisplayText = Item.IsValid()
		? FString::Printf(TEXT("%s [%s]"), *Item->FunctionName, *Item->Category)
		: TEXT("Invalid Function");

	return SNew(STableRow<TSharedPtr<FEngineFunctionItem>>, OwnerTable)
	[
		SNew(STextBlock)
		.Text(FText::FromString(DisplayText))
	];
}

void SHelperMainWidget::OnUnresolvedSelectionChanged(TSharedPtr<FUnresolvedNodeItem> Item, ESelectInfo::Type SelectInfo)
{
	CurrentSelectedUnresolved = Item;
	if (!Item.IsValid())
	{
		return;
	}

	if (FunctionSearchBox.IsValid())
	{
		FunctionSearchBox->SetText(FText::FromString(Item->NodeData.FunctionName));
	}

	OnSearchTextChanged(FText::FromString(Item->NodeData.FunctionName));
	SetStatusMessage(FString::Printf(TEXT("已选择未匹配节点：%s"), *Item->NodeData.FunctionName));
}

FReply SHelperMainWidget::OnParseClipboardClicked()
{
	const FString BlueprintSourceText = ResolveBlueprintSourceText();
	if (!FBlueprintToTextConverter::IsBlueprintT3DText(BlueprintSourceText))
	{
		SetStatusMessage(TEXT("主文本区与剪贴板中都未找到有效 Blueprint T3D 文本，请先在蓝图中复制节点，或将复制内容粘贴到主文本区。"));
		return FReply::Handled();
	}

	const FString JsonText = FBlueprintToTextConverter::ConvertTextToJson(BlueprintSourceText);
	if (JsonText.IsEmpty())
	{
		SetStatusMessage(TEXT("Blueprint T3D 转换 JSON 失败，请检查输入内容是否完整。"));
		return FReply::Handled();
	}

	SetMainText(JsonText);
	SetStatusMessage(TEXT("已将 Blueprint 文本转换为插件可回放 JSON。"));
	return FReply::Handled();
}

FReply SHelperMainWidget::OnCopyResultClicked()
{
	const FString CurrentText = GetMainText();
	FPlatformApplicationMisc::ClipboardCopy(*CurrentText);
	SetStatusMessage(TEXT("已复制主文本区内容到剪贴板。"));
	return FReply::Handled();
}

FReply SHelperMainWidget::OnCopyJsonRuleClicked()
{
	const FString RuleMarkdown = FBlueprintHelperModule::Get().GetJsonToBlueprintRuleMarkdown();
	if (RuleMarkdown.IsEmpty())
	{
		SetStatusMessage(TEXT("未找到 Json -> 蓝图规则 Markdown，请检查插件 Resources 目录。"));
		return FReply::Handled();
	}

	FPlatformApplicationMisc::ClipboardCopy(*RuleMarkdown);
	SetStatusMessage(TEXT("已复制 Json -> 蓝图规则 Markdown，可直接提供给外部大模型。"));
	return FReply::Handled();
}

FReply SHelperMainWidget::OnGenerateFromTextClicked()
{
	UEdGraph* TargetGraph = GetCurrentTargetGraph();
	if (!TargetGraph)
	{
		SetStatusMessage(TEXT("未找到当前激活的蓝图图表，请先聚焦一个 Blueprint Graph。"));
		return FReply::Handled();
	}

	const FString JsonText = GetMainText().TrimStartAndEnd();
	if (JsonText.IsEmpty())
	{
		SetStatusMessage(TEXT("主文本区为空，请先输入或粘贴 JSON。"));
		return FReply::Handled();
	}

	if (!JsonText.TrimStartAndEnd().StartsWith(TEXT("{")))
	{
		SetStatusMessage(TEXT("当前主文本区不是 JSON，请先点击“从蓝图文本/剪贴板转换为JSON”，或粘贴符合规则的 JSON。"));
		return FReply::Handled();
	}

	const FBlueprintGenerateResult GenerateResult = TextToBlueprintGenerator::GenerateBlueprintFromJson(TargetGraph, JsonText, UnresolvedSource);
	RefreshUnresolvedList();
	SetStatusMessage(GenerateResult.Message);
	return FReply::Handled();
}

FString SHelperMainWidget::GetMainText() const
{
	return MainTextBox.IsValid() ? MainTextBox->GetText().ToString() : TEXT("");
}

void SHelperMainWidget::SetMainText(const FString& InText) const
{
	if (MainTextBox.IsValid())
	{
		MainTextBox->SetText(FText::FromString(InText));
	}
}

void SHelperMainWidget::SetStatusMessage(const FString& InMessage) const
{
	if (StatusTextBlock.IsValid())
	{
		StatusTextBlock->SetText(FText::FromString(InMessage));
	}
}

FString SHelperMainWidget::ResolveBlueprintSourceText() const
{
	const FString CurrentText = GetMainText();
	if (FBlueprintToTextConverter::IsBlueprintT3DText(CurrentText))
	{
		return CurrentText;
	}

	FString ClipboardText;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardText);
	return ClipboardText;
}

UEdGraph* SHelperMainWidget::GetCurrentTargetGraph() const
{
	if (GraphResolverPtr)
	{
		return GraphResolverPtr->GetFocusedGraph();
	}
	return FBlueprintHelperModule::Get().GetActiveBlueprintGraph();
}

void SHelperMainWidget::RefreshUnresolvedList() const
{
	if (UnresolvedListView.IsValid())
	{
		UnresolvedListView->RequestListRefresh();
	}
}

void SHelperMainWidget::GenerateMappedNodeForCurrentSelection(UFunction* TargetFunction)
{
	if (!CurrentSelectedUnresolved.IsValid() || !TargetFunction)
	{
		SetStatusMessage(TEXT("请先选择未匹配节点与有效函数。"));
		return;
	}

	UEdGraph* TargetGraph = GetCurrentTargetGraph();
	if (!TargetGraph)
	{
		SetStatusMessage(TEXT("未找到当前激活图表，无法执行映射生成。"));
		return;
	}

	const FScopedTransaction Transaction(FText::FromString(TEXT("Map Unresolved Node")));
	TargetGraph->Modify();

	if (!TextToBlueprintGenerator::SpawnFunctionNode(TargetGraph, TargetFunction, CurrentSelectedUnresolved->NodeData))
	{
		SetStatusMessage(TEXT("映射生成失败，请检查目标函数是否可用于蓝图调用。"));
		return;
	}

	const FString ResolvedName = CurrentSelectedUnresolved->NodeData.FunctionName;
	UnresolvedSource.Remove(CurrentSelectedUnresolved);
	CurrentSelectedUnresolved.Reset();
	RefreshUnresolvedList();
	TargetGraph->NotifyGraphChanged();
	SetStatusMessage(FString::Printf(TEXT("已完成未匹配节点映射：%s"), *ResolvedName));
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
