#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformScheduleActionResolver.h"

#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_ClassDynamicCast.h"
#include "K2Node_DynamicCast.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"

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

	auto CustomizeCastNodeLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode, UClass* InTargetClass)
	{
		if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(NewNode))
		{
			CastNode->TargetType = InTargetClass;
		}
	};

	UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(ResolvedNodeClass);
	if (!Spawner)
	{
		return MakeUnsupportedResult(
			TEXT("unsupported_generic_transform_spawner"),
			FString::Printf(TEXT("Generic transform operation '%s' could not create a UBlueprintNodeSpawner."), *Operation));
	}
	Spawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(CustomizeCastNodeLambda, TargetClass);

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

	if (!FBlueprintHelperGenericTransformScheduleActionResolver::IsSupportedTransformOperation(Operation))
	{
		return MakeUnsupportedResult(
			TEXT("unsupported_generic_transform_operation"),
			FString::Printf(TEXT("Unsupported generic transform_operation '%s'."), *Operation));
	}

	if (Operation == TEXT("type_promotion"))
	{
		return MakeInvalidResult(
			TEXT("needs_more_semantic_context"),
			TEXT("type_promotion requires projected type-promotion spawner evidence."));
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
	const FBlueprintHelperActionClusterContextView& Context)
{
	const FString Operation = NormalizeOperation(Context.GetSemantic().ScheduleOperation);
	if (Operation.IsEmpty())
	{
		return MakeInvalidResult(
			TEXT("needs_more_semantic_context"),
			TEXT("Generic Schedule requires schedule_operation."));
	}

	if (!FBlueprintHelperGenericTransformScheduleActionResolver::IsSupportedScheduleOperation(Operation))
	{
		return MakeUnsupportedResult(
			TEXT("unsupported_generic_schedule_operation"),
			FString::Printf(TEXT("Unsupported generic schedule_operation '%s'."), *Operation));
	}

	if (Operation == TEXT("timer_delegate_node"))
	{
		return MakeInvalidResult(
			TEXT("needs_more_semantic_context"),
			TEXT("timer_delegate_node requires projected timer and delegate selected spawner evidence."));
	}

	return MakeInvalidResult(
		TEXT("needs_more_semantic_context"),
		TEXT("latent_or_async_node requires projected latent/async selected spawner evidence and graph latent permission."));
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
		return ResolveSchedule(Context);
	}

	return MakeUnsupportedResult(
		TEXT("unsupported_generic_transform_schedule_semantic"),
		FString::Printf(
			TEXT("GenericTransformScheduleActionResolver does not own semantic kind '%s'."),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Context.GetSemantic().Kind)));
}
