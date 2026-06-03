#include "Systems/ToolClusters/BlueprintVariables/BlueprintHelperVariableReplicationService.h"

#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"

class FBlueprintHelperVariableReplicationServiceLocalUtils
{
public:
static FString NormalizeToken(const FString& Value)
{
	FString Result = Value;
	Result.TrimStartAndEndInline();
	return Result.ToLower();
}

static bool TryReadBoolField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	bool& OutValue,
	FBlueprintHelperVariableReplicationError& OutError)
{
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		return true;
	}

	if (Object->TryGetBoolField(FieldName, OutValue))
	{
		return true;
	}

	OutError.Set(
		TEXT("invalid_replication_setting"),
		FString::Printf(TEXT("%s must be boolean."), FieldName),
		FieldName);
	return false;
}

static UEdGraph* FindNotifyGraph(UBlueprint* Blueprint, const FName NotifyFunctionName)
{
	if (!Blueprint || NotifyFunctionName.IsNone())
	{
		return nullptr;
	}

	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == NotifyFunctionName)
		{
			return Graph;
		}
	}

	return FindObject<UEdGraph>(Blueprint, *NotifyFunctionName.ToString());
}

static const UEdGraph* FindNotifyGraph(const UBlueprint* Blueprint, const FName NotifyFunctionName)
{
	return FindNotifyGraph(const_cast<UBlueprint*>(Blueprint), NotifyFunctionName);
}

static const UFunction* FindFunction(const UBlueprint* Blueprint, const FName FunctionName)
{
	if (!Blueprint || FunctionName.IsNone())
	{
		return nullptr;
	}

	if (Blueprint->SkeletonGeneratedClass)
	{
		if (const UFunction* Function = Blueprint->SkeletonGeneratedClass->FindFunctionByName(FunctionName))
		{
			return Function;
		}
	}

	if (Blueprint->GeneratedClass)
	{
		if (const UFunction* Function = Blueprint->GeneratedClass->FindFunctionByName(FunctionName))
		{
			return Function;
		}
	}

	return nullptr;
}

static bool IsValidRepNotifySignature(
	const UBlueprint* Blueprint,
	const FName NotifyFunctionName,
	FBlueprintHelperVariableReplicationError& OutError)
{
	if (const UFunction* Function = FindFunction(Blueprint, NotifyFunctionName))
	{
		if (Function->NumParms != 0 || Function->GetReturnProperty() != nullptr)
		{
			OutError.Set(
				TEXT("invalid_rep_notify_function_signature"),
				FString::Printf(TEXT("RepNotify function '%s' must have no parameters and no return value."), *NotifyFunctionName.ToString()),
				TEXT("notify_function"));
			return false;
		}
	}

	return true;
}

static bool EnsureNotifyGraph(
	UBlueprint* Blueprint,
	const FName VariableName,
	const FName NotifyFunctionName,
	const bool bCreateNotifyFunction,
	const bool bReuseExistingNotifyFunction,
	FBlueprintHelperVariableReplicationError& OutError)
{
	if (!Blueprint || NotifyFunctionName.IsNone())
	{
		OutError.Set(
			TEXT("rep_notify_function_missing"),
			TEXT("rep_notify mode requires notify_function."),
			TEXT("notify_function"));
		return false;
	}

	const FName CurrentNotifyFunction = FBlueprintEditorUtils::GetBlueprintVariableRepNotifyFunc(Blueprint, VariableName);
	const bool bAssignedToSameVariable = CurrentNotifyFunction == NotifyFunctionName;
	UEdGraph* ExistingGraph = FindNotifyGraph(Blueprint, NotifyFunctionName);
	const bool bExistingFunction = ExistingGraph != nullptr || FindFunction(Blueprint, NotifyFunctionName) != nullptr;

	if (bExistingFunction && !bAssignedToSameVariable && !bReuseExistingNotifyFunction)
	{
		OutError.Set(
			TEXT("rep_notify_function_conflict"),
			FString::Printf(TEXT("RepNotify function '%s' already exists; set reuse_existing_notify_function=true to reuse it."), *NotifyFunctionName.ToString()),
			TEXT("notify_function"));
		return false;
	}

	if (bExistingFunction)
	{
		return IsValidRepNotifySignature(Blueprint, NotifyFunctionName, OutError);
	}

	if (!bCreateNotifyFunction)
	{
		OutError.Set(
			TEXT("rep_notify_function_missing"),
			FString::Printf(TEXT("RepNotify function '%s' does not exist."), *NotifyFunctionName.ToString()),
			TEXT("notify_function"));
		return false;
	}

	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		NotifyFunctionName,
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass());
	if (!NewGraph)
	{
		OutError.Set(
			TEXT("rep_notify_function_missing"),
			FString::Printf(TEXT("Failed to create RepNotify function graph '%s'."), *NotifyFunctionName.ToString()),
			TEXT("notify_function"));
		return false;
	}

	FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, NewGraph, false, nullptr);
	return true;
}

static EBlueprintHelperVariableReplicationMode ModeFromFacts(const FBlueprintHelperVariableReplicationFacts& Facts)
{
	if (Facts.Mode == TEXT("rep_notify"))
	{
		return EBlueprintHelperVariableReplicationMode::RepNotify;
	}
	if (Facts.Mode == TEXT("replicated"))
	{
		return EBlueprintHelperVariableReplicationMode::Replicated;
	}
	return EBlueprintHelperVariableReplicationMode::None;
}

static bool FactsMatchRequest(
	const FBlueprintHelperVariableReplicationFacts& Facts,
	const FBlueprintHelperVariableReplicationRequest& Request)
{
	if (ModeFromFacts(Facts) != Request.Mode)
	{
		return false;
	}
	if (Facts.Condition != FBlueprintHelperVariableReplicationService::ConditionToString(Request.Condition))
	{
		return false;
	}
	if (Request.Mode == EBlueprintHelperVariableReplicationMode::RepNotify &&
		Facts.NotifyFunctionName != Request.NotifyFunctionName.ToString())
	{
		return false;
	}
	if (Request.Mode != EBlueprintHelperVariableReplicationMode::RepNotify &&
		!Facts.NotifyFunctionName.IsEmpty())
	{
		return false;
	}
	return true;
}
};

bool FBlueprintHelperVariableReplicationService::TryStringToMode(
	const FString& Value,
	EBlueprintHelperVariableReplicationMode& OutMode)
{
	const FString Normalized = FBlueprintHelperVariableReplicationServiceLocalUtils::NormalizeToken(Value);
	if (Normalized == TEXT("none"))
	{
		OutMode = EBlueprintHelperVariableReplicationMode::None;
		return true;
	}
	if (Normalized == TEXT("replicated"))
	{
		OutMode = EBlueprintHelperVariableReplicationMode::Replicated;
		return true;
	}
	if (Normalized == TEXT("rep_notify"))
	{
		OutMode = EBlueprintHelperVariableReplicationMode::RepNotify;
		return true;
	}
	return false;
}

bool FBlueprintHelperVariableReplicationService::TryStringToCondition(
	const FString& Value,
	ELifetimeCondition& OutCondition)
{
	const FString Normalized = FBlueprintHelperVariableReplicationServiceLocalUtils::NormalizeToken(Value);
	if (Normalized == TEXT("none")) { OutCondition = COND_None; return true; }
	if (Normalized == TEXT("initial_only")) { OutCondition = COND_InitialOnly; return true; }
	if (Normalized == TEXT("owner_only")) { OutCondition = COND_OwnerOnly; return true; }
	if (Normalized == TEXT("skip_owner")) { OutCondition = COND_SkipOwner; return true; }
	if (Normalized == TEXT("simulated_only")) { OutCondition = COND_SimulatedOnly; return true; }
	if (Normalized == TEXT("autonomous_only")) { OutCondition = COND_AutonomousOnly; return true; }
	if (Normalized == TEXT("simulated_or_physics")) { OutCondition = COND_SimulatedOrPhysics; return true; }
	if (Normalized == TEXT("initial_or_owner")) { OutCondition = COND_InitialOrOwner; return true; }
	if (Normalized == TEXT("custom")) { OutCondition = COND_Custom; return true; }
	if (Normalized == TEXT("replay_or_owner")) { OutCondition = COND_ReplayOrOwner; return true; }
	if (Normalized == TEXT("replay_only")) { OutCondition = COND_ReplayOnly; return true; }
	if (Normalized == TEXT("simulated_only_no_replay")) { OutCondition = COND_SimulatedOnlyNoReplay; return true; }
	if (Normalized == TEXT("simulated_or_physics_no_replay")) { OutCondition = COND_SimulatedOrPhysicsNoReplay; return true; }
	if (Normalized == TEXT("skip_replay")) { OutCondition = COND_SkipReplay; return true; }
	return false;
}

FString FBlueprintHelperVariableReplicationService::ModeToString(
	const EBlueprintHelperVariableReplicationMode Mode)
{
	switch (Mode)
	{
	case EBlueprintHelperVariableReplicationMode::Replicated:
		return TEXT("replicated");
	case EBlueprintHelperVariableReplicationMode::RepNotify:
		return TEXT("rep_notify");
	case EBlueprintHelperVariableReplicationMode::None:
	default:
		return TEXT("none");
	}
}

FString FBlueprintHelperVariableReplicationService::ConditionToString(
	const ELifetimeCondition Condition)
{
	switch (Condition)
	{
	case COND_InitialOnly: return TEXT("initial_only");
	case COND_OwnerOnly: return TEXT("owner_only");
	case COND_SkipOwner: return TEXT("skip_owner");
	case COND_SimulatedOnly: return TEXT("simulated_only");
	case COND_AutonomousOnly: return TEXT("autonomous_only");
	case COND_SimulatedOrPhysics: return TEXT("simulated_or_physics");
	case COND_InitialOrOwner: return TEXT("initial_or_owner");
	case COND_Custom: return TEXT("custom");
	case COND_ReplayOrOwner: return TEXT("replay_or_owner");
	case COND_ReplayOnly: return TEXT("replay_only");
	case COND_SimulatedOnlyNoReplay: return TEXT("simulated_only_no_replay");
	case COND_SimulatedOrPhysicsNoReplay: return TEXT("simulated_or_physics_no_replay");
	case COND_SkipReplay: return TEXT("skip_replay");
	case COND_None:
	default:
		return TEXT("none");
	}
}

FString FBlueprintHelperVariableReplicationService::ConditionToEngineName(
	const ELifetimeCondition Condition)
{
	switch (Condition)
	{
	case COND_InitialOnly: return TEXT("COND_InitialOnly");
	case COND_OwnerOnly: return TEXT("COND_OwnerOnly");
	case COND_SkipOwner: return TEXT("COND_SkipOwner");
	case COND_SimulatedOnly: return TEXT("COND_SimulatedOnly");
	case COND_AutonomousOnly: return TEXT("COND_AutonomousOnly");
	case COND_SimulatedOrPhysics: return TEXT("COND_SimulatedOrPhysics");
	case COND_InitialOrOwner: return TEXT("COND_InitialOrOwner");
	case COND_Custom: return TEXT("COND_Custom");
	case COND_ReplayOrOwner: return TEXT("COND_ReplayOrOwner");
	case COND_ReplayOnly: return TEXT("COND_ReplayOnly");
	case COND_SimulatedOnlyNoReplay: return TEXT("COND_SimulatedOnlyNoReplay");
	case COND_SimulatedOrPhysicsNoReplay: return TEXT("COND_SimulatedOrPhysicsNoReplay");
	case COND_SkipReplay: return TEXT("COND_SkipReplay");
	case COND_None:
	default:
		return TEXT("COND_None");
	}
}

bool FBlueprintHelperVariableReplicationService::TryParseRequest(
	const TSharedPtr<FJsonObject>& ValueObject,
	const FName VariableName,
	FBlueprintHelperVariableReplicationRequest& OutRequest,
	FBlueprintHelperVariableReplicationError& OutError)
{
	OutRequest = {};
	if (!ValueObject.IsValid())
	{
		OutError.Set(
			TEXT("invalid_replication_setting"),
			TEXT("replication value must be an object."),
			TEXT("value"));
		return false;
	}

	FString ModeString;
	if (!ValueObject->TryGetStringField(TEXT("mode"), ModeString) ||
		!TryStringToMode(ModeString, OutRequest.Mode))
	{
		OutError.Set(
			TEXT("invalid_replication_mode"),
			TEXT("replication.mode must be one of none, replicated, or rep_notify."),
			TEXT("value.mode"));
		return false;
	}

	FString ConditionString;
	if (!ValueObject->TryGetStringField(TEXT("condition"), ConditionString) || ConditionString.IsEmpty())
	{
		ConditionString = TEXT("none");
	}
	if (!TryStringToCondition(ConditionString, OutRequest.Condition))
	{
		OutError.Set(
			TEXT("invalid_replication_condition"),
			TEXT("replication.condition must be a public UE editor-facing condition."),
			TEXT("value.condition"));
		return false;
	}
	if (OutRequest.Mode == EBlueprintHelperVariableReplicationMode::None &&
		OutRequest.Condition != COND_None)
	{
		OutError.Set(
			TEXT("replication_condition_requires_networked_mode"),
			TEXT("replication.condition is accepted only for replicated and rep_notify modes."),
			TEXT("value.condition"));
		return false;
	}

	FString NotifyFunctionString;
	ValueObject->TryGetStringField(TEXT("notify_function"), NotifyFunctionString);
	NotifyFunctionString.TrimStartAndEndInline();
	if (OutRequest.Mode == EBlueprintHelperVariableReplicationMode::RepNotify)
	{
		if (NotifyFunctionString.IsEmpty())
		{
			NotifyFunctionString = FString::Printf(TEXT("OnRep_%s"), *VariableName.ToString());
		}
		OutRequest.NotifyFunctionName = FName(*NotifyFunctionString);
		if (OutRequest.NotifyFunctionName.IsNone())
		{
			OutError.Set(
				TEXT("rep_notify_function_missing"),
				TEXT("rep_notify mode requires notify_function."),
				TEXT("value.notify_function"));
			return false;
		}
	}

	if (!FBlueprintHelperVariableReplicationServiceLocalUtils::TryReadBoolField(
		ValueObject,
		TEXT("create_notify_function"),
		OutRequest.bCreateNotifyFunction,
		OutError))
	{
		OutError.Field = TEXT("value.") + OutError.Field;
		return false;
	}
	if (!FBlueprintHelperVariableReplicationServiceLocalUtils::TryReadBoolField(
		ValueObject,
		TEXT("reuse_existing_notify_function"),
		OutRequest.bReuseExistingNotifyFunction,
		OutError))
	{
		OutError.Field = TEXT("value.") + OutError.Field;
		return false;
	}

	return true;
}

bool FBlueprintHelperVariableReplicationService::ApplyToMemberVariable(
	UBlueprint* Blueprint,
	const FName VariableName,
	FBPVariableDescription& Variable,
	const FBlueprintHelperVariableReplicationRequest& Request,
	bool& bOutChanged,
	FBlueprintHelperVariableReplicationError& OutError)
{
	bOutChanged = false;
	if (!Blueprint)
	{
		OutError.Set(TEXT("invalid_request"), TEXT("Blueprint asset could not be resolved."), TEXT("asset_path"));
		return false;
	}

	if (Request.Mode == EBlueprintHelperVariableReplicationMode::RepNotify)
	{
		if (!FBlueprintHelperVariableReplicationServiceLocalUtils::EnsureNotifyGraph(
			Blueprint,
			VariableName,
			Request.NotifyFunctionName,
			Request.bCreateNotifyFunction,
			Request.bReuseExistingNotifyFunction,
			OutError))
		{
			OutError.Field = TEXT("value.") + OutError.Field;
			return false;
		}
	}

	const FBlueprintHelperVariableReplicationFacts Before = ReadMemberVariableFacts(*Blueprint, Variable);
	if (FBlueprintHelperVariableReplicationServiceLocalUtils::FactsMatchRequest(Before, Request))
	{
		return true;
	}

	Blueprint->Modify();
	if (Request.Mode == EBlueprintHelperVariableReplicationMode::None)
	{
		Variable.PropertyFlags &= ~CPF_Net;
		Variable.PropertyFlags &= ~CPF_RepNotify;
		FBlueprintEditorUtils::SetBlueprintVariableRepNotifyFunc(Blueprint, VariableName, NAME_None);
		Variable.ReplicationCondition = COND_None;
	}
	else if (Request.Mode == EBlueprintHelperVariableReplicationMode::Replicated)
	{
		Variable.PropertyFlags |= CPF_Net;
		Variable.PropertyFlags &= ~CPF_RepNotify;
		FBlueprintEditorUtils::SetBlueprintVariableRepNotifyFunc(Blueprint, VariableName, NAME_None);
		Variable.ReplicationCondition = Request.Condition;
	}
	else
	{
		Variable.PropertyFlags |= CPF_Net;
		Variable.PropertyFlags |= CPF_RepNotify;
		FBlueprintEditorUtils::SetBlueprintVariableRepNotifyFunc(Blueprint, VariableName, Request.NotifyFunctionName);
		Variable.ReplicationCondition = Request.Condition;
	}

	bOutChanged = true;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return true;
}

FBlueprintHelperVariableReplicationFacts FBlueprintHelperVariableReplicationService::ReadMemberVariableFacts(
	const UBlueprint& Blueprint,
	const FBPVariableDescription& Variable)
{
	bool bIsNetworked = (Variable.PropertyFlags & CPF_Net) != 0;
	bool bHasRepNotifyFlag = (Variable.PropertyFlags & CPF_RepNotify) != 0;
	FName NotifyFunctionName =
		FBlueprintEditorUtils::GetBlueprintVariableRepNotifyFunc(
			const_cast<UBlueprint*>(&Blueprint),
			Variable.VarName);
	if (NotifyFunctionName.IsNone())
	{
		NotifyFunctionName = Variable.RepNotifyFunc;
	}
	ELifetimeCondition Condition = static_cast<ELifetimeCondition>(Variable.ReplicationCondition.GetValue());

	FBlueprintHelperVariableReplicationFacts Facts;
	Facts.Condition = bIsNetworked ? ConditionToString(Condition) : TEXT("none");
	Facts.ConditionEngineName = bIsNetworked ? ConditionToEngineName(Condition) : TEXT("COND_None");
	Facts.NotifyFunctionName = NotifyFunctionName.IsNone() ? FString() : NotifyFunctionName.ToString();
	Facts.bNotifyGraphExists =
		!NotifyFunctionName.IsNone() &&
		FBlueprintHelperVariableReplicationServiceLocalUtils::FindNotifyGraph(&Blueprint, NotifyFunctionName) != nullptr;

	if (!bIsNetworked)
	{
		Facts.Mode = TEXT("none");
		Facts.NotifyFunctionName.Empty();
		Facts.bNotifyGraphExists = false;
	}
	else if (bHasRepNotifyFlag && !NotifyFunctionName.IsNone())
	{
		Facts.Mode = TEXT("rep_notify");
	}
	else
	{
		Facts.Mode = TEXT("replicated");
		Facts.NotifyFunctionName.Empty();
		Facts.bNotifyGraphExists = false;
	}

	return Facts;
}
