#include "Entry/Bridge/BlueprintHelperBridgeRoutePlanner.h"
#include "Entry/Bridge/Routes/BlueprintHelperAssetFactoryBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperBlueprintVariablesBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperClassSettingsBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperComponentBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperDataTableBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperGraphWriteBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperObjectPropertyBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperUMGWidgetBridgeRoutes.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperBridgeRoutePlanner_KnownCommandsMapToClusters,
	"BlueprintHelper.Router.Cluster.KnownCommandsMapToClusters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperBridgeRoutePlanner_KnownCommandsMapToClusters::RunTest(const FString& Parameters)
{
	const TPair<FString, EBlueprintHelperBridgeRouteCluster> Cases[] = {
		{TEXT("append_blueprint_graph"), EBlueprintHelperBridgeRouteCluster::GraphWrite},
		{TEXT("read_blueprint_member_variables"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables},
		{TEXT("create_asset"), EBlueprintHelperBridgeRouteCluster::AssetFactory},
		{TEXT("read_components"), EBlueprintHelperBridgeRouteCluster::Component},
		{TEXT("read_class_settings"), EBlueprintHelperBridgeRouteCluster::ClassSettings},
		{TEXT("get_widget_tree"), EBlueprintHelperBridgeRouteCluster::UMGWidget},
		{TEXT("get_datatable_rows"), EBlueprintHelperBridgeRouteCluster::DataTable},
		{TEXT("get_object_properties"), EBlueprintHelperBridgeRouteCluster::ObjectProperty},
		{TEXT("preview_task_plan"), EBlueprintHelperBridgeRouteCluster::TaskRuntime},
		{TEXT("diagnostics_runtime"), EBlueprintHelperBridgeRouteCluster::Debug},
		{TEXT("get_debug_case"), EBlueprintHelperBridgeRouteCluster::Debug},
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
