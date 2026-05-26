#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace
{
static FString MakeTransformPolicyName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeTransformPolicyBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperTransformPolicy/%s"),
		*MakeTransformPolicyName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeTransformPolicyName(TEXT("BP_TransformPolicy")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperTransformConversionPolicyTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsFunctionBackedConversionWrongOwnerTest,
	"BlueprintHelper.GraphWrite.GenericOps.Transform.FunctionBackedConversionWrongOwner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericOpsFunctionBackedConversionWrongOwnerTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeTransformPolicyBlueprint();
	UEdGraph* Graph = Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.StatementId = MakeTransformPolicyName(TEXT("Stmt"));
	Request.ProjectedContextHash = TEXT("transform_policy_projected_context");
	Request.SemanticConstraintsHash = TEXT("transform_policy_semantic_constraints");
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Convert;
	Request.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Convert;
	Request.Semantic.TransformOperation = TEXT("function_conversion");
	Request.Semantic.FunctionOperation = TEXT("convert_function");
	Request.MaxCandidates = 4;

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("function conversion wrong generic owner"), Result.Status, EBlueprintHelperActionResolutionStatus::UnsupportedIntent);
	TestEqual(TEXT("wrong owner code"), Result.ErrorCode, FString(TEXT("function_backed_operation_wrong_owner")));
	TestFalse(TEXT("wrong owner has no fake spawner"), Result.SelectedSpawner.IsValid());
	return true;
}

#endif
