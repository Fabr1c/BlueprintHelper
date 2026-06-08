#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextScope.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuilderRegistry.h"

#include "Blueprint/UserWidget.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node.h"
#include "K2Node_ConstructObjectFromClass.h"
#include "K2Node_MakeArray.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Tests/GraphWrite/BlueprintHelperGraphWriteTestUtils.h"
#include "UObject/Package.h"

namespace
{
static FString MakeGenericCreateName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeGenericCreateBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperGenericCreateExtension/%s"),
		*MakeGenericCreateName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeGenericCreateName(TEXT("BP_GenericCreate")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperGenericCreateExtensionTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsMakeArrayCreateResolvesTest,
	"BlueprintHelper.GraphWrite.GenericOps.Create.MakeArrayResolves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericOpsMakeArrayCreateResolvesTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericCreateBlueprint();
	UEdGraph* Graph = Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.StatementId = MakeGenericCreateName(TEXT("Stmt"));
	Request.ProjectedContextHash = TEXT("generic_create_extension_projected_context");
	Request.SemanticConstraintsHash = TEXT("generic_create_extension_semantic_constraints");
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Create;
	Request.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Create;
	Request.Semantic.CreateOperation = TEXT("make_array");
	Request.Semantic.ArgumentTypes.Add(TEXT("element"), TEXT("int"));
	Request.MaxCandidates = 4;

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("make_array resolved"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestTrue(TEXT("make_array selected spawner"), Result.SelectedSpawner.IsValid());
	TestTrue(TEXT("make_array node class"), Result.NodeClass.Contains(TEXT("K2Node_MakeArray")));

	FString SpawnError;
	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
		Graph,
		Result,
		FVector2D(320.0f, 64.0f),
		SpawnError);
	TestNotNull(TEXT("make_array spawned node"), SpawnedNode);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsMakeArrayJsonPinTypeFragmentTest,
	"BlueprintHelper.GraphWrite.GenericOps.Create.MakeArrayFragmentPinsUseJsonPinType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericOpsMakeArrayJsonPinTypeFragmentTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericCreateBlueprint();
	UEdGraph* Graph = Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	FBlueprintHelperGraphStatementIR Statement;
	Statement.StatementId = MakeGenericCreateName(TEXT("Stmt"));
	Statement.Path = TEXT("$.statements[0]");
	Statement.Kind = EBlueprintHelperGraphStatementKind::Create;
	Statement.PatternName = TEXT("create");
	Statement.CreateOperation = TEXT("make_array");
	Statement.PinType = TEXT("{\"category\":\"string\"}");

	TSharedPtr<FBlueprintHelperGraphExpressionIR> Item = MakeShared<FBlueprintHelperGraphExpressionIR>();
	Item->ExpressionId = MakeGenericCreateName(TEXT("Expr"));
	Item->Path = TEXT("$.statements[0].args.item");
	Item->Kind = EBlueprintHelperGraphExpressionKind::Literal;
	Item->Type = TEXT("string");
	Item->LiteralValue = TEXT("typed item");
	Statement.Args.Add(TEXT("item"), Item);

	FBlueprintHelperActionContextScope ActionContextScope;
	FString ScopeError;
	const bool bScopeBuilt = FBlueprintHelperGraphWriteTestUtils::BuildActionContextScopeForStatement(
		*this,
		Blueprint,
		Graph,
		Statement,
		TEXT("make array action context demands exist"),
		TEXT("generic_create_extension_tests"),
		TEXT("make_array_json_pin_type"),
		ActionContextScope,
		ScopeError);
	TestTrue(TEXT("make array action context scope builds"), bScopeBuilt);
	if (!ScopeError.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("scope error: %s"), *ScopeError));
	}
	if (!bScopeBuilt)
	{
		return false;
	}

	FBlueprintHelperNodeFragment Fragment;
	FString BuildError;
	const bool bBuilt = FBlueprintHelperGraphFragmentBuilderRegistry::TryBuildStatement(
		Graph,
		&ActionContextScope,
		Statement,
		Fragment,
		BuildError);
	TestTrue(TEXT("make array fragment builds"), bBuilt);
	if (!BuildError.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("build error: %s"), *BuildError));
	}
	if (!bBuilt)
	{
		return false;
	}

	UK2Node_MakeArray* MakeArray = FBlueprintHelperGraphWriteTestUtils::FindSingleFragmentNode<UK2Node_MakeArray>(
		*this,
		Fragment,
		TEXT("make array"));
	TestNotNull(TEXT("make array node"), MakeArray);
	if (!MakeArray)
	{
		return false;
	}

	UEdGraphPin* OutputPin = MakeArray->GetOutputPin();
	TestNotNull(TEXT("make array output pin"), OutputPin);
	if (!OutputPin)
	{
		return false;
	}

	UEdGraphPin* FirstInputPin = nullptr;
	for (UEdGraphPin* Pin : MakeArray->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input)
		{
			FirstInputPin = Pin;
			break;
		}
	}
	TestNotNull(TEXT("make array first input pin"), FirstInputPin);
	if (!FirstInputPin)
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestEqual(TEXT("make array output category"), OutputPin->PinType.PinCategory, UEdGraphSchema_K2::PC_String);
	bPassed &= TestEqual(TEXT("make array output container"), OutputPin->PinType.ContainerType, EPinContainerType::Array);
	bPassed &= TestEqual(TEXT("make array first input category"), FirstInputPin->PinType.PinCategory, UEdGraphSchema_K2::PC_String);
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsCreateWidgetFragmentAppliesClassPathTest,
	"BlueprintHelper.GraphWrite.GenericOps.Create.CreateWidgetFragmentAppliesClassPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericOpsCreateWidgetFragmentAppliesClassPathTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericCreateBlueprint();
	UEdGraph* Graph = Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	FBlueprintHelperGraphStatementIR Statement;
	Statement.StatementId = MakeGenericCreateName(TEXT("Stmt"));
	Statement.Path = TEXT("$.statements[0]");
	Statement.Kind = EBlueprintHelperGraphStatementKind::Create;
	Statement.PatternName = TEXT("create");
	Statement.CreateOperation = TEXT("create_widget");
	Statement.ClassPath = UUserWidget::StaticClass()->GetPathName();

	FBlueprintHelperActionContextScope ActionContextScope;
	FString ScopeError;
	const bool bScopeBuilt = FBlueprintHelperGraphWriteTestUtils::BuildActionContextScopeForStatement(
		*this,
		Blueprint,
		Graph,
		Statement,
		TEXT("create widget action context demands exist"),
		TEXT("generic_create_extension_tests"),
		TEXT("create_widget_class_path"),
		ActionContextScope,
		ScopeError);
	TestTrue(TEXT("create widget action context scope builds"), bScopeBuilt);
	if (!ScopeError.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("scope error: %s"), *ScopeError));
	}
	if (!bScopeBuilt)
	{
		return false;
	}

	FBlueprintHelperNodeFragment Fragment;
	FString BuildError;
	const bool bBuilt = FBlueprintHelperGraphFragmentBuilderRegistry::TryBuildStatement(
		Graph,
		&ActionContextScope,
		Statement,
		Fragment,
		BuildError);
	TestTrue(TEXT("create widget fragment builds"), bBuilt);
	if (!BuildError.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("build error: %s"), *BuildError));
	}
	if (!bBuilt)
	{
		return false;
	}

	UK2Node_ConstructObjectFromClass* CreateWidgetNode =
		Cast<UK2Node_ConstructObjectFromClass>(Fragment.PrimaryNode);
	TestNotNull(TEXT("create widget class-backed node"), CreateWidgetNode);
	if (!CreateWidgetNode)
	{
		return false;
	}

	UEdGraphPin* ClassPin = CreateWidgetNode->GetClassPin();
	UEdGraphPin* ResultPin = CreateWidgetNode->GetResultPin();
	bool bPassed = true;
	bPassed &= TestNotNull(TEXT("create widget class pin"), ClassPin);
	bPassed &= TestNotNull(TEXT("create widget result pin"), ResultPin);
	if (ClassPin)
	{
		bPassed &= TestTrue(
			TEXT("create widget class pin default"),
			ClassPin->DefaultObject == UUserWidget::StaticClass());
	}
	if (ResultPin)
	{
		bPassed &= TestTrue(
			TEXT("create widget result pin type"),
			ResultPin->PinType.PinSubCategoryObject.Get() == UUserWidget::StaticClass());
	}
	return bPassed;
}

#endif
