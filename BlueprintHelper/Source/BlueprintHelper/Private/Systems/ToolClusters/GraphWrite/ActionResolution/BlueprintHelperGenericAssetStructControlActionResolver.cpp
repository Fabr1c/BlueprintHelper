#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.h"

#include "BlueprintNodeSpawner.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSingletonControlFlowEvidenceProvider.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_MultiGate.h"
#include "K2Node_SwitchEnum.h"
#include "K2Node_SwitchInteger.h"
#include "K2Node_SwitchName.h"
#include "K2Node_SwitchString.h"
#include "UObject/WeakObjectPtr.h"

namespace
{
static bool IsGenericNodeSpawnerSemantic(const EBlueprintHelperActionSemanticKind Kind)
{
	return Kind == EBlueprintHelperActionSemanticKind::Select
		|| Kind == EBlueprintHelperActionSemanticKind::Control;
}

static FString Clean(const FString& Value)
{
	return Value.TrimStartAndEnd();
}

static FString NormalizeOperation(const FString& Value)
{
	return Clean(Value).ToLower();
}

static FString EvidenceValue(const FBlueprintHelperActionResolutionRequest& Request, const TCHAR* Key)
{
	if (const FString* Value = Request.ContextEvidence.Find(Key))
	{
		return Clean(*Value);
	}
	if (const FString* Value = Request.Semantic.DefaultValues.Find(Key))
	{
		return Clean(*Value);
	}
	return FString();
}

static FString ResolveControlOperation(const FBlueprintHelperActionResolutionRequest& Request)
{
	const FString EvidenceOperation = EvidenceValue(Request, TEXT("generic.control.operation"));
	return NormalizeOperation(EvidenceOperation.IsEmpty() ? Request.Semantic.Query : EvidenceOperation);
}

static FBlueprintHelperActionResolutionResult MakeUnsupportedGenericSemanticResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("unsupported_generic_node_spawner_candidate_semantic");
	Result.Message = Message;
	return Result;
}

static FBlueprintHelperActionResolutionResult MakeInvalidGenericNodeSpawnerResult(
	const FString& ErrorCode,
	const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = ErrorCode;
	Result.Message = Message;
	return Result;
}

static FBlueprintHelperActionResolutionResult MakeBlockedGenericNodeSpawnerResult(
	const FString& ErrorCode,
	const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::Blocked;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = ErrorCode;
	Result.Message = Message;
	return Result;
}

static TArray<FString> ParseDelimitedEvidenceList(const FString& Value)
{
	TArray<FString> Result;
	Value.ParseIntoArray(Result, TEXT(","), true);
	for (FString& Entry : Result)
	{
		Entry = Entry.TrimStartAndEnd();
	}
	Result.RemoveAll([](const FString& Entry)
	{
		return Entry.IsEmpty();
	});
	return Result;
}

static void AddGenericOpsReadbackFacts(
	FBlueprintHelperCallFunctionCandidateInfo& Candidate,
	const FString& Family,
	const FString& Operation,
	const TMap<FString, FString>& ExtraFacts)
{
	Candidate.ReadbackFacts.Add(TEXT("generic.family"), Family);
	Candidate.ReadbackFacts.Add(TEXT("generic.operation_id"), FString::Printf(TEXT("generic_ops.%s.%s"), *Family, *Operation));
	Candidate.ReadbackFacts.Add(TEXT("generic.operation"), Operation);
	Candidate.ReadbackFacts.Add(TEXT("generic.wildcard_residual"), TEXT("false"));
	for (const TPair<FString, FString>& Fact : ExtraFacts)
	{
		if (!Fact.Key.IsEmpty() && !Fact.Value.TrimStartAndEnd().IsEmpty())
		{
			Candidate.ReadbackFacts.Add(Fact.Key, Fact.Value.TrimStartAndEnd());
		}
	}
}

static FBlueprintHelperActionResolutionResult MakeResolvedGenericNodeSpawnerResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& Family,
	const FString& Operation,
	const FString& StableEvidence,
	TSubclassOf<UEdGraphNode> NodeClass,
	UBlueprintNodeSpawner* Spawner,
	const FString& Category,
	const FString& MatchReason,
	const TMap<FString, FString>& ExtraFacts)
{
	if (!NodeClass)
	{
		return MakeBlockedGenericNodeSpawnerResult(
			TEXT("generic_node_class_unavailable"),
			FString::Printf(TEXT("GenericOps operation '%s' does not have a loadable node class."), *Operation));
	}
	if (!Spawner)
	{
		return MakeBlockedGenericNodeSpawnerResult(
			TEXT("generic_node_spawner_unavailable"),
			FString::Printf(TEXT("GenericOps operation '%s' could not create a UBlueprintNodeSpawner."), *Operation));
	}

	const FString StableId = FString::Printf(
		TEXT("generic_ops:%s:%s:%s"),
		*Family,
		*Operation,
		StableEvidence.IsEmpty() ? TEXT("default") : *StableEvidence);

	FBlueprintHelperCallFunctionCandidateInfo Candidate;
	Candidate.StableId = StableId;
	Candidate.DisplayName = FString::Printf(TEXT("GenericOps %s"), *Operation);
	Candidate.Category = Category;
	Candidate.NodeClassPath = NodeClass->GetPathName();
	Candidate.MatchReason = MatchReason;
	Candidate.Score = 100;
	Candidate.bGraphCompatible = Request.TargetGraph != nullptr;
	Candidate.bFromActionDatabase = false;
	Candidate.bBlueprintCallable = true;
	AddGenericOpsReadbackFacts(Candidate, Family, Operation, ExtraFacts);

	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.SelectedStableId = StableId;
	Result.SelectedSpawner = Spawner;
	Result.CandidateActions.Add(MoveTemp(Candidate));
	Result.SpawnerClass = Spawner->GetClass()->GetPathName();
	Result.NodeClass = NodeClass->GetPathName();
	Result.MatchReason = MatchReason;
	return Result;
}

static UBlueprintNodeSpawner* CreateSwitchEnumSpawner(UEnum* Enum)
{
	if (!Enum)
	{
		return nullptr;
	}

	TWeakObjectPtr<UEnum> EnumPtr = Enum;
	UBlueprintNodeSpawner::FCustomizeNodeDelegate CustomizeSwitchEnum =
		UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateLambda(
			[EnumPtr](UEdGraphNode* NewNode, bool)
			{
				UK2Node_SwitchEnum* SwitchNode = Cast<UK2Node_SwitchEnum>(NewNode);
				if (SwitchNode && EnumPtr.IsValid())
				{
					SwitchNode->SetEnum(EnumPtr.Get());
				}
			});
	return UBlueprintNodeSpawner::Create(UK2Node_SwitchEnum::StaticClass(), nullptr, CustomizeSwitchEnum);
}

static UBlueprintNodeSpawner* CreateGenericControlSpawner(TSubclassOf<UEdGraphNode> ResolvedNodeClass)
{
	return ResolvedNodeClass ? UBlueprintNodeSpawner::Create(ResolvedNodeClass) : nullptr;
}

static UBlueprintNodeSpawner* CreateMacroInstanceSpawner(UEdGraph* MacroGraph)
{
	if (!MacroGraph)
	{
		return nullptr;
	}

	TWeakObjectPtr<UEdGraph> MacroGraphPtr = MacroGraph;
	UBlueprintNodeSpawner::FCustomizeNodeDelegate CustomizeMacro =
		UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateLambda(
			[MacroGraphPtr](UEdGraphNode* NewNode, bool)
			{
				UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(NewNode);
				if (MacroNode && MacroGraphPtr.IsValid())
				{
					MacroNode->SetMacroGraph(MacroGraphPtr.Get());
				}
			});
	return UBlueprintNodeSpawner::Create(UK2Node_MacroInstance::StaticClass(), nullptr, CustomizeMacro);
}

static UEnum* ResolveEnumEvidence(const FBlueprintHelperActionResolutionRequest& Request)
{
	const FString EnumPath = EvidenceValue(Request, TEXT("generic.control.enum_path"));
	if (EnumPath.IsEmpty())
	{
		return nullptr;
	}
	if (UEnum* ExistingEnum = FindObject<UEnum>(nullptr, *EnumPath))
	{
		return ExistingEnum;
	}
	return LoadObject<UEnum>(nullptr, *EnumPath);
}

static UEdGraph* ResolveMacroGraphEvidence(const FBlueprintHelperActionResolutionRequest& Request)
{
	const FString GraphPath = EvidenceValue(Request, TEXT("generic.macro.graph_path"));
	if (GraphPath.IsEmpty())
	{
		return nullptr;
	}
	if (UEdGraph* ExistingGraph = FindObject<UEdGraph>(nullptr, *GraphPath))
	{
		return ExistingGraph;
	}
	return LoadObject<UEdGraph>(nullptr, *GraphPath);
}

static FBlueprintHelperActionResolutionResult ResolveDedicatedControlFlowNodeSpawner(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	const FString Operation = ResolveControlOperation(Request);
	const FString CaseValues = EvidenceValue(Request, TEXT("generic.control.case_values"));
	const FString DynamicOutputCount = EvidenceValue(Request, TEXT("generic.control.dynamic_output_count"));

	TSubclassOf<UEdGraphNode> NodeClass = nullptr;
	UBlueprintNodeSpawner* Spawner = nullptr;
	TMap<FString, FString> Facts;
	Facts.Add(TEXT("generic.control.operation"), Operation);

	if (Operation == TEXT("switch_int"))
	{
		if (ParseDelimitedEvidenceList(CaseValues).Num() == 0)
		{
			return MakeInvalidGenericNodeSpawnerResult(
				TEXT("missing_required_evidence"),
				TEXT("switch_int requires generic.control.case_values evidence."));
		}
		NodeClass = UK2Node_SwitchInteger::StaticClass();
		Spawner = CreateGenericControlSpawner(NodeClass);
		Facts.Add(TEXT("generic.control.case_values"), CaseValues);
	}
	else if (Operation == TEXT("switch_string"))
	{
		if (ParseDelimitedEvidenceList(CaseValues).Num() == 0)
		{
			return MakeInvalidGenericNodeSpawnerResult(
				TEXT("missing_required_evidence"),
				TEXT("switch_string requires generic.control.case_values evidence."));
		}
		NodeClass = UK2Node_SwitchString::StaticClass();
		Spawner = CreateGenericControlSpawner(NodeClass);
		Facts.Add(TEXT("generic.control.case_values"), CaseValues);
	}
	else if (Operation == TEXT("switch_name"))
	{
		if (ParseDelimitedEvidenceList(CaseValues).Num() == 0)
		{
			return MakeInvalidGenericNodeSpawnerResult(
				TEXT("missing_required_evidence"),
				TEXT("switch_name requires generic.control.case_values evidence."));
		}
		NodeClass = UK2Node_SwitchName::StaticClass();
		Spawner = CreateGenericControlSpawner(NodeClass);
		Facts.Add(TEXT("generic.control.case_values"), CaseValues);
	}
	else if (Operation == TEXT("switch_enum"))
	{
		if (ParseDelimitedEvidenceList(CaseValues).Num() == 0)
		{
			return MakeInvalidGenericNodeSpawnerResult(
				TEXT("missing_required_evidence"),
				TEXT("switch_enum requires generic.control.case_values evidence."));
		}
		UEnum* Enum = ResolveEnumEvidence(Request);
		if (!Enum)
		{
			return MakeInvalidGenericNodeSpawnerResult(
				TEXT("missing_required_evidence"),
				TEXT("switch_enum requires loadable generic.control.enum_path evidence."));
		}
		NodeClass = UK2Node_SwitchEnum::StaticClass();
		Spawner = CreateSwitchEnumSpawner(Enum);
		Facts.Add(TEXT("generic.control.case_values"), CaseValues);
		Facts.Add(TEXT("generic.control.enum_path"), Enum->GetPathName());
	}
	else if (Operation == TEXT("multi_gate"))
	{
		int32 OutputCount = 0;
		if (!LexTryParseString(OutputCount, *DynamicOutputCount) || OutputCount <= 0)
		{
			return MakeInvalidGenericNodeSpawnerResult(
				TEXT("missing_required_evidence"),
				TEXT("multi_gate requires positive generic.control.dynamic_output_count evidence."));
		}
		NodeClass = UK2Node_MultiGate::StaticClass();
		Spawner = CreateGenericControlSpawner(NodeClass);
		Facts.Add(TEXT("generic.control.dynamic_output_count"), FString::FromInt(OutputCount));
	}
	else
	{
		return MakeUnsupportedGenericSemanticResult(
			Request,
			FString::Printf(TEXT("Unsupported dedicated GenericOps control operation '%s'."), *Operation));
	}

	return MakeResolvedGenericNodeSpawnerResult(
		Request,
		TEXT("control"),
		Operation,
		Operation == TEXT("multi_gate") ? DynamicOutputCount : CaseValues,
		NodeClass,
		Spawner,
		TEXT("GenericOps.Control"),
		FString::Printf(TEXT("GenericOps dedicated control operation=%s"), *Operation),
		Facts);
}

static bool IsStandardMacroOperation(const FString& Operation)
{
	return Operation == TEXT("do_once")
		|| Operation == TEXT("do_n")
		|| Operation == TEXT("gate")
		|| Operation == TEXT("flip_flop")
		|| Operation == TEXT("for_loop")
		|| Operation == TEXT("for_loop_with_break")
		|| Operation == TEXT("foreach_loop")
		|| Operation == TEXT("foreach_loop_with_break")
		|| Operation == TEXT("while_loop");
}

static FBlueprintHelperActionResolutionResult ResolveStandardMacroNodeSpawner(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	const FString Operation = ResolveControlOperation(Request);
	if (!IsStandardMacroOperation(Operation))
	{
		return MakeUnsupportedGenericSemanticResult(
			Request,
			FString::Printf(TEXT("Unsupported StandardMacros GenericOps operation '%s'."), *Operation));
	}

	const FString GraphPath = EvidenceValue(Request, TEXT("generic.macro.graph_path"));
	const FString PinShapeSnapshot = EvidenceValue(Request, TEXT("generic.macro.pin_shape_snapshot"));
	if (GraphPath.IsEmpty() || PinShapeSnapshot.IsEmpty())
	{
		return MakeInvalidGenericNodeSpawnerResult(
			TEXT("missing_required_evidence"),
			TEXT("StandardMacros control requires generic.macro.graph_path and generic.macro.pin_shape_snapshot evidence."));
	}

	UEdGraph* MacroGraph = ResolveMacroGraphEvidence(Request);
	if (!MacroGraph)
	{
		return MakeInvalidGenericNodeSpawnerResult(
			TEXT("macro_graph_not_found"),
			FString::Printf(TEXT("StandardMacros control macro graph could not be loaded: %s."), *GraphPath));
	}
	if (!MacroGraph->GetSchema() || MacroGraph->GetSchema()->GetGraphType(MacroGraph) != GT_Macro)
	{
		return MakeInvalidGenericNodeSpawnerResult(
			TEXT("macro_graph_not_macro"),
			FString::Printf(TEXT("StandardMacros control graph is not a macro graph: %s."), *MacroGraph->GetPathName()));
	}

	UBlueprintNodeSpawner* Spawner = CreateMacroInstanceSpawner(MacroGraph);
	TMap<FString, FString> Facts;
	Facts.Add(TEXT("generic.control.operation"), Operation);
	Facts.Add(TEXT("generic.macro.graph_path"), MacroGraph->GetPathName());
	Facts.Add(TEXT("generic.macro.pin_shape_snapshot"), PinShapeSnapshot);

	return MakeResolvedGenericNodeSpawnerResult(
		Request,
		TEXT("control"),
		Operation,
		MacroGraph->GetPathName(),
		UK2Node_MacroInstance::StaticClass(),
		Spawner,
		TEXT("GenericOps.StandardMacros"),
		FString::Printf(TEXT("GenericOps StandardMacros operation=%s macro=%s"), *Operation, *MacroGraph->GetPathName()),
		Facts);
}

static FBlueprintHelperActionResolutionResult ResolveSingletonControlFlowNodeSpawner(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperSingletonControlFlowEvidence Evidence;
	if (FBlueprintHelperSingletonControlFlowEvidenceProvider::TryResolve(Request, Evidence))
	{
		return FBlueprintHelperSingletonControlFlowEvidenceProvider::MakeResolvedResult(Request, Evidence);
	}

	const FString Operation = ResolveControlOperation(Request);
	if (Operation.StartsWith(TEXT("switch_")) || Operation == TEXT("multi_gate"))
	{
		return ResolveDedicatedControlFlowNodeSpawner(Request);
	}
	if (IsStandardMacroOperation(Operation))
	{
		return ResolveStandardMacroNodeSpawner(Request);
	}

	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("unsupported_singleton_control_flow_semantic");
	Result.Message = FString::Printf(
		TEXT("Unsupported singleton control-flow semantic '%s' with query '%s'."),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
		*Request.Semantic.Query);
	return Result;
}
} // namespace

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetStructControlActionResolver::ResolveNodeSpawnerCandidate(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	if (!IsGenericNodeSpawnerSemantic(Context.GetSemantic().Kind))
	{
		return MakeUnsupportedGenericSemanticResult(
			Request,
			TEXT("Generic NodeSpawner candidate resolver only accepts select/control semantics; Struct/TypeStructure construct/deconstruct must use FBlueprintHelperStructTypeStructureActionResolver."));
	}

	return ResolveSingletonControlFlowNodeSpawner(Request);
}
