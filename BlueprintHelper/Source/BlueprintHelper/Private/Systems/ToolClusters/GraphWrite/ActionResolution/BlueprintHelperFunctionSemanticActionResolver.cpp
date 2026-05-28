#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionSemanticActionResolver.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphTokenWrappers.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionResolverUtils.h"

bool FBlueprintHelperFunctionSemanticActionResolver::IsSupportedSemanticKind(
	const FBlueprintHelperActionSemanticConstraints& Semantic)
{
	const FString FunctionOperation = NormalizeOperation(UGraphWriteActionResolverUtils::GetFunctionOperation(Semantic));
	switch (Semantic.Kind)
	{
	case EBlueprintHelperActionSemanticKind::Create:
		return FunctionOperation == TEXT("create_function");
	case EBlueprintHelperActionSemanticKind::Convert:
		return FunctionOperation == TEXT("convert_function");
	case EBlueprintHelperActionSemanticKind::Schedule:
		return FunctionOperation == TEXT("schedule_function")
			|| FunctionOperation == TEXT("latent_or_async_function");
	default:
		return false;
	}
}

FBlueprintHelperActionResolutionResult FBlueprintHelperFunctionSemanticActionResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	const FBlueprintHelperActionSemanticConstraints& Semantic = Context.GetSemantic();
	const FString Query = Semantic.Query.TrimStartAndEnd();
	if (Query.IsEmpty())
	{
		return UGraphWriteActionResolverUtils::MakeInvalidRequestResult(
			TEXT("needs_more_semantic_context"),
			TEXT("Function semantic callable resolution requires a non-empty query."));
	}

	const FString FunctionOperation = NormalizeOperation(UGraphWriteActionResolverUtils::GetFunctionOperation(Semantic));
	if (!IsSupportedSemanticKind(Semantic))
	{
		return UGraphWriteActionResolverUtils::MakeInvalidRequestResult(
			TEXT("needs_more_semantic_context"),
			FString::Printf(
				TEXT("Unsupported function semantic operation '%s' for semantic kind '%s'."),
				FunctionOperation.IsEmpty() ? TEXT("<empty>") : *FunctionOperation,
				*FBlueprintHelperActionResolutionCore::SemanticKindToString(Semantic.Kind)));
	}

	if (Semantic.Kind == EBlueprintHelperActionSemanticKind::Convert
		&& !UGraphWriteActionResolverUtils::HasTypedArgumentPinEvidence(Semantic)
		&& !Semantic.ExpectedReturnPinType.IsValid())
	{
		return UGraphWriteActionResolverUtils::MakeInvalidRequestResult(
			TEXT("needs_more_semantic_context"),
			TEXT("Convert callable resolution requires typed argument pins or expected return pin evidence."));
	}

	if (Semantic.Kind == EBlueprintHelperActionSemanticKind::Schedule
		&& FunctionOperation == TEXT("latent_or_async_function")
		&& !UGraphWriteActionResolverUtils::IsTrueEvidence(Request.ContextEvidence, TEXT("graph_latent_allowed")))
	{
		return UGraphWriteActionResolverUtils::MakeInvalidRequestResult(
			TEXT("latent_function_not_allowed_in_graph"),
			TEXT("Latent or async function scheduling requires graph_latent_allowed=true evidence."));
	}

	return UGraphWriteActionResolverUtils::ResolveViaCallFunctionResolver(Request, Semantic);
}
