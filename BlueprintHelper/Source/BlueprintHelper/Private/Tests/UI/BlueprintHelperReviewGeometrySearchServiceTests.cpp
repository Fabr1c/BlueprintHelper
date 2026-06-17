// BlueprintHelper Review geometry search service tests.

#include "UI/Review/BlueprintHelperReviewGeometrySearchService.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewGeometrySearchService_NormalizesSearchText,
	"BlueprintHelper.Review.Panel.GeometrySearchService.NormalizesSearchText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewGeometrySearchService_NormalizesSearchText::RunTest(const FString&)
{
	TestEqual(
		TEXT("normalizes punctuation and case"),
		FBlueprintHelperReviewGeometrySearchService::NormalizeSearchText(TEXT("Smoke.Label_01")),
		FString(TEXT("smokelabel01")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewGeometrySearchService_BuildsDisplayCandidates,
	"BlueprintHelper.Review.Panel.GeometrySearchService.BuildsDisplayCandidates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewGeometrySearchService_BuildsDisplayCandidates::RunTest(const FString&)
{
	TArray<FString> Candidates;
	FBlueprintHelperReviewGeometrySearchService::AddDisplaySearchCandidatesFromText(
		TEXT("variable:SmokeLabel"),
		Candidates);

	TestTrue(TEXT("full candidate included"), Candidates.Contains(TEXT("variable:SmokeLabel")));
	TestTrue(TEXT("tail candidate included"), Candidates.Contains(TEXT("SmokeLabel")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewGeometrySearchService_MatchesDisplayAndGeometryText,
	"BlueprintHelper.Review.Panel.GeometrySearchService.MatchesDisplayAndGeometryText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewGeometrySearchService_MatchesDisplayAndGeometryText::RunTest(const FString&)
{
	TestTrue(
		TEXT("display search matches tail"),
		FBlueprintHelperReviewGeometrySearchService::DisplaySearchTextMatches(
			TEXT("Smoke Label"),
			TEXT("variable:SmokeLabel")));
	TestTrue(
		TEXT("geometry search matches token"),
		FBlueprintHelperReviewGeometrySearchService::GeometrySearchTextMatches(
			TEXT("DoorFrame"),
			TEXT("component:DoorFrame.StaticMesh")));
	TestFalse(
		TEXT("short row does not match display"),
		FBlueprintHelperReviewGeometrySearchService::DisplaySearchTextMatches(
			TEXT("x"),
			TEXT("variable:SmokeLabel")));
	return true;
}

#endif
