#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "MaterialEditingLibrary.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphSchema.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.h"
#include "Systems/ToolClusters/Material/BlueprintHelperMaterialLogicJsonExtractor.h"
#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialGraphFacade.h"
#include "UI/Review/BlueprintHelperReviewDebugProjectionRegistry.h"
#include "UI/Review/BlueprintHelperReviewSurfaceProjectionRegistry.h"
#include "UObject/Package.h"

class FBlueprintHelperMaterialGraphProjectionTestsLocalUtils
{
public:
	static UMaterial* MakeMaterialFixture(const FString& Prefix)
	{
		const FString AssetName = FString::Printf(
			TEXT("%s_%s"),
			*Prefix,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperMaterialGraphProjectionTests/%s"),
			*AssetName));
		if (!Package)
		{
			return nullptr;
		}

		UMaterial* Material = NewObject<UMaterial>(
			Package,
			UMaterial::StaticClass(),
			*AssetName,
			RF_Public | RF_Standalone | RF_Transactional);
		if (!Material)
		{
			return nullptr;
		}

		Material->MaterialGraph = CastChecked<UMaterialGraph>(FBlueprintEditorUtils::CreateNewGraph(
			Material,
			NAME_None,
			UMaterialGraph::StaticClass(),
			UMaterialGraphSchema::StaticClass()));
		Material->MaterialGraph->Material = Material;
		Material->MaterialGraph->RebuildGraph();
		Package->SetDirtyFlag(false);
		return Material;
	}

	static TSharedRef<FJsonObject> MakeEndpoint(const FString& NodeKey, const FString& Pin)
	{
		TSharedRef<FJsonObject> Endpoint = MakeShared<FJsonObject>();
		Endpoint->SetStringField(TEXT("node_key"), NodeKey);
		Endpoint->SetStringField(TEXT("pin"), Pin);
		return Endpoint;
	}

	static TSharedRef<FJsonObject> MakeSpawnScalarOp(
		const FString& BlockId,
		const FString& NodeKey,
		const FString& ParameterName,
		const double DefaultValue)
	{
		TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
		Properties->SetStringField(TEXT("parameter_name"), ParameterName);
		Properties->SetStringField(TEXT("group"), TEXT("BlueprintHelper"));
		Properties->SetNumberField(TEXT("default_value"), DefaultValue);

		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("spawn_material_expression"));
		Op->SetStringField(TEXT("block_id"), BlockId);
		Op->SetStringField(TEXT("node_key"), NodeKey);
		Op->SetStringField(TEXT("selector"), TEXT("scalar_parameter"));
		Op->SetObjectField(TEXT("properties"), Properties);
		return Op;
	}

	static TSharedRef<FJsonObject> MakeConnectOutputOp(
		const FString& FromNodeKey,
		const FString& FromPin,
		const FString& OutputPin)
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("connect_material_property"));
		Op->SetObjectField(TEXT("from"), MakeEndpoint(FromNodeKey, FromPin));
		Op->SetObjectField(TEXT("to"), MakeEndpoint(TEXT("$material_output"), OutputPin));
		return Op;
	}

	static TSharedRef<FJsonObject> MakeExpressionRef(
		const FString& NodeKey,
		const FString& BlockId,
		const FString& ExpressionGuid,
		const FString& ClassName,
		const FString& Selector)
	{
		TSharedRef<FJsonObject> Expression = MakeShared<FJsonObject>();
		Expression->SetStringField(TEXT("node_key"), NodeKey);
		Expression->SetStringField(TEXT("block_id"), BlockId);
		Expression->SetStringField(TEXT("expression_guid"), ExpressionGuid);
		Expression->SetStringField(TEXT("class_name"), ClassName);
		Expression->SetStringField(TEXT("selector"), Selector);
		return Expression;
	}

	static TSharedRef<FJsonObject> MakeConnectionRef(
		const FString& FromNodeKey,
		const FString& FromPin,
		const FString& ToNodeKey,
		const FString& ToPin)
	{
		TSharedRef<FJsonObject> Connection = MakeShared<FJsonObject>();
		Connection->SetStringField(TEXT("from_node_key"), FromNodeKey);
		Connection->SetStringField(TEXT("from_pin"), FromPin);
		Connection->SetStringField(TEXT("to_node_key"), ToNodeKey);
		Connection->SetStringField(TEXT("to_pin"), ToPin);
		return Connection;
	}

	static TSharedRef<FJsonObject> MakePayload(
		const UMaterial* Material,
		const FString& Strategy,
		const TArray<TSharedPtr<FJsonValue>>& Ops)
	{
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), Material ? Material->GetPathName() : FString());

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("target"), Target);
		Payload->SetStringField(TEXT("material_strategy"), Strategy);
		Payload->SetArrayField(TEXT("ops"), Ops);
		return Payload;
	}

	static bool LogicJsonOutputUsesNodeKey(
		const TSharedPtr<FJsonObject>& LogicJson,
		const FString& PropertyName,
		const FString& ExpectedNodeKey)
	{
		const TSharedPtr<FJsonObject>* Logic = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Outputs = nullptr;
		if (!LogicJson.IsValid() ||
			!LogicJson->TryGetObjectField(TEXT("logic"), Logic) ||
			!Logic ||
			!Logic->IsValid() ||
			!(*Logic)->TryGetArrayField(TEXT("material_outputs"), Outputs))
		{
			return false;
		}

		for (const TSharedPtr<FJsonValue>& OutputValue : *Outputs)
		{
			const TSharedPtr<FJsonObject> Output = OutputValue.IsValid() ? OutputValue->AsObject() : nullptr;
			FString Property;
			FString SourceNodeKey;
			if (Output.IsValid() &&
				Output->TryGetStringField(TEXT("property"), Property) &&
				Output->TryGetStringField(TEXT("source_node_key"), SourceNodeKey) &&
				Property == PropertyName &&
				SourceNodeKey == ExpectedNodeKey)
			{
				return true;
			}
		}
		return false;
	}

	static int32 CountTargetsByKind(
		const FBlueprintHelperWriteReviewEvidence& Evidence,
		const FString& TargetKind)
	{
		int32 Count = 0;
		for (const FBlueprintHelperReviewAtomicTarget& Target : Evidence.AtomicTargets)
		{
			if (Target.TargetKind == TargetKind)
			{
				++Count;
			}
		}
		return Count;
	}

	static bool AllTargetsUseMaterialSurface(const FBlueprintHelperWriteReviewEvidence& Evidence)
	{
		for (const FBlueprintHelperReviewAtomicTarget& Target : Evidence.AtomicTargets)
		{
			if (Target.Surface != EBlueprintHelperReviewSurface::Material ||
				Target.GraphName != TEXT("MaterialGraph"))
			{
				return false;
			}
		}
		return true;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialGraphReviewEvidenceAdapterTest,
	"BlueprintHelper.MaterialGraph.Review.EvidenceAdapter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialGraphReviewEvidenceAdapterTest::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), TEXT("/Game/M_BH_MaterialReviewEvidence"));
	Target->SetStringField(TEXT("graph"), TEXT("MaterialGraph"));
	Target->SetStringField(TEXT("target_type"), TEXT("material_graph"));

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	LoweredStep.Capability = TEXT("graph_write");
	LoweredStep.AdapterOperation = TEXT("material_graph_edit");
	LoweredStep.Payload = MakeShared<FJsonObject>();
	LoweredStep.Payload->SetObjectField(TEXT("target"), Target);

	TArray<TSharedPtr<FJsonValue>> CreatedExpressions;
	CreatedExpressions.Add(MakeShared<FJsonValueObject>(
		FBlueprintHelperMaterialGraphProjectionTestsLocalUtils::MakeExpressionRef(
			TEXT("created_scalar"),
			TEXT("review_block"),
			TEXT("11111111-1111-1111-1111-111111111111"),
			TEXT("MaterialExpressionScalarParameter"),
			TEXT("scalar_parameter"))));

	TSharedRef<FJsonObject> UpdatedExpression =
		FBlueprintHelperMaterialGraphProjectionTestsLocalUtils::MakeExpressionRef(
			TEXT("updated_scalar"),
			TEXT("review_block"),
			TEXT("22222222-2222-2222-2222-222222222222"),
			TEXT("MaterialExpressionScalarParameter"),
			TEXT("scalar_parameter"));
	TSharedRef<FJsonObject> BeforeUpdate = MakeShared<FJsonObject>();
	BeforeUpdate->SetNumberField(TEXT("default_value"), 0.1);
	TSharedRef<FJsonObject> AfterUpdate = MakeShared<FJsonObject>();
	AfterUpdate->SetNumberField(TEXT("default_value"), 0.8);
	UpdatedExpression->SetObjectField(TEXT("before"), BeforeUpdate);
	UpdatedExpression->SetObjectField(TEXT("after"), AfterUpdate);
	TArray<TSharedPtr<FJsonValue>> UpdatedExpressions;
	UpdatedExpressions.Add(MakeShared<FJsonValueObject>(UpdatedExpression));

	TArray<TSharedPtr<FJsonValue>> DeletedExpressions;
	DeletedExpressions.Add(MakeShared<FJsonValueObject>(
		FBlueprintHelperMaterialGraphProjectionTestsLocalUtils::MakeExpressionRef(
			TEXT("deleted_scalar"),
			TEXT("review_block"),
			TEXT("33333333-3333-3333-3333-333333333333"),
			TEXT("MaterialExpressionScalarParameter"),
			TEXT("scalar_parameter"))));

	TArray<TSharedPtr<FJsonValue>> Connections;
	Connections.Add(MakeShared<FJsonValueObject>(
		FBlueprintHelperMaterialGraphProjectionTestsLocalUtils::MakeConnectionRef(
			TEXT("created_scalar"),
			TEXT("Value"),
			TEXT("updated_scalar"),
			TEXT("A"))));
	Connections.Add(MakeShared<FJsonValueObject>(
		FBlueprintHelperMaterialGraphProjectionTestsLocalUtils::MakeConnectionRef(
			TEXT("updated_scalar"),
			TEXT("Value"),
			TEXT("$material_output"),
			TEXT("Roughness"))));

	FBlueprintHelperToolResultBase StepResult;
	StepResult.bOk = true;
	StepResult.Status = EBlueprintHelperToolStatus::Applied;
	StepResult.Data = MakeShared<FJsonObject>();
	StepResult.Data->SetArrayField(TEXT("created_expression_refs"), CreatedExpressions);
	StepResult.Data->SetArrayField(TEXT("updated_property_refs"), UpdatedExpressions);
	StepResult.Data->SetArrayField(TEXT("deleted_expression_refs"), DeletedExpressions);
	StepResult.Data->SetArrayField(TEXT("connections"), Connections);

	FBlueprintHelperWriteReviewEvidence Evidence;
	const bool bBuilt = FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(
		LoweredStep,
		StepResult,
		TEXT("archive_material_review_adapter"),
		TEXT("task_material_review_adapter"),
		9,
		Evidence);

	TestTrue(TEXT("MaterialGraph review evidence builds"), bBuilt);
	TestEqual(TEXT("all material atomic targets are emitted"), Evidence.AtomicTargets.Num(), 5);
	TestEqual(
		TEXT("three expression targets are emitted"),
		FBlueprintHelperMaterialGraphProjectionTestsLocalUtils::CountTargetsByKind(Evidence, TEXT("material_expression")),
		3);
	TestEqual(
		TEXT("one expression link target is emitted"),
		FBlueprintHelperMaterialGraphProjectionTestsLocalUtils::CountTargetsByKind(Evidence, TEXT("material_expression_link")),
		1);
	TestEqual(
		TEXT("one material output link target is emitted"),
		FBlueprintHelperMaterialGraphProjectionTestsLocalUtils::CountTargetsByKind(Evidence, TEXT("material_output_link")),
		1);
	TestTrue(
		TEXT("all review targets route to Material surface"),
		FBlueprintHelperMaterialGraphProjectionTestsLocalUtils::AllTargetsUseMaterialSurface(Evidence));
	return bBuilt && Evidence.AtomicTargets.Num() == 5;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialGraphSurfaceProjectionAdapterTest,
	"BlueprintHelper.MaterialGraph.Surface.ProjectionAdapter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialGraphSurfaceProjectionAdapterTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("material_graph_surface_change");
	Change.AssetPath = TEXT("/Game/M_BH_Surface");
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
	Change.Status = EBlueprintHelperReviewChangeStatus::Pending;
	Change.DisplayLabel = TEXT("BH Surface Scalar");

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Change.AssetPath;
	Target.Surface = EBlueprintHelperReviewSurface::Material;
	Target.TargetKind = TEXT("material_expression");
	Target.TargetKey = TEXT("material_expression:bh_surface_scalar");
	Target.DisplayLabel = TEXT("BH Surface Scalar");
	Target.Status = EBlueprintHelperReviewChangeStatus::Pending;
	Change.AtomicTargets.Add(Target);

	const TSharedRef<FBlueprintHelperReviewSurfaceProjectionRegistry> Registry =
		FBlueprintHelperReviewSurfaceProjectionRegistry::CreateDefault();
	const TArray<FBlueprintHelperReviewSurfaceDiffProjectionModel> Models =
		Registry->ProjectVisibleChange(Change, TEXT("material"), TEXT("material"));

	TestEqual(TEXT("one MaterialGraph surface diff model"), Models.Num(), 1);
	if (Models.Num() == 1)
	{
		TestEqual(TEXT("review event id"), Models[0].ReviewEventId, Change.ChangeId);
		TestEqual(TEXT("surface kind"), Models[0].SurfaceKind, FString(TEXT("material")));
		TestEqual(TEXT("target kind"), Models[0].TargetKind, Target.TargetKind);
		TestEqual(TEXT("target key"), Models[0].TargetKey, Target.TargetKey);
		TestTrue(TEXT("material model can accept"), Models[0].bCanAccept);
		TestTrue(TEXT("material model can reject"), Models[0].bCanReject);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialGraphDebugProjectionAdapterTest,
	"BlueprintHelper.MaterialGraph.Debug.ProjectionAdapter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialGraphDebugProjectionAdapterTest::RunTest(const FString& Parameters)
{
	const TSharedRef<FBlueprintHelperReviewDebugProjectionRegistry> Registry =
		FBlueprintHelperReviewDebugProjectionRegistry::CreateDefault();

	TSharedRef<FJsonObject> EventJson = MakeShared<FJsonObject>();
	EventJson->SetStringField(TEXT("event_type"), TEXT("material_graph.connectivity_validation"));
	EventJson->SetStringField(TEXT("review_event_id"), TEXT("material_graph_review_event"));
	EventJson->SetStringField(TEXT("asset_path"), TEXT("/Game/M_BH_Debug"));
	EventJson->SetStringField(TEXT("target_key"), TEXT("material_output_link:roughness"));
	EventJson->SetStringField(TEXT("surface_kind"), TEXT("material"));
	EventJson->SetStringField(TEXT("result"), TEXT("valid"));
	EventJson->SetStringField(TEXT("message"), TEXT("connected"));

	const TArray<FBlueprintHelperReviewDebugEventModel> Events = Registry->ProjectEvent(EventJson);

	TestEqual(TEXT("one MaterialGraph debug projection event"), Events.Num(), 1);
	if (Events.Num() == 1)
	{
		TestEqual(TEXT("event type"), Events[0].EventType, FString(TEXT("material_graph.connectivity_validation")));
		TestEqual(TEXT("review event id"), Events[0].ReviewEventId, FString(TEXT("material_graph_review_event")));
		TestEqual(TEXT("target key"), Events[0].TargetKey, FString(TEXT("material_output_link:roughness")));
		TestEqual(TEXT("surface kind"), Events[0].SurfaceKind, FString(TEXT("material")));
		TestEqual(TEXT("result"), Events[0].Result, FString(TEXT("valid")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialGraphReadContextProjectionTest,
	"BlueprintHelper.MaterialGraph.ReadContext.Projection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialGraphReadContextProjectionTest::RunTest(const FString& Parameters)
{
	UMaterial* Material = FBlueprintHelperMaterialGraphProjectionTestsLocalUtils::MakeMaterialFixture(TEXT("M_BH_ReadContext"));
	TestNotNull(TEXT("Material fixture is created"), Material);
	if (!Material)
	{
		return false;
	}

	const FString NodeKey = TEXT("material_graph_read_context_scalar");
	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(
		FBlueprintHelperMaterialGraphProjectionTestsLocalUtils::MakeSpawnScalarOp(
			TEXT("read_context_block"),
			NodeKey,
			TEXT("BH_ReadContext_Roughness"),
			0.31)));
	Ops.Add(MakeShared<FJsonValueObject>(
		FBlueprintHelperMaterialGraphProjectionTestsLocalUtils::MakeConnectOutputOp(
			NodeKey,
			TEXT("Value"),
			TEXT("Roughness"))));

	FBlueprintHelperMaterialGraphFacade Facade;
	FBlueprintHelperMaterialGraphExecutionInput Input;
	Input.Payload = FBlueprintHelperMaterialGraphProjectionTestsLocalUtils::MakePayload(
		Material,
		TEXT("append_new_owned_graph"),
		Ops);
	Input.bDryRun = false;

	const FBlueprintHelperToolResultBase Result = Facade.Execute(Input);
	TestTrue(TEXT("MaterialGraph execute succeeds"), Result.bOk);
	if (!Result.bOk)
	{
		Material->GetOutermost()->SetDirtyFlag(false);
		return false;
	}

	FBlueprintHelperMaterialLogicJsonExtractor Extractor;
	TSharedPtr<FJsonObject> LogicJson;
	FString Error;
	TestTrue(TEXT("material read context builds"), Extractor.BuildLogicJson(Material->GetPathName(), LogicJson, Error));
	if (!LogicJson.IsValid())
	{
		Material->GetOutermost()->SetDirtyFlag(false);
		return false;
	}

	TestEqual(TEXT("schema"), LogicJson->GetStringField(TEXT("schema")), FString(TEXT("LogicJson.v1")));
	TestEqual(TEXT("scope"), LogicJson->GetStringField(TEXT("scope")), FString(TEXT("material_graph")));
	TestTrue(
		TEXT("Roughness read context output uses owned node key"),
		FBlueprintHelperMaterialGraphProjectionTestsLocalUtils::LogicJsonOutputUsesNodeKey(
			LogicJson,
			TEXT("Roughness"),
			NodeKey));
	TestTrue(TEXT("diagnostics field exists"), LogicJson->HasTypedField<EJson::Array>(TEXT("diagnostics")));

	Material->GetOutermost()->SetDirtyFlag(false);
	return true;
}

#endif
