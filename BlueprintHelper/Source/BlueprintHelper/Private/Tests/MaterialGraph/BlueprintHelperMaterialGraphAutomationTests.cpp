#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "MaterialEditingLibrary.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphNode_Root.h"
#include "MaterialGraph/MaterialGraphSchema.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "Systems/Review/Utils/BlueprintHelperReviewSnapshotRestoreService.h"
#include "Systems/ToolClusters/Material/BlueprintHelperMaterialLogicJsonExtractor.h"
#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialGraphFacade.h"
#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialGraphOwnershipService.h"
#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialGraphReadbackService.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

class FBlueprintHelperMaterialGraphAutomationTestUtils
{
public:
	static UMaterial* MakeMaterialFixture(const FString& Prefix)
	{
		const FString AssetName = FString::Printf(
			TEXT("%s_%s"),
			*Prefix,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperMaterialGraphTests/%s"),
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

	static TSharedRef<FJsonObject> MakeSpawnVectorOp(
		const FString& BlockId,
		const FString& NodeKey,
		const FString& ParameterName)
	{
		TSharedRef<FJsonObject> DefaultValue = MakeShared<FJsonObject>();
		DefaultValue->SetNumberField(TEXT("r"), 0.1);
		DefaultValue->SetNumberField(TEXT("g"), 0.2);
		DefaultValue->SetNumberField(TEXT("b"), 0.3);
		DefaultValue->SetNumberField(TEXT("a"), 1.0);

		TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
		Properties->SetStringField(TEXT("parameter_name"), ParameterName);
		Properties->SetStringField(TEXT("group"), TEXT("BlueprintHelper"));
		Properties->SetObjectField(TEXT("default_value"), DefaultValue);

		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("spawn_material_expression"));
		Op->SetStringField(TEXT("block_id"), BlockId);
		Op->SetStringField(TEXT("node_key"), NodeKey);
		Op->SetStringField(TEXT("selector"), TEXT("vector_parameter"));
		Op->SetObjectField(TEXT("properties"), Properties);
		return Op;
	}

	static TSharedRef<FJsonObject> MakeSpawnCommonSelectorOp(
		const FString& BlockId,
		const FString& NodeKey,
		const FString& Selector)
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("spawn_material_expression"));
		Op->SetStringField(TEXT("block_id"), BlockId);
		Op->SetStringField(TEXT("node_key"), NodeKey);
		Op->SetStringField(TEXT("selector"), Selector);
		return Op;
	}

	static TSharedRef<FJsonObject> MakeConnectExpressionOp(
		const FString& FromNodeKey,
		const FString& FromPin,
		const FString& ToNodeKey,
		const FString& ToPin)
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("connect_material_expression"));
		Op->SetObjectField(TEXT("from"), MakeEndpoint(FromNodeKey, FromPin));
		Op->SetObjectField(TEXT("to"), MakeEndpoint(ToNodeKey, ToPin));
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

	static TSharedRef<FJsonObject> MakeDeleteOp(const FString& NodeKey)
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("delete_owned_material_expression"));
		Op->SetStringField(TEXT("node_key"), NodeKey);
		return Op;
	}

	static TSharedRef<FJsonObject> MakeSetScalarDefaultOp(const FString& NodeKey, const double DefaultValue)
	{
		TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
		Properties->SetNumberField(TEXT("default_value"), DefaultValue);

		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("set_material_expression_properties"));
		Op->SetStringField(TEXT("node_key"), NodeKey);
		Op->SetObjectField(TEXT("properties"), Properties);
		return Op;
	}

	static TSharedRef<FJsonObject> MakeResolveQueryOp(const FString& Query)
	{
		TSharedRef<FJsonObject> Selector = MakeShared<FJsonObject>();
		Selector->SetStringField(TEXT("query"), Query);

		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("resolve_material_expression"));
		Op->SetObjectField(TEXT("selector"), Selector);
		return Op;
	}

	static TSharedRef<FJsonObject> MakeSpawnCandidateOp(
		const FString& BlockId,
		const FString& NodeKey,
		const FString& CandidateId)
	{
		TSharedRef<FJsonObject> Selector = MakeShared<FJsonObject>();
		Selector->SetStringField(TEXT("candidate_id"), CandidateId);

		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("spawn_material_expression"));
		Op->SetStringField(TEXT("block_id"), BlockId);
		Op->SetStringField(TEXT("node_key"), NodeKey);
		Op->SetObjectField(TEXT("selector"), Selector);
		return Op;
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

	static TArray<TSharedPtr<FJsonValue>> MakeAppendOps(
		const FString& BlockId,
		const FString& RoughnessNodeKey,
		const FString& BaseColorNodeKey)
	{
		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(MakeSpawnScalarOp(BlockId, RoughnessNodeKey, TEXT("BH_Test_Roughness"), 0.42)));
		Ops.Add(MakeShared<FJsonValueObject>(MakeSpawnVectorOp(BlockId, BaseColorNodeKey, TEXT("BH_Test_BaseColor"))));
		Ops.Add(MakeShared<FJsonValueObject>(MakeConnectOutputOp(RoughnessNodeKey, TEXT("Value"), TEXT("Roughness"))));
		Ops.Add(MakeShared<FJsonValueObject>(MakeConnectOutputOp(BaseColorNodeKey, TEXT("RGB"), TEXT("BaseColor"))));
		return Ops;
	}

	static FString ReadMetadata(const UMaterialExpression* Expression, const TCHAR* Key)
	{
		if (!Expression || !Key)
		{
			return FString();
		}
#if WITH_METADATA
		if (UPackage* Package = Expression->GetPackage())
		{
			return Package->GetMetaData().GetValue(Expression, Key);
		}
#endif
		return FString();
	}

	static UMaterialExpression* FindOwnedExpressionByNodeKey(UMaterial* Material, const FString& NodeKey)
	{
		if (!Material)
		{
			return nullptr;
		}
		for (UMaterialExpression* Expression : Material->GetExpressions())
		{
			if (ReadMetadata(Expression, FBlueprintHelperMaterialGraphOwnershipService::NodeKeyMetadataKey()) == NodeKey)
			{
				return Expression;
			}
		}
		return nullptr;
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

	static bool LogicJsonHasDiagnosticCode(
		const TSharedPtr<FJsonObject>& LogicJson,
		const FString& ExpectedCode)
	{
		const TArray<TSharedPtr<FJsonValue>>* Diagnostics = nullptr;
		if (!LogicJson.IsValid() ||
			!LogicJson->TryGetArrayField(TEXT("diagnostics"), Diagnostics) ||
			!Diagnostics)
		{
			return false;
		}

		for (const TSharedPtr<FJsonValue>& DiagnosticValue : *Diagnostics)
		{
			const TSharedPtr<FJsonObject> Diagnostic = DiagnosticValue.IsValid()
				? DiagnosticValue->AsObject()
				: nullptr;
			FString Code;
			if (Diagnostic.IsValid() &&
				Diagnostic->TryGetStringField(TEXT("code"), Code) &&
				Code.Equals(ExpectedCode, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	static bool CandidateRowsAreCompact(const TArray<TSharedPtr<FJsonValue>>& Candidates)
	{
		for (const TSharedPtr<FJsonValue>& CandidateValue : Candidates)
		{
			const TSharedPtr<FJsonObject> Candidate = CandidateValue.IsValid() ? CandidateValue->AsObject() : nullptr;
			if (!Candidate.IsValid() ||
				Candidate->Values.Num() != 5 ||
				!Candidate->HasField(TEXT("candidate_id")) ||
				!Candidate->HasField(TEXT("display_name")) ||
				!Candidate->HasField(TEXT("class_name")) ||
				!Candidate->HasField(TEXT("category")) ||
				!Candidate->HasField(TEXT("reason")) ||
				Candidate->HasField(TEXT("selector")) ||
				Candidate->HasField(TEXT("fingerprint")) ||
				Candidate->HasField(TEXT("class_catalog_revision")))
			{
				return false;
			}
		}
		return true;
	}

	static bool FindCandidateIdForClass(
		const TArray<TSharedPtr<FJsonValue>>& Candidates,
		const FString& ExpectedClassName,
		FString& OutCandidateId)
	{
		for (const TSharedPtr<FJsonValue>& CandidateValue : Candidates)
		{
			const TSharedPtr<FJsonObject> Candidate = CandidateValue.IsValid() ? CandidateValue->AsObject() : nullptr;
			FString CandidateClassName;
			FString CandidateId;
			if (Candidate.IsValid() &&
				Candidate->TryGetStringField(TEXT("class_name"), CandidateClassName) &&
				Candidate->TryGetStringField(TEXT("candidate_id"), CandidateId) &&
				(CandidateClassName == ExpectedClassName || CandidateClassName == FString::Printf(TEXT("U%s"), *ExpectedClassName)))
			{
				OutCandidateId = CandidateId;
				return true;
			}
		}
		return false;
	}

	static bool HasCandidateClassContaining(
		const TArray<TSharedPtr<FJsonValue>>& Candidates,
		const FString& ClassNameNeedle)
	{
		for (const TSharedPtr<FJsonValue>& CandidateValue : Candidates)
		{
			const TSharedPtr<FJsonObject> Candidate = CandidateValue.IsValid() ? CandidateValue->AsObject() : nullptr;
			FString CandidateClassName;
			if (Candidate.IsValid() &&
				Candidate->TryGetStringField(TEXT("class_name"), CandidateClassName) &&
				CandidateClassName.Contains(ClassNameNeedle))
			{
				return true;
			}
		}
		return false;
	}

	static bool MaterialGraphRootOutputLinksExpression(
		const UMaterialGraph* MaterialGraph,
		EMaterialProperty MaterialProperty,
		const UMaterialExpression* ExpectedExpression)
	{
		if (!MaterialGraph || !MaterialGraph->RootNode || !ExpectedExpression || !ExpectedExpression->GraphNode)
		{
			return false;
		}

		int32 MaterialInputIndex = INDEX_NONE;
		for (int32 Index = 0; Index < MaterialGraph->MaterialInputs.Num(); ++Index)
		{
			if (MaterialGraph->MaterialInputs[Index].GetProperty() == MaterialProperty)
			{
				MaterialInputIndex = Index;
				break;
			}
		}
		if (MaterialInputIndex == INDEX_NONE)
		{
			return false;
		}

		UEdGraphPin* InputPin = MaterialGraph->RootNode->GetInputPin(MaterialInputIndex);
		if (!InputPin)
		{
			return false;
		}

		for (UEdGraphPin* LinkedPin : InputPin->LinkedTo)
		{
			if (LinkedPin && LinkedPin->GetOwningNode() == ExpectedExpression->GraphNode)
			{
				return true;
			}
		}
		return false;
	}

	static FString SerializeJsonObject(const TSharedRef<FJsonObject>& Object)
	{
		FString Output;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(Object, Writer);
		return Output;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialGraphAppendReadbackAutomationTest,
	"BlueprintHelper.MaterialGraph.AppendCommonSelectorsReadback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialGraphAppendReadbackAutomationTest::RunTest(const FString& Parameters)
{
	UMaterial* Material = FBlueprintHelperMaterialGraphAutomationTestUtils::MakeMaterialFixture(TEXT("M_BH_Append"));
	TestNotNull(TEXT("Material fixture is created"), Material);
	if (!Material)
	{
		return false;
	}

	const FString BlockId = TEXT("automation_append_block");
	const FString RoughnessNodeKey = TEXT("automation_roughness");
	const FString BaseColorNodeKey = TEXT("automation_base_color");
	const TArray<TSharedPtr<FJsonValue>> Ops =
		FBlueprintHelperMaterialGraphAutomationTestUtils::MakeAppendOps(BlockId, RoughnessNodeKey, BaseColorNodeKey);

	FBlueprintHelperMaterialGraphFacade Facade;
	FBlueprintHelperMaterialGraphExecutionInput Input;
	Input.Payload = FBlueprintHelperMaterialGraphAutomationTestUtils::MakePayload(
		Material,
		TEXT("append_new_owned_graph"),
		Ops);
	Input.bDryRun = false;

	const FBlueprintHelperToolResultBase Result = Facade.Execute(Input);
	TestTrue(TEXT("append execute succeeds"), Result.bOk);
	if (!Result.bOk && Result.Error.IsSet())
	{
		AddError(FString::Printf(TEXT("append failed: %s"), *Result.Error->Message));
		return false;
	}

	UMaterialExpression* RoughnessExpression =
		FBlueprintHelperMaterialGraphAutomationTestUtils::FindOwnedExpressionByNodeKey(Material, RoughnessNodeKey);
	UMaterialExpression* BaseColorExpression =
		FBlueprintHelperMaterialGraphAutomationTestUtils::FindOwnedExpressionByNodeKey(Material, BaseColorNodeKey);
	TestNotNull(TEXT("roughness owned expression exists"), RoughnessExpression);
	TestNotNull(TEXT("base color owned expression exists"), BaseColorExpression);
	TestEqual(
		TEXT("roughness block metadata is written"),
		FBlueprintHelperMaterialGraphAutomationTestUtils::ReadMetadata(
			RoughnessExpression,
			FBlueprintHelperMaterialGraphOwnershipService::BlockIdMetadataKey()),
		BlockId);
	TestEqual(
		TEXT("base color block metadata is written"),
		FBlueprintHelperMaterialGraphAutomationTestUtils::ReadMetadata(
			BaseColorExpression,
			FBlueprintHelperMaterialGraphOwnershipService::BlockIdMetadataKey()),
		BlockId);
	TestEqual(
		TEXT("Roughness material output points at owned expression"),
		UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, MP_Roughness),
		RoughnessExpression);
	TestEqual(
		TEXT("BaseColor material output points at owned expression"),
		UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, MP_BaseColor),
		BaseColorExpression);
	TestNotNull(TEXT("material graph exists for graph pin readback"), Material->MaterialGraph.Get());
	TestNotNull(
		TEXT("material graph root node exists for graph pin readback"),
		Material->MaterialGraph ? Material->MaterialGraph->RootNode.Get() : nullptr);
	TestTrue(
		TEXT("Roughness material graph root pin links to owned expression"),
		FBlueprintHelperMaterialGraphAutomationTestUtils::MaterialGraphRootOutputLinksExpression(
			Material->MaterialGraph,
			MP_Roughness,
			RoughnessExpression));
	TestTrue(
		TEXT("BaseColor material graph root pin links to owned expression"),
		FBlueprintHelperMaterialGraphAutomationTestUtils::MaterialGraphRootOutputLinksExpression(
			Material->MaterialGraph,
			MP_BaseColor,
			BaseColorExpression));

	FBlueprintHelperMaterialLogicJsonExtractor Extractor;
	TSharedPtr<FJsonObject> LogicJson;
	FString Error;
	TestTrue(TEXT("material readback logic json builds"), Extractor.BuildLogicJson(Material->GetPathName(), LogicJson, Error));
	TestTrue(TEXT("Roughness output readback uses owned node_key"),
		FBlueprintHelperMaterialGraphAutomationTestUtils::LogicJsonOutputUsesNodeKey(LogicJson, TEXT("Roughness"), RoughnessNodeKey));
	TestTrue(TEXT("BaseColor output readback uses owned node_key"),
		FBlueprintHelperMaterialGraphAutomationTestUtils::LogicJsonOutputUsesNodeKey(LogicJson, TEXT("BaseColor"), BaseColorNodeKey));

	Material->GetOutermost()->SetDirtyFlag(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialGraphReadbackReportsExtractorDiagnosticsAutomationTest,
	"BlueprintHelper.MaterialGraph.ReadbackReportsExtractorDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialGraphReadbackReportsExtractorDiagnosticsAutomationTest::RunTest(const FString& Parameters)
{
	UMaterial* Material = FBlueprintHelperMaterialGraphAutomationTestUtils::MakeMaterialFixture(TEXT("M_BH_ReadbackDiagnostics"));
	TestNotNull(TEXT("Material fixture is created"), Material);
	if (!Material)
	{
		return false;
	}

	UMaterialExpressionScalarParameter* MissingAnchorExpression = NewObject<UMaterialExpressionScalarParameter>(
		Material,
		UMaterialExpressionScalarParameter::StaticClass(),
		NAME_None,
		RF_Transactional);
	TestNotNull(TEXT("missing anchor expression is created"), MissingAnchorExpression);
	if (!MissingAnchorExpression)
	{
		Material->GetOutermost()->SetDirtyFlag(false);
		return false;
	}

	MissingAnchorExpression->ParameterName = TEXT("BH_MissingAnchor");
	MissingAnchorExpression->MaterialExpressionGuid.Invalidate();
	MissingAnchorExpression->ExpressionGUID = FGuid::NewGuid();
	Material->GetExpressionCollection().AddExpression(MissingAnchorExpression);

	UMaterialExpressionScalarParameter* PartialParameterExpression = NewObject<UMaterialExpressionScalarParameter>(
		Material,
		UMaterialExpressionScalarParameter::StaticClass(),
		NAME_None,
		RF_Transactional);
	TestNotNull(TEXT("partial parameter expression is created"), PartialParameterExpression);
	if (!PartialParameterExpression)
	{
		Material->GetOutermost()->SetDirtyFlag(false);
		return false;
	}

	PartialParameterExpression->ParameterName = NAME_None;
	PartialParameterExpression->MaterialExpressionGuid = FGuid::NewGuid();
	PartialParameterExpression->ExpressionGUID.Invalidate();
	Material->GetExpressionCollection().AddExpression(PartialParameterExpression);

	FBlueprintHelperMaterialLogicJsonExtractor Extractor;
	TSharedPtr<FJsonObject> LogicJson;
	FString Error;
	TestTrue(
		TEXT("material readback logic json builds for diagnostic fixture"),
		Extractor.BuildLogicJson(Material->GetPathName(), LogicJson, Error));
	if (!Error.IsEmpty())
	{
		AddError(FString::Printf(TEXT("extractor error: %s"), *Error));
	}
	TestTrue(
		TEXT("readback reports missing expression anchor diagnostic"),
		FBlueprintHelperMaterialGraphAutomationTestUtils::LogicJsonHasDiagnosticCode(
			LogicJson,
			TEXT("material_expression_anchor_missing")));
	TestTrue(
		TEXT("readback reports partial parameter diagnostic"),
		FBlueprintHelperMaterialGraphAutomationTestUtils::LogicJsonHasDiagnosticCode(
			LogicJson,
			TEXT("material_parameter_read_partial")));

	Material->GetOutermost()->SetDirtyFlag(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialGraphPreviewRejectsInvalidOutputAutomationTest,
	"BlueprintHelper.MaterialGraph.PreviewRejectsInvalidMaterialOutput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialGraphPreviewRejectsInvalidOutputAutomationTest::RunTest(const FString& Parameters)
{
	UMaterial* Material = FBlueprintHelperMaterialGraphAutomationTestUtils::MakeMaterialFixture(TEXT("M_BH_InvalidOutput"));
	TestNotNull(TEXT("Material fixture is created"), Material);
	if (!Material)
	{
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(
		FBlueprintHelperMaterialGraphAutomationTestUtils::MakeSpawnScalarOp(
			TEXT("invalid_output_block"),
			TEXT("invalid_output_scalar"),
			TEXT("BH_Invalid_Output"),
			0.25)));
	Ops.Add(MakeShared<FJsonValueObject>(
		FBlueprintHelperMaterialGraphAutomationTestUtils::MakeConnectOutputOp(
			TEXT("invalid_output_scalar"),
			TEXT("Value"),
			TEXT("NotAMaterialProperty"))));

	FBlueprintHelperMaterialGraphFacade Facade;
	FBlueprintHelperMaterialGraphExecutionInput Input;
	Input.Payload = FBlueprintHelperMaterialGraphAutomationTestUtils::MakePayload(
		Material,
		TEXT("append_new_owned_graph"),
		Ops);
	Input.bDryRun = true;

	const FBlueprintHelperToolResultBase Result = Facade.Execute(Input);
	TestFalse(TEXT("invalid material output preview is blocked"), Result.bOk);
	TestTrue(TEXT("invalid material output returns structured error"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("invalid material output error code"), Result.Error->Code, FString(TEXT("material_property_not_supported")));
	}
	Material->GetOutermost()->SetDirtyFlag(false);
	return !Result.bOk;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialGraphPreviewRejectsUnconsumedExpressionAutomationTest,
	"BlueprintHelper.MaterialGraph.PreviewRejectsUnconsumedExpression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialGraphPreviewRejectsUnconsumedExpressionAutomationTest::RunTest(const FString& Parameters)
{
	UMaterial* Material = FBlueprintHelperMaterialGraphAutomationTestUtils::MakeMaterialFixture(TEXT("M_BH_Unconsumed"));
	TestNotNull(TEXT("Material fixture is created"), Material);
	if (!Material)
	{
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(
		FBlueprintHelperMaterialGraphAutomationTestUtils::MakeSpawnScalarOp(
			TEXT("unconsumed_block"),
			TEXT("unconsumed_scalar"),
			TEXT("BH_Unconsumed_Scalar"),
			0.5)));

	FBlueprintHelperMaterialGraphFacade Facade;
	FBlueprintHelperMaterialGraphExecutionInput Input;
	Input.Payload = FBlueprintHelperMaterialGraphAutomationTestUtils::MakePayload(
		Material,
		TEXT("append_new_owned_graph"),
		Ops);
	Input.bDryRun = true;

	const FBlueprintHelperToolResultBase Result = Facade.Execute(Input);
	TestFalse(TEXT("unconsumed material expression preview is blocked"), Result.bOk);
	TestTrue(TEXT("unconsumed material expression returns structured error"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(
			TEXT("unconsumed expression error code"),
			Result.Error->Code,
			FString(TEXT("material_unconsumed_expression")));
	}
	const TSharedPtr<FJsonObject>* Connectivity = nullptr;
	TestTrue(
		TEXT("unconsumed expression result exposes connectivity object"),
		Result.Data.IsValid() &&
			Result.Data->TryGetObjectField(TEXT("connectivity"), Connectivity) &&
			Connectivity &&
			Connectivity->IsValid());
	Material->GetOutermost()->SetDirtyFlag(false);
	return !Result.bOk;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialGraphRejectsDeadEndPlannedConnectionAutomationTest,
	"BlueprintHelper.MaterialGraph.RejectsDeadEndPlannedConnection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialGraphRejectsDeadEndPlannedConnectionAutomationTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperMaterialGraphExecutionState State;
	State.GeneratedExpressionNodeKeys.Add(TEXT("dead_end_source"));
	State.ExpressionClassNameByNodeKey.Add(
		TEXT("dead_end_source"),
		UMaterialExpressionScalarParameter::StaticClass()->GetName());
	State.ExpressionClassNameByNodeKey.Add(
		TEXT("dead_end_middle"),
		UMaterialExpressionAdd::StaticClass()->GetName());

	FBlueprintHelperMaterialGraphPlannedConnection Connection;
	Connection.FromNodeKey = TEXT("dead_end_source");
	Connection.FromPin = TEXT("Value");
	Connection.ToNodeKey = TEXT("dead_end_middle");
	Connection.ToPin = TEXT("A");
	Connection.FieldPath = TEXT("ops[0]");
	State.PlannedConnections.Add(Connection);

	FBlueprintHelperMaterialGraphReadbackService::ValidatePlannedExpressionConsumption(State);

	TestEqual(TEXT("dead-end planned connection creates one connectivity diagnostic"), State.ConnectivityDiagnostics.Num(), 1);
	if (State.ConnectivityDiagnostics.Num() == 0)
	{
		return false;
	}

	TestEqual(
		TEXT("dead-end planned connection diagnostic code"),
		State.ConnectivityDiagnostics[0].Code,
		FString(TEXT("material_unconsumed_expression")));
	TestTrue(
		TEXT("dead-end planned connection diagnostic explains material output reachability"),
		State.ConnectivityDiagnostics[0].Message.Contains(TEXT("no path to a material output")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialGraphAllowsPlannedOutputPathAutomationTest,
	"BlueprintHelper.MaterialGraph.AllowsPlannedOutputPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialGraphAllowsPlannedOutputPathAutomationTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperMaterialGraphExecutionState State;
	State.GeneratedExpressionNodeKeys.Add(TEXT("planned_source"));
	State.GeneratedExpressionNodeKeys.Add(TEXT("planned_middle"));
	State.ExpressionClassNameByNodeKey.Add(
		TEXT("planned_source"),
		UMaterialExpressionScalarParameter::StaticClass()->GetName());
	State.ExpressionClassNameByNodeKey.Add(
		TEXT("planned_middle"),
		UMaterialExpressionAdd::StaticClass()->GetName());

	FBlueprintHelperMaterialGraphPlannedConnection FirstConnection;
	FirstConnection.FromNodeKey = TEXT("planned_source");
	FirstConnection.FromPin = TEXT("Value");
	FirstConnection.ToNodeKey = TEXT("planned_middle");
	FirstConnection.ToPin = TEXT("A");
	FirstConnection.FieldPath = TEXT("ops[0]");
	State.PlannedConnections.Add(FirstConnection);

	FBlueprintHelperMaterialGraphPlannedConnection OutputConnection;
	OutputConnection.FromNodeKey = TEXT("planned_middle");
	OutputConnection.FromPin = TEXT("Value");
	OutputConnection.ToNodeKey = TEXT("$material_output");
	OutputConnection.ToPin = TEXT("Roughness");
	OutputConnection.bMaterialOutput = true;
	OutputConnection.FieldPath = TEXT("ops[1]");
	State.PlannedConnections.Add(OutputConnection);

	FBlueprintHelperMaterialGraphReadbackService::ValidatePlannedExpressionConsumption(State);

	TestEqual(
		TEXT("planned DAG path to material output does not create connectivity diagnostics"),
		State.ConnectivityDiagnostics.Num(),
		0);
	return State.ConnectivityDiagnostics.Num() == 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialGraphRejectsInvalidPlannedInputPinAutomationTest,
	"BlueprintHelper.MaterialGraph.RejectsInvalidPlannedInputPin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialGraphRejectsInvalidPlannedInputPinAutomationTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperMaterialGraphExecutionState State;
	State.GeneratedExpressionNodeKeys.Add(TEXT("planned_source"));
	State.GeneratedExpressionNodeKeys.Add(TEXT("planned_middle"));
	State.ExpressionClassNameByNodeKey.Add(
		TEXT("planned_source"),
		UMaterialExpressionScalarParameter::StaticClass()->GetName());
	State.ExpressionClassNameByNodeKey.Add(
		TEXT("planned_middle"),
		UMaterialExpressionAdd::StaticClass()->GetName());

	FBlueprintHelperMaterialGraphPlannedConnection InvalidInputConnection;
	InvalidInputConnection.FromNodeKey = TEXT("planned_source");
	InvalidInputConnection.FromPin = TEXT("Value");
	InvalidInputConnection.ToNodeKey = TEXT("planned_middle");
	InvalidInputConnection.ToPin = TEXT("MissingInput");
	InvalidInputConnection.FieldPath = TEXT("ops[0]");
	State.PlannedConnections.Add(InvalidInputConnection);

	FBlueprintHelperMaterialGraphPlannedConnection OutputConnection;
	OutputConnection.FromNodeKey = TEXT("planned_middle");
	OutputConnection.FromPin = TEXT("Value");
	OutputConnection.ToNodeKey = TEXT("$material_output");
	OutputConnection.ToPin = TEXT("Roughness");
	OutputConnection.bMaterialOutput = true;
	OutputConnection.FieldPath = TEXT("ops[1]");
	State.PlannedConnections.Add(OutputConnection);

	FBlueprintHelperMaterialGraphReadbackService::ValidatePlannedExpressionConsumption(State);

	TestTrue(
		TEXT("invalid planned input pin creates connectivity diagnostics"),
		State.ConnectivityDiagnostics.Num() > 0);

	bool bFoundPinDiagnostic = false;
	for (const FBlueprintHelperDiagnosticItem& Diagnostic : State.ConnectivityDiagnostics)
	{
		if (Diagnostic.Code == TEXT("material_pin_not_found") &&
			Diagnostic.Message.Contains(TEXT("MissingInput")))
		{
			bFoundPinDiagnostic = true;
			break;
		}
	}
	TestTrue(TEXT("invalid planned input pin returns material_pin_not_found"), bFoundPinDiagnostic);
	return bFoundPinDiagnostic;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialGraphAllowsExistingOutputPathAutomationTest,
	"BlueprintHelper.MaterialGraph.AllowsExistingOutputPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialGraphAllowsExistingOutputPathAutomationTest::RunTest(const FString& Parameters)
{
	UMaterial* Material = FBlueprintHelperMaterialGraphAutomationTestUtils::MakeMaterialFixture(TEXT("M_BH_ExistingPath"));
	TestNotNull(TEXT("Material fixture is created"), Material);
	if (!Material)
	{
		return false;
	}

	const FString ExistingNodeKey = TEXT("existing_roughness_output");
	FBlueprintHelperMaterialGraphFacade Facade;
	FBlueprintHelperMaterialGraphExecutionInput AppendInput;
	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(
		FBlueprintHelperMaterialGraphAutomationTestUtils::MakeSpawnCommonSelectorOp(
			TEXT("existing_path_block"),
			ExistingNodeKey,
			TEXT("add"))));
	Ops.Add(MakeShared<FJsonValueObject>(
		FBlueprintHelperMaterialGraphAutomationTestUtils::MakeConnectOutputOp(
			ExistingNodeKey,
			TEXT("Value"),
			TEXT("Roughness"))));
	AppendInput.Payload = FBlueprintHelperMaterialGraphAutomationTestUtils::MakePayload(
		Material,
		TEXT("append_new_owned_graph"),
		Ops);
	AppendInput.bDryRun = false;
	const FBlueprintHelperToolResultBase AppendResult = Facade.Execute(AppendInput);
	TestTrue(TEXT("existing output path fixture append succeeds"), AppendResult.bOk);
	if (!AppendResult.bOk)
	{
		Material->GetOutermost()->SetDirtyFlag(false);
		return false;
	}

	UMaterialExpression* ExistingOutputExpression =
		FBlueprintHelperMaterialGraphAutomationTestUtils::FindOwnedExpressionByNodeKey(Material, ExistingNodeKey);
	TestNotNull(TEXT("existing output expression is available"), ExistingOutputExpression);
	if (!ExistingOutputExpression)
	{
		Material->GetOutermost()->SetDirtyFlag(false);
		return false;
	}

	FBlueprintHelperMaterialGraphExecutionState State;
	State.Material = Material;
	State.GeneratedExpressionNodeKeys.Add(TEXT("new_source"));
	State.ExpressionClassNameByNodeKey.Add(
		TEXT("new_source"),
		UMaterialExpressionScalarParameter::StaticClass()->GetName());
	State.ExpressionsByNodeKey.Add(ExistingNodeKey, ExistingOutputExpression);

	FBlueprintHelperMaterialGraphPlannedConnection Connection;
	Connection.FromNodeKey = TEXT("new_source");
	Connection.FromPin = TEXT("Value");
	Connection.ToNodeKey = ExistingNodeKey;
	Connection.ToPin = TEXT("A");
	Connection.FieldPath = TEXT("ops[2]");
	State.PlannedConnections.Add(Connection);

	FBlueprintHelperMaterialGraphReadbackService::ValidatePlannedExpressionConsumption(State);

	TestEqual(
		TEXT("generated expression connected to an existing output chain is considered consumed"),
		State.ConnectivityDiagnostics.Num(),
		0);
	Material->GetOutermost()->SetDirtyFlag(false);
	return State.ConnectivityDiagnostics.Num() == 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialGraphOwnedStrategyAutomationTest,
	"BlueprintHelper.MaterialGraph.OwnedReplacePatchMergeRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialGraphOwnedStrategyAutomationTest::RunTest(const FString& Parameters)
{
	UMaterial* Material = FBlueprintHelperMaterialGraphAutomationTestUtils::MakeMaterialFixture(TEXT("M_BH_OwnedStrategies"));
	TestNotNull(TEXT("Material fixture is created"), Material);
	if (!Material)
	{
		return false;
	}

	FBlueprintHelperMaterialGraphFacade Facade;
	const FString BlockId = TEXT("automation_owned_block");
	{
		FBlueprintHelperMaterialGraphExecutionInput AppendInput;
		AppendInput.Payload = FBlueprintHelperMaterialGraphAutomationTestUtils::MakePayload(
			Material,
			TEXT("append_new_owned_graph"),
			FBlueprintHelperMaterialGraphAutomationTestUtils::MakeAppendOps(
				BlockId,
				TEXT("owned_roughness"),
				TEXT("owned_base_color")));
		AppendInput.bDryRun = false;
		TestTrue(TEXT("initial append succeeds"), Facade.Execute(AppendInput).bOk);
	}

	{
		TArray<TSharedPtr<FJsonValue>> ReplaceOps;
		ReplaceOps.Add(MakeShared<FJsonValueObject>(
			FBlueprintHelperMaterialGraphAutomationTestUtils::MakeSpawnScalarOp(
				BlockId,
				TEXT("owned_roughness_replaced"),
				TEXT("BH_Replaced_Roughness"),
				0.5)));
		ReplaceOps.Add(MakeShared<FJsonValueObject>(
			FBlueprintHelperMaterialGraphAutomationTestUtils::MakeConnectOutputOp(
				TEXT("owned_roughness_replaced"),
				TEXT("Value"),
				TEXT("Roughness"))));

		FBlueprintHelperMaterialGraphExecutionInput ReplaceInput;
		ReplaceInput.Payload = FBlueprintHelperMaterialGraphAutomationTestUtils::MakePayload(
			Material,
			TEXT("replace_owned_graph"),
			ReplaceOps);
		ReplaceInput.bDryRun = false;
		const FBlueprintHelperToolResultBase ReplaceResult = Facade.Execute(ReplaceInput);
		TestTrue(TEXT("replace owned graph succeeds"), ReplaceResult.bOk);
		TestNull(TEXT("old owned roughness was deleted"),
			FBlueprintHelperMaterialGraphAutomationTestUtils::FindOwnedExpressionByNodeKey(Material, TEXT("owned_roughness")));
		TestNotNull(TEXT("replacement owned roughness exists"),
			FBlueprintHelperMaterialGraphAutomationTestUtils::FindOwnedExpressionByNodeKey(Material, TEXT("owned_roughness_replaced")));
	}

	{
		TArray<TSharedPtr<FJsonValue>> PatchOps;
		PatchOps.Add(MakeShared<FJsonValueObject>(
			FBlueprintHelperMaterialGraphAutomationTestUtils::MakeSetScalarDefaultOp(
				TEXT("owned_roughness_replaced"),
				0.77)));

		FBlueprintHelperMaterialGraphExecutionInput PatchInput;
		PatchInput.Payload = FBlueprintHelperMaterialGraphAutomationTestUtils::MakePayload(
			Material,
			TEXT("patch_owned_graph"),
			PatchOps);
		PatchInput.bDryRun = false;
		const FBlueprintHelperToolResultBase PatchResult = Facade.Execute(PatchInput);
		TestTrue(TEXT("patch owned graph succeeds"), PatchResult.bOk);
		const UMaterialExpressionScalarParameter* Patched =
			Cast<UMaterialExpressionScalarParameter>(
				FBlueprintHelperMaterialGraphAutomationTestUtils::FindOwnedExpressionByNodeKey(
					Material,
					TEXT("owned_roughness_replaced")));
		TestNotNull(TEXT("patched scalar exists"), Patched);
		TestEqual(TEXT("patch updates scalar default"), Patched ? Patched->DefaultValue : -1.0f, 0.77f);
	}

	{
		TArray<TSharedPtr<FJsonValue>> MergeOps;
		MergeOps.Add(MakeShared<FJsonValueObject>(
			FBlueprintHelperMaterialGraphAutomationTestUtils::MakeSpawnScalarOp(
				BlockId,
				TEXT("owned_metallic_merged"),
				TEXT("BH_Merged_Metallic"),
				0.1)));
		MergeOps.Add(MakeShared<FJsonValueObject>(
			FBlueprintHelperMaterialGraphAutomationTestUtils::MakeConnectOutputOp(
				TEXT("owned_metallic_merged"),
				TEXT("Value"),
				TEXT("Metallic"))));

		FBlueprintHelperMaterialGraphExecutionInput MergeInput;
		MergeInput.Payload = FBlueprintHelperMaterialGraphAutomationTestUtils::MakePayload(
			Material,
			TEXT("merge_owned_graph"),
			MergeOps);
		MergeInput.bDryRun = false;
		const FBlueprintHelperToolResultBase MergeResult = Facade.Execute(MergeInput);
		TestTrue(TEXT("merge owned graph succeeds"), MergeResult.bOk);
		TestNotNull(TEXT("merged owned metallic exists"),
			FBlueprintHelperMaterialGraphAutomationTestUtils::FindOwnedExpressionByNodeKey(Material, TEXT("owned_metallic_merged")));
		TestEqual(
			TEXT("Metallic material output points at merged owned expression"),
			UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, MP_Metallic),
			FBlueprintHelperMaterialGraphAutomationTestUtils::FindOwnedExpressionByNodeKey(Material, TEXT("owned_metallic_merged")));
	}

	Material->GetOutermost()->SetDirtyFlag(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialGraphRejectRestoresDeletedExpressionAutomationTest,
	"BlueprintHelper.MaterialGraph.RejectRestoresDeletedExpression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialGraphRejectRestoresDeletedExpressionAutomationTest::RunTest(const FString& Parameters)
{
	UMaterial* Material = FBlueprintHelperMaterialGraphAutomationTestUtils::MakeMaterialFixture(TEXT("M_BH_RestoreDeleted"));
	TestNotNull(TEXT("Material fixture is created"), Material);
	if (!Material)
	{
		return false;
	}

	FBlueprintHelperMaterialGraphFacade Facade;
	const FString BlockId = TEXT("automation_restore_deleted_block");
	const FString NodeKey = TEXT("restore_deleted_roughness");
	{
		FBlueprintHelperMaterialGraphExecutionInput AppendInput;
		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(
			FBlueprintHelperMaterialGraphAutomationTestUtils::MakeSpawnScalarOp(
				BlockId,
				NodeKey,
				TEXT("BH_Restore_Deleted_Roughness"),
				0.33)));
		Ops.Add(MakeShared<FJsonValueObject>(
			FBlueprintHelperMaterialGraphAutomationTestUtils::MakeConnectOutputOp(
				NodeKey,
				TEXT("Value"),
				TEXT("Roughness"))));
		AppendInput.Payload = FBlueprintHelperMaterialGraphAutomationTestUtils::MakePayload(
			Material,
			TEXT("append_new_owned_graph"),
			Ops);
		AppendInput.bDryRun = false;
		const FBlueprintHelperToolResultBase AppendResult = Facade.Execute(AppendInput);
		TestTrue(TEXT("append fixture succeeds"), AppendResult.bOk);
		if (!AppendResult.bOk)
		{
			Material->GetOutermost()->SetDirtyFlag(false);
			return false;
		}
	}

	UMaterialExpression* OriginalExpression =
		FBlueprintHelperMaterialGraphAutomationTestUtils::FindOwnedExpressionByNodeKey(Material, NodeKey);
	TestNotNull(TEXT("fixture expression exists before delete"), OriginalExpression);
	if (!OriginalExpression)
	{
		Material->GetOutermost()->SetDirtyFlag(false);
		return false;
	}

	FString BeforeGuid = OriginalExpression->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphensLower);
	FBlueprintHelperMaterialGraphExecutionInput DeleteInput;
	TArray<TSharedPtr<FJsonValue>> DeleteOps;
	DeleteOps.Add(MakeShared<FJsonValueObject>(
		FBlueprintHelperMaterialGraphAutomationTestUtils::MakeDeleteOp(NodeKey)));
	DeleteInput.Payload = FBlueprintHelperMaterialGraphAutomationTestUtils::MakePayload(
		Material,
		TEXT("patch_owned_graph"),
		DeleteOps);
	DeleteInput.bDryRun = false;
	const FBlueprintHelperToolResultBase DeleteResult = Facade.Execute(DeleteInput);
	TestTrue(TEXT("delete owned expression succeeds"), DeleteResult.bOk);
	if (!DeleteResult.bOk || !DeleteResult.Data.IsValid())
	{
		Material->GetOutermost()->SetDirtyFlag(false);
		return false;
	}
	TestNull(
		TEXT("expression is deleted before restore"),
		FBlueprintHelperMaterialGraphAutomationTestUtils::FindOwnedExpressionByNodeKey(Material, NodeKey));

	const TArray<TSharedPtr<FJsonValue>>* DeletedRefs = nullptr;
	TestTrue(
		TEXT("delete result carries deleted expression refs"),
		DeleteResult.Data->TryGetArrayField(TEXT("deleted_expression_refs"), DeletedRefs) &&
			DeletedRefs &&
			DeletedRefs->Num() == 1);
	if (!DeletedRefs || DeletedRefs->Num() != 1)
	{
		Material->GetOutermost()->SetDirtyFlag(false);
		return false;
	}
	const TSharedPtr<FJsonObject> DeletedSnapshot = (*DeletedRefs)[0].IsValid()
		? (*DeletedRefs)[0]->AsObject()
		: nullptr;
	TestTrue(TEXT("deleted expression snapshot is an object"), DeletedSnapshot.IsValid());
	if (!DeletedSnapshot.IsValid())
	{
		Material->GetOutermost()->SetDirtyFlag(false);
		return false;
	}

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Material->GetPathName();
	Target.Surface = EBlueprintHelperReviewSurface::Material;
	Target.GraphName = TEXT("MaterialGraph");
	Target.TargetKind = TEXT("material_expression");
	Target.TargetSubKind = TEXT("deleted_expression");
	Target.TargetKey = FString::Printf(TEXT("material_expression:%s"), *NodeKey);
	Target.BeforeSnapshotJson =
		FBlueprintHelperMaterialGraphAutomationTestUtils::SerializeJsonObject(DeletedSnapshot.ToSharedRef());

	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("material_graph_deleted_expression_review_action");
	Change.AssetPath = Material->GetPathName();
	Change.GraphName = TEXT("MaterialGraph");
	Change.DisplayLabel = TEXT("deleted material expression");
	Change.LatestEvidenceId = TEXT("material_graph_deleted_expression_evidence");
	Change.AtomicTargets.Add(Target);

	FBlueprintHelperReviewActionService ActionService;
	FBlueprintHelperReviewRejectOptions RejectOptions;
	const FBlueprintHelperReviewActionResult RejectResult =
		ActionService.RejectVisibleChange(Change, RejectOptions);
	TestTrue(
		TEXT("review reject recreates deleted material expression through default dispatcher"),
		RejectResult.bSucceeded);
	if (!RejectResult.bSucceeded)
	{
		AddError(FString::Printf(TEXT("review reject error: %s"), *RejectResult.Message));
	}
	TestEqual(
		TEXT("review reject reports rejected status"),
		RejectResult.NewStatus,
		EBlueprintHelperReviewChangeStatus::Rejected);

	UMaterialExpression* RestoredExpression =
		FBlueprintHelperMaterialGraphAutomationTestUtils::FindOwnedExpressionByNodeKey(Material, NodeKey);
	TestNotNull(TEXT("deleted expression is restored by node_key"), RestoredExpression);
	if (!RestoredExpression)
	{
		Material->GetOutermost()->SetDirtyFlag(false);
		return false;
	}
	TestEqual(
		TEXT("restored expression preserves guid"),
		RestoredExpression->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphensLower),
		BeforeGuid);
	const UMaterialExpressionScalarParameter* RestoredScalar =
		Cast<UMaterialExpressionScalarParameter>(RestoredExpression);
	TestNotNull(TEXT("restored expression preserves class"), RestoredScalar);
	if (RestoredScalar)
	{
		TestEqual(TEXT("restored scalar default value"), RestoredScalar->DefaultValue, 0.33f);
	}
	TestEqual(
		TEXT("Roughness output points at restored expression"),
		UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, MP_Roughness),
		RestoredExpression);

	Material->GetOutermost()->SetDirtyFlag(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialGraphCandidateSearchPathAutomationTest,
	"BlueprintHelper.MaterialGraph.CandidateSearchPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialGraphCandidateSearchPathAutomationTest::RunTest(const FString& Parameters)
{
	UMaterial* MaterialA = FBlueprintHelperMaterialGraphAutomationTestUtils::MakeMaterialFixture(TEXT("M_BH_CandidatesA"));
	UMaterial* MaterialB = FBlueprintHelperMaterialGraphAutomationTestUtils::MakeMaterialFixture(TEXT("M_BH_CandidatesB"));
	TestNotNull(TEXT("Material A fixture is created"), MaterialA);
	TestNotNull(TEXT("Material B fixture is created"), MaterialB);
	if (!MaterialA || !MaterialB)
	{
		return false;
	}

	FBlueprintHelperMaterialGraphFacade Facade;
	{
		TArray<TSharedPtr<FJsonValue>> QueryOps;
		QueryOps.Add(MakeShared<FJsonValueObject>(
			FBlueprintHelperMaterialGraphAutomationTestUtils::MakeResolveQueryOp(TEXT("scalar parameter"))));

		FBlueprintHelperMaterialGraphExecutionInput QueryInput;
		QueryInput.Payload = FBlueprintHelperMaterialGraphAutomationTestUtils::MakePayload(
			MaterialA,
			TEXT("append_new_owned_graph"),
			QueryOps);
		QueryInput.bDryRun = true;

		const FBlueprintHelperToolResultBase QueryResult = Facade.Execute(QueryInput);
		TestFalse(TEXT("query preview blocks for candidate confirmation"), QueryResult.bOk);
		TestTrue(TEXT("query preview has structured error"), QueryResult.Error.IsSet());
		if (QueryResult.Error.IsSet())
		{
			TestEqual(
				TEXT("query preview returns candidate confirmation code"),
				QueryResult.Error->Code,
				FString(TEXT("material_expression_candidate_confirmation_required")));
		}
		TestTrue(TEXT("query preview returns candidate data"), QueryResult.Data.IsValid());

		const TArray<TSharedPtr<FJsonValue>>* Candidates = nullptr;
		FString Fingerprint;
		FString SchemaFingerprint;
		FString ClassCatalogFingerprint;
		FString ClassCatalogRevision;
		TestTrue(TEXT("query preview has cache fingerprint"),
			QueryResult.Data.IsValid() && QueryResult.Data->TryGetStringField(TEXT("fingerprint"), Fingerprint) && !Fingerprint.IsEmpty());
		TestTrue(TEXT("query preview has schema fingerprint"),
			QueryResult.Data.IsValid() && QueryResult.Data->TryGetStringField(TEXT("schema_fingerprint"), SchemaFingerprint) && !SchemaFingerprint.IsEmpty());
		TestTrue(TEXT("query preview has class catalog fingerprint"),
			QueryResult.Data.IsValid() && QueryResult.Data->TryGetStringField(TEXT("class_catalog_fingerprint"), ClassCatalogFingerprint) && !ClassCatalogFingerprint.IsEmpty());
		TestTrue(TEXT("query preview has class catalog revision"),
			QueryResult.Data.IsValid() && QueryResult.Data->TryGetStringField(TEXT("class_catalog_revision"), ClassCatalogRevision) && !ClassCatalogRevision.IsEmpty());
		TestTrue(TEXT("query preview has candidates"),
			QueryResult.Data.IsValid() && QueryResult.Data->TryGetArrayField(TEXT("candidates"), Candidates) && Candidates && Candidates->Num() > 0);
		if (Candidates)
		{
			TestTrue(TEXT("candidate rows use compact contract"),
				FBlueprintHelperMaterialGraphAutomationTestUtils::CandidateRowsAreCompact(*Candidates));
			TestTrue(TEXT("query search includes executable scalar parameter class candidate"),
				FBlueprintHelperMaterialGraphAutomationTestUtils::HasCandidateClassContaining(*Candidates, TEXT("ScalarParameter")));
		}
	}

	FString ScalarCandidateId;
	{
		TArray<TSharedPtr<FJsonValue>> QueryOps;
		QueryOps.Add(MakeShared<FJsonValueObject>(
			FBlueprintHelperMaterialGraphAutomationTestUtils::MakeResolveQueryOp(TEXT("scalar parameter"))));

		FBlueprintHelperMaterialGraphExecutionInput QueryInput;
		QueryInput.Payload = FBlueprintHelperMaterialGraphAutomationTestUtils::MakePayload(
			MaterialA,
			TEXT("append_new_owned_graph"),
			QueryOps);
		QueryInput.bDryRun = true;

		const FBlueprintHelperToolResultBase QueryResult = Facade.Execute(QueryInput);
		const TArray<TSharedPtr<FJsonValue>>* Candidates = nullptr;
		TestTrue(TEXT("scalar query preview has candidates"),
			QueryResult.Data.IsValid() && QueryResult.Data->TryGetArrayField(TEXT("candidates"), Candidates) && Candidates && Candidates->Num() > 0);
		if (Candidates)
		{
			TestTrue(TEXT("scalar query returns P0 scalar candidate id"),
				FBlueprintHelperMaterialGraphAutomationTestUtils::FindCandidateIdForClass(
					*Candidates,
					UMaterialExpressionScalarParameter::StaticClass()->GetName(),
					ScalarCandidateId));
		}
		TestFalse(TEXT("scalar candidate id is captured"), ScalarCandidateId.IsEmpty());
	}

	{
		TArray<TSharedPtr<FJsonValue>> CrossAssetOps;
		CrossAssetOps.Add(MakeShared<FJsonValueObject>(
			FBlueprintHelperMaterialGraphAutomationTestUtils::MakeSpawnCandidateOp(
				TEXT("candidate_block"),
				TEXT("candidate_scalar"),
				ScalarCandidateId)));

		FBlueprintHelperMaterialGraphExecutionInput CrossAssetInput;
		CrossAssetInput.Payload = FBlueprintHelperMaterialGraphAutomationTestUtils::MakePayload(
			MaterialB,
			TEXT("append_new_owned_graph"),
			CrossAssetOps);
		CrossAssetInput.bDryRun = false;

		const FBlueprintHelperToolResultBase CrossAssetResult = Facade.Execute(CrossAssetInput);
		TestFalse(TEXT("cross-asset candidate execute is rejected"), CrossAssetResult.bOk);
		TestTrue(TEXT("cross-asset candidate has structured error"), CrossAssetResult.Error.IsSet());
		if (CrossAssetResult.Error.IsSet())
		{
			TestEqual(
				TEXT("cross-asset candidate returns expired code"),
				CrossAssetResult.Error->Code,
				FString(TEXT("material_expression_candidate_expired")));
		}
	}

	{
		TArray<TSharedPtr<FJsonValue>> SameAssetOps;
		SameAssetOps.Add(MakeShared<FJsonValueObject>(
			FBlueprintHelperMaterialGraphAutomationTestUtils::MakeSpawnCandidateOp(
				TEXT("candidate_block"),
				TEXT("candidate_scalar_success"),
				ScalarCandidateId)));
		SameAssetOps.Add(MakeShared<FJsonValueObject>(
			FBlueprintHelperMaterialGraphAutomationTestUtils::MakeConnectOutputOp(
				TEXT("candidate_scalar_success"),
				TEXT("Value"),
				TEXT("Specular"))));

		FBlueprintHelperMaterialGraphExecutionInput SameAssetInput;
		SameAssetInput.Payload = FBlueprintHelperMaterialGraphAutomationTestUtils::MakePayload(
			MaterialA,
			TEXT("append_new_owned_graph"),
			SameAssetOps);
		SameAssetInput.bDryRun = false;

		const FBlueprintHelperToolResultBase SameAssetResult = Facade.Execute(SameAssetInput);
		TestTrue(TEXT("same-asset candidate execute succeeds"), SameAssetResult.bOk);
		TestNotNull(TEXT("same-asset candidate creates owned expression"),
			FBlueprintHelperMaterialGraphAutomationTestUtils::FindOwnedExpressionByNodeKey(
				MaterialA,
				TEXT("candidate_scalar_success")));
		TestEqual(
			TEXT("same-asset candidate connects to Specular"),
			UMaterialEditingLibrary::GetMaterialPropertyInputNode(MaterialA, MP_Specular),
			FBlueprintHelperMaterialGraphAutomationTestUtils::FindOwnedExpressionByNodeKey(
				MaterialA,
				TEXT("candidate_scalar_success")));
	}

	{
		TArray<TSharedPtr<FJsonValue>> ExpiredOps;
		ExpiredOps.Add(MakeShared<FJsonValueObject>(
			FBlueprintHelperMaterialGraphAutomationTestUtils::MakeSpawnCandidateOp(
				TEXT("candidate_block"),
				TEXT("expired_candidate_scalar"),
				TEXT("mat_expr_deadbeef"))));

		FBlueprintHelperMaterialGraphExecutionInput ExpiredInput;
		ExpiredInput.Payload = FBlueprintHelperMaterialGraphAutomationTestUtils::MakePayload(
			MaterialA,
			TEXT("append_new_owned_graph"),
			ExpiredOps);
		ExpiredInput.bDryRun = false;

		const FBlueprintHelperToolResultBase ExpiredResult = Facade.Execute(ExpiredInput);
		TestFalse(TEXT("missing candidate execute is rejected as expired"), ExpiredResult.bOk);
		TestTrue(TEXT("missing candidate has structured error"), ExpiredResult.Error.IsSet());
		if (ExpiredResult.Error.IsSet())
		{
			TestEqual(
				TEXT("missing candidate returns expired code"),
				ExpiredResult.Error->Code,
				FString(TEXT("material_expression_candidate_expired")));
		}
	}

	MaterialA->GetOutermost()->SetDirtyFlag(false);
	MaterialB->GetOutermost()->SetDirtyFlag(false);
	return true;
}

#endif
