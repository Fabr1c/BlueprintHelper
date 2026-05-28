#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/BlueprintHelperGraphActionUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphTokenUtils.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperGraphActionUtils::MakeInvalidResult(
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

FBlueprintHelperActionResolutionResult FBlueprintHelperGraphActionUtils::MakeUnsupportedResult(
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

bool FBlueprintHelperGraphActionUtils::HasFunctionBackedOperationEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic)
{
	return !Semantic.FunctionOperation.TrimStartAndEnd().IsEmpty();
}

FString FBlueprintHelperGraphActionUtils::ResolveClassEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic)
{
	return FBlueprintHelperGraphTokenUtils::FirstNonEmptyString(
		Semantic.ClassPath,
		Semantic.TargetPath,
		Semantic.TypeName,
		Semantic.Query);
}
