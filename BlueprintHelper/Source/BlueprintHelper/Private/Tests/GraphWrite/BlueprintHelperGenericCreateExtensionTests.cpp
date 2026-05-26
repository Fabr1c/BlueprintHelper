#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
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

#endif
