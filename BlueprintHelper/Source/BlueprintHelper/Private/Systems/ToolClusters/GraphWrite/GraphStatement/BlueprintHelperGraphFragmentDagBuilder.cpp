#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDagBuilder.h"

#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagBuilderUtils.h"
bool FBlueprintHelperGraphFragmentDagBuilder::BuildFromSemanticIR(
	const FBlueprintHelperGraphSemanticIR& SemanticIR,
	FBlueprintHelperGraphFragmentDag& OutDag)
{
	OutDag.Reset();
	OutDag.Schema = TEXT("BlueprintHelperGraphFragmentDag.v1");
	OutDag.Metadata.Add(TEXT("builder"), TEXT("statement_tree_to_fragment_dag.phase2.narrow"));
	OutDag.Metadata.Add(TEXT("source_schema"), SemanticIR.Schema);
	OutDag.Metadata.Add(TEXT("statement_count"), LexToString(SemanticIR.Statements.Num()));
	OutDag.Metadata.Add(TEXT("symbol_count"), LexToString(SemanticIR.Symbols.Num()));

	FBlueprintHelperGraphFragmentDagBuilderUtils::CopySemanticDiagnostics(SemanticIR, OutDag);

	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagBuildState State;
	State.Dag = &OutDag;

	TArray<TMap<FString, FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer>> SymbolScopes;
	SymbolScopes.AddDefaulted();

	const FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagExecFlow TopLevelFlow = FBlueprintHelperGraphFragmentDagBuilderUtils::BuildStatementArray(
		SemanticIR.Statements,
		TEXT("$.statements"),
		State,
		SymbolScopes);

	OutDag.EntryExitRefs.Entries = TopLevelFlow.Entries;
	OutDag.EntryExitRefs.Exits = TopLevelFlow.Exits;

	if (SemanticIR.Statements.Num() == 0)
	{
		OutDag.AddDiagnostic(
			TEXT("statement_tree_empty"),
			TEXT("$.statements"),
			TEXT("Semantic IR contains no statements to arrange into a fragment DAG."),
			EBlueprintHelperGraphFragmentDiagnosticSeverity::Warning);
	}
	else
	{
		if (!OutDag.EntryExitRefs.HasEntry())
		{
			OutDag.AddDiagnostic(
				TEXT("dag_entry_missing"),
				TEXT("$.statements"),
				TEXT("Fragment DAG has no valid entry endpoint."),
				EBlueprintHelperGraphFragmentDiagnosticSeverity::Error);
		}
		if (!OutDag.EntryExitRefs.HasExit())
		{
			OutDag.AddDiagnostic(
				TEXT("dag_exit_missing"),
				TEXT("$.statements"),
				TEXT("Fragment DAG has no valid exit endpoint."),
				EBlueprintHelperGraphFragmentDiagnosticSeverity::Error);
		}
	}

	return !OutDag.HasErrors();
}
