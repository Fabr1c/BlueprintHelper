#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/BlueprintSignature/BlueprintHelperOverrideEventResolver.h"

#include "Engine/Blueprint.h"
#include "GameFramework/Character.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOverrideEventResolverAdditionalClassAliasTest,
	"BlueprintHelper.Signature.OverrideEventResolver.AdditionalClassAlias",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOverrideEventResolverAdditionalClassAliasTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperSignatureTestFixtures::MakeSignatureServiceActorBlueprint(TEXT("OverrideResolverAdditionalClass"));
	TestNotNull(TEXT("Blueprint exists"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperOverrideEventResolveRequest Request;
	Request.Blueprint = Blueprint;
	Request.RequestedEventName = TEXT("OnStartCrouch");
	Request.AdditionalCandidateClasses.Add(ACharacter::StaticClass());

	FBlueprintHelperOverrideEventResolver Resolver;
	FBlueprintHelperOverrideEventResolveResult Result = Resolver.Resolve(Request);
	TestTrue(TEXT("resolver succeeds through additional candidate class alias"), Result.bResolved);
	TestEqual(TEXT("resolved character event"), Result.ResolvedEventName.ToString(), FString(TEXT("K2_OnStartCrouch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOverrideEventResolverCandidateAliasDiagnosticsTest,
	"BlueprintHelper.Signature.OverrideEventResolver.CandidateAliasDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOverrideEventResolverCandidateAliasDiagnosticsTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperSignatureTestFixtures::MakeSignatureServiceBlueprint(
		TEXT("OverrideResolverAliasDiagnostics"),
		ACharacter::StaticClass());
	TestNotNull(TEXT("Blueprint exists"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperOverrideEventResolver Resolver;
	FBlueprintHelperOverrideEventResolveResult Result = Resolver.Resolve(Blueprint, TEXT("DefinitelyMissingEvent"));
	TestFalse(TEXT("resolver does not resolve"), Result.bResolved);

	const FBlueprintHelperOverrideEventCandidate* OnStartCrouchCandidate = Result.Candidates.FindByPredicate(
		[](const FBlueprintHelperOverrideEventCandidate& Candidate)
		{
			return Candidate.FunctionName == TEXT("K2_OnStartCrouch");
		});
	TestNotNull(TEXT("character event candidate is reported"), OnStartCrouchCandidate);
	if (OnStartCrouchCandidate)
	{
		TestTrue(
			TEXT("candidate exposes normalized script/display alias"),
			OnStartCrouchCandidate->MatchAliases.Contains(TEXT("OnStartCrouch")));
	}
	return true;
}

#endif
