#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

class FBlueprintHelperGraphResolverPolicyTestsLocalUtils
{
public:
	static FString MakeObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static UBlueprint* MakeTransientActorBlueprint()
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperResolverPolicy/%s"),
			*MakeObjectName(TEXT("Pkg"))));
		Package->SetDirtyFlag(false);

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*MakeObjectName(TEXT("BP_ResolverPolicy")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperGraphResolverPolicyTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static bool HasDiagnosticCode(const FBlueprintHelperDiagnosticSet& Diagnostics, const FString& Code)
	{
		for (const FBlueprintHelperDiagnosticItem& Item : Diagnostics.Items)
		{
			if (Item.Code == Code)
			{
				return true;
			}
		}
		return false;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphResolverMutationRejectsEmptyBlueprintPathTest,
	"BlueprintHelper.GraphWrite.ResolverPolicy.MutationRejectsEmptyBlueprintPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphResolverMutationRejectsEmptyBlueprintPathTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperGraphTarget Target;
	FBlueprintHelperDiagnosticSet Diag;

	UBlueprint* Blueprint = Resolver.ResolveBlueprint(Target, Diag, FBlueprintHelperResolvePolicy::Mutation());

	TestNull(TEXT("mutation policy rejects empty blueprint path"), Blueprint);
	TestTrue(TEXT("diagnostics has errors"), Diag.HasErrors());
	TestTrue(
		TEXT("diagnostics include target_blueprint_required"),
		FBlueprintHelperGraphResolverPolicyTestsLocalUtils::HasDiagnosticCode(Diag, TEXT("target_blueprint_required")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphResolverMutationReportsEmptyGraphAndBlueprintTargetTest,
	"BlueprintHelper.GraphWrite.ResolverPolicy.MutationReportsEmptyGraphAndBlueprintTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphResolverMutationReportsEmptyGraphAndBlueprintTargetTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperGraphTarget Target;
	FBlueprintHelperDiagnosticSet Diag;

	UEdGraph* Graph = Resolver.ResolveGraph(Target, Diag, FBlueprintHelperResolvePolicy::Mutation());

	TestNull(TEXT("mutation policy rejects empty graph target"), Graph);
	TestTrue(TEXT("diagnostics has errors"), Diag.HasErrors());
	TestTrue(
		TEXT("diagnostics include target_blueprint_required"),
		FBlueprintHelperGraphResolverPolicyTestsLocalUtils::HasDiagnosticCode(Diag, TEXT("target_blueprint_required")));
	TestTrue(
		TEXT("diagnostics include target_graph_required"),
		FBlueprintHelperGraphResolverPolicyTestsLocalUtils::HasDiagnosticCode(Diag, TEXT("target_graph_required")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphResolverMutationRejectsEmptyGraphNameTest,
	"BlueprintHelper.GraphWrite.ResolverPolicy.MutationRejectsEmptyGraphName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphResolverMutationRejectsEmptyGraphNameTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphResolverPolicyTestsLocalUtils::MakeTransientActorBlueprint();
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperGraphTarget Target;
	Target.BlueprintPath = Blueprint->GetPathName();
	FBlueprintHelperDiagnosticSet Diag;

	UEdGraph* Graph = Resolver.ResolveGraph(Target, Diag, FBlueprintHelperResolvePolicy::Mutation());

	TestNull(TEXT("mutation policy rejects empty graph name"), Graph);
	TestTrue(TEXT("diagnostics has errors"), Diag.HasErrors());
	TestTrue(
		TEXT("diagnostics include target_graph_required"),
		FBlueprintHelperGraphResolverPolicyTestsLocalUtils::HasDiagnosticCode(Diag, TEXT("target_graph_required")));
	return true;
}

#endif
