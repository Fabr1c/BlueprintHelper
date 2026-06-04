#include "Systems/ToolClusters/GraphWrite/AutoSearch/BlueprintHelperGraphWriteAutoSearchPolicyResolver.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteAutoSearchPolicy_ParseEnabledPolicy,
	"BlueprintHelper.GraphWrite.AutoSearch.Policy.ParseEnabledPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperGraphWriteAutoSearchPolicy_ParseEnabledPolicy::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> Write = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> Policy = MakeShared<FJsonObject>();
	Policy->SetStringField(TEXT("mode"), TEXT("on_preview_resolution_failure"));
	Write->SetObjectField(TEXT("auto_search_policy"), Policy);

	FBlueprintHelperGraphWriteAutoSearchPolicy Parsed;
	FString Error;
	TestTrue(TEXT("policy parses"), FBlueprintHelperGraphWriteAutoSearchPolicyResolver::TryParseFromWriteObject(Write, Parsed, Error));
	TestTrue(TEXT("policy enabled"), Parsed.bEnablePreviewRecovery);
	TestEqual(TEXT("default max candidates"), Parsed.MaxCandidatesPerStatement, 3);
	TestEqual(TEXT("default max statements"), Parsed.MaxAutoSearchStatements, 16);
	TestEqual(TEXT("default budget ms"), Parsed.MaxTotalSearchMs, 120);
	TestEqual(TEXT("default detail level"), Parsed.DetailLevel, FString(TEXT("short")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteAutoSearchPolicy_DisabledWhenMissingOrOff,
	"BlueprintHelper.GraphWrite.AutoSearch.Policy.DisabledWhenMissingOrOff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperGraphWriteAutoSearchPolicy_DisabledWhenMissingOrOff::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphWriteAutoSearchPolicy Parsed;
	FString Error;
	TestTrue(TEXT("null write object is disabled"), FBlueprintHelperGraphWriteAutoSearchPolicyResolver::TryParseFromWriteObject(nullptr, Parsed, Error));
	TestFalse(TEXT("null write object disabled"), Parsed.bEnablePreviewRecovery);

	TSharedRef<FJsonObject> MissingPolicyWrite = MakeShared<FJsonObject>();
	TestTrue(TEXT("missing policy parses disabled"), FBlueprintHelperGraphWriteAutoSearchPolicyResolver::TryParseFromWriteObject(MissingPolicyWrite, Parsed, Error));
	TestFalse(TEXT("missing policy disabled"), Parsed.bEnablePreviewRecovery);

	TSharedRef<FJsonObject> OffWrite = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> OffPolicy = MakeShared<FJsonObject>();
	OffPolicy->SetStringField(TEXT("mode"), TEXT("off"));
	OffWrite->SetObjectField(TEXT("auto_search_policy"), OffPolicy);
	TestTrue(TEXT("off policy parses disabled"), FBlueprintHelperGraphWriteAutoSearchPolicyResolver::TryParseFromWriteObject(OffWrite, Parsed, Error));
	TestFalse(TEXT("off policy disabled"), Parsed.bEnablePreviewRecovery);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteAutoSearchPolicy_RejectsInvalidBudget,
	"BlueprintHelper.GraphWrite.AutoSearch.Policy.RejectsInvalidBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperGraphWriteAutoSearchPolicy_RejectsInvalidBudget::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> Write = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> Policy = MakeShared<FJsonObject>();
	Policy->SetStringField(TEXT("mode"), TEXT("on_preview_resolution_failure"));
	Policy->SetNumberField(TEXT("max_candidates_per_statement"), 0);
	Write->SetObjectField(TEXT("auto_search_policy"), Policy);

	FBlueprintHelperGraphWriteAutoSearchPolicy Parsed;
	FString Error;
	TestFalse(TEXT("invalid candidate count rejected"), FBlueprintHelperGraphWriteAutoSearchPolicyResolver::TryParseFromWriteObject(Write, Parsed, Error));
	TestTrue(TEXT("error code present"), Error.Contains(TEXT("invalid_graphwrite_autosearch_policy")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteAutoSearchPolicy_RejectsInvalidModeAndDetail,
	"BlueprintHelper.GraphWrite.AutoSearch.Policy.RejectsInvalidModeAndDetail",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperGraphWriteAutoSearchPolicy_RejectsInvalidModeAndDetail::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> BadModeWrite = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> BadModePolicy = MakeShared<FJsonObject>();
	BadModePolicy->SetStringField(TEXT("mode"), TEXT("always"));
	BadModeWrite->SetObjectField(TEXT("auto_search_policy"), BadModePolicy);

	FBlueprintHelperGraphWriteAutoSearchPolicy Parsed;
	FString Error;
	TestFalse(TEXT("invalid mode rejected"), FBlueprintHelperGraphWriteAutoSearchPolicyResolver::TryParseFromWriteObject(BadModeWrite, Parsed, Error));
	TestTrue(TEXT("mode error code present"), Error.Contains(TEXT("invalid_graphwrite_autosearch_policy")));

	TSharedRef<FJsonObject> BadModeTypeWrite = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> BadModeTypePolicy = MakeShared<FJsonObject>();
	BadModeTypePolicy->SetNumberField(TEXT("mode"), 1);
	BadModeTypeWrite->SetObjectField(TEXT("auto_search_policy"), BadModeTypePolicy);
	TestFalse(TEXT("non-string mode rejected"), FBlueprintHelperGraphWriteAutoSearchPolicyResolver::TryParseFromWriteObject(BadModeTypeWrite, Parsed, Error));
	TestTrue(TEXT("mode type error code present"), Error.Contains(TEXT("invalid_graphwrite_autosearch_policy")));

	TSharedRef<FJsonObject> BadDetailWrite = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> BadDetailPolicy = MakeShared<FJsonObject>();
	BadDetailPolicy->SetStringField(TEXT("mode"), TEXT("on_preview_resolution_failure"));
	BadDetailPolicy->SetStringField(TEXT("detail_level"), TEXT("full"));
	BadDetailWrite->SetObjectField(TEXT("auto_search_policy"), BadDetailPolicy);

	TestFalse(TEXT("invalid detail rejected"), FBlueprintHelperGraphWriteAutoSearchPolicyResolver::TryParseFromWriteObject(BadDetailWrite, Parsed, Error));
	TestTrue(TEXT("detail error code present"), Error.Contains(TEXT("invalid_graphwrite_autosearch_policy")));
	return true;
}

#endif
