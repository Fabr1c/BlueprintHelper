#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformScheduleActionResolver.h"

#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_ClassDynamicCast.h"
#include "K2Node_DynamicCast.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionDatabaseProjectionService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformSpawnerFactory.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperTypePromotionSpawnerEvidenceResolver.h"

namespace
{
static FString NormalizeOperation(const FString& Operation)
{
	return Operation.TrimStartAndEnd().ToLower();
}

static FString FirstNonEmpty(const FString& First, const FString& Second)
{
	return First.TrimStartAndEnd().IsEmpty() ? Second.TrimStartAndEnd() : First.TrimStartAndEnd();
}

static FString FirstNonEmpty(const FString& First, const FString& Second, const FString& Third)
{
	return FirstNonEmpty(FirstNonEmpty(First, Second), Third);
}

static FString FirstNonEmpty(const FString& First, const FString& Second, const FString& Third, const FString& Fourth)
{
	return FirstNonEmpty(FirstNonEmpty(First, Second, Third), Fourth);
}

static FString ResolveClassEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic)
{
	return FirstNonEmpty(
		Semantic.ClassPath,
		Semantic.TargetPath,
		Semantic.TypeName,
		Semantic.Query);
}

static UClass* ResolveTargetClass(const FString& ClassEvidence)
{
	const FString CleanEvidence = ClassEvidence.TrimStartAndEnd();
	if (CleanEvidence.IsEmpty())
	{
		return nullptr;
	}

	if (UClass* ExistingClass = FindObject<UClass>(nullptr, *CleanEvidence))
	{
		return ExistingClass;
	}
	return LoadObject<UClass>(nullptr, *CleanEvidence);
}

static FBlueprintHelperActionResolutionResult MakeInvalidResult(
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

static FBlueprintHelperActionResolutionResult MakeUnsupportedResult(
	const FString& ErrorCode,
	const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = ErrorCode;
	Result.Message = Message;
	return Result;
}

static FBlueprintHelperActionResolutionResult MakeScheduleInvalidResult(
	const TCHAR* ErrorCode,
	const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = ErrorCode;
	Result.Message = Message;
	return Result;
}

static bool IsTimerOperation(const FString& Operation)
{
	return Operation.Equals(TEXT("timer_delegate_node"), ESearchCase::IgnoreCase);
}

static bool IsLatentOrAsyncOperation(const FString& Operation)
{
	return Operation.Equals(TEXT("latent_or_async_node"), ESearchCase::IgnoreCase);
}

static bool HasFunctionBackedOperationEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic)
{
	return !Semantic.FunctionOperation.TrimStartAndEnd().IsEmpty();
}

static bool IsFunctionBackedTransformOperation(const FString& Operation)
{
	const FString Normalized = NormalizeOperation(Operation);
	return Normalized == TEXT("function_conversion")
		|| Normalized == TEXT("blueprint_autocast")
		|| Normalized == TEXT("numeric_conversion")
		|| Normalized == TEXT("string_name_text_conversion")
		|| Normalized == TEXT("enum_conversion")
		|| Normalized == TEXT("object_to_soft_object")
		|| Normalized == TEXT("class_to_soft_class");
}

static bool IsFunctionBackedScheduleOperation(const FString& Operation)
{
	const FString Normalized = NormalizeOperation(Operation);
	return Normalized == TEXT("timer_by_function_name")
		|| Normalized == TEXT("timer_by_handle")
		|| Normalized == TEXT("timer_clear_by_handle")
		|| Normalized == TEXT("timer_clear_by_function_name")
		|| Normalized == TEXT("timer_pause_by_handle")
		|| Normalized == TEXT("timer_pause_by_function_name")
		|| Normalized == TEXT("timer_unpause_by_handle")
		|| Normalized == TEXT("timer_unpause_by_function_name")
		|| Normalized == TEXT("delay")
		|| Normalized == TEXT("retriggerable_delay")
		|| Normalized == TEXT("delay_until_next_tick")
		|| Normalized == TEXT("generic_latent_function_call")
		|| Normalized == TEXT("async_proxy_output_delegate_connection");
}

static FBlueprintHelperActionDatabaseProjectionEvidence ToProjectionEvidence(
	const FBlueprintHelperProjectedScheduleActionEvidence& Evidence)
{
	FBlueprintHelperActionDatabaseProjectionEvidence Result;
	Result.StableId = Evidence.StableId;
	Result.NodeClassPath = Evidence.NodeClassPath;
	Result.SpawnerSignature = Evidence.SpawnerSignature;
	Result.OwnerPath = Evidence.OwnerPath;
	Result.Query = Evidence.Query;
	Result.MenuName = Evidence.MenuName;
	Result.Category = Evidence.Category;
	return Result;
}

static FBlueprintHelperCallFunctionCandidateInfo MakeScheduleCandidateInfo(
	const FBlueprintHelperActionDatabaseProjectedCandidate& Match,
	const FString& Operation)
{
	FBlueprintHelperCallFunctionCandidateInfo Candidate;
	Candidate.StableId = Match.StableId;
	Candidate.DisplayName = Match.MenuName.IsEmpty() ? TEXT("generic_schedule") : Match.MenuName;
	Candidate.Category = Match.Category;
	Candidate.NodeClassPath = Match.NodeClassPath;
	Candidate.MatchReason = FString::Printf(
		TEXT("generic_schedule operation=%s owner=%s node=%s menu=%s"),
		*Operation,
		Match.OwnerPath.IsEmpty() ? TEXT("none") : *Match.OwnerPath,
		Match.NodeClassPath.IsEmpty() ? TEXT("none") : *Match.NodeClassPath,
		Match.MenuName.IsEmpty() ? TEXT("none") : *Match.MenuName);
	Candidate.Score = 100;
	Candidate.bGraphCompatible = Match.Spawner != nullptr;
	Candidate.bFromActionDatabase = true;
	return Candidate;
}

static FBlueprintHelperActionResolutionResult MakeResolvedTransformResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& Operation,
	const FString& ClassEvidence,
	UClass* TargetClass,
	TSubclassOf<UEdGraphNode> ResolvedNodeClass)
{
	if (!TargetClass)
	{
		return MakeInvalidResult(
			TEXT("needs_more_semantic_context"),
			TEXT("Generic cast requires target class evidence."));
	}

	if (!ResolvedNodeClass || !ResolvedNodeClass->IsChildOf(UEdGraphNode::StaticClass()))
	{
		return MakeUnsupportedResult(
			TEXT("unsupported_generic_transform_node_class"),
			FString::Printf(TEXT("Generic transform operation '%s' does not have a loadable UE graph node class."), *Operation));
	}

	UBlueprintNodeSpawner* Spawner =
		FBlueprintHelperGenericTransformSpawnerFactory::CreateCastSpawner(ResolvedNodeClass, TargetClass);
	if (!Spawner)
	{
		return MakeUnsupportedResult(
			TEXT("unsupported_generic_transform_spawner"),
			FString::Printf(TEXT("Generic transform operation '%s' could not create a UBlueprintNodeSpawner."), *Operation));
	}

	const FString StableId = FString::Printf(TEXT("generic_transform:%s:%s"), *Operation, *ClassEvidence);

	FBlueprintHelperCallFunctionCandidateInfo Candidate;
	Candidate.StableId = StableId;
	Candidate.DisplayName = FString::Printf(TEXT("Generic %s"), *Operation);
	Candidate.Category = TEXT("GenericTransform");
	Candidate.NodeClassPath = ResolvedNodeClass->GetPathName();
	Candidate.MatchReason = FString::Printf(TEXT("generic_transform operation=%s target=%s"), *Operation, *ClassEvidence);
	Candidate.ReturnType = ClassEvidence;
	Candidate.Score = 100;
	Candidate.bGraphCompatible = Request.TargetGraph != nullptr;
	Candidate.bFromActionDatabase = true;

	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.SelectedStableId = StableId;
	Result.SelectedSpawner = Spawner;
	Result.CandidateActions.Add(MoveTemp(Candidate));
	Result.SpawnerClass = Spawner->GetClass()->GetPathName();
	Result.NodeClass = ResolvedNodeClass->GetPathName();
	Result.MatchReason = FString::Printf(TEXT("generic_transform operation=%s"), *Operation);
	return Result;
}

static FBlueprintHelperActionResolutionResult ResolveConvert(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	const FString Operation = NormalizeOperation(Context.GetSemantic().TransformOperation);
	if (Operation.IsEmpty())
	{
		return MakeInvalidResult(
			TEXT("needs_more_semantic_context"),
			TEXT("Generic Convert requires transform_operation."));
	}

	if (HasFunctionBackedOperationEvidence(Context.GetSemantic()) || IsFunctionBackedTransformOperation(Operation))
	{
		return MakeUnsupportedResult(
			TEXT("function_backed_operation_wrong_owner"),
			TEXT("Function-backed convert operations must route through FunctionActionCluster."));
	}

	if (!FBlueprintHelperGenericTransformScheduleActionResolver::IsSupportedTransformOperation(Operation))
	{
		return MakeUnsupportedResult(
			TEXT("unsupported_generic_transform_operation"),
			FString::Printf(TEXT("Unsupported generic transform_operation '%s'."), *Operation));
	}

	if (Operation == TEXT("type_promotion"))
	{
		return FBlueprintHelperTypePromotionSpawnerEvidenceResolver::Resolve(Request, Context);
	}

	const FString ClassEvidence = ResolveClassEvidence(Context.GetSemantic());
	if (ClassEvidence.IsEmpty())
	{
		return MakeInvalidResult(
			TEXT("needs_more_semantic_context"),
			TEXT("Generic cast requires target class evidence."));
	}

	UClass* TargetClass = ResolveTargetClass(ClassEvidence);
	if (!TargetClass)
	{
		return MakeInvalidResult(
			TEXT("needs_more_semantic_context"),
			TEXT("Generic cast requires loadable target class evidence."));
	}

	return MakeResolvedTransformResult(
		Request,
		Operation,
		ClassEvidence,
		TargetClass,
		Operation == TEXT("class_cast")
			? UK2Node_ClassDynamicCast::StaticClass()
			: UK2Node_DynamicCast::StaticClass());
}

static FBlueprintHelperActionResolutionResult ResolveSchedule(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	const FString Operation = NormalizeOperation(Context.GetSemantic().ScheduleOperation);
	if (Operation.IsEmpty())
	{
		return MakeInvalidResult(
			TEXT("needs_more_semantic_context"),
			TEXT("Generic Schedule requires schedule_operation."));
	}

	if (HasFunctionBackedOperationEvidence(Context.GetSemantic()) || IsFunctionBackedScheduleOperation(Operation))
	{
		return MakeUnsupportedResult(
			TEXT("function_backed_operation_wrong_owner"),
			TEXT("Function-backed schedule operations must route through FunctionActionCluster."));
	}

	if (!FBlueprintHelperGenericTransformScheduleActionResolver::IsSupportedScheduleOperation(Operation))
	{
		return MakeUnsupportedResult(
			TEXT("unsupported_generic_schedule_operation"),
			FString::Printf(TEXT("Unsupported generic schedule_operation '%s'."), *Operation));
	}

	const FBlueprintHelperProjectedScheduleActionEvidence Evidence =
		FBlueprintHelperProjectedSpawnerEvidence::ReadScheduleActionEvidence(Context.GetRequest());
	if (!Evidence.HasProjectedIdentity())
	{
		return MakeScheduleInvalidResult(
			TEXT("schedule_spawner_evidence_missing"),
			TEXT("Generic schedule requires projected ActionDatabase spawner identity evidence."));
	}

	if (IsTimerOperation(Operation) && !Evidence.HasTimerHandlerEvidence())
	{
		return MakeScheduleInvalidResult(
			TEXT("handler_evidence_missing"),
			TEXT("timer_delegate_node requires projected handler and signature evidence from BlueprintSignature or ActionContext."));
	}

	if (IsLatentOrAsyncOperation(Operation) && !Evidence.IsGraphLatentAllowed())
	{
		return MakeScheduleInvalidResult(
			TEXT("latent_function_not_allowed_in_graph"),
			TEXT("latent_or_async_node requires graph_latent_allowed=true evidence."));
	}

	FBlueprintHelperActionDatabaseProjectionRequest ProjectionRequest;
	ProjectionRequest.Blueprint = Context.GetRequest().Blueprint;
	ProjectionRequest.TargetGraph = Context.GetRequest().TargetGraph;
	ProjectionRequest.RequiredEvidence = ToProjectionEvidence(Evidence);
	ProjectionRequest.Query = Evidence.Query;
	ProjectionRequest.ErrorPrefix = TEXT("schedule");

	const FBlueprintHelperActionDatabaseProjectionResult Projection =
		FBlueprintHelperActionDatabaseProjectionService::Project(ProjectionRequest);
	if (Projection.Status == EBlueprintHelperActionResolutionStatus::InvalidRequest)
	{
		return MakeScheduleInvalidResult(
			Projection.ErrorCode.IsEmpty() ? TEXT("schedule_spawner_evidence_missing") : *Projection.ErrorCode,
			Projection.Message.IsEmpty()
				? TEXT("Generic schedule requires projected ActionDatabase spawner evidence.")
				: Projection.Message);
	}
	if (Projection.Status == EBlueprintHelperActionResolutionStatus::NotFound || Projection.Candidates.Num() == 0)
	{
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
		Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
		Result.ErrorCode = TEXT("schedule_spawner_not_found");
		Result.Message = TEXT("Generic schedule projected evidence did not match any current ActionDatabase spawner.");
		return Result;
	}
	if (Projection.Status == EBlueprintHelperActionResolutionStatus::Ambiguous || Projection.Candidates.Num() > 1)
	{
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::Ambiguous;
		Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
		Result.ErrorCode = TEXT("schedule_spawner_ambiguous");
		Result.Message = Projection.Message;
		return Result;
	}

	const FBlueprintHelperActionDatabaseProjectedCandidate& Match = Projection.Candidates[0];
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.SelectedStableId = Match.StableId;
	Result.SelectedSpawner = Match.Spawner;
	Result.SpawnerClass = Match.Spawner ? Match.Spawner->GetClass()->GetPathName() : FString();
	Result.NodeClass = Match.NodeClassPath;
	Result.MatchReason = FString::Printf(TEXT("generic_schedule operation=%s"), *Operation);
	Result.CandidateActions.Add(MakeScheduleCandidateInfo(Match, Operation));
	Result.Message = FString::Printf(
		TEXT("Resolved generic schedule operation '%s' from ActionDatabase spawner '%s'."),
		*Operation,
		*Match.StableId);
	return Result;
}
}

bool FBlueprintHelperGenericTransformScheduleActionResolver::IsSupportedTransformOperation(const FString& TransformOperation)
{
	const FString Normalized = NormalizeOperation(TransformOperation);
	return Normalized == TEXT("dynamic_cast")
		|| Normalized == TEXT("class_cast")
		|| Normalized == TEXT("type_promotion");
}

bool FBlueprintHelperGenericTransformScheduleActionResolver::IsSupportedScheduleOperation(const FString& ScheduleOperation)
{
	const FString Normalized = NormalizeOperation(ScheduleOperation);
	return Normalized == TEXT("timer_delegate_node")
		|| Normalized == TEXT("latent_or_async_node");
}

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericTransformScheduleActionResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	if (Context.GetSemantic().Kind == EBlueprintHelperActionSemanticKind::Convert)
	{
		return ResolveConvert(Request, Context);
	}

	if (Context.GetSemantic().Kind == EBlueprintHelperActionSemanticKind::Schedule)
	{
		return ResolveSchedule(Request, Context);
	}

	return MakeUnsupportedResult(
		TEXT("unsupported_generic_transform_schedule_semantic"),
		FString::Printf(
			TEXT("GenericTransformScheduleActionResolver does not own semantic kind '%s'."),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Context.GetSemantic().Kind)));
}
