// This Project Is Made By Fabric

#pragma once

#include "CoreMinimal.h"
#include "TextToBlueprintGenerator.h"
#include "Widgets/SCompoundWidget.h"

class SListViewBase;
class SMultiLineEditableTextBox;
class SSearchBox;
class STextBlock;
class FBlueprintHelperImportService;
class FBlueprintHelperExportService;
class FBlueprintHelperGraphResolver;
template <typename ItemType> class SListView;

/**
 * BlueprintHelper 主窗口，负责展示 Blueprint T3D 与 JSON 的双向转换及未匹配节点映射界面。
 */
class BLUEPRINTHELPER_API SHelperMainWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHelperMainWidget)
		: _ImportService(nullptr)
		, _GraphResolver(nullptr)
		{
		}

	SLATE_ARGUMENT(const FBlueprintHelperImportService*, ImportService)
	SLATE_ARGUMENT(const FBlueprintHelperGraphResolver*, GraphResolver)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
	
	/** 根据搜索框内容刷新右侧函数过滤结果。 */
	void OnSearchTextChanged(const FText& InFilterText);
	
	/** 双击引擎函数列表项时，执行未匹配节点映射。 */
	void OnFunctionDoubleClicked(TSharedPtr<FEngineFunctionItem> Item);

private:
	/** 生成左侧未匹配节点列表行。 */
	TSharedRef<ITableRow> GenerateUnresolvedRow(TSharedPtr<FUnresolvedNodeItem> Item, const TSharedRef<STableViewBase>& OwnerTable) const;

	/** 生成右侧引擎函数列表行。 */
	TSharedRef<ITableRow> GenerateFunctionRow(TSharedPtr<FEngineFunctionItem> Item, const TSharedRef<STableViewBase>& OwnerTable) const;

	/** 未匹配节点选中变化时更新当前映射目标。 */
	void OnUnresolvedSelectionChanged(TSharedPtr<FUnresolvedNodeItem> Item, ESelectInfo::Type SelectInfo);

	/** 将蓝图文本或剪贴板内容转换为插件可回放 JSON。 */
	FReply OnParseClipboardClicked();

	/** 将主文本区内容复制回剪贴板。 */
	FReply OnCopyResultClicked();

	/** 将 Json -> 蓝图规则 Markdown 复制到剪贴板。 */
	FReply OnCopyJsonRuleClicked();

	/** 根据主文本区 JSON 内容生成蓝图节点。 */
	FReply OnGenerateFromTextClicked();

	/** 读取主文本区内容。 */
	FString GetMainText() const;

	/** 设置主文本区内容。 */
	void SetMainText(const FString& InText) const;

	/** 设置底部状态栏文本。 */
	void SetStatusMessage(const FString& InMessage) const;

	/** 获取本次转换优先使用的蓝图源文本。 */
	FString ResolveBlueprintSourceText() const;

	/** 获取当前目标蓝图图表。 */
	UEdGraph* GetCurrentTargetGraph() const;

	/** 刷新未匹配列表显示。 */
	void RefreshUnresolvedList() const;

	/** 根据当前选择生成映射后的真实节点。 */
	void GenerateMappedNodeForCurrentSelection(UFunction* TargetFunction);

	/** 未匹配节点列表数据源。 */
	TArray<TSharedPtr<FUnresolvedNodeItem>> UnresolvedSource;
	/** 引擎函数列表全量缓存。 */
	TArray<TSharedPtr<FEngineFunctionItem>> FunctionSource;
	/** 当前右侧过滤后的函数列表。 */
	TArray<TSharedPtr<FEngineFunctionItem>> FilteredFunctionSource;
	/** 主文本编辑框。 */
	TSharedPtr<SMultiLineEditableTextBox> MainTextBox;
	/** 未匹配列表视图。 */
	TSharedPtr<SListView<TSharedPtr<FUnresolvedNodeItem>>> UnresolvedListView;
	/** 引擎函数列表视图。 */
	TSharedPtr<SListView<TSharedPtr<FEngineFunctionItem>>> FunctionListView;
	/** 引擎函数搜索框。 */
	TSharedPtr<SSearchBox> FunctionSearchBox;
	/** 底部状态栏文本。 */
	TSharedPtr<STextBlock> StatusTextBlock;
	/** 当前选中的未匹配节点。 */
	TSharedPtr<FUnresolvedNodeItem> CurrentSelectedUnresolved;

	/** Service 层引用（由 Module 通过 SLATE_ARGUMENT 传入）。 */
	const FBlueprintHelperImportService* ImportService = nullptr;
	const FBlueprintHelperGraphResolver* GraphResolverPtr = nullptr;
};
