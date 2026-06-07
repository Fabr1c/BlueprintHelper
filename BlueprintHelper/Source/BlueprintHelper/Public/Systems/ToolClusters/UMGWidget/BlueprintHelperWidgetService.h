// BlueprintHelper Service Layer — UMG Widget Tree 操作服务

#pragma once

#include "CoreMinimal.h"
#include "Shared/UMGWidget/BlueprintHelperWidgetTypes.h"

class UWidgetBlueprint;
class UWidget;
class UPanelWidget;

// ─── Widget 节点信息 ───

/** 单个 Widget 的摘要信息。 */
struct BLUEPRINTHELPER_API FBlueprintHelperWidgetInfo
{
	/** Widget 名称。 */
	FString Name;

	/** Widget 类名（如 CanvasPanel、TextBlock）。 */
	FString WidgetClass;

	/** 父 Widget 名称（根 Widget 为空）。 */
	FString ParentName;

	/** Slot 类名（如 CanvasPanelSlot、VerticalBoxSlot）。 */
	FString SlotClass;

	/** 子 Widget 数量（叶子节点为 0）。 */
	int32 ChildCount = 0;

	/** 树深度（根节点为 0）。 */
	int32 Depth = 0;
};

// ─── Widget 属性信息 ───

/** 单个可编辑属性的摘要。 */
struct BLUEPRINTHELPER_API FBlueprintHelperWidgetPropertyInfo
{
	/** 属性名称。 */
	FString Name;

	/** 属性类型。 */
	FString TypeName;

	/** 当前值的文本表示。 */
	FString Value;

	/** 属性所属的 CPF 标志摘要。 */
	FString Flags;
};

// ─── 查询结果 ───

struct BLUEPRINTHELPER_API FBlueprintHelperWidgetTreeResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	FString RootWidgetName;
	TArray<FBlueprintHelperWidgetInfo> Widgets;
	FBlueprintHelperWidgetTreeSummary Summary;
};

struct BLUEPRINTHELPER_API FBlueprintHelperWidgetPropertyResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	TArray<FBlueprintHelperWidgetPropertyInfo> Properties;
};

struct BLUEPRINTHELPER_API FBlueprintHelperWidgetMutationResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	FString AffectedWidget;
	bool bDryRun = false;
	TSharedPtr<FJsonObject> ReadbackContext;
};

struct BLUEPRINTHELPER_API FBlueprintHelperAddWidgetRequest
{
	FString AssetPath;
	FString ParentName;
	FString SlotName;
	FString WidgetClass;
	FString WidgetName;
	TOptional<int32> VirtualIndex;
	FString ExpectedParentName;
	bool bDryRun = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMoveWidgetRequest
{
	FString AssetPath;
	FString WidgetName;
	FString NewParentName;
	FString SlotName;
	TOptional<int32> VirtualIndex;
	FString ExpectedParentName;
	TOptional<int32> ExpectedVirtualIndex;
	bool bDryRun = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperSetNamedSlotContentRequest
{
	FString AssetPath;
	FString HostWidgetName;
	FString SlotName;
	FString WidgetClass;
	FString WidgetName;
	TOptional<int32> VirtualIndex;
	FString ExpectedContentWidgetName;
	bool bReplaceExisting = false;
	bool bDryRun = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperSetSlotPropertyRequest
{
	FString AssetPath;
	FString WidgetName;
	FString PropertyPath;
	FString Value;
	FString ExpectedSlotClassPath;
	bool bDryRun = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperSetWidgetAsVariableRequest
{
	FString AssetPath;
	FString WidgetName;
	bool bIsVariable = false;
	FString ExpectedWidgetClassPath;
	bool bDryRun = false;
};

/**
 * UMG Widget Tree 操作服务。
 * 提供 WidgetBlueprint 的 WidgetTree 查询、增删、移动、属性读写等操作。
 */
class BLUEPRINTHELPER_API FBlueprintHelperWidgetService
{
public:

#pragma region API

	/** 获取 WidgetBlueprint 的完整 Widget 树。 */
	FBlueprintHelperWidgetTreeResult GetWidgetTree(const FString& AssetPath) const;

	/**
	 * 向面板 Widget 添加子 Widget。
	 * @param AssetPath  WidgetBlueprint 资产路径
	 * @param ParentName 父面板 Widget 名称（空则添加到根）
	 * @param WidgetClass 要创建的 Widget 类名（如 TextBlock、Button、CanvasPanel）
	 * @param WidgetName 新 Widget 的名称（空则自动生成）
	 */
	FBlueprintHelperWidgetMutationResult AddWidget(
		const FBlueprintHelperAddWidgetRequest& Request) const;

	FBlueprintHelperWidgetMutationResult AddWidget(
		const FString& AssetPath,
		const FString& ParentName,
		const FString& WidgetClass,
		const FString& WidgetName,
		bool bDryRun = false) const;

	/** 从 Widget 树中移除指定 Widget（及其子树）。 */
	FBlueprintHelperWidgetMutationResult RemoveWidget(
		const FString& AssetPath,
		const FString& WidgetName,
		bool bDryRun = false) const;

	/**
	 * 将 Widget 移动到新的父面板下。
	 * @param AssetPath  WidgetBlueprint 资产路径
	 * @param WidgetName 要移动的 Widget 名称
	 * @param NewParentName 新父面板名称
	 * @param VirtualIndex virtual child position (-1 = append)
	 */
	FBlueprintHelperWidgetMutationResult MoveWidget(
		const FBlueprintHelperMoveWidgetRequest& Request) const;

	FBlueprintHelperWidgetMutationResult MoveWidget(
		const FString& AssetPath,
		const FString& WidgetName,
		const FString& NewParentName,
		int32 VirtualIndex = -1) const;

	FBlueprintHelperWidgetMutationResult SetNamedSlotContent(
		const FBlueprintHelperSetNamedSlotContentRequest& Request) const;

	FBlueprintHelperWidgetMutationResult SetSlotProperty(
		const FBlueprintHelperSetSlotPropertyRequest& Request) const;

	FBlueprintHelperWidgetMutationResult SetWidgetAsVariable(
		const FBlueprintHelperSetWidgetAsVariableRequest& Request) const;

	/** 获取 Widget 的可编辑属性列表。 */
	FBlueprintHelperWidgetPropertyResult GetWidgetProperties(
		const FString& AssetPath,
		const FString& WidgetName) const;

	/** 设置 Widget 的属性值。 */
	FBlueprintHelperWidgetMutationResult SetWidgetProperty(
		const FString& AssetPath,
		const FString& WidgetName,
		const FString& PropertyName,
		const FString& Value,
		bool bDryRun = false) const;

#pragma endregion API

private:
	/** 根据资产路径加载 WidgetBlueprint。 */
	UWidgetBlueprint* ResolveWidgetBlueprint(const FString& AssetPath, FString& OutError) const;

	/** 在 WidgetTree 中按名称查找 Widget。 */
	UWidget* FindWidgetByName(UWidgetBlueprint* WBP, const FString& Name, FString& OutError) const;

	/** 递归收集 Widget 树信息。 */
	void CollectWidgetInfo(UWidget* Widget, const FString& ParentName, int32 Depth,
		TArray<FBlueprintHelperWidgetInfo>& OutWidgets) const;

	/** 根据类名查找 UWidget 子类。 */
	UClass* FindWidgetClass(const FString& ClassName, FString& OutError) const;
};
