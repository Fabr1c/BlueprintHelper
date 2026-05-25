#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FJsonValue;
class UBlueprint;
class UStruct;

enum class EBlueprintHelperGraphStatementKind : uint8
{
	Unknown,
	Call,
	Field,
	Branch,
	Sequence,
	Let,
	Return,
	Create,
	Convert,
	Schedule,
	ContainerAction,
	ComponentBoundEvent,
	Delegate
};

enum class EBlueprintHelperGraphExpressionKind : uint8
{
	Unknown,
	Literal,
	Field,
	Call,
	Op,
	Construct,
	Deconstruct,
	Select,
	Create,
	Convert,
	Schedule,
	ContainerAction
};

enum class EBlueprintHelperGraphTargetKind : uint8
{
	Unknown,
	Function,
	Component,
	ComponentMemberFunction,
	Variable,
	PropertyPath,
	Temporary
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphSemanticDiagnostic
{
	FString Code;
	FString Path;
	FString Message;
	FString Severity = TEXT("error");
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphResolvedTarget
{
	EBlueprintHelperGraphTargetKind Kind = EBlueprintHelperGraphTargetKind::Unknown;
	FString Raw;
	FString Owner;
	FString Member;
	FString PropertyPath;
	FString Type;
	bool bVerifiedByContext = false;

	bool IsResolved() const;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphSymbol
{
	FString Name;
	FString SymbolId;
	FString ScopeId;
	FString SourceStatementId;
	FString SourceExpressionId;
	FString Type;
	FString Path;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphSemanticContext
{
	TSet<FString> VariableNames;
	TSet<FString> ComponentNames;
	TSet<FString> FunctionNames;
	TMap<FString, FString> TargetTypes;
	TMap<FString, const UStruct*> TargetStructs;

	static FBlueprintHelperGraphSemanticContext FromBlueprint(const UBlueprint* Blueprint);

	bool HasVariables() const;
	bool IsVariable(const FString& Name) const;
	bool IsComponent(const FString& Name) const;
	bool IsFunction(const FString& Name) const;
	FString FindTargetType(const FString& Name) const;
	bool TryFindTargetStruct(const FString& Name, const UStruct*& OutStruct) const;
	bool HasMemberFunction(const FString& OwnerName, const FString& FunctionName) const;
	bool HasPropertyPath(const FString& OwnerName, const FString& PropertyPath, FString& OutType) const;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphExpressionIR
{
	FString ExpressionId;
	FString Path;
	EBlueprintHelperGraphExpressionKind Kind = EBlueprintHelperGraphExpressionKind::Unknown;
	FString PatternName;
	FString Target;
	FString Name;
	FString Property;
	FString FieldOperation;
	FString FieldScope;
	FString FunctionOperation;
	FString TransformOperation;
	FString ScheduleOperation;
	FString CreateOperation;
	FString ContainerKind;
	FString ContainerOperation;
	FString ClassPath;
	FString AssetPath;
	FString GraphLatentAllowed;
	FString ElementType;
	FString KeyType;
	FString ValueType;
	FString PinType;
	FString KeyPinType;
	FString ValuePinType;
	FString Type;
	FString Operator;
	FString LiteralValue;
	FString ResolvedCallFunctionStableId;
	FString SearchMode;
	FString AmbiguityPolicy;
	TArray<FString> CategoryPriority;
	TMap<FString, FString> ContextEvidence;
	FBlueprintHelperGraphResolvedTarget ResolvedTarget;
	TSharedPtr<FBlueprintHelperGraphExpressionIR> TargetObject;
	TSharedPtr<FBlueprintHelperGraphExpressionIR> Value;
	TSharedPtr<FBlueprintHelperGraphExpressionIR> Condition;
	TSharedPtr<FBlueprintHelperGraphExpressionIR> ThenValue;
	TSharedPtr<FBlueprintHelperGraphExpressionIR> ElseValue;
	TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>> Args;
	TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>> Fields;
	TArray<FString> FieldNames;
	TArray<TSharedPtr<FBlueprintHelperGraphExpressionIR>> Options;
	TSharedPtr<FBlueprintHelperGraphExpressionIR> Left;
	TSharedPtr<FBlueprintHelperGraphExpressionIR> Right;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphStatementIR
{
	FString StatementId;
	FString Path;
	EBlueprintHelperGraphStatementKind Kind = EBlueprintHelperGraphStatementKind::Unknown;
	FString PatternName;
	FString Target;
	FString Name;
	FString Property;
	FString FieldOperation;
	FString FieldScope;
	FString FunctionOperation;
	FString TransformOperation;
	FString ScheduleOperation;
	FString CreateOperation;
	FString ContainerKind;
	FString ContainerOperation;
	FString ClassPath;
	FString AssetPath;
	FString GraphLatentAllowed;
	FString ElementType;
	FString KeyType;
	FString ValueType;
	FString PinType;
	FString KeyPinType;
	FString ValuePinType;
	FString ComponentName;
	FString DelegateName;
	FString DelegateOperation;
	FString HandlerName;
	FString UnbindMode;
	FString ResultSymbolName;
	FString ResolvedCallFunctionStableId;
	FString SearchMode;
	FString AmbiguityPolicy;
	TArray<FString> CategoryPriority;
	TMap<FString, FString> ContextEvidence;
	FBlueprintHelperGraphResolvedTarget ResolvedTarget;
	TSharedPtr<FBlueprintHelperGraphExpressionIR> Value;
	TSharedPtr<FBlueprintHelperGraphExpressionIR> Condition;
	TSharedPtr<FBlueprintHelperGraphExpressionIR> TargetObject;
	TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>> Args;
	TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>> ThenStatements;
	TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>> ElseStatements;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphEntryIR
{
	FString Kind;
	FString Name;
	FString GraphName;
	FString EventTaxonomy;
	FString SourceCluster;
	FString SignatureEvidenceId;
	TMap<FString, FString> ContextEvidence;

	bool IsEmpty() const
	{
		return Kind.TrimStartAndEnd().IsEmpty()
			&& Name.TrimStartAndEnd().IsEmpty();
	}
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphSemanticIR
{
	FString Schema;
	FBlueprintHelperGraphEntryIR Entry;
	TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>> Statements;
	TMap<FString, FBlueprintHelperGraphSymbol> Symbols;
	TArray<FBlueprintHelperGraphSemanticDiagnostic> Diagnostics;

	bool HasErrors() const;
	bool TryFindSymbol(const FString& Name, FBlueprintHelperGraphSymbol& OutSymbol) const;
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphSemanticIRBuilder
{
public:
	static bool BuildFromLogicSpec(
		const TSharedPtr<FJsonObject>& LogicSpecObject,
		FBlueprintHelperGraphSemanticIR& OutIR);

	static bool BuildFromLogicSpec(
		const TSharedPtr<FJsonObject>& LogicSpecObject,
		const UBlueprint* Blueprint,
		FBlueprintHelperGraphSemanticIR& OutIR);

	static bool BuildFromLogicSpec(
		const TSharedPtr<FJsonObject>& LogicSpecObject,
		const FBlueprintHelperGraphSemanticContext& Context,
		FBlueprintHelperGraphSemanticIR& OutIR);

private:
	static TSharedPtr<FBlueprintHelperGraphStatementIR> ParseStatement(
		const TSharedPtr<FJsonObject>& StatementObject,
		const FString& Path,
		FBlueprintHelperGraphSemanticIR& OutIR);

	static void ParseStatementArray(
		const TArray<TSharedPtr<FJsonValue>>& StatementValues,
		const FString& Path,
		TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>>& OutStatements,
		FBlueprintHelperGraphSemanticIR& OutIR);

	static TSharedPtr<FBlueprintHelperGraphExpressionIR> ParseExpression(
		const TSharedPtr<FJsonValue>& ExpressionValue,
		const FString& Path,
		FBlueprintHelperGraphSemanticIR& OutIR);

	static void ParseExpressionMap(
		const TSharedPtr<FJsonObject>& Object,
		const FString& FieldName,
		const FString& Path,
		TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& OutExpressions,
		FBlueprintHelperGraphSemanticIR& OutIR);

	static void ResolveSemanticIR(
		FBlueprintHelperGraphSemanticIR& OutIR,
		const FBlueprintHelperGraphSemanticContext& Context);

	static void ResolveStatementArray(
		TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>>& Statements,
		FBlueprintHelperGraphSemanticIR& OutIR,
		const FBlueprintHelperGraphSemanticContext& Context,
		TArray<TMap<FString, FBlueprintHelperGraphSymbol>>& ScopeStack);

	static void ResolveStatement(
		const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement,
		FBlueprintHelperGraphSemanticIR& OutIR,
		const FBlueprintHelperGraphSemanticContext& Context,
		TArray<TMap<FString, FBlueprintHelperGraphSymbol>>& ScopeStack);

	static void ResolveExpression(
		const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
		FBlueprintHelperGraphSemanticIR& OutIR,
		const FBlueprintHelperGraphSemanticContext& Context,
		TArray<TMap<FString, FBlueprintHelperGraphSymbol>>& ScopeStack);
};
