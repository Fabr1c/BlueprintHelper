#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Systems/ToolClusters/GraphWrite/Patch/BlueprintHelperPatchOperationHandlerRegistry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperPatchOperationHandlerRegistryCoverageTest,
	"BlueprintHelper.GraphWrite.PatchOperationHandler.RegistryCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperPatchOperationHandlerRegistryCoverageTest::RunTest(const FString&)
{
	const TArray<FString> ExpectedPatchKinds =
	{
		TEXT("set_pin_default"),
		TEXT("set_node_comment"),
		TEXT("connect_pins"),
		TEXT("disconnect_link"),
		TEXT("replace_link"),
		TEXT("delete_owned_node")
	};

	for (const FString& PatchKind : ExpectedPatchKinds)
	{
		TestTrue(
			FString::Printf(TEXT("patch handler registered: %s"), *PatchKind),
			FBlueprintHelperPatchOperationHandlerRegistry::IsPatchKindRegistered(PatchKind));
	}

	const IBlueprintHelperPatchOperationHandler* Handler =
		FBlueprintHelperPatchOperationHandlerRegistry::FindHandler(TEXT("set_pin_default"));
	TestNotNull(TEXT("set_pin_default handler is findable"), Handler);
	if (Handler)
	{
		TSharedRef<FJsonObject> PatchJson = MakeShared<FJsonObject>();
		PatchJson->SetStringField(TEXT("value"), TEXT("42"));
		FBlueprintHelperGraphWriteMutationIntent Intent;
		FBlueprintHelperToolError Error;
		TestTrue(TEXT("set_pin_default builds a mutation intent"), Handler->BuildMutationIntent(PatchJson, Intent, Error));
		TestEqual(TEXT("set_pin_default intent kind"), Intent.Kind, EBlueprintHelperGraphWriteMutationIntentKind::SetPinDefault);
		TestEqual(TEXT("set_pin_default intent value"), Intent.DefaultValue, FString(TEXT("42")));
	}

	TestNull(
		TEXT("unsupported handler is absent"),
		FBlueprintHelperPatchOperationHandlerRegistry::FindHandler(TEXT("set_call_target")));
	return true;
}

#endif
