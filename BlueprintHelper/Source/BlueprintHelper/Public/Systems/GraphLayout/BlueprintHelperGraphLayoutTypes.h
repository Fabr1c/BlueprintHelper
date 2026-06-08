#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace BlueprintHelper::GraphLayout
{
inline constexpr const TCHAR* RuleSetSchemaV1 = TEXT("BlueprintHelper.GraphLayoutRuleSet.v1");

enum class ENodeRole : uint8
{
	Unknown,
	EventEntry,
	ExecNode,
	BranchControl,
	PureFunction,
	OperatorOrCompare,
	VariableInput,
	AsyncNode,
	DelegateNode,
	Comment
};

enum class EPinDirection : uint8
{
	Input,
	Output
};

enum class ESemanticScene : uint8
{
	LinearExecChain,
	PureDataSubgraph,
	NodeInputCluster,
	MultiExecOutput,
	Occupancy
};

struct FValidationResult
{
	bool bValid = true;
	TArray<FString> Errors;
	TArray<FString> Warnings;

	void AddError(const FString& Message);
};

struct FRoleRule
{
	FString Id;
	FString Color = TEXT("gray");
	int32 Priority = 0;
	TArray<FString> MatchClassContains;
	TArray<FString> MatchTitleContains;
	bool bHasExecPinMatcher = false;
	bool bMatchHasExecPin = false;
	ENodeRole Role = ENodeRole::Unknown;
};

struct FEditorCanvasSceneState
{
	TMap<ENodeRole, FVector2D> RoleCenters;
};

struct FRuleSet
{
	FRuleSet();

	FString Schema = RuleSetSchemaV1;
	FString Id = TEXT("default_readable_exec_with_left_data");
	FString DisplayName = TEXT("默认可读执行与左侧数据");
	int32 Version = 1;
	float ExecColumnSpacing = 360.0f;
	float ExecRowSpacing = 220.0f;
	float BranchRowSpacing = 260.0f;
	float PureInputOffsetX = 300.0f;
	float VariableInputOffsetX = 260.0f;
	float InputPinRowSpacing = 44.0f;
	bool bAlignExecNodesHorizontally = true;
	bool bUsePureDataSubgraphLayout = true;
	bool bUsePatternRowHeightBudget = true;
	float DataClusterPaddingX = 40.0f;
	float DataClusterPaddingY = 40.0f;
	float BranchRowPaddingY = 80.0f;
	float CollisionPaddingX = 60.0f;
	float CollisionPaddingY = 40.0f;
	float CollisionStepY = 64.0f;
	int32 MaxCollisionAttempts = 64;
	bool bUseTargetPinOrderForVariableInputs = true;
	bool bMoveGeneratedNodes = true;
	bool bMoveExistingNodes = false;
	int32 MaxNodesPerFrame = 24;
	float MaxMillisecondsPerFrame = 2.0f;
	bool bMarkDirtyAfterApply = true;
	bool bSaveAfterApply = false;
	TArray<FRoleRule> RoleRules;
	TMap<ESemanticScene, FEditorCanvasSceneState> EditorCanvasScenes;
};

struct FPinSnapshot
{
	FString PinId;
	FString Name;
	EPinDirection Direction = EPinDirection::Input;
	FString Category;
	bool bExec = false;
	TArray<FString> LinkedNodeIds;
};

struct FNodeSnapshot
{
	FString NodeId;
	FString StableName;
	FString ClassPath;
	FString Title;
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Size = FVector2D(180.0f, 80.0f);
	bool bExisting = true;
	FString LayoutBlockId;
	int32 LayoutBlockOrder = INDEX_NONE;
	int32 LayoutNodeOrder = INDEX_NONE;
	TArray<FPinSnapshot> Pins;
};

struct FGraphSnapshot
{
	FString GraphName;
	TArray<FNodeSnapshot> Nodes;
};

struct FNodeClassification
{
	FString NodeId;
	ENodeRole Role = ENodeRole::Unknown;
	FString Reason;
};

struct FNodePlacement
{
	FString NodeId;
	ENodeRole Role = ENodeRole::Unknown;
	FVector2D CurrentPosition = FVector2D::ZeroVector;
	FVector2D TargetPosition = FVector2D::ZeroVector;
	FVector2D TargetSize = FVector2D::ZeroVector;
	bool bMoveExisting = false;
	FString Reason;
};

struct FLayoutPlan
{
	FString Schema = TEXT("BlueprintHelper.GraphLayoutPlan.v1");
	TArray<FNodeClassification> Classifications;
	TArray<FNodePlacement> Placements;
	TArray<FString> Issues;
};

BLUEPRINTHELPER_API const TCHAR* ToString(ENodeRole Role);
BLUEPRINTHELPER_API bool LexTryParseString(ENodeRole& OutRole, const FString& Value);
BLUEPRINTHELPER_API const TCHAR* ToString(EPinDirection Direction);
BLUEPRINTHELPER_API const TCHAR* ToString(ESemanticScene Scene);
BLUEPRINTHELPER_API bool LexTryParseString(ESemanticScene& OutScene, const FString& Value);

BLUEPRINTHELPER_API TSharedRef<FJsonObject> ToJson(const FRuleSet& RuleSet);
BLUEPRINTHELPER_API TSharedRef<FJsonObject> ToJson(const FLayoutPlan& Plan);
}
