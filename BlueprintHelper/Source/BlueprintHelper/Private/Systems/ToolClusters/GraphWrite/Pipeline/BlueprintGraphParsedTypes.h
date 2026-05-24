#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"

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
	Self,
	DynamicCast,
	SpawnActorFromClass,
	FormatText,
	GetArrayItem,
	Knot,
	Comment,
	Literal,
	GetEnumeratorName,
	GetEnumeratorNameAsString,
	ComponentBoundEvent,
	EnhancedInputAction,
	PromotableOperator,
	CommutativeAssociativeBinaryOperator,
	SwitchInteger,
	SwitchString,
	SwitchName,
	SwitchEnum,
	Select
};

struct FParsedPinType
{
	FString Category;
	FString SubCategory;
	FString SubCategoryObjectPath;
	FString ContainerType;
	bool bIsReference = false;
	bool bIsConst = false;

	bool IsValid() const
	{
		return !Category.IsEmpty();
	}

	bool Equals(const FParsedPinType& Other) const
	{
		return Category == Other.Category
			&& SubCategory == Other.SubCategory
			&& SubCategoryObjectPath == Other.SubCategoryObjectPath
			&& ContainerType == Other.ContainerType
			&& bIsReference == Other.bIsReference
			&& bIsConst == Other.bIsConst;
	}

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

struct FParsedVariableReference
{
	FString ScopeType;
	FString VariableName;
	FString OwnerClassPath;
	FString ScopeGraphName;
	bool bSelfContext = true;
	bool bEnsureExists = false;
	FParsedPinType PinType;
	FString DefaultValue;

	bool IsLocalVariable() const
	{
		return ScopeType.Equals(TEXT("local"), ESearchCase::IgnoreCase);
	}

	bool IsMemberVariable() const
	{
		return ScopeType.IsEmpty() || ScopeType.Equals(TEXT("member"), ESearchCase::IgnoreCase);
	}
};

struct FParsedEventParam
{
	FString Name;
	FParsedPinType PinType;
};

struct FParsedEventReference
{
	FString EventName;
	TArray<FParsedEventParam> Params;
};

struct FParsedDelegateReference
{
	FString DelegatePropertyName;
	FString FunctionName;
	FString SearchMode;
	FString AmbiguityPolicy;
	TArray<FString> CategoryPriority;
	FString ActionContextStatementId;
};

struct FParsedContainerReference
{
	int32 NumInputs = 0;
	int32 NumPairs = 0;
	FParsedPinType ElementType;
	FParsedPinType KeyType;
	FParsedPinType ValueType;
};

struct FParsedStructReference
{
	FString StructPath;
};

struct FParsedCastReference
{
	FString TargetClassPath;
};

struct FParsedSpawnReference
{
	FString ClassPath;
};

struct FParsedFormatTextReference
{
	FString FormatString;
};

struct FParsedLiteralReference
{
	FString ObjectPath;
};

struct FParsedComponentBoundEventReference
{
	FString DelegatePropertyName;
	FString DelegateOwnerClassPath;
	FString ComponentPropertyName;
};

struct FParsedCommentReference
{
	FString CommentText;
	float Width = 400.0f;
	float Height = 100.0f;
	FString CommentColor;
	int32 FontSize = 18;
};

struct FParsedEnhancedInputActionReference
{
	FString InputActionPath;
};

struct FParsedSwitchReference
{
	TArray<FString> CaseValues;
	bool bHasDefaultPin = true;
	FString EnumPath;
	int32 StartIndex = 0;
};

struct FParsedSelectReference
{
	int32 NumOptions = 2;
	FString EnumPath;
};

struct FParsedLocalVariableDeclaration
{
	FString Name;
	FParsedPinType PinType;
	FString DefaultValue;
	bool bEnsureExists = true;
};

struct FParsedMacroReference
{
	FString LibraryType;
	FString MacroName;
	FString MacroAssetPath;
};

struct FParsedLink
{
	FString FromId;
	FString FromPin;
	FString ToId;
	FString ToPin;
};

struct FParsedNode
{
	FString Id;
	EParsedBlueprintNodeType NodeType = EParsedBlueprintNodeType::Unknown;
	FString SourceType;
	FString FunctionName;
	FString ResolvedCallFunctionStableId;
	FString SearchMode;
	FString AmbiguityPolicy;
	TArray<FString> CategoryPriority;
	FString ActionContextStatementId;
	TMap<FString, FString> ArgumentTypes;
	TMap<FString, FBlueprintHelperCallFunctionPinType> ArgumentPinTypes;
	FString TargetObjectName;
	FString TargetObjectType;
	FBlueprintHelperCallFunctionPinType TargetObjectPinType;
	FString ExpectedReturnType;
	FBlueprintHelperCallFunctionPinType ExpectedReturnPinType;
	float X = 0.0f;
	float Y = 0.0f;
	TMap<FString, FString> DefaultValues;
	FParsedVariableReference VariableReference;
	FParsedMacroReference MacroReference;
	FParsedEventReference EventReference;
	FParsedDelegateReference DelegateReference;
	FParsedContainerReference ContainerReference;
	FParsedStructReference StructReference;
	FParsedCastReference CastReference;
	FParsedSpawnReference SpawnReference;
	FParsedFormatTextReference FormatTextReference;
	FParsedLiteralReference LiteralReference;
	FParsedComponentBoundEventReference ComponentBoundEventReference;
	FParsedCommentReference CommentReference;
	FParsedEnhancedInputActionReference EnhancedInputActionReference;
	FParsedSwitchReference SwitchReference;
	FParsedSelectReference SelectReference;
};
