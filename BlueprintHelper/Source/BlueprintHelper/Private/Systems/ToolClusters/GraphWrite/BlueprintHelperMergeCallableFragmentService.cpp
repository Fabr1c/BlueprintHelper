#include "Systems/ToolClusters/GraphWrite/BlueprintHelperMergeCallableFragmentService.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBuildService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h"
#include "Systems/ToolClusters/GraphWrite/Utils/GraphWriteCoreUtils.h"

FBlueprintHelperMergeCallableFragmentResult FBlueprintHelperMergeCallableFragmentService::ValidateCallable(
	const FBlueprintHelperMergeCallableFragmentRequest& Request)
{
	FBlueprintHelperMergeCallableFragmentResult Result;

	FBlueprintHelperGraphFragmentBuildRequest BuildRequest = MakeBuildRequest(Request);
	FBlueprintHelperActionContextScope ActionContextScope;
	FString BuildError;
	if (!UGraphWriteCoreUtils::BuildCallActionContextScope(
		Request.Blueprint,
		Request.Graph,
		BuildRequest,
		ActionContextScope,
		BuildError))
	{
		Result.Message = BuildError.IsEmpty()
			? TEXT("callable_action_context_build_failed")
			: BuildError;
		return Result;
	}

	FString ResolvedStableId;
	if (!FBlueprintHelperGraphStatementBuilder::ValidateCallFunctionFragment(
		Request.Graph,
		BuildRequest,
		BuildError,
		&ResolvedStableId,
		nullptr,
		&ActionContextScope))
	{
		Result.Message = BuildError;
		return Result;
	}

	Result.bOk = true;
	Result.ResolvedStableId = ResolvedStableId;
	return Result;
}

FBlueprintHelperMergeCallableFragmentResult FBlueprintHelperMergeCallableFragmentService::BuildCallableFragment(
	const FBlueprintHelperMergeCallableFragmentRequest& Request)
{
	FBlueprintHelperMergeCallableFragmentResult Result = ValidateCallable(Request);
	if (!Result.bOk)
	{
		return Result;
	}

	FBlueprintHelperGraphFragmentBuildRequest BuildRequest = MakeBuildRequest(Request);
	if (BuildRequest.ResolvedStableId.IsEmpty())
	{
		BuildRequest.ResolvedStableId = Result.ResolvedStableId;
	}

	FBlueprintHelperActionContextScope ActionContextScope;
	FString BuildError;
	if (!UGraphWriteCoreUtils::BuildCallActionContextScope(
		Request.Blueprint,
		Request.Graph,
		BuildRequest,
		ActionContextScope,
		BuildError))
	{
		Result.bOk = false;
		Result.Message = BuildError.IsEmpty()
			? TEXT("callable_action_context_build_failed")
			: BuildError;
		return Result;
	}

	if (!FBlueprintHelperGraphStatementBuilder::BuildCallFunctionFragment(
		Request.Graph,
		BuildRequest,
		Result.Fragment,
		BuildError,
		nullptr,
		&ActionContextScope))
	{
		Result.bOk = false;
		Result.Message = BuildError;
		return Result;
	}

	Result.bOk = Result.Fragment.IsValid();
	Result.PrimaryNode = Result.Fragment.PrimaryNode;
	if (!Result.bOk && Result.Message.IsEmpty())
	{
		Result.Message = TEXT("callable_fragment_build_failed");
	}
	return Result;
}

FBlueprintHelperGraphFragmentBuildRequest FBlueprintHelperMergeCallableFragmentService::MakeBuildRequest(
	const FBlueprintHelperMergeCallableFragmentRequest& Request)
{
	FBlueprintHelperGraphFragmentBuildRequest BuildRequest;
	BuildRequest.FragmentId = Request.FragmentId.IsEmpty()
		? Request.Query
		: Request.FragmentId;
	BuildRequest.SourceStatementId = Request.SourceStatementId.IsEmpty()
		? BuildRequest.FragmentId
		: Request.SourceStatementId;
	BuildRequest.ActionContextStatementId = Request.ActionContextStatementId.IsEmpty()
		? BuildRequest.FragmentId
		: Request.ActionContextStatementId;
	BuildRequest.Query = Request.Query;
	BuildRequest.FunctionOperation = TEXT("function_call");
	BuildRequest.SearchMode = Request.SearchMode;
	BuildRequest.AmbiguityPolicy = Request.AmbiguityPolicy;
	BuildRequest.CategoryPriority = Request.CategoryPriority;
	BuildRequest.Location = Request.Location;
	BuildRequest.DefaultValues = Request.DefaultValues;
	BuildRequest.ContextEvidence = Request.ContextEvidence;
	BuildRequest.ResolvedStableId = Request.ResolvedStableId;
	return BuildRequest;
}
