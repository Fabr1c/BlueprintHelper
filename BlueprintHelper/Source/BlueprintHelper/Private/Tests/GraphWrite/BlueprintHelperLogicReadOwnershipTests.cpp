#include "Misc/AutomationTest.h"
#include "Shared/BlueprintHelperLogicReadTypes.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicGroupBuilder.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperLogicReadEventTargetUsesAdapterBodyEntryTest,
	"BlueprintHelper.ReadContext.Logic.EventTargetUsesAdapterBodyEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperLogicReadEventTargetUsesAdapterBodyEntryTest::RunTest(const FString& Parameters)
{
	const TSharedRef<FJsonObject> RawJson = MakeShared<FJsonObject>();
	const TSharedRef<FJsonObject> AdapterBoundary = MakeShared<FJsonObject>();
	AdapterBoundary->SetStringField(TEXT("graph_name"), TEXT("EventGraph"));
	AdapterBoundary->SetStringField(TEXT("body_kind"), TEXT("k2.external_body"));
	const TSharedRef<FJsonObject> BodyEntry = MakeShared<FJsonObject>();
	BodyEntry->SetStringField(TEXT("node_guid"), TEXT("EVENTGUID"));
	AdapterBoundary->SetObjectField(TEXT("body_entry"), BodyEntry);
	const TSharedRef<FJsonObject> EntryBoundary = MakeShared<FJsonObject>();
	EntryBoundary->SetStringField(TEXT("node_ref"), TEXT("Event:OnShooterFireStartedInput"));
	EntryBoundary->SetStringField(TEXT("display_name"), TEXT("OnShooterFireStartedInput"));
	AdapterBoundary->SetArrayField(TEXT("entry_boundaries"), { MakeShared<FJsonValueObject>(EntryBoundary) });
	RawJson->SetObjectField(TEXT("adapter_boundary"), AdapterBoundary);

	const TSharedRef<FJsonObject> Graph = MakeShared<FJsonObject>();
	Graph->SetStringField(TEXT("name"), TEXT("EventGraph"));
	const TSharedRef<FJsonObject> CustomEvent = MakeShared<FJsonObject>();
	CustomEvent->SetStringField(TEXT("class"), TEXT("/Script/BlueprintGraph.K2Node_CustomEvent"));
	CustomEvent->SetStringField(TEXT("node_guid"), TEXT("CUSTOMGUID"));
	CustomEvent->SetStringField(TEXT("name"), TEXT("BH_OnShooterFireStartedInput"));
	const TSharedRef<FJsonObject> NativeEvent = MakeShared<FJsonObject>();
	NativeEvent->SetStringField(TEXT("class"), TEXT("/Script/BlueprintGraph.K2Node_Event"));
	NativeEvent->SetStringField(TEXT("node_guid"), TEXT("EVENTGUID"));
	NativeEvent->SetStringField(TEXT("member_name"), TEXT("OnShooterFireStartedInput"));
	Graph->SetArrayField(
		TEXT("nodes"),
		{
			MakeShared<FJsonValueObject>(CustomEvent),
			MakeShared<FJsonValueObject>(NativeEvent)
		});
	RawJson->SetArrayField(TEXT("graphs"), { MakeShared<FJsonValueObject>(Graph) });

	const FBlueprintHelperLogicGroupBuilder Builder;
	const FBlueprintHelperLogicJsonPayload Payload = Builder.BuildTargetEntry(
		RawJson,
		TEXT("/Game/BP_Test.BP_Test"),
		TEXT("EventGraph"),
		TEXT("OnShooterFireStartedInput"),
		EBlueprintHelperLogicScope::TargetEvent);

	TestTrue(TEXT("entry exists"), Payload.Entry.IsSet());
	if (!Payload.Entry.IsSet())
	{
		return false;
	}
	TestEqual(TEXT("entry kind is native event"), Payload.Entry->Kind, EBlueprintHelperLogicNodeKind::Event);
	TestEqual(TEXT("entry name"), Payload.Entry->Name, FString(TEXT("OnShooterFireStartedInput")));
	return true;
}
