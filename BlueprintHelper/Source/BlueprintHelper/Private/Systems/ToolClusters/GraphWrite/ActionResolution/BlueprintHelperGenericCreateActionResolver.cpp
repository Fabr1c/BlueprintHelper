#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericCreateActionResolver.h"

#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_GenericCreateObject.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MakeMap.h"
#include "K2Node_MakeSet.h"
#include "K2Node_SpawnActorFromClass.h"
#include "Modules/ModuleManager.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"

namespace
{
static FString NormalizeCreateOperation(const FString& Operation)
{
	return Operation.TrimStartAndEnd().ToLower();
}

static FString DescribePinType(const FBlueprintHelperCallFunctionPinType& PinType)
{
	if (!PinType.IsValid())
	{
		return FString();
	}

	TArray<FString> Parts;
	if (!PinType.Category.IsEmpty())
	{
		Parts.Add(PinType.Category);
	}
	if (!PinType.SubCategory.IsEmpty())
	{
		Parts.Add(PinType.SubCategory);
	}
	if (!PinType.ObjectPath.IsEmpty())
	{
		Parts.Add(PinType.ObjectPath);
	}
	if (!PinType.ContainerType.IsEmpty())
	{
		Parts.Add(PinType.ContainerType);
	}
	return FString::Join(Parts, TEXT("|"));
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

static FString ResolveContainerElementEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic)
{
	return FirstNonEmpty(
		Semantic.ArgumentTypes.FindRef(TEXT("element")),
		Semantic.ArgumentTypes.FindRef(TEXT("value")),
		DescribePinType(Semantic.ContainerElementPinType),
		Semantic.ExpectedReturnType);
}

static FString ResolveContainerKeyEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic)
{
	return FirstNonEmpty(
		Semantic.ArgumentTypes.FindRef(TEXT("key")),
		DescribePinType(Semantic.ContainerKeyPinType));
}

static FString ResolveContainerValueEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic)
{
	return FirstNonEmpty(
		Semantic.ArgumentTypes.FindRef(TEXT("value")),
		Semantic.ArgumentTypes.FindRef(TEXT("element")),
		DescribePinType(Semantic.ContainerValuePinType));
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

static UClass* ResolveCreateWidgetNodeClass()
{
	FModuleManager::Get().LoadModule(FName(TEXT("UMGEditor")));
	return LoadObject<UClass>(nullptr, TEXT("/Script/UMGEditor.K2Node_CreateWidget"));
}

static FBlueprintHelperActionResolutionResult MakeResolvedCreateResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& Operation,
	UClass* ResolvedNodeClass,
	const FString& Evidence,
	const FString& ReturnType)
{
	if (!ResolvedNodeClass || !ResolvedNodeClass->IsChildOf(UEdGraphNode::StaticClass()))
	{
		return MakeUnsupportedResult(
			TEXT("unsupported_create_node_class"),
			FString::Printf(TEXT("Create operation '%s' does not have a loadable UE graph node class."), *Operation));
	}

	UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(ResolvedNodeClass);
	if (!Spawner)
	{
		return MakeUnsupportedResult(
			TEXT("unsupported_create_node_spawner"),
			FString::Printf(TEXT("Create operation '%s' could not create a UBlueprintNodeSpawner."), *Operation));
	}

	const FString CleanEvidence = Evidence.TrimStartAndEnd().IsEmpty() ? TEXT("default") : Evidence.TrimStartAndEnd();
	const FString StableId = FString::Printf(TEXT("generic_create:%s:%s"), *Operation, *CleanEvidence);

	FBlueprintHelperCallFunctionCandidateInfo Candidate;
	Candidate.StableId = StableId;
	Candidate.DisplayName = FString::Printf(TEXT("Create %s"), *Operation);
	Candidate.Category = TEXT("GenericCreate");
	Candidate.NodeClassPath = ResolvedNodeClass->GetPathName();
	Candidate.MatchReason = FString::Printf(TEXT("create_operation=%s evidence=%s"), *Operation, *CleanEvidence);
	Candidate.ReturnType = ReturnType;
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
	Result.MatchReason = FString::Printf(TEXT("create_operation=%s"), *Operation);
	return Result;
}
} // namespace

bool FBlueprintHelperGenericCreateActionResolver::IsSupportedCreateOperation(const FString& CreateOperation)
{
	const FString Normalized = NormalizeCreateOperation(CreateOperation);
	return Normalized == TEXT("spawn_actor")
		|| Normalized == TEXT("create_widget")
		|| Normalized == TEXT("construct_object")
		|| Normalized == TEXT("make_array")
		|| Normalized == TEXT("make_map")
		|| Normalized == TEXT("make_set")
		|| Normalized == TEXT("asset_action");
}

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericCreateActionResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	const FString Operation = NormalizeCreateOperation(Context.GetSemantic().CreateOperation);
	if (Operation.IsEmpty())
	{
		return MakeInvalidResult(
			TEXT("needs_more_semantic_context"),
			TEXT("Create semantic requires create_operation."));
	}

	if (!IsSupportedCreateOperation(Operation))
	{
		return MakeUnsupportedResult(
			TEXT("unsupported_create_operation"),
			FString::Printf(TEXT("Unsupported create_operation '%s'."), *Operation));
	}

	if (Operation == TEXT("asset_action"))
	{
		return MakeInvalidResult(
			TEXT("needs_more_semantic_context"),
			TEXT("asset_action create requires projected asset action database evidence."));
	}

	if (Operation == TEXT("make_array"))
	{
		const FString ElementEvidence = ResolveContainerElementEvidence(Context.GetSemantic());
		if (ElementEvidence.IsEmpty())
		{
			return MakeInvalidResult(TEXT("needs_more_semantic_context"), TEXT("make_array create requires element pin type evidence."));
		}
		return MakeResolvedCreateResult(Request, Operation, UK2Node_MakeArray::StaticClass(), ElementEvidence, ElementEvidence);
	}

	if (Operation == TEXT("make_map"))
	{
		const FString KeyEvidence = ResolveContainerKeyEvidence(Context.GetSemantic());
		const FString ValueEvidence = ResolveContainerValueEvidence(Context.GetSemantic());
		if (KeyEvidence.IsEmpty() || ValueEvidence.IsEmpty())
		{
			return MakeInvalidResult(TEXT("needs_more_semantic_context"), TEXT("make_map create requires key and value pin type evidence."));
		}
		return MakeResolvedCreateResult(
			Request,
			Operation,
			UK2Node_MakeMap::StaticClass(),
			KeyEvidence + TEXT(":") + ValueEvidence,
			TEXT("map"));
	}

	if (Operation == TEXT("make_set"))
	{
		const FString ElementEvidence = ResolveContainerElementEvidence(Context.GetSemantic());
		if (ElementEvidence.IsEmpty())
		{
			return MakeInvalidResult(TEXT("needs_more_semantic_context"), TEXT("make_set create requires element pin type evidence."));
		}
		return MakeResolvedCreateResult(Request, Operation, UK2Node_MakeSet::StaticClass(), ElementEvidence, TEXT("set"));
	}

	const FString ClassEvidence = ResolveClassEvidence(Context.GetSemantic());
	if (ClassEvidence.IsEmpty())
	{
		return MakeInvalidResult(
			TEXT("needs_more_semantic_context"),
			FString::Printf(TEXT("%s create requires class evidence."), *Operation));
	}

	if (Operation == TEXT("spawn_actor"))
	{
		return MakeResolvedCreateResult(Request, Operation, UK2Node_SpawnActorFromClass::StaticClass(), ClassEvidence, ClassEvidence);
	}

	if (Operation == TEXT("create_widget"))
	{
		return MakeResolvedCreateResult(Request, Operation, ResolveCreateWidgetNodeClass(), ClassEvidence, ClassEvidence);
	}

	if (Operation == TEXT("construct_object"))
	{
		return MakeResolvedCreateResult(Request, Operation, UK2Node_GenericCreateObject::StaticClass(), ClassEvidence, ClassEvidence);
	}

	return MakeUnsupportedResult(
		TEXT("unsupported_create_operation"),
		FString::Printf(TEXT("Unsupported create_operation '%s'."), *Operation));
}
