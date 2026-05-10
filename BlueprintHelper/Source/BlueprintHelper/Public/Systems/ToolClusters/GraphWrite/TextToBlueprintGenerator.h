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
	MacroInstance,
	Branch,
	Sequence,
	CustomEvent,
	Event,
	CallDelegate,
	AddDelegate,
	RemoveDelegate,
	ClearDelegate,
	AssignDelegate,
	CreateDelegate,
	MakeArray,
	MakeMap,
	MakeSet,
	MakeStruct,
	BreakStruct,
	// v2.2 — 高级节点
	Self,
	DynamicCast,
	SpawnActorFromClass,
	FormatText,
	GetArrayItem,
	Timeline,
	// v2.3 — 全覆盖收尾
	Knot,
	Comment,
	Literal,
	GetEnumeratorName,
	GetEnumeratorNameAsString,
	ComponentBoundEvent,
	// v2.9 — Enhanced Input / 数学运算 / 流程控制
	EnhancedInputAction,
	PromotableOperator,
	CommutativeAssociativeBinaryOperator,
	SwitchInteger,
	SwitchString,
	SwitchName,
	SwitchEnum,
	Select
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
 * 自定义事件/引擎事件参数描述。
 */
struct FParsedEventParam
{
	/** 参数名称。 */
	FString Name;

	/** 参数引脚类型。 */
	FParsedPinType PinType;
};

/**
 * 事件节点引用描述。
 */
struct FParsedEventReference
{
	/** 事件名称（CustomEvent 用 event_name，Event 用引擎事件名如 ReceiveBeginPlay）。 */
	FString EventName;

	/** 自定义事件参数列表。 */
	TArray<FParsedEventParam> Params;
};

/**
 * 委托节点引用描述。
 */
struct FParsedDelegateReference
{
	/** 事件分发器属性名称。 */
	FString DelegatePropertyName;

	/** 绑定的函数名称（用于 CreateDelegate / AssignDelegate）。 */
	FString FunctionName;
};

/**
 * 容器构造节点引用描述（MakeArray / MakeSet / MakeMap）。
 */
struct FParsedContainerReference
{
	/** MakeArray / MakeSet：元素数量。 */
	int32 NumInputs = 0;

	/** MakeMap：键值对数量。 */
	int32 NumPairs = 0;

	/** MakeArray / MakeSet 的元素类型。 */
	FParsedPinType ElementType;

	/** MakeMap 的键类型。 */
	FParsedPinType KeyType;

	/** MakeMap 的值类型。 */
	FParsedPinType ValueType;
};

/**
 * 结构体操作节点引用描述（MakeStruct / BreakStruct）。
 */
struct FParsedStructReference
{
	/** 结构体路径，例如 "/Script/CoreUObject.Vector"。 */
	FString StructPath;
};

/**
 * v2.2 — Cast 节点引用描述。
 */
struct FParsedCastReference
{
	/** 目标类路径，例如 "/Script/Engine.Character"。 */
	FString TargetClassPath;
};

/**
 * v2.2 — SpawnActor 节点引用描述。
 */
struct FParsedSpawnReference
{
	/** 要生成的 Actor 类路径。 */
	FString ClassPath;
};

/**
 * v2.2 — FormatText 节点引用描述。
 */
struct FParsedFormatTextReference
{
	/** 格式化字符串，例如 "{Name} has {Count} items"。 */
	FString FormatString;
};

/**
 * v2.2 — Timeline 节点引用描述。
 */
struct FParsedTimelineReference
{
	/** Timeline 名称。 */
	FString TimelineName;

	/** 是否自动播放。 */
	bool bAutoPlay = false;

	/** 是否循环。 */
	bool bLoop = false;

	/** 浮点轨道名称列表。 */
	TArray<FString> FloatTracks;

	/** 向量轨道名称列表。 */
	TArray<FString> VectorTracks;

	/** 事件轨道名称列表。 */
	TArray<FString> EventTracks;
};

/**
 * v2.3 — Literal 节点引用描述（对象引用常量）。
 */
struct FParsedLiteralReference
{
	/** 对象引用路径，例如 "/Script/Engine.Actor:DefaultSubobjectName"。 */
	FString ObjectPath;
};

/**
 * v2.3 — ComponentBoundEvent 节点引用描述。
 */
struct FParsedComponentBoundEventReference
{
	/** 委托属性名称。 */
	FString DelegatePropertyName;

	/** 委托所属类路径。 */
	FString DelegateOwnerClassPath;

	/** 组件属性名称。 */
	FString ComponentPropertyName;
};

/**
 * v2.3 — Comment 节点引用描述。
 */
struct FParsedCommentReference
{
	/** 注释文本。 */
	FString CommentText;

	/** 注释框宽度。 */
	float Width = 400.0f;

	/** 注释框高度。 */
	float Height = 100.0f;

	/** 注释框颜色（R,G,B,A 格式字符串）。 */
	FString CommentColor;

	/** 字体大小。 */
	int32 FontSize = 18;
};

/**
 * v2.9 — Enhanced Input Action 节点引用描述。
 */
struct FParsedEnhancedInputActionReference
{
	/** 输入动作资产路径，例如 "/Game/Input/IA_Jump"。 */
	FString InputActionPath;
};

/**
 * v2.9 — Switch 节点引用描述。
 */
struct FParsedSwitchReference
{
	/** Switch 分支的 case 值列表。 */
	TArray<FString> CaseValues;

	/** 是否包含 Default 引脚，默认 true。 */
	bool bHasDefaultPin = true;

	/** SwitchEnum 的枚举路径。 */
	FString EnumPath;

	/** SwitchInteger 的起始索引。 */
	int32 StartIndex = 0;
};

/**
 * v2.9 — Select 节点引用描述。
 */
struct FParsedSelectReference
{
	/** 选项数量。 */
	int32 NumOptions = 2;

	/** 绑定的枚举路径（可选，如果基于枚举选择）。 */
	FString EnumPath;
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

	/** 事件节点引用数据。 */
	FParsedEventReference EventReference;

	/** 委托节点引用数据。 */
	FParsedDelegateReference DelegateReference;

	/** 容器构造节点引用数据。 */
	FParsedContainerReference ContainerReference;

	/** 结构体操作节点引用数据。 */
	FParsedStructReference StructReference;

	/** v2.2 — Cast 节点引用数据。 */
	FParsedCastReference CastReference;

	/** v2.2 — SpawnActor 节点引用数据。 */
	FParsedSpawnReference SpawnReference;

	/** v2.2 — FormatText 节点引用数据。 */
	FParsedFormatTextReference FormatTextReference;

	/** v2.2 — Timeline 节点引用数据。 */
	FParsedTimelineReference TimelineReference;

	/** v2.3 — Literal 节点引用数据。 */
	FParsedLiteralReference LiteralReference;

	/** v2.3 — ComponentBoundEvent 节点引用数据。 */
	FParsedComponentBoundEventReference ComponentBoundEventReference;

	/** v2.3 — Comment 节点引用数据。 */
	FParsedCommentReference CommentReference;

	/** v2.9 — Enhanced Input Action 节点引用数据。 */
	FParsedEnhancedInputActionReference EnhancedInputActionReference;

	/** v2.9 — Switch 节点引用数据。 */
	FParsedSwitchReference SwitchReference;

	/** v2.9 — Select 节点引用数据。 */
	FParsedSelectReference SelectReference;
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
 * JSON 生成阶段的结构化诊断。
 */
struct FBlueprintGeneratorDiagnostic
{
	/** severity: info / warning / error。 */
	FString Severity;

	/** 稳定错误码，供服务层和 MCP 客户端识别。 */
	FString Code;

	/** JSON 节点 ID。 */
	FString NodeId;

	/** 相关引脚名称。 */
	FString PinName;

	/** 面向人的诊断信息。 */
	FString Message;

	bool IsError() const
	{
		return Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase);
	}
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

	/** 请求应用的默认值数量。 */
	int32 RequestedDefaultValueCount = 0;

	/** 成功应用的默认值数量。 */
	int32 AppliedDefaultValueCount = 0;

	/** 默认值应用诊断。 */
	TArray<FBlueprintGeneratorDiagnostic> DefaultValueDiagnostics;

	/** 请求解析的 pin_type 数量。 */
	int32 RequestedPinTypeCount = 0;

	/** 成功解析的 pin_type 数量。 */
	int32 ResolvedPinTypeCount = 0;

	/** pin_type 解析诊断。 */
	TArray<FBlueprintGeneratorDiagnostic> PinTypeDiagnostics;

	/** 请求建立的连线数量。 */
	int32 RequestedConnectionCount = 0;

	/** 成功建立的连线数量。 */
	int32 CreatedConnectionCount = 0;

	/** 连线创建诊断。 */
	TArray<FBlueprintGeneratorDiagnostic> ConnectionDiagnostics;

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

	/** 在蓝图的多个图表中根据 JSON 生成节点（支持 graphs 数组和单图模式）。v2.1 */
	static FBlueprintGenerateResult GenerateMultiGraphFromJson(UBlueprint* Blueprint, const FString& JsonString, TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes);

	/** 按名称查找蓝图中的图表（EventGraph / 函数图 / 宏图）。v2.1 */
	static UEdGraph* FindGraphByName(UBlueprint* Blueprint, const FString& GraphName);

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

	/** 为目标节点应用默认值，并返回结构化诊断。 */
	static TArray<FBlueprintGeneratorDiagnostic> ApplyDefaultValues(
		UK2Node* TargetNode,
		const TMap<FString, FString>& DefaultValues,
		const FString& NodeId = TEXT(""));

	/** 确保目标函数图中存在指定本地变量。失败时填写 OutErrorMessage。 */
	static bool EnsureLocalVariableExists(UEdGraph* TargetGraph, const FParsedLocalVariableDeclaration& Declaration, FString& OutErrorMessage);

	/** 将轻量引脚类型转换为 UE 引脚类型。 */
	static bool ConvertToEdGraphPinType(const FParsedPinType& InPinType, struct FEdGraphPinType& OutPinType, FString& OutErrorMessage);

	/** 根据别名查找节点引脚。 */
	static class UEdGraphPin* FindPinByAlias(UK2Node* TargetNode, const FString& RequestedPinName);

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

	/** 从图表解析本地变量作用域结构。 */
	static class UStruct* ResolveLocalVariableScope(UEdGraph* TargetGraph, FString& OutErrorMessage);

	/** 从变量描述解析变量节点的来源结构。 */
	static class UStruct* ResolveVariableSource(UEdGraph* TargetGraph, const FParsedVariableReference& VariableReference, FString& OutErrorMessage);

	/** 查找标准宏图。 */
	static class UEdGraph* ResolveMacroGraph(const FParsedMacroReference& MacroReference, FString& OutErrorMessage);

	/** 解析 JSON 中的事件引用描述。 */
	static FParsedEventReference ResolveEventReference(const TSharedPtr<class FJsonObject>& NodeObject);

	/** 解析 JSON 中的委托引用描述。 */
	static FParsedDelegateReference ResolveDelegateReference(const TSharedPtr<class FJsonObject>& NodeObject);

	/** 解析 JSON 中的容器构造引用描述。 */
	static FParsedContainerReference ResolveContainerReference(const TSharedPtr<class FJsonObject>& NodeObject);

	/** 解析 JSON 中的结构体操作引用描述。 */
	static FParsedStructReference ResolveStructReference(const TSharedPtr<class FJsonObject>& NodeObject);

	/** 解析 JSON 中的类型转换引用描述。v2.2 */
	static FParsedCastReference ResolveCastReference(const TSharedPtr<class FJsonObject>& NodeObject);

	/** 解析 JSON 中的 SpawnActor 引用描述。v2.2 */
	static FParsedSpawnReference ResolveSpawnReference(const TSharedPtr<class FJsonObject>& NodeObject);

	/** 解析 JSON 中的 FormatText 引用描述。v2.2 */
	static FParsedFormatTextReference ResolveFormatTextReference(const TSharedPtr<class FJsonObject>& NodeObject);

	/** 解析 JSON 中的 Timeline 引用描述。v2.2 */
	static FParsedTimelineReference ResolveTimelineReference(const TSharedPtr<class FJsonObject>& NodeObject);

	/** 解析 JSON 中的 Literal 引用描述。v2.3 */
	static FParsedLiteralReference ResolveLiteralReference(const TSharedPtr<class FJsonObject>& NodeObject);

	/** 解析 JSON 中的 ComponentBoundEvent 引用描述。v2.3 */
	static FParsedComponentBoundEventReference ResolveComponentBoundEventReference(const TSharedPtr<class FJsonObject>& NodeObject);

	/** 解析 JSON 中的 Comment 引用描述。v2.3 */
	static FParsedCommentReference ResolveCommentReference(const TSharedPtr<class FJsonObject>& NodeObject);

	/** 解析 JSON 中的 Enhanced Input Action 引用描述。v2.9 */
	static FParsedEnhancedInputActionReference ResolveEnhancedInputActionReference(const TSharedPtr<class FJsonObject>& NodeObject);

	/** 解析 JSON 中的 Switch 引用描述。v2.9 */
	static FParsedSwitchReference ResolveSwitchReference(const TSharedPtr<class FJsonObject>& NodeObject);

	/** 解析 JSON 中的 Select 引用描述。v2.9 */
	static FParsedSelectReference ResolveSelectReference(const TSharedPtr<class FJsonObject>& NodeObject);

	/** 在指定图表中根据 JSON 子对象生成节点和连线（内部辅助）。v2.1 */
	static FBlueprintGenerateResult GenerateNodesAndLinksForGraph(UEdGraph* TargetGraph, const TSharedPtr<class FJsonObject>& GraphJsonObject, TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes);

	/** 设置单个引脚默认值。 */
	static bool ApplyPinDefaultValue(
		class UEdGraphPin* TargetPin,
		const FString& InValue,
		FString& OutDiagnosticCode,
		FString& OutMessage);
};
