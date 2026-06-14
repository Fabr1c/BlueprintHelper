#include "Entry/Bridge/BlueprintHelperBridgeRoutePlanner.h"
#include "Entry/Bridge/Routes/BlueprintHelperAssetDiscoveryBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperAssetFactoryBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperBlueprintVariablesBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperClassSettingsBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperComponentBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperDataTableBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperGraphWriteBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperObjectPropertyBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperScreenshotBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperUMGWidgetBridgeRoutes.h"
#include "Entry/Bridge/BlueprintHelperBridgeCommandRegistry.h"
#include "Entry/Bridge/BlueprintHelperRequestValidator.h"
#include "Entry/Bridge/BlueprintHelperRequestValidationRegistry.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Systems/ToolClusters/AssetDiscovery/BlueprintHelperAssetDiscoveryService.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

static bool BlueprintHelperBridgeTestHasRequiredField(
	const FBlueprintHelperRequestValidationDescriptor& Descriptor,
	const FString& FieldName)
{
	return Descriptor.RequiredFields.ContainsByPredicate(
		[&FieldName](const FBlueprintHelperFieldRule& Rule)
		{
			return Rule.FieldName == FieldName;
		});
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperBridgeRoutePlanner_KnownCommandsMapToClusters,
	"BlueprintHelper.Router.Cluster.KnownCommandsMapToClusters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperBridgeRoutePlanner_KnownCommandsMapToClusters::RunTest(const FString& Parameters)
{
	const TPair<FString, EBlueprintHelperBridgeRouteCluster> Cases[] = {
		{TEXT("append_blueprint_graph"), EBlueprintHelperBridgeRouteCluster::GraphWrite},
		{TEXT("merge_external_flow"), EBlueprintHelperBridgeRouteCluster::GraphWrite},
		{TEXT("patch_external_graph"), EBlueprintHelperBridgeRouteCluster::GraphWrite},
		{TEXT("replace_external_body"), EBlueprintHelperBridgeRouteCluster::GraphWrite},
		{TEXT("read_blueprint_member_variables"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables},
		{TEXT("find_assets"), EBlueprintHelperBridgeRouteCluster::AssetDiscovery},
		{TEXT("create_asset"), EBlueprintHelperBridgeRouteCluster::AssetFactory},
		{TEXT("read_components"), EBlueprintHelperBridgeRouteCluster::Component},
		{TEXT("read_class_settings"), EBlueprintHelperBridgeRouteCluster::ClassSettings},
		{TEXT("get_widget_tree"), EBlueprintHelperBridgeRouteCluster::UMGWidget},
		{TEXT("get_datatable_rows"), EBlueprintHelperBridgeRouteCluster::DataTable},
		{TEXT("get_object_properties"), EBlueprintHelperBridgeRouteCluster::ObjectProperty},
		{TEXT("preview_task_plan"), EBlueprintHelperBridgeRouteCluster::TaskRuntime},
		{TEXT("diagnostics_runtime"), EBlueprintHelperBridgeRouteCluster::Debug},
		{TEXT("get_debug_case"), EBlueprintHelperBridgeRouteCluster::Debug},
		{TEXT("focus_blueprint_editor_target"), EBlueprintHelperBridgeRouteCluster::Debug},
		{TEXT("compile_blueprint"), EBlueprintHelperBridgeRouteCluster::Debug},
		{TEXT("capture_editor_screenshot"), EBlueprintHelperBridgeRouteCluster::Debug},
		{TEXT("capture_focused_graph_screenshot"), EBlueprintHelperBridgeRouteCluster::Debug},
		{TEXT("read_function_chain_context"), EBlueprintHelperBridgeRouteCluster::SharedServices},
		{TEXT("query_review_records"), EBlueprintHelperBridgeRouteCluster::Review},
	};

	for (const TPair<FString, EBlueprintHelperBridgeRouteCluster>& Case : Cases)
	{
		const FBlueprintHelperBridgeRoutePlan Plan = FBlueprintHelperBridgeRoutePlanner::BuildPlan(Case.Key);
		TestTrue(FString::Printf(TEXT("%s is known"), *Case.Key), Plan.bKnownCommand);
		TestTrue(FString::Printf(TEXT("%s requires GameThread execution"), *Case.Key), Plan.bRequiresGameThread);
		TestTrue(FString::Printf(TEXT("%s maps to expected cluster"), *Case.Key), Plan.Cluster == Case.Value);
		TestEqual(FString::Printf(TEXT("%s command is preserved"), *Case.Key), Plan.Command, Case.Key);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperBridgeRoutePlanner_UnknownCommandStaysUnknown,
	"BlueprintHelper.Router.Cluster.UnknownCommandStaysUnknown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperBridgeRoutePlanner_UnknownCommandStaysUnknown::RunTest(const FString& Parameters)
{
	const FBlueprintHelperBridgeRoutePlan Plan = FBlueprintHelperBridgeRoutePlanner::BuildPlan(TEXT("definitely_not_a_blueprinthelper_command"));
	TestFalse(TEXT("Unknown command is not known"), Plan.bKnownCommand);
	TestFalse(TEXT("Unknown command does not need GameThread execution"), Plan.bRequiresGameThread);
	TestTrue(TEXT("Unknown command maps to Unknown cluster"), Plan.Cluster == EBlueprintHelperBridgeRouteCluster::Unknown);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperBridgeCommandRegistry_DescriptorsMatchRoutePlanner,
	"BlueprintHelper.Bridge.CommandRegistry.DescriptorsMatchRoutePlanner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperBridgeCommandRegistry_DescriptorsMatchRoutePlanner::RunTest(const FString& Parameters)
{
	const TArray<FBlueprintHelperBridgeCommandDescriptor> Descriptors =
		FBlueprintHelperBridgeCommandRegistry::GetRepresentativeDescriptors();
	TestTrue(TEXT("representative descriptors exist"), Descriptors.Num() > 0);

	for (const FBlueprintHelperBridgeCommandDescriptor& Descriptor : Descriptors)
	{
		const FBlueprintHelperBridgeRoutePlan Plan =
			FBlueprintHelperBridgeRoutePlanner::BuildPlan(Descriptor.Command);
		TestTrue(FString::Printf(TEXT("%s is known"), *Descriptor.Command), Plan.bKnownCommand);
		TestEqual(
			FString::Printf(TEXT("%s keeps route cluster"), *Descriptor.Command),
			static_cast<int32>(Descriptor.RouteCluster),
			static_cast<int32>(Plan.Cluster));
		TestEqual(
			FString::Printf(TEXT("%s keeps GameThread requirement"), *Descriptor.Command),
			Descriptor.bRequiresGameThread,
			Plan.bRequiresGameThread);
		if (Plan.Cluster == EBlueprintHelperBridgeRouteCluster::GraphWrite)
		{
			TestTrue(
				FString::Printf(TEXT("%s allows GraphWrite validation policy"), *Descriptor.Command),
				Descriptor.bAllowsGraphWriteValidationPolicy);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperRequestValidationRegistry_RepresentativeRulesMatchValidator,
	"BlueprintHelper.Bridge.ValidationRegistry.RepresentativeRulesMatchValidator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperRequestValidationRegistry_RepresentativeRulesMatchValidator::RunTest(const FString& Parameters)
{
	FBlueprintHelperRequestValidationDescriptor AppendDescriptor;
	TestTrue(
		TEXT("append descriptor exists"),
		FBlueprintHelperRequestValidationRegistry::TryFindDescriptor(
			TEXT("append_blueprint_graph"),
			AppendDescriptor));
	TestTrue(TEXT("append descriptor requires target"), BlueprintHelperBridgeTestHasRequiredField(AppendDescriptor, TEXT("target")));
	TestTrue(TEXT("append descriptor requires nodes"), BlueprintHelperBridgeTestHasRequiredField(AppendDescriptor, TEXT("nodes")));

	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetObjectField(TEXT("target"), MakeShared<FJsonObject>());
	FBlueprintHelperBridgeValidationError Error;
	TestFalse(
		TEXT("old validator rejects missing append nodes"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(
			TEXT("append_blueprint_graph"),
			Payload,
			Error));
	TestEqual(TEXT("old validator reports missing nodes"), Error.Field, FString(TEXT("payload.nodes")));

	FBlueprintHelperRequestValidationDescriptor LogicDescriptor;
	TestTrue(
		TEXT("logic json descriptor exists"),
		FBlueprintHelperRequestValidationRegistry::TryFindDescriptor(
			TEXT("read_blueprint_logic_json"),
			LogicDescriptor));
	TestTrue(TEXT("logic json descriptor requires asset_path"), BlueprintHelperBridgeTestHasRequiredField(LogicDescriptor, TEXT("asset_path")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperRequestValidator_CompileBlueprintRequiresExactTarget,
	"BlueprintHelper.Bridge.RequestValidator.CompileBlueprintRequiresExactTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperRequestValidator_CompileBlueprintRequiresExactTarget::RunTest(const FString& Parameters)
{
	FBlueprintHelperBridgeValidationError Error;

	TSharedPtr<FJsonObject> MissingPayload = MakeShared<FJsonObject>();
	TestFalse(
		TEXT("compile_blueprint rejects missing target_blueprint"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(
			TEXT("compile_blueprint"),
			MissingPayload,
			Error));
	TestEqual(TEXT("missing target field"), Error.Field, FString(TEXT("payload.target_blueprint")));
	TestEqual(TEXT("missing target actual type"), Error.ActualType, FString(TEXT("missing")));

	TSharedPtr<FJsonObject> EmptyPayload = MakeShared<FJsonObject>();
	EmptyPayload->SetStringField(TEXT("target_blueprint"), TEXT(""));
	TestFalse(
		TEXT("compile_blueprint rejects empty target_blueprint"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(
			TEXT("compile_blueprint"),
			EmptyPayload,
			Error));
	TestEqual(TEXT("empty target field"), Error.Field, FString(TEXT("payload.target_blueprint")));
	TestEqual(TEXT("empty target expected type"), Error.ExpectedType, FString(TEXT("non-empty string")));
	TestEqual(TEXT("empty target actual type"), Error.ActualType, FString(TEXT("empty string")));

	TSharedPtr<FJsonObject> AliasPayload = MakeShared<FJsonObject>();
	AliasPayload->SetStringField(TEXT("target_blueprint"), TEXT("/Game/BP_Test.BP_Test"));
	AliasPayload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_Test.BP_Test"));
	TestFalse(
		TEXT("compile_blueprint rejects unexpected asset_path alias"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(
			TEXT("compile_blueprint"),
			AliasPayload,
			Error));
	TestEqual(TEXT("unexpected alias code"), Error.Code, FString(TEXT("unexpected_field")));
	TestEqual(TEXT("unexpected alias field"), Error.Field, FString(TEXT("payload.asset_path")));
	TestEqual(TEXT("unexpected alias expected type"), Error.ExpectedType, FString(TEXT("absent")));
	TestEqual(TEXT("unexpected alias actual type"), Error.ActualType, FString(TEXT("present")));

	TSharedPtr<FJsonObject> ValidPayload = MakeShared<FJsonObject>();
	ValidPayload->SetStringField(TEXT("target_blueprint"), TEXT("/Game/BP_Test.BP_Test"));
	TestTrue(
		TEXT("compile_blueprint accepts explicit target_blueprint"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(
			TEXT("compile_blueprint"),
			ValidPayload,
			Error));
	TestTrue(TEXT("compile_blueprint remains write-classified"), FBlueprintHelperRequestValidator::IsWriteCommand(TEXT("compile_blueprint")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperBridgeRoutePlanner_CanBuildOnWorkerThread,
	"BlueprintHelper.Router.Cluster.RoutePlannerIsWorkerThreadSafe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperBridgeRoutePlanner_CanBuildOnWorkerThread::RunTest(const FString& Parameters)
{
	TFuture<FBlueprintHelperBridgeRoutePlan> Future = Async(EAsyncExecution::ThreadPool, []()
	{
		return FBlueprintHelperBridgeRoutePlanner::BuildPlan(TEXT("append_blueprint_graph"));
	});

	const FBlueprintHelperBridgeRoutePlan Plan = Future.Get();
	TestTrue(TEXT("Worker thread plan is known"), Plan.bKnownCommand);
	TestTrue(TEXT("Worker thread plan maps GraphWrite"), Plan.Cluster == EBlueprintHelperBridgeRouteCluster::GraphWrite);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteBridgeRoutes_RecognizesOnlyGraphWriteCommands,
	"BlueprintHelper.Router.Cluster.GraphWriteRoutesRecognizeOnlyGraphWriteCommands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperGraphWriteBridgeRoutes_RecognizesOnlyGraphWriteCommands::RunTest(const FString& Parameters)
{
	const FString GraphWriteCommands[] = {
		TEXT("append_blueprint_graph"),
		TEXT("replace_blueprint_graph"),
		TEXT("patch_blueprint_graph"),
		TEXT("merge_blueprint_graph"),
		TEXT("merge_external_flow"),
		TEXT("patch_external_graph"),
		TEXT("replace_external_body"),
	};

	for (const FString& Command : GraphWriteCommands)
	{
		TestTrue(
			FString::Printf(TEXT("%s is a GraphWrite route command"), *Command),
			FBlueprintHelperGraphWriteBridgeRoutes::IsGraphWriteCommand(Command));
	}

	TestFalse(
		TEXT("unknown route is not a GraphWrite route command"),
		FBlueprintHelperGraphWriteBridgeRoutes::IsGraphWriteCommand(TEXT("unknown_command")));
	TestFalse(
		TEXT("unknown route is not a GraphWrite route command"),
		FBlueprintHelperGraphWriteBridgeRoutes::IsGraphWriteCommand(TEXT("unknown_command")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSecondBatchBridgeRoutes_RecognizeOnlyOwnedCommands,
	"BlueprintHelper.Router.Cluster.SecondBatchRoutesRecognizeOnlyOwnedCommands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperSecondBatchBridgeRoutes_RecognizeOnlyOwnedCommands::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("BlueprintVariables route recognizes member variables read"),
		FBlueprintHelperBlueprintVariablesBridgeRoutes::IsBlueprintVariablesCommand(TEXT("read_blueprint_member_variables")));
	TestTrue(
		TEXT("BlueprintVariables route recognizes local variable write"),
		FBlueprintHelperBlueprintVariablesBridgeRoutes::IsBlueprintVariablesCommand(TEXT("add_blueprint_local_variable")));
	TestFalse(
		TEXT("BlueprintVariables route rejects component command"),
		FBlueprintHelperBlueprintVariablesBridgeRoutes::IsBlueprintVariablesCommand(TEXT("add_component")));

	TestTrue(
		TEXT("AssetFactory route recognizes create asset"),
		FBlueprintHelperAssetFactoryBridgeRoutes::IsAssetFactoryCommand(TEXT("create_asset")));
	TestFalse(
		TEXT("AssetFactory route rejects graph command"),
		FBlueprintHelperAssetFactoryBridgeRoutes::IsAssetFactoryCommand(TEXT("append_blueprint_graph")));

	TestTrue(
		TEXT("Component route recognizes component add"),
		FBlueprintHelperComponentBridgeRoutes::IsComponentCommand(TEXT("add_component")));
	TestTrue(
		TEXT("Component route recognizes component property batch"),
		FBlueprintHelperComponentBridgeRoutes::IsComponentCommand(TEXT("set_component_properties")));
	TestFalse(
		TEXT("Component route rejects class settings command"),
		FBlueprintHelperComponentBridgeRoutes::IsComponentCommand(TEXT("read_class_settings")));

	TestTrue(
		TEXT("ClassSettings route recognizes interface add"),
		FBlueprintHelperClassSettingsBridgeRoutes::IsClassSettingsCommand(TEXT("add_implemented_interface")));
	TestTrue(
		TEXT("ClassSettings route recognizes default property batch"),
		FBlueprintHelperClassSettingsBridgeRoutes::IsClassSettingsCommand(TEXT("set_class_default_properties")));
	TestTrue(
		TEXT("ClassSettings route recognizes reparent"),
		FBlueprintHelperClassSettingsBridgeRoutes::IsClassSettingsCommand(TEXT("reparent_blueprint")));
	TestFalse(
		TEXT("ClassSettings route rejects graph command"),
		FBlueprintHelperClassSettingsBridgeRoutes::IsClassSettingsCommand(TEXT("append_blueprint_graph")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperScreenshotBridgeRoutes_RecognizeOnlyOwnedCommands,
	"BlueprintHelper.Router.Cluster.ScreenshotRoutesRecognizeOnlyOwnedCommands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperScreenshotBridgeRoutes_RecognizeOnlyOwnedCommands::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("Screenshot route recognizes editor focus"),
		FBlueprintHelperScreenshotBridgeRoutes::IsScreenshotCommand(TEXT("focus_blueprint_editor_target")));
	TestTrue(
		TEXT("Screenshot route recognizes editor screenshot capture"),
		FBlueprintHelperScreenshotBridgeRoutes::IsScreenshotCommand(TEXT("capture_editor_screenshot")));
	TestTrue(
		TEXT("Screenshot route recognizes focused graph screenshot capture"),
		FBlueprintHelperScreenshotBridgeRoutes::IsScreenshotCommand(TEXT("capture_focused_graph_screenshot")));
	TestFalse(
		TEXT("Screenshot route rejects asset open"),
		FBlueprintHelperScreenshotBridgeRoutes::IsScreenshotCommand(TEXT("open_asset")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperScreenshotBridgeValidator_ValidatesSemanticInputs,
	"BlueprintHelper.Router.Cluster.ScreenshotValidatorValidatesSemanticInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperScreenshotBridgeValidator_ValidatesSemanticInputs::RunTest(const FString& Parameters)
{
	FBlueprintHelperBridgeValidationError Error;

	TSharedPtr<FJsonObject> FocusPayload = MakeShared<FJsonObject>();
	FocusPayload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_Test.BP_Test"));
	FocusPayload->SetStringField(TEXT("block_ref"), TEXT("BeginPlaySetup0"));
	TestFalse(
		TEXT("focus target requires graph_name with block_ref"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(
			TEXT("focus_blueprint_editor_target"),
			FocusPayload,
			Error));
	TestEqual(TEXT("focus validation field"), Error.Field, FString(TEXT("payload.graph_name")));

	TSharedPtr<FJsonObject> CapturePayload = MakeShared<FJsonObject>();
	CapturePayload->SetStringField(TEXT("target"), TEXT("active_window"));
	CapturePayload->SetStringField(TEXT("label"), TEXT("bp_test"));
	TestTrue(
		TEXT("capture screenshot accepts safe target and label"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(
			TEXT("capture_editor_screenshot"),
			CapturePayload,
			Error));

	CapturePayload->SetStringField(TEXT("label"), TEXT("../escape"));
	TestFalse(
		TEXT("capture screenshot rejects unsafe label"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(
			TEXT("capture_editor_screenshot"),
			CapturePayload,
			Error));
	TestEqual(TEXT("capture label validation field"), Error.Field, FString(TEXT("payload.label")));

	TSharedPtr<FJsonObject> GraphCapturePayload = MakeShared<FJsonObject>();
	GraphCapturePayload->SetStringField(TEXT("label"), TEXT("bp_test_graph"));
	GraphCapturePayload->SetNumberField(TEXT("max_nodes_per_image"), 2);
	TestTrue(
		TEXT("graph screenshot capture accepts tile cap and safe label"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(
			TEXT("capture_focused_graph_screenshot"),
			GraphCapturePayload,
			Error));

	GraphCapturePayload->SetNumberField(TEXT("max_nodes_per_image"), 0);
	TestFalse(
		TEXT("graph screenshot capture rejects invalid tile cap"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(
			TEXT("capture_focused_graph_screenshot"),
			GraphCapturePayload,
			Error));
	TestEqual(TEXT("graph capture max nodes validation field"), Error.Field, FString(TEXT("payload.max_nodes_per_image")));
	TestFalse(
		TEXT("capture screenshot is read/debug classified"),
		FBlueprintHelperRequestValidator::IsWriteCommand(TEXT("capture_editor_screenshot")));
	TestFalse(
		TEXT("graph capture screenshot is read/debug classified"),
		FBlueprintHelperRequestValidator::IsWriteCommand(TEXT("capture_focused_graph_screenshot")));
	TestFalse(
		TEXT("focus screenshot is read/debug classified"),
		FBlueprintHelperRequestValidator::IsWriteCommand(TEXT("focus_blueprint_editor_target")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentBridgeValidatorRejectsInvalidPolicyEnums,
	"BlueprintHelper.Router.Cluster.ComponentValidatorRejectsInvalidPolicyEnums",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperComponentBridgeValidatorRejectsInvalidPolicyEnums::RunTest(const FString& Parameters)
{
	FBlueprintHelperBridgeValidationError Error;

	TSharedPtr<FJsonObject> ReparentPayload = MakeShared<FJsonObject>();
	ReparentPayload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_Test.BP_Test"));
	ReparentPayload->SetStringField(TEXT("component_name"), TEXT("Door"));
	ReparentPayload->SetStringField(TEXT("new_parent_component"), TEXT("Root"));
	ReparentPayload->SetStringField(TEXT("transform_policy"), TEXT("teleport_somewhere"));
	TestFalse(
		TEXT("reparent rejects invalid transform_policy"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("reparent_component"), ReparentPayload, Error));
	TestEqual(TEXT("reparent transform policy field"), Error.Field, FString(TEXT("payload.transform_policy")));

	TSharedPtr<FJsonObject> AttachPayload = MakeShared<FJsonObject>();
	AttachPayload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_Test.BP_Test"));
	AttachPayload->SetStringField(TEXT("component_name"), TEXT("Door"));
	AttachPayload->SetStringField(TEXT("parent_component"), TEXT("Root"));
	AttachPayload->SetStringField(TEXT("attach_rule"), TEXT("teleport"));
	TestFalse(
		TEXT("attach rejects invalid attach_rule"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("attach_component"), AttachPayload, Error));
	TestEqual(TEXT("attach rule field"), Error.Field, FString(TEXT("payload.attach_rule")));

	TSharedPtr<FJsonObject> DetachPayload = MakeShared<FJsonObject>();
	DetachPayload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_Test.BP_Test"));
	DetachPayload->SetStringField(TEXT("component_name"), TEXT("Door"));
	DetachPayload->SetStringField(TEXT("default_root_policy"), TEXT("invent_root"));
	TestFalse(
		TEXT("detach rejects invalid default_root_policy"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("detach_component"), DetachPayload, Error));
	TestEqual(TEXT("detach default root policy field"), Error.Field, FString(TEXT("payload.default_root_policy")));

	TSharedPtr<FJsonObject> SetRootPayload = MakeShared<FJsonObject>();
	SetRootPayload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_Test.BP_Test"));
	SetRootPayload->SetStringField(TEXT("component_name"), TEXT("Door"));
	SetRootPayload->SetStringField(TEXT("old_root_policy"), TEXT("delete_everything"));
	TestFalse(
		TEXT("set root rejects invalid old_root_policy"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("set_root_component"), SetRootPayload, Error));
	TestEqual(TEXT("set root old root policy field"), Error.Field, FString(TEXT("payload.old_root_policy")));

	TSharedPtr<FJsonObject> RemovePayload = MakeShared<FJsonObject>();
	RemovePayload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_Test.BP_Test"));
	RemovePayload->SetStringField(TEXT("component_name"), TEXT("Door"));
	RemovePayload->SetStringField(TEXT("delete_policy"), TEXT("silently_default"));
	TestFalse(
		TEXT("remove rejects invalid delete_policy"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("remove_component"), RemovePayload, Error));
	TestEqual(TEXT("remove delete policy field"), Error.Field, FString(TEXT("payload.delete_policy")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperUMGWidgetBridgeValidatorConsumesDescriptorEnumRules,
	"BlueprintHelper.Router.Cluster.UMGWidgetValidatorConsumesDescriptorEnumRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperUMGWidgetBridgeValidatorConsumesDescriptorEnumRules::RunTest(const FString& Parameters)
{
	FBlueprintHelperBridgeValidationError Error;

	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/WBP_Test.WBP_Test"));
	Payload->SetStringField(TEXT("root_widget_name"), TEXT("CanvasRoot"));
	Payload->SetStringField(TEXT("replacement_policy"), TEXT("promote_single_child"));
	TestTrue(
		TEXT("remove_root_widget accepts descriptor-owned replacement policy"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("remove_root_widget"), Payload, Error));

	Payload->SetStringField(TEXT("replacement_policy"), TEXT("invalid_policy_for_descriptor_e2e"));
	TestFalse(
		TEXT("remove_root_widget rejects descriptor-owned replacement policy enum"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("remove_root_widget"), Payload, Error));
	TestEqual(TEXT("replacement policy validation field"), Error.Field, FString(TEXT("payload.replacement_policy")));
	TestTrue(
		TEXT("replacement policy expected values come from descriptor enum rules"),
		Error.ExpectedType.Contains(TEXT("one_of[")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperClassSettingsBridgeRoutes_ReparentForwardsPayload,
	"BlueprintHelper.Router.Cluster.ClassSettingsReparentForwardsPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperClassSettingsBridgeRoutes_ReparentForwardsPayload::RunTest(const FString& Parameters)
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperBridge/%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*FString::Printf(TEXT("BP_BridgeReparent_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperBridgeRouteTests"));
	TestNotNull(TEXT("target Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}
	Package->SetDirtyFlag(false);

	FBlueprintHelperGraphResolver Resolver;
	const FBlueprintHelperClassSettingsService Service(Resolver);
	const FBlueprintHelperClassSettingsBridgeRoutes Routes(Service);

	FBlueprintHelperBridgeRequest Request;
	Request.RequestId = TEXT("bridge_reparent_request");
	Request.Command = TEXT("reparent_blueprint");
	Request.Payload = MakeShared<FJsonObject>();
	Request.Payload->SetStringField(TEXT("asset_path"), Blueprint->GetPathName());
	Request.Payload->SetStringField(TEXT("new_parent_class"), APawn::StaticClass()->GetPathName());

	const FBlueprintHelperBridgeResponse Response = Routes.HandleRequest(Request);

	TestTrue(TEXT("bridge response succeeds"), Response.bSuccess);
	TestTrue(TEXT("dry-run is not requested by direct bridge route"), Blueprint->ParentClass.Get() == APawn::StaticClass());
	TestNotNull(TEXT("bridge response carries result"), Response.Result.Get());
	if (!Response.Result.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* DataObject = nullptr;
	TestTrue(TEXT("result carries data"), Response.Result->TryGetObjectField(TEXT("data"), DataObject));
	if (!DataObject || !DataObject->IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* ReparentResult = nullptr;
	TestTrue(TEXT("data carries reparent_result"), (*DataObject)->TryGetObjectField(TEXT("reparent_result"), ReparentResult));
	if (!ReparentResult || !ReparentResult->IsValid())
	{
		return false;
	}

	FString NewParentClass;
	TestTrue(TEXT("reparent_result carries new parent"), (*ReparentResult)->TryGetStringField(TEXT("new_parent_class"), NewParentClass));
	TestEqual(TEXT("bridge forwarded new_parent_class"), NewParentClass, FString(APawn::StaticClass()->GetPathName()));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFinalBatchBridgeRoutes_RecognizeOnlyOwnedCommands,
	"BlueprintHelper.Router.Cluster.FinalBatchRoutesRecognizeOnlyOwnedCommands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperFinalBatchBridgeRoutes_RecognizeOnlyOwnedCommands::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("UMGWidget route recognizes widget tree read"),
		FBlueprintHelperUMGWidgetBridgeRoutes::IsUMGWidgetCommand(TEXT("get_widget_tree")));
	TestTrue(
		TEXT("UMGWidget route recognizes widget property write"),
		FBlueprintHelperUMGWidgetBridgeRoutes::IsUMGWidgetCommand(TEXT("set_widget_property")));
	TestFalse(
		TEXT("UMGWidget route rejects datatable command"),
		FBlueprintHelperUMGWidgetBridgeRoutes::IsUMGWidgetCommand(TEXT("get_datatable_rows")));

	TestTrue(
		TEXT("DataTable route recognizes row update"),
		FBlueprintHelperDataTableBridgeRoutes::IsDataTableCommand(TEXT("update_datatable_row")));
	TestFalse(
		TEXT("DataTable route rejects object property command"),
		FBlueprintHelperDataTableBridgeRoutes::IsDataTableCommand(TEXT("set_object_property")));

	TestTrue(
		TEXT("ObjectProperty route recognizes object property write"),
		FBlueprintHelperObjectPropertyBridgeRoutes::IsObjectPropertyCommand(TEXT("set_object_property")));
	TestFalse(
		TEXT("ObjectProperty route rejects graph command"),
		FBlueprintHelperObjectPropertyBridgeRoutes::IsObjectPropertyCommand(TEXT("append_blueprint_graph")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetDiscoveryBridgeRoute_AcceptsFindAssetsPayload,
	"BlueprintHelper.AssetDiscovery.Route.AcceptsFindAssetsPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperAssetDiscoveryBridgeRoute_AcceptsFindAssetsPayload::RunTest(const FString& Parameters)
{
	const FBlueprintHelperAssetDiscoveryService Service;
	const FBlueprintHelperAssetDiscoveryBridgeRoutes Routes(Service);

	FBlueprintHelperBridgeRequest Request;
	Request.RequestId = TEXT("asset_discovery_accepts_payload");
	Request.Command = TEXT("find_assets");
	Request.Payload = MakeShared<FJsonObject>();
	Request.Payload->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.FindAssetsRequest.v1"));
	Request.Payload->SetStringField(TEXT("query"), TEXT("GraphLocalValueProducer"));
	Request.Payload->SetArrayField(TEXT("path_prefixes"), {MakeShared<FJsonValueString>(TEXT("/Game"))});
	Request.Payload->SetArrayField(TEXT("asset_types"), {MakeShared<FJsonValueString>(TEXT("blueprint"))});
	Request.Payload->SetArrayField(TEXT("asset_classes"), {MakeShared<FJsonValueString>(TEXT("/Script/Engine.Blueprint"))});
	Request.Payload->SetBoolField(TEXT("recursive"), true);
	Request.Payload->SetNumberField(TEXT("limit"), 1);
	Request.Payload->SetBoolField(TEXT("include_plugin_content"), false);
	Request.Payload->SetBoolField(TEXT("include_engine_content"), false);
	Request.Payload->SetBoolField(TEXT("include_redirectors"), false);

	const FBlueprintHelperBridgeResponse Response = Routes.HandleRequest(Request);

	TestTrue(TEXT("find_assets route accepts valid payload"), Response.bSuccess);
	TestNotNull(TEXT("find_assets route returns result json"), Response.Result.Get());
	return Response.bSuccess && Response.Result.IsValid();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetDiscoveryBridgeRoute_RejectsCursorInP0,
	"BlueprintHelper.AssetDiscovery.Route.RejectsCursorInP0",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperAssetDiscoveryBridgeRoute_RejectsCursorInP0::RunTest(const FString& Parameters)
{
	const FBlueprintHelperAssetDiscoveryService Service;
	const FBlueprintHelperAssetDiscoveryBridgeRoutes Routes(Service);

	FBlueprintHelperBridgeRequest Request;
	Request.RequestId = TEXT("asset_discovery_rejects_cursor");
	Request.Command = TEXT("find_assets");
	Request.Payload = MakeShared<FJsonObject>();
	Request.Payload->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.FindAssetsRequest.v1"));
	Request.Payload->SetStringField(TEXT("cursor"), TEXT("page-2"));

	const FBlueprintHelperBridgeResponse Response = Routes.HandleRequest(Request);

	TestFalse(TEXT("find_assets cursor is rejected in P0"), Response.bSuccess);
	TestEqual(TEXT("cursor rejection uses invalid request"), Response.ErrorCode, EBlueprintHelperBridgeError::InvalidRequest);
	TestTrue(
		TEXT("cursor rejection reports cursor_not_supported_in_p0"),
		Response.Message.Contains(TEXT("cursor_not_supported_in_p0")));
	const TSharedPtr<FJsonObject>* ErrorJson = nullptr;
	TestTrue(TEXT("cursor rejection result carries error"), Response.Result->TryGetObjectField(TEXT("error"), ErrorJson));
	if (ErrorJson && ErrorJson->IsValid())
	{
		FString Field;
		TestTrue(TEXT("cursor rejection result carries field"), (*ErrorJson)->TryGetStringField(TEXT("field"), Field));
		TestEqual(TEXT("cursor rejection field is stable"), Field, FString(TEXT("payload.cursor")));
	}
	return !Response.bSuccess &&
		Response.ErrorCode == EBlueprintHelperBridgeError::InvalidRequest &&
		Response.Message.Contains(TEXT("cursor_not_supported_in_p0"));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetDiscoveryBridgeRoute_RejectsInvalidSchema,
	"BlueprintHelper.AssetDiscovery.Route.RejectsInvalidSchema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperAssetDiscoveryBridgeRoute_RejectsInvalidSchema::RunTest(const FString& Parameters)
{
	const FBlueprintHelperAssetDiscoveryService Service;
	const FBlueprintHelperAssetDiscoveryBridgeRoutes Routes(Service);

	FBlueprintHelperBridgeRequest Request;
	Request.RequestId = TEXT("asset_discovery_rejects_invalid_schema");
	Request.Command = TEXT("find_assets");
	Request.Payload = MakeShared<FJsonObject>();
	Request.Payload->SetStringField(TEXT("schema"), TEXT("FindAssets.v1"));

	const FBlueprintHelperBridgeResponse Response = Routes.HandleRequest(Request);

	TestFalse(TEXT("find_assets rejects invalid request schema"), Response.bSuccess);
	TestEqual(TEXT("invalid schema uses invalid request"), Response.ErrorCode, EBlueprintHelperBridgeError::InvalidRequest);
	TestTrue(
		TEXT("invalid schema message names the required literal"),
		Response.Message.Contains(TEXT("BlueprintHelper.FindAssetsRequest.v1")));
	const TSharedPtr<FJsonObject>* ErrorJson = nullptr;
	TestTrue(TEXT("invalid schema result carries error"), Response.Result->TryGetObjectField(TEXT("error"), ErrorJson));
	if (ErrorJson && ErrorJson->IsValid())
	{
		FString Field;
		TestTrue(TEXT("invalid schema result carries field"), (*ErrorJson)->TryGetStringField(TEXT("field"), Field));
		TestEqual(TEXT("invalid schema field is stable"), Field, FString(TEXT("payload.schema")));
	}
	return !Response.bSuccess &&
		Response.ErrorCode == EBlueprintHelperBridgeError::InvalidRequest &&
		Response.Message.Contains(TEXT("BlueprintHelper.FindAssetsRequest.v1"));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetDiscoveryBridgeRoute_RejectsNonArrayStringListField,
	"BlueprintHelper.AssetDiscovery.Route.RejectsNonArrayStringListField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperAssetDiscoveryBridgeRoute_RejectsNonArrayStringListField::RunTest(const FString& Parameters)
{
	const FBlueprintHelperAssetDiscoveryService Service;
	const FBlueprintHelperAssetDiscoveryBridgeRoutes Routes(Service);

	FBlueprintHelperBridgeRequest Request;
	Request.RequestId = TEXT("asset_discovery_rejects_non_array_string_list_field");
	Request.Command = TEXT("find_assets");
	Request.Payload = MakeShared<FJsonObject>();
	Request.Payload->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.FindAssetsRequest.v1"));
	Request.Payload->SetNumberField(TEXT("path_prefixes"), 42.0);

	const FBlueprintHelperBridgeResponse Response = Routes.HandleRequest(Request);

	TestFalse(TEXT("find_assets rejects non-array path_prefixes"), Response.bSuccess);
	TestEqual(TEXT("non-array string list uses invalid request"), Response.ErrorCode, EBlueprintHelperBridgeError::InvalidRequest);
	TestTrue(
		TEXT("non-array string list identifies the field"),
		Response.Message.Contains(TEXT("path_prefixes")));
	const TSharedPtr<FJsonObject>* ErrorJson = nullptr;
	TestTrue(TEXT("non-array string list result carries error"), Response.Result->TryGetObjectField(TEXT("error"), ErrorJson));
	if (ErrorJson && ErrorJson->IsValid())
	{
		FString Field;
		TestTrue(TEXT("non-array string list result carries field"), (*ErrorJson)->TryGetStringField(TEXT("field"), Field));
		TestEqual(TEXT("non-array string list field is stable"), Field, FString(TEXT("payload.path_prefixes")));
	}
	return !Response.bSuccess &&
		Response.ErrorCode == EBlueprintHelperBridgeError::InvalidRequest &&
		Response.Message.Contains(TEXT("path_prefixes"));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetDiscoveryBridgeRoute_RejectsNonStringArrayItem,
	"BlueprintHelper.AssetDiscovery.Route.RejectsNonStringArrayItem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperAssetDiscoveryBridgeRoute_RejectsNonStringArrayItem::RunTest(const FString& Parameters)
{
	const FBlueprintHelperAssetDiscoveryService Service;
	const FBlueprintHelperAssetDiscoveryBridgeRoutes Routes(Service);

	FBlueprintHelperBridgeRequest Request;
	Request.RequestId = TEXT("asset_discovery_rejects_non_string_array_item");
	Request.Command = TEXT("find_assets");
	Request.Payload = MakeShared<FJsonObject>();
	Request.Payload->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.FindAssetsRequest.v1"));
	Request.Payload->SetArrayField(TEXT("path_prefixes"), {MakeShared<FJsonValueNumber>(42.0)});

	const FBlueprintHelperBridgeResponse Response = Routes.HandleRequest(Request);

	TestFalse(TEXT("find_assets rejects non-string path_prefixes entries"), Response.bSuccess);
	TestEqual(TEXT("non-string array item uses invalid request"), Response.ErrorCode, EBlueprintHelperBridgeError::InvalidRequest);
	TestTrue(
		TEXT("non-string array item identifies the field"),
		Response.Message.Contains(TEXT("path_prefixes[0]")));
	const TSharedPtr<FJsonObject>* ErrorJson = nullptr;
	TestTrue(TEXT("non-string array item result carries error"), Response.Result->TryGetObjectField(TEXT("error"), ErrorJson));
	if (ErrorJson && ErrorJson->IsValid())
	{
		FString Field;
		TestTrue(TEXT("non-string array item result carries field"), (*ErrorJson)->TryGetStringField(TEXT("field"), Field));
		TestEqual(TEXT("non-string array item field is stable"), Field, FString(TEXT("payload.path_prefixes[0]")));
	}
	return !Response.bSuccess &&
		Response.ErrorCode == EBlueprintHelperBridgeError::InvalidRequest &&
		Response.Message.Contains(TEXT("path_prefixes[0]"));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetDiscoveryBridgeRoute_RejectsInvalidSemanticTypeCasing,
	"BlueprintHelper.AssetDiscovery.Route.RejectsInvalidSemanticTypeCasing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperAssetDiscoveryBridgeRoute_RejectsInvalidSemanticTypeCasing::RunTest(const FString& Parameters)
{
	const FBlueprintHelperAssetDiscoveryService Service;
	const FBlueprintHelperAssetDiscoveryBridgeRoutes Routes(Service);

	FBlueprintHelperBridgeRequest Request;
	Request.RequestId = TEXT("asset_discovery_rejects_invalid_semantic_type_casing");
	Request.Command = TEXT("find_assets");
	Request.Payload = MakeShared<FJsonObject>();
	Request.Payload->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.FindAssetsRequest.v1"));
	Request.Payload->SetArrayField(TEXT("asset_types"), {MakeShared<FJsonValueString>(TEXT("Blueprint"))});

	const FBlueprintHelperBridgeResponse Response = Routes.HandleRequest(Request);

	TestFalse(TEXT("find_assets route rejects semantic type casing outside task-core enum"), Response.bSuccess);
	TestEqual(TEXT("semantic type casing uses invalid request"), Response.ErrorCode, EBlueprintHelperBridgeError::InvalidRequest);
	const TSharedPtr<FJsonObject>* ErrorJson = nullptr;
	TestTrue(TEXT("semantic type casing result carries error"), Response.Result->TryGetObjectField(TEXT("error"), ErrorJson));
	if (ErrorJson && ErrorJson->IsValid())
	{
		FString Field;
		TestTrue(TEXT("semantic type casing result carries field"), (*ErrorJson)->TryGetStringField(TEXT("field"), Field));
		TestEqual(TEXT("semantic type casing field is stable"), Field, FString(TEXT("payload.asset_types[0]")));
	}
	return !Response.bSuccess &&
		Response.ErrorCode == EBlueprintHelperBridgeError::InvalidRequest;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetDiscoveryValidator_RejectsOutOfRangeLimit,
	"BlueprintHelper.AssetDiscovery.Route.ValidatorRejectsOutOfRangeLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperAssetDiscoveryValidator_RejectsOutOfRangeLimit::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.FindAssetsRequest.v1"));
	Payload->SetNumberField(TEXT("limit"), 101);

	FBlueprintHelperBridgeValidationError Error;
	TestFalse(
		TEXT("validator rejects out-of-range limit"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("find_assets"), Payload, Error));
	TestEqual(TEXT("limit error field is stable"), Error.Field, FString(TEXT("payload.limit")));
	return Error.Field == TEXT("payload.limit");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetDiscoveryValidator_RejectsInvalidPathPrefix,
	"BlueprintHelper.AssetDiscovery.Route.ValidatorRejectsInvalidPathPrefix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperAssetDiscoveryValidator_RejectsInvalidPathPrefix::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.FindAssetsRequest.v1"));
	Payload->SetArrayField(TEXT("path_prefixes"), {MakeShared<FJsonValueString>(TEXT("Game"))});

	FBlueprintHelperBridgeValidationError Error;
	TestFalse(
		TEXT("validator rejects path prefixes without slash"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("find_assets"), Payload, Error));
	TestEqual(TEXT("path prefix error field is stable"), Error.Field, FString(TEXT("payload.path_prefixes[0]")));
	return Error.Field == TEXT("payload.path_prefixes[0]");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetDiscoveryValidator_RejectsInvalidSemanticTypeCasing,
	"BlueprintHelper.AssetDiscovery.Route.ValidatorRejectsInvalidSemanticTypeCasing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperAssetDiscoveryValidator_RejectsInvalidSemanticTypeCasing::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.FindAssetsRequest.v1"));
	Payload->SetArrayField(TEXT("asset_types"), {MakeShared<FJsonValueString>(TEXT("Blueprint"))});

	FBlueprintHelperBridgeValidationError Error;
	TestFalse(
		TEXT("validator rejects semantic type casing outside task-core enum"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("find_assets"), Payload, Error));
	TestEqual(TEXT("semantic type error field is stable"), Error.Field, FString(TEXT("payload.asset_types[0]")));
	return Error.Field == TEXT("payload.asset_types[0]");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetDiscoveryValidator_RejectsMalformedClassPath,
	"BlueprintHelper.AssetDiscovery.Route.ValidatorRejectsMalformedClassPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperAssetDiscoveryValidator_RejectsMalformedClassPath::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.FindAssetsRequest.v1"));
	Payload->SetArrayField(TEXT("asset_classes"), {MakeShared<FJsonValueString>(TEXT("/Script/Engine."))});

	FBlueprintHelperBridgeValidationError Error;
	TestFalse(
		TEXT("validator rejects malformed full class path"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("find_assets"), Payload, Error));
	TestEqual(TEXT("class path error field is stable"), Error.Field, FString(TEXT("payload.asset_classes[0]")));

	Payload->SetArrayField(TEXT("asset_classes"), {MakeShared<FJsonValueString>(TEXT("/Script/Foo.Bar.Baz"))});
	FBlueprintHelperBridgeValidationError MultiDotError;
	TestFalse(
		TEXT("validator rejects class paths with more than module and class segments"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("find_assets"), Payload, MultiDotError));
	TestEqual(TEXT("multi-dot class path error field is stable"), MultiDotError.Field, FString(TEXT("payload.asset_classes[0]")));
	Payload->SetArrayField(TEXT("asset_classes"), {MakeShared<FJsonValueString>(TEXT("/script/Engine.Blueprint"))});
	FBlueprintHelperBridgeValidationError PrefixCaseError;
	TestFalse(
		TEXT("validator rejects class paths whose /Script prefix casing differs from task-core regex"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("find_assets"), Payload, PrefixCaseError));
	TestEqual(TEXT("prefix case class path error field is stable"), PrefixCaseError.Field, FString(TEXT("payload.asset_classes[0]")));
	return Error.Field == TEXT("payload.asset_classes[0]") &&
		MultiDotError.Field == TEXT("payload.asset_classes[0]") &&
		PrefixCaseError.Field == TEXT("payload.asset_classes[0]");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetDiscoveryBridgeRoute_SerializesFindAssetsResult,
	"BlueprintHelper.AssetDiscovery.Route.SerializesFindAssetsResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperAssetDiscoveryBridgeRoute_SerializesFindAssetsResult::RunTest(const FString& Parameters)
{
	const FBlueprintHelperAssetDiscoveryService Service;
	const FBlueprintHelperAssetDiscoveryBridgeRoutes Routes(Service);

	FBlueprintHelperBridgeRequest Request;
	Request.RequestId = TEXT("asset_discovery_serializes_result");
	Request.Command = TEXT("find_assets");
	Request.Payload = MakeShared<FJsonObject>();
	Request.Payload->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.FindAssetsRequest.v1"));
	Request.Payload->SetArrayField(TEXT("path_prefixes"), {MakeShared<FJsonValueString>(TEXT("/Game"))});
	Request.Payload->SetNumberField(TEXT("limit"), 1);

	const FBlueprintHelperBridgeResponse Response = Routes.HandleRequest(Request);

	TestTrue(TEXT("find_assets route succeeds"), Response.bSuccess);
	TestNotNull(TEXT("find_assets route carries compact result"), Response.Result.Get());
	if (!Response.bSuccess || !Response.Result.IsValid())
	{
		return false;
	}

	FString Schema;
	TestTrue(TEXT("result carries schema"), Response.Result->TryGetStringField(TEXT("schema"), Schema));
	TestEqual(TEXT("result schema is FindAssets.v1"), Schema, FString(TEXT("FindAssets.v1")));
	TestTrue(TEXT("result carries assets array"), Response.Result->HasTypedField<EJson::Array>(TEXT("assets")));
	TestTrue(TEXT("result carries page object"), Response.Result->HasTypedField<EJson::Object>(TEXT("page")));
	TestFalse(TEXT("result is not wrapped in tool result status"), Response.Result->HasField(TEXT("status")));
	TestFalse(TEXT("result is not wrapped in data"), Response.Result->HasField(TEXT("data")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperRetiredEmptyClustersAreUnknownTest,
	"BlueprintHelper.Bridge.RoutePlanner.RetiredEmptyClustersAreUnknown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperRetiredEmptyClustersAreUnknownTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperBridgeRoutePlan AnimationPlan =
		FBlueprintHelperBridgeRoutePlanner::BuildPlan(TEXT("animation_blueprint"));
	TestFalse(TEXT("animation_blueprint route is not known"), AnimationPlan.bKnownCommand);
	TestEqual(TEXT("animation_blueprint cluster is Unknown"),
		AnimationPlan.Cluster,
		EBlueprintHelperBridgeRouteCluster::Unknown);

	const FBlueprintHelperBridgeRoutePlan MaterialPlan =
		FBlueprintHelperBridgeRoutePlanner::BuildPlan(TEXT("material"));
	TestFalse(TEXT("material route is not known"), MaterialPlan.bKnownCommand);
	TestEqual(TEXT("material cluster is Unknown"),
		MaterialPlan.Cluster,
		EBlueprintHelperBridgeRouteCluster::Unknown);

	return true;
}

#endif
