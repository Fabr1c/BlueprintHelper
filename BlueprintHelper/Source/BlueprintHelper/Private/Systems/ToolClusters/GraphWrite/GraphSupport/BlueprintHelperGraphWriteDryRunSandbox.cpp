#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphWriteDryRunSandbox.h"

#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteResultTypes.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

class FBlueprintHelperGraphWriteDryRunSandboxLocalUtils
{
public:
	static UPackage* CreateSandboxPackage(const UBlueprint* SourceBlueprint)
	{
		const FString SourceName = SourceBlueprint ? SourceBlueprint->GetName() : TEXT("Blueprint");
		UPackage* SandboxPackage = CreatePackage(*FString::Printf(
			TEXT("/Temp/BlueprintHelperDryRun/%s_%s"),
			*SourceName,
		*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		if (SandboxPackage)
		{
			SandboxPackage->SetFlags(RF_Transient);
			SandboxPackage->SetDirtyFlag(false);
		}
		return SandboxPackage;
	}

	static FName MakeSandboxBlueprintName(const UBlueprint* SourceBlueprint)
	{
		const FString SourceName = SourceBlueprint ? SourceBlueprint->GetName() : TEXT("BP");
		return FName(*FString::Printf(
			TEXT("%s_DryRunSandbox_%s"),
			*SourceName,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	static void ReleaseSandbox(UBlueprint* SandboxBlueprint, UPackage* SandboxPackage)
	{
		if (SandboxBlueprint)
		{
			SandboxBlueprint->ClearFlags(RF_Public | RF_Standalone);
			SandboxBlueprint->SetFlags(RF_Transient);
		}
		if (SandboxPackage)
		{
			SandboxPackage->SetDirtyFlag(false);
		}
	}

	static UEdGraph* FindOrCreateUbergraphPage(
		UBlueprint* SandboxBlueprint,
		const FString& GraphName,
		FString& OutError)
	{
		if (!SandboxBlueprint)
		{
			OutError = TEXT("Sandbox Blueprint is null.");
			return nullptr;
		}

		if (GraphName.IsEmpty())
		{
			OutError = TEXT("Target graph name is empty.");
			return nullptr;
		}

		for (UEdGraph* Page : SandboxBlueprint->UbergraphPages)
		{
			if (Page && Page->GetName() == GraphName)
			{
				return Page;
			}
		}

		for (UEdGraph* FunctionGraph : SandboxBlueprint->FunctionGraphs)
		{
			if (FunctionGraph && FunctionGraph->GetName() == GraphName)
			{
				OutError = TEXT("target_graph_type_invalid: a function graph with the same name exists.");
				return nullptr;
			}
		}

		for (UEdGraph* MacroGraph : SandboxBlueprint->MacroGraphs)
		{
			if (MacroGraph && MacroGraph->GetName() == GraphName)
			{
				OutError = TEXT("target_graph_type_invalid: a macro graph with the same name exists.");
				return nullptr;
			}
		}

		UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
			SandboxBlueprint,
			FName(*GraphName),
			UEdGraph::StaticClass(),
			UEdGraphSchema_K2::StaticClass());
		if (!NewGraph)
		{
			OutError = TEXT("Failed to create transient sandbox graph.");
			return nullptr;
		}

		FBlueprintEditorUtils::AddUbergraphPage(SandboxBlueprint, NewGraph);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(SandboxBlueprint);
		return NewGraph;
	}
};

FBlueprintHelperGraphWriteDryRunSandboxResult FBlueprintHelperGraphWriteDryRunSandbox::RunAppendPreview(
	const FBlueprintHelperGraphWriteDryRunSandboxInput& Input) const
{
	FBlueprintHelperGraphWriteDryRunSandboxResult Result;

	if (!Input.SourceBlueprint)
	{
		Result.ErrorCode = TEXT("dry_run_sandbox_source_blueprint_null");
		Result.Message = TEXT("Source Blueprint is required for GraphWrite dry-run sandbox.");
		return Result;
	}

	UPackage* SandboxPackage = FBlueprintHelperGraphWriteDryRunSandboxLocalUtils::CreateSandboxPackage(
		Input.SourceBlueprint);
	if (!SandboxPackage)
	{
		Result.ErrorCode = TEXT("dry_run_sandbox_package_failed");
		Result.Message = TEXT("Failed to create transient dry-run sandbox package.");
		return Result;
	}

	UBlueprint* SandboxBlueprint = DuplicateObject<UBlueprint>(
		Input.SourceBlueprint,
		SandboxPackage,
		FBlueprintHelperGraphWriteDryRunSandboxLocalUtils::MakeSandboxBlueprintName(Input.SourceBlueprint));
	if (!SandboxBlueprint)
	{
		Result.ErrorCode = TEXT("dry_run_sandbox_duplicate_failed");
		Result.Message = TEXT("Failed to duplicate Blueprint into transient dry-run sandbox.");
		FBlueprintHelperGraphWriteDryRunSandboxLocalUtils::ReleaseSandbox(nullptr, SandboxPackage);
		return Result;
	}
	SandboxBlueprint->SetFlags(RF_Transient);
	SandboxBlueprint->ClearFlags(RF_Public | RF_Standalone);

	FString GraphError;
	UEdGraph* SandboxGraph = FBlueprintHelperGraphWriteDryRunSandboxLocalUtils::FindOrCreateUbergraphPage(
		SandboxBlueprint,
		Input.GraphName,
		GraphError);
	if (!SandboxGraph)
	{
		Result.ErrorCode = TEXT("dry_run_sandbox_graph_resolve_failed");
		Result.Message = GraphError;
		FBlueprintHelperGraphWriteDryRunSandboxLocalUtils::ReleaseSandbox(SandboxBlueprint, SandboxPackage);
		return Result;
	}

	TArray<TSharedPtr<FUnresolvedNodeItem>> UnresolvedNodes;
	const FBlueprintGenerateResult GenerateResult =
		FBlueprintGraphGenerationPipeline::GenerateBlueprintFromJson(
			SandboxGraph,
			Input.GraphWritePayload,
			UnresolvedNodes);

	Result.GeneratedNodeCount = GenerateResult.GeneratedNodeCount;
	Result.ExecutionStats = GenerateResult.ExecutionStats;
	if (GenerateResult.bSucceed)
	{
		Result.bSucceeded = true;
		FBlueprintHelperGraphWriteDryRunSandboxLocalUtils::ReleaseSandbox(SandboxBlueprint, SandboxPackage);
		return Result;
	}

	Result.ErrorCode = GenerateResult.ConnectivityViolationCount > 0
		? TEXT("graphwrite_connectivity_failed")
		: TEXT("semantic_graph_write_failed");
	Result.Message = GenerateResult.Message;
	Result.ConnectivityDiagnostics = GenerateResult.ConnectivityDiagnostics;
	if (UnresolvedNodes.Num() > 0 && UnresolvedNodes[0].IsValid())
	{
		Result.Message += FString::Printf(
			TEXT(" First unresolved: %s - %s"),
			*UnresolvedNodes[0]->DisplayText,
			*UnresolvedNodes[0]->Reason);
	}
	FBlueprintHelperGraphWriteDryRunSandboxLocalUtils::ReleaseSandbox(SandboxBlueprint, SandboxPackage);
	return Result;
}
