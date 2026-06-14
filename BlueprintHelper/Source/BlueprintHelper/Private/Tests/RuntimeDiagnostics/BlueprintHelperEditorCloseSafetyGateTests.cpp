#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Systems/Editor/BlueprintHelperEditorCloseSafetyGate.h"
#include "Systems/SourceControl/BlueprintHelperSourceControlService.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEditorCloseSafetyGateBlocksCheckoutRequiredTest,
	"BlueprintHelper.Editor.CloseSafetyGate.BlocksCheckoutRequired",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEditorCloseSafetyGateBlocksCheckoutRequiredTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperSourceControlResult SourceControl;
	SourceControl.bSuccess = true;
	SourceControl.Status = TEXT("checkout_required");
	SourceControl.RecommendedAction = TEXT("Run blueprinthelper_source_control_checkout for dirty assets before close_editor(save_all=true).");

	FBlueprintHelperSourceControlFileState FileState;
	FileState.Input = TEXT("/Game/BP_Dirty.BP_Dirty");
	FileState.Status = TEXT("checkout_required");
	FileState.bCanEdit = false;
	FileState.AgentHint = TEXT("Run blueprinthelper_source_control_checkout for this target before editing.");
	SourceControl.Files.Add(FileState);

	const FBlueprintHelperEditorCloseSafetyGate Gate;
	const FBlueprintHelperEditorCloseSafetyGateResult Result = Gate.EvaluateDirtyPackageStatus(SourceControl);
	const TSharedRef<FJsonObject> Json = Result.ToJson();

	TestFalse(TEXT("close is blocked"), Result.bCanProceed);
	TestEqual(TEXT("block code"), Result.Code, FString(TEXT("checkout_required")));
	TestEqual(TEXT("block message uses recommended action"), Result.Message, SourceControl.RecommendedAction);
	TestTrue(TEXT("result has can_proceed"), Json->HasField(TEXT("can_proceed")));
	TestTrue(TEXT("result has code"), Json->HasField(TEXT("code")));
	TestTrue(TEXT("result has message"), Json->HasField(TEXT("message")));
	TestTrue(TEXT("result has source control gate"), Json->HasField(TEXT("source_control_gate")));
	const TSharedPtr<FJsonObject> SourceControlGateJson = Json->GetObjectField(TEXT("source_control_gate"));
	TestNotNull(TEXT("source control gate json is present"), SourceControlGateJson.Get());
	if (SourceControlGateJson.IsValid())
	{
		TestEqual(TEXT("source control gate status is preserved"), SourceControlGateJson->GetStringField(TEXT("status")), FString(TEXT("checkout_required")));
		TestEqual(TEXT("source control gate recommended action is preserved"), SourceControlGateJson->GetStringField(TEXT("recommended_action")), SourceControl.RecommendedAction);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEditorCloseSafetyGateAutoCheckoutRequiredTest,
	"BlueprintHelper.Editor.CloseSafetyGate.AutoCheckoutRequired",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEditorCloseSafetyGateAutoCheckoutRequiredTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperSourceControlResult SourceControl;
	SourceControl.bSuccess = true;
	SourceControl.Status = TEXT("editable");

	FBlueprintHelperSourceControlFileState EditableFile;
	EditableFile.Status = TEXT("editable");
	EditableFile.bCanEdit = true;
	SourceControl.Files.Add(EditableFile);

	FBlueprintHelperSourceControlFileState CheckoutFile;
	CheckoutFile.Status = TEXT("checkout_required");
	CheckoutFile.bCanEdit = false;
	CheckoutFile.bCanCheckOut = true;
	SourceControl.Files.Add(CheckoutFile);

	const FBlueprintHelperEditorCloseSafetyGate Gate;
	TestTrue(TEXT("checkout-required files request auto checkout"), Gate.ShouldAttemptAutoCheckout(SourceControl));

	CheckoutFile.Status = TEXT("checked_out_by_other");
	CheckoutFile.bCanCheckOut = false;
	SourceControl.Files[1] = CheckoutFile;
	TestFalse(TEXT("checked-out-by-other files do not request auto checkout"), Gate.ShouldAttemptAutoCheckout(SourceControl));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEditorCloseSafetyGateAllowsEditablePackagesTest,
	"BlueprintHelper.Editor.CloseSafetyGate.AllowsEditablePackages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEditorCloseSafetyGateAllowsEditablePackagesTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperSourceControlResult SourceControl;
	SourceControl.bSuccess = true;
	SourceControl.Status = TEXT("editable");

	FBlueprintHelperSourceControlFileState FileState;
	FileState.Input = TEXT("/Game/BP_Editable.BP_Editable");
	FileState.Status = TEXT("editable");
	FileState.bCanEdit = true;
	SourceControl.Files.Add(FileState);

	const FBlueprintHelperEditorCloseSafetyGate Gate;
	const FBlueprintHelperEditorCloseSafetyGateResult Result = Gate.EvaluateDirtyPackageStatus(SourceControl);
	const TSharedRef<FJsonObject> Json = Result.ToJson();

	TestTrue(TEXT("close can proceed"), Result.bCanProceed);
	TestEqual(TEXT("editable code is surfaced"), Result.Code, FString(TEXT("editable")));
	TestEqual(TEXT("editable message is surfaced"), Result.Message, FString(TEXT("Dirty packages are editable for close_editor(save_all=true).")));
	TestTrue(TEXT("editable result still exposes source control gate"), Json->HasField(TEXT("source_control_gate")));
	return true;
}

#endif
