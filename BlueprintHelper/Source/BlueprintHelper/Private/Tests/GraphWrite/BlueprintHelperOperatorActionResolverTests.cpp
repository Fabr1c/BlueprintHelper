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
