#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

#include <initializer_list>

namespace BlueprintHelper::GraphLayout
{
static FRoleRule MakeClassRoleRule(
	const FString& Id,
	ENodeRole Role,
	const FString& Color,
	int32 Priority,
	std::initializer_list<const TCHAR*> ClassTokens)
{
	FRoleRule Rule;
	Rule.Id = Id;
	Rule.Role = Role;
	Rule.Color = Color;
	Rule.Priority = Priority;
	for (const TCHAR* ClassToken : ClassTokens)
	{
		Rule.MatchClassContains.Add(ClassToken);
	}
	return Rule;
}

FRuleSet::FRuleSet()
{
	RoleRules.Add(MakeClassRoleRule(TEXT("EventEntry"), ENodeRole::EventEntry, TEXT("purple"), 100, {
		TEXT("K2Node_CustomEvent"),
		TEXT("K2Node_Event"),
		TEXT("K2Node_InputAction")
	}));
	RoleRules.Add(MakeClassRoleRule(TEXT("BranchControl"), ENodeRole::BranchControl, TEXT("orange"), 90, {
		TEXT("K2Node_IfThenElse"),
		TEXT("K2Node_Switch"),
		TEXT("K2Node_ExecutionSequence"),
		TEXT("K2Node_MultiGate"),
		TEXT("K2Node_DoOnce"),
		TEXT("K2Node_ForEachElementInEnum")
	}));
	{
		FRoleRule Rule;
		Rule.Id = TEXT("BranchControlTitle");
		Rule.Role = ENodeRole::BranchControl;
		Rule.Color = TEXT("orange");
		Rule.Priority = 89;
		Rule.MatchTitleContains.Append({
			TEXT("Branch"),
			TEXT("Switch"),
			TEXT("Sequence"),
			TEXT("Gate"),
			TEXT("DoOnce"),
			TEXT("ForLoop"),
			TEXT("ForEach"),
			TEXT("WhileLoop")
		});
		RoleRules.Add(Rule);
	}
	RoleRules.Add(MakeClassRoleRule(TEXT("AsyncNode"), ENodeRole::AsyncNode, TEXT("cyan"), 80, {
		TEXT("Async"),
		TEXT("Latent"),
		TEXT("Delay")
	}));
	{
		FRoleRule Rule;
		Rule.Id = TEXT("AsyncNodeTitle");
		Rule.Role = ENodeRole::AsyncNode;
		Rule.Color = TEXT("cyan");
		Rule.Priority = 79;
		Rule.MatchTitleContains.Append({
			TEXT("Delay"),
			TEXT("Async")
		});
		RoleRules.Add(Rule);
	}
	RoleRules.Add(MakeClassRoleRule(TEXT("DelegateNode"), ENodeRole::DelegateNode, TEXT("yellow"), 70, {
		TEXT("Delegate"),
		TEXT("EventDispatcher")
	}));
	{
		FRoleRule Rule;
		Rule.Id = TEXT("ExecNode");
		Rule.Role = ENodeRole::ExecNode;
		Rule.Color = TEXT("red");
		Rule.Priority = 50;
		Rule.bHasExecPinMatcher = true;
		Rule.bMatchHasExecPin = true;
		RoleRules.Add(Rule);
	}
	RoleRules.Add(MakeClassRoleRule(TEXT("PureFunction"), ENodeRole::PureFunction, TEXT("green"), 40, {
		TEXT("K2Node_CallFunction"),
		TEXT("K2Node_Select"),
		TEXT("K2Node_FormatText"),
		TEXT("K2Node_Make")
	}));
	RoleRules.Add(MakeClassRoleRule(TEXT("OperatorOrCompare"), ENodeRole::OperatorOrCompare, TEXT("lime"), 45, {
		TEXT("K2Node_CommutativeAssociativeBinaryOperator"),
		TEXT("K2Node_PromotableOperator")
	}));
	{
		FRoleRule Rule;
		Rule.Id = TEXT("OperatorOrCompareTitle");
		Rule.Role = ENodeRole::OperatorOrCompare;
		Rule.Color = TEXT("lime");
		Rule.Priority = 44;
		Rule.MatchTitleContains.Append({
			TEXT("=="),
			TEXT("!="),
			TEXT(">"),
			TEXT("<"),
			TEXT("+"),
			TEXT("-"),
			TEXT("*"),
			TEXT("/"),
			TEXT("AND"),
			TEXT("OR"),
			TEXT("NOT"),
			TEXT("Equal"),
			TEXT("Greater"),
			TEXT("Less"),
			TEXT("Is Valid"),
			TEXT("IsValid")
		});
		RoleRules.Add(Rule);
	}
	RoleRules.Add(MakeClassRoleRule(TEXT("VariableInput"), ENodeRole::VariableInput, TEXT("blue"), 30, {
		TEXT("K2Node_VariableGet"),
		TEXT("K2Node_Self"),
		TEXT("K2Node_Literal")
	}));
	RoleRules.Add(MakeClassRoleRule(TEXT("Comment"), ENodeRole::Comment, TEXT("gray"), 10, {
		TEXT("EdGraphNode_Comment")
	}));
	RoleRules.Sort([](const FRoleRule& Left, const FRoleRule& Right)
	{
		return Left.Priority > Right.Priority;
	});
}

void FValidationResult::AddError(const FString& Message)
{
	bValid = false;
	Errors.Add(Message);
}

const TCHAR* ToString(ENodeRole Role)
{
	switch (Role)
	{
	case ENodeRole::EventEntry: return TEXT("EventEntry");
	case ENodeRole::ExecNode: return TEXT("ExecNode");
	case ENodeRole::BranchControl: return TEXT("BranchControl");
	case ENodeRole::PureFunction: return TEXT("PureFunction");
	case ENodeRole::OperatorOrCompare: return TEXT("OperatorOrCompare");
	case ENodeRole::VariableInput: return TEXT("VariableInput");
	case ENodeRole::AsyncNode: return TEXT("AsyncNode");
	case ENodeRole::DelegateNode: return TEXT("DelegateNode");
	case ENodeRole::Comment: return TEXT("Comment");
	default: return TEXT("Unknown");
	}
}

bool LexTryParseString(ENodeRole& OutRole, const FString& Value)
{
	if (Value.Equals(TEXT("EventEntry"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("event_entry"), ESearchCase::IgnoreCase))
	{
		OutRole = ENodeRole::EventEntry;
		return true;
	}
	if (Value.Equals(TEXT("ExecNode"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("exec_node"), ESearchCase::IgnoreCase))
	{
		OutRole = ENodeRole::ExecNode;
		return true;
	}
	if (Value.Equals(TEXT("BranchControl"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("branch_control"), ESearchCase::IgnoreCase))
	{
		OutRole = ENodeRole::BranchControl;
		return true;
	}
	if (Value.Equals(TEXT("PureFunction"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("pure_function"), ESearchCase::IgnoreCase))
	{
		OutRole = ENodeRole::PureFunction;
		return true;
	}
	if (Value.Equals(TEXT("OperatorOrCompare"), ESearchCase::IgnoreCase) ||
		Value.Equals(TEXT("operator_or_compare"), ESearchCase::IgnoreCase) ||
		Value.Equals(TEXT("Operator"), ESearchCase::IgnoreCase) ||
		Value.Equals(TEXT("Compare"), ESearchCase::IgnoreCase))
	{
		OutRole = ENodeRole::OperatorOrCompare;
		return true;
	}
	if (Value.Equals(TEXT("VariableInput"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("variable_input"), ESearchCase::IgnoreCase))
	{
		OutRole = ENodeRole::VariableInput;
		return true;
	}
	if (Value.Equals(TEXT("AsyncNode"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("async_node"), ESearchCase::IgnoreCase))
	{
		OutRole = ENodeRole::AsyncNode;
		return true;
	}
	if (Value.Equals(TEXT("DelegateNode"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("delegate_node"), ESearchCase::IgnoreCase))
	{
		OutRole = ENodeRole::DelegateNode;
		return true;
	}
	if (Value.Equals(TEXT("Comment"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("comment"), ESearchCase::IgnoreCase))
	{
		OutRole = ENodeRole::Comment;
		return true;
	}
	if (Value.Equals(TEXT("Unknown"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("unknown"), ESearchCase::IgnoreCase))
	{
		OutRole = ENodeRole::Unknown;
		return true;
	}
	return false;
}

const TCHAR* ToString(EPinDirection Direction)
{
	return Direction == EPinDirection::Output ? TEXT("output") : TEXT("input");
}

static TSharedRef<FJsonObject> VectorToJson(const FVector2D& Value)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("x"), Value.X);
	Json->SetNumberField(TEXT("y"), Value.Y);
	return Json;
}

TSharedRef<FJsonObject> ToJson(const FRuleSet& RuleSet)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), RuleSet.Schema);
	Json->SetStringField(TEXT("id"), RuleSet.Id);
	Json->SetStringField(TEXT("display_name"), RuleSet.DisplayName);
	Json->SetNumberField(TEXT("version"), RuleSet.Version);
	Json->SetNumberField(TEXT("exec_column_spacing"), RuleSet.ExecColumnSpacing);
	Json->SetNumberField(TEXT("exec_row_spacing"), RuleSet.ExecRowSpacing);
	Json->SetNumberField(TEXT("branch_row_spacing"), RuleSet.BranchRowSpacing);
	Json->SetNumberField(TEXT("pure_input_offset_x"), RuleSet.PureInputOffsetX);
	Json->SetNumberField(TEXT("variable_input_offset_x"), RuleSet.VariableInputOffsetX);
	Json->SetNumberField(TEXT("input_pin_row_spacing"), RuleSet.InputPinRowSpacing);
	Json->SetBoolField(TEXT("target_pin_order_variable_input_alignment"), RuleSet.bUseTargetPinOrderForVariableInputs);
	Json->SetBoolField(TEXT("move_generated_nodes"), RuleSet.bMoveGeneratedNodes);
	Json->SetBoolField(TEXT("move_existing_nodes"), RuleSet.bMoveExistingNodes);
	Json->SetNumberField(TEXT("max_nodes_per_frame"), RuleSet.MaxNodesPerFrame);
	Json->SetNumberField(TEXT("max_ms_per_frame"), RuleSet.MaxMillisecondsPerFrame);
	Json->SetBoolField(TEXT("mark_dirty_after_apply"), RuleSet.bMarkDirtyAfterApply);
	Json->SetBoolField(TEXT("save_after_apply"), RuleSet.bSaveAfterApply);

	if (RuleSet.EditorCanvasRoleCenters.Num() > 0)
	{
		TSharedRef<FJsonObject> EditorCanvasJson = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> RoleCentersJson = MakeShared<FJsonObject>();
		for (const TPair<ENodeRole, FVector2D>& Pair : RuleSet.EditorCanvasRoleCenters)
		{
			if (Pair.Key != ENodeRole::Unknown)
			{
				RoleCentersJson->SetObjectField(ToString(Pair.Key), VectorToJson(Pair.Value));
			}
		}
		EditorCanvasJson->SetObjectField(TEXT("role_centers"), RoleCentersJson);
		Json->SetObjectField(TEXT("editor_canvas"), EditorCanvasJson);
	}

	TArray<TSharedPtr<FJsonValue>> RulesJson;
	for (const FRoleRule& Rule : RuleSet.RoleRules)
	{
		TSharedRef<FJsonObject> RuleJson = MakeShared<FJsonObject>();
		if (!Rule.Id.IsEmpty())
		{
			RuleJson->SetStringField(TEXT("id"), Rule.Id);
		}
		RuleJson->SetStringField(TEXT("color"), Rule.Color);
		RuleJson->SetNumberField(TEXT("priority"), Rule.Priority);
		if (Rule.MatchClassContains.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> ClassTokens;
			for (const FString& Token : Rule.MatchClassContains)
			{
				ClassTokens.Add(MakeShared<FJsonValueString>(Token));
			}
			RuleJson->SetArrayField(TEXT("match_class_contains"), ClassTokens);
		}
		if (Rule.MatchTitleContains.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> TitleTokens;
			for (const FString& Token : Rule.MatchTitleContains)
			{
				TitleTokens.Add(MakeShared<FJsonValueString>(Token));
			}
			RuleJson->SetArrayField(TEXT("match_title_contains"), TitleTokens);
		}
		if (Rule.bHasExecPinMatcher)
		{
			RuleJson->SetBoolField(TEXT("match_has_exec_pin"), Rule.bMatchHasExecPin);
		}
		RuleJson->SetStringField(TEXT("role"), ToString(Rule.Role));
		RulesJson.Add(MakeShared<FJsonValueObject>(RuleJson));
	}
	Json->SetArrayField(TEXT("role_rules"), RulesJson);
	return Json;
}

TSharedRef<FJsonObject> ToJson(const FLayoutPlan& Plan)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), Plan.Schema);

	TArray<TSharedPtr<FJsonValue>> Classifications;
	for (const FNodeClassification& Classification : Plan.Classifications)
	{
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("node_id"), Classification.NodeId);
		Item->SetStringField(TEXT("role"), ToString(Classification.Role));
		Item->SetStringField(TEXT("reason"), Classification.Reason);
		Classifications.Add(MakeShared<FJsonValueObject>(Item));
	}
	Json->SetArrayField(TEXT("classifications"), Classifications);

	TArray<TSharedPtr<FJsonValue>> Placements;
	for (const FNodePlacement& Placement : Plan.Placements)
	{
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("node_id"), Placement.NodeId);
		Item->SetStringField(TEXT("role"), ToString(Placement.Role));
		Item->SetObjectField(TEXT("current_position"), VectorToJson(Placement.CurrentPosition));
		Item->SetObjectField(TEXT("target_position"), VectorToJson(Placement.TargetPosition));
		Item->SetBoolField(TEXT("move_existing"), Placement.bMoveExisting);
		Item->SetStringField(TEXT("reason"), Placement.Reason);
		Placements.Add(MakeShared<FJsonValueObject>(Item));
	}
	Json->SetArrayField(TEXT("placements"), Placements);

	TArray<TSharedPtr<FJsonValue>> Issues;
	for (const FString& Issue : Plan.Issues)
	{
		Issues.Add(MakeShared<FJsonValueString>(Issue));
	}
	Json->SetArrayField(TEXT("issues"), Issues);
	return Json;
}
}
