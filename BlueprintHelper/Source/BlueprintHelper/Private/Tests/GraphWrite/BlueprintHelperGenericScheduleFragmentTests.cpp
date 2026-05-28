#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionDatabaseProjectionService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextScope.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperDelegateLinkFragmentUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuilderRegistry.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CreateDelegate.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "Tests/GraphWrite/BlueprintHelperScheduleTestUtils.h"

namespace
{
static FString MakeGenericScheduleFragmentTestObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeGenericScheduleFragmentTestBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperGenericScheduleFragment/%s"),
		*MakeGenericScheduleFragmentTestObjectName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeGenericScheduleFragmentTestObjectName(TEXT("BP_GenericScheduleFragment")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperGenericScheduleFragmentTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}

static UEdGraph* GetGenericScheduleFragmentTestGraph(UBlueprint* Blueprint)
{
	return Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
}

static void AddScheduleEvidence(
	const FBlueprintHelperProjectedScheduleActionEvidence& Evidence,
	FBlueprintHelperGraphStatementIR& Statement)
{
	FBlueprintHelperProjectedSpawnerEvidence::WriteScheduleActionEvidence(Evidence, Statement.ContextEvidence);
}

static void AddTimerHandlerEvidence(
	FBlueprintHelperGraphStatementIR& Statement,
	const TCHAR* HandlerName)
{
	Statement.HandlerName = HandlerName;
	Statement.ContextEvidence.Add(TEXT("handler_name"), HandlerName);
	if (UFunction* HandlerFunction = AActor::StaticClass()->FindFunctionByName(FName(HandlerName)))
	{
		Statement.ContextEvidence.Add(TEXT("handler_function_path"), HandlerFunction->GetPathName());
		Statement.ContextEvidence.Add(TEXT("handler_source_cluster"), TEXT("BlueprintSignature"));
		Statement.ContextEvidence.Add(TEXT("signature_evidence_id"), FString::Printf(TEXT("signature:handler:%s"), HandlerName));
	}
}

static FBlueprintHelperGraphStatementIR MakeGenericScheduleStatement(
	const FString& StatementId,
	const FString& ScheduleOperation,
	const FBlueprintHelperProjectedScheduleActionEvidence& Evidence)
{
	FBlueprintHelperGraphStatementIR Statement;
	Statement.StatementId = StatementId;
	Statement.Path = TEXT("$.statements[0]");
	Statement.Kind = EBlueprintHelperGraphStatementKind::Schedule;
	Statement.PatternName = TEXT("schedule");
	Statement.ScheduleOperation = ScheduleOperation;
	Statement.Target = Evidence.Query;
	Statement.SearchMode = TEXT("contextual");
	Statement.AmbiguityPolicy = TEXT("fail_on_ambiguity");
	AddScheduleEvidence(Evidence, Statement);
	return Statement;
}

static bool BuildActionContextScopeForStatement(
	FAutomationTestBase& Test,
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperActionContextScope& OutScope,
	FString& OutError)
{
	TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>> Statements;
	Statements.Add(MakeShared<FBlueprintHelperGraphStatementIR>(Statement));

	const TArray<FBlueprintHelperActionContextDemand> Demands =
		FBlueprintHelperActionContextDemandCollector::CollectFromStatements(Statements);
	Test.TestTrue(TEXT("generic schedule action context demands exist"), Demands.Num() > 0);
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
			TEXT("generic_schedule_fragment_tests"),
			TEXT("task5")),
		OutScope,
		OutError);
}

template <typename TNode>
static TNode* FindSingleFragmentNode(
	FAutomationTestBase& Test,
	const FBlueprintHelperNodeFragment& Fragment,
	const TCHAR* Label)
{
	TNode* Result = nullptr;
	int32 MatchCount = 0;
	for (UEdGraphNode* Node : Fragment.Nodes)
	{
		if (TNode* TypedNode = Cast<TNode>(Node))
		{
			Result = TypedNode;
			++MatchCount;
		}
	}

	Test.TestEqual(FString::Printf(TEXT("%s count"), Label), MatchCount, 1);
	return MatchCount == 1 ? Result : nullptr;
}

static int32 CountFragmentNodesOfClass(
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

static bool BuildScheduleFragment(
	FAutomationTestBase& Test,
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutBuildError)
{
	FBlueprintHelperActionContextScope ActionContextScope;
	FString ScopeError;
	const bool bScopeBuilt = BuildActionContextScopeForStatement(
		Test,
		Blueprint,
		Graph,
		Statement,
		ActionContextScope,
		ScopeError);
	Test.TestTrue(TEXT("generic schedule scope builds"), bScopeBuilt);
	if (!ScopeError.IsEmpty())
	{
		Test.AddInfo(FString::Printf(TEXT("generic schedule scope error: %s"), *ScopeError));
	}
	if (!bScopeBuilt)
	{
		OutBuildError = ScopeError;
		return false;
	}

	const bool bBuilt = FBlueprintHelperGraphFragmentBuilderRegistry::TryBuildStatement(
		Graph,
		&ActionContextScope,
		Statement,
		OutFragment,
		OutBuildError);
	if (!OutBuildError.IsEmpty())
	{
		Test.AddInfo(FString::Printf(TEXT("generic schedule build error: %s"), *OutBuildError));
	}
	return bBuilt;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericScheduleTimerDelegateFragmentBuildTest,
	"BlueprintHelper.GraphWrite.GenericSchedule.TimerDelegate.FragmentBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericScheduleTimerDelegateFragmentBuildTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericScheduleFragmentTestBlueprint();
	UEdGraph* Graph = GetGenericScheduleFragmentTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FBlueprintHelperProjectedScheduleActionEvidence Evidence;
	TestTrue(
		TEXT("project timer schedule evidence"),
		TryProjectScheduleEvidenceFromQueries(
			Blueprint,
			Graph,
			{ TEXT("Set Timer by Event"), TEXT("Set Timer by Delegate"), TEXT("Set Timer") },
			Evidence));
	if (!Evidence.HasProjectedIdentity())
	{
		return false;
	}

	FBlueprintHelperGraphStatementIR Statement = MakeGenericScheduleStatement(
		TEXT("stmt_generic_schedule_timer_delegate"),
		TEXT("timer_delegate_node"),
		Evidence);
	AddTimerHandlerEvidence(Statement, TEXT("K2_DestroyActor"));

	FBlueprintHelperNodeFragment Fragment;
	FString BuildError;
	const int32 FunctionGraphCountBefore = Blueprint->FunctionGraphs.Num();
	const int32 DelegateSignatureGraphCountBefore = Blueprint->DelegateSignatureGraphs.Num();
	const bool bBuilt = BuildScheduleFragment(*this, Blueprint, Graph, Statement, Fragment, BuildError);
	TestTrue(TEXT("timer delegate schedule fragment builds"), bBuilt);
	if (!bBuilt)
	{
		return false;
	}
	TestEqual(TEXT("timer schedule does not create handler function graph"), Blueprint->FunctionGraphs.Num(), FunctionGraphCountBefore);
	TestEqual(TEXT("timer schedule does not create delegate signature graph"), Blueprint->DelegateSignatureGraphs.Num(), DelegateSignatureGraphCountBefore);

	UK2Node_CreateDelegate* CreateDelegateNode =
		FindSingleFragmentNode<UK2Node_CreateDelegate>(*this, Fragment, TEXT("timer create delegate node"));
	TestNotNull(TEXT("timer primary node"), Fragment.PrimaryNode);
	if (Fragment.PrimaryNode)
	{
		TestEqual(TEXT("timer primary node class matches projected evidence"), Fragment.PrimaryNode->GetClass()->GetPathName(), Evidence.NodeClassPath);
	}
	TestEqual(TEXT("timer fragment node count"), Fragment.Nodes.Num(), 2);
	TestEqual(TEXT("timer fragment internal link count"), Fragment.InternalLinks.Num(), 1);
	TestEqual(TEXT("timer ownership schedule operation"), Fragment.OwnershipTags.FindRef(TEXT("schedule_operation")), FString(TEXT("timer_delegate_node")));
	TestEqual(TEXT("timer ownership handler source"), Fragment.OwnershipTags.FindRef(TEXT("handler_source_cluster")), FString(TEXT("BlueprintSignature")));
	TestEqual(TEXT("timer ownership signature evidence"), Fragment.OwnershipTags.FindRef(TEXT("signature_evidence_id")), FString(TEXT("signature:handler:K2_DestroyActor")));
	TestTrue(TEXT("timer create delegate readback binding"), Fragment.PinBindings.Contains(TEXT("create_delegate.event")));
	TestTrue(TEXT("timer delegate input readback binding"), Fragment.PinBindings.Contains(TEXT("delegate.event")));
	if (CreateDelegateNode)
	{
		TestEqual(TEXT("timer create delegate selected function"), CreateDelegateNode->GetFunctionName(), FName(TEXT("K2_DestroyActor")));
	}
	if (CreateDelegateNode && Fragment.InternalLinks.Num() == 1)
	{
		TestEqual(TEXT("timer link source is create delegate output"), Fragment.InternalLinks[0].From.Pin, CreateDelegateNode->GetDelegateOutPin());
		TestTrue(TEXT("timer delegate input is linked"), Fragment.InternalLinks[0].To.Pin && Fragment.InternalLinks[0].To.Pin->LinkedTo.Contains(CreateDelegateNode->GetDelegateOutPin()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericScheduleLatentAsyncFragmentBuildTest,
	"BlueprintHelper.GraphWrite.GenericSchedule.LatentAsync.FragmentBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericScheduleLatentAsyncFragmentBuildTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericScheduleFragmentTestBlueprint();
	UEdGraph* Graph = GetGenericScheduleFragmentTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FBlueprintHelperProjectedScheduleActionEvidence Evidence;
	TestTrue(
		TEXT("project latent schedule evidence"),
		TryProjectScheduleEvidence(Blueprint, Graph, TEXT("Async Load Primary Asset"), Evidence));
	if (!Evidence.HasProjectedIdentity())
	{
		return false;
	}

	FBlueprintHelperGraphStatementIR Statement = MakeGenericScheduleStatement(
		TEXT("stmt_generic_schedule_latent_async"),
		TEXT("latent_or_async_node"),
		Evidence);
	Statement.GraphLatentAllowed = TEXT("true");
	Statement.ContextEvidence.Add(TEXT("graph_latent_allowed"), TEXT("true"));

	FBlueprintHelperNodeFragment Fragment;
	FString BuildError;
	const bool bBuilt = BuildScheduleFragment(*this, Blueprint, Graph, Statement, Fragment, BuildError);
	TestTrue(TEXT("latent async schedule fragment builds"), bBuilt);
	if (!bBuilt)
	{
		return false;
	}

	TestNotNull(TEXT("latent async primary node"), Fragment.PrimaryNode);
	if (Fragment.PrimaryNode)
	{
		TestEqual(TEXT("latent primary node class matches projected evidence"), Fragment.PrimaryNode->GetClass()->GetPathName(), Evidence.NodeClassPath);
	}
	TestEqual(TEXT("latent async create delegate count"), CountFragmentNodesOfClass(Fragment, UK2Node_CreateDelegate::StaticClass()), 0);
	TestEqual(TEXT("latent async internal link count"), Fragment.InternalLinks.Num(), 0);
	TestEqual(TEXT("latent ownership schedule operation"), Fragment.OwnershipTags.FindRef(TEXT("schedule_operation")), FString(TEXT("latent_or_async_node")));
	TestTrue(TEXT("latent action provider pins populated"), Fragment.PinBindings.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericScheduleDelegatePinMissingDiagnosticTest,
	"BlueprintHelper.GraphWrite.GenericSchedule.DelegateLink.PinMissingDiagnostic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericScheduleDelegatePinMissingDiagnosticTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericScheduleFragmentTestBlueprint();
	UEdGraph* Graph = GetGenericScheduleFragmentTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	UK2Node_CallFunction* PrimaryNode = NewObject<UK2Node_CallFunction>(Graph);
	TestNotNull(TEXT("primary node"), PrimaryNode);
	FString Error;
	UEdGraphPin* DelegatePin = FBlueprintHelperDelegateLinkFragmentUtils::ResolveDelegateInputPin(
		PrimaryNode,
		FString(),
		TEXT("timer_delegate"),
		Error);
	TestNull(TEXT("missing delegate pin returns null"), DelegatePin);
	TestTrue(TEXT("missing delegate pin diagnostic"), Error.Contains(TEXT("timer_delegate_pin_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericScheduleDelegatePinAmbiguousDiagnosticTest,
	"BlueprintHelper.GraphWrite.GenericSchedule.DelegateLink.PinAmbiguousDiagnostic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericScheduleDelegatePinAmbiguousDiagnosticTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericScheduleFragmentTestBlueprint();
	UEdGraph* Graph = GetGenericScheduleFragmentTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	UK2Node_CallFunction* PrimaryNode = NewObject<UK2Node_CallFunction>(Graph);
	TestNotNull(TEXT("primary node"), PrimaryNode);
	if (!PrimaryNode)
	{
		return false;
	}
	PrimaryNode->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Delegate, FName(TEXT("DelegateA")));
	PrimaryNode->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Delegate, FName(TEXT("DelegateB")));

	FString Error;
	UEdGraphPin* DelegatePin = FBlueprintHelperDelegateLinkFragmentUtils::ResolveDelegateInputPin(
		PrimaryNode,
		FString(),
		TEXT("timer_delegate"),
		Error);
	TestNull(TEXT("ambiguous delegate pin returns null"), DelegatePin);
	TestTrue(TEXT("ambiguous delegate pin diagnostic"), Error.Contains(TEXT("timer_delegate_pin_ambiguous")));
	return true;
}

#endif
