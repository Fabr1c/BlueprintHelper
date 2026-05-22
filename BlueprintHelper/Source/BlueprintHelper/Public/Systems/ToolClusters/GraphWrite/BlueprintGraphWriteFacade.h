#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphWriteExecutionStats.h"

class UEdGraph;
class UBlueprint;
class UFunction;
class UK2Node;
class UK2Node_CallFunction;

/**
 * 鍙В鏋愯摑鍥捐妭鐐圭被鍨嬨€?
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
	// v2.2 鈥?楂樼骇鑺傜偣
	Self,
	DynamicCast,
	SpawnActorFromClass,
	FormatText,
	GetArrayItem,
	// v2.3 鈥?鍏ㄨ鐩栨敹灏?
	Knot,
	Comment,
	Literal,
	GetEnumeratorName,
	GetEnumeratorNameAsString,
	ComponentBoundEvent,
	// v2.9 鈥?Enhanced Input / 鏁板杩愮畻 / 娴佺▼鎺у埗
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
 * 杞婚噺寮曡剼绫诲瀷鎻忚堪锛岀敤浜庢湰鍦板彉閲忓０鏄庝笌鍙橀噺鑺傜偣閲嶅缓銆?
 */
struct FParsedPinType
{
	/** 寮曡剼涓诲垎绫汇€?*/
	FString Category;

	/** 寮曡剼瀛愬垎绫汇€?*/
	FString SubCategory;

	/** 瀛愬垎绫诲璞¤矾寰勩€?*/
	FString SubCategoryObjectPath;

	/** 瀹瑰櫒绫诲瀷銆?*/
	FString ContainerType;

	/** 鏄惁涓哄紩鐢ㄣ€?*/
	bool bIsReference = false;

	/** 鏄惁涓哄父閲忋€?*/
	bool bIsConst = false;

	/** 鏄惁鏈夋晥銆?*/
	bool IsValid() const
	{
		return !Category.IsEmpty();
	}

	/** 鏄惁涓庡彟涓€鎻忚堪涓€鑷淬€?*/
	bool Equals(const FParsedPinType& Other) const
	{
		return Category == Other.Category
			&& SubCategory == Other.SubCategory
			&& SubCategoryObjectPath == Other.SubCategoryObjectPath
			&& ContainerType == Other.ContainerType
			&& bIsReference == Other.bIsReference
			&& bIsConst == Other.bIsConst;
	}

	/** 鐢熸垚璋冭瘯瀛楃涓层€?*/
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
 * 鍙橀噺寮曠敤鎻忚堪锛屾敮鎸佹垚鍛樺彉閲忎笌鏈湴鍙橀噺銆?
 */
struct FParsedVariableReference
{
	/** 鍙橀噺浣滅敤鍩熺被鍨嬶細member/local銆?*/
	FString ScopeType;

	/** 鍙橀噺鍚嶇О銆?*/
	FString VariableName;

	/** 鍙橀噺鎵€灞炵被璺緞锛屾垚鍛樺彉閲忔椂鍙娇鐢ㄣ€?*/
	FString OwnerClassPath;

	/** 浣滅敤鍩熷浘鍚嶏紝鏈湴鍙橀噺鏃跺彲浣滀负鎻愮ず銆?*/
	FString ScopeGraphName;

	/** 鏄惁瑙嗕负 Self 涓婁笅鏂囨垚鍛樺彉閲忋€?*/
	bool bSelfContext = true;

	/** 鑻ュ彉閲忎笉瀛樺湪锛屾槸鍚﹀皾璇曡嚜鍔ㄥ垱寤恒€?*/
	bool bEnsureExists = false;

	/** 鍙橀噺寮曡剼绫诲瀷銆?*/
	FParsedPinType PinType;

	/** 鍙橀噺榛樿鍊笺€?*/
	FString DefaultValue;

	/** 鏄惁涓烘湰鍦板彉閲忋€?*/
	bool IsLocalVariable() const
	{
		return ScopeType.Equals(TEXT("local"), ESearchCase::IgnoreCase);
	}

	/** 鏄惁涓烘垚鍛樺彉閲忋€?*/
	bool IsMemberVariable() const
	{
		return ScopeType.IsEmpty() || ScopeType.Equals(TEXT("member"), ESearchCase::IgnoreCase);
	}
};

/**
 * 鑷畾涔変簨浠?寮曟搸浜嬩欢鍙傛暟鎻忚堪銆?
 */
struct FParsedEventParam
{
	/** 鍙傛暟鍚嶇О銆?*/
	FString Name;

	/** 鍙傛暟寮曡剼绫诲瀷銆?*/
	FParsedPinType PinType;
};

/**
 * 浜嬩欢鑺傜偣寮曠敤鎻忚堪銆?
 */
struct FParsedEventReference
{
	/** 浜嬩欢鍚嶇О锛圕ustomEvent 鐢?event_name锛孍vent 鐢ㄥ紩鎿庝簨浠跺悕濡?ReceiveBeginPlay锛夈€?*/
	FString EventName;

	/** 鑷畾涔変簨浠跺弬鏁板垪琛ㄣ€?*/
	TArray<FParsedEventParam> Params;
};

/**
 * 濮旀墭鑺傜偣寮曠敤鎻忚堪銆?
 */
struct FParsedDelegateReference
{
	/** 浜嬩欢鍒嗗彂鍣ㄥ睘鎬у悕绉般€?*/
	FString DelegatePropertyName;

	/** 缁戝畾鐨勫嚱鏁板悕绉帮紙鐢ㄤ簬 CreateDelegate / AssignDelegate锛夈€?*/
	FString FunctionName;

	FString SearchMode;
	FString AmbiguityPolicy;
	TArray<FString> CategoryPriority;
	FString ActionContextStatementId;
};

/**
 * 瀹瑰櫒鏋勯€犺妭鐐瑰紩鐢ㄦ弿杩帮紙MakeArray / MakeSet / MakeMap锛夈€?
 */
struct FParsedContainerReference
{
	/** MakeArray / MakeSet锛氬厓绱犳暟閲忋€?*/
	int32 NumInputs = 0;

	/** MakeMap锛氶敭鍊煎鏁伴噺銆?*/
	int32 NumPairs = 0;

	/** MakeArray / MakeSet 鐨勫厓绱犵被鍨嬨€?*/
	FParsedPinType ElementType;

	/** MakeMap 鐨勯敭绫诲瀷銆?*/
	FParsedPinType KeyType;

	/** MakeMap 鐨勫€肩被鍨嬨€?*/
	FParsedPinType ValueType;
};

/**
 * 缁撴瀯浣撴搷浣滆妭鐐瑰紩鐢ㄦ弿杩帮紙MakeStruct / BreakStruct锛夈€?
 */
struct FParsedStructReference
{
	/** 缁撴瀯浣撹矾寰勶紝渚嬪 "/Script/CoreUObject.Vector"銆?*/
	FString StructPath;
};

/**
 * v2.2 鈥?Cast 鑺傜偣寮曠敤鎻忚堪銆?
 */
struct FParsedCastReference
{
	/** 鐩爣绫昏矾寰勶紝渚嬪 "/Script/Engine.Character"銆?*/
	FString TargetClassPath;
};

/**
 * v2.2 鈥?SpawnActor 鑺傜偣寮曠敤鎻忚堪銆?
 */
struct FParsedSpawnReference
{
	/** 瑕佺敓鎴愮殑 Actor 绫昏矾寰勩€?*/
	FString ClassPath;
};

/**
 * v2.2 鈥?FormatText 鑺傜偣寮曠敤鎻忚堪銆?
 */
struct FParsedFormatTextReference
{
	/** 鏍煎紡鍖栧瓧绗︿覆锛屼緥濡?"{Name} has {Count} items"銆?*/
	FString FormatString;
};

/**
 * v2.3 鈥?Literal 鑺傜偣寮曠敤鎻忚堪锛堝璞″紩鐢ㄥ父閲忥級銆? */
struct FParsedLiteralReference
{
	/** 瀵硅薄寮曠敤璺緞锛屼緥濡?"/Script/Engine.Actor:DefaultSubobjectName"銆?*/
	FString ObjectPath;
};

/**
 * v2.3 鈥?ComponentBoundEvent 鑺傜偣寮曠敤鎻忚堪銆?
 */
struct FParsedComponentBoundEventReference
{
	/** 濮旀墭灞炴€у悕绉般€?*/
	FString DelegatePropertyName;

	/** 濮旀墭鎵€灞炵被璺緞銆?*/
	FString DelegateOwnerClassPath;

	/** 缁勪欢灞炴€у悕绉般€?*/
	FString ComponentPropertyName;
};

/**
 * v2.3 鈥?Comment 鑺傜偣寮曠敤鎻忚堪銆?
 */
struct FParsedCommentReference
{
	/** 娉ㄩ噴鏂囨湰銆?*/
	FString CommentText;

	/** 娉ㄩ噴妗嗗搴︺€?*/
	float Width = 400.0f;

	/** 娉ㄩ噴妗嗛珮搴︺€?*/
	float Height = 100.0f;

	/** 娉ㄩ噴妗嗛鑹诧紙R,G,B,A 鏍煎紡瀛楃涓诧級銆?*/
	FString CommentColor;

	/** 瀛椾綋澶у皬銆?*/
	int32 FontSize = 18;
};

/**
 * v2.9 鈥?Enhanced Input Action 鑺傜偣寮曠敤鎻忚堪銆?
 */
struct FParsedEnhancedInputActionReference
{
	/** 杈撳叆鍔ㄤ綔璧勪骇璺緞锛屼緥濡?"/Game/Input/IA_Jump"銆?*/
	FString InputActionPath;
};

/**
 * v2.9 鈥?Switch 鑺傜偣寮曠敤鎻忚堪銆?
 */
struct FParsedSwitchReference
{
	/** Switch 鍒嗘敮鐨?case 鍊煎垪琛ㄣ€?*/
	TArray<FString> CaseValues;

	/** 鏄惁鍖呭惈 Default 寮曡剼锛岄粯璁?true銆?*/
	bool bHasDefaultPin = true;

	/** SwitchEnum 鐨勬灇涓捐矾寰勩€?*/
	FString EnumPath;

	/** SwitchInteger 鐨勮捣濮嬬储寮曘€?*/
	int32 StartIndex = 0;
};

/**
 * v2.9 鈥?Select 鑺傜偣寮曠敤鎻忚堪銆?
 */
struct FParsedSelectReference
{
	/** 閫夐」鏁伴噺銆?*/
	int32 NumOptions = 2;

	/** 缁戝畾鐨勬灇涓捐矾寰勶紙鍙€夛紝濡傛灉鍩轰簬鏋氫妇閫夋嫨锛夈€?*/
	FString EnumPath;
};

/**
 * 鏈湴鍙橀噺澹版槑鎻忚堪銆?
 */
struct FParsedLocalVariableDeclaration
{
	/** 鏈湴鍙橀噺鍚嶇О銆?*/
	FString Name;

	/** 鏈湴鍙橀噺绫诲瀷銆?*/
	FParsedPinType PinType;

	/** 榛樿鍊笺€?*/
	FString DefaultValue;

	/** 鏄惁鍦ㄧ己澶辨椂鑷姩鍒涘缓銆?*/
	bool bEnsureExists = true;
};

/**
 * 鏈尮閰嶈妭鐐规暟鎹紝渚?Slate 宸︿晶鍒楄〃灞曠ず涓庢墜鍔ㄦ槧灏勪娇鐢ㄣ€?
 */
struct FBlueprintHelperCandidateFunctionGroup
{
	FString Target;
	TArray<FBlueprintHelperCallFunctionCandidateInfo> Candidates;
};

struct FUnresolvedNodeItem
{
	/** 鍒楄〃鏄剧ず鏂囨湰銆?*/
	FString DisplayText;

	/** 鏈В鏋愬師鍥犮€?*/
	FString Reason;
	TArray<FBlueprintHelperCandidateFunctionGroup> CandidateFunctions;
};

/**
 * JSON 鐢熸垚闃舵鐨勭粨鏋勫寲璇婃柇銆?
 */
struct FBlueprintGeneratorDiagnostic
{
	/** severity: info / warning / error銆?*/
	FString Severity;

	/** 绋冲畾閿欒鐮侊紝渚涙湇鍔″眰鍜?MCP 瀹㈡埛绔瘑鍒€?*/
	FString Code;

	/** JSON 鑺傜偣 ID銆?*/
	FString NodeId;

	/** 鐩稿叧寮曡剼鍚嶇О銆?*/
	FString PinName;

	/** 闈㈠悜浜虹殑璇婃柇淇℃伅銆?*/
	FString Message;

	bool IsError() const
	{
		return Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase);
	}
};

/**
 * 寮曟搸鍑芥暟鍒楄〃鏁版嵁锛屼緵鍙充晶鎼滅储涓庢槧灏勯€夋嫨浣跨敤銆?
 */
struct FEngineFunctionItem
{
	/** 鐪熷疄鍑芥暟鎸囬拡銆?*/
	UFunction* FunctionPtr = nullptr;

	/** 鏄剧ず鍑芥暟鍚嶇О銆?*/
	FString FunctionName;

	FString SearchMode;
	FString AmbiguityPolicy;
	TArray<FString> CategoryPriority;
	FString ActionContextStatementId;

	/** 鍘熺敓鍑芥暟鍚嶇О銆?*/
	FString NativeFunctionName;

	/** 钃濆浘鍒嗙被銆?*/
	FString Category;
};

/**
 * 钃濆浘 JSON 鐢熸垚缁撴灉锛屼緵 UI 灞曠ず瑙ｆ瀽涓庣敓鎴愮姸鎬併€?
 */
struct FBlueprintGenerateResult
{
	/** 鏈 JSON 瑙ｆ瀽涓庣敓鎴愰摼璺槸鍚︽垚鍔熸墽琛屻€?*/
	bool bSucceed = false;

	/** 鎴愬姛鐢熸垚鐨勮妭鐐规暟閲忋€?*/
	int32 GeneratedNodeCount = 0;

	/** 璇锋眰搴旂敤鐨勯粯璁ゅ€兼暟閲忋€?*/
	int32 RequestedDefaultValueCount = 0;

	/** 鎴愬姛搴旂敤鐨勯粯璁ゅ€兼暟閲忋€?*/
	int32 AppliedDefaultValueCount = 0;

	/** 榛樿鍊煎簲鐢ㄨ瘖鏂€?*/
	TArray<FBlueprintGeneratorDiagnostic> DefaultValueDiagnostics;

	/** 璇锋眰瑙ｆ瀽鐨?pin_type 鏁伴噺銆?*/
	int32 RequestedPinTypeCount = 0;

	/** 鎴愬姛瑙ｆ瀽鐨?pin_type 鏁伴噺銆?*/
	int32 ResolvedPinTypeCount = 0;

	/** pin_type 瑙ｆ瀽璇婃柇銆?*/
	TArray<FBlueprintGeneratorDiagnostic> PinTypeDiagnostics;

	/** 璇锋眰寤虹珛鐨勮繛绾挎暟閲忋€?*/
	int32 RequestedConnectionCount = 0;

	/** 鎴愬姛寤虹珛鐨勮繛绾挎暟閲忋€?*/
	int32 CreatedConnectionCount = 0;

	/** 杩炵嚎鍒涘缓璇婃柇銆?*/
	TArray<FBlueprintGeneratorDiagnostic> ConnectionDiagnostics;

	/** 鏈尮閰嶈妭鐐规暟閲忋€?*/
	int32 UnresolvedNodeCount = 0;

	/** 鐢ㄤ簬鐘舵€佹爮灞曠ず鐨勭粨鏋滄枃鏈€?*/
	FString Message;

	FBlueprintGraphWriteExecutionStats ExecutionStats;
};

/**
 * 鏂囨湰杞摑鍥剧敓鎴愬櫒锛岃礋璐ｈВ鏋?JSON 骞跺湪鍥捐〃涓敓鎴愬嚱鏁拌妭鐐广€?
 */
class BLUEPRINTHELPER_API FBlueprintGraphWriteFacade
{
public:
	static FBlueprintHelperCallFunctionResolveResult ResolveFunctionForGraph(UEdGraph* TargetGraph, const FString& FunctionQuery, const TMap<FString, FString>& DefaultValues);
	static FBlueprintHelperActionResolutionResult ResolveActionForGraph(const FBlueprintHelperActionResolutionRequest& Request);
	static FBlueprintGenerateResult GenerateBlueprintFromJson(UEdGraph* TargetGraph, const FString& JsonString, TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes);
	static FBlueprintGenerateResult GenerateMultiGraphFromJson(UBlueprint* Blueprint, const FString& JsonString, TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes);
	static UEdGraph* FindGraphByName(UBlueprint* Blueprint, const FString& GraphName);
	static TArray<TSharedPtr<FEngineFunctionItem>> GetAllBlueprintFunctions();
	static TArray<FBlueprintGeneratorDiagnostic> ApplyDefaultValues(UK2Node* TargetNode, const TMap<FString, FString>& DefaultValues, const FString& NodeId = TEXT(""));
	static bool EnsureLocalVariableExists(UEdGraph* TargetGraph, const FParsedLocalVariableDeclaration& Declaration, FString& OutErrorMessage);
	static bool ConvertToEdGraphPinType(const FParsedPinType& InPinType, struct FEdGraphPinType& OutPinType, FString& OutErrorMessage);
	static class UEdGraphPin* FindPinByAlias(UK2Node* TargetNode, const FString& RequestedPinName);
};
