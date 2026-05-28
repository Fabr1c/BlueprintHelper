#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperStructTypeStructureActionResolver.h"

#include "BlueprintFieldNodeSpawner.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_StructOperation.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionResolverUtils.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperStructTypeStructureActionResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	if (!UGraphWriteActionResolverUtils::IsStructTypeStructureRequest(Request))
	{
		return UGraphWriteActionResolverUtils::MakeInvalidSemanticResult(Request);
	}

	const FString TypeName = Context.GetSemantic().TypeName.TrimStartAndEnd();
	if (TypeName.IsEmpty())
	{
		return UGraphWriteActionResolverUtils::MakeNeedsContextResult(
			Request,
			TEXT("Struct/TypeStructure type_operation requires Semantic.TypeName before resolving NodeSpawner candidates."));
	}

	UScriptStruct* TargetStruct = UGraphWriteActionResolverUtils::ResolveStructType(TypeName);
	if (!TargetStruct)
	{
		return UGraphWriteActionResolverUtils::MakeStructTypeNotFoundResult(
			Request,
			FString::Printf(TEXT("Could not resolve Semantic.TypeName '%s' to a UScriptStruct."), *TypeName));
	}

	const bool bConstruct = UGraphWriteActionResolverUtils::IsConstructOperation(Request);
	TArray<FString> AttemptMessages;
	TArray<FBlueprintHelperCallFunctionCandidateInfo> CandidateActions;
	FBlueprintHelperActionResolutionResult FunctionResolvedResult;

	const FString NativeFunctionPath = TargetStruct->GetMetaData(bConstruct ? TEXT("HasNativeMake") : TEXT("HasNativeBreak"));
	const FString NormalizedNativeFunctionPath = UGraphWriteActionResolverUtils::NormalizeNativeFunctionPath(NativeFunctionPath);
	if (UGraphWriteActionResolverUtils::TryResolveFunctionActionSpawner(
		Request,
		TargetStruct,
		NormalizedNativeFunctionPath,
		TEXT("exact"),
		bConstruct,
		bConstruct ? TEXT("native_construct") : TEXT("native_deconstruct"),
		AttemptMessages,
		CandidateActions,
		FunctionResolvedResult))
	{
		return FunctionResolvedResult;
	}
	if (UGraphWriteActionResolverUtils::TryResolveNativeStructFunctionSpawner(
		NormalizedNativeFunctionPath,
		Request,
		TargetStruct,
		bConstruct,
		CandidateActions,
		FunctionResolvedResult))
	{
		return FunctionResolvedResult;
	}

	TArray<FString> SearchQueries;
	const FString DisplayName = TargetStruct->GetDisplayNameText().ToString();
	const FString StructName = TargetStruct->GetName();
	UGraphWriteActionResolverUtils::AddUniqueQuery(SearchQueries, FString::Printf(TEXT("%s %s"), bConstruct ? TEXT("Make") : TEXT("Break"), *DisplayName));
	UGraphWriteActionResolverUtils::AddUniqueQuery(SearchQueries, FString::Printf(TEXT("%s %s"), bConstruct ? TEXT("Make") : TEXT("Break"), *StructName));

	for (const FString& SearchQuery : SearchQueries)
	{
		if (UGraphWriteActionResolverUtils::TryResolveFunctionActionSpawner(
			Request,
			TargetStruct,
			SearchQuery,
			Request.Semantic.SearchMode.IsEmpty() ? FString(TEXT("ue_search")) : Request.Semantic.SearchMode,
			bConstruct,
			bConstruct ? TEXT("search_construct") : TEXT("search_deconstruct"),
			AttemptMessages,
			CandidateActions,
			FunctionResolvedResult))
		{
			return FunctionResolvedResult;
		}
	}

	return UGraphWriteActionResolverUtils::MakeDirectStructSpawnerResult(Request, TargetStruct, AttemptMessages, CandidateActions);
}
