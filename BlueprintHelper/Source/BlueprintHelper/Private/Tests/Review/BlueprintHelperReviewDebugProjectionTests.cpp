#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "UI/Review/BlueprintHelperReviewGenericDebugProjectionAdapter.h"
#include "UI/Review/BlueprintHelperReviewDebugProjectionRegistry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewDebugProjectionRegistryUsesEventTypeTest,
	"BlueprintHelper.Review.DebugProjection.RegistryUsesEventType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewDebugProjectionRegistryUsesEventTypeTest::RunTest(const FString& Parameters)
{
	TSharedRef<FBlueprintHelperReviewDebugProjectionRegistry> Registry =
		MakeShared<FBlueprintHelperReviewDebugProjectionRegistry>();
	FString Error;
	const bool bRegistered = Registry->RegisterProjectionAdapter(
		MakeShared<FBlueprintHelperReviewGenericDebugProjectionAdapter>(TEXT("review.action.result")),
		Error);

	TSharedRef<FJsonObject> EventJson = MakeShared<FJsonObject>();
	EventJson->SetStringField(TEXT("event_type"), TEXT("review.action.result"));
	EventJson->SetStringField(TEXT("review_event_id"), TEXT("change_001"));
	EventJson->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_Test"));
	EventJson->SetStringField(TEXT("target_key"), TEXT("blueprint_variable:SmokeFloat"));
	EventJson->SetStringField(TEXT("result"), TEXT("accepted"));
	EventJson->SetStringField(TEXT("message"), TEXT("accepted"));

	const TArray<FBlueprintHelperReviewDebugEventModel> Events = Registry->ProjectEvent(EventJson);

	TestTrue(TEXT("debug projection adapter registers"), bRegistered);
	TestTrue(TEXT("debug projection error stays empty"), Error.IsEmpty());
	TestEqual(TEXT("one projected timeline item"), Events.Num(), 1);
	if (Events.Num() == 1)
	{
		TestEqual(TEXT("projected review event id"), Events[0].ReviewEventId, FString(TEXT("change_001")));
		TestEqual(TEXT("projected result"), Events[0].Result, FString(TEXT("accepted")));
	}
	return true;
}

#endif
