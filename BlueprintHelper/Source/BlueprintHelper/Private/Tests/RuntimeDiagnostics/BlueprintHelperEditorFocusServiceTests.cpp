#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/Debug/BlueprintHelperEditorFocusService.h"

#include "Misc/AutomationTest.h"
#include "Shared/Debug/BlueprintHelperScreenshotTypes.h"
#include "Systems/Debug/BlueprintHelperAssetBrowseService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonPathService.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

class FBlueprintHelperEditorFocusServiceTestUtils
{
public:
	static bool LoadFocusServiceSource(FAutomationTestBase& Test, FString& OutText)
	{
		const FString FullPath = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/Debug/BlueprintHelperEditorFocusService.cpp"));
		if (!FFileHelper::LoadFileToString(OutText, *FullPath))
		{
			Test.AddError(FString::Printf(TEXT("focus service source not readable: %s"), *FullPath));
			return false;
		}
		return true;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEditorFocusServiceRequiresAssetPathTest,
	"BlueprintHelper.RuntimeDiagnostics.Screenshot.FocusRequiresAssetPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEditorFocusServiceRequiresAssetPathTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperAssetBrowseService AssetBrowseService;
	FBlueprintHelperGraphResolver GraphResolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperLogicJsonPathService PathService;
	const FBlueprintHelperEditorFocusService FocusService(
		AssetBrowseService,
		GraphResolver,
		BlockIdService,
		PathService);

	FBlueprintHelperEditorFocusRequest Request;
	Request.GraphName = TEXT("EventGraph");
	Request.NodeRef = TEXT("nodes[0]");
	const FBlueprintHelperEditorFocusResult Result =
		FocusService.FocusBlueprintEditorTarget(Request);

	TestFalse(TEXT("focus fails without asset_path"), Result.bSuccess);
	TestEqual(TEXT("failure code is stable"), Result.ErrorCode, FString(TEXT("asset_path_required")));
	TSharedRef<FJsonObject> Json = Result.ToJson();
	TestEqual(TEXT("schema is stable"), Json->GetStringField(TEXT("schema")),
		FString(TEXT("BlueprintHelper.EditorFocusResult.v1")));
	TestFalse(TEXT("ok serializes"), Json->GetBoolField(TEXT("ok")));
	TestEqual(TEXT("node_ref serializes"), Json->GetStringField(TEXT("node_ref")), Request.NodeRef);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEditorFocusServiceNodeFocusViewportContractTest,
	"BlueprintHelper.RuntimeDiagnostics.Screenshot.FocusSelectsNodeAndZoomsSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEditorFocusServiceNodeFocusViewportContractTest::RunTest(const FString& Parameters)
{
	FString SourceText;
	if (!FBlueprintHelperEditorFocusServiceTestUtils::LoadFocusServiceSource(*this, SourceText))
	{
		return false;
	}

	TestTrue(
		TEXT("node focus opens the graph editor explicitly"),
		SourceText.Contains(TEXT("OpenGraphAndBringToFront")));
	TestTrue(
		TEXT("node focus clears previous graph selection"),
		SourceText.Contains(TEXT("ClearSelectionSet")));
	TestTrue(
		TEXT("node focus selects the resolved node"),
		SourceText.Contains(TEXT("SetNodeSelection(Node, true)")));
	TestTrue(
		TEXT("node focus expands to event logic before graph screenshot capture"),
		SourceText.Contains(TEXT("CollectEventLogicNodes")));
	TestTrue(
		TEXT("block focus selects all owned block nodes before graph screenshot capture"),
		SourceText.Contains(TEXT("CollectBlockNodes")));
	TestTrue(
		TEXT("node focus zooms to selected node before screenshot settle"),
		SourceText.Contains(TEXT("ZoomToFit(true)")));
	return true;
}

#endif
