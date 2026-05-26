#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

#include "BlueprintActionDatabase.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace
{
static FString BuildOperatorActionForbiddenToken(const TCHAR* Left, const TCHAR* Right)
{
	return FString(Left) + FString(Right);
}

static FString MakeOperatorActionTestObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeOperatorActionTestBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperOperatorAction/%s"),
		*MakeOperatorActionTestObjectName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeOperatorActionTestObjectName(TEXT("BP_OperatorAction")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperOperatorActionResolverTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}

static UEdGraph* GetOperatorActionTestGraph(UBlueprint* Blueprint)
{
	return Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
}

static FBlueprintHelperActionResolutionRequest MakeOperatorActionRequest(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FString& OperationId,
	const FString& Query = FString())
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.StatementId = MakeOperatorActionTestObjectName(TEXT("Stmt"));
	Request.ProjectedContextHash = TEXT("operator_projected_context");
	Request.SemanticConstraintsHash = TEXT("operator_semantic_constraints");
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Op;
	Request.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Operator;
	Request.Semantic.Query = Query;
	Request.Semantic.FunctionOperation = FString::Printf(TEXT("op.%s"), *OperationId);
	Request.ContextEvidence.Add(TEXT("op.operation_id"), OperationId);
	return Request;
}

static bool ScanBlueprintHelperSourceForOperatorForbiddenToken(
	FAutomationTestBase& Test,
	const FString& SourceRoot,
	const FString& Token)
{
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *SourceRoot, TEXT("*.h"), true, false);
	IFileManager::Get().FindFilesRecursive(Files, *SourceRoot, TEXT("*.cpp"), true, false);

	bool bClean = true;
	for (const FString& File : Files)
	{
		if (File.EndsWith(TEXT("BlueprintHelperOperatorActionResolverTests.cpp")))
		{
			continue;
		}

		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *File))
		{
			continue;
		}

		if (Text.Contains(Token))
		{
			Test.AddError(FString::Printf(TEXT("Forbidden operator action token '%s' found in %s"), *Token, *File));
			bClean = false;
		}
	}
	return bClean;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOperatorActionDispatchTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.OperatorDispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOperatorActionDispatchTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeOperatorActionTestBlueprint();
	UEdGraph* Graph = GetOperatorActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.StatementId = MakeOperatorActionTestObjectName(TEXT("Stmt"));
	Request.ProjectedContextHash = TEXT("operator_projected_context");
	Request.SemanticConstraintsHash = TEXT("operator_semantic_constraints");
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Op;
	Request.Semantic.Query = TEXT(">");
	FBlueprintActionDatabase::Get().RefreshAll();

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestNotEqual(
		TEXT("Op no longer reports the old migration marker"),
		Result.ErrorCode,
		BuildOperatorActionForbiddenToken(TEXT("operator_action_cluster_"), TEXT("migration_pending")));
	TestEqual(TEXT("Op resolves through FunctionAction cluster"), Result.ClusterKind, EBlueprintHelperSpawnerClusterKind::FunctionAction);
	TestEqual(TEXT("Op resolves to UE promotable operator"), Result.SelectedStableId, TEXT("promotable_operator:Greater"));
	TestTrue(TEXT("Op returns a UE node spawner"), Result.SelectedSpawner.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOperatorActionAddUsesTypePromotionFirstTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.OperatorAddStillUsesTypePromotionFirst",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOperatorActionAddUsesTypePromotionFirstTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeOperatorActionTestBlueprint();
	UEdGraph* Graph = GetOperatorActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request = MakeOperatorActionRequest(Blueprint, Graph, TEXT("add"));
	FBlueprintActionDatabase::Get().RefreshAll();

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("type promotion op resolves"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestEqual(TEXT("type promotion stable id"), Result.SelectedStableId, FString(TEXT("promotable_operator:Add")));
	TestTrue(TEXT("type promotion spawner"), Result.SelectedSpawner.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOperatorActionBooleanAndCallableTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.OperatorBooleanAndUsesRequestScopedPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOperatorActionBooleanAndCallableTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeOperatorActionTestBlueprint();
	UEdGraph* Graph = GetOperatorActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request = MakeOperatorActionRequest(Blueprint, Graph, TEXT("boolean_and"));
	FBlueprintActionDatabase::Get().RefreshAll();

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("boolean_and resolves"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestEqual(TEXT("boolean_and stable id"), Result.SelectedStableId, FString(TEXT("/Script/Engine.KismetMathLibrary:BooleanAND")));
	TestTrue(TEXT("boolean_and function"), Result.SelectedFunction.IsValid());
	TestTrue(TEXT("boolean_and spawner"), Result.SelectedSpawner.IsValid());
	TestEqual(TEXT("boolean_and node class"), Result.FunctionCandidate.NodeClassPath, FString(TEXT("/Script/BlueprintGraph.K2Node_CommutativeAssociativeBinaryOperator")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOperatorActionP0CommutativeCallableSurfaceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.OperatorP0CommutativeSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOperatorActionP0CommutativeCallableSurfaceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeOperatorActionTestBlueprint();
	UEdGraph* Graph = GetOperatorActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	struct FExpectedOp
	{
		const TCHAR* OperationId;
		const TCHAR* StableId;
	};

	const TArray<FExpectedOp> ExpectedOps = {
		{ TEXT("bitwise_and"), TEXT("/Script/Engine.KismetMathLibrary:And_IntInt") },
		{ TEXT("bitwise_or"), TEXT("/Script/Engine.KismetMathLibrary:Or_IntInt") },
		{ TEXT("boolean_or"), TEXT("/Script/Engine.KismetMathLibrary:BooleanOR") },
		{ TEXT("boolean_nand"), TEXT("/Script/Engine.KismetMathLibrary:BooleanNAND") },
		{ TEXT("max"), TEXT("/Script/Engine.KismetMathLibrary:FMax") },
		{ TEXT("min"), TEXT("/Script/Engine.KismetMathLibrary:FMin") },
		{ TEXT("string_append"), TEXT("/Script/Engine.KismetStringLibrary:Concat_StrStr") }
	};

	FBlueprintActionDatabase::Get().RefreshAll();

	bool bPassed = true;
	for (const FExpectedOp& ExpectedOp : ExpectedOps)
	{
		const FBlueprintHelperActionResolutionResult Result =
			FBlueprintHelperActionResolutionCore::Resolve(MakeOperatorActionRequest(Blueprint, Graph, ExpectedOp.OperationId));
		bPassed &= TestEqual(FString::Printf(TEXT("%s resolves"), ExpectedOp.OperationId), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
		bPassed &= TestEqual(FString::Printf(TEXT("%s stable id"), ExpectedOp.OperationId), Result.SelectedStableId, FString(ExpectedOp.StableId));
		bPassed &= TestEqual(
			FString::Printf(TEXT("%s node class"), ExpectedOp.OperationId),
			Result.FunctionCandidate.NodeClassPath,
			FString(TEXT("/Script/BlueprintGraph.K2Node_CommutativeAssociativeBinaryOperator")));
	}
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOperatorActionAbsCallableTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.OperatorAbsResolvesToCompactFunctionStableId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOperatorActionAbsCallableTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeOperatorActionTestBlueprint();
	UEdGraph* Graph = GetOperatorActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request = MakeOperatorActionRequest(Blueprint, Graph, TEXT("abs"));
	FBlueprintActionDatabase::Get().RefreshAll();

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("abs resolves"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestEqual(TEXT("abs stable id"), Result.SelectedStableId, FString(TEXT("/Script/Engine.KismetMathLibrary:Abs")));
	TestTrue(TEXT("abs function"), Result.SelectedFunction.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOperatorActionEnumEqualRejectedTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.OperatorEnumEqualRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOperatorActionEnumEqualRejectedTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeOperatorActionTestBlueprint();
	UEdGraph* Graph = GetOperatorActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(MakeOperatorActionRequest(Blueprint, Graph, TEXT("enum_equal")));
	TestEqual(TEXT("enum_equal rejected status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("enum_equal rejected code"), Result.ErrorCode, FString(TEXT("excluded_op_operation")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOperatorActionUnknownOpRejectedTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.OperatorUnknownOpRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOperatorActionUnknownOpRejectedTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeOperatorActionTestBlueprint();
	UEdGraph* Graph = GetOperatorActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(MakeOperatorActionRequest(Blueprint, Graph, TEXT("unknown_vectorish")));
	TestEqual(TEXT("unknown op rejected status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("unknown op rejected code"), Result.ErrorCode, FString(TEXT("unsupported_op_operation")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOperatorActionArrayIdenticalCallableTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.OperatorArrayIdenticalUsesCallArrayFunctionPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOperatorActionArrayIdenticalCallableTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeOperatorActionTestBlueprint();
	UEdGraph* Graph = GetOperatorActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request = MakeOperatorActionRequest(Blueprint, Graph, TEXT("array_identical"));
	Request.ContextEvidence.Add(TEXT("op.array_lhs_pin_type"), TEXT("array|int"));
	Request.ContextEvidence.Add(TEXT("op.array_rhs_pin_type"), TEXT("array|int"));
	FBlueprintActionDatabase::Get().RefreshAll();

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("array_identical resolves"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestEqual(TEXT("array_identical stable id"), Result.SelectedStableId, FString(TEXT("/Script/Engine.KismetArrayLibrary:Array_Identical")));
	TestTrue(TEXT("array_identical function"), Result.SelectedFunction.IsValid());
	TestTrue(TEXT("array_identical spawner"), Result.SelectedSpawner.IsValid());
	TestEqual(TEXT("array_identical node class"), Result.FunctionCandidate.NodeClassPath, FString(TEXT("/Script/BlueprintGraph.K2Node_CallArrayFunction")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOperatorActionSourceHygieneTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.OperatorSourceHygiene",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOperatorActionSourceHygieneTest::RunTest(const FString& Parameters)
{
	const FString SourceRoot = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper"),
		TEXT("BlueprintHelper"),
		TEXT("Source"),
		TEXT("BlueprintHelper"));

	TestTrue(TEXT("BlueprintHelper source root exists"), IFileManager::Get().DirectoryExists(*SourceRoot));

	const TArray<FString> ForbiddenTokens = {
		BuildOperatorActionForbiddenToken(TEXT("operator_action_cluster_"), TEXT("migration_pending")),
		BuildOperatorActionForbiddenToken(TEXT("FunctionActionCluster owns "), TEXT("semantic kind")),
		BuildOperatorActionForbiddenToken(TEXT("Greater_"), TEXT("IntInt")),
		BuildOperatorActionForbiddenToken(TEXT("Greater_"), TEXT("FloatFloat")),
		BuildOperatorActionForbiddenToken(TEXT("Greater_"), TEXT("DoubleDouble"))
	};

	bool bClean = true;
	for (const FString& Token : ForbiddenTokens)
	{
		bClean &= ScanBlueprintHelperSourceForOperatorForbiddenToken(*this, SourceRoot, Token);
	}

	TestTrue(TEXT("FunctionAction operator path has no migration-pending marker"), bClean);
	return true;
}

#endif
