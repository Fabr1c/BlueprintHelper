// GraphWrite 测试工具类实现

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/GraphWrite/BlueprintHelperGraphWriteTestUtils.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextScope.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"

bool FBlueprintHelperGraphWriteTestUtils::BuildActionContextScopeForStatement(
	FAutomationTestBase& Test,
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FBlueprintHelperGraphStatementIR& Statement,
	const TCHAR* DemandsTestLabel,
	const TCHAR* RevisionTestId,
	const TCHAR* RevisionTaskId,
	FBlueprintHelperActionContextScope& OutScope,
	FString& OutError)
{
	TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>> Statements;
	Statements.Add(MakeShared<FBlueprintHelperGraphStatementIR>(Statement));

	const TArray<FBlueprintHelperActionContextDemand> Demands =
		FBlueprintHelperActionContextDemandCollector::CollectFromStatements(Statements);
	Test.TestTrue(DemandsTestLabel, Demands.Num() > 0);
	if (Demands.Num() == 0)
	{
		OutError = TEXT("no_action_context_demands");
		return false;
	}

	return FBlueprintHelperActionContextScope::Build(
		Blueprint,
		Graph,
		Demands,
		FBlueprintHelperActionContextScope::MakeRevision(
			Blueprint,
			Graph,
			RevisionTestId,
			RevisionTaskId),
		OutScope,
		OutError);
}

int32 FBlueprintHelperGraphWriteTestUtils::CountFragmentNodesOfClass(
	const FBlueprintHelperNodeFragment& Fragment,
	const UClass* NodeClass)
{
	int32 Count = 0;
	for (UEdGraphNode* Node : Fragment.Nodes)
	{
		if (Node && NodeClass && Node->IsA(NodeClass))
		{
			++Count;
		}
	}
	return Count;
}

#endif // WITH_DEV_AUTOMATION_TESTS
