#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/BlueprintSignature/BlueprintHelperOverrideEventResolver.h"

#include "Engine/Blueprint.h"
#include "Misc/AutomationTest.h"
#include "Tests/BlueprintSignature/BlueprintHelperSignatureTestFixtures.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOverrideEventResolverExactBeginPlayTest,
	"BlueprintHelper.Signature.OverrideEventResolver.ExactBeginPlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOverrideEventResolverExactBeginPlayTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperSignatureTestFixtures::MakeSignatureServiceActorBlueprint(TEXT("OverrideResolverBeginPlay"));
	TestNotNull(TEXT("Blueprint exists"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperOverrideEventResolver Resolver;
	FBlueprintHelperOverrideEventResolveResult Result = Resolver.Resolve(Blueprint, TEXT("ReceiveBeginPlay"));
	TestTrue(TEXT("resolver succeeds"), Result.bResolved);
	TestEqual(TEXT("resolved event"), Result.ResolvedEventName.ToString(), FString(TEXT("ReceiveBeginPlay")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOverrideEventResolverNotFoundReportsCandidatesTest,
	"BlueprintHelper.Signature.OverrideEventResolver.NotFoundReportsCandidates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOverrideEventResolverNotFoundReportsCandidatesTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperSignatureTestFixtures::MakeSignatureServiceActorBlueprint(TEXT("OverrideResolverMissing"));
	TestNotNull(TEXT("Blueprint exists"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperOverrideEventResolver Resolver;
	FBlueprintHelperOverrideEventResolveResult Result = Resolver.Resolve(Blueprint, TEXT("DefinitelyMissingEvent"));
	TestFalse(TEXT("resolver does not resolve"), Result.bResolved);
	TestTrue(TEXT("resolver reports candidates"), Result.Candidates.Num() > 0);
	return true;
}

#endif
