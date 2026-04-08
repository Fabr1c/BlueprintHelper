#pragma once

#include "CoreMinimal.h"

class UEdGraph;
class UFunction;
class UK2Node;
class UK2Node_CallFunction;

/**
 * 可解析蓝图节点类型。
 */
enum class EParsedBlueprintNodeType : uint8
{
	Unknown,
	CallFunction,
	VariableGet,
	VariableSet,
	MacroInstance
};

/**
 * 轻量引脚类型描述，用于本地变量声明与变量节点重建。
 */
struct FParsedPinType
{
	/** 引脚主分类。 */
	FString Category;

	/** 引脚子分类。 */
	FString SubCategory;

	/** 子分类对象路径。 */
	FString SubCategoryObjectPath;

	/** 容器类型。 */
	FString ContainerType;

	/** 是否为引用。 */
	bool bIsReference = false;

	/** 是否为常量。 */
	bool bIsConst = false;

	/** 是否有效。 */
	bool IsValid() const
	{
		return !Category.IsEmpty();
	}

	/** 是否与另一描述一致。 */
	bool Equals(const FParsedPinType& Other) const
	{
		return Category == Other.Category
			&& SubCategory == Other.SubCategory
			&& SubCategoryObjectPath == Other.SubCategoryObjectPath
			&& ContainerType == Other.ContainerType
			&& bIsReference == Other.bIsReference
			&& bIsConst == Other.bIsConst;
	}

	/** 生成调试字符串。 */
	FString ToDebugString() const
	{
		return FString::Printf(TEXT("Category=%s, SubCategory=%s, Object=%s, Container=%s, Ref=%s, Const=%s"),
			*Category,
			*SubCategory,
			*SubCategoryObjectPath,
			*ContainerType,
			bIsReference ? TEXT("true") : TEXT("false"),
			bIsConst ? TEXT("true") : TEXT("false"));
	}
};

/**
 * 变量引用描述，支持成员变量与本地变量。
 */
struct FParsedVariableReference
{
	/** 变量作用域类型：member/local。 */
	FString ScopeType;

	/** 变量名称。 */
	FString VariableName;

	/** 变量所属类路径，成员变量时可使用。 */
	FString OwnerClassPath;

	/** 作用域图名，本地变量时可作为提示。 */
	FString ScopeGraphName;

	/** 是否视为 Self 上下文成员变量。 */
	bool bSelfContext = true;

	/** 若变量不存在，是否尝试自动创建。 */
	bool bEnsureExists = false;

	/** 变量引脚类型。 */
	FParsedPinType PinType;

	/** 变量默认值。 */
	FString DefaultValue;

	/** 是否为本地变量。 */
	bool IsLocalVariable() const
	{
		return ScopeType.Equals(TEXT("local"), ESearchCase::IgnoreCase);
	}

	/** 是否为成员变量。 */
	bool IsMemberVariable() const
	{
		return ScopeType.IsEmpty() || ScopeType.Equals(TEXT("member"), ESearchCase::IgnoreCase);
	}
};

/**
 * 宏节点引用描述。
 */
struct FParsedMacroReference
{
	/** 宏库来源：standard/asset_path。 */
	FString LibraryType;

	/** 宏名称。 */
	FString MacroName;

	/** 宏蓝图资产路径。 */
	FString MacroAssetPath;
};

/**
 * 本地变量声明描述。
 */
struct FParsedLocalVariableDeclaration
{
	/** 本地变量名称。 */
	FString Name;

	/** 本地变量类型。 */
	FParsedPinType PinType;

	/** 默认值。 */
	FString DefaultValue;

	/** 是否在缺失时自动创建。 */
	bool bEnsureExists = true;
};

/**
 * 蓝图连线解析数据，描述两个节点之间的引脚连接关系。
 */
struct FParsedLink
{
	/** 起始节点 ID。 */
	FString FromId;

	/** 起始引脚名称。 */
	FString FromPin;

	/** 目标节点 ID。 */
	FString ToId;

	/** 目标引脚名称。 */
	FString ToPin;
};

/**
 * 蓝图节点解析数据，描述待生成的函数节点和默认值。
 */
struct FParsedNode
{
	/** 节点唯一标识。 */
	FString Id;

	/** 节点类型。 */
	EParsedBlueprintNodeType NodeType = EParsedBlueprintNodeType::Unknown;

	/** 原始类型字符串。 */
	FString SourceType;

	/** 节点对应的函数名称。 */
	FString FunctionName;

	/** 节点 X 坐标。 */
	float X = 0.0f;

	/** 节点 Y 坐标。 */
	float Y = 0.0f;

	/** 引脚默认值集合，Key 为引脚名。 */
	TMap<FString, FString> DefaultValues;

	/** 变量节点引用数据。 */
	FParsedVariableReference VariableReference;

	/** 宏节点引用数据。 */
	FParsedMacroReference MacroReference;
};

/**
 * 未匹配节点数据，供 Slate 左侧列表展示与手动映射使用。
 */
struct FUnresolvedNodeItem
{
	/** 原始节点数据。 */
	FParsedNode NodeData;

	/** 列表显示文本。 */
	FString DisplayText;

	/** 未解析原因。 */
	FString Reason;
};

/**
 * 引擎函数列表数据，供右侧搜索与映射选择使用。
 */
struct FEngineFunctionItem
{
	/** 真实函数指针。 */
	UFunction* FunctionPtr = nullptr;

	/** 显示函数名称。 */
	FString FunctionName;

	/** 原生函数名称。 */
	FString NativeFunctionName;

	/** 蓝图分类。 */
	FString Category;
};

/**
 * 蓝图 JSON 生成结果，供 UI 展示解析与生成状态。
 */
struct FBlueprintGenerateResult
{
	/** 本次 JSON 解析与生成链路是否成功执行。 */
	bool bSucceed = false;

	/** 成功生成的节点数量。 */
	int32 GeneratedNodeCount = 0;

	/** 未匹配节点数量。 */
	int32 UnresolvedNodeCount = 0;

	/** 用于状态栏展示的结果文本。 */
	FString Message;
};

/**
 * 文本转蓝图生成器，负责解析 JSON 并在图表中生成函数节点。
 */
class BLUEPRINTHELPER_API TextToBlueprintGenerator
{
public:
	/** 查找函数名称对应的蓝图函数。 */
	static UFunction* FindFunctionByName(const FString& FuncName);

	/** 根据 JSON 在目标图表中生成蓝图节点。 */
	static FBlueprintGenerateResult GenerateBlueprintFromJson(UEdGraph* TargetGraph, const FString& JsonString, TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes);

	/** 获取所有可用于蓝图调用的函数列表。 */
	static TArray<TSharedPtr<FEngineFunctionItem>> GetAllBlueprintFunctions();

	/** 在目标图表中生成一个函数节点。 */
	static UK2Node_CallFunction* SpawnFunctionNode(UEdGraph* TargetGraph, UFunction* TargetFunction, const FParsedNode& NodeData);

	/** 在目标图表中生成一个变量读取节点。 */
	static UK2Node* SpawnVariableGetNode(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutErrorMessage);

	/** 在目标图表中生成一个变量写入节点。 */
	static UK2Node* SpawnVariableSetNode(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutErrorMessage);

	/** 在目标图表中生成一个宏节点。 */
	static UK2Node* SpawnMacroNode(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutErrorMessage);

	/** 为目标节点应用默认值。 */
	static void ApplyDefaultValues(UK2Node* TargetNode, const TMap<FString, FString>& DefaultValues);

private:
	/** 解析 JSON 中的节点类型字段。 */
	static EParsedBlueprintNodeType ResolveNodeType(const TSharedPtr<class FJsonObject>& NodeObject);

	/** 将 Json 值转换为字符串。 */
	static FString ConvertJsonValueToString(const TSharedPtr<class FJsonValue>& JsonValue);

	/** 解析 JSON 节点中的函数名字段。 */
	static FString ResolveNodeFunctionName(const TSharedPtr<class FJsonObject>& NodeObject);

	/** 解析 JSON 中的轻量引脚类型。 */
	static FParsedPinType ResolvePinType(const TSharedPtr<class FJsonObject>& PinTypeObject);

	/** 解析 JSON 中的变量引用描述。 */
	static FParsedVariableReference ResolveVariableReference(const TSharedPtr<class FJsonObject>& NodeObject);

	/** 解析 JSON 中的宏引用描述。 */
	static FParsedMacroReference ResolveMacroReference(const TSharedPtr<class FJsonObject>& NodeObject);

	/** 解析顶层本地变量声明。 */
	static void ResolveLocalVariableDeclarations(const TSharedPtr<class FJsonObject>& JsonObject, TArray<FParsedLocalVariableDeclaration>& OutDeclarations);

	/** 将轻量引脚类型转换为 UE 引脚类型。 */
	static bool ConvertToEdGraphPinType(const FParsedPinType& InPinType, struct FEdGraphPinType& OutPinType, FString& OutErrorMessage);

	/** 确保目标函数图中存在指定本地变量。 */
	static bool EnsureLocalVariableExists(UEdGraph* TargetGraph, const FParsedLocalVariableDeclaration& Declaration, FString& OutErrorMessage);

	/** 从图表解析本地变量作用域结构。 */
	static class UStruct* ResolveLocalVariableScope(UEdGraph* TargetGraph, FString& OutErrorMessage);

	/** 从变量描述解析变量节点的来源结构。 */
	static class UStruct* ResolveVariableSource(UEdGraph* TargetGraph, const FParsedVariableReference& VariableReference, FString& OutErrorMessage);

	/** 查找标准宏图。 */
	static class UEdGraph* ResolveMacroGraph(const FParsedMacroReference& MacroReference, FString& OutErrorMessage);

	/** 根据别名查找节点引脚。 */
	static class UEdGraphPin* FindPinByAlias(UK2Node* TargetNode, const FString& RequestedPinName);

	/** 设置单个引脚默认值。 */
	static bool ApplyPinDefaultValue(class UEdGraphPin* TargetPin, const FString& InValue);
};
