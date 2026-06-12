#include "Misc/AutomationTest.h"
#include "Shared/BlueprintHelperLogicReadTypes.h"
#include "Systems/ToolClusters/GraphWrite/Logic/Utils/BlueprintHelperGraphWriteClassificationUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperLogicReadLinkOwnershipSerializesTest,
	"BlueprintHelper.LogicRead.Ownership.LinkOwnershipSerializes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperLogicReadLinkOwnershipSerializesTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperLogicLink Link;
	Link.LinkRef = TEXT("links[0]");
	Link.Type = EBlueprintHelperLogicLinkType::Exec;
	Link.FromPin = TEXT("then");
	Link.PinRef = TEXT("then");
	Link.ToNode = TEXT("nodes[1]");
	Link.ToPin = TEXT("execute");
	Link.Ownership = TEXT("external_user");

	const TSharedRef<FJsonObject> Json = Link.ToJson();
	TestEqual(TEXT("ownership serializes"), Json->GetStringField(TEXT("ownership")), FString(TEXT("external_user")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperLogicReadOwnershipClassifiesLinksTest,
	"BlueprintHelper.LogicRead.Ownership.ClassifiesLinks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperLogicReadOwnershipClassifiesLinksTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("user to user is external_user"),
		FBlueprintHelperGraphWriteClassificationUtils::ClassifyLinkOwnership(false, false),
		FString(TEXT("external_user")));
	TestEqual(
		TEXT("owned to owned is owned_internal"),
		FBlueprintHelperGraphWriteClassificationUtils::ClassifyLinkOwnership(true, true),
		FString(TEXT("owned_internal")));
	TestEqual(
		TEXT("owned to user is external_boundary"),
		FBlueprintHelperGraphWriteClassificationUtils::ClassifyLinkOwnership(true, false),
		FString(TEXT("external_boundary")));
	TestEqual(
		TEXT("user to owned is external_boundary"),
		FBlueprintHelperGraphWriteClassificationUtils::ClassifyLinkOwnership(false, true),
		FString(TEXT("external_boundary")));
	return true;
}
