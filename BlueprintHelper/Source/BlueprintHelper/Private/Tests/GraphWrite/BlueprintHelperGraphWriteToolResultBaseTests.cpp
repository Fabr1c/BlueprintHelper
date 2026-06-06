#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataTable.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "K2Node_BreakStruct.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphSnapshotService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_Select.h"
#include "K2Node_Tunnel.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintTextConverter.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonPathService.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Systems/ToolClusters/AssetFactory/BlueprintHelperAssetFactoryService.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"
#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentService.h"
#include "Shared/Services/BlueprintHelperBlueprintStructureService.h"
#include "Systems/ToolClusters/BlueprintVariables/BlueprintHelperBlueprintVariableService.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "Systems/ToolClusters/DataTable/BlueprintHelperDataTableService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperAppendBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteServiceRegistry.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphWriteSemanticPayload.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGraphWriteCapabilityMetrics.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/Debug/BlueprintHelperAssetBrowseService.h"
#include "Systems/Debug/BlueprintHelperCompileAssetService.h"
#include "Systems/Debug/BlueprintHelperCompileService.h"
#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"
#include "UI/Review/BlueprintHelperReviewGraphBounds.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/Class.h"
#include "UObject/NoExportTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WidgetBlueprint.h"

class FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils
{
public:
	static FString MakeGraphWriteTestObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static UPackage* MakeGraphWriteTestPackage(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperGraphWrite/%s"),
			*MakeGraphWriteTestObjectName(Prefix)));
		Package->SetDirtyFlag(false);
		return Package;
	}

	static UBlueprint* MakeGraphWriteTestBlueprint(const FString& Prefix)
	{
		UPackage* Package = MakeGraphWriteTestPackage(Prefix);
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*MakeGraphWriteTestObjectName(TEXT("BP_GraphWriteToolResult")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperGraphWriteToolResultBaseTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static UEdGraph* AddGraphWriteFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName)
	{
		if (!Blueprint)
		{
			return nullptr;
		}

		UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(
			Blueprint,
			FName(*FunctionName),
			UEdGraph::StaticClass(),
			UEdGraphSchema_K2::StaticClass());
		if (!FunctionGraph)
		{
			return nullptr;
		}

		FBlueprintEditorUtils::AddFunctionGraph<UFunction>(
			Blueprint,
			FunctionGraph,
			true,
			nullptr);
		Blueprint->GetOutermost()->SetDirtyFlag(false);
		return FunctionGraph;
	}

	static UK2Node_FunctionEntry* FindGraphWriteFunctionEntry(UEdGraph* FunctionGraph)
	{
		if (!FunctionGraph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : FunctionGraph->Nodes)
		{
			if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
			{
				return Entry;
			}
		}
		return nullptr;
	}

	static FEdGraphPinType MakeGraphWriteTestPinType(const FName Category, const FName SubCategory = NAME_None)
	{
		return FEdGraphPinType(Category, SubCategory, nullptr, EPinContainerType::None, false, FEdGraphTerminalType());
	}

	static bool AddGraphWriteFunctionInputPin(
		UBlueprint* Blueprint,
		UEdGraph* FunctionGraph,
		const FString& PinName,
		const FEdGraphPinType& PinType)
	{
		UK2Node_FunctionEntry* Entry = FindGraphWriteFunctionEntry(FunctionGraph);
		if (!Blueprint || !Entry || PinName.IsEmpty())
		{
			return false;
		}

		TSharedPtr<FUserPinInfo> NewPin = MakeShared<FUserPinInfo>();
		NewPin->PinName = FName(*PinName);
		NewPin->PinType = PinType;
		NewPin->DesiredPinDirection = EGPD_Output;
		Entry->UserDefinedPins.Add(NewPin);
		Entry->ReconstructNode();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		return true;
	}

	static UK2Node_FunctionResult* FindGraphWriteFunctionResult(UEdGraph* FunctionGraph)
	{
		if (!FunctionGraph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : FunctionGraph->Nodes)
		{
			if (UK2Node_FunctionResult* Result = Cast<UK2Node_FunctionResult>(Node))
			{
				return Result;
			}
		}
		return nullptr;
	}

	static bool AddGraphWriteFunctionOutputPin(
		UBlueprint* Blueprint,
		UEdGraph* FunctionGraph,
		const FString& PinName,
		const FEdGraphPinType& PinType)
	{
		if (!Blueprint || !FunctionGraph || PinName.IsEmpty())
		{
			return false;
		}

		UK2Node_FunctionResult* ResultNode = FindGraphWriteFunctionResult(FunctionGraph);
		if (!ResultNode)
		{
			FGraphNodeCreator<UK2Node_FunctionResult> NodeCreator(*FunctionGraph);
			ResultNode = NodeCreator.CreateNode(true);
			ResultNode->NodePosX = 600;
			ResultNode->NodePosY = 0;
			NodeCreator.Finalize();
		}
		if (!ResultNode)
		{
			return false;
		}

		TSharedPtr<FUserPinInfo> NewPin = MakeShared<FUserPinInfo>();
		NewPin->PinName = FName(*PinName);
		NewPin->PinType = PinType;
		NewPin->DesiredPinDirection = EGPD_Input;
		ResultNode->UserDefinedPins.Add(NewPin);
		ResultNode->ReconstructNode();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		return true;
	}

	static TSharedRef<FJsonObject> MakeStringLiteralExpression(const FString& Value)
	{
		TSharedRef<FJsonObject> Literal = MakeShared<FJsonObject>();
		Literal->SetStringField(TEXT("kind"), TEXT("literal"));
		Literal->SetStringField(TEXT("value_type"), TEXT("string"));
		Literal->SetStringField(TEXT("value"), Value);
		return Literal;
	}

	static TSharedRef<FJsonObject> MakeBoolLiteralExpression(const bool bValue)
	{
		TSharedRef<FJsonObject> Literal = MakeShared<FJsonObject>();
		Literal->SetStringField(TEXT("kind"), TEXT("literal"));
		Literal->SetStringField(TEXT("value_type"), TEXT("bool"));
		Literal->SetBoolField(TEXT("value"), bValue);
		return Literal;
	}

	static TSharedRef<FJsonObject> MakeGenericGetExpression(const FString& Target)
	{
		TSharedRef<FJsonObject> Expression = MakeShared<FJsonObject>();
		Expression->SetStringField(TEXT("kind"), TEXT("get"));
		Expression->SetStringField(TEXT("target"), Target);
		return Expression;
	}

	static TSharedRef<FJsonObject> MakeComponentRefExpression(
		const FString& ComponentName,
		const UClass* ComponentClass,
		const FString& TypeOverride = FString(),
		const FString& PinObjectPathOverride = FString())
	{
		TSharedRef<FJsonObject> Expression = MakeShared<FJsonObject>();
		Expression->SetStringField(TEXT("kind"), TEXT("field"));
		Expression->SetStringField(TEXT("field_operation"), TEXT("get"));
		Expression->SetStringField(TEXT("field_scope"), TEXT("component_ref"));
		Expression->SetStringField(TEXT("target"), ComponentName);
		Expression->SetStringField(
			TEXT("type"),
			!TypeOverride.IsEmpty() ? TypeOverride : (ComponentClass ? ComponentClass->GetPathName() : FString()));
		if (!PinObjectPathOverride.IsEmpty())
		{
			Expression->SetStringField(
				TEXT("pin_type"),
				FString::Printf(TEXT("category=object|object=%s"), *PinObjectPathOverride));
		}

		TSharedRef<FJsonObject> ContextEvidence = MakeShared<FJsonObject>();
		ContextEvidence->SetStringField(TEXT("component_name"), ComponentName);
		Expression->SetObjectField(TEXT("context_evidence"), ContextEvidence);
		return Expression;
	}

	static TSharedRef<FJsonObject> MakeFunctionParamGetExpression(
		const FString& ParamName,
		const FString& FunctionName,
		const FString& PinType = TEXT("category=string"))
	{
		TSharedRef<FJsonObject> Expression = MakeShared<FJsonObject>();
		Expression->SetStringField(TEXT("kind"), TEXT("field"));
		Expression->SetStringField(TEXT("capability_id"), TEXT("field.function_param_get"));
		Expression->SetStringField(TEXT("field_operation"), TEXT("get"));
		Expression->SetStringField(TEXT("field_scope"), TEXT("variable"));
		Expression->SetStringField(TEXT("target"), ParamName);
		Expression->SetStringField(TEXT("function_name"), FunctionName);
		Expression->SetStringField(TEXT("pin_type"), PinType);

		TSharedRef<FJsonObject> CapabilityFacts = MakeShared<FJsonObject>();
		CapabilityFacts->SetStringField(TEXT("field.member_name"), ParamName);
		CapabilityFacts->SetStringField(TEXT("field.function_name"), FunctionName);
		Expression->SetObjectField(TEXT("capability_facts"), CapabilityFacts);
		return Expression;
	}

	static TSharedRef<FJsonObject> MakeCallStatement(const FString& FunctionName, const FString& Message = TEXT("graph write test"))
	{
		TSharedRef<FJsonObject> Statement = MakeShared<FJsonObject>();
		Statement->SetStringField(TEXT("kind"), TEXT("call"));
		Statement->SetStringField(TEXT("target"), FunctionName);

		TSharedRef<FJsonObject> Args = MakeShared<FJsonObject>();
		Args->SetObjectField(TEXT("InString"), MakeStringLiteralExpression(Message));
		Statement->SetObjectField(TEXT("args"), Args);
		return Statement;
	}

	static TSharedRef<FJsonObject> MakeCallNameStatement(const FString& FunctionName)
	{
		TSharedRef<FJsonObject> Statement = MakeShared<FJsonObject>();
		Statement->SetStringField(TEXT("kind"), TEXT("call"));
		Statement->SetStringField(TEXT("target"), FunctionName);
		return Statement;
	}

	static TSharedRef<FJsonObject> MakeAutoSearchCallNameStatement(const FString& FunctionName)
	{
		TSharedRef<FJsonObject> Statement = MakeCallNameStatement(FunctionName);
		Statement->SetStringField(TEXT("resolution_policy"), TEXT("auto_search"));
		return Statement;
	}

	static TSharedRef<FJsonObject> MakeGraphWriteLogicSpec(
		const FString& EventName,
		const TArray<TSharedPtr<FJsonValue>>& Statements)
	{
		TSharedRef<FJsonObject> LogicSpec = MakeShared<FJsonObject>();
		LogicSpec->SetStringField(TEXT("schema"), TEXT("BlueprintLogicSpec.v2"));
		if (!EventName.IsEmpty())
		{
			TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("id"), EventName + TEXT("_entry"));
			Entry->SetStringField(TEXT("kind"), TEXT("custom_event"));
			Entry->SetStringField(TEXT("name"), EventName);
			Entry->SetStringField(TEXT("source_cluster"), TEXT("blueprint_signature"));
			Entry->SetStringField(TEXT("signature_evidence_id"), EventName + TEXT("_signature_evidence"));
			LogicSpec->SetObjectField(TEXT("entry"), Entry);
		}
		LogicSpec->SetArrayField(TEXT("statements"), Statements);
		return LogicSpec;
	}

	static TSharedRef<FJsonObject> MakeEntryOnlyLogicSpec(const FString& EventName)
	{
		TArray<TSharedPtr<FJsonValue>> Statements;
		return MakeGraphWriteLogicSpec(EventName, Statements);
	}

	static TSharedRef<FJsonObject> MakeCallLogicSpec(
		const FString& EventName,
		const FString& FunctionName,
		const FString& Message = TEXT("graph write test"))
	{
		TArray<TSharedPtr<FJsonValue>> Statements;
		Statements.Add(MakeShared<FJsonValueObject>(MakeCallStatement(FunctionName, Message)));
		return MakeGraphWriteLogicSpec(EventName, Statements);
	}

	static TSharedRef<FJsonObject> MakeCallNameLogicSpec(
		const FString& EventName,
		const FString& FunctionName)
	{
		TArray<TSharedPtr<FJsonValue>> Statements;
		Statements.Add(MakeShared<FJsonValueObject>(MakeCallNameStatement(FunctionName)));
		return MakeGraphWriteLogicSpec(EventName, Statements);
	}

	static TSharedRef<FJsonObject> MakePrintFunctionParamLogicSpec(const FString& ParamName, const FString& FunctionName)
	{
		TSharedRef<FJsonObject> Statement = MakeShared<FJsonObject>();
		Statement->SetStringField(TEXT("id"), TEXT("stmt_print_function_param"));
		Statement->SetStringField(TEXT("kind"), TEXT("call"));
		Statement->SetStringField(TEXT("target"), TEXT("PrintString"));

		TSharedRef<FJsonObject> Args = MakeShared<FJsonObject>();
		Args->SetObjectField(TEXT("InString"), MakeFunctionParamGetExpression(ParamName, FunctionName));
		Statement->SetObjectField(TEXT("args"), Args);

		TArray<TSharedPtr<FJsonValue>> Statements;
		Statements.Add(MakeShared<FJsonValueObject>(Statement));
		return MakeGraphWriteLogicSpec(FString(), Statements);
	}

	static TSharedRef<FJsonObject> MakeSetMemberVariableStatement(
		const FString& VariableName,
		const TSharedRef<FJsonObject>& Value)
	{
		TSharedRef<FJsonObject> Statement = MakeShared<FJsonObject>();
		Statement->SetStringField(TEXT("id"), FString::Printf(TEXT("stmt_set_%s"), *VariableName));
		Statement->SetStringField(TEXT("kind"), TEXT("field"));
		Statement->SetStringField(TEXT("target"), VariableName);
		Statement->SetStringField(TEXT("field_operation"), TEXT("set"));
		Statement->SetStringField(TEXT("field_scope"), TEXT("variable"));
		Statement->SetObjectField(TEXT("value"), Value);
		return Statement;
	}

	static TSharedRef<FJsonObject> MakeReturnValueStatement(
		const TSharedRef<FJsonObject>& Value)
	{
		TSharedRef<FJsonObject> Statement = MakeShared<FJsonObject>();
		Statement->SetStringField(TEXT("id"), TEXT("stmt_return_value"));
		Statement->SetStringField(TEXT("kind"), TEXT("return"));
		Statement->SetObjectField(TEXT("value"), Value);
		return Statement;
	}

	static TSharedRef<FJsonObject> MakeSetThenReturnVariableLogicSpec(
		const FString& VariableName,
		const FString& LiteralValue)
	{
		TArray<TSharedPtr<FJsonValue>> Statements;
		Statements.Add(MakeShared<FJsonValueObject>(
			MakeSetMemberVariableStatement(VariableName, MakeStringLiteralExpression(LiteralValue))));
		Statements.Add(MakeShared<FJsonValueObject>(
			MakeReturnValueStatement(MakeGenericGetExpression(VariableName))));
		return MakeGraphWriteLogicSpec(FString(), Statements);
	}

	static TSharedRef<FJsonObject> MakePrintGenericGetLogicSpec(const FString& Target)
	{
		TSharedRef<FJsonObject> Statement = MakeShared<FJsonObject>();
		Statement->SetStringField(TEXT("id"), TEXT("stmt_print_generic_get"));
		Statement->SetStringField(TEXT("kind"), TEXT("call"));
		Statement->SetStringField(TEXT("target"), TEXT("PrintString"));

		TSharedRef<FJsonObject> Args = MakeShared<FJsonObject>();
		Args->SetObjectField(TEXT("InString"), MakeGenericGetExpression(Target));
		Statement->SetObjectField(TEXT("args"), Args);

		TArray<TSharedPtr<FJsonValue>> Statements;
		Statements.Add(MakeShared<FJsonValueObject>(Statement));
		return MakeGraphWriteLogicSpec(FString(), Statements);
	}

	static FString MakeRawReplaceGraphWritePayload(
		const FString& AssetPath,
		const FString& GraphName,
		const TSharedPtr<FJsonObject>& LogicSpec)
	{
		FBlueprintHelperGraphWriteSemanticPayload Payload;
		Payload.TargetAssetPath = AssetPath;
		Payload.TargetGraph = GraphName;
		Payload.Mode = TEXT("replace");
		Payload.bCompile = false;
		Payload.bSave = false;
		Payload.bStrict = true;
		Payload.bDryRun = false;
		Payload.bCreateMissingVariables = false;
		Payload.bReconstructExistingNodes = false;
		Payload.LogicSpec = LogicSpec;
		return Payload.ToJsonString();
	}

	static bool GenerateResultHasConnectivityCode(
		const FBlueprintGenerateResult& Result,
		const FString& Code)
	{
		for (const FBlueprintGeneratorDiagnostic& Diagnostic : Result.ConnectivityDiagnostics)
		{
			if (Diagnostic.Code.Equals(Code, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	static TSharedRef<FJsonObject> MakeUnconsumedPureDataLogicSpec(const FString& EventName)
	{
		TSharedRef<FJsonObject> Statement = MakeShared<FJsonObject>();
		Statement->SetStringField(TEXT("id"), TEXT("stmt_unconsumed_bool"));
		Statement->SetStringField(TEXT("kind"), TEXT("call"));
		Statement->SetStringField(TEXT("target"), TEXT("/Script/Engine.KismetMathLibrary:InRange_IntInt"));
		Statement->SetStringField(TEXT("value_type"), TEXT("bool"));
		Statement->SetStringField(TEXT("result_symbol"), TEXT("UnusedBool"));

		TSharedRef<FJsonObject> Args = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> Value = MakeShared<FJsonObject>();
		Value->SetStringField(TEXT("kind"), TEXT("literal"));
		Value->SetStringField(TEXT("value_type"), TEXT("int"));
		Value->SetNumberField(TEXT("value"), 1);
		Args->SetObjectField(TEXT("Value"), Value);

		TSharedRef<FJsonObject> Min = MakeShared<FJsonObject>();
		Min->SetStringField(TEXT("kind"), TEXT("literal"));
		Min->SetStringField(TEXT("value_type"), TEXT("int"));
		Min->SetNumberField(TEXT("value"), 0);
		Args->SetObjectField(TEXT("Min"), Min);

		TSharedRef<FJsonObject> Max = MakeShared<FJsonObject>();
		Max->SetStringField(TEXT("kind"), TEXT("literal"));
		Max->SetStringField(TEXT("value_type"), TEXT("int"));
		Max->SetNumberField(TEXT("value"), 2);
		Args->SetObjectField(TEXT("Max"), Max);

		Args->SetObjectField(TEXT("InclusiveMin"), MakeBoolLiteralExpression(true));
		Args->SetObjectField(TEXT("InclusiveMax"), MakeBoolLiteralExpression(true));
		Statement->SetObjectField(TEXT("args"), Args);

		TArray<TSharedPtr<FJsonValue>> Statements;
		Statements.Add(MakeShared<FJsonValueObject>(Statement));
		return MakeGraphWriteLogicSpec(EventName, Statements);
	}

	static TSharedRef<FJsonObject> MakeSetSimulatePhysicsTargetObjectLogicSpec(
		const FString& TargetObjectTypeOverride = FString(),
		const FString& TargetObjectPinObjectPathOverride = FString())
	{
		TSharedRef<FJsonObject> Statement = MakeShared<FJsonObject>();
		Statement->SetStringField(TEXT("kind"), TEXT("call"));
		Statement->SetStringField(TEXT("target"), TEXT("SetSimulatePhysics"));
		Statement->SetObjectField(
			TEXT("target_object"),
			MakeComponentRefExpression(
				TEXT("DoorMesh"),
				UStaticMeshComponent::StaticClass(),
				TargetObjectTypeOverride,
				TargetObjectPinObjectPathOverride));

		TSharedRef<FJsonObject> Args = MakeShared<FJsonObject>();
		Args->SetObjectField(TEXT("bSimulate"), MakeBoolLiteralExpression(true));
		Statement->SetObjectField(TEXT("args"), Args);

		TArray<TSharedPtr<FJsonValue>> Statements;
		Statements.Add(MakeShared<FJsonValueObject>(Statement));
		return MakeGraphWriteLogicSpec(FString(), Statements);
	}

	static TSharedRef<FJsonObject> MakeAppendPreviewPayload(const FString& AssetPath, const FString& GraphName)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Target->SetStringField(TEXT("graph"), GraphName);
		Payload->SetObjectField(TEXT("target"), Target);
		Payload->SetBoolField(TEXT("dry_run"), true);
		Payload->SetObjectField(
			TEXT("logic_spec"),
			MakeCallLogicSpec(FString(), TEXT("PrintString"), TEXT("append body")));
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeAppendExecutePayload(const FString& AssetPath, const FString& GraphName)
	{
		TSharedRef<FJsonObject> Payload = MakeAppendPreviewPayload(AssetPath, GraphName);
		Payload->SetBoolField(TEXT("dry_run"), false);
		Payload->SetStringField(TEXT("feature_name"), TEXT("SmokeFeature"));
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeAppendReuseExistingEntryExecutePayload(const FString& AssetPath, const FString& GraphName)
	{
		TSharedRef<FJsonObject> Payload = MakeAppendExecutePayload(AssetPath, GraphName);
		Payload->SetBoolField(TEXT("reuse_existing_entries"), true);
		Payload->SetObjectField(
			TEXT("logic_spec"),
			MakeCallLogicSpec(TEXT("SmokeCustomEvent"), TEXT("PrintString"), TEXT("append reuse")));
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeAppendUnconsumedPureDataExecutePayload(const FString& AssetPath, const FString& GraphName)
	{
		TSharedRef<FJsonObject> Payload = MakeAppendExecutePayload(AssetPath, GraphName);
		Payload->SetBoolField(TEXT("reuse_existing_entries"), true);
		Payload->SetObjectField(
			TEXT("logic_spec"),
			MakeUnconsumedPureDataLogicSpec(TEXT("SmokeCustomEvent")));
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeAppendUnconsumedPureDataPreviewPayload(const FString& AssetPath, const FString& GraphName)
	{
		TSharedRef<FJsonObject> Payload = MakeAppendUnconsumedPureDataExecutePayload(AssetPath, GraphName);
		Payload->SetBoolField(TEXT("dry_run"), true);
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeReplacementNode()
	{
		TSharedRef<FJsonObject> Node = MakeShared<FJsonObject>();
		Node->SetStringField(TEXT("id"), TEXT("replacement_01"));
		Node->SetStringField(TEXT("kind"), TEXT("call"));
		Node->SetStringField(TEXT("function"), TEXT("PrintString"));
		return Node;
	}

	static TSharedRef<FJsonObject> MakeReplacePreviewPayload(const FString& AssetPath, const FString& GraphName)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();

		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Target->SetStringField(TEXT("graph"), GraphName);
		Target->SetStringField(TEXT("replace_scope"), TEXT("custom_event_body"));
		Payload->SetObjectField(TEXT("target"), Target);

		TSharedRef<FJsonObject> Selector = MakeShared<FJsonObject>();
		Selector->SetStringField(TEXT("entry_name"), TEXT("SmokeCustomEvent"));
		Payload->SetObjectField(TEXT("selector"), Selector);

		Payload->SetObjectField(
			TEXT("logic_spec"),
			MakeCallLogicSpec(FString(), TEXT("PrintString"), TEXT("replace body")));

		TSharedRef<FJsonObject> Options = MakeShared<FJsonObject>();
		Options->SetBoolField(TEXT("dry_run"), true);
		Payload->SetObjectField(TEXT("options"), Options);
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeReplaceGraphScopeEntrySelectorPreviewPayload(const FString& AssetPath, const FString& GraphName)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();

		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Target->SetStringField(TEXT("graph"), GraphName);
		Target->SetStringField(TEXT("replace_scope"), TEXT("graph"));
		Payload->SetObjectField(TEXT("target"), Target);

		TSharedRef<FJsonObject> Selector = MakeShared<FJsonObject>();
		Selector->SetStringField(TEXT("entry_name"), TEXT("SmokeCustomEvent"));
		Payload->SetObjectField(TEXT("selector"), Selector);

		Payload->SetObjectField(
			TEXT("logic_spec"),
			MakeCallLogicSpec(FString(), TEXT("PrintString"), TEXT("replace graph with invalid entry selector")));

		TSharedRef<FJsonObject> Options = MakeShared<FJsonObject>();
		Options->SetBoolField(TEXT("dry_run"), true);
		Payload->SetObjectField(TEXT("options"), Options);
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeReplaceExecutePayload(const FString& AssetPath, const FString& GraphName)
	{
		TSharedRef<FJsonObject> Payload = MakeReplacePreviewPayload(AssetPath, GraphName);

		const TSharedPtr<FJsonObject>* Options = nullptr;
		if (Payload->TryGetObjectField(TEXT("options"), Options) && Options && Options->IsValid())
		{
			(*Options)->SetBoolField(TEXT("dry_run"), false);
		}
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeReplaceCustomEventExecutePayload(
		const FString& AssetPath,
		const FString& GraphName,
		const FString& EventName)
	{
		TSharedRef<FJsonObject> Payload = MakeReplaceExecutePayload(AssetPath, GraphName);

		const TSharedPtr<FJsonObject>* Selector = nullptr;
		if (Payload->TryGetObjectField(TEXT("selector"), Selector) && Selector && Selector->IsValid())
		{
			(*Selector)->SetStringField(TEXT("entry_name"), EventName);
		}
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeReplaceFunctionBodyExecutePayload(
		const FString& AssetPath,
		const FString& FunctionName)
	{
		TSharedRef<FJsonObject> Payload = MakeReplaceExecutePayload(AssetPath, FunctionName);

		const TSharedPtr<FJsonObject>* Target = nullptr;
		if (Payload->TryGetObjectField(TEXT("target"), Target) && Target && Target->IsValid())
		{
			(*Target)->SetStringField(TEXT("graph"), FunctionName);
			(*Target)->SetStringField(TEXT("replace_scope"), TEXT("function_body"));
		}

		const TSharedPtr<FJsonObject>* Selector = nullptr;
		if (Payload->TryGetObjectField(TEXT("selector"), Selector) && Selector && Selector->IsValid())
		{
			(*Selector)->RemoveField(TEXT("entry_name"));
			(*Selector)->SetStringField(TEXT("function_name"), FunctionName);
		}
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeReplaceMacroBodyExecutePayload(
		const FString& AssetPath,
		const FString& MacroName)
	{
		TSharedRef<FJsonObject> Payload = MakeReplaceExecutePayload(AssetPath, MacroName);

		const TSharedPtr<FJsonObject>* Target = nullptr;
		if (Payload->TryGetObjectField(TEXT("target"), Target) && Target && Target->IsValid())
		{
			(*Target)->SetStringField(TEXT("graph"), MacroName);
			(*Target)->SetStringField(TEXT("replace_scope"), TEXT("macro_body"));
		}

		const TSharedPtr<FJsonObject>* Selector = nullptr;
		if (Payload->TryGetObjectField(TEXT("selector"), Selector) && Selector && Selector->IsValid())
		{
			(*Selector)->SetStringField(TEXT("entry_name"), MacroName);
		}
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeReplaceFunctionBodyUnconsumedPureDataExecutePayload(
		const FString& AssetPath,
		const FString& FunctionName)
	{
		TSharedRef<FJsonObject> Payload = MakeReplaceFunctionBodyExecutePayload(AssetPath, FunctionName);
		Payload->SetObjectField(
			TEXT("logic_spec"),
			MakeUnconsumedPureDataLogicSpec(FString()));
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeReplaceUnconsumedPureDataExecutePayload(const FString& AssetPath, const FString& GraphName)
	{
		TSharedRef<FJsonObject> Payload = MakeReplaceExecutePayload(AssetPath, GraphName);
		Payload->SetObjectField(
			TEXT("logic_spec"),
			MakeUnconsumedPureDataLogicSpec(FString()));
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeReplaceCustomEventUnconsumedPureDataExecutePayload(
		const FString& AssetPath,
		const FString& GraphName,
		const FString& EventName)
	{
		TSharedRef<FJsonObject> Payload = MakeReplaceUnconsumedPureDataExecutePayload(AssetPath, GraphName);

		const TSharedPtr<FJsonObject>* Selector = nullptr;
		if (Payload->TryGetObjectField(TEXT("selector"), Selector) && Selector && Selector->IsValid())
		{
			(*Selector)->SetStringField(TEXT("entry_name"), EventName);
		}
		return Payload;
	}

	static UK2Node_CustomEvent* AddGraphWriteCustomEvent(UEdGraph* Graph, const FString& EventName)
	{
		if (!Graph)
		{
			return nullptr;
		}

		UK2Node_CustomEvent* EventNode = NewObject<UK2Node_CustomEvent>(Graph);
		Graph->AddNode(EventNode, true, false);
		EventNode->CreateNewGuid();
		EventNode->CustomFunctionName = FName(*EventName);
		EventNode->PostPlacedNewNode();
		EventNode->AllocateDefaultPins();
		return EventNode;
	}

	static UK2Node_CallFunction* AddGraphWritePrintStringCall(UEdGraph* Graph)
	{
		if (!Graph)
		{
			return nullptr;
		}

		UK2Node_CallFunction* PrintNode = NewObject<UK2Node_CallFunction>(Graph);
		Graph->AddNode(PrintNode, true, false);
		PrintNode->CreateNewGuid();
		PrintNode->FunctionReference.SetExternalMember(
			GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString),
			UKismetSystemLibrary::StaticClass());
		PrintNode->PostPlacedNewNode();
		PrintNode->AllocateDefaultPins();
		return PrintNode;
	}

	static UK2Node_IfThenElse* AddGraphWriteBranchNode(UEdGraph* Graph)
	{
		if (!Graph)
		{
			return nullptr;
		}

		UK2Node_IfThenElse* BranchNode = NewObject<UK2Node_IfThenElse>(Graph);
		Graph->AddNode(BranchNode, true, false);
		BranchNode->CreateNewGuid();
		BranchNode->PostPlacedNewNode();
		BranchNode->AllocateDefaultPins();
		return BranchNode;
	}

	static UK2Node_ExecutionSequence* AddGraphWriteSequenceNode(UEdGraph* Graph)
	{
		if (!Graph)
		{
			return nullptr;
		}

		UK2Node_ExecutionSequence* SequenceNode = NewObject<UK2Node_ExecutionSequence>(Graph);
		Graph->AddNode(SequenceNode, true, false);
		SequenceNode->CreateNewGuid();
		SequenceNode->PostPlacedNewNode();
		SequenceNode->AllocateDefaultPins();
		return SequenceNode;
	}

	static UK2Node_Select* AddGraphWriteSelectNode(UEdGraph* Graph)
	{
		if (!Graph)
		{
			return nullptr;
		}

		UK2Node_Select* SelectNode = NewObject<UK2Node_Select>(Graph);
		Graph->AddNode(SelectNode, true, false);
		SelectNode->CreateNewGuid();
		SelectNode->PostPlacedNewNode();
		SelectNode->AllocateDefaultPins();
		return SelectNode;
	}

	static UK2Node_VariableGet* AddGraphWriteVariableGetNode(UEdGraph* Graph, const FName VariableName)
	{
		if (!Graph)
		{
			return nullptr;
		}

		UK2Node_VariableGet* VariableNode = NewObject<UK2Node_VariableGet>(Graph);
		Graph->AddNode(VariableNode, true, false);
		VariableNode->CreateNewGuid();
		VariableNode->VariableReference.SetSelfMember(VariableName);
		VariableNode->PostPlacedNewNode();
		VariableNode->AllocateDefaultPins();
		return VariableNode;
	}

	static UK2Node_VariableSet* AddGraphWriteVariableSetNode(UEdGraph* Graph, const FName VariableName)
	{
		if (!Graph)
		{
			return nullptr;
		}

		UK2Node_VariableSet* VariableNode = NewObject<UK2Node_VariableSet>(Graph);
		Graph->AddNode(VariableNode, true, false);
		VariableNode->CreateNewGuid();
		VariableNode->VariableReference.SetSelfMember(VariableName);
		VariableNode->PostPlacedNewNode();
		VariableNode->AllocateDefaultPins();
		return VariableNode;
	}

	static UK2Node_MakeStruct* AddGraphWriteMakeStructNode(UEdGraph* Graph, UScriptStruct* StructType)
	{
		if (!Graph || !StructType)
		{
			return nullptr;
		}

		UK2Node_MakeStruct* StructNode = NewObject<UK2Node_MakeStruct>(Graph);
		Graph->AddNode(StructNode, true, false);
		StructNode->CreateNewGuid();
		StructNode->StructType = StructType;
		StructNode->PostPlacedNewNode();
		StructNode->AllocateDefaultPins();
		return StructNode;
	}

	static UK2Node_BreakStruct* AddGraphWriteBreakStructNode(UEdGraph* Graph, UScriptStruct* StructType)
	{
		if (!Graph || !StructType)
		{
			return nullptr;
		}

		UK2Node_BreakStruct* StructNode = NewObject<UK2Node_BreakStruct>(Graph);
		Graph->AddNode(StructNode, true, false);
		StructNode->CreateNewGuid();
		StructNode->StructType = StructType;
		StructNode->PostPlacedNewNode();
		StructNode->AllocateDefaultPins();
		return StructNode;
	}

	static UEdGraphPin* FindFirstExecPin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
	{
		if (!Node)
		{
			return nullptr;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	static bool ConnectFirstExecPins(UEdGraphNode* FromNode, UEdGraphNode* ToNode)
	{
		UEdGraphPin* FromPin = FindFirstExecPin(FromNode, EGPD_Output);
		UEdGraphPin* ToPin = FindFirstExecPin(ToNode, EGPD_Input);
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		return FromPin && ToPin && Schema && Schema->TryCreateConnection(FromPin, ToPin);
	}

	static TArray<UEdGraphPin*> FindExecPins(UEdGraphNode* Node, EEdGraphPinDirection Direction)
	{
		TArray<UEdGraphPin*> Result;
		if (!Node)
		{
			return Result;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				Result.Add(Pin);
			}
		}
		return Result;
	}

	static UK2Node_ExecutionSequence* FindGraphWriteSequenceNode(UEdGraph* Graph)
	{
		if (!Graph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_ExecutionSequence* SequenceNode = Cast<UK2Node_ExecutionSequence>(Node))
			{
				return SequenceNode;
			}
		}
		return nullptr;
	}

	static UK2Node_CallFunction* FindGraphWriteCallFunctionNode(UEdGraph* Graph, const FName FunctionName)
	{
		if (!Graph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
			if (CallNode && CallNode->GetFunctionName() == FunctionName)
			{
				return CallNode;
			}
		}
		return nullptr;
	}

	static bool IsExecReachable(UEdGraphNode* FromNode, UEdGraphNode* ToNode)
	{
		if (!FromNode || !ToNode)
		{
			return false;
		}

		TSet<UEdGraphNode*> Visited;
		TArray<UEdGraphNode*> Stack;
		Stack.Add(FromNode);

		while (Stack.Num() > 0)
		{
			UEdGraphNode* Current = FBlueprintHelperVersionCompat::PopNoShrink(Stack);
			if (!Current || Visited.Contains(Current))
			{
				continue;
			}
			if (Current == ToNode)
			{
				return true;
			}

			Visited.Add(Current);
			for (UEdGraphPin* Pin : Current->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
				{
					continue;
				}
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (LinkedPin && LinkedPin->GetOwningNode())
					{
						Stack.Add(LinkedPin->GetOwningNode());
					}
				}
			}
		}
		return false;
	}

	static bool GraphHasVariableGetLinkedToFunctionResult(
		FAutomationTestBase& Test,
		UEdGraph* Graph,
		const FName VariableName)
	{
		if (!Graph)
		{
			return false;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UK2Node_VariableGet* Getter = Cast<UK2Node_VariableGet>(Node);
			if (!Getter || Getter->GetVarName() != VariableName)
			{
				continue;
			}

			for (UEdGraphPin* Pin : Getter->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
				{
					continue;
				}

				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					UK2Node_FunctionResult* ResultNode = LinkedPin
						? Cast<UK2Node_FunctionResult>(LinkedPin->GetOwningNode())
						: nullptr;
					if (ResultNode)
					{
						return true;
					}
				}
			}
		}

		Test.AddInfo(FString::Printf(TEXT("No variable getter for %s is linked to a FunctionResult."), *VariableName.ToString()));
		return false;
	}

	static void MarkGraphWriteNodeAsBlueprintHelperOwned(UEdGraphNode* Node, const FString& BlockId)
	{
		if (!Node)
		{
			return;
		}

		if (UPackage* Package = Node->GetOutermost())
		{
			FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
			MetaData.SetValue(Node, TEXT("BlueprintHelperOwned"), TEXT("true"));
			MetaData.SetValue(Node, TEXT("BlueprintHelperBlockId"), *BlockId);
		}
	}

	static FString MakeGraphWriteEntryBlockId(UBlueprint* Blueprint, UEdGraph* Graph, const FString& EntryName)
	{
		FBlueprintHelperBlockIdService BlockIdService;
		const FString BlockRef = BlockIdService.MakeBlockRef(Blueprint, Graph, EntryName);
		return BlockIdService.MakeFullBlockId(Graph ? Graph->GetName() : FString(), BlockRef);
	}

	static bool NodeHasBlueprintHelperBlockId(UEdGraphNode* Node, const FString& BlockId)
	{
		if (!Node)
		{
			return false;
		}

		UPackage* Package = Node->GetOutermost();
		if (!Package)
		{
			return false;
		}

		FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
		return MetaData.GetValue(Node, TEXT("BlueprintHelperOwned")) == TEXT("true") &&
			MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId")) == BlockId;
	}

	static int32 CountNodesWithBlueprintHelperBlockId(UEdGraph* Graph, const FString& BlockId)
	{
		if (!Graph)
		{
			return 0;
		}

		int32 Count = 0;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (NodeHasBlueprintHelperBlockId(Node, BlockId))
			{
				++Count;
			}
		}
		return Count;
	}

	static void AssertNodeHasOwnershipMetadata(
		FAutomationTestBase& Test,
		UEdGraphNode* Node,
		const FString& BlockId,
		const FString& FeatureName)
	{
		Test.TestNotNull(TEXT("owned node exists"), Node);
		if (!Node)
		{
			return;
		}

		UPackage* Package = Node->GetOutermost();
		Test.TestNotNull(TEXT("node package exists"), Package);
		if (!Package)
		{
			return;
		}

		FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
		Test.TestTrue(TEXT("metadata marks node as BlueprintHelper owned"),
			MetaData.GetValue(Node, TEXT("BlueprintHelperOwned")) == FString(TEXT("true")));
		Test.TestTrue(TEXT("metadata keeps block id"),
			MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId")) == BlockId);
		Test.TestTrue(TEXT("metadata keeps feature name"),
			MetaData.GetValue(Node, TEXT("BlueprintHelperFeatureName")) == FeatureName);
		Test.TestTrue(TEXT("metadata omits legacy tool field"),
			MetaData.GetValue(Node, TEXT("BlueprintHelperTool")).IsEmpty());
	}

	static UEdGraph* FindUbergraphPageByName(UBlueprint* Blueprint, const FString& GraphName)
	{
		if (!Blueprint)
		{
			return nullptr;
		}

		for (UEdGraph* Page : Blueprint->UbergraphPages)
		{
			if (Page && Page->GetName() == GraphName)
			{
				return Page;
			}
		}
		return nullptr;
	}

	static int32 CountCustomEventsByName(UEdGraph* Graph, const FString& EventName)
	{
		if (!Graph)
		{
			return 0;
		}

		int32 Count = 0;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			const UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node);
			if (CustomEvent && CustomEvent->CustomFunctionName.ToString().Equals(EventName, ESearchCase::IgnoreCase))
			{
				++Count;
			}
		}
		return Count;
	}

	static bool ExportHasExecLinkFromCustomEventToFunction(
		UEdGraph* Graph,
		const FString& EventName,
		const FString& FunctionName)
	{
		const TSharedPtr<FJsonObject> ExportedGraph = FBlueprintToTextConverter::ConvertGraphToJsonObject(Graph);
		if (!ExportedGraph.IsValid())
		{
			return false;
		}

		FString EventNodeId;
		FString FunctionNodeId;
		const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
		if (ExportedGraph->TryGetArrayField(TEXT("nodes"), Nodes) && Nodes)
		{
			for (const TSharedPtr<FJsonValue>& NodeValue : *Nodes)
			{
				const TSharedPtr<FJsonObject>* NodeObject = nullptr;
				if (!NodeValue.IsValid() || !NodeValue->TryGetObject(NodeObject) || !NodeObject || !NodeObject->IsValid())
				{
					continue;
				}

				FString NodeId;
				(*NodeObject)->TryGetStringField(TEXT("id"), NodeId);

				const TSharedPtr<FJsonObject>* EventObject = nullptr;
				FString ExportedEventName;
				if ((*NodeObject)->TryGetObjectField(TEXT("event"), EventObject) &&
					EventObject && EventObject->IsValid() &&
					(*EventObject)->TryGetStringField(TEXT("event_name"), ExportedEventName) &&
					ExportedEventName.Equals(EventName, ESearchCase::IgnoreCase))
				{
					EventNodeId = NodeId;
				}

				FString ExportedFunctionName;
				if ((*NodeObject)->TryGetStringField(TEXT("function_name"), ExportedFunctionName) &&
					ExportedFunctionName.Equals(FunctionName, ESearchCase::IgnoreCase))
				{
					FunctionNodeId = NodeId;
				}
			}
		}

		if (EventNodeId.IsEmpty() || FunctionNodeId.IsEmpty())
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
		if (!ExportedGraph->TryGetArrayField(TEXT("links"), Links) || !Links)
		{
			return false;
		}

		for (const TSharedPtr<FJsonValue>& LinkValue : *Links)
		{
			const TSharedPtr<FJsonObject>* LinkObject = nullptr;
			if (!LinkValue.IsValid() || !LinkValue->TryGetObject(LinkObject) || !LinkObject || !LinkObject->IsValid())
			{
				continue;
			}

			FString Kind;
			FString FromId;
			FString ToId;
			(*LinkObject)->TryGetStringField(TEXT("kind"), Kind);
			(*LinkObject)->TryGetStringField(TEXT("from_id"), FromId);
			(*LinkObject)->TryGetStringField(TEXT("to_id"), ToId);
			if (Kind.Equals(TEXT("exec"), ESearchCase::IgnoreCase) &&
				FromId == EventNodeId &&
				ToId == FunctionNodeId)
			{
				return true;
			}
		}

		return false;
	}

	static FString DescribeGraphExecLinks(UEdGraph* Graph)
	{
		if (!Graph)
		{
			return TEXT("graph=null");
		}

		TArray<FString> Lines;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}

			FString NodeLabel = Node->GetName();
			if (const UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
			{
				NodeLabel += FString::Printf(TEXT(":event=%s"), *CustomEvent->CustomFunctionName.ToString());
			}
			if (const UK2Node_CallFunction* CallFunction = Cast<UK2Node_CallFunction>(Node))
			{
				NodeLabel += FString::Printf(TEXT(":function=%s"), *CallFunction->GetFunctionName().ToString());
			}

			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
				{
					continue;
				}
				if (Pin->LinkedTo.Num() == 0)
				{
					Lines.Add(FString::Printf(TEXT("%s.%s -> <none>"), *NodeLabel, *Pin->PinName.ToString()));
					continue;
				}
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
					Lines.Add(FString::Printf(
						TEXT("%s.%s -> %s.%s"),
						*NodeLabel,
						*Pin->PinName.ToString(),
						LinkedNode ? *LinkedNode->GetName() : TEXT("<null>"),
						LinkedPin ? *LinkedPin->PinName.ToString() : TEXT("<null>")));
				}
			}
		}
		return FString::Join(Lines, TEXT(" | "));
	}

	static bool GraphHasVariableGetLinkedToFunctionInput(
		FAutomationTestBase& Test,
		UEdGraph* Graph,
		const FName VariableName,
		const FString& FunctionName)
	{
		if (!Graph)
		{
			return false;
		}

		TArray<FString> Diagnostics;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (const UK2Node_VariableGet* VariableNode = Cast<UK2Node_VariableGet>(Node))
			{
				Diagnostics.Add(FString::Printf(
					TEXT("target_object graph variable get observed: node=%s variable=%s"),
					*VariableNode->GetName(),
					*VariableNode->VariableReference.GetMemberName().ToString()));
			}

			const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
			const UFunction* Function = CallNode ? CallNode->GetTargetFunction() : nullptr;
			if (!Function || !Function->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
			{
				if (Function)
				{
					Diagnostics.Add(FString::Printf(TEXT("target_object call node function observed: %s"), *Function->GetName()));
				}
				continue;
			}

			for (UEdGraphPin* Pin : CallNode->Pins)
			{
				if (Pin)
				{
					Diagnostics.Add(FString::Printf(
						TEXT("target_object call pin observed: pin=%s direction=%d category=%s object=%s linked=%d hidden=%d"),
						*Pin->PinName.ToString(),
						static_cast<int32>(Pin->Direction),
						*Pin->PinType.PinCategory.ToString(),
						Pin->PinType.PinSubCategoryObject.IsValid() ? *Pin->PinType.PinSubCategoryObject->GetPathName() : TEXT("<none>"),
						Pin->LinkedTo.Num(),
						Pin->bHidden ? 1 : 0));
				}
				if (!Pin || Pin->Direction != EGPD_Input || Pin->LinkedTo.Num() == 0)
				{
					continue;
				}

				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					const UK2Node_VariableGet* VariableGet = LinkedPin ? Cast<UK2Node_VariableGet>(LinkedPin->GetOwningNode()) : nullptr;
					Diagnostics.Add(FString::Printf(
						TEXT("target_object candidate link: pin=%s linked_node=%s linked_node_class=%s linked_pin=%s variable=%s"),
						*Pin->PinName.ToString(),
						LinkedPin && LinkedPin->GetOwningNode() ? *LinkedPin->GetOwningNode()->GetName() : TEXT("<none>"),
						LinkedPin && LinkedPin->GetOwningNode() ? *LinkedPin->GetOwningNode()->GetClass()->GetName() : TEXT("<none>"),
						LinkedPin ? *LinkedPin->PinName.ToString() : TEXT("<none>"),
						VariableGet ? *VariableGet->VariableReference.GetMemberName().ToString() : TEXT("<none>")));
					if (VariableGet && VariableGet->VariableReference.GetMemberName() == VariableName)
					{
						return true;
					}
				}
			}
		}

		for (const FString& Diagnostic : Diagnostics)
		{
			Test.AddInfo(Diagnostic);
		}
		return false;
	}

	static bool HasSCSComponentNamed(UBlueprint* Blueprint, const FString& ComponentName)
	{
		if (!Blueprint || !Blueprint->SimpleConstructionScript)
		{
			return false;
		}

		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->GetVariableName().ToString() == ComponentName)
			{
				return true;
			}
		}
		return false;
	}

	static bool HasMemberVariableNamed(UBlueprint* Blueprint, const FName VariableName)
	{
		if (!Blueprint)
		{
			return false;
		}

		for (const FBPVariableDescription& VariableDescription : Blueprint->NewVariables)
		{
			if (VariableDescription.VarName == VariableName)
			{
				return true;
			}
		}
		return false;
	}

		static TSharedRef<FJsonObject> MakePatchPreviewPayload(const FString& AssetPath, const FString& GraphName)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();

		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Target->SetStringField(TEXT("graph"), GraphName);
		Target->SetStringField(TEXT("patch_scope"), TEXT("pin_default"));
		Payload->SetObjectField(TEXT("target"), Target);

		Payload->SetStringField(TEXT("patch_type"), TEXT("set_pin_default"));
		Payload->SetBoolField(TEXT("dry_run"), true);

		TSharedRef<FJsonObject> PatchedRef = MakeShared<FJsonObject>();
		PatchedRef->SetStringField(TEXT("node_ref"), TEXT("Branch_0"));
		PatchedRef->SetStringField(TEXT("pin_ref"), TEXT("Condition"));
		Payload->SetObjectField(TEXT("patched_ref"), PatchedRef);

		TSharedRef<FJsonObject> Patch = MakeShared<FJsonObject>();
		Patch->SetBoolField(TEXT("value"), true);
		Payload->SetObjectField(TEXT("patch"), Patch);
		return Payload;
	}

	static TSharedRef<FJsonObject> MakePatchConnectPinsPayload(
		const FString& AssetPath,
		const FString& GraphName,
		const FString& SourceNodeRef,
		const FString& SourcePinRef,
		const FString& TargetNodeRef,
		const FString& TargetPinRef,
		bool bDryRun,
		const FString& SourceNodePath = FString(),
		const FString& SourcePinPath = FString(),
		const FString& TargetBlockId = FString(),
		const FString& SourceBlockId = FString())
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();

		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Target->SetStringField(TEXT("graph"), GraphName);
		Target->SetStringField(TEXT("patch_scope"), TEXT("connect_pins"));
		Payload->SetObjectField(TEXT("target"), Target);

		Payload->SetStringField(TEXT("patch_type"), TEXT("connect_pins"));
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		TSharedRef<FJsonObject> PatchedRef = MakeShared<FJsonObject>();
		if (!TargetBlockId.IsEmpty())
		{
			PatchedRef->SetStringField(TEXT("block_id"), TargetBlockId);
		}
		PatchedRef->SetStringField(TEXT("node_ref"), TargetNodeRef);
		PatchedRef->SetStringField(TEXT("pin_ref"), TargetPinRef);
		Payload->SetObjectField(TEXT("patched_ref"), PatchedRef);

		TSharedRef<FJsonObject> Patch = MakeShared<FJsonObject>();
		const FString EffectiveSourceBlockId = SourceBlockId.IsEmpty() ? TargetBlockId : SourceBlockId;
		if (!EffectiveSourceBlockId.IsEmpty())
		{
			Patch->SetStringField(TEXT("source_block_id"), EffectiveSourceBlockId);
		}
		Patch->SetStringField(TEXT("source_node_ref"), SourceNodeRef);
		Patch->SetStringField(TEXT("source_pin_ref"), SourcePinRef);
		if (!SourceNodePath.IsEmpty())
		{
			Patch->SetStringField(TEXT("source_node_path"), SourceNodePath);
		}
		if (!SourcePinPath.IsEmpty())
		{
			Patch->SetStringField(TEXT("source_pin_path"), SourcePinPath);
		}
		Payload->SetObjectField(TEXT("patch"), Patch);

		return Payload;
	}

	static TSharedRef<FJsonObject> MakePatchDisconnectLinkPayload(
		const FString& AssetPath,
		const FString& GraphName,
		const FString& SourceNodeRef,
		const FString& SourcePinRef,
		const FString& TargetNodeRef,
		const FString& TargetPinRef,
		const FString& BlockId,
		bool bDryRun,
		bool bWithExpectedOldState = false,
		const FString& ExpectedSourceNodeRef = FString(),
		const FString& ExpectedSourcePinRef = FString(),
		const FString& ExpectedTargetNodeRef = FString(),
		const FString& ExpectedTargetPinRef = FString())
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();

		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Target->SetStringField(TEXT("graph"), GraphName);
		Target->SetStringField(TEXT("patch_scope"), TEXT("disconnect_link"));
		Payload->SetObjectField(TEXT("target"), Target);

		Payload->SetStringField(TEXT("patch_type"), TEXT("disconnect_link"));
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		TSharedRef<FJsonObject> PatchedRef = MakeShared<FJsonObject>();
		PatchedRef->SetStringField(TEXT("block_id"), BlockId);
		PatchedRef->SetStringField(TEXT("node_ref"), SourceNodeRef);
		PatchedRef->SetStringField(TEXT("pin_ref"), SourcePinRef);
		PatchedRef->SetStringField(
			TEXT("link_ref"),
			FString::Printf(TEXT("%s.%s->%s.%s"), *SourceNodeRef, *SourcePinRef, *TargetNodeRef, *TargetPinRef));
		Payload->SetObjectField(TEXT("patched_ref"), PatchedRef);

		Payload->SetObjectField(TEXT("patch"), MakeShared<FJsonObject>());

		if (bWithExpectedOldState)
		{
			TSharedRef<FJsonObject> ExpectedOldState = MakeShared<FJsonObject>();
			ExpectedOldState->SetStringField(TEXT("source_node_ref"), ExpectedSourceNodeRef);
			ExpectedOldState->SetStringField(TEXT("source_pin_ref"), ExpectedSourcePinRef);
			ExpectedOldState->SetStringField(TEXT("target_node_ref"), ExpectedTargetNodeRef);
			ExpectedOldState->SetStringField(TEXT("target_pin_ref"), ExpectedTargetPinRef);
			Payload->SetObjectField(TEXT("expected_old_state"), ExpectedOldState);
		}

		return Payload;
	}

	static TSharedRef<FJsonObject> MakePatchReplaceLinkPayload(
		const FString& AssetPath,
		const FString& GraphName,
		const FString& SourceNodeRef,
		const FString& SourcePinRef,
		const FString& CurrentTargetNodeRef,
		const FString& CurrentTargetPinRef,
		const FString& ReplacementNodeRef,
		const FString& ReplacementPinRef,
		const FString& BlockId,
		bool bDryRun)
	{
		TSharedRef<FJsonObject> Payload = MakePatchDisconnectLinkPayload(
			AssetPath,
			GraphName,
			SourceNodeRef,
			SourcePinRef,
			CurrentTargetNodeRef,
			CurrentTargetPinRef,
			BlockId,
			bDryRun);

		const TSharedPtr<FJsonObject> Target = Payload->GetObjectField(TEXT("target"));
		if (Target.IsValid())
		{
			Target->SetStringField(TEXT("patch_scope"), TEXT("replace_link"));
		}
		Payload->SetStringField(TEXT("patch_type"), TEXT("replace_link"));

		TSharedRef<FJsonObject> Patch = MakeShared<FJsonObject>();
		Patch->SetStringField(TEXT("replacement_block_id"), BlockId);
		Patch->SetStringField(TEXT("replacement_node_ref"), ReplacementNodeRef);
		Patch->SetStringField(TEXT("replacement_pin_ref"), ReplacementPinRef);
		Payload->SetObjectField(TEXT("patch"), Patch);
		return Payload;
	}

	static TSharedRef<FJsonObject> MakePatchDeleteOwnedNodePayload(
		const FString& AssetPath,
		const FString& GraphName,
		const FString& NodeRef,
		const FString& BlockId,
		bool bDryRun,
		bool bBreakLinks = true,
		bool bAllowEntryNode = false,
		bool bAllowLifecycleRoot = false)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();

		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Target->SetStringField(TEXT("graph"), GraphName);
		Target->SetStringField(TEXT("patch_scope"), TEXT("node_delete"));
		Payload->SetObjectField(TEXT("target"), Target);

		Payload->SetStringField(TEXT("patch_type"), TEXT("delete_owned_node"));
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		TSharedRef<FJsonObject> PatchedRef = MakeShared<FJsonObject>();
		PatchedRef->SetStringField(TEXT("block_id"), BlockId);
		PatchedRef->SetStringField(TEXT("node_ref"), NodeRef);
		Payload->SetObjectField(TEXT("patched_ref"), PatchedRef);

		TSharedRef<FJsonObject> Patch = MakeShared<FJsonObject>();
		Patch->SetBoolField(TEXT("break_links"), bBreakLinks);
		Patch->SetBoolField(TEXT("allow_entry_node"), bAllowEntryNode);
		Patch->SetBoolField(TEXT("allow_lifecycle_root"), bAllowLifecycleRoot);
		Payload->SetObjectField(TEXT("patch"), Patch);

		return Payload;
	}

	static TSharedRef<FJsonObject> MakeMergePreviewPayload(const FString& AssetPath, const FString& GraphName)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();

		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Target->SetStringField(TEXT("graph"), GraphName);
		Target->SetStringField(TEXT("merge_scope"), TEXT("custom_event_call"));
		Target->SetStringField(TEXT("insert_strategy"), TEXT("append_after"));
		Payload->SetObjectField(TEXT("target"), Target);

		TSharedRef<FJsonObject> Anchor = MakeShared<FJsonObject>();
		Anchor->SetStringField(TEXT("node_ref"), TEXT("BeginPlay_0"));
		Anchor->SetStringField(TEXT("pin_ref"), TEXT("Then"));
		Payload->SetObjectField(TEXT("anchor"), Anchor);

		TSharedRef<FJsonObject> Inserted = MakeShared<FJsonObject>();
		Inserted->SetStringField(TEXT("custom_event"), TEXT("SmokeCustomEvent"));
		Payload->SetObjectField(TEXT("inserted"), Inserted);
		Payload->SetBoolField(TEXT("dry_run"), true);
		return Payload;
	}

	static void AssertBlockedDryRunFailure(
		FAutomationTestBase& Test,
		const FBlueprintHelperToolResultBase& Result,
		const FString& ExpectedOperation,
		const FString& ExpectedCode,
		const FString& ExpectedField)
	{
		Test.TestFalse(TEXT("blocked dry-run returns failure"), Result.bOk);
		Test.TestEqual(TEXT("blocked dry-run uses failed status"), Result.Status, EBlueprintHelperToolStatus::Failed);
		Test.TestEqual(TEXT("blocked dry-run operation is preserved"), Result.Operation, ExpectedOperation);
		Test.TestFalse(TEXT("blocked dry-run does not modify assets"), Result.bModified);
		Test.TestTrue(TEXT("blocked dry-run carries top-level error"), Result.Error.IsSet());
		if (Result.Error.IsSet())
		{
			Test.TestEqual(TEXT("error code is readable"), Result.Error->Code, ExpectedCode);
			Test.TestEqual(TEXT("error stage is preflight"), Result.Error->Stage, EBlueprintHelperToolStage::Preflight);
			Test.TestEqual(TEXT("error field is readable"), Result.Error->Field, ExpectedField);
			Test.TestFalse(TEXT("error message is not empty"), Result.Error->Message.IsEmpty());
		}

		Test.TestNotNull(TEXT("blocked dry-run still returns data"), Result.Data.Get());
		const TSharedPtr<FJsonObject>* DryRun = nullptr;
		Test.TestTrue(TEXT("data contains dry_run payload"),
			Result.Data.IsValid() && Result.Data->TryGetObjectField(TEXT("dry_run"), DryRun));
		if (DryRun && DryRun->IsValid())
		{
			FString DryRunResult;
			bool bCanExecute = true;
			Test.TestTrue(TEXT("dry_run.result exists"), (*DryRun)->TryGetStringField(TEXT("result"), DryRunResult));
			Test.TestTrue(TEXT("dry_run.can_execute exists"), (*DryRun)->TryGetBoolField(TEXT("can_execute"), bCanExecute));
			Test.TestEqual(TEXT("dry_run result is blocked"), DryRunResult, FString(TEXT("blocked")));
			Test.TestFalse(TEXT("blocked dry-run cannot execute"), bCanExecute);
		}
	}

	static void AssertConnectivityFailureData(
		FAutomationTestBase& Test,
		const FBlueprintHelperToolResultBase& Result,
		const FString& ExpectedOperation,
		const FString& ExpectedStageLabel)
	{
		Test.TestFalse(TEXT("connectivity failure returns failure"), Result.bOk);
		Test.TestEqual(TEXT("connectivity failure operation is preserved"), Result.Operation, ExpectedOperation);
		Test.TestTrue(TEXT("connectivity failure carries top-level error"), Result.Error.IsSet());
		if (Result.Error.IsSet())
		{
			Test.TestEqual(TEXT("connectivity failure code"), Result.Error->Code, FString(TEXT("graphwrite_connectivity_failed")));
			Test.TestFalse(TEXT("connectivity failure message is readable"), Result.Error->Message.IsEmpty());
		}

		const TSharedPtr<FJsonObject>* Connectivity = nullptr;
		Test.TestTrue(
			TEXT("connectivity failure data contains connectivity object"),
			Result.Data.IsValid() && Result.Data->TryGetObjectField(TEXT("connectivity"), Connectivity) && Connectivity && Connectivity->IsValid());
		if (!Connectivity || !Connectivity->IsValid())
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* Violations = nullptr;
		Test.TestTrue(
			TEXT("connectivity failure exposes concise violations"),
			(*Connectivity)->TryGetArrayField(TEXT("violations"), Violations) && Violations && Violations->Num() > 0);
		if (!Violations || Violations->Num() == 0 || !(*Violations)[0].IsValid())
		{
			return;
		}

		const TSharedPtr<FJsonObject> FirstViolation = (*Violations)[0]->AsObject();
		Test.TestTrue(TEXT("connectivity violation object is valid"), FirstViolation.IsValid());
		if (!FirstViolation.IsValid())
		{
			return;
		}

		Test.TestEqual(
			*FString::Printf(TEXT("%s connectivity violation code"), *ExpectedStageLabel),
			FirstViolation->GetStringField(TEXT("code")),
			FString(TEXT("unconsumed_pure_data_node")));
		Test.TestTrue(
			*FString::Printf(TEXT("%s connectivity violation has node_id"), *ExpectedStageLabel),
			FirstViolation->HasTypedField<EJson::String>(TEXT("node_id")));
		Test.TestTrue(
			*FString::Printf(TEXT("%s connectivity violation has message"), *ExpectedStageLabel),
			FirstViolation->HasTypedField<EJson::String>(TEXT("message")));
		Test.TestFalse(
			*FString::Printf(TEXT("%s connectivity ordinary output omits pin details"), *ExpectedStageLabel),
			FirstViolation->HasField(TEXT("pin_name")));
	}

	static TSharedRef<FJsonObject> MakeGraphWriteTaskPlanPayload(
		const FString& AssetPath,
		const FString& GraphName,
		const TSharedRef<FJsonObject>& Op,
		bool bShouldCompile = false)
	{
		TSharedRef<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), TEXT("step_001"));
		Step->SetStringField(TEXT("capability"), TEXT("graph_write"));

		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Target->SetStringField(TEXT("graph"), GraphName);
		Step->SetObjectField(TEXT("target"), Target);

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op));
		TSharedRef<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), TEXT("owned_graph_edit"));
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);

		TSharedRef<FJsonObject> Constraints = MakeShared<FJsonObject>();
		Constraints->SetBoolField(TEXT("allow_modify_user_nodes"), false);
		Constraints->SetStringField(TEXT("ownership_scope"), TEXT("blueprinthelper_owned"));
		Step->SetObjectField(TEXT("constraints"), Constraints);

		TSharedRef<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
		TaskPlan->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TaskPlan.v1"));
		TaskPlan->SetStringField(TEXT("task_name"), TEXT("GraphWriteRuntimeDryRun"));
		TaskPlan->SetStringField(TEXT("task_type"), TEXT("edit_blueprint_graph"));
		TaskPlan->SetStringField(TEXT("context_id"), TEXT("ctx_graphwrite_runtime_dryrun"));

		TArray<TSharedPtr<FJsonValue>> TargetAssets;
		TargetAssets.Add(MakeShared<FJsonValueString>(AssetPath));
		TaskPlan->SetArrayField(TEXT("target_assets"), TargetAssets);

		TSharedRef<FJsonObject> ExecutionPolicy = MakeShared<FJsonObject>();
		ExecutionPolicy->SetStringField(TEXT("dry_run_mode"), TEXT("full"));
		ExecutionPolicy->SetBoolField(TEXT("should_compile"), bShouldCompile);
		ExecutionPolicy->SetBoolField(TEXT("should_save"), false);
		ExecutionPolicy->SetStringField(TEXT("review_baseline_dirty_asset_policy"), TEXT("allow_stale_disk_snapshot"));
		TaskPlan->SetObjectField(TEXT("execution_policy"), ExecutionPolicy);

		TArray<TSharedPtr<FJsonValue>> Steps;
		Steps.Add(MakeShared<FJsonValueObject>(Step));
		TaskPlan->SetArrayField(TEXT("steps"), Steps);

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("task_plan"), TaskPlan);
		return Payload;
	}

	static bool SetFirstGraphWriteStepAutoSearchPolicy(
		const TSharedRef<FJsonObject>& Payload,
		int32 MaxCandidatesPerStatement,
		int32 MaxAutoSearchStatements,
		int32 MaxTotalAutoSearchMs)
	{
		const TSharedPtr<FJsonObject>* TaskPlanPtr = nullptr;
		if (!Payload->TryGetObjectField(TEXT("task_plan"), TaskPlanPtr) || !TaskPlanPtr || !TaskPlanPtr->IsValid())
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
		if (!(*TaskPlanPtr)->TryGetArrayField(TEXT("steps"), Steps) || !Steps || Steps->Num() == 0)
		{
			return false;
		}

		const TSharedPtr<FJsonObject> Step = (*Steps)[0].IsValid()
			? (*Steps)[0]->AsObject()
			: nullptr;
		const TSharedPtr<FJsonObject>* WritePtr = nullptr;
		if (!Step.IsValid() || !Step->TryGetObjectField(TEXT("write"), WritePtr) || !WritePtr || !WritePtr->IsValid())
		{
			return false;
		}

		TSharedRef<FJsonObject> Policy = MakeShared<FJsonObject>();
		Policy->SetStringField(TEXT("mode"), TEXT("on_preview_resolution_failure"));
		Policy->SetNumberField(TEXT("max_candidates_per_statement"), MaxCandidatesPerStatement);
		Policy->SetNumberField(TEXT("max_auto_search_statements"), MaxAutoSearchStatements);
		Policy->SetNumberField(TEXT("max_total_auto_search_ms"), MaxTotalAutoSearchMs);
		Policy->SetStringField(TEXT("detail_level"), TEXT("short"));
		(*WritePtr)->SetObjectField(TEXT("auto_search_policy"), Policy);
		return true;
	}

	static TSharedRef<FJsonObject> MakeCompositeComponentStep(
		const FString& StepId,
		const FString& AssetPath,
		const FString& ComponentName)
	{
		TSharedRef<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), StepId);
		Step->SetStringField(TEXT("capability"), TEXT("blueprint_component"));

		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Step->SetObjectField(TEXT("target"), Target);

		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("add_component"));
		Op->SetStringField(TEXT("component_name"), ComponentName);
		Op->SetStringField(TEXT("component_class"), TEXT("SceneComponent"));
		Op->SetStringField(TEXT("name_collision_policy"), TEXT("reuse_if_exists"));

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op));

		TSharedRef<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), TEXT("component_tree"));
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);
		return Step;
	}

	static TSharedRef<FJsonObject> MakeCompositeVariableStep(
		const FString& StepId,
		const FString& AssetPath,
		const FString& VariableName)
	{
		TSharedRef<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), StepId);
		Step->SetStringField(TEXT("capability"), TEXT("blueprint_variable"));

		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Step->SetObjectField(TEXT("target"), Target);

		TSharedRef<FJsonObject> PinType = MakeShared<FJsonObject>();
		PinType->SetStringField(TEXT("category"), TEXT("bool"));

		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("ensure_member_variable"));
		Op->SetStringField(TEXT("name"), VariableName);
		Op->SetObjectField(TEXT("pin_type"), PinType);
		Op->SetStringField(TEXT("category"), TEXT("BHSmoke"));

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op));

		TSharedRef<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), TEXT("member_variables"));
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);

		TSharedRef<FJsonObject> Constraints = MakeShared<FJsonObject>();
		Constraints->SetBoolField(TEXT("allow_remove_referenced_variables"), false);
		Step->SetObjectField(TEXT("constraints"), Constraints);
		return Step;
	}

	static TSharedRef<FJsonObject> MakeCompositeCustomEventSignatureStep(
		const FString& StepId,
		const FString& AssetPath,
		const FString& GraphName,
		const FString& EventName)
	{
		TSharedRef<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), StepId);
		Step->SetStringField(TEXT("capability"), TEXT("blueprint_signature"));

		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Step->SetObjectField(TEXT("target"), Target);

		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("ensure_custom_event"));
		Op->SetStringField(TEXT("event_name"), EventName);
		Op->SetStringField(TEXT("graph_name"), GraphName);
		Op->SetStringField(TEXT("name_collision_policy"), TEXT("reuse_if_exists"));

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op));

		TSharedRef<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), TEXT("custom_event_signature"));
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);
		return Step;
	}

	static TSharedRef<FJsonObject> MakeCompositeGraphWriteStep(
		const FString& StepId,
		const FString& AssetPath,
		const FString& GraphName,
		const FString& EventName,
		const FString& DependsOnStepId)
	{
		TSharedRef<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), StepId);
		Step->SetStringField(TEXT("capability"), TEXT("graph_write"));

		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Target->SetStringField(TEXT("graph"), GraphName);
		Step->SetObjectField(TEXT("target"), Target);

		TSharedRef<FJsonObject> Selector = MakeShared<FJsonObject>();
		Selector->SetStringField(TEXT("entry_name"), EventName);

		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("replace_body"));
		Op->SetStringField(TEXT("replace_scope"), TEXT("custom_event_body"));
		Op->SetObjectField(TEXT("selector"), Selector);
		Op->SetObjectField(
			TEXT("logic_spec"),
			MakeCallLogicSpec(FString(), TEXT("PrintString"), TEXT("replace body")));

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op));

		TSharedRef<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), TEXT("owned_graph_edit"));
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);

		TSharedRef<FJsonObject> Constraints = MakeShared<FJsonObject>();
		Constraints->SetBoolField(TEXT("allow_modify_user_nodes"), false);
		Constraints->SetStringField(TEXT("ownership_scope"), TEXT("blueprinthelper_owned"));
		Step->SetObjectField(TEXT("constraints"), Constraints);

		TArray<TSharedPtr<FJsonValue>> DependsOn;
		DependsOn.Add(MakeShared<FJsonValueString>(DependsOnStepId));
		Step->SetArrayField(TEXT("depends_on"), DependsOn);
		return Step;
	}

	static TSharedRef<FJsonObject> MakeCompositeCreateBlueprintFeaturePayload(
		const FString& AssetPath,
		const FString& GraphName,
		const FString& EventName,
		const FString& ComponentName,
		const FString& VariableName)
	{
		const FString SignatureStepId = TEXT("step_signature_custom_event");
		TArray<TSharedPtr<FJsonValue>> Steps;
		Steps.Add(MakeShared<FJsonValueObject>(MakeCompositeComponentStep(TEXT("step_component"), AssetPath, ComponentName)));
		Steps.Add(MakeShared<FJsonValueObject>(MakeCompositeVariableStep(TEXT("step_variable"), AssetPath, VariableName)));
		Steps.Add(MakeShared<FJsonValueObject>(MakeCompositeCustomEventSignatureStep(SignatureStepId, AssetPath, GraphName, EventName)));
		Steps.Add(MakeShared<FJsonValueObject>(MakeCompositeGraphWriteStep(TEXT("step_graph_body"), AssetPath, GraphName, EventName, SignatureStepId)));

		TArray<TSharedPtr<FJsonValue>> TargetAssets;
		TargetAssets.Add(MakeShared<FJsonValueString>(AssetPath));

		TSharedRef<FJsonObject> ExecutionPolicy = MakeShared<FJsonObject>();
		ExecutionPolicy->SetStringField(TEXT("dry_run_mode"), TEXT("full"));
		ExecutionPolicy->SetBoolField(TEXT("should_compile"), true);
		ExecutionPolicy->SetBoolField(TEXT("should_save"), false);

		TSharedRef<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
		TaskPlan->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TaskPlan.v1"));
		TaskPlan->SetStringField(TEXT("task_name"), TEXT("CompositeExecuteFixture"));
		TaskPlan->SetStringField(TEXT("task_type"), TEXT("create_blueprint_feature"));
		TaskPlan->SetStringField(TEXT("context_id"), TEXT("ctx_composite_execute_fixture"));
		TaskPlan->SetArrayField(TEXT("target_assets"), TargetAssets);
		TaskPlan->SetObjectField(TEXT("execution_policy"), ExecutionPolicy);
		TaskPlan->SetArrayField(TEXT("steps"), Steps);

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("task_plan"), TaskPlan);
		return Payload;
	}

	struct FWidgetRuntimeDryRunFixture
	{
		UPackage* Package = nullptr;
		UWidgetBlueprint* Blueprint = nullptr;
		UCanvasPanel* Root = nullptr;
	};

	static FWidgetRuntimeDryRunFixture MakeWidgetRuntimeDryRunFixture(const FString& Prefix)
	{
		FWidgetRuntimeDryRunFixture Fixture;
		Fixture.Package = MakeGraphWriteTestPackage(Prefix);
		Fixture.Blueprint = NewObject<UWidgetBlueprint>(
			Fixture.Package,
			UWidgetBlueprint::StaticClass(),
			*MakeGraphWriteTestObjectName(TEXT("WBP_TaskRuntimeDryRun")),
			RF_Public | RF_Standalone | RF_Transactional);
		if (!Fixture.Blueprint)
		{
			return Fixture;
		}

		Fixture.Blueprint->WidgetTree = NewObject<UWidgetTree>(
			Fixture.Blueprint,
			UWidgetTree::StaticClass(),
			TEXT("WidgetTree"),
			RF_Transactional);
		if (!Fixture.Blueprint->WidgetTree)
		{
			return Fixture;
		}

		Fixture.Root = Fixture.Blueprint->WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("CanvasRoot"));
		Fixture.Blueprint->WidgetTree->RootWidget = Fixture.Root;
		Fixture.Package->SetDirtyFlag(false);
		return Fixture;
	}

	static FWidgetRuntimeDryRunFixture MakeWidgetRuntimeExecuteFixture(const FString& Prefix)
	{
		FWidgetRuntimeDryRunFixture Fixture;
		Fixture.Package = MakeGraphWriteTestPackage(Prefix);
		Fixture.Blueprint = NewObject<UWidgetBlueprint>(
			Fixture.Package,
			UWidgetBlueprint::StaticClass(),
			*MakeGraphWriteTestObjectName(TEXT("WBP_TaskRuntimeExecute")),
			RF_Public | RF_Standalone | RF_Transactional);
		if (!Fixture.Blueprint)
		{
			return Fixture;
		}

		Fixture.Blueprint->WidgetTree = NewObject<UWidgetTree>(
			Fixture.Blueprint,
			UWidgetTree::StaticClass(),
			TEXT("WidgetTree"),
			RF_Transactional);
		Fixture.Package->SetDirtyFlag(false);
		return Fixture;
	}

	static UDataTable* MakeVectorDataTableRuntimeDryRunFixture(const FString& Prefix)
	{
		UPackage* Package = MakeGraphWriteTestPackage(Prefix);
		UDataTable* DataTable = NewObject<UDataTable>(
			Package,
			UDataTable::StaticClass(),
			*MakeGraphWriteTestObjectName(TEXT("DT_TaskRuntimeDryRun")),
			RF_Public | RF_Standalone | RF_Transactional);
		if (!DataTable)
		{
			return nullptr;
		}

		TMap<FName, const uint8*> RawRows;
		DataTable->CreateTableFromRawData(RawRows, TBaseStructure<FVector>::Get());
		Package->SetDirtyFlag(false);
		return DataTable;
	}

	static TSharedRef<FJsonObject> MakeStructuredStep(
		const FString& StepId,
		const FString& Capability,
		const FString& AssetPath,
		const FString& Strategy,
		const TSharedRef<FJsonObject>& Op)
	{
		TSharedRef<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), StepId);
		Step->SetStringField(TEXT("capability"), Capability);

		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Step->SetObjectField(TEXT("target"), Target);

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op));

		TSharedRef<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), Strategy);
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);
		return Step;
	}

	static TSharedRef<FJsonObject> MakeMultiStepTaskPlanPayload(
		const FString& TaskName,
		const FString& TaskType,
		const FString& AssetPath,
		const TArray<TSharedPtr<FJsonValue>>& Steps)
	{
		TArray<TSharedPtr<FJsonValue>> TargetAssets;
		TargetAssets.Add(MakeShared<FJsonValueString>(AssetPath));

		TSharedRef<FJsonObject> ExecutionPolicy = MakeShared<FJsonObject>();
		ExecutionPolicy->SetStringField(TEXT("dry_run_mode"), TEXT("full"));
		ExecutionPolicy->SetBoolField(TEXT("should_compile"), false);
		ExecutionPolicy->SetBoolField(TEXT("should_save"), false);

		TSharedRef<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
		TaskPlan->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TaskPlan.v1"));
		TaskPlan->SetStringField(TEXT("task_name"), TaskName);
		TaskPlan->SetStringField(TEXT("task_type"), TaskType);
		TaskPlan->SetStringField(TEXT("context_id"), TEXT("ctx_task_runtime_planned_state"));
		TaskPlan->SetArrayField(TEXT("target_assets"), TargetAssets);
		TaskPlan->SetObjectField(TEXT("execution_policy"), ExecutionPolicy);
		TaskPlan->SetArrayField(TEXT("steps"), Steps);

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("task_plan"), TaskPlan);
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeGraphWritePlannedVariableDryRunPayload(
		const FString& AssetPath,
		const FString& GraphName,
		const FString& OwnerClassPath)
	{
		const FString VariableName = TEXT("PlannedSessionId");
		const FString EventName = TEXT("CE_PlannedVariablePreview");

		TSharedRef<FJsonObject> VariableOp = MakeShared<FJsonObject>();
		VariableOp->SetStringField(TEXT("op"), TEXT("ensure_member_variable"));
		VariableOp->SetStringField(TEXT("name"), VariableName);
		TSharedRef<FJsonObject> PinType = MakeShared<FJsonObject>();
		PinType->SetStringField(TEXT("category"), TEXT("string"));
		VariableOp->SetObjectField(TEXT("pin_type"), PinType);
		VariableOp->SetStringField(TEXT("category"), TEXT("BH_Test"));

		TSharedRef<FJsonObject> SignatureOp = MakeShared<FJsonObject>();
		SignatureOp->SetStringField(TEXT("op"), TEXT("ensure_custom_event"));
		SignatureOp->SetStringField(TEXT("event_name"), EventName);
		SignatureOp->SetStringField(TEXT("graph_name"), GraphName);
		SignatureOp->SetStringField(TEXT("name_collision_policy"), TEXT("reuse_if_exists"));

		TSharedRef<FJsonObject> FieldExpression = MakeShared<FJsonObject>();
		FieldExpression->SetStringField(TEXT("kind"), TEXT("get"));
		FieldExpression->SetStringField(TEXT("target"), VariableName);
		TSharedRef<FJsonObject> ContextEvidence = MakeShared<FJsonObject>();
		ContextEvidence->SetStringField(TEXT("field_owner_class"), OwnerClassPath);
		FieldExpression->SetObjectField(TEXT("context_evidence"), ContextEvidence);

		TSharedRef<FJsonObject> PrintStatement = MakeShared<FJsonObject>();
		PrintStatement->SetStringField(TEXT("kind"), TEXT("call"));
		PrintStatement->SetStringField(TEXT("target"), TEXT("PrintString"));
		TSharedRef<FJsonObject> Args = MakeShared<FJsonObject>();
		Args->SetObjectField(TEXT("InString"), FieldExpression);
		PrintStatement->SetObjectField(TEXT("args"), Args);

		TArray<TSharedPtr<FJsonValue>> Statements;
		Statements.Add(MakeShared<FJsonValueObject>(PrintStatement));

		TSharedRef<FJsonObject> GraphWriteOp = MakeShared<FJsonObject>();
		GraphWriteOp->SetStringField(TEXT("op"), TEXT("ensure_entry"));
		GraphWriteOp->SetStringField(TEXT("entry_type"), TEXT("custom_event"));
		GraphWriteOp->SetStringField(TEXT("name"), EventName);
		GraphWriteOp->SetStringField(TEXT("signature_evidence_id"), TEXT("signature:custom_event:CE_PlannedVariablePreview"));
		GraphWriteOp->SetObjectField(TEXT("body"), MakeGraphWriteLogicSpec(EventName, Statements));

		TSharedRef<FJsonObject> GraphWriteStep = MakeStructuredStep(
			TEXT("step_graph"),
			TEXT("graph_write"),
			AssetPath,
			TEXT("owned_graph_edit"),
			GraphWriteOp);
		TSharedRef<FJsonObject> GraphTarget = MakeShared<FJsonObject>();
		GraphTarget->SetStringField(TEXT("asset_path"), AssetPath);
		GraphTarget->SetStringField(TEXT("graph"), GraphName);
		GraphWriteStep->SetObjectField(TEXT("target"), GraphTarget);
		TArray<TSharedPtr<FJsonValue>> DependsOn;
		DependsOn.Add(MakeShared<FJsonValueString>(TEXT("step_signature")));
		GraphWriteStep->SetArrayField(TEXT("depends_on"), DependsOn);

		TSharedRef<FJsonObject> Constraints = MakeShared<FJsonObject>();
		Constraints->SetBoolField(TEXT("allow_modify_user_nodes"), false);
		Constraints->SetStringField(TEXT("ownership_scope"), TEXT("blueprinthelper_owned"));
		GraphWriteStep->SetObjectField(TEXT("constraints"), Constraints);

		TArray<TSharedPtr<FJsonValue>> Steps;
		Steps.Add(MakeShared<FJsonValueObject>(MakeStructuredStep(
			TEXT("step_variable"),
			TEXT("blueprint_variable"),
			AssetPath,
			TEXT("member_variables"),
			VariableOp)));
		Steps.Add(MakeShared<FJsonValueObject>(MakeStructuredStep(
			TEXT("step_signature"),
			TEXT("blueprint_signature"),
			AssetPath,
			TEXT("custom_event_signature"),
			SignatureOp)));
		Steps.Add(MakeShared<FJsonValueObject>(GraphWriteStep));

		return MakeMultiStepTaskPlanPayload(
			TEXT("GraphWritePlannedMemberVariableDryRun"),
			TEXT("create_blueprint_feature"),
			AssetPath,
			Steps);
	}

	static TSharedRef<FJsonObject> MakeWidgetTreeExecutePayload(const FString& AssetPath)
	{
		TSharedRef<FJsonObject> AddRootOp = MakeShared<FJsonObject>();
		AddRootOp->SetStringField(TEXT("op"), TEXT("add_widget"));
		AddRootOp->SetStringField(TEXT("widget_class"), TEXT("CanvasPanel"));
		AddRootOp->SetStringField(TEXT("widget_name"), TEXT("RootCanvas"));

		TSharedRef<FJsonObject> AddChildOp = MakeShared<FJsonObject>();
		AddChildOp->SetStringField(TEXT("op"), TEXT("add_widget"));
		AddChildOp->SetStringField(TEXT("widget_class"), TEXT("TextBlock"));
		AddChildOp->SetStringField(TEXT("widget_name"), TEXT("SmokeText"));
		AddChildOp->SetStringField(TEXT("parent_widget_name"), TEXT("RootCanvas"));

		TSharedRef<FJsonObject> SetOpacityOp = MakeShared<FJsonObject>();
		SetOpacityOp->SetStringField(TEXT("op"), TEXT("set_widget_property"));
		SetOpacityOp->SetStringField(TEXT("widget_name"), TEXT("SmokeText"));
		SetOpacityOp->SetStringField(TEXT("property_path"), TEXT("RenderOpacity"));
		SetOpacityOp->SetStringField(TEXT("value"), TEXT("0.35"));

		TArray<TSharedPtr<FJsonValue>> Steps;
		Steps.Add(MakeShared<FJsonValueObject>(MakeStructuredStep(
			TEXT("step_add_root"),
			TEXT("umg_widget"),
			AssetPath,
			TEXT("widget_tree_edit"),
			AddRootOp)));
		Steps.Add(MakeShared<FJsonValueObject>(MakeStructuredStep(
			TEXT("step_add_child"),
			TEXT("umg_widget"),
			AssetPath,
			TEXT("widget_tree_edit"),
			AddChildOp)));
		Steps.Add(MakeShared<FJsonValueObject>(MakeStructuredStep(
			TEXT("step_set_child_opacity"),
			TEXT("umg_widget"),
			AssetPath,
			TEXT("widget_property_edit"),
			SetOpacityOp)));

		return MakeMultiStepTaskPlanPayload(
			TEXT("WidgetTreeExecuteSmoke"),
			TEXT("edit_umg_widget"),
			AssetPath,
			Steps);
	}

	static TSharedRef<FJsonObject> MakeWidgetPlannedPropertyDryRunPayload(const FString& AssetPath)
	{
		TSharedRef<FJsonObject> AddOp = MakeShared<FJsonObject>();
		AddOp->SetStringField(TEXT("op"), TEXT("add_widget"));
		AddOp->SetStringField(TEXT("widget_class"), TEXT("TextBlock"));
		AddOp->SetStringField(TEXT("widget_name"), TEXT("PlannedText"));
		AddOp->SetStringField(TEXT("parent_widget_name"), TEXT("CanvasRoot"));

		TSharedRef<FJsonObject> SetOp = MakeShared<FJsonObject>();
		SetOp->SetStringField(TEXT("op"), TEXT("set_widget_property"));
		SetOp->SetStringField(TEXT("widget_name"), TEXT("PlannedText"));
		SetOp->SetStringField(TEXT("property_path"), TEXT("Text"));
		SetOp->SetStringField(TEXT("value"), TEXT("Preview Only"));

		TArray<TSharedPtr<FJsonValue>> Steps;
		Steps.Add(MakeShared<FJsonValueObject>(MakeStructuredStep(
			TEXT("step_add_widget"),
			TEXT("umg_widget"),
			AssetPath,
			TEXT("widget_tree_edit"),
			AddOp)));
		Steps.Add(MakeShared<FJsonValueObject>(MakeStructuredStep(
			TEXT("step_set_widget_text"),
			TEXT("umg_widget"),
			AssetPath,
			TEXT("widget_property_edit"),
			SetOp)));

		return MakeMultiStepTaskPlanPayload(
			TEXT("WidgetPlannedPropertyDryRun"),
			TEXT("edit_umg_widget"),
			AssetPath,
			Steps);
	}

	static TSharedRef<FJsonObject> MakeDataTablePlannedRowUpdateDryRunPayload(const FString& AssetPath)
	{
		TSharedRef<FJsonObject> AddFields = MakeShared<FJsonObject>();
		AddFields->SetNumberField(TEXT("X"), 1.0);
		AddFields->SetNumberField(TEXT("Y"), 2.0);
		AddFields->SetNumberField(TEXT("Z"), 3.0);

		TSharedRef<FJsonObject> AddOp = MakeShared<FJsonObject>();
		AddOp->SetStringField(TEXT("op"), TEXT("add_row"));
		AddOp->SetStringField(TEXT("row_name"), TEXT("FutureRow"));
		AddOp->SetObjectField(TEXT("fields"), AddFields);

		TSharedRef<FJsonObject> UpdateFields = MakeShared<FJsonObject>();
		UpdateFields->SetNumberField(TEXT("X"), 4.0);

		TSharedRef<FJsonObject> UpdateOp = MakeShared<FJsonObject>();
		UpdateOp->SetStringField(TEXT("op"), TEXT("update_row"));
		UpdateOp->SetStringField(TEXT("row_name"), TEXT("FutureRow"));
		UpdateOp->SetObjectField(TEXT("fields"), UpdateFields);

		TArray<TSharedPtr<FJsonValue>> Steps;
		Steps.Add(MakeShared<FJsonValueObject>(MakeStructuredStep(
			TEXT("step_add_row"),
			TEXT("data_table"),
			AssetPath,
			TEXT("row_edit"),
			AddOp)));
		Steps.Add(MakeShared<FJsonValueObject>(MakeStructuredStep(
			TEXT("step_update_row"),
			TEXT("data_table"),
			AssetPath,
			TEXT("row_edit"),
			UpdateOp)));

		return MakeMultiStepTaskPlanPayload(
			TEXT("DataTablePlannedRowUpdateDryRun"),
			TEXT("edit_data_table"),
			AssetPath,
			Steps);
	}

	static void AssertRuntimePreviewCanExecute(
		FAutomationTestBase& Test,
		const FBlueprintHelperToolResultBase& Result,
		int32 ExpectedStepCount)
	{
		Test.TestTrue(TEXT("runtime preview succeeds"), Result.bOk);
		Test.TestEqual(TEXT("runtime preview status is dry-run"), Result.Status, EBlueprintHelperToolStatus::DryRun);
		Test.TestNotNull(TEXT("runtime preview data exists"), Result.Data.Get());

		const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
		Test.TestTrue(TEXT("runtime preview has child steps"),
			Result.Data.IsValid() && Result.Data->TryGetArrayField(TEXT("steps"), Steps));
		Test.TestEqual(TEXT("runtime preview child step count"), Steps ? Steps->Num() : 0, ExpectedStepCount);

		const TSharedPtr<FJsonObject>* DryRun = nullptr;
		Test.TestTrue(TEXT("runtime preview has dry_run summary"),
			Result.Data.IsValid() && Result.Data->TryGetObjectField(TEXT("dry_run"), DryRun));
		bool bCanExecute = false;
		Test.TestTrue(TEXT("dry_run.can_execute is present"),
			DryRun && DryRun->IsValid() && (*DryRun)->TryGetBoolField(TEXT("can_execute"), bCanExecute));
		Test.TestTrue(TEXT("planned-state dry-run can execute"), bCanExecute);
	}

	static TSharedPtr<FJsonObject> FindRuntimePreviewStep(
		const FBlueprintHelperToolResultBase& Result,
		const FString& StepId)
	{
		const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
		if (!Result.Data.IsValid() ||
			!Result.Data->TryGetArrayField(TEXT("steps"), Steps) ||
			!Steps)
		{
			return nullptr;
		}

		for (const TSharedPtr<FJsonValue>& StepValue : *Steps)
		{
			const TSharedPtr<FJsonObject> StepObject = StepValue.IsValid()
				? StepValue->AsObject()
				: nullptr;
			FString CandidateStepId;
			if (StepObject.IsValid() &&
				StepObject->TryGetStringField(TEXT("step_id"), CandidateStepId) &&
				CandidateStepId == StepId)
			{
				return StepObject;
			}
		}
		return nullptr;
	}

	static bool TryReadRuntimePreviewStepResultOk(
		const TSharedPtr<FJsonObject>& StepObject,
		bool& bOutOk,
		FString& OutResultJson)
	{
		bOutOk = false;
		OutResultJson.Reset();

		const TSharedPtr<FJsonObject>* ResultObject = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetObjectField(TEXT("result"), ResultObject) ||
			!ResultObject ||
			!ResultObject->IsValid())
		{
			return false;
		}

		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutResultJson);
		FJsonSerializer::Serialize((*ResultObject).ToSharedRef(), Writer);
		return (*ResultObject)->TryGetBoolField(TEXT("ok"), bOutOk);
	}

	static TSharedRef<FJsonObject> MakeReplaceBodyOp()
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("replace_body"));
		Op->SetStringField(TEXT("replace_scope"), TEXT("custom_event_body"));

		TSharedRef<FJsonObject> Selector = MakeShared<FJsonObject>();
		Selector->SetStringField(TEXT("entry_name"), TEXT("SmokeCustomEvent"));
		Op->SetObjectField(TEXT("selector"), Selector);

		Op->SetObjectField(
			TEXT("logic_spec"),
			MakeCallLogicSpec(FString(), TEXT("PrintString"), TEXT("replace body")));
		return Op;
	}

	static TSharedRef<FJsonObject> MakeReplaceFunctionBodyParamDataFlowOp(
		const FString& FunctionName,
		const FString& ParamName)
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("replace_body"));
		Op->SetStringField(TEXT("replace_scope"), TEXT("function_body"));

		TSharedRef<FJsonObject> Selector = MakeShared<FJsonObject>();
		Selector->SetStringField(TEXT("function_name"), FunctionName);
		Op->SetObjectField(TEXT("selector"), Selector);

		Op->SetObjectField(
			TEXT("logic_spec"),
			MakePrintFunctionParamLogicSpec(ParamName, FunctionName));
		return Op;
	}

	static TSharedRef<FJsonObject> MakeSetPinDefaultOp()
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("set_pin_default"));
		Op->SetStringField(TEXT("patch_scope"), TEXT("pin_default"));

		TSharedRef<FJsonObject> PatchedRef = MakeShared<FJsonObject>();
		PatchedRef->SetStringField(TEXT("node_ref"), TEXT("MissingNode"));
		PatchedRef->SetStringField(TEXT("pin_ref"), TEXT("Condition"));
		Op->SetObjectField(TEXT("patched_ref"), PatchedRef);

		TSharedRef<FJsonObject> Patch = MakeShared<FJsonObject>();
		Patch->SetStringField(TEXT("value"), TEXT("true"));
		Op->SetObjectField(TEXT("patch"), Patch);
		return Op;
	}

	static TSharedRef<FJsonObject> MakeBranchForkOwnedBlockCallOp(
		const FString& AnchorBlockId,
		const FString& AnchorEntryNodePath,
		const FString& InsertedBlockId)
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("insert_flow"));
		Op->SetStringField(TEXT("merge_scope"), TEXT("owned_block_call"));
		Op->SetStringField(TEXT("insert_strategy"), TEXT("branch_fork"));

		TSharedRef<FJsonObject> Anchor = MakeShared<FJsonObject>();
		Anchor->SetStringField(TEXT("block_id"), AnchorBlockId);
		Anchor->SetStringField(TEXT("group_entry_node_path"), AnchorEntryNodePath);
		Anchor->SetStringField(TEXT("node_ref"), TEXT("nodes[0]"));
		Anchor->SetStringField(TEXT("pin_ref"), TEXT("Then"));
		Op->SetObjectField(TEXT("anchor"), Anchor);

		TSharedRef<FJsonObject> Inserted = MakeShared<FJsonObject>();
		Inserted->SetStringField(TEXT("block_id"), InsertedBlockId);
		Op->SetObjectField(TEXT("inserted"), Inserted);

		TArray<TSharedPtr<FJsonValue>> SequenceOrder;
		SequenceOrder.Add(MakeShared<FJsonValueString>(TEXT("inserted_logic")));
		SequenceOrder.Add(MakeShared<FJsonValueString>(TEXT("original_successor")));
		Op->SetArrayField(TEXT("sequence_order"), SequenceOrder);
		return Op;
	}

	static TSharedRef<FJsonObject> MakeInsertFlowOp()
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("insert_flow"));
		Op->SetStringField(TEXT("merge_scope"), TEXT("custom_event_call"));
		Op->SetStringField(TEXT("insert_strategy"), TEXT("append_after"));

		TSharedRef<FJsonObject> Anchor = MakeShared<FJsonObject>();
		Anchor->SetStringField(TEXT("node_ref"), TEXT("MissingAnchor"));
		Anchor->SetStringField(TEXT("pin_ref"), TEXT("Then"));
		Op->SetObjectField(TEXT("anchor"), Anchor);

		TSharedRef<FJsonObject> Inserted = MakeShared<FJsonObject>();
		Inserted->SetStringField(TEXT("custom_event"), TEXT("SmokeCustomEvent"));
		Op->SetObjectField(TEXT("inserted"), Inserted);
		return Op;
	}

	static TSharedRef<FJsonObject> MakeEnsureEntryCallFunctionOp(
		const FString& EventName,
		const FString& FunctionName)
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("replace_body"));
		Op->SetStringField(TEXT("replace_scope"), TEXT("custom_event_body"));

		TSharedRef<FJsonObject> Selector = MakeShared<FJsonObject>();
		Selector->SetStringField(TEXT("entry_name"), EventName);
		Op->SetObjectField(TEXT("selector"), Selector);
		Op->SetObjectField(
			TEXT("logic_spec"),
			MakeCallLogicSpec(FString(), FunctionName, TEXT("runtime call_function")));
		return Op;
	}

	static TSharedRef<FJsonObject> MakeEnsureEntryCallFunctionNameOp(
		const FString& EventName,
		const FString& FunctionName)
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("replace_body"));
		Op->SetStringField(TEXT("replace_scope"), TEXT("custom_event_body"));

		TSharedRef<FJsonObject> Selector = MakeShared<FJsonObject>();
		Selector->SetStringField(TEXT("entry_name"), EventName);
		Op->SetObjectField(TEXT("selector"), Selector);
		Op->SetObjectField(
			TEXT("logic_spec"),
			MakeCallNameLogicSpec(FString(), FunctionName));
		return Op;
	}

	static TSharedRef<FJsonObject> MakeEnsureEntrySetSimulatePhysicsTargetObjectOp(
		const FString& EventName,
		const FString& TargetObjectTypeOverride = FString(),
		const FString& TargetObjectPinObjectPathOverride = FString())
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("replace_body"));
		Op->SetStringField(TEXT("replace_scope"), TEXT("custom_event_body"));

		TSharedRef<FJsonObject> Selector = MakeShared<FJsonObject>();
		Selector->SetStringField(TEXT("entry_name"), EventName);
		Op->SetObjectField(TEXT("selector"), Selector);
		Op->SetObjectField(
			TEXT("logic_spec"),
			MakeSetSimulatePhysicsTargetObjectLogicSpec(
				TargetObjectTypeOverride,
				TargetObjectPinObjectPathOverride));
		return Op;
	}

	struct FGraphWriteRuntimeHarness
	{
		FBlueprintHelperGraphResolver Resolver;
		FBlueprintHelperCompileService CompileService;
		FBlueprintHelperAssetBrowseService AssetBrowseService;
		FBlueprintHelperBlockIdService BlockIdService;
		FBlueprintHelperOwnershipService OwnershipService;
		FBlueprintHelperAppendBlueprintGraphService AppendGraphService;
		FBlueprintHelperGraphSnapshotService SnapshotService;
		FBlueprintHelperReplaceBlueprintGraphService ReplaceGraphService;
		FBlueprintHelperLogicJsonPathService PathService;
		FBlueprintHelperPatchBlueprintGraphService PatchGraphService;
		FBlueprintHelperMergeBlueprintGraphService MergeGraphService;
		FBlueprintHelperGraphWriteServiceRegistry GraphWriteRegistry;
		FBlueprintHelperBlueprintStructureService StructureService;
		FBlueprintHelperBlueprintVariableService VariableService;
		FBlueprintHelperAssetFactoryService AssetFactoryService;
		FBlueprintHelperComponentService ComponentService;
		FBlueprintHelperClassSettingsService ClassSettingsService;
		FBlueprintHelperWidgetService WidgetService;
		FBlueprintHelperDataTableService DataTableService;
		FBlueprintHelperPropertyReflectionService PropertyReflectionService;
		FBlueprintHelperCompileAssetService CompileAssetService;
		FBlueprintHelperTaskRuntimeService RuntimeService;

		FGraphWriteRuntimeHarness()
			: CompileService(Resolver)
			, AppendGraphService(Resolver, BlockIdService, OwnershipService)
			, ReplaceGraphService(Resolver, BlockIdService, OwnershipService, SnapshotService)
			, PatchGraphService(Resolver, PathService)
			, MergeGraphService(Resolver, PathService)
			, StructureService(Resolver)
			, VariableService(Resolver, StructureService)
			, ComponentService(Resolver)
			, ClassSettingsService(Resolver)
			, CompileAssetService(CompileService)
			, RuntimeService(
				GraphWriteRegistry,
				VariableService,
				StructureService,
				AssetFactoryService,
				ComponentService,
				ClassSettingsService,
				WidgetService,
				DataTableService,
				PropertyReflectionService,
				CompileAssetService,
				AssetBrowseService)
		{
			GraphWriteRegistry.RegisterHandler(TEXT("append_blueprint_graph"), [this](const TSharedRef<FJsonObject>& Payload)
			{
				return AppendGraphService.Execute(Payload);
			});
			GraphWriteRegistry.RegisterHandler(TEXT("replace_blueprint_graph"), [this](const TSharedRef<FJsonObject>& Payload)
			{
				return ReplaceGraphService.Execute(Payload);
			});
			GraphWriteRegistry.RegisterHandler(TEXT("patch_blueprint_graph"), [this](const TSharedRef<FJsonObject>& Payload)
			{
				return PatchGraphService.Execute(Payload);
			});
			GraphWriteRegistry.RegisterHandler(TEXT("merge_blueprint_graph"), [this](const TSharedRef<FJsonObject>& Payload)
			{
				return MergeGraphService.Execute(Payload);
			});
		}
	};

	static void AssertRuntimePreviewReachedGraphWriteService(
		FAutomationTestBase& Test,
		const FBlueprintHelperToolResultBase& Result,
		const FString& ExpectedAdapterOperation,
		bool bExpectedCanExecute)
	{
		Test.TestTrue(TEXT("preview_task_plan command returns structured dry-run result"), Result.bOk);
		Test.TestEqual(TEXT("runtime preview operation is preserved"), Result.Operation, FString(TEXT("preview_task_plan")));
		Test.TestEqual(TEXT("runtime preview status is dry-run"), Result.Status, EBlueprintHelperToolStatus::DryRun);
		Test.TestNotNull(TEXT("runtime preview data exists"), Result.Data.Get());

		const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
		Test.TestTrue(TEXT("runtime preview data contains child steps"),
			Result.Data.IsValid() && Result.Data->TryGetArrayField(TEXT("steps"), Steps));
		Test.TestTrue(TEXT("runtime preview has one child step"), Steps && Steps->Num() == 1);
		if (!Steps || Steps->Num() == 0)
		{
			return;
		}

		const TSharedPtr<FJsonObject> Step = (*Steps)[0]->AsObject();
		FString AdapterOperation;
		Test.TestTrue(TEXT("child step records adapter operation"),
			Step.IsValid() && Step->TryGetStringField(TEXT("adapter_operation"), AdapterOperation));
		Test.TestEqual(TEXT("child step adapter operation reaches graph write service"), AdapterOperation, ExpectedAdapterOperation);

		const TSharedPtr<FJsonObject>* ChildResult = nullptr;
		Test.TestTrue(TEXT("child step carries ToolResultBase"),
			Step.IsValid() && Step->TryGetObjectField(TEXT("result"), ChildResult));
		FString ChildOperation;
		Test.TestTrue(TEXT("child ToolResultBase operation is readable"),
			ChildResult && ChildResult->IsValid() && (*ChildResult)->TryGetStringField(TEXT("operation"), ChildOperation));
		Test.TestEqual(TEXT("child ToolResultBase operation is adapter"), ChildOperation, ExpectedAdapterOperation);

		const TSharedPtr<FJsonObject>* DryRun = nullptr;
		Test.TestTrue(TEXT("runtime preview exposes dry_run summary"),
			Result.Data.IsValid() && Result.Data->TryGetObjectField(TEXT("dry_run"), DryRun));
		bool bCanExecute = !bExpectedCanExecute;
		Test.TestTrue(TEXT("dry_run.can_execute is present"),
			DryRun && DryRun->IsValid() && (*DryRun)->TryGetBoolField(TEXT("can_execute"), bCanExecute));
		if (bCanExecute != bExpectedCanExecute && DryRun && DryRun->IsValid())
		{
			FString DryRunJson;
			const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&DryRunJson);
			FJsonSerializer::Serialize((*DryRun).ToSharedRef(), Writer);
			Test.AddInfo(FString::Printf(TEXT("dry_run mismatch payload: %s"), *DryRunJson));
		}
		Test.TestEqual(TEXT("dry_run.can_execute matches child preflight"), bCanExecute, bExpectedCanExecute);
	}

	static void AddToolResultFailureDetail(
		FAutomationTestBase& Test,
		const FString& Label,
		const FBlueprintHelperToolResultBase& Result)
	{
		if (Result.bOk)
		{
			return;
		}

		FString Message = FString::Printf(
			TEXT("%s failed: operation=%s status=%d modified=%d"),
			*Label,
			*Result.Operation,
			static_cast<int32>(Result.Status),
			Result.bModified ? 1 : 0);
		if (Result.Error.IsSet())
		{
			Message += FString::Printf(
				TEXT(" error_code=%s error_stage=%d error_field=%s error_message=%s"),
				*Result.Error->Code,
				static_cast<int32>(Result.Error->Stage),
				*Result.Error->Field,
				*Result.Error->Message);
		}
		if (Result.Data.IsValid())
		{
			FString DataJson;
			const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&DataJson);
			FJsonSerializer::Serialize(Result.Data.ToSharedRef(), Writer);
			Message += FString::Printf(TEXT(" data=%s"), *DataJson);
		}
		Test.AddError(Message);
	}

	static bool GetRuntimeDryRunFirstError(
		const FBlueprintHelperToolResultBase& Result,
		FString& OutCode,
		FString& OutMessage)
	{
		OutCode.Reset();
		OutMessage.Reset();

		if (!Result.Data.IsValid())
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* DryRun = nullptr;
		if (!Result.Data->TryGetObjectField(TEXT("dry_run"), DryRun) ||
			!DryRun ||
			!DryRun->IsValid())
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Errors = nullptr;
		if (!(*DryRun)->TryGetArrayField(TEXT("errors"), Errors) ||
			!Errors ||
			Errors->Num() == 0)
		{
			return false;
		}

		const TSharedPtr<FJsonObject> ErrorObject = (*Errors)[0].IsValid()
			? (*Errors)[0]->AsObject()
			: nullptr;
		if (!ErrorObject.IsValid())
		{
			return false;
		}

		ErrorObject->TryGetStringField(TEXT("code"), OutCode);
		ErrorObject->TryGetStringField(TEXT("message"), OutMessage);
		return !OutCode.IsEmpty() || !OutMessage.IsEmpty();
	}

	static bool GetRuntimeDryRunFirstErrorObject(
		const FBlueprintHelperToolResultBase& Result,
		TSharedPtr<FJsonObject>& OutError)
	{
		OutError.Reset();
		if (!Result.Data.IsValid())
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* DryRun = nullptr;
		if (!Result.Data->TryGetObjectField(TEXT("dry_run"), DryRun) ||
			!DryRun ||
			!DryRun->IsValid())
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Errors = nullptr;
		if (!(*DryRun)->TryGetArrayField(TEXT("errors"), Errors) ||
			!Errors ||
			Errors->Num() == 0)
		{
			return false;
		}

		OutError = (*Errors)[0].IsValid()
			? (*Errors)[0]->AsObject()
			: nullptr;
		return OutError.IsValid();
	}

	static bool GetFirstResolvedCallFunctionFact(
		const TSharedPtr<FJsonObject>& Data,
		TSharedPtr<FJsonObject>& OutFact)
	{
		OutFact.Reset();
		if (!Data.IsValid())
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* RuntimeFacts = nullptr;
		if (!Data->TryGetObjectField(TEXT("runtime_facts"), RuntimeFacts) ||
			!RuntimeFacts ||
			!RuntimeFacts->IsValid())
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* ResolvedCalls = nullptr;
		if (!(*RuntimeFacts)->TryGetArrayField(TEXT("resolved_call_functions"), ResolvedCalls) ||
			!ResolvedCalls ||
			ResolvedCalls->Num() == 0)
		{
			return false;
		}

		OutFact = (*ResolvedCalls)[0].IsValid()
			? (*ResolvedCalls)[0]->AsObject()
			: nullptr;
		return OutFact.IsValid();
	}

	struct FGraphWriteReadbackEvidence
	{
		bool bHasResolverEvidence = false;
		bool bHasSpawnEvidence = false;
		FString SelectedStableId;
		FString SelectedSpawnerClass;
		FString FunctionReference;
		FString FieldName;
		FString SingletonStableId;
		FString EventName;
		FString StructTypeName;
	};

	static bool HasResolverAndSpawnEvidence(const FGraphWriteReadbackEvidence& Evidence)
	{
		return Evidence.bHasResolverEvidence && Evidence.bHasSpawnEvidence;
	}

	static FString MakeCallFunctionStableId(const UK2Node_CallFunction* CallNode)
	{
		const UFunction* Function = CallNode ? CallNode->GetTargetFunction() : nullptr;
		const UClass* OwnerClass = Function ? Function->GetOwnerClass() : nullptr;
		return Function && OwnerClass
			? FString::Printf(TEXT("%s:%s"), *OwnerClass->GetPathName(), *Function->GetName())
			: FString();
	}

	static bool ReadbackFindsFunctionCallByEvidence(
		UEdGraph* Graph,
		const FGraphWriteReadbackEvidence& Evidence)
	{
		if (!Graph || !HasResolverAndSpawnEvidence(Evidence))
		{
			return false;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
			if (!CallNode)
			{
				continue;
			}

			const FString StableId = MakeCallFunctionStableId(CallNode);
			const FString FunctionName = CallNode->GetFunctionName().ToString();
			if ((!Evidence.SelectedStableId.IsEmpty() && StableId.Equals(Evidence.SelectedStableId, ESearchCase::IgnoreCase)) ||
				(!Evidence.FunctionReference.IsEmpty() && FunctionName.Equals(Evidence.FunctionReference, ESearchCase::IgnoreCase)))
			{
				return true;
			}
		}
		return false;
	}

	static bool ReadbackFindsVariableNodeByEvidence(
		UEdGraph* Graph,
		const FGraphWriteReadbackEvidence& Evidence,
		const bool bExpectSet)
	{
		if (!Graph || !HasResolverAndSpawnEvidence(Evidence) || Evidence.FieldName.IsEmpty())
		{
			return false;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			const UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(Node);
			if (!VariableNode)
			{
				continue;
			}
			if (bExpectSet != Node->IsA<UK2Node_VariableSet>())
			{
				continue;
			}
			if (VariableNode->VariableReference.GetMemberName().ToString().Equals(Evidence.FieldName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	static bool ReadbackFindsSingletonControlByEvidence(
		UEdGraph* Graph,
		const FGraphWriteReadbackEvidence& Evidence)
	{
		if (!Graph || !HasResolverAndSpawnEvidence(Evidence) || Evidence.SingletonStableId.IsEmpty())
		{
			return false;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}
			if (Evidence.SingletonStableId.Contains(TEXT("branch")) && Node->IsA<UK2Node_IfThenElse>())
			{
				return true;
			}
			if (Evidence.SingletonStableId.Contains(TEXT("sequence")) && Node->IsA<UK2Node_ExecutionSequence>())
			{
				return true;
			}
			if (Evidence.SingletonStableId.Contains(TEXT("select")) && Node->IsA<UK2Node_Select>())
			{
				return true;
			}
		}
		return false;
	}

	static bool ReadbackFindsCustomEventByEvidence(
		UEdGraph* Graph,
		const FGraphWriteReadbackEvidence& Evidence)
	{
		return Graph &&
			HasResolverAndSpawnEvidence(Evidence) &&
			!Evidence.EventName.IsEmpty() &&
			CountCustomEventsByName(Graph, Evidence.EventName) > 0;
	}

	static bool ReadbackFindsStructNodeByEvidence(
		UEdGraph* Graph,
		const FGraphWriteReadbackEvidence& Evidence,
		const bool bExpectMake)
	{
		if (!Graph || !HasResolverAndSpawnEvidence(Evidence) || Evidence.StructTypeName.IsEmpty())
		{
			return false;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			const UScriptStruct* StructType = nullptr;
			if (bExpectMake)
			{
				const UK2Node_MakeStruct* MakeStructNode = Cast<UK2Node_MakeStruct>(Node);
				StructType = MakeStructNode ? MakeStructNode->StructType.Get() : nullptr;
			}
			else
			{
				const UK2Node_BreakStruct* BreakStructNode = Cast<UK2Node_BreakStruct>(Node);
				StructType = BreakStructNode ? BreakStructNode->StructType.Get() : nullptr;
			}

			if (StructType &&
				(StructType->GetName().Equals(Evidence.StructTypeName, ESearchCase::IgnoreCase) ||
				 StructType->GetPathName().Equals(Evidence.StructTypeName, ESearchCase::IgnoreCase)))
			{
				return true;
			}
		}
		return false;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteAppendBlockedDryRunErrorEnvelopeTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.AppendBlockedDryRunErrorEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteAppendBlockedDryRunErrorEnvelopeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("AppendBlockedDryRun"));
	UEdGraph* FunctionGraph = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteFunctionGraph(Blueprint, TEXT("CalculateSmokeValue"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	TestNotNull(TEXT("function graph is created"), FunctionGraph);
	if (!Blueprint || !FunctionGraph)
	{
		return false;
	}

	const int32 NodeCountBefore = FunctionGraph->Nodes.Num();
	const bool bDirtyBefore = Blueprint->GetOutermost()->IsDirty();

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperAppendBlueprintGraphService AppendService(
		Resolver,
		BlockIdService,
		OwnershipService);
	const FBlueprintHelperToolResultBase Result = AppendService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeAppendPreviewPayload(Blueprint->GetPathName(), FunctionGraph->GetName()));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertBlockedDryRunFailure(
		*this,
		Result,
		TEXT("append_blueprint_graph"),
		TEXT("target_graph_type_invalid"),
		TEXT("target.graph"));
	TestEqual(TEXT("blocked append preview leaves function graph nodes unchanged"), FunctionGraph->Nodes.Num(), NodeCountBefore);
	TestEqual(TEXT("blocked append preview leaves package dirty flag unchanged"), Blueprint->GetOutermost()->IsDirty(), bDirtyBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteReplaceBlockedDryRunErrorEnvelopeTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.ReplaceBlockedDryRunErrorEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteReplaceBlockedDryRunErrorEnvelopeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("ReplaceBlockedDryRun"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	const int32 UbergraphCountBefore = Blueprint->UbergraphPages.Num();
	const bool bDirtyBefore = Blueprint->GetOutermost()->IsDirty();

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperGraphSnapshotService SnapshotService;
	FBlueprintHelperReplaceBlueprintGraphService ReplaceService(
		Resolver,
		BlockIdService,
		OwnershipService,
		SnapshotService);

	const FBlueprintHelperToolResultBase Result = ReplaceService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeReplacePreviewPayload(Blueprint->GetPathName(), TEXT("MissingGraph")));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertBlockedDryRunFailure(
		*this,
		Result,
		TEXT("replace_blueprint_graph"),
		TEXT("target_graph_not_found"),
		TEXT("target.graph"));
	TestEqual(TEXT("blocked replace preview leaves graph count unchanged"), Blueprint->UbergraphPages.Num(), UbergraphCountBefore);
	TestEqual(TEXT("blocked replace preview leaves package dirty flag unchanged"), Blueprint->GetOutermost()->IsDirty(), bDirtyBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteReplaceGraphScopeEntrySelectorUnsupportedTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.ReplaceGraphScopeEntrySelectorUnsupported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteReplaceGraphScopeEntrySelectorUnsupportedTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("ReplaceGraphEntrySelector"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	const UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const int32 NodeCountBefore = Graph->Nodes.Num();
	const bool bDirtyBefore = Blueprint->GetOutermost()->IsDirty();

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperGraphSnapshotService SnapshotService;
	FBlueprintHelperReplaceBlueprintGraphService ReplaceService(
		Resolver,
		BlockIdService,
		OwnershipService,
		SnapshotService);

	const FBlueprintHelperToolResultBase Result = ReplaceService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeReplaceGraphScopeEntrySelectorPreviewPayload(
			Blueprint->GetPathName(),
			Graph->GetName()));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertBlockedDryRunFailure(
		*this,
		Result,
		TEXT("replace_blueprint_graph"),
		TEXT("graph_scope_entry_selector_unsupported"),
		TEXT("selector.entry_name"));
	TestEqual(TEXT("blocked graph-scope replace leaves graph nodes unchanged"), Graph->Nodes.Num(), NodeCountBefore);
	TestEqual(TEXT("blocked graph-scope replace leaves package dirty flag unchanged"), Blueprint->GetOutermost()->IsDirty(), bDirtyBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchBlockedDryRunErrorEnvelopeTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchBlockedDryRunErrorEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchBlockedDryRunErrorEnvelopeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchBlockedDryRun"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	const int32 UbergraphCountBefore = Blueprint->UbergraphPages.Num();
	const bool bDirtyBefore = Blueprint->GetOutermost()->IsDirty();

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchPreviewPayload(Blueprint->GetPathName(), TEXT("MissingGraph")));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertBlockedDryRunFailure(
		*this,
		Result,
		TEXT("patch_blueprint_graph"),
		TEXT("target_graph_not_found"),
		TEXT("target.graph"));
	TestEqual(TEXT("blocked patch preview leaves graph count unchanged"), Blueprint->UbergraphPages.Num(), UbergraphCountBefore);
	TestEqual(TEXT("blocked patch preview leaves package dirty flag unchanged"), Blueprint->GetOutermost()->IsDirty(), bDirtyBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchConnectPinsDryRunResolvesEndpointsTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchConnectPinsDryRunResolvesEndpoints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchConnectPinsDryRunResolvesEndpointsTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchConnectPinsDryRun"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Source = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("PatchConnectSource"));
	UK2Node_CallFunction* Target = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	TestNotNull(TEXT("source node exists"), Source);
	TestNotNull(TEXT("target node exists"), Target);
	if (!Source || !Target)
	{
		return false;
	}
	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("PatchConnectPinsDryRun"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Source, BlockId);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Target, BlockId);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchConnectPinsPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			Source->GetName(),
			TEXT("then"),
			Target->GetName(),
			TEXT("execute"),
			true,
			FString(),
			FString(),
			BlockId));

	TestTrue(TEXT("connect_pins dry-run succeeds"), Result.bOk);
	TestEqual(TEXT("operation is patch_blueprint_graph"), Result.Operation, FString(TEXT("patch_blueprint_graph")));
	TestFalse(TEXT("dry-run does not create source links"), Source->FindPin(TEXT("then")) && Source->FindPin(TEXT("then"))->LinkedTo.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchConnectPinsDryRunRequiresSourceEndpointTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchConnectPinsDryRunRequiresSourceEndpoint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchConnectPinsDryRunRequiresSourceEndpointTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchConnectPinsDryRunMissingSource"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Source = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("PatchConnectMissingSource"));
	UK2Node_CallFunction* Target = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	TestNotNull(TEXT("source node exists"), Source);
	TestNotNull(TEXT("target node exists"), Target);
	if (!Source || !Target)
	{
		return false;
	}
	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("PatchConnectPinsMissingSource"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Source, BlockId);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Target, BlockId);

	TSharedRef<FJsonObject> Payload = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchConnectPinsPayload(
		Blueprint->GetPathName(),
		Graph->GetName(),
		TEXT(""),
		TEXT(""),
		Target->GetName(),
		TEXT("execute"),
		true,
		FString(),
		FString(),
		BlockId);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(Payload);

	TestFalse(TEXT("connect_pins dry-run fails without source endpoint"), Result.bOk);
	TestTrue(TEXT("dry-run carries source endpoint error"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("dry-run source endpoint code"), Result.Error->Code, FString(TEXT("source_node_required")));
		TestEqual(TEXT("dry-run source endpoint field"), Result.Error->Field, FString(TEXT("patch.source_node_ref")));
	}
	TestTrue(TEXT("dry-run reports source endpoint requirement"),
		Result.Error.IsSet() && Result.Error->Message.Contains(TEXT("patch.source_node_ref")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchConnectPinsExecuteRequiresSourceEndpointTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchConnectPinsExecuteRequiresSourceEndpoint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchConnectPinsExecuteRequiresSourceEndpointTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchConnectPinsExecuteMissingSource"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CallFunction* Target = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	TestNotNull(TEXT("target node exists"), Target);
	if (!Target)
	{
		return false;
	}
	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("PatchConnectPinsExecuteMissingSource"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Target, BlockId);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchConnectPinsPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			TEXT(""),
			TEXT(""),
			Target->GetName(),
			TEXT("execute"),
			false,
			FString(),
			FString(),
			BlockId));

	TestFalse(TEXT("connect_pins execute fails without source endpoint"), Result.bOk);
	TestTrue(TEXT("execute carries source endpoint error"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("execute source endpoint code"), Result.Error->Code, FString(TEXT("source_node_required")));
		TestEqual(TEXT("execute source endpoint stage"), Result.Error->Stage, EBlueprintHelperToolStage::Preflight);
		TestEqual(TEXT("execute source endpoint field"), Result.Error->Field, FString(TEXT("patch.source_node_ref")));
		TestTrue(TEXT("execute source endpoint message mentions missing source"), Result.Error->Message.Contains(TEXT("patch.source_node_ref")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchConnectPinsRejectsSourceNodePathTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchConnectPinsRejectsSourceNodePath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchConnectPinsRejectsSourceNodePathTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchConnectPinsRejectSourceNodePath"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Source = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("PatchConnectSourcePath"));
	UK2Node_CallFunction* Target = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	TestNotNull(TEXT("source node exists"), Source);
	TestNotNull(TEXT("target node exists"), Target);
	if (!Source || !Target)
	{
		return false;
	}
	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("PatchConnectPinsRejectSourceNodePath"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Source, BlockId);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Target, BlockId);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchConnectPinsPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			Source->GetName(),
			TEXT("then"),
			Target->GetName(),
			TEXT("execute"),
			true,
			TEXT("MissingSourcePath"),
			FString(),
			BlockId));

	TestFalse(TEXT("connect_pins dry-run rejects source node path"), Result.bOk);
	TestTrue(TEXT("dry-run carries source node path contract error"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("source node path contract code"), Result.Error->Code, FString(TEXT("unsupported_graph_write_anchor")));
		TestEqual(TEXT("source node path contract field"), Result.Error->Field, FString(TEXT("patch.source_node_path")));
		TestTrue(TEXT("source node path message mentions path field"), Result.Error->Message.Contains(TEXT("patch.source_node_path")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchConnectPinsRejectsSourcePinPathTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchConnectPinsRejectsSourcePinPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchConnectPinsRejectsSourcePinPathTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchConnectPinsRejectSourcePinPath"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Source = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("PatchConnectSourcePinPath"));
	UK2Node_CallFunction* Target = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	TestNotNull(TEXT("source node exists"), Source);
	TestNotNull(TEXT("target node exists"), Target);
	if (!Source || !Target)
	{
		return false;
	}
	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("PatchConnectPinsRejectSourcePinPath"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Source, BlockId);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Target, BlockId);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchConnectPinsPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			Source->GetName(),
			TEXT("then"),
			Target->GetName(),
			TEXT("execute"),
			true,
			FString(),
			TEXT("MissingSourcePinPath"),
			BlockId));

	TestFalse(TEXT("connect_pins dry-run rejects source pin path"), Result.bOk);
	TestTrue(TEXT("dry-run carries source pin path contract error"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("source pin path contract code"), Result.Error->Code, FString(TEXT("unsupported_graph_write_anchor")));
		TestEqual(TEXT("source pin path contract field"), Result.Error->Field, FString(TEXT("patch.source_pin_path")));
		TestTrue(TEXT("source pin path message mentions path field"), Result.Error->Message.Contains(TEXT("patch.source_pin_path")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchConnectPinsExecutesViaCoordinatorTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchConnectPinsExecutesViaCoordinator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchConnectPinsExecutesViaCoordinatorTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchConnectPinsExecute"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Source = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("PatchConnectExecSource"));
	UK2Node_CallFunction* Target = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	UEdGraphPin* SourceThen = Source ? Source->FindPin(TEXT("then")) : nullptr;
	UEdGraphPin* TargetExec = Target ? Target->FindPin(TEXT("execute")) : nullptr;
	TestNotNull(TEXT("source then pin exists"), SourceThen);
	TestNotNull(TEXT("target execute pin exists"), TargetExec);
	if (!SourceThen || !TargetExec)
	{
		return false;
	}
	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("PatchConnectPinsExecute"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Source, BlockId);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Target, BlockId);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchConnectPinsPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			Source->GetName(),
			TEXT("then"),
			Target->GetName(),
			TEXT("execute"),
			false,
			FString(),
			FString(),
			BlockId));

	TestTrue(TEXT("connect_pins execute succeeds"), Result.bOk);
	TestEqual(TEXT("source then has one linked pin"), SourceThen->LinkedTo.Num(), 1);
	TestTrue(TEXT("source then links to target execute"), SourceThen->LinkedTo.Contains(TargetExec));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchConnectPinsRejectsCrossBlockSourceRefTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchConnectPinsRejectsCrossBlockSourceRef",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchConnectPinsRejectsCrossBlockSourceRefTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchConnectPinsCrossBlockSource"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Source = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("PatchConnectCrossBlockSource"));
	UK2Node_CallFunction* Target = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	TestNotNull(TEXT("source node exists"), Source);
	TestNotNull(TEXT("target node exists"), Target);
	if (!Source || !Target)
	{
		return false;
	}

	const FString SourceBlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("PatchConnectSourceBlock"));
	const FString TargetBlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("PatchConnectTargetBlock"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Source, SourceBlockId);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Target, TargetBlockId);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchConnectPinsPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			Source->GetName(),
			TEXT("then"),
			Target->GetName(),
			TEXT("execute"),
			true,
			FString(),
			FString(),
			TargetBlockId,
			SourceBlockId));

	TestFalse(TEXT("cross-block source ref is rejected"), Result.bOk);
	TestTrue(TEXT("cross-block rejection carries owned patch code"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("cross-block source code"), Result.Error->Code, FString(TEXT("owned_patch_cross_block_disallowed")));
		TestEqual(TEXT("cross-block source field"), Result.Error->Field, FString(TEXT("patch.source_block_id")));
	}
	TestFalse(TEXT("dry-run does not connect cross-block pins"), Source->FindPin(TEXT("then")) && Source->FindPin(TEXT("then"))->LinkedTo.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchDisconnectLinkRejectsExpectedOldStateTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchDisconnectLinkRejectsExpectedOldState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchDisconnectLinkRejectsExpectedOldStateTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchDisconnectRejectExpectedOldState"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Source = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("PatchDisconnectSource"));
	UK2Node_CallFunction* Target = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	TestTrue(TEXT("existing link is created"), FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ConnectFirstExecPins(Source, Target));
	UEdGraphPin* SourceThen = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(Source, EGPD_Output);
	UEdGraphPin* TargetExec = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(Target, EGPD_Input);
	TestNotNull(TEXT("source exec pin exists"), SourceThen);
	TestNotNull(TEXT("target exec pin exists"), TargetExec);
	if (!SourceThen || !TargetExec)
	{
		return false;
	}

	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("PatchDisconnectRejectExpectedOldState"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Source, BlockId);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Target, BlockId);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchDisconnectLinkPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			Source->GetName(),
			TEXT("then"),
			Target->GetName(),
			TEXT("execute"),
			BlockId,
			false,
			true,
			Source->GetName(),
			TEXT("then"),
			TEXT("UnexpectedTarget"),
			TEXT("execute")));

	TestFalse(TEXT("disconnect_link rejects expected_old_state"), Result.bOk);
	TestTrue(TEXT("expected_old_state rejection carries owned patch code"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("expected_old_state rejection code"), Result.Error->Code, FString(TEXT("redundant_owned_patch_expected_old_state")));
		TestEqual(TEXT("expected_old_state rejection field"), Result.Error->Field, FString(TEXT("expected_old_state")));
	}
	TestTrue(TEXT("expected_old_state rejection leaves original link intact"), SourceThen->LinkedTo.Contains(TargetExec));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchDisconnectLinkAcceptsGuidEndpointRefsTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchDisconnectLinkAcceptsGuidEndpointRefs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchDisconnectLinkAcceptsGuidEndpointRefsTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchDisconnectGuidEndpointRefs"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Source = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("PatchDisconnectGuidSource"));
	UK2Node_CallFunction* Target = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	TestTrue(TEXT("existing link is created"), FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ConnectFirstExecPins(Source, Target));
	UEdGraphPin* SourceThen = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(Source, EGPD_Output);
	UEdGraphPin* TargetExec = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(Target, EGPD_Input);
	TestNotNull(TEXT("source exec pin exists"), SourceThen);
	TestNotNull(TEXT("target exec pin exists"), TargetExec);
	if (!Source || !Target || !SourceThen || !TargetExec)
	{
		return false;
	}

	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("PatchDisconnectGuidEndpointRefs"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Source, BlockId);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Target, BlockId);
	const FString SourceGuidRef = Source->NodeGuid.ToString(EGuidFormats::Digits);
	const FString TargetGuidRef = Target->NodeGuid.ToString(EGuidFormats::Digits);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchDisconnectLinkPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			SourceGuidRef,
			TEXT("then"),
			TargetGuidRef,
			TEXT("execute"),
			BlockId,
			false));

	TestTrue(TEXT("disconnect_link accepts GUID endpoint refs"), Result.bOk);
	TestFalse(TEXT("GUID endpoint refs disconnect original link"), SourceThen->LinkedTo.Contains(TargetExec));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchDisconnectLinkRejectsIndexedLinkRefTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchDisconnectLinkRejectsIndexedLinkRef",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchDisconnectLinkRejectsIndexedLinkRefTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchDisconnectRejectIndexedLinkRef"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Source = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("PatchDisconnectIndexedSource"));
	UK2Node_CallFunction* Target = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	TestTrue(TEXT("existing link is created"), FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ConnectFirstExecPins(Source, Target));
	UEdGraphPin* SourceThen = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(Source, EGPD_Output);
	UEdGraphPin* TargetExec = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(Target, EGPD_Input);
	TestNotNull(TEXT("source exec pin exists"), SourceThen);
	TestNotNull(TEXT("target exec pin exists"), TargetExec);
	if (!Source || !Target || !SourceThen || !TargetExec)
	{
		return false;
	}

	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("PatchDisconnectRejectIndexedLinkRef"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Source, BlockId);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Target, BlockId);

	TSharedRef<FJsonObject> Payload =
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchDisconnectLinkPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			Source->GetName(),
			TEXT("then"),
			Target->GetName(),
			TEXT("execute"),
			BlockId,
			false);
	const TSharedPtr<FJsonObject> PatchedRef = Payload->GetObjectField(TEXT("patched_ref"));
	TestTrue(TEXT("patched_ref exists"), PatchedRef.IsValid());
	if (!PatchedRef.IsValid())
	{
		return false;
	}
	PatchedRef->SetStringField(TEXT("link_ref"), TEXT("links[0]"));

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(Payload);

	TestFalse(TEXT("disconnect_link rejects indexed link_ref"), Result.bOk);
	TestTrue(TEXT("indexed link_ref rejection carries contract code"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("indexed link_ref rejection code"), Result.Error->Code, FString(TEXT("unsupported_graph_write_anchor")));
		TestEqual(TEXT("indexed link_ref rejection field"), Result.Error->Field, FString(TEXT("patched_ref.link_ref")));
	}
	TestTrue(TEXT("indexed link_ref rejection leaves original link intact"), SourceThen->LinkedTo.Contains(TargetExec));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchReplaceLinkUsesReplacementRefTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchReplaceLinkUsesReplacementRef",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchReplaceLinkUsesReplacementRefTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchReplaceLinkReplacementRef"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Source = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("PatchReplaceSource"));
	UK2Node_CallFunction* OldTarget = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	UK2Node_CallFunction* ReplacementTarget = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	TestTrue(TEXT("existing link is created"), FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ConnectFirstExecPins(Source, OldTarget));
	UEdGraphPin* SourceThen = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(Source, EGPD_Output);
	UEdGraphPin* OldExec = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(OldTarget, EGPD_Input);
	UEdGraphPin* ReplacementExec = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(ReplacementTarget, EGPD_Input);
	TestNotNull(TEXT("source exec pin exists"), SourceThen);
	TestNotNull(TEXT("old target exec pin exists"), OldExec);
	TestNotNull(TEXT("replacement exec pin exists"), ReplacementExec);
	if (!SourceThen || !OldExec || !ReplacementExec)
	{
		return false;
	}

	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("PatchReplaceLinkReplacementRef"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Source, BlockId);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(OldTarget, BlockId);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(ReplacementTarget, BlockId);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchReplaceLinkPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			Source->GetName(),
			TEXT("then"),
			OldTarget->GetName(),
			TEXT("execute"),
			ReplacementTarget->GetName(),
			TEXT("execute"),
			BlockId,
			false));

	TestTrue(TEXT("replace_link execute succeeds"), Result.bOk);
	TestFalse(TEXT("old link is removed"), SourceThen->LinkedTo.Contains(OldExec));
	TestTrue(TEXT("replacement link is created"), SourceThen->LinkedTo.Contains(ReplacementExec));
	const TArray<TSharedPtr<FJsonValue>>* BlockRefs = nullptr;
	TestTrue(TEXT("replace_link result publishes block refs"),
		Result.Data.IsValid() && Result.Data->TryGetArrayField(TEXT("block_refs"), BlockRefs));
	TestEqual(TEXT("replace_link result publishes owning block"),
		BlockRefs && BlockRefs->Num() > 0 ? (*BlockRefs)[0]->AsString() : FString(),
		BlockId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchDisconnectLinkExecutesViaCoordinatorTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchDisconnectLinkExecutesViaCoordinator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchDisconnectLinkExecutesViaCoordinatorTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchDisconnectLinkExecute"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Source = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("PatchDisconnectExecSource"));
	UK2Node_CallFunction* Target = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	TestTrue(TEXT("existing link is created"), FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ConnectFirstExecPins(Source, Target));
	UEdGraphPin* SourceThen = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(Source, EGPD_Output);
	UEdGraphPin* TargetExec = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(Target, EGPD_Input);
	TestNotNull(TEXT("source exec pin exists"), SourceThen);
	TestNotNull(TEXT("target exec pin exists"), TargetExec);
	if (!SourceThen || !TargetExec)
	{
		return false;
	}

	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("PatchDisconnectLinkExecute"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Source, BlockId);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Target, BlockId);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchDisconnectLinkPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			Source->GetName(),
			TEXT("then"),
			Target->GetName(),
			TEXT("execute"),
			BlockId,
			false));

	TestTrue(TEXT("disconnect_link execute succeeds"), Result.bOk);
	TestFalse(TEXT("source no longer links to target"), SourceThen->LinkedTo.Contains(TargetExec));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchDisconnectLinkRejectsUserAuthoredEndpointTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchDisconnectLinkRejectsUserAuthoredEndpoint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchDisconnectLinkRejectsUserAuthoredEndpointTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchDisconnectUserAuthoredEndpoint"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Source = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("PatchDisconnectOwnedSource"));
	UK2Node_CallFunction* UserAuthoredTarget = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	TestTrue(TEXT("existing link is created"), FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ConnectFirstExecPins(Source, UserAuthoredTarget));
	UEdGraphPin* SourceThen = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(Source, EGPD_Output);
	UEdGraphPin* TargetExec = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(UserAuthoredTarget, EGPD_Input);
	TestNotNull(TEXT("source exec pin exists"), SourceThen);
	TestNotNull(TEXT("target exec pin exists"), TargetExec);
	if (!SourceThen || !TargetExec)
	{
		return false;
	}

	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("PatchDisconnectUserAuthoredEndpoint"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Source, BlockId);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchDisconnectLinkPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			Source->GetName(),
			TEXT("then"),
			UserAuthoredTarget->GetName(),
			TEXT("execute"),
			BlockId,
			false));

	TestFalse(TEXT("disconnect_link rejects user-authored endpoint"), Result.bOk);
	TestTrue(TEXT("original link remains after rejection"), SourceThen->LinkedTo.Contains(TargetExec));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchReplaceLinkRejectsMissingReplacementRefTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchReplaceLinkRejectsMissingReplacementRef",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchReplaceLinkRejectsMissingReplacementRefTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchReplaceMissingReplacement"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Source = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("PatchReplaceMissingSource"));
	UK2Node_CallFunction* OldTarget = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	TestTrue(TEXT("existing link is created"), FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ConnectFirstExecPins(Source, OldTarget));
	UEdGraphPin* SourceThen = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(Source, EGPD_Output);
	UEdGraphPin* OldExec = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(OldTarget, EGPD_Input);
	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("PatchReplaceMissingReplacement"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Source, BlockId);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(OldTarget, BlockId);

	TSharedRef<FJsonObject> Payload = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchDisconnectLinkPayload(
		Blueprint->GetPathName(),
		Graph->GetName(),
		Source->GetName(),
		TEXT("then"),
		OldTarget->GetName(),
		TEXT("execute"),
		BlockId,
		false);
	Payload->SetStringField(TEXT("patch_type"), TEXT("replace_link"));
	const TSharedPtr<FJsonObject> Target = Payload->GetObjectField(TEXT("target"));
	if (Target.IsValid())
	{
		Target->SetStringField(TEXT("patch_scope"), TEXT("replace_link"));
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(Payload);

	TestFalse(TEXT("replace_link rejects missing replacement ref"), Result.bOk);
	TestTrue(TEXT("missing replacement ref reports owned patch code"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("missing replacement code"), Result.Error->Code, FString(TEXT("owned_patch_replacement_ref_required")));
	}
	TestTrue(TEXT("original link remains after missing replacement rejection"), SourceThen && OldExec && SourceThen->LinkedTo.Contains(OldExec));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchReplaceLinkRejectsCrossBlockReplacementTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchReplaceLinkRejectsCrossBlockReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchReplaceLinkRejectsCrossBlockReplacementTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchReplaceCrossBlockReplacement"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Source = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("PatchReplaceCrossSource"));
	UK2Node_CallFunction* OldTarget = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	UK2Node_CallFunction* ReplacementTarget = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	TestTrue(TEXT("existing link is created"), FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ConnectFirstExecPins(Source, OldTarget));
	UEdGraphPin* SourceThen = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(Source, EGPD_Output);
	UEdGraphPin* OldExec = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(OldTarget, EGPD_Input);

	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("PatchReplaceCrossBlock"));
	const FString ReplacementBlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("PatchReplaceOtherBlock"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Source, BlockId);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(OldTarget, BlockId);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(ReplacementTarget, ReplacementBlockId);

	TSharedRef<FJsonObject> Payload = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchReplaceLinkPayload(
		Blueprint->GetPathName(),
		Graph->GetName(),
		Source->GetName(),
		TEXT("then"),
		OldTarget->GetName(),
		TEXT("execute"),
		ReplacementTarget->GetName(),
		TEXT("execute"),
		BlockId,
		false);
	const TSharedPtr<FJsonObject> Patch = Payload->GetObjectField(TEXT("patch"));
	if (Patch.IsValid())
	{
		Patch->SetStringField(TEXT("replacement_block_id"), ReplacementBlockId);
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(Payload);

	TestFalse(TEXT("replace_link rejects cross-block replacement"), Result.bOk);
	TestTrue(TEXT("cross-block replacement reports owned patch code"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("cross-block replacement code"), Result.Error->Code, FString(TEXT("owned_patch_cross_block_disallowed")));
		TestEqual(TEXT("cross-block replacement field"), Result.Error->Field, FString(TEXT("patch.replacement_block_id")));
	}
	TestTrue(TEXT("original link remains after cross-block replacement rejection"), SourceThen && OldExec && SourceThen->LinkedTo.Contains(OldExec));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchDeleteOwnedNodeRemovesNodeTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchDeleteOwnedNodeRemovesNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchDeleteOwnedNodeRemovesNodeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchDeleteOwnedNode"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Entry = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("PatchDeleteEntry"));
	UK2Node_CallFunction* DeletedTarget = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	TestTrue(TEXT("entry links to delete target before patch"), FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ConnectFirstExecPins(Entry, DeletedTarget));
	UEdGraphPin* EntryThen = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(Entry, EGPD_Output);
	UEdGraphPin* DeletedExec = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(DeletedTarget, EGPD_Input);
	TestNotNull(TEXT("delete target exists"), DeletedTarget);
	TestNotNull(TEXT("entry then pin exists"), EntryThen);
	TestNotNull(TEXT("deleted exec pin exists"), DeletedExec);
	if (!Entry || !DeletedTarget)
	{
		return false;
	}
	const FString DeletedNodeName = DeletedTarget->GetName();
	const int32 NodeCountBefore = Graph->Nodes.Num();
	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("PatchDeleteOwnedNode"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Entry, BlockId);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(DeletedTarget, BlockId);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchDeleteOwnedNodePayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			DeletedNodeName,
			BlockId,
			false));

	TestTrue(TEXT("delete_owned_node execute succeeds"), Result.bOk);
	TestEqual(TEXT("owned node is removed"), Graph->Nodes.Num(), NodeCountBefore - 1);
	TestFalse(TEXT("deleted node no longer exists in graph"),
		Graph->Nodes.ContainsByPredicate([&DeletedNodeName](UEdGraphNode* Node)
		{
			return Node && Node->GetName() == DeletedNodeName;
		}));
	TestTrue(TEXT("entry then no longer references deleted pin"), EntryThen && !EntryThen->LinkedTo.Contains(DeletedExec));
	const TArray<TSharedPtr<FJsonValue>>* BlockRefs = nullptr;
	TestTrue(TEXT("delete_owned_node result publishes block refs"),
		Result.Data.IsValid() && Result.Data->TryGetArrayField(TEXT("block_refs"), BlockRefs));
	TestEqual(TEXT("delete_owned_node result publishes owning block"),
		BlockRefs && BlockRefs->Num() > 0 ? (*BlockRefs)[0]->AsString() : FString(),
		BlockId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchDeleteOwnedNodeRejectsEntryNodeTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchDeleteOwnedNodeRejectsEntryNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchDeleteOwnedNodeRejectsEntryNodeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchDeleteEntryNode"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Entry = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("PatchDeleteEntryNode"));
	TestNotNull(TEXT("entry node exists"), Entry);
	if (!Entry)
	{
		return false;
	}
	const int32 NodeCountBefore = Graph->Nodes.Num();
	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("PatchDeleteEntryNode"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(Entry, BlockId);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchDeleteOwnedNodePayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			Entry->GetName(),
			BlockId,
			false));

	TestFalse(TEXT("delete_owned_node rejects entry node"), Result.bOk);
	TestTrue(TEXT("entry-node rejection carries owned delete code"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("entry-node delete code"), Result.Error->Code, FString(TEXT("owned_delete_entry_node_disallowed")));
		TestEqual(TEXT("entry-node delete field"), Result.Error->Field, FString(TEXT("patched_ref.node_ref")));
	}
	TestEqual(TEXT("entry node remains in graph"), Graph->Nodes.Num(), NodeCountBefore);
	TestTrue(TEXT("entry node is still present"), Graph->Nodes.Contains(Entry));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchDeleteOwnedNodeRejectsUserAuthoredNodeTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchDeleteOwnedNodeRejectsUserAuthoredNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchDeleteOwnedNodeRejectsUserAuthoredNodeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchDeleteUserAuthoredNode"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CallFunction* UserAuthoredNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	TestNotNull(TEXT("user-authored node exists"), UserAuthoredNode);
	if (!UserAuthoredNode)
	{
		return false;
	}
	const int32 NodeCountBefore = Graph->Nodes.Num();
	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("PatchDeleteUserAuthoredNode"));

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchDeleteOwnedNodePayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			UserAuthoredNode->GetName(),
			BlockId,
			false));

	TestFalse(TEXT("delete_owned_node rejects user-authored node"), Result.bOk);
	TestEqual(TEXT("user-authored delete leaves graph unchanged"), Graph->Nodes.Num(), NodeCountBefore);
	TestTrue(TEXT("user-authored node remains in graph"), Graph->Nodes.Contains(UserAuthoredNode));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchDeleteOwnedNodeRejectsLifecycleRootTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchDeleteOwnedNodeRejectsLifecycleRoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchDeleteOwnedNodeRejectsLifecycleRootTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchDeleteLifecycleRoot"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CallFunction* LifecycleRootNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	TestNotNull(TEXT("lifecycle root node exists"), LifecycleRootNode);
	if (!LifecycleRootNode)
	{
		return false;
	}
	const int32 NodeCountBefore = Graph->Nodes.Num();
	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("PatchDeleteLifecycleRoot"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(LifecycleRootNode, BlockId);
	if (UPackage* Package = LifecycleRootNode->GetOutermost())
	{
		FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
		MetaData.SetValue(LifecycleRootNode, TEXT("BlueprintHelperLifecycleRoot"), TEXT("true"));
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchDeleteOwnedNodePayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			LifecycleRootNode->GetName(),
			BlockId,
			false));

	TestFalse(TEXT("delete_owned_node rejects lifecycle root"), Result.bOk);
	TestTrue(TEXT("lifecycle-root rejection carries owned delete code"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("lifecycle-root delete code"), Result.Error->Code, FString(TEXT("owned_delete_lifecycle_root_disallowed")));
	}
	TestEqual(TEXT("lifecycle root delete leaves graph unchanged"), Graph->Nodes.Num(), NodeCountBefore);
	TestTrue(TEXT("lifecycle root remains in graph"), Graph->Nodes.Contains(LifecycleRootNode));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchDeleteOwnedNodeRejectsBreakLinksFalseTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchDeleteOwnedNodeRejectsBreakLinksFalse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchDeleteOwnedNodeRejectsBreakLinksFalseTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("PatchDeleteBreakLinksFalse"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CallFunction* TargetNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	TestNotNull(TEXT("target node exists"), TargetNode);
	if (!TargetNode)
	{
		return false;
	}
	const int32 NodeCountBefore = Graph->Nodes.Num();
	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("PatchDeleteBreakLinksFalse"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(TargetNode, BlockId);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePatchDeleteOwnedNodePayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			TargetNode->GetName(),
			BlockId,
			false,
			false));

	TestFalse(TEXT("delete_owned_node rejects break_links=false"), Result.bOk);
	TestTrue(TEXT("break_links=false rejection carries policy code"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("break_links=false delete code"), Result.Error->Code, FString(TEXT("owned_delete_policy_disallowed")));
	}
	TestEqual(TEXT("break_links=false delete leaves graph unchanged"), Graph->Nodes.Num(), NodeCountBefore);
	TestTrue(TEXT("target node remains in graph"), Graph->Nodes.Contains(TargetNode));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteMergeBlockedDryRunErrorEnvelopeTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.MergeBlockedDryRunErrorEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteMergeBlockedDryRunErrorEnvelopeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("MergeBlockedDryRun"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	const int32 UbergraphCountBefore = Blueprint->UbergraphPages.Num();
	const bool bDirtyBefore = Blueprint->GetOutermost()->IsDirty();

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperMergeBlueprintGraphService MergeService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = MergeService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeMergePreviewPayload(Blueprint->GetPathName(), TEXT("MissingGraph")));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertBlockedDryRunFailure(
		*this,
		Result,
		TEXT("merge_blueprint_graph"),
		TEXT("target_graph_not_found"),
		TEXT("target.graph"));
	TestEqual(TEXT("blocked merge preview leaves graph count unchanged"), Blueprint->UbergraphPages.Num(), UbergraphCountBefore);
	TestEqual(TEXT("blocked merge preview leaves package dirty flag unchanged"), Blueprint->GetOutermost()->IsDirty(), bDirtyBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteLogicJsonNodeIndexRefResolvesTest,
	"BlueprintHelper.GraphWrite.LogicJsonPath.NodeIndexRefResolves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteLogicJsonNodeIndexRefResolvesTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("LogicJsonNodeIndexRef"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("IndexRefFirst"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("IndexRefSecond"));

	FBlueprintHelperLogicJsonPathService PathService;
	UEdGraphNode* ResolvedNode = nullptr;
	FBlueprintHelperPatchResolveError ResolveError;

	TestTrue(TEXT("LogicJson node_ref nodes[0] resolves to graph node index 0"),
		PathService.ResolveNode(Graph, TEXT("nodes[0]"), FString(), ResolvedNode, ResolveError));
	TestTrue(TEXT("nodes[0] resolves exact first graph node"), ResolvedNode == Graph->Nodes[0]);

	ResolvedNode = nullptr;
	TestTrue(TEXT("LogicJson node_ref nodes[1] resolves to graph node index 1"),
		PathService.ResolveNode(Graph, TEXT("nodes[1]"), FString(), ResolvedNode, ResolveError));
	TestTrue(TEXT("nodes[1] resolves exact second graph node"), ResolvedNode == Graph->Nodes[1]);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteReplaceCustomEventBodyReconnectsEntryExecTest,
	"BlueprintHelper.GraphWrite.Replace.CustomEventBodyReconnectsEntryExec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteReplaceCustomEventBodyReconnectsEntryExecTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("ReplaceReconnectsEntryExec"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* EntryNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("SmokeCustomEvent"));
	TestNotNull(TEXT("custom event entry is created"), EntryNode);
	if (!EntryNode)
	{
		return false;
	}

	UK2Node_CallFunction* OldPrintNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	TestNotNull(TEXT("old PrintString body node is created"), OldPrintNode);
	TestTrue(TEXT("old custom event body is linked before replace"),
		OldPrintNode && FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ConnectFirstExecPins(EntryNode, OldPrintNode));
	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("SmokeCustomEvent"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(EntryNode, BlockId);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(OldPrintNode, BlockId);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperGraphSnapshotService SnapshotService;
	FBlueprintHelperReplaceBlueprintGraphService ReplaceService(
		Resolver,
		BlockIdService,
		OwnershipService,
		SnapshotService);

	const FBlueprintHelperToolResultBase Result = ReplaceService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeReplaceExecutePayload(Blueprint->GetPathName(), Graph->GetName()));

	TestTrue(TEXT("replace custom event body succeeds"), Result.bOk);
	TestEqual(TEXT("replace status is applied"), Result.Status, EBlueprintHelperToolStatus::Applied);

	UEdGraphPin* EntryExecOut = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(EntryNode, EGPD_Output);
	TestNotNull(TEXT("custom event has output exec pin"), EntryExecOut);
	TestTrue(TEXT("custom event output exec is linked after replace"),
		EntryExecOut && EntryExecOut->LinkedTo.Num() > 0);

	bool bLinkedToPrintString = false;
	UK2Node_CallFunction* ReplacementPrintNode = nullptr;
	if (EntryExecOut)
	{
		for (UEdGraphPin* LinkedPin : EntryExecOut->LinkedTo)
		{
			UK2Node_CallFunction* CallNode = LinkedPin ? Cast<UK2Node_CallFunction>(LinkedPin->GetOwningNode()) : nullptr;
			if (CallNode && CallNode->GetFunctionName().ToString().Equals(TEXT("PrintString"), ESearchCase::IgnoreCase))
			{
				bLinkedToPrintString = true;
				ReplacementPrintNode = CallNode;
				break;
			}
		}
	}
	TestTrue(TEXT("custom event output exec links to replacement PrintString"), bLinkedToPrintString);
	TestTrue(TEXT("replacement body node keeps BlueprintHelper ownership"), FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::NodeHasBlueprintHelperBlockId(ReplacementPrintNode, BlockId));
	const TArray<TSharedPtr<FJsonValue>>* ReplaceBlockRefs = nullptr;
	TestTrue(TEXT("replace result publishes block refs"),
		Result.Data.IsValid() && Result.Data->TryGetArrayField(TEXT("block_refs"), ReplaceBlockRefs));
	TestEqual(TEXT("replace result publishes replaced block id"),
		ReplaceBlockRefs && ReplaceBlockRefs->Num() > 0 ? (*ReplaceBlockRefs)[0]->AsString() : FString(),
		BlockId);
	TestTrue(TEXT("exported graph contains event to replacement PrintString exec link"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ExportHasExecLinkFromCustomEventToFunction(Graph, TEXT("SmokeCustomEvent"), TEXT("PrintString")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteReplaceFunctionBodyReconnectsEntryExecTest,
	"BlueprintHelper.GraphWrite.Replace.FunctionBodyReconnectsEntryExec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteReplaceFunctionBodyReconnectsEntryExecTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("ReplaceFunctionBodyReconnectsEntryExec"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	const FString FunctionName = TEXT("ComputeFunctionBodySmoke");
	UEdGraph* FunctionGraph = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteFunctionGraph(Blueprint, FunctionName);
	TestNotNull(TEXT("function graph is created"), FunctionGraph);
	UK2Node_FunctionEntry* EntryNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindGraphWriteFunctionEntry(FunctionGraph);
	TestNotNull(TEXT("function entry is created"), EntryNode);
	if (!FunctionGraph || !EntryNode)
	{
		return false;
	}

	UK2Node_CallFunction* OldPrintNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(FunctionGraph);
	TestNotNull(TEXT("old PrintString body node is created"), OldPrintNode);
	TestTrue(TEXT("old function body is linked before replace"),
		OldPrintNode && FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ConnectFirstExecPins(EntryNode, OldPrintNode));

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperGraphSnapshotService SnapshotService;
	FBlueprintHelperReplaceBlueprintGraphService ReplaceService(
		Resolver,
		BlockIdService,
		OwnershipService,
		SnapshotService);

	const FBlueprintHelperToolResultBase Result = ReplaceService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeReplaceFunctionBodyExecutePayload(
			Blueprint->GetPathName(),
			FunctionName));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddToolResultFailureDetail(
		*this,
		TEXT("replace function body"),
		Result);
	TestTrue(TEXT("replace function body succeeds"), Result.bOk);
	TestEqual(TEXT("replace function body status is applied"), Result.Status, EBlueprintHelperToolStatus::Applied);

	UEdGraphPin* EntryExecOut = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(EntryNode, EGPD_Output);
	TestNotNull(TEXT("function entry has output exec pin"), EntryExecOut);
	TestTrue(TEXT("function entry output exec is linked after replace"),
		EntryExecOut && EntryExecOut->LinkedTo.Num() > 0);

	bool bLinkedToPrintString = false;
	if (EntryExecOut)
	{
		for (UEdGraphPin* LinkedPin : EntryExecOut->LinkedTo)
		{
			UK2Node_CallFunction* CallNode = LinkedPin ? Cast<UK2Node_CallFunction>(LinkedPin->GetOwningNode()) : nullptr;
			if (CallNode && CallNode->GetFunctionName().ToString().Equals(TEXT("PrintString"), ESearchCase::IgnoreCase))
			{
				bLinkedToPrintString = true;
				break;
			}
		}
	}
	TestTrue(TEXT("function entry output exec links to replacement PrintString"), bLinkedToPrintString);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteReplaceMacroBodyReconnectsEntryAndExitExecTest,
	"BlueprintHelper.GraphWrite.Replace.MacroBodyReconnectsEntryAndExitExec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteReplaceMacroBodyReconnectsEntryAndExitExecTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("ReplaceMacroBodyReconnectsEntryAndExitExec"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	const FString MacroName = TEXT("ClampScoreMacro");
	UEdGraph* MacroGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FName(*MacroName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddMacroGraph(Blueprint, MacroGraph, true, nullptr);
	TestNotNull(TEXT("macro graph is created"), MacroGraph);
	if (!MacroGraph)
	{
		return false;
	}

	UK2Node_Tunnel* EntryNode = nullptr;
	UK2Node_Tunnel* ExitNode = nullptr;
	for (UEdGraphNode* Node : MacroGraph->Nodes)
	{
		UK2Node_Tunnel* Tunnel = Cast<UK2Node_Tunnel>(Node);
		if (!Tunnel)
		{
			continue;
		}
		if (Tunnel->bCanHaveOutputs && !EntryNode)
		{
			EntryNode = Tunnel;
		}
		if (Tunnel->bCanHaveInputs && !ExitNode)
		{
			ExitNode = Tunnel;
		}
	}
	TestNotNull(TEXT("macro entry tunnel exists"), EntryNode);
	TestNotNull(TEXT("macro exit tunnel exists"), ExitNode);
	if (!EntryNode || !ExitNode)
	{
		return false;
	}

	FEdGraphPinType ExecPinType;
	ExecPinType.PinCategory = UEdGraphSchema_K2::PC_Exec;
	EntryNode->CreateUserDefinedPin(TEXT("Execute"), ExecPinType, EGPD_Output, false);
	ExitNode->CreateUserDefinedPin(TEXT("Then"), ExecPinType, EGPD_Input, false);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperGraphSnapshotService SnapshotService;
	FBlueprintHelperReplaceBlueprintGraphService ReplaceService(
		Resolver,
		BlockIdService,
		OwnershipService,
		SnapshotService);

	const FBlueprintHelperToolResultBase Result = ReplaceService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeReplaceMacroBodyExecutePayload(
			Blueprint->GetPathName(),
			MacroName));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddToolResultFailureDetail(
		*this,
		TEXT("replace macro body"),
		Result);
	TestTrue(TEXT("replace macro body succeeds"), Result.bOk);
	TestEqual(TEXT("replace macro body status is applied"), Result.Status, EBlueprintHelperToolStatus::Applied);

	UK2Node_CallFunction* ReplacementPrintNode = nullptr;
	for (UEdGraphNode* Node : MacroGraph->Nodes)
	{
		UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
		if (CallNode && CallNode->GetFunctionName().ToString().Equals(TEXT("PrintString"), ESearchCase::IgnoreCase))
		{
			ReplacementPrintNode = CallNode;
			break;
		}
	}
	TestNotNull(TEXT("replacement PrintString node exists"), ReplacementPrintNode);

	UEdGraphPin* EntryExecOut = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(EntryNode, EGPD_Output);
	UEdGraphPin* PrintExecIn = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(ReplacementPrintNode, EGPD_Input);
	UEdGraphPin* PrintExecOut = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(ReplacementPrintNode, EGPD_Output);
	UEdGraphPin* ExitExecIn = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(ExitNode, EGPD_Input);
	TestTrue(TEXT("macro entry links to replacement body"),
		EntryExecOut && PrintExecIn && EntryExecOut->LinkedTo.Contains(PrintExecIn));
	TestTrue(TEXT("replacement body links to macro exit"),
		PrintExecOut && ExitExecIn && PrintExecOut->LinkedTo.Contains(ExitExecIn));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteReplaceFunctionBodyReconnectsParamDataFlowTest,
	"BlueprintHelper.GraphWrite.Replace.FunctionBodyReconnectsParamDataFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteReplaceFunctionBodyReconnectsParamDataFlowTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("ReplaceFunctionBodyReconnectsParamDataFlow"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	const FString FunctionName = TEXT("ComputeFunctionBodyParamSmoke");
	const FString ParamName = TEXT("InputMessage");
	TSharedRef<FJsonObject> ParamLogicSpec =
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePrintFunctionParamLogicSpec(
			ParamName,
			FunctionName);

	UBlueprint* RawBlueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("RawFunctionBodyParamDataFlowFailsBeforeEntryReconnect"));
	UEdGraph* RawFunctionGraph = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteFunctionGraph(RawBlueprint, FunctionName);
	TestNotNull(TEXT("raw function graph is created"), RawFunctionGraph);
	TestTrue(TEXT("raw function input pin is created"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteFunctionInputPin(
			RawBlueprint,
			RawFunctionGraph,
			ParamName,
			FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestPinType(UEdGraphSchema_K2::PC_String)));
	if (RawFunctionGraph)
	{
		TArray<TSharedPtr<FUnresolvedNodeItem>> RawUnresolvedNodes;
		const FBlueprintGenerateResult RawGenerateResult =
			FBlueprintGraphGenerationPipeline::GenerateBlueprintFromJson(
				RawFunctionGraph,
				FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeRawReplaceGraphWritePayload(
					RawBlueprint ? RawBlueprint->GetPathName() : FString(),
					FunctionName,
					ParamLogicSpec),
				RawUnresolvedNodes);
		TestTrue(TEXT("raw function body generation succeeds with preserved function entry root"), RawGenerateResult.bSucceed);
		TestEqual(TEXT("raw function body param generation has no unresolved nodes"), RawUnresolvedNodes.Num(), 0);
		TestFalse(TEXT("raw function body generation has no unreachable exec after entry root resolution"),
			FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::GenerateResultHasConnectivityCode(
				RawGenerateResult,
				TEXT("unreachable_exec_node")));
		TestTrue(TEXT("raw function param getter links to generated PrintString"),
			FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::GraphHasVariableGetLinkedToFunctionInput(
				*this,
				RawFunctionGraph,
				FName(*ParamName),
				TEXT("PrintString")));
	}

	UEdGraph* FunctionGraph = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteFunctionGraph(Blueprint, FunctionName);
	TestNotNull(TEXT("function graph is created"), FunctionGraph);
	TestTrue(TEXT("function input pin is created"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteFunctionInputPin(
			Blueprint,
			FunctionGraph,
			ParamName,
			FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestPinType(UEdGraphSchema_K2::PC_String)));
	UK2Node_FunctionEntry* EntryNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindGraphWriteFunctionEntry(FunctionGraph);
	TestNotNull(TEXT("function entry is created"), EntryNode);
	if (!FunctionGraph || !EntryNode)
	{
		return false;
	}

	UK2Node_CallFunction* OldPrintNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(FunctionGraph);
	TestNotNull(TEXT("old PrintString body node is created"), OldPrintNode);
	TestTrue(TEXT("old function body is linked before replace"),
		OldPrintNode && FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ConnectFirstExecPins(EntryNode, OldPrintNode));

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperGraphSnapshotService SnapshotService;
	FBlueprintHelperReplaceBlueprintGraphService ReplaceService(
		Resolver,
		BlockIdService,
		OwnershipService,
		SnapshotService);

	TSharedRef<FJsonObject> Payload =
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeReplaceFunctionBodyExecutePayload(
			Blueprint->GetPathName(),
			FunctionName);
	Payload->SetObjectField(
		TEXT("logic_spec"),
		ParamLogicSpec);

	const FBlueprintHelperToolResultBase Result = ReplaceService.Execute(Payload);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddToolResultFailureDetail(
		*this,
		TEXT("replace function body with param data flow"),
		Result);
	TestTrue(TEXT("replace function body with param data flow succeeds"), Result.bOk);
	TestEqual(TEXT("replace function body with param data flow status is applied"), Result.Status, EBlueprintHelperToolStatus::Applied);

	UEdGraphPin* EntryExecOut = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(EntryNode, EGPD_Output);
	TestNotNull(TEXT("function entry has output exec pin"), EntryExecOut);
	TestTrue(TEXT("function entry output exec is linked after replace"),
		EntryExecOut && EntryExecOut->LinkedTo.Num() > 0);
	TestTrue(TEXT("function param getter links to replacement PrintString"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::GraphHasVariableGetLinkedToFunctionInput(
			*this,
			FunctionGraph,
			FName(*ParamName),
			TEXT("PrintString")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteReplaceFunctionBodyPreviewBlocksGenericParamGetTest,
	"BlueprintHelper.GraphWrite.Replace.FunctionBodyPreviewBlocksGenericParamGet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteReplaceFunctionBodyPreviewBlocksGenericParamGetTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("ReplaceFunctionBodyPreviewBlocksGenericParamGet"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	const FString FunctionName = TEXT("ComputeFunctionBodyGenericParamGet");
	const FString ParamName = TEXT("InputMessage");
	UEdGraph* FunctionGraph = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteFunctionGraph(Blueprint, FunctionName);
	TestNotNull(TEXT("function graph is created"), FunctionGraph);
	TestTrue(TEXT("function input pin is created"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteFunctionInputPin(
			Blueprint,
			FunctionGraph,
			ParamName,
			FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestPinType(UEdGraphSchema_K2::PC_String)));
	if (!FunctionGraph)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperGraphSnapshotService SnapshotService;
	FBlueprintHelperReplaceBlueprintGraphService ReplaceService(
		Resolver,
		BlockIdService,
		OwnershipService,
		SnapshotService);

	TSharedRef<FJsonObject> Payload =
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeReplaceFunctionBodyExecutePayload(
			Blueprint->GetPathName(),
			FunctionName);
	const TSharedPtr<FJsonObject>* Options = nullptr;
	if (Payload->TryGetObjectField(TEXT("options"), Options) && Options && Options->IsValid())
	{
		(*Options)->SetBoolField(TEXT("dry_run"), true);
	}
	Payload->SetObjectField(
		TEXT("logic_spec"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakePrintGenericGetLogicSpec(ParamName));

	const FBlueprintHelperToolResultBase Result = ReplaceService.Execute(Payload);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertBlockedDryRunFailure(
		*this,
		Result,
		TEXT("replace_blueprint_graph"),
		TEXT("target_unverified"),
		TEXT("logic_spec"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteReplaceFunctionBodyReconnectsReturnDataFlowTest,
	"BlueprintHelper.GraphWrite.Replace.FunctionBodyReconnectsReturnDataFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteReplaceFunctionBodyReconnectsReturnDataFlowTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("ReplaceFunctionBodyReconnectsReturnDataFlow"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	const FString FunctionName = TEXT("ReturnFunctionBodyStatusSmoke");
	const FString VariableName = TEXT("CurrentSaveGameStatus");
	const FEdGraphPinType StringPinType =
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestPinType(UEdGraphSchema_K2::PC_String);
	TestTrue(TEXT("member variable is created"),
		FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VariableName), StringPinType));

	UEdGraph* FunctionGraph = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteFunctionGraph(Blueprint, FunctionName);
	TestNotNull(TEXT("function graph is created"), FunctionGraph);
	TestTrue(TEXT("function output pin is created"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteFunctionOutputPin(
			Blueprint,
			FunctionGraph,
			TEXT("ReturnValue"),
			StringPinType));
	UK2Node_FunctionEntry* EntryNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindGraphWriteFunctionEntry(FunctionGraph);
	TestNotNull(TEXT("function entry is created"), EntryNode);
	if (!FunctionGraph || !EntryNode)
	{
		return false;
	}

	UK2Node_CallFunction* OldPrintNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(FunctionGraph);
	TestNotNull(TEXT("old PrintString body node is created"), OldPrintNode);
	TestTrue(TEXT("old function body is linked before replace"),
		OldPrintNode && FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ConnectFirstExecPins(EntryNode, OldPrintNode));

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperGraphSnapshotService SnapshotService;
	FBlueprintHelperReplaceBlueprintGraphService ReplaceService(
		Resolver,
		BlockIdService,
		OwnershipService,
		SnapshotService);

	TSharedRef<FJsonObject> Payload =
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeReplaceFunctionBodyExecutePayload(
			Blueprint->GetPathName(),
			FunctionName);
	Payload->SetObjectField(
		TEXT("logic_spec"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeSetThenReturnVariableLogicSpec(
			VariableName,
			TEXT("Ready")));

	const FBlueprintHelperToolResultBase Result = ReplaceService.Execute(Payload);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddToolResultFailureDetail(
		*this,
		TEXT("replace function body with return data flow"),
		Result);
	TestTrue(TEXT("replace function body with return data flow succeeds"), Result.bOk);
	TestEqual(TEXT("replace function body with return data flow status is applied"), Result.Status, EBlueprintHelperToolStatus::Applied);

	UEdGraphPin* EntryExecOut = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(EntryNode, EGPD_Output);
	TestNotNull(TEXT("function entry has output exec pin"), EntryExecOut);
	TestTrue(TEXT("function entry output exec is linked after replace"),
		EntryExecOut && EntryExecOut->LinkedTo.Num() > 0);
	TestTrue(TEXT("member variable getter links to function return"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::GraphHasVariableGetLinkedToFunctionResult(
			*this,
			FunctionGraph,
			FName(*VariableName)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteReplaceFunctionBodyRejectsUnreachablePureDataChainTest,
	"BlueprintHelper.GraphWrite.Replace.FunctionBodyRejectsUnreachablePureDataChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteReplaceFunctionBodyRejectsUnreachablePureDataChainTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("ReplaceFunctionBodyRejectsUnreachablePureDataChain"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	const FString FunctionName = TEXT("ComputeFunctionBodyRejectsUnreachablePureData");
	UEdGraph* FunctionGraph = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteFunctionGraph(Blueprint, FunctionName);
	TestNotNull(TEXT("function graph is created"), FunctionGraph);
	UK2Node_FunctionEntry* EntryNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindGraphWriteFunctionEntry(FunctionGraph);
	TestNotNull(TEXT("function entry is created"), EntryNode);
	if (!FunctionGraph || !EntryNode)
	{
		return false;
	}

	UK2Node_CallFunction* OldPrintNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(FunctionGraph);
	TestNotNull(TEXT("old PrintString body node is created"), OldPrintNode);
	TestTrue(TEXT("old function body is linked before replace"),
		OldPrintNode && FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ConnectFirstExecPins(EntryNode, OldPrintNode));
	const int32 NodeCountBefore = FunctionGraph->Nodes.Num();

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperGraphSnapshotService SnapshotService;
	FBlueprintHelperReplaceBlueprintGraphService ReplaceService(
		Resolver,
		BlockIdService,
		OwnershipService,
		SnapshotService);

	const FBlueprintHelperToolResultBase Result = ReplaceService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeReplaceFunctionBodyUnconsumedPureDataExecutePayload(
			Blueprint->GetPathName(),
			FunctionName));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertConnectivityFailureData(
		*this,
		Result,
		TEXT("replace_blueprint_graph"),
		TEXT("replace function body unconsumed pure data"));
	TestEqual(TEXT("replace function body connectivity rollback restores node count"), FunctionGraph->Nodes.Num(), NodeCountBefore);

	UEdGraphPin* EntryExecOut = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(EntryNode, EGPD_Output);
	TestNotNull(TEXT("function entry has output exec pin after rollback"), EntryExecOut);
	bool bLinkedToPrintString = false;
	if (EntryExecOut)
	{
		for (UEdGraphPin* LinkedPin : EntryExecOut->LinkedTo)
		{
			UK2Node_CallFunction* CallNode = LinkedPin ? Cast<UK2Node_CallFunction>(LinkedPin->GetOwningNode()) : nullptr;
			if (CallNode && CallNode->GetFunctionName().ToString().Equals(TEXT("PrintString"), ESearchCase::IgnoreCase))
			{
				bLinkedToPrintString = true;
				break;
			}
		}
	}
	if (!bLinkedToPrintString)
	{
		AddInfo(FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::DescribeGraphExecLinks(FunctionGraph));
	}
	TestTrue(TEXT("replace function body rollback preserves old body link"), bLinkedToPrintString);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteReplaceCustomEventBodyConnectsSignatureCreatedEntryTest,
	"BlueprintHelper.GraphWrite.Replace.CustomEventBodyConnectsSignatureCreatedEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteReplaceCustomEventBodyConnectsSignatureCreatedEntryTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("ReplaceSignatureCreatedEntry"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const FString EventName = TEXT("SignatureCreatedEvent");
	UK2Node_CustomEvent* EntryNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, EventName);
	TestNotNull(TEXT("signature-created custom event entry is created"), EntryNode);
	if (!EntryNode)
	{
		return false;
	}

	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), *EventName);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(EntryNode, BlockId);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperGraphSnapshotService SnapshotService;
	FBlueprintHelperReplaceBlueprintGraphService ReplaceService(
		Resolver,
		BlockIdService,
		OwnershipService,
		SnapshotService);

	const FBlueprintHelperToolResultBase Result = ReplaceService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeReplaceCustomEventExecutePayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			EventName));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddToolResultFailureDetail(
		*this,
		TEXT("replace signature-created custom event body"),
		Result);
	TestTrue(TEXT("replace signature-created custom event body succeeds"), Result.bOk);
	TestEqual(TEXT("replace signature-created custom event status is applied"), Result.Status, EBlueprintHelperToolStatus::Applied);
	TestEqual(TEXT("signature-created custom event block is entry plus replacement body"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::CountNodesWithBlueprintHelperBlockId(Graph, BlockId),
		2);
	TestTrue(TEXT("signature-created custom event links to replacement PrintString"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ExportHasExecLinkFromCustomEventToFunction(Graph, EventName, TEXT("PrintString")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteReplaceCustomEventBodyConnectivityFailureSkipsMissingOldBodyRollbackTest,
	"BlueprintHelper.GraphWrite.Replace.CustomEventBodyConnectivityFailureSkipsMissingOldBodyRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteReplaceCustomEventBodyConnectivityFailureSkipsMissingOldBodyRollbackTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("ReplaceEmptyBodyConnectivityRollback"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const FString EventName = TEXT("SignatureCreatedRollbackEvent");
	UK2Node_CustomEvent* EntryNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, EventName);
	TestNotNull(TEXT("signature-created custom event entry is created"), EntryNode);
	if (!EntryNode)
	{
		return false;
	}

	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), *EventName);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(EntryNode, BlockId);
	const int32 NodeCountBefore = Graph->Nodes.Num();

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperGraphSnapshotService SnapshotService;
	FBlueprintHelperReplaceBlueprintGraphService ReplaceService(
		Resolver,
		BlockIdService,
		OwnershipService,
		SnapshotService);

	const FBlueprintHelperToolResultBase Result = ReplaceService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeReplaceCustomEventUnconsumedPureDataExecutePayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			EventName));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertConnectivityFailureData(
		*this,
		Result,
		TEXT("replace_blueprint_graph"),
		TEXT("replace empty-body connectivity failure"));
	TestTrue(TEXT("connectivity failure rollback succeeds"),
		Result.Error.IsSet() && Result.Error->RollbackResult == EBlueprintHelperRollbackResult::RolledBack);
	TestFalse(TEXT("empty old body rollback does not require missing owned body"),
		Result.Error.IsSet() && Result.Error->Message.Contains(TEXT("graph_snapshot_restore_owned_body_missing")));
	TestEqual(TEXT("replace connectivity rollback restores signature-created entry-only graph"), Graph->Nodes.Num(), NodeCountBefore);
	TestEqual(TEXT("signature-created entry remains the only owned block node after rollback"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::CountNodesWithBlueprintHelperBlockId(Graph, BlockId),
		1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteReplaceExecuteConnectivityFailureEnvelopeTest,
	"BlueprintHelper.GraphWrite.Replace.ExecuteConnectivityFailureEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteReplaceExecuteConnectivityFailureEnvelopeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("ReplaceConnectivityFailure"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* EntryNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("SmokeCustomEvent"));
	UK2Node_CallFunction* OldPrintNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	TestNotNull(TEXT("custom event entry is created"), EntryNode);
	TestNotNull(TEXT("old PrintString body node is created"), OldPrintNode);
	TestTrue(TEXT("old custom event body is linked before replace"),
		EntryNode && OldPrintNode && FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ConnectFirstExecPins(EntryNode, OldPrintNode));
	if (!EntryNode || !OldPrintNode)
	{
		return false;
	}

	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("SmokeCustomEvent"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(EntryNode, BlockId);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(OldPrintNode, BlockId);
	const int32 NodeCountBefore = Graph->Nodes.Num();

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperGraphSnapshotService SnapshotService;
	FBlueprintHelperReplaceBlueprintGraphService ReplaceService(
		Resolver,
		BlockIdService,
		OwnershipService,
		SnapshotService);

	const FBlueprintHelperToolResultBase Result = ReplaceService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeReplaceUnconsumedPureDataExecutePayload(Blueprint->GetPathName(), Graph->GetName()));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertConnectivityFailureData(
		*this,
		Result,
		TEXT("replace_blueprint_graph"),
		TEXT("replace execute"));
	TestEqual(TEXT("replace connectivity rollback restores node count"), Graph->Nodes.Num(), NodeCountBefore);
	const bool bRollbackPreservedBody =
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ExportHasExecLinkFromCustomEventToFunction(Graph, TEXT("SmokeCustomEvent"), TEXT("PrintString"));
	if (!bRollbackPreservedBody)
	{
		AddInfo(FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::DescribeGraphExecLinks(Graph));
	}
	TestTrue(TEXT("replace rollback preserves old custom event body"), bRollbackPreservedBody);
	TestEqual(TEXT("replace rollback preserves old body ownership metadata"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::CountNodesWithBlueprintHelperBlockId(Graph, BlockId),
		2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteReplaceCustomEventBodyRejectsEmptyUnownedEntryTest,
	"BlueprintHelper.GraphWrite.Replace.RejectsImplicitUserAuthoredAdoption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteReplaceCustomEventBodyRejectsEmptyUnownedEntryTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("ReplaceRejectsEmptyUnownedEntry"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const FString EventName = TEXT("AdoptEmptyEvent");
	UK2Node_CustomEvent* EntryNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, EventName);
	TestNotNull(TEXT("custom event entry is created"), EntryNode);
	if (!EntryNode)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperGraphSnapshotService SnapshotService;
	const FString ExpectedBlockRef = BlockIdService.MakeBlockRef(Blueprint, Graph, EventName);
	const FString ExpectedBlockId = BlockIdService.MakeFullBlockId(Graph->GetName(), ExpectedBlockRef);
	FBlueprintHelperReplaceBlueprintGraphService ReplaceService(
		Resolver,
		BlockIdService,
		OwnershipService,
		SnapshotService);

	const FBlueprintHelperToolResultBase Result = ReplaceService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeReplaceCustomEventExecutePayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			EventName));

	TestFalse(TEXT("replace empty unowned custom event body is rejected"), Result.bOk);
	TestTrue(TEXT("replace empty unowned custom event reports owned-only policy"),
		Result.Error.IsSet() && Result.Error->Code == TEXT("owned_replace_target_not_blueprinthelper_owned"));

	UEdGraphPin* EntryExecOut = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(EntryNode, EGPD_Output);
	TestNotNull(TEXT("custom event has output exec pin"), EntryExecOut);
	if (EntryExecOut)
	{
		TestEqual(TEXT("unowned entry remains disconnected after rejected replace"), EntryExecOut->LinkedTo.Num(), 0);
	}

	TestFalse(TEXT("unowned entry is not adopted as BlueprintHelper owned"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::NodeHasBlueprintHelperBlockId(EntryNode, ExpectedBlockId));
	TestEqual(TEXT("no node receives synthetic ownership after rejected replace"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::CountNodesWithBlueprintHelperBlockId(Graph, ExpectedBlockId),
		0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteReplaceCustomEventBodyPreservesSiblingEventBodyTest,
	"BlueprintHelper.GraphWrite.Replace.CustomEventBodyPreservesSiblingEventBody",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteReplaceCustomEventBodyPreservesSiblingEventBodyTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("ReplacePreservesSiblingEventBody"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* EntryA = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("ReplaceEventA"));
	UK2Node_CallFunction* OldBodyA = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	UK2Node_CustomEvent* EntryB = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("ReplaceEventB"));
	UK2Node_CallFunction* OldBodyB = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	TestNotNull(TEXT("first custom event entry is created"), EntryA);
	TestNotNull(TEXT("first custom event old body is created"), OldBodyA);
	TestNotNull(TEXT("second custom event entry is created"), EntryB);
	TestNotNull(TEXT("second custom event old body is created"), OldBodyB);
	if (!EntryA || !OldBodyA || !EntryB || !OldBodyB)
	{
		return false;
	}

	TestTrue(TEXT("first custom event body is linked before replace"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ConnectFirstExecPins(EntryA, OldBodyA));
	TestTrue(TEXT("second custom event body is linked before replace"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ConnectFirstExecPins(EntryB, OldBodyB));

	const FString BlockIdA = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("ReplaceEventA"));
	const FString BlockIdB = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("ReplaceEventB"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(EntryA, BlockIdA);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(OldBodyA, BlockIdA);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(EntryB, BlockIdB);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(OldBodyB, BlockIdB);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperGraphSnapshotService SnapshotService;
	FBlueprintHelperReplaceBlueprintGraphService ReplaceService(
		Resolver,
		BlockIdService,
		OwnershipService,
		SnapshotService);

	const FBlueprintHelperToolResultBase Result = ReplaceService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeReplaceCustomEventExecutePayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			TEXT("ReplaceEventA")));

	TestTrue(TEXT("replace first custom event body succeeds"), Result.bOk);
	TestEqual(TEXT("replace first custom event status is applied"), Result.Status, EBlueprintHelperToolStatus::Applied);
	TestEqual(TEXT("sibling custom event body ownership remains intact"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::CountNodesWithBlueprintHelperBlockId(Graph, BlockIdB),
		2);
	TestEqual(TEXT("target custom event block is entry plus replacement body"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::CountNodesWithBlueprintHelperBlockId(Graph, BlockIdA),
		2);
	UEdGraphPin* EntryBExecOut = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(EntryB, EGPD_Output);
	UEdGraphPin* OldBodyBExecIn = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(OldBodyB, EGPD_Input);
	TestTrue(TEXT("target custom event links to replacement PrintString"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ExportHasExecLinkFromCustomEventToFunction(Graph, TEXT("ReplaceEventA"), TEXT("PrintString")));
	TestTrue(TEXT("sibling custom event still links to its original body"),
		EntryBExecOut && OldBodyBExecIn && EntryBExecOut->LinkedTo.Contains(OldBodyBExecIn));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewGraphBoundsUsesNodeGuidBeforeDisplayLabelTest,
	"BlueprintHelper.Review.GraphBounds.UsesNodeGuidBeforeDisplayLabel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewGraphBoundsUsesNodeGuidBeforeDisplayLabelTest::RunTest(const FString& Parameters)
{
	UEdGraph* Graph = NewObject<UEdGraph>(GetTransientPackage());

	UEdGraphNode* LabelMatchedNode = NewObject<UEdGraphNode>(Graph, FName(TEXT("DisplayLabelNode")));
	LabelMatchedNode->CreateNewGuid();
	LabelMatchedNode->NodePosX = 100;
	LabelMatchedNode->NodePosY = 40;
	LabelMatchedNode->NodeWidth = 240;
	LabelMatchedNode->NodeHeight = 88;
	Graph->AddNode(LabelMatchedNode, false, false);

	UEdGraphNode* GuidMatchedNode = NewObject<UEdGraphNode>(Graph, FName(TEXT("GuidMatchedNode")));
	GuidMatchedNode->CreateNewGuid();
	GuidMatchedNode->NodePosX = 500;
	GuidMatchedNode->NodePosY = 180;
	GuidMatchedNode->NodeWidth = 260;
	GuidMatchedNode->NodeHeight = 96;
	Graph->AddNode(GuidMatchedNode, false, false);

	FBlueprintHelperReviewAtomicTarget Target;
	Target.Surface = EBlueprintHelperReviewSurface::Graph;
	Target.GraphName = TEXT("EventGraph");
	Target.TargetKind = TEXT("graph_node");
	Target.NodeGuid = GuidMatchedNode->NodeGuid.ToString(EGuidFormats::Digits);
	Target.TargetKey = FString::Printf(TEXT("graph:EventGraph:node:%s"), *Target.NodeGuid);
	Target.DisplayLabel = LabelMatchedNode->GetName();

	TArray<FBlueprintHelperReviewAtomicTarget> Targets;
	Targets.Add(Target);

	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;
	FString DebugSummary;
	const bool bBuilt = FBlueprintHelperReviewGraphBounds::BuildBoundsForTargets(
		Targets,
		Graph,
		TEXT("EventGraph"),
		nullptr,
		Position,
		Size,
		&DebugSummary);

	TestTrue(TEXT("node guid target builds bounds"), bBuilt);
	TestTrue(TEXT("node guid match wins before display label match"),
		FMath::IsNearlyEqual(static_cast<float>(Position.X), 480.0f, 0.01f));
	TestTrue(TEXT("node guid match reports one matched node"),
		DebugSummary.Contains(TEXT("matchedNodes=1")));
	TestTrue(TEXT("debug reports node guid evidence"),
		DebugSummary.Contains(TEXT("hasNodeGuidTargets=1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewGraphBoundsUsesRecordedAnchorWhenNodeMatchFailsTest,
	"BlueprintHelper.Review.GraphBounds.UsesRecordedBoundsWhenNodeMatchFails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewGraphBoundsUsesRecordedAnchorWhenNodeMatchFailsTest::RunTest(const FString& Parameters)
{
	UEdGraph* Graph = NewObject<UEdGraph>(GetTransientPackage());

	FBlueprintHelperReviewAtomicTarget Target;
	Target.Surface = EBlueprintHelperReviewSurface::Graph;
	Target.GraphName = TEXT("EventGraph");
	Target.TargetKind = TEXT("graph_node");
	Target.NodeGuid = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Target.TargetKey = FString::Printf(TEXT("graph:EventGraph:node:%s"), *Target.NodeGuid);
	Target.DisplayLabel = TEXT("Deleted Print String");
	Target.AnchorJson = FString(TEXT("{\"node_path\":\"/Transient/DeletedNode\",\"node_guid\":\""))
		+ Target.NodeGuid
		+ TEXT("\",\"display_label\":\"Deleted Print String\",\"graph_position\":{\"x\":120,\"y\":80},\"graph_size\":{\"x\":300,\"y\":140},\"has_graph_bounds\":true}");

	TArray<FBlueprintHelperReviewAtomicTarget> Targets;
	Targets.Add(Target);

	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;
	FString DebugSummary;
	const bool bBuilt = FBlueprintHelperReviewGraphBounds::BuildBoundsForTargets(
		Targets,
		Graph,
		TEXT("EventGraph"),
		nullptr,
		Position,
		Size,
		&DebugSummary);

	TestTrue(TEXT("recorded anchor json supplies bounds when node match fails"), bBuilt);
	TestTrue(TEXT("recorded anchor bounds keep left padding"),
		FMath::IsNearlyEqual(static_cast<float>(Position.X), 100.0f, 0.01f));
	TestTrue(TEXT("recorded anchor bounds keep padded width"),
		FMath::IsNearlyEqual(static_cast<float>(Size.X), 340.0f, 0.01f));
	TestTrue(TEXT("debug reports recorded bounds evidence"),
		DebugSummary.Contains(TEXT("hasRecordedBounds=1")));
	TestTrue(TEXT("debug reports structured anchor source"),
		DebugSummary.Contains(TEXT("anchorSource=structured")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteOwnershipWritesMetadataWithoutManagedCommentTest,
	"BlueprintHelper.GraphWrite.Ownership.WritesMetadataWithoutManagedComment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteOwnershipWritesMetadataWithoutManagedCommentTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("OwnershipMetadataOnly"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* EventNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("SmokeCustomEvent"));
	TestNotNull(TEXT("custom event is created"), EventNode);
	if (!EventNode)
	{
		return false;
	}

	EventNode->NodeComment = TEXT("Designer note");
	FBlueprintHelperPackageMetaData& PreWriteMetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(EventNode->GetOutermost());
	PreWriteMetaData.SetValue(EventNode, TEXT("BlueprintHelperTool"), TEXT("legacy_graph_write"));

	FBlueprintHelperOwnershipService OwnershipService;
	FString Error;
	const bool bWritten = OwnershipService.WriteNodeOwnership(
		Blueprint,
		EventNode,
		TEXT("EventGraph_SmokeCustomEvent"),
		TEXT("SmokeFeature"),
		Error);

	TestTrue(TEXT("ownership writes successfully"), bWritten);
	TestEqual(TEXT("user node comment is preserved"), EventNode->NodeComment, FString(TEXT("Designer note")));
	TestFalse(TEXT("comment omits block_id"), EventNode->NodeComment.Contains(TEXT("block_id=")));
	TestFalse(TEXT("comment omits legacy id"), EventNode->NodeComment.Contains(TEXT("legacy_id=")));
	TestFalse(TEXT("comment omits tool field"), EventNode->NodeComment.Contains(TEXT("tool=")));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertNodeHasOwnershipMetadata(
		*this,
		EventNode,
		TEXT("EventGraph_SmokeCustomEvent"),
		TEXT("SmokeFeature"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteAppendOwnershipWritesMetadataWithoutManagedCommentTest,
	"BlueprintHelper.GraphWrite.Append.OwnershipWritesMetadataWithoutManagedComment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteAppendOwnershipWritesMetadataWithoutManagedCommentTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("AppendOwnershipMetadata"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* EntryNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("SmokeCustomEvent"));
	TestNotNull(TEXT("signature-created custom event exists"), EntryNode);
	if (!EntryNode)
	{
		return false;
	}
	TSet<UEdGraphNode*> NodeSnapshot;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node)
		{
			NodeSnapshot.Add(Node);
		}
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperAppendBlueprintGraphService AppendService(
		Resolver,
		BlockIdService,
		OwnershipService);
	const FString GraphName = Graph->GetName();
	const FBlueprintHelperToolResultBase Result = AppendService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeAppendReuseExistingEntryExecutePayload(Blueprint->GetPathName(), GraphName));
	if (!Result.bOk && Result.Error.IsSet())
	{
		AddError(FString::Printf(
			TEXT("append write failed: code=%s stage=%s message=%s"),
			*Result.Error->Code,
			ToolStageToString(Result.Error->Stage),
			*Result.Error->Message));
	}

	TestTrue(TEXT("append write succeeds"), Result.bOk);
	TestEqual(TEXT("append write status is applied"), Result.Status, EBlueprintHelperToolStatus::Applied);
	const TSharedPtr<FJsonObject>* AppendResult = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* BlockRefs = nullptr;
	FString BlockRef;
	TestTrue(TEXT("append result exposes append_result"),
		Result.Data.IsValid() && Result.Data->TryGetObjectField(TEXT("append_result"), AppendResult));
	TestTrue(TEXT("append result exposes block refs"),
		AppendResult && AppendResult->IsValid() && (*AppendResult)->TryGetArrayField(TEXT("block_refs"), BlockRefs));
	TestTrue(TEXT("append result exposes first block ref"),
		BlockRefs && BlockRefs->Num() > 0 && (*BlockRefs)[0].IsValid() && (*BlockRefs)[0]->TryGetString(BlockRef));

	TestNotNull(TEXT("append graph exists"), Graph);
	TestTrue(TEXT("append graph has created nodes"), Graph && Graph->Nodes.Num() > NodeSnapshot.Num());
	if (!Graph || Graph->Nodes.Num() <= NodeSnapshot.Num())
	{
		return false;
	}

	const FString ExpectedBlockId = BlockIdService.MakeFullBlockId(
		GraphName,
		BlockRef);
	TArray<UEdGraphNode*> OwnedNodes;
	OwnedNodes.Add(EntryNode);
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && !NodeSnapshot.Contains(Node))
		{
			OwnedNodes.AddUnique(Node);
		}
	}
	TestTrue(TEXT("append-created ownership target nodes are present"), OwnedNodes.Num() > 1);
	for (UEdGraphNode* Node : OwnedNodes)
	{
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertNodeHasOwnershipMetadata(*this, Node, ExpectedBlockId, TEXT("SmokeFeature"));
		TestFalse(TEXT("append-created node comment omits block_id"),
			Node && Node->NodeComment.Contains(TEXT("block_id=")));
		TestFalse(TEXT("append-created node comment omits legacy id"),
			Node && Node->NodeComment.Contains(TEXT("legacy_id=")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteAppendReusesSignatureEntryTest,
	"BlueprintHelper.GraphWrite.Append.ReusesSignatureEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteAppendReusesSignatureEntryTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("AppendReusesSignatureEntry"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* EntryNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("SmokeCustomEvent"));
	TestNotNull(TEXT("signature-created custom event exists"), EntryNode);
	if (!EntryNode)
	{
		return false;
	}

	const int32 EventCountBefore = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::CountCustomEventsByName(Graph, TEXT("SmokeCustomEvent"));

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperAppendBlueprintGraphService AppendService(
		Resolver,
		BlockIdService,
		OwnershipService);
	const FBlueprintHelperToolResultBase Result = AppendService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeAppendReuseExistingEntryExecutePayload(Blueprint->GetPathName(), Graph->GetName()));
	if (!Result.bOk && Result.Error.IsSet())
	{
		AddError(FString::Printf(
			TEXT("append reuse failed: code=%s stage=%s message=%s"),
			*Result.Error->Code,
			ToolStageToString(Result.Error->Stage),
			*Result.Error->Message));
	}

	TestTrue(TEXT("append reusing signature entry succeeds"), Result.bOk);
	TestEqual(TEXT("append reuse write status is applied"), Result.Status, EBlueprintHelperToolStatus::Applied);
	TestEqual(TEXT("append reuse does not duplicate custom event"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::CountCustomEventsByName(Graph, TEXT("SmokeCustomEvent")),
		EventCountBefore);
	TestTrue(TEXT("append reuse connects existing custom event to imported body"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ExportHasExecLinkFromCustomEventToFunction(Graph, TEXT("SmokeCustomEvent"), TEXT("PrintString")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteAppendPreviewConnectivityFailureEnvelopeTest,
	"BlueprintHelper.GraphWrite.Append.PreviewConnectivityFailureEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteAppendPreviewConnectivityFailureEnvelopeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("AppendPreviewConnectivityFailure"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* EntryNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("SmokeCustomEvent"));
	TestNotNull(TEXT("signature-created custom event exists"), EntryNode);
	if (!EntryNode)
	{
		return false;
	}

	const int32 NodeCountBefore = Graph->Nodes.Num();
	const bool bDirtyBefore = Blueprint->GetOutermost()->IsDirty();

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperAppendBlueprintGraphService AppendService(
		Resolver,
		BlockIdService,
		OwnershipService);
	const FBlueprintHelperToolResultBase Result = AppendService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeAppendUnconsumedPureDataPreviewPayload(
			Blueprint->GetPathName(),
			Graph->GetName()));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertConnectivityFailureData(
		*this,
		Result,
		TEXT("append_blueprint_graph"),
		TEXT("append preview"));
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("append preview connectivity failure is preflight-stage"), Result.Error->Stage, EBlueprintHelperToolStage::Preflight);
	}
	TestEqual(TEXT("append preview connectivity cleanup removes preview nodes"), Graph->Nodes.Num(), NodeCountBefore);
	TestEqual(TEXT("append preview connectivity cleanup restores package dirty flag"), Blueprint->GetOutermost()->IsDirty(), bDirtyBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteAppendExecuteConnectivityFailureEnvelopeTest,
	"BlueprintHelper.GraphWrite.Append.ExecuteConnectivityFailureEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteAppendExecuteConnectivityFailureEnvelopeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("AppendConnectivityFailure"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* EntryNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("SmokeCustomEvent"));
	TestNotNull(TEXT("signature-created custom event exists"), EntryNode);
	if (!EntryNode)
	{
		return false;
	}

	const int32 NodeCountBefore = Graph->Nodes.Num();
	const bool bDirtyBefore = Blueprint->GetOutermost()->IsDirty();

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperAppendBlueprintGraphService AppendService(
		Resolver,
		BlockIdService,
		OwnershipService);
	const FBlueprintHelperToolResultBase Result = AppendService.Execute(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeAppendUnconsumedPureDataExecutePayload(
			Blueprint->GetPathName(),
			Graph->GetName()));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertConnectivityFailureData(
		*this,
		Result,
		TEXT("append_blueprint_graph"),
		TEXT("append execute"));
	TestEqual(TEXT("append connectivity rollback removes generated nodes"), Graph->Nodes.Num(), NodeCountBefore);
	TestEqual(TEXT("append connectivity rollback restores package dirty flag"), Blueprint->GetOutermost()->IsDirty(), bDirtyBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteTaskRuntimeReplaceCustomEventBodyReconnectsEntryExecTest,
	"BlueprintHelper.GraphWrite.TaskRuntime.Replace.CustomEventBodyReconnectsEntryExec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteTaskRuntimeReplaceCustomEventBodyReconnectsEntryExecTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("RuntimeReplaceReconnectsEntryExec"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* EntryNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("SmokeCustomEvent"));
	UK2Node_CallFunction* OldPrintNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(Graph);
	TestNotNull(TEXT("custom event entry is created"), EntryNode);
	TestNotNull(TEXT("old PrintString body node is created"), OldPrintNode);
	TestTrue(TEXT("old custom event body is linked before runtime replace"),
		EntryNode && OldPrintNode && FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ConnectFirstExecPins(EntryNode, OldPrintNode));
	if (!EntryNode || !OldPrintNode)
	{
		return false;
	}
	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("SmokeCustomEvent"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(EntryNode, BlockId);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(OldPrintNode, BlockId);

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness Harness;
	const FBlueprintHelperToolResultBase Result = Harness.RuntimeService.ExecuteTaskPlan(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTaskPlanPayload(Blueprint->GetPathName(), Graph->GetName(), FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeReplaceBodyOp()));

	TestTrue(TEXT("runtime replace custom event body succeeds"), Result.bOk);
	TestEqual(TEXT("runtime replace status is applied"), Result.Status, EBlueprintHelperToolStatus::Applied);

	UEdGraphPin* EntryExecOut = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(EntryNode, EGPD_Output);
	TestNotNull(TEXT("custom event has output exec pin after runtime replace"), EntryExecOut);
	TestTrue(TEXT("custom event output exec is linked after runtime replace"),
		EntryExecOut && EntryExecOut->LinkedTo.Num() > 0);
	TestTrue(TEXT("exported graph contains runtime event to replacement PrintString exec link"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ExportHasExecLinkFromCustomEventToFunction(Graph, TEXT("SmokeCustomEvent"), TEXT("PrintString")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteTaskRuntimeReplaceFunctionBodyReconnectsParamDataFlowTest,
	"BlueprintHelper.GraphWrite.TaskRuntime.Replace.FunctionBodyReconnectsParamDataFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteTaskRuntimeReplaceFunctionBodyReconnectsParamDataFlowTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("RuntimeReplaceFunctionBodyParamDataFlow"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	const FString FunctionName = TEXT("RuntimeFunctionBodyParamSmoke");
	const FString ParamName = TEXT("InputMessage");
	UEdGraph* FunctionGraph = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteFunctionGraph(Blueprint, FunctionName);
	TestNotNull(TEXT("function graph is created"), FunctionGraph);
	TestTrue(TEXT("function input pin is created"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteFunctionInputPin(
			Blueprint,
			FunctionGraph,
			ParamName,
			FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestPinType(UEdGraphSchema_K2::PC_String)));
	UK2Node_FunctionEntry* EntryNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindGraphWriteFunctionEntry(FunctionGraph);
	TestNotNull(TEXT("function entry is created"), EntryNode);
	if (!FunctionGraph || !EntryNode)
	{
		return false;
	}

	UK2Node_CallFunction* OldPrintNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWritePrintStringCall(FunctionGraph);
	TestNotNull(TEXT("old PrintString body node is created"), OldPrintNode);
	TestTrue(TEXT("old function body is linked before runtime replace"),
		OldPrintNode && FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ConnectFirstExecPins(EntryNode, OldPrintNode));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness Harness;
	const FBlueprintHelperToolResultBase Result = Harness.RuntimeService.ExecuteTaskPlan(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTaskPlanPayload(
			Blueprint->GetPathName(),
			FunctionName,
			FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeReplaceFunctionBodyParamDataFlowOp(
				FunctionName,
				ParamName)));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddToolResultFailureDetail(
		*this,
		TEXT("runtime replace function body with param data flow"),
		Result);
	TestTrue(TEXT("runtime replace function body with param data flow succeeds"), Result.bOk);
	TestEqual(TEXT("runtime replace function body status is applied"), Result.Status, EBlueprintHelperToolStatus::Applied);

	UEdGraphPin* EntryExecOut = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(EntryNode, EGPD_Output);
	TestNotNull(TEXT("function entry has output exec pin after runtime replace"), EntryExecOut);
	TestTrue(TEXT("function entry output exec is linked after runtime replace"),
		EntryExecOut && EntryExecOut->LinkedTo.Num() > 0);
	TestTrue(TEXT("runtime function param getter links to replacement PrintString"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::GraphHasVariableGetLinkedToFunctionInput(
			*this,
			FunctionGraph,
			FName(*ParamName),
			TEXT("PrintString")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCompositeCreateBlueprintFeatureExecuteReadBackTest,
	"BlueprintHelper.TaskRuntime.Composite.CreateBlueprintFeatureExecuteReadBack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperCompositeCreateBlueprintFeatureExecuteReadBackTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("CompositeCreateFeatureExecute"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const FString AssetPath = Blueprint->GetPathName();
	const FString GraphName = Graph->GetName();
	const FString EventName = TEXT("BH_CompositeExecuteEvent");
	const FString ComponentName = TEXT("BHCompositeScene");
	const FString VariableName = TEXT("bBHCompositeEnabled");

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness Harness;
	const FBlueprintHelperToolResultBase ExecuteResult = Harness.RuntimeService.ExecuteTaskPlan(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeCompositeCreateBlueprintFeaturePayload(
			AssetPath,
			GraphName,
			EventName,
			ComponentName,
			VariableName));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddToolResultFailureDetail(
		*this,
		TEXT("composite create_blueprint_feature execute"),
		ExecuteResult);
	TestTrue(TEXT("composite create_blueprint_feature execute succeeds"), ExecuteResult.bOk);
	if (!ExecuteResult.bOk && ExecuteResult.Error.IsSet())
	{
		TestFalse(TEXT("execute failure has diagnosable message"), ExecuteResult.Error->Message.IsEmpty());
	}
	TestEqual(TEXT("composite execute status is applied"), ExecuteResult.Status, EBlueprintHelperToolStatus::Applied);

	FString TaskRunId;
	TestTrue(TEXT("execute result carries task_run_id"),
		ExecuteResult.Data.IsValid() && ExecuteResult.Data->TryGetStringField(TEXT("task_run_id"), TaskRunId));
	const FBlueprintHelperToolResultBase JournalResult = Harness.RuntimeService.GetTaskRunJournal(TaskRunId);
	TestTrue(TEXT("TaskRunJournal can be loaded for composite execute"), JournalResult.bOk);
	FString JournalStatus;
	TestTrue(TEXT("TaskRunJournal has status"),
		JournalResult.Data.IsValid() && JournalResult.Data->TryGetStringField(TEXT("status"), JournalStatus));
	TestEqual(TEXT("TaskRunJournal completed"), JournalStatus, FString(TEXT("completed")));

	const TArray<TSharedPtr<FJsonValue>>* JournalSteps = nullptr;
	TestTrue(TEXT("TaskRunJournal exposes composite steps"),
		JournalResult.Data.IsValid() && JournalResult.Data->TryGetArrayField(TEXT("steps"), JournalSteps));
	TestEqual(TEXT("component, variable, signature, and graph_write steps are recorded"), JournalSteps ? JournalSteps->Num() : 0, 4);

	TestTrue(TEXT("read-back finds created component"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::HasSCSComponentNamed(Blueprint, ComponentName));
	TestTrue(TEXT("read-back finds created variable"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::HasMemberVariableNamed(Blueprint, FName(*VariableName)));
	TestEqual(TEXT("read-back finds one custom event"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::CountCustomEventsByName(Graph, EventName),
		1);
	TestEqual(TEXT("signature-created custom event body is BlueprintHelper-owned"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::CountNodesWithBlueprintHelperBlockId(
			Graph,
			FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteEntryBlockId(Blueprint, Graph, EventName)),
		2);
	TestTrue(TEXT("read-back finds custom event body graph write"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ExportHasExecLinkFromCustomEventToFunction(Graph, EventName, TEXT("PrintString")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteTaskRuntimeMergeBranchForkOwnedBlockCallReadBackTest,
	"BlueprintHelper.GraphWrite.TaskRuntime.Merge.BranchForkOwnedBlockCallReadBack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteTaskRuntimeMergeBranchForkOwnedBlockCallReadBackTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("RuntimeMergeBranchForkOwnedBlockCall"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const FString AnchorBlockId = FString::Printf(TEXT("%s_AnchorBlock0"), *Graph->GetName());
	const FString InsertedBlockId = FString::Printf(TEXT("%s_InsertedBlock0"), *Graph->GetName());

	UK2Node_CustomEvent* AnchorEntry = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("OwnedAnchorBlock"));
	UK2Node_IfThenElse* OriginalSuccessor = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteBranchNode(Graph);
	UK2Node_CustomEvent* InsertedEntry = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("OwnedInsertedBlock"));
	TestNotNull(TEXT("anchor owned block entry is created"), AnchorEntry);
	TestNotNull(TEXT("original successor is created"), OriginalSuccessor);
	TestNotNull(TEXT("inserted owned block entry is created"), InsertedEntry);
	if (!AnchorEntry || !OriginalSuccessor || !InsertedEntry)
	{
		return false;
	}

	TestTrue(TEXT("fixture starts with anchor linked to original successor"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ConnectFirstExecPins(AnchorEntry, OriginalSuccessor));
	UEdGraphPin* AnchorExecOut = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(AnchorEntry, EGPD_Output);
	UEdGraphPin* OriginalExecIn = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(OriginalSuccessor, EGPD_Input);
	TestTrue(TEXT("anchor has exactly one original successor before branch_fork"), AnchorExecOut && AnchorExecOut->LinkedTo.Num() == 1);
	if (!AnchorExecOut || !OriginalExecIn || AnchorExecOut->LinkedTo.Num() != 1)
	{
		return false;
	}

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(AnchorEntry, AnchorBlockId);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(OriginalSuccessor, AnchorBlockId);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(InsertedEntry, InsertedBlockId);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness Harness;
	const FString AssetPath = Blueprint->GetPathName();
	const FString GraphName = Graph->GetName();

	const FBlueprintHelperToolResultBase Preview = Harness.RuntimeService.PreviewTaskPlan(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTaskPlanPayload(
			AssetPath,
			GraphName,
			FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeBranchForkOwnedBlockCallOp(
				AnchorBlockId,
				AnchorEntry->GetName(),
				InsertedBlockId),
			true));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertRuntimePreviewReachedGraphWriteService(
		*this,
		Preview,
		TEXT("merge_blueprint_graph"),
		true);

	const FBlueprintHelperToolResultBase ExecuteResult = Harness.RuntimeService.ExecuteTaskPlan(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTaskPlanPayload(
			AssetPath,
			GraphName,
			FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeBranchForkOwnedBlockCallOp(
				AnchorBlockId,
				AnchorEntry->GetName(),
				InsertedBlockId),
			true));
	TestTrue(TEXT("runtime branch_fork owned block call succeeds"), ExecuteResult.bOk);
	if (!ExecuteResult.bOk && ExecuteResult.Error.IsSet())
	{
		TestFalse(TEXT("execute failure has diagnosable message"), ExecuteResult.Error->Message.IsEmpty());
	}
	TestEqual(TEXT("runtime branch_fork status is applied"), ExecuteResult.Status, EBlueprintHelperToolStatus::Applied);

	FString TaskRunId;
	TestTrue(TEXT("execute result has data"), ExecuteResult.Data.IsValid());
	TestTrue(TEXT("execute result carries task_run_id"),
		ExecuteResult.Data.IsValid() && ExecuteResult.Data->TryGetStringField(TEXT("task_run_id"), TaskRunId));
	TestFalse(TEXT("task_run_id is not empty"), TaskRunId.IsEmpty());
	const FBlueprintHelperToolResultBase JournalResult = Harness.RuntimeService.GetTaskRunJournal(TaskRunId);
	TestTrue(TEXT("TaskRunJournal can be loaded for branch_fork task"), JournalResult.bOk);
	FString JournalStatus;
	TestTrue(TEXT("TaskRunJournal has status"),
		JournalResult.Data.IsValid() && JournalResult.Data->TryGetStringField(TEXT("status"), JournalStatus));
	TestEqual(TEXT("TaskRunJournal completed"), JournalStatus, FString(TEXT("completed")));

	UK2Node_ExecutionSequence* SequenceNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindGraphWriteSequenceNode(Graph);
	UK2Node_CallFunction* InsertedCall = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindGraphWriteCallFunctionNode(Graph, FName(TEXT("OwnedInsertedBlock")));
	UEdGraphPin* SequenceExecIn = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(SequenceNode, EGPD_Input);
	const TArray<UEdGraphPin*> SequenceThenPins = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindExecPins(SequenceNode, EGPD_Output);
	UEdGraphPin* InsertedExecIn = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindFirstExecPin(InsertedCall, EGPD_Input);

	TestNotNull(TEXT("read-back finds branch_fork sequence node"), SequenceNode);
	TestNotNull(TEXT("read-back finds owned block call node"), InsertedCall);
	TestTrue(TEXT("anchor now links to sequence input"), AnchorExecOut && SequenceExecIn && AnchorExecOut->LinkedTo.Contains(SequenceExecIn));
	TestFalse(TEXT("anchor no longer directly links original successor"), AnchorExecOut && AnchorExecOut->LinkedTo.Contains(OriginalExecIn));
	TestTrue(TEXT("sequence has inserted and original successor branches"), SequenceThenPins.Num() >= 2);
	if (SequenceThenPins.Num() >= 2)
	{
		TestTrue(TEXT("inserted branch calls owned block"), InsertedExecIn && SequenceThenPins[0]->LinkedTo.Contains(InsertedExecIn));
		TestTrue(TEXT("original successor branch is preserved"), OriginalExecIn && SequenceThenPins[1]->LinkedTo.Contains(OriginalExecIn));
	}
	TestTrue(TEXT("inserted call is reachable from anchor"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::IsExecReachable(AnchorEntry, InsertedCall));
	TestTrue(TEXT("original successor is reachable from anchor"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::IsExecReachable(AnchorEntry, OriginalSuccessor));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteReadbackEvidence SequenceEvidence;
	SequenceEvidence.bHasResolverEvidence = true;
	SequenceEvidence.bHasSpawnEvidence = true;
	SequenceEvidence.SingletonStableId = TEXT("singleton_control_flow:sequence");
	TestTrue(TEXT("readback locates branch_fork sequence by singleton provider evidence"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ReadbackFindsSingletonControlByEvidence(Graph, SequenceEvidence));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteTaskRuntimeCallFunctionDisplayNameReadBackTest,
	"BlueprintHelper.GraphWrite.TaskRuntime.CallFunction.DisplayNameReadBack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteTaskRuntimeCallFunctionDisplayNameReadBackTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("RuntimeCallFunctionDisplayName"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const FString EventName = TEXT("RuntimeDisplayNameCall");
	UK2Node_CustomEvent* EntryNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, EventName);
	TestNotNull(TEXT("fixture custom event exists"), EntryNode);
	if (!EntryNode)
	{
		return false;
	}
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(
		EntryNode,
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteEntryBlockId(Blueprint, Graph, EventName));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness Harness;
	const TSharedRef<FJsonObject> TaskPlanPayload = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTaskPlanPayload(
		Blueprint->GetPathName(),
		Graph->GetName(),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeEnsureEntryCallFunctionOp(EventName, TEXT("Print String")));

	const FBlueprintHelperToolResultBase Preview = Harness.RuntimeService.PreviewTaskPlan(TaskPlanPayload);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertRuntimePreviewReachedGraphWriteService(
		*this,
		Preview,
		TEXT("replace_blueprint_graph"),
		true);

	const FBlueprintHelperToolResultBase ExecuteResult = Harness.RuntimeService.ExecuteTaskPlan(TaskPlanPayload);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddToolResultFailureDetail(
		*this,
		TEXT("runtime display-name call_function execute"),
		ExecuteResult);
	TestTrue(TEXT("runtime display-name call_function execute succeeds"), ExecuteResult.bOk);
	if (!ExecuteResult.bOk && ExecuteResult.Error.IsSet())
	{
		TestFalse(TEXT("execute failure has diagnosable message"), ExecuteResult.Error->Message.IsEmpty());
	}
	TestTrue(TEXT("read-back finds event to resolved PrintString exec link"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ExportHasExecLinkFromCustomEventToFunction(Graph, EventName, TEXT("PrintString")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteTaskRuntimeCallFunctionQualifiedNameReadBackTest,
	"BlueprintHelper.GraphWrite.TaskRuntime.CallFunction.QualifiedNameReadBack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteTaskRuntimeCallFunctionQualifiedNameReadBackTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("RuntimeCallFunctionQualifiedName"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const FString EventName = TEXT("RuntimeQualifiedNameCall");
	UK2Node_CustomEvent* EntryNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, EventName);
	TestNotNull(TEXT("fixture custom event exists"), EntryNode);
	if (!EntryNode)
	{
		return false;
	}
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(
		EntryNode,
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteEntryBlockId(Blueprint, Graph, EventName));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness Harness;
	const TSharedRef<FJsonObject> TaskPlanPayload = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTaskPlanPayload(
		Blueprint->GetPathName(),
		Graph->GetName(),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeEnsureEntryCallFunctionOp(
			EventName,
			TEXT("/Script/Engine.KismetSystemLibrary:PrintString")));

	const FBlueprintHelperToolResultBase Preview = Harness.RuntimeService.PreviewTaskPlan(TaskPlanPayload);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertRuntimePreviewReachedGraphWriteService(
		*this,
		Preview,
		TEXT("replace_blueprint_graph"),
		true);

	const FBlueprintHelperToolResultBase ExecuteResult = Harness.RuntimeService.ExecuteTaskPlan(TaskPlanPayload);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddToolResultFailureDetail(
		*this,
		TEXT("runtime qualified call_function execute"),
		ExecuteResult);
	TestTrue(TEXT("runtime qualified call_function execute succeeds"), ExecuteResult.bOk);
	if (!ExecuteResult.bOk && ExecuteResult.Error.IsSet())
	{
		TestFalse(TEXT("execute failure has diagnosable message"), ExecuteResult.Error->Message.IsEmpty());
	}
	TestTrue(TEXT("read-back finds qualified event to PrintString exec link"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ExportHasExecLinkFromCustomEventToFunction(Graph, EventName, TEXT("PrintString")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteTaskRuntimeCallFunctionTargetObjectPreviewExecuteReadBackTest,
	"BlueprintHelper.GraphWrite.TaskRuntime.CallFunction.TargetObjectPreviewExecuteReadBack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteTaskRuntimeCallFunctionTargetObjectPreviewExecuteReadBackTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("RuntimeCallFunctionTargetObject"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	const FName DoorMeshComponentName(TEXT("DoorMesh"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness Harness;

	FBlueprintHelperAddComponentRequest AddComponentRequest;
	AddComponentRequest.AssetPath = Blueprint->GetPathName();
	AddComponentRequest.ComponentName = DoorMeshComponentName.ToString();
	AddComponentRequest.ComponentClass = TEXT("StaticMeshComponent");
	const FBlueprintHelperToolResultBase AddComponentResult = Harness.ComponentService.AddComponent(AddComponentRequest);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddToolResultFailureDetail(
		*this,
		TEXT("target_object setup component"),
		AddComponentResult);
	TestTrue(TEXT("DoorMesh StaticMeshComponent fixture is added"), AddComponentResult.bOk);
	TestTrue(TEXT("DoorMesh component_ref exists in SCS"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::HasSCSComponentNamed(Blueprint, DoorMeshComponentName.ToString()));
	if (!AddComponentResult.bOk)
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const FString EventName = TEXT("RuntimeTargetObjectCall");
	UK2Node_CustomEvent* EntryNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, EventName);
	TestNotNull(TEXT("fixture custom event exists"), EntryNode);
	if (!EntryNode)
	{
		return false;
	}
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(
		EntryNode,
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteEntryBlockId(Blueprint, Graph, EventName));

	const TSharedRef<FJsonObject> TaskPlanPayload = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTaskPlanPayload(
		Blueprint->GetPathName(),
		Graph->GetName(),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeEnsureEntrySetSimulatePhysicsTargetObjectOp(EventName),
		true);

	const FBlueprintHelperToolResultBase Preview = Harness.RuntimeService.PreviewTaskPlan(TaskPlanPayload);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertRuntimePreviewReachedGraphWriteService(
		*this,
		Preview,
		TEXT("replace_blueprint_graph"),
		true);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddToolResultFailureDetail(
		*this,
		TEXT("target_object preview"),
		Preview);

	const FBlueprintHelperToolResultBase ExecuteResult = Harness.RuntimeService.ExecuteTaskPlan(TaskPlanPayload);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddToolResultFailureDetail(
		*this,
		TEXT("target_object execute"),
		ExecuteResult);
	TestTrue(TEXT("target_object execute succeeds"), ExecuteResult.bOk);
	TestEqual(TEXT("target_object execute status is applied"), ExecuteResult.Status, EBlueprintHelperToolStatus::Applied);

	const TSharedPtr<FJsonObject>* CacheStats = nullptr;
	double CacheHits = 0.0;
	TestTrue(TEXT("execute exposes call_function cache stats"),
		ExecuteResult.Data.IsValid() && ExecuteResult.Data->TryGetObjectField(TEXT("call_function_resolution_cache"), CacheStats));
	TestTrue(TEXT("execute reads call_function cache hits"),
		CacheStats && CacheStats->IsValid() && (*CacheStats)->TryGetNumberField(TEXT("hits"), CacheHits));
	TestTrue(TEXT("execute reuses preview target_object call_function resolution"), CacheHits > 0.0);

	TestTrue(TEXT("target_object receiver links DoorMesh to SetSimulatePhysics"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::GraphHasVariableGetLinkedToFunctionInput(
			*this,
		Graph,
		DoorMeshComponentName,
		TEXT("SetSimulatePhysics")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteTaskRuntimeCallFunctionTargetObjectPinTypeCacheKeyTest,
	"BlueprintHelper.GraphWrite.TaskRuntime.CallFunction.TargetObjectPinTypeCacheKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteTaskRuntimeCallFunctionTargetObjectPinTypeCacheKeyTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("RuntimeCallFunctionTargetObjectPinType"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	const FName DoorMeshComponentName(TEXT("DoorMesh"));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness Harness;

	FBlueprintHelperAddComponentRequest AddComponentRequest;
	AddComponentRequest.AssetPath = Blueprint->GetPathName();
	AddComponentRequest.ComponentName = DoorMeshComponentName.ToString();
	AddComponentRequest.ComponentClass = TEXT("StaticMeshComponent");
	const FBlueprintHelperToolResultBase AddComponentResult = Harness.ComponentService.AddComponent(AddComponentRequest);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddToolResultFailureDetail(
		*this,
		TEXT("target_object pin cache setup component"),
		AddComponentResult);
	TestTrue(TEXT("DoorMesh StaticMeshComponent fixture is added"), AddComponentResult.bOk);
	if (!AddComponentResult.bOk)
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const FString EventName = TEXT("RuntimeTargetObjectPinTypeCache");
	TestNotNull(TEXT("fixture custom event exists"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, EventName));

	auto MakePayloadWithTargetPinType = [&](const FString& PinObjectPath) -> TSharedRef<FJsonObject>
	{
		return FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTaskPlanPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeEnsureEntrySetSimulatePhysicsTargetObjectOp(
				EventName,
				UStaticMeshComponent::StaticClass()->GetPathName(),
				PinObjectPath),
			true);
	};

	const FBlueprintHelperToolResultBase PrimitivePreview = Harness.RuntimeService.PreviewTaskPlan(
		MakePayloadWithTargetPinType(UPrimitiveComponent::StaticClass()->GetPathName()));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertRuntimePreviewReachedGraphWriteService(
		*this,
		PrimitivePreview,
		TEXT("replace_blueprint_graph"),
		true);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddToolResultFailureDetail(
		*this,
		TEXT("target_object primitive pin preview"),
		PrimitivePreview);

	const FBlueprintHelperToolResultBase StaticMeshPreview = Harness.RuntimeService.PreviewTaskPlan(
		MakePayloadWithTargetPinType(UStaticMeshComponent::StaticClass()->GetPathName()));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertRuntimePreviewReachedGraphWriteService(
		*this,
		StaticMeshPreview,
		TEXT("replace_blueprint_graph"),
		true);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddToolResultFailureDetail(
		*this,
		TEXT("target_object static mesh pin preview"),
		StaticMeshPreview);

	const TSharedPtr<FJsonObject>* CacheStats = nullptr;
	double CacheHits = 0.0;
	double CacheMisses = 0.0;
	double CacheEntries = 0.0;
	TestTrue(TEXT("second preview exposes call_function cache stats"),
		StaticMeshPreview.Data.IsValid() && StaticMeshPreview.Data->TryGetObjectField(TEXT("call_function_resolution_cache"), CacheStats));
	if (CacheStats && CacheStats->IsValid())
	{
		TestTrue(TEXT("second preview reads call_function cache hits"), (*CacheStats)->TryGetNumberField(TEXT("hits"), CacheHits));
		TestTrue(TEXT("second preview reads call_function cache misses"), (*CacheStats)->TryGetNumberField(TEXT("misses"), CacheMisses));
		TestTrue(TEXT("second preview reads call_function cache entries"), (*CacheStats)->TryGetNumberField(TEXT("entries"), CacheEntries));
		TestEqual(TEXT("target_object pin type changes runtime call_function cache key"), CacheHits, 0.0);
		TestTrue(TEXT("target_object pin type forces a new runtime call_function resolution"), CacheMisses > 0.0);
		TestTrue(TEXT("target_object pin type stores a distinct runtime call_function cache entry"), CacheEntries >= 2.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteCallFunctionResolverPreviewBlocksAmbiguousFunctionTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.PreviewBlocksAmbiguousFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteCallFunctionResolverPreviewBlocksAmbiguousFunctionTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("RuntimeCallFunctionPreviewAmbiguous"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const FString EventName = TEXT("RuntimeAmbiguousCall");
	TestNotNull(TEXT("fixture custom event exists"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, EventName));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness Harness;
	const FBlueprintHelperToolResultBase Preview = Harness.RuntimeService.PreviewTaskPlan(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTaskPlanPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeEnsureEntryCallFunctionNameOp(EventName, TEXT("Set"))));

	TestTrue(TEXT("preview returns dry-run envelope"), Preview.bOk);
	TestEqual(TEXT("preview status is dry-run"), Preview.Status, EBlueprintHelperToolStatus::DryRun);

	const TSharedPtr<FJsonObject>* DryRun = nullptr;
	TestTrue(TEXT("preview has dry_run diagnostics"),
		Preview.Data.IsValid() && Preview.Data->TryGetObjectField(TEXT("dry_run"), DryRun));
	if (DryRun && DryRun->IsValid())
	{
		FString Result;
		bool bCanExecute = true;
		TestTrue(TEXT("dry_run.result exists"), (*DryRun)->TryGetStringField(TEXT("result"), Result));
		TestTrue(TEXT("dry_run.can_execute exists"), (*DryRun)->TryGetBoolField(TEXT("can_execute"), bCanExecute));
		TestEqual(TEXT("dry_run blocks ambiguous function"), Result, FString(TEXT("blocked")));
		TestFalse(TEXT("blocked preview cannot execute"), bCanExecute);
	}

	TSharedPtr<FJsonObject> ErrorObject;
	TestTrue(TEXT("dry_run exposes first error"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::GetRuntimeDryRunFirstErrorObject(Preview, ErrorObject));
	if (ErrorObject.IsValid())
	{
		FString Code;
		FString Stage;
		FString Path;
		ErrorObject->TryGetStringField(TEXT("code"), Code);
		ErrorObject->TryGetStringField(TEXT("stage"), Stage);
		ErrorObject->TryGetStringField(TEXT("path"), Path);
		TestEqual(TEXT("error code"), Code, FString(TEXT("ambiguous_function_call")));
		TestEqual(TEXT("error stage"), Stage, FString(TEXT("dry_run")));
		TestEqual(TEXT("error path"), Path, FString(TEXT("write.ops[0].logic_spec.statements[0].target")));
	}

	TestEqual(TEXT("blocked preview does not create an additional event"), FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::CountCustomEventsByName(Graph, EventName), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteCallFunctionResolverPreviewReportsCandidateSummariesTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.PreviewReportsCandidateSummaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteCallFunctionResolverPreviewReportsCandidateSummariesTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("RuntimeCallFunctionCandidateSummary"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const FString EventName = TEXT("RuntimeCandidateSummaryCall");
	TestNotNull(TEXT("fixture custom event exists"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, EventName));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness Harness;
	const FBlueprintHelperToolResultBase Preview = Harness.RuntimeService.PreviewTaskPlan(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTaskPlanPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeEnsureEntryCallFunctionNameOp(
				EventName,
				TEXT("Set"))));

	TSharedPtr<FJsonObject> ErrorObject;
	TestTrue(TEXT("dry_run exposes first error"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::GetRuntimeDryRunFirstErrorObject(Preview, ErrorObject));

	const TArray<TSharedPtr<FJsonValue>>* CandidateGroups = nullptr;
	TestTrue(TEXT("error includes candidate_functions"),
		ErrorObject.IsValid() && ErrorObject->TryGetArrayField(TEXT("candidate_functions"), CandidateGroups));
	TestTrue(TEXT("candidate_functions has a group"), CandidateGroups && CandidateGroups->Num() > 0);
	if (!CandidateGroups || CandidateGroups->Num() == 0)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Group = (*CandidateGroups)[0].IsValid()
		? (*CandidateGroups)[0]->AsObject()
		: nullptr;
	TestNotNull(TEXT("candidate group is object"), Group.Get());
	if (!Group.IsValid())
	{
		return false;
	}

	FString Query;
	TestTrue(TEXT("candidate group records query"), Group->TryGetStringField(TEXT("query"), Query));
	TestEqual(TEXT("candidate group query"), Query, FString(TEXT("Set")));

	const TArray<TSharedPtr<FJsonValue>>* Candidates = nullptr;
	TestTrue(TEXT("candidate group includes candidates"), Group->TryGetArrayField(TEXT("candidates"), Candidates));
	TestTrue(TEXT("candidate summary exists"), Candidates && Candidates->Num() > 0);
	if (!Candidates || Candidates->Num() == 0)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Candidate = (*Candidates)[0].IsValid()
		? (*Candidates)[0]->AsObject()
		: nullptr;
	TestNotNull(TEXT("candidate summary is object"), Candidate.Get());
	if (!Candidate.IsValid())
	{
		return false;
	}

	FString StableId;
	FString DisplayName;
	FString OwnerClass;
	TestTrue(TEXT("candidate stable_id is present"), Candidate->TryGetStringField(TEXT("stable_id"), StableId));
	TestTrue(TEXT("candidate display_name is present"), Candidate->TryGetStringField(TEXT("display_name"), DisplayName));
	TestTrue(TEXT("candidate owner_class is present"), Candidate->TryGetStringField(TEXT("owner_class"), OwnerClass));
	TestFalse(TEXT("stable_id is not empty"), StableId.IsEmpty());
	TestFalse(TEXT("display_name is not empty"), DisplayName.IsEmpty());
	TestFalse(TEXT("owner_class is not empty"), OwnerClass.IsEmpty());
	TestFalse(TEXT("node_class is not exposed"), Candidate->HasField(TEXT("node_class")));
	TestFalse(TEXT("match_reason is not exposed"), Candidate->HasField(TEXT("match_reason")));
	TestFalse(TEXT("input_pins are not exposed"), Candidate->HasField(TEXT("input_pins")));

	const FString PreviewJson = Preview.ToJsonString();
	TestFalse(TEXT("no node spawner leak"), PreviewJson.Contains(TEXT("UBlueprintFunctionNodeSpawner")));
	TestFalse(TEXT("no schema action leak"), PreviewJson.Contains(TEXT("FEdGraphSchemaAction")));
	TestFalse(TEXT("no binding object leak"), PreviewJson.Contains(TEXT("Binding")));
	TestFalse(TEXT("no selected object payload leak"), PreviewJson.Contains(TEXT("SelectedObjects")));
	TestFalse(TEXT("no debug bundle path leak"), PreviewJson.Contains(TEXT("DebugBundle")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteAutoSearchPreviewRequiresCandidateSelectionTest,
	"BlueprintHelper.GraphWrite.TaskRuntime.AutoSearch.PreviewRequiresCandidateSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteAutoSearchPreviewRequiresCandidateSelectionTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("RuntimeAutoSearchCandidateRequired"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const FString EventName = TEXT("RuntimeAutoSearchCandidateRequired");
	TestNotNull(TEXT("fixture custom event exists"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, EventName));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness Harness;
	const TSharedRef<FJsonObject> TaskPlanPayload =
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTaskPlanPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeEnsureEntryCallFunctionNameOp(
				EventName,
				TEXT("Set")));
	TestTrue(TEXT("auto_search policy attaches to graph_write step"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::SetFirstGraphWriteStepAutoSearchPolicy(
			TaskPlanPayload,
			3,
			16,
			120));

	const FBlueprintHelperToolResultBase Preview = Harness.RuntimeService.PreviewTaskPlan(TaskPlanPayload);
	TestTrue(TEXT("preview returns dry-run envelope"), Preview.bOk);
	TestEqual(TEXT("preview status is dry-run"), Preview.Status, EBlueprintHelperToolStatus::DryRun);

	TSharedPtr<FJsonObject> ErrorObject;
	TestTrue(TEXT("dry_run exposes first error"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::GetRuntimeDryRunFirstErrorObject(Preview, ErrorObject));
	if (ErrorObject.IsValid())
	{
		FString Code;
		FString Status;
		ErrorObject->TryGetStringField(TEXT("code"), Code);
		ErrorObject->TryGetStringField(TEXT("resolution_status"), Status);
		TestEqual(TEXT("candidate-required error code"), Code, FString(TEXT("graphwrite_autosearch_candidate_required")));
		TestEqual(TEXT("candidate-required status"), Status, FString(TEXT("candidate_required")));

		const TArray<TSharedPtr<FJsonValue>>* Candidates = nullptr;
		TestTrue(TEXT("candidate-required error includes candidates"),
			ErrorObject->TryGetArrayField(TEXT("candidates"), Candidates));
		TestTrue(TEXT("candidate-required candidates are non-empty"), Candidates && Candidates->Num() > 0);
	}

	const FString PreviewJson = Preview.ToJsonString();
	auto ContainsWithPreviewContext = [this, &PreviewJson](const FString& Needle) -> bool
	{
		const int32 Index = PreviewJson.Find(Needle, ESearchCase::CaseSensitive);
		if (Index != INDEX_NONE)
		{
			const int32 Start = FMath::Max(0, Index - 160);
			AddInfo(FString::Printf(
				TEXT("candidate-required leak context for '%s': %s"),
				*Needle,
				*PreviewJson.Mid(Start, 360)));
			return true;
		}
		return false;
	};
	TestFalse(TEXT("candidate-required output hides stable id"), ContainsWithPreviewContext(TEXT("\"stable_id\"")));
	TestFalse(TEXT("candidate-required output hides spawner signature"), ContainsWithPreviewContext(TEXT("spawner_signature")));
	TestFalse(TEXT("candidate-required output hides internal artifact"), ContainsWithPreviewContext(TEXT("graph_write_candidate_artifact")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteAutoSearchPreviewRetryUsesCurrentProjectionTest,
	"BlueprintHelper.GraphWrite.TaskRuntime.AutoSearch.PreviewRetryUsesCurrentProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteAutoSearchPreviewRetryUsesCurrentProjectionTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("RuntimeAutoSearchPreviewRetry"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const FString EventName = TEXT("RuntimeAutoSearchPreviewRetry");
	TestNotNull(TEXT("fixture custom event exists"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, EventName));

	auto ConfigureAutoSearchStatement = [](const TSharedRef<FJsonObject>& Op, const FString& CandidateId)
	{
		const TSharedPtr<FJsonObject>* LogicSpecPtr = nullptr;
		if (!Op->TryGetObjectField(TEXT("logic_spec"), LogicSpecPtr) || !LogicSpecPtr || !LogicSpecPtr->IsValid())
		{
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* Statements = nullptr;
		if (!(*LogicSpecPtr)->TryGetArrayField(TEXT("statements"), Statements) || !Statements || Statements->Num() == 0)
		{
			return false;
		}
		const TSharedPtr<FJsonObject> Statement = (*Statements)[0].IsValid()
			? (*Statements)[0]->AsObject()
			: nullptr;
		if (!Statement.IsValid())
		{
			return false;
		}
		Statement->SetStringField(TEXT("statement_id"), TEXT("s_autosearch_print"));
		Statement->SetStringField(TEXT("resolution_policy"), TEXT("auto_search"));
		if (!CandidateId.IsEmpty())
		{
			TSharedRef<FJsonObject> ActionSelection = MakeShared<FJsonObject>();
			ActionSelection->SetStringField(TEXT("candidate_id"), CandidateId);
			Statement->SetObjectField(TEXT("action_selection"), ActionSelection);
		}
		return true;
	};

	TSharedRef<FJsonObject> CandidateRequiredOp =
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeEnsureEntryCallFunctionNameOp(
			EventName,
			TEXT("Print"));
	TestTrue(TEXT("candidate-required statement configured"), ConfigureAutoSearchStatement(CandidateRequiredOp, FString()));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness FirstHarness;
	TSharedRef<FJsonObject> CandidatePayload =
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTaskPlanPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			CandidateRequiredOp);
	TestTrue(TEXT("auto_search policy attaches to graph_write step"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::SetFirstGraphWriteStepAutoSearchPolicy(
			CandidatePayload,
			3,
			16,
			120));
	const FBlueprintHelperToolResultBase CandidatePreview = FirstHarness.RuntimeService.PreviewTaskPlan(CandidatePayload);

	TSharedPtr<FJsonObject> ErrorObject;
	TestTrue(TEXT("candidate preview exposes first error"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::GetRuntimeDryRunFirstErrorObject(CandidatePreview, ErrorObject));
	const TArray<TSharedPtr<FJsonValue>>* Candidates = nullptr;
	TestTrue(TEXT("candidate preview includes candidates"),
		ErrorObject.IsValid() && ErrorObject->TryGetArrayField(TEXT("candidates"), Candidates));
	TestTrue(TEXT("candidate preview has at least one candidate"), Candidates && Candidates->Num() > 0);
	if (!Candidates || Candidates->Num() == 0)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Candidate = (*Candidates)[0].IsValid()
		? (*Candidates)[0]->AsObject()
		: nullptr;
	TestNotNull(TEXT("candidate is object"), Candidate.Get());
	if (!Candidate.IsValid())
	{
		return false;
	}
	FString CandidateId;
	TestTrue(TEXT("candidate id is readable"), Candidate->TryGetStringField(TEXT("candidate_id"), CandidateId));
	TestFalse(TEXT("candidate id is not empty"), CandidateId.IsEmpty());

	TSharedRef<FJsonObject> SelectedOp =
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeEnsureEntryCallFunctionNameOp(
			EventName,
			TEXT("Print"));
	TestTrue(TEXT("selected statement configured"), ConfigureAutoSearchStatement(SelectedOp, CandidateId));
	TSharedRef<FJsonObject> SelectedPayload =
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTaskPlanPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			SelectedOp);
	TestTrue(TEXT("auto_search policy attaches to selected graph_write step"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::SetFirstGraphWriteStepAutoSearchPolicy(
			SelectedPayload,
			3,
			16,
			120));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness RetryHarness;
	const FBlueprintHelperToolResultBase SelectedPreview = RetryHarness.RuntimeService.PreviewTaskPlan(SelectedPayload);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertRuntimePreviewReachedGraphWriteService(
		*this,
		SelectedPreview,
		TEXT("replace_blueprint_graph"),
		true);
	TestFalse(TEXT("selected preview does not request another candidate"),
		SelectedPreview.ToJsonString().Contains(TEXT("graphwrite_autosearch_candidate_required")));
	if (SelectedPreview.ToJsonString().Contains(TEXT("graphwrite_autosearch_candidate_required")))
	{
		FString SelectedPayloadJson;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SelectedPayloadJson);
		FJsonSerializer::Serialize(SelectedPayload, Writer);
		AddInfo(FString::Printf(TEXT("selected payload still candidate-required: %s"), *SelectedPayloadJson));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteAutoSearchFastPathDoesNotEmitCandidateRequiredTest,
	"BlueprintHelper.GraphWrite.TaskRuntime.AutoSearch.FastPathDoesNotEmitCandidateRequired",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteAutoSearchFastPathDoesNotEmitCandidateRequiredTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("RuntimeAutoSearchFastPath"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const FString EventName = TEXT("RuntimeAutoSearchFastPath");
	TestNotNull(TEXT("fixture custom event exists"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, EventName));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness Harness;
	const FBlueprintHelperToolResultBase Preview = Harness.RuntimeService.PreviewTaskPlan(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTaskPlanPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeEnsureEntryCallFunctionOp(
				EventName,
				TEXT("Print String"))));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertRuntimePreviewReachedGraphWriteService(
		*this,
		Preview,
		TEXT("replace_blueprint_graph"),
		true);
	TestFalse(TEXT("fast path does not emit candidate required"),
		Preview.ToJsonString().Contains(TEXT("graphwrite_autosearch_candidate_required")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteAutoSearchBudgetExceededTest,
	"BlueprintHelper.GraphWrite.TaskRuntime.AutoSearch.BudgetExceeded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteAutoSearchBudgetExceededTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("RuntimeAutoSearchBudget"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const FString EventName = TEXT("RuntimeAutoSearchBudget");
	TestNotNull(TEXT("fixture custom event exists"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, EventName));

	TSharedRef<FJsonObject> FirstStatement =
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeCallStatement(
			TEXT("Print String"),
			TEXT("budget first statement"));
	FirstStatement->SetStringField(TEXT("resolution_policy"), TEXT("auto_search"));

	TArray<TSharedPtr<FJsonValue>> Statements;
	Statements.Add(MakeShared<FJsonValueObject>(FirstStatement));
	Statements.Add(MakeShared<FJsonValueObject>(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeAutoSearchCallNameStatement(TEXT("Set"))));

	TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
	Op->SetStringField(TEXT("op"), TEXT("replace_body"));
	Op->SetStringField(TEXT("replace_scope"), TEXT("custom_event_body"));
	TSharedRef<FJsonObject> Selector = MakeShared<FJsonObject>();
	Selector->SetStringField(TEXT("entry_name"), EventName);
	Op->SetObjectField(TEXT("selector"), Selector);
	Op->SetObjectField(
		TEXT("logic_spec"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteLogicSpec(FString(), Statements));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness Harness;
	const TSharedRef<FJsonObject> TaskPlanPayload =
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTaskPlanPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			Op);
	TestTrue(TEXT("auto_search policy attaches to graph_write step"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::SetFirstGraphWriteStepAutoSearchPolicy(
			TaskPlanPayload,
			3,
			1,
			120));

	const FBlueprintHelperToolResultBase Preview = Harness.RuntimeService.PreviewTaskPlan(TaskPlanPayload);
	TSharedPtr<FJsonObject> ErrorObject;
	TestTrue(TEXT("dry_run exposes first error"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::GetRuntimeDryRunFirstErrorObject(Preview, ErrorObject));
	if (ErrorObject.IsValid())
	{
		FString Code;
		ErrorObject->TryGetStringField(TEXT("code"), Code);
		TestEqual(TEXT("budget exceeded is reported"), Code, FString(TEXT("graphwrite_autosearch_budget_exceeded")));
	}
	TestTrue(TEXT("preview JSON reports budget exceeded"),
		Preview.ToJsonString().Contains(TEXT("graphwrite_autosearch_budget_exceeded")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteCallFunctionResolverExecuteRevalidatesStableIdTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.ExecuteRevalidatesStableId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteCallFunctionResolverExecuteRevalidatesStableIdTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("RuntimeCallFunctionStableId"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const FString EventName = TEXT("RuntimeStableIdCall");
	UK2Node_CustomEvent* EntryNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, EventName);
	TestNotNull(TEXT("fixture custom event exists"), EntryNode);
	if (!EntryNode)
	{
		return false;
	}
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(
		EntryNode,
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteEntryBlockId(Blueprint, Graph, EventName));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness Harness;
	const TSharedRef<FJsonObject> TaskPlanPayload = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTaskPlanPayload(
		Blueprint->GetPathName(),
		Graph->GetName(),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeEnsureEntryCallFunctionOp(EventName, TEXT("Print String")));

	const FBlueprintHelperToolResultBase Preview = Harness.RuntimeService.PreviewTaskPlan(TaskPlanPayload);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertRuntimePreviewReachedGraphWriteService(
		*this,
		Preview,
		TEXT("replace_blueprint_graph"),
		true);

	TSharedPtr<FJsonObject> PreviewFact;
	TestTrue(TEXT("preview records resolved call_function runtime fact"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::GetFirstResolvedCallFunctionFact(Preview.Data, PreviewFact));
	FString PreviewStableId;
	FString PreviewNativeName;
	FString PreviewDisplayName;
	if (PreviewFact.IsValid())
	{
		PreviewFact->TryGetStringField(TEXT("stable_id"), PreviewStableId);
		PreviewFact->TryGetStringField(TEXT("native_name"), PreviewNativeName);
		PreviewFact->TryGetStringField(TEXT("display_name"), PreviewDisplayName);
		TestEqual(TEXT("preview stable_id"), PreviewStableId, FString(TEXT("/Script/Engine.KismetSystemLibrary:PrintString")));
		TestEqual(TEXT("preview native_name"), PreviewNativeName, FString(TEXT("PrintString")));
		TestFalse(TEXT("preview display_name is present"), PreviewDisplayName.IsEmpty());
	}

	const FBlueprintHelperToolResultBase ExecuteResult = Harness.RuntimeService.ExecuteTaskPlan(TaskPlanPayload);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddToolResultFailureDetail(
		*this,
		TEXT("stable-id revalidation execute"),
		ExecuteResult);
	TestTrue(TEXT("execute succeeds after stable-id revalidation"), ExecuteResult.bOk);
	TSharedPtr<FJsonObject> ExecuteFact;
	TestTrue(TEXT("execute records resolved call_function runtime fact"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::GetFirstResolvedCallFunctionFact(ExecuteResult.Data, ExecuteFact));
	FString ExecuteStableId;
	if (ExecuteFact.IsValid())
	{
		ExecuteFact->TryGetStringField(TEXT("stable_id"), ExecuteStableId);
		TestEqual(TEXT("execute stable_id matches preview"), ExecuteStableId, PreviewStableId);
	}

	FString TaskRunId;
	TestTrue(TEXT("execute result carries task_run_id"),
		ExecuteResult.Data.IsValid() && ExecuteResult.Data->TryGetStringField(TEXT("task_run_id"), TaskRunId));
	TestFalse(TEXT("task_run_id is not empty"), TaskRunId.IsEmpty());
	const FBlueprintHelperToolResultBase JournalResult = Harness.RuntimeService.GetTaskRunJournal(TaskRunId);
	TestTrue(TEXT("journal read succeeds"), JournalResult.bOk);
	TSharedPtr<FJsonObject> JournalFact;
	TestTrue(TEXT("journal records resolved call_function runtime fact"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::GetFirstResolvedCallFunctionFact(JournalResult.Data, JournalFact));
	if (JournalFact.IsValid())
	{
		FString JournalStableId;
		JournalFact->TryGetStringField(TEXT("stable_id"), JournalStableId);
		TestEqual(TEXT("journal stable_id matches execute"), JournalStableId, ExecuteStableId);
	}

	TestTrue(TEXT("execute creates event to resolved PrintString exec link"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ExportHasExecLinkFromCustomEventToFunction(Graph, EventName, TEXT("PrintString")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteTaskRuntimeP6EvidenceBackedReadbackCoverageTest,
	"BlueprintHelper.GraphWrite.TaskRuntime.P6.EvidenceBackedReadbackCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteTaskRuntimeP6EvidenceBackedReadbackCoverageTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("RuntimeP6ReadbackCoverage"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const FString EventName = TEXT("RuntimeP6ReadbackEvent");
	UK2Node_CustomEvent* EntryNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, EventName);
	TestNotNull(TEXT("fixture custom event exists"), EntryNode);
	if (!EntryNode)
	{
		return false;
	}
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MarkGraphWriteNodeAsBlueprintHelperOwned(
		EntryNode,
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteEntryBlockId(Blueprint, Graph, EventName));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness Harness;
	const TSharedRef<FJsonObject> TaskPlanPayload = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTaskPlanPayload(
		Blueprint->GetPathName(),
		Graph->GetName(),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeEnsureEntryCallFunctionOp(EventName, TEXT("Print String")));

	const FBlueprintHelperToolResultBase Preview = Harness.RuntimeService.PreviewTaskPlan(TaskPlanPayload);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertRuntimePreviewReachedGraphWriteService(
		*this,
		Preview,
		TEXT("replace_blueprint_graph"),
		true);

	TSharedPtr<FJsonObject> PreviewFact;
	TestTrue(TEXT("preview records resolver evidence before readback"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::GetFirstResolvedCallFunctionFact(Preview.Data, PreviewFact));
	FString StableId;
	if (PreviewFact.IsValid())
	{
		PreviewFact->TryGetStringField(TEXT("stable_id"), StableId);
	}
	TestEqual(TEXT("selected stable id is PrintString"),
		StableId,
		FString(TEXT("/Script/Engine.KismetSystemLibrary:PrintString")));

	const FBlueprintHelperToolResultBase ExecuteResult = Harness.RuntimeService.ExecuteTaskPlan(TaskPlanPayload);
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddToolResultFailureDetail(
		*this,
		TEXT("P6 evidence-backed readback execute"),
		ExecuteResult);
	TestTrue(TEXT("execute creates graph before readback"), ExecuteResult.bOk);
	if (!ExecuteResult.bOk)
	{
		return false;
	}

	const FName VariableName(TEXT("bP6GateOpen"));
	FEdGraphPinType BoolPinType(
		UEdGraphSchema_K2::PC_Boolean,
		NAME_None,
		nullptr,
		EPinContainerType::None,
		false,
		FEdGraphTerminalType());
	FBlueprintEditorUtils::AddMemberVariable(Blueprint, VariableName, BoolPinType);
	UK2Node_VariableGet* VariableGet = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteVariableGetNode(Graph, VariableName);
	UK2Node_VariableSet* VariableSet = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteVariableSetNode(Graph, VariableName);
	UK2Node_IfThenElse* BranchNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteBranchNode(Graph);
	UK2Node_ExecutionSequence* SequenceNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteSequenceNode(Graph);
	UK2Node_Select* SelectNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteSelectNode(Graph);
	UK2Node_MakeStruct* MakeVectorNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteMakeStructNode(Graph, TBaseStructure<FVector>::Get());
	UK2Node_BreakStruct* BreakRotatorNode = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteBreakStructNode(Graph, TBaseStructure<FRotator>::Get());
	TestNotNull(TEXT("variable get fixture exists"), VariableGet);
	TestNotNull(TEXT("variable set fixture exists"), VariableSet);
	TestNotNull(TEXT("branch fixture exists"), BranchNode);
	TestNotNull(TEXT("sequence fixture exists"), SequenceNode);
	TestNotNull(TEXT("select fixture exists"), SelectNode);
	TestNotNull(TEXT("make vector fixture exists"), MakeVectorNode);
	TestNotNull(TEXT("break rotator fixture exists"), BreakRotatorNode);

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteReadbackEvidence FunctionStableIdEvidence;
	FunctionStableIdEvidence.bHasResolverEvidence = true;
	FunctionStableIdEvidence.bHasSpawnEvidence = true;
	FunctionStableIdEvidence.SelectedStableId = StableId;
	FunctionStableIdEvidence.SelectedSpawnerClass = TEXT("UBlueprintFunctionNodeSpawner");
	TestTrue(TEXT("readback locates function call by selected stable id"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ReadbackFindsFunctionCallByEvidence(Graph, FunctionStableIdEvidence));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteReadbackEvidence FunctionRefEvidence = FunctionStableIdEvidence;
	FunctionRefEvidence.SelectedStableId.Reset();
	FunctionRefEvidence.FunctionReference = TEXT("PrintString");
	TestTrue(TEXT("readback locates function call by function reference"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ReadbackFindsFunctionCallByEvidence(Graph, FunctionRefEvidence));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteReadbackEvidence FieldEvidence;
	FieldEvidence.bHasResolverEvidence = true;
	FieldEvidence.bHasSpawnEvidence = true;
	FieldEvidence.SelectedStableId = TEXT("field:self:bP6GateOpen");
	FieldEvidence.SelectedSpawnerClass = TEXT("UBlueprintVariableNodeSpawner");
	FieldEvidence.FieldName = VariableName.ToString();
	TestTrue(TEXT("readback locates variable get by field evidence"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ReadbackFindsVariableNodeByEvidence(Graph, FieldEvidence, false));
	TestTrue(TEXT("readback locates variable set by field evidence"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ReadbackFindsVariableNodeByEvidence(Graph, FieldEvidence, true));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteReadbackEvidence SingletonEvidence;
	SingletonEvidence.bHasResolverEvidence = true;
	SingletonEvidence.bHasSpawnEvidence = true;
	SingletonEvidence.SelectedSpawnerClass = TEXT("UBlueprintNodeSpawner");
	SingletonEvidence.SingletonStableId = TEXT("singleton_control_flow:branch");
	TestTrue(TEXT("readback locates branch by singleton stable id"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ReadbackFindsSingletonControlByEvidence(Graph, SingletonEvidence));
	SingletonEvidence.SingletonStableId = TEXT("singleton_control_flow:sequence");
	TestTrue(TEXT("readback locates sequence by singleton stable id"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ReadbackFindsSingletonControlByEvidence(Graph, SingletonEvidence));
	SingletonEvidence.SingletonStableId = TEXT("singleton_control_flow:select");
	TestTrue(TEXT("readback locates select by singleton stable id"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ReadbackFindsSingletonControlByEvidence(Graph, SingletonEvidence));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteReadbackEvidence EventEvidence;
	EventEvidence.bHasResolverEvidence = true;
	EventEvidence.bHasSpawnEvidence = true;
	EventEvidence.SelectedStableId = FString::Printf(TEXT("event:%s"), *EventName);
	EventEvidence.SelectedSpawnerClass = TEXT("UBlueprintEventNodeSpawner");
	EventEvidence.EventName = EventName;
	TestTrue(TEXT("readback locates custom event by event name"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ReadbackFindsCustomEventByEvidence(Graph, EventEvidence));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteReadbackEvidence StructEvidence;
	StructEvidence.bHasResolverEvidence = true;
	StructEvidence.bHasSpawnEvidence = true;
	StructEvidence.SelectedSpawnerClass = TEXT("UBlueprintNodeSpawner");
	StructEvidence.StructTypeName = TEXT("Vector");
	TestTrue(TEXT("readback locates make struct by struct type"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ReadbackFindsStructNodeByEvidence(Graph, StructEvidence, true));
	StructEvidence.StructTypeName = TEXT("Rotator");
	TestTrue(TEXT("readback locates break struct by struct type"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ReadbackFindsStructNodeByEvidence(Graph, StructEvidence, false));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteReadbackEvidence ReadbackOnly = FunctionStableIdEvidence;
	ReadbackOnly.bHasResolverEvidence = false;
	ReadbackOnly.bHasSpawnEvidence = false;
	TestFalse(TEXT("readback alone cannot create function success evidence"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ReadbackFindsFunctionCallByEvidence(Graph, ReadbackOnly));

	ReadbackOnly = FieldEvidence;
	ReadbackOnly.bHasResolverEvidence = false;
	ReadbackOnly.bHasSpawnEvidence = false;
	TestFalse(TEXT("readback alone cannot create variable success evidence"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ReadbackFindsVariableNodeByEvidence(Graph, ReadbackOnly, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteTaskRuntimeCallFunctionMemberPrefixPreviewBlocksTest,
	"BlueprintHelper.GraphWrite.TaskRuntime.CallFunction.MemberPrefixPreviewBlocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteTaskRuntimeCallFunctionMemberPrefixPreviewBlocksTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("RuntimeCallFunctionMemberPrefix"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	TestNotNull(TEXT("fixture custom event exists"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AddGraphWriteCustomEvent(Graph, TEXT("RuntimeMemberPrefixCall")));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness Harness;
	const FBlueprintHelperToolResultBase Preview = Harness.RuntimeService.PreviewTaskPlan(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTaskPlanPayload(
			Blueprint->GetPathName(),
			Graph->GetName(),
			FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeEnsureEntryCallFunctionOp(
				TEXT("RuntimeMemberPrefixCall"),
				TEXT("DoorMesh.AddAngularImpulseInDegrees"))));

	TestTrue(TEXT("member-prefix preview returns dry-run envelope"), Preview.bOk);
	TestEqual(TEXT("member-prefix preview status is dry-run"), Preview.Status, EBlueprintHelperToolStatus::DryRun);

	TSharedPtr<FJsonObject> ErrorObject;
	TestTrue(TEXT("member-prefix preview exposes first error"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::GetRuntimeDryRunFirstErrorObject(Preview, ErrorObject));
	if (ErrorObject.IsValid())
	{
		FString Code;
		FString Stage;
		FString Path;
		ErrorObject->TryGetStringField(TEXT("code"), Code);
		ErrorObject->TryGetStringField(TEXT("stage"), Stage);
		ErrorObject->TryGetStringField(TEXT("path"), Path);
		TestEqual(TEXT("member-prefix code"), Code, FString(TEXT("explicit_member_call_not_supported")));
		TestEqual(TEXT("member-prefix stage"), Stage, FString(TEXT("dry_run")));
		TestEqual(TEXT("member-prefix path"), Path, FString(TEXT("write.ops[0].logic_spec.statements[0].target")));
	}

	TestEqual(TEXT("member-prefix preview does not create an additional event"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::CountCustomEventsByName(Graph, TEXT("RuntimeMemberPrefixCall")),
		1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeUMGWidgetTreeExecuteSmokeTest,
	"BlueprintHelper.TaskRuntime.UMGWidget.WidgetTreeExecuteSmoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskRuntimeUMGWidgetTreeExecuteSmokeTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FWidgetRuntimeDryRunFixture Fixture =
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeWidgetRuntimeExecuteFixture(TEXT("RuntimeUMGExecute"));
	TestNotNull(TEXT("WidgetBlueprint fixture is created"), Fixture.Blueprint);
	TestNotNull(TEXT("WidgetTree fixture is created"), Fixture.Blueprint ? Fixture.Blueprint->WidgetTree.Get() : nullptr);
	if (!Fixture.Blueprint || !Fixture.Blueprint->WidgetTree)
	{
		return false;
	}

	TestNull(TEXT("fixture starts without a root widget"), Fixture.Blueprint->WidgetTree->RootWidget);

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness Harness;
	const FBlueprintHelperToolResultBase ExecuteResult = Harness.RuntimeService.ExecuteTaskPlan(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeWidgetTreeExecutePayload(Fixture.Blueprint->GetPathName()));

	TestTrue(TEXT("UMG widget tree execute succeeds"), ExecuteResult.bOk);
	if (!ExecuteResult.bOk && ExecuteResult.Error.IsSet())
	{
		TestFalse(TEXT("execute failure has diagnosable message"), ExecuteResult.Error->Message.IsEmpty());
	}
	TestEqual(TEXT("UMG execute status is applied"), ExecuteResult.Status, EBlueprintHelperToolStatus::Applied);

	FString TaskRunId;
	TestTrue(TEXT("execute result carries task_run_id"),
		ExecuteResult.Data.IsValid() && ExecuteResult.Data->TryGetStringField(TEXT("task_run_id"), TaskRunId));
	const FBlueprintHelperToolResultBase JournalResult = Harness.RuntimeService.GetTaskRunJournal(TaskRunId);
	TestTrue(TEXT("TaskRunJournal can be loaded for UMG execute"), JournalResult.bOk);
	FString JournalStatus;
	TestTrue(TEXT("TaskRunJournal has status"),
		JournalResult.Data.IsValid() && JournalResult.Data->TryGetStringField(TEXT("status"), JournalStatus));
	TestEqual(TEXT("TaskRunJournal completed"), JournalStatus, FString(TEXT("completed")));

	UWidget* RootWidget = Fixture.Blueprint->WidgetTree->RootWidget;
	TestNotNull(TEXT("RootCanvas becomes the root widget"), RootWidget);
	TestEqual(TEXT("root widget name"), RootWidget ? RootWidget->GetName() : FString(), FString(TEXT("RootCanvas")));

	UTextBlock* SmokeText = Cast<UTextBlock>(Fixture.Blueprint->WidgetTree->FindWidget(FName(TEXT("SmokeText"))));
	TestNotNull(TEXT("SmokeText child is created"), SmokeText);
	TestEqual(TEXT("SmokeText opacity is written through TaskRuntime"),
		SmokeText ? SmokeText->GetRenderOpacity() : -1.0f,
		0.35f);

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(RootWidget);
	TestNotNull(TEXT("root widget is a CanvasPanel"), RootCanvas);
	TestEqual(TEXT("RootCanvas has one child"), RootCanvas ? RootCanvas->GetChildrenCount() : 0, 1);
	TestEqual(TEXT("RootCanvas child is SmokeText"),
		RootCanvas && RootCanvas->GetChildrenCount() > 0 ? RootCanvas->GetChildAt(0) : nullptr,
		Cast<UWidget>(SmokeText));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeUMGWidgetDryRunUsesPlannedWidgetStateTest,
	"BlueprintHelper.TaskRuntime.UMGWidget.DryRunUsesPlannedWidgetState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskRuntimeUMGWidgetDryRunUsesPlannedWidgetStateTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FWidgetRuntimeDryRunFixture Fixture =
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeWidgetRuntimeDryRunFixture(TEXT("RuntimeUMGPlannedWidget"));
	TestNotNull(TEXT("WidgetBlueprint fixture is created"), Fixture.Blueprint);
	TestNotNull(TEXT("WidgetBlueprint root exists"), Fixture.Root);
	if (!Fixture.Blueprint || !Fixture.Root)
	{
		return false;
	}

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness Harness;
	const FBlueprintHelperToolResultBase Preview = Harness.RuntimeService.PreviewTaskPlan(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeWidgetPlannedPropertyDryRunPayload(Fixture.Blueprint->GetPathName()));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertRuntimePreviewCanExecute(*this, Preview, 2);
	TestNull(TEXT("dry-run does not create planned widget"),
		Fixture.Blueprint->WidgetTree->FindWidget(FName(TEXT("PlannedText"))));

	const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
	TestTrue(TEXT("preview exposes child steps"),
		Preview.Data.IsValid() && Preview.Data->TryGetArrayField(TEXT("steps"), Steps));
	if (Steps && Steps->Num() >= 2)
	{
		const TSharedPtr<FJsonObject> SecondStep = (*Steps)[1].IsValid() ? (*Steps)[1]->AsObject() : nullptr;
		const TSharedPtr<FJsonObject>* ResultObject = nullptr;
		const TSharedPtr<FJsonObject>* DataObject = nullptr;
		const TSharedPtr<FJsonObject>* DryRunObject = nullptr;
		FString PreviewKind;
		TestTrue(TEXT("planned widget second step has result"),
			SecondStep.IsValid() && SecondStep->TryGetObjectField(TEXT("result"), ResultObject));
		TestTrue(TEXT("planned widget second step has data"),
			ResultObject && ResultObject->IsValid() && (*ResultObject)->TryGetObjectField(TEXT("data"), DataObject));
		TestTrue(TEXT("planned widget second step has dry_run"),
			DataObject && DataObject->IsValid() && (*DataObject)->TryGetObjectField(TEXT("dry_run"), DryRunObject));
		TestTrue(TEXT("planned widget preview kind is explicit"),
			DryRunObject && DryRunObject->IsValid() && (*DryRunObject)->TryGetStringField(TEXT("preview_kind"), PreviewKind));
		TestEqual(TEXT("planned widget preview kind"), PreviewKind, FString(TEXT("task_runtime_planned_widget")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeDataTableDryRunUsesPlannedRowStateTest,
	"BlueprintHelper.TaskRuntime.DataTable.DryRunUsesPlannedRowState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskRuntimeDataTableDryRunUsesPlannedRowStateTest::RunTest(const FString& Parameters)
{
	UDataTable* DataTable = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeVectorDataTableRuntimeDryRunFixture(TEXT("RuntimeDataTablePlannedRow"));
	TestNotNull(TEXT("DataTable fixture is created"), DataTable);
	if (!DataTable)
	{
		return false;
	}

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness Harness;
	const FBlueprintHelperToolResultBase Preview = Harness.RuntimeService.PreviewTaskPlan(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeDataTablePlannedRowUpdateDryRunPayload(DataTable->GetPathName()));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertRuntimePreviewCanExecute(*this, Preview, 2);
	TestNull(TEXT("dry-run does not create planned row"),
		DataTable->FindRowUnchecked(FName(TEXT("FutureRow"))));

	const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
	TestTrue(TEXT("preview exposes child steps"),
		Preview.Data.IsValid() && Preview.Data->TryGetArrayField(TEXT("steps"), Steps));
	if (Steps && Steps->Num() >= 2)
	{
		const TSharedPtr<FJsonObject> SecondStep = (*Steps)[1].IsValid() ? (*Steps)[1]->AsObject() : nullptr;
		const TSharedPtr<FJsonObject>* ResultObject = nullptr;
		const TSharedPtr<FJsonObject>* DataObject = nullptr;
		const TSharedPtr<FJsonObject>* DryRunObject = nullptr;
		FString PreviewKind;
		TestTrue(TEXT("planned row second step has result"),
			SecondStep.IsValid() && SecondStep->TryGetObjectField(TEXT("result"), ResultObject));
		TestTrue(TEXT("planned row second step has data"),
			ResultObject && ResultObject->IsValid() && (*ResultObject)->TryGetObjectField(TEXT("data"), DataObject));
		TestTrue(TEXT("planned row second step has dry_run"),
			DataObject && DataObject->IsValid() && (*DataObject)->TryGetObjectField(TEXT("dry_run"), DryRunObject));
		TestTrue(TEXT("planned row preview kind is explicit"),
			DryRunObject && DryRunObject->IsValid() && (*DryRunObject)->TryGetStringField(TEXT("preview_kind"), PreviewKind));
		TestEqual(TEXT("planned row preview kind"), PreviewKind, FString(TEXT("task_runtime_planned_data_table_row")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeGraphWriteDryRunUsesPlannedMemberVariableStateTest,
	"BlueprintHelper.TaskRuntime.GraphWrite.DryRunUsesPlannedMemberVariableState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskRuntimeGraphWriteDryRunUsesPlannedMemberVariableStateTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("RuntimeGraphWritePlannedVariable"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	const FString AssetPath = Blueprint->GetPathName();
	const FString GraphName = Blueprint->UbergraphPages[0]->GetName();
	const FString OwnerClassPath = Blueprint->GeneratedClass
		? Blueprint->GeneratedClass->GetPathName()
		: FString();
	TestFalse(TEXT("planned variable is not physically present before preview"),
		Blueprint->NewVariables.ContainsByPredicate([](const FBPVariableDescription& Variable)
		{
			return Variable.VarName == FName(TEXT("PlannedSessionId"));
		}));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness Harness;
	const FBlueprintHelperToolResultBase Preview = Harness.RuntimeService.PreviewTaskPlan(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWritePlannedVariableDryRunPayload(
			AssetPath,
			GraphName,
			OwnerClassPath));

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertRuntimePreviewCanExecute(*this, Preview, 3);
	const TSharedPtr<FJsonObject> GraphStep =
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FindRuntimePreviewStep(Preview, TEXT("step_graph"));
	TestNotNull(TEXT("planned variable preview records graph_write step"), GraphStep.Get());
	bool bGraphStepOk = false;
	FString GraphStepResultJson;
	TestTrue(TEXT("graph_write step exposes result ok"),
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::TryReadRuntimePreviewStepResultOk(
			GraphStep,
			bGraphStepOk,
			GraphStepResultJson));
	if (!bGraphStepOk)
	{
		AddError(FString::Printf(TEXT("graph_write planned-variable dry-run failed: %s"), *GraphStepResultJson));
	}
	TestTrue(TEXT("graph_write dry-run can resolve the planned member variable"), bGraphStepOk);
	TestFalse(TEXT("dry-run does not persist planned member variable"),
		Blueprint->NewVariables.ContainsByPredicate([](const FBPVariableDescription& Variable)
		{
			return Variable.VarName == FName(TEXT("PlannedSessionId"));
		}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteTaskRuntimeReplacePatchMergeDryRunEnvelopeTest,
	"BlueprintHelper.GraphWrite.TaskRuntime.ReplacePatchMergeDryRunEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteTaskRuntimeReplacePatchMergeDryRunEnvelopeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTestBlueprint(TEXT("TaskRuntimeGraphWriteDryRun"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	const FString AssetPath = Blueprint->GetPathName();
	const FString GraphName = Blueprint->UbergraphPages[0]->GetName();

	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteRuntimeHarness Harness;

	const FBlueprintHelperToolResultBase ReplacePreview = Harness.RuntimeService.PreviewTaskPlan(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTaskPlanPayload(AssetPath, GraphName, FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeReplaceBodyOp()));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertRuntimePreviewReachedGraphWriteService(
		*this,
		ReplacePreview,
		TEXT("replace_blueprint_graph"),
		true);

	const FBlueprintHelperToolResultBase PatchPreview = Harness.RuntimeService.PreviewTaskPlan(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTaskPlanPayload(AssetPath, GraphName, FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeSetPinDefaultOp()));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertRuntimePreviewReachedGraphWriteService(
		*this,
		PatchPreview,
		TEXT("patch_blueprint_graph"),
		false);

	const FBlueprintHelperToolResultBase MergePreview = Harness.RuntimeService.PreviewTaskPlan(
		FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeGraphWriteTaskPlanPayload(AssetPath, GraphName, FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::MakeInsertFlowOp()));
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::AssertRuntimePreviewReachedGraphWriteService(
		*this,
		MergePreview,
		TEXT("merge_blueprint_graph"),
		false);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteEventDelegateToolResultReadbackContractTest,
	"BlueprintHelper.GraphWrite.ToolResult.EventDelegate.ReadbackFactContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteEventDelegateToolResultReadbackContractTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphWriteCapabilityCaseResult Result;
	Result.CaseName = TEXT("event_delegate_readback_contract");
	Result.Phase = TEXT("GraphWrite");
	Result.Capability = TEXT("EventDelegate");
	Result.SemanticKind = TEXT("delegate");
	Result.ClusterKind = TEXT("EventDelegateAction");
	Result.ErrorKind = EBlueprintHelperGraphWriteCapabilityErrorKind::None;
	Result.bGraphWriteCorrect = true;
	Result.bCallCorrect = true;
	Result.bHasResolverEvidence = true;
	Result.bHasSpawnEvidence = true;
	Result.bReadbackComplete = true;
	Result.SelectedStableId = TEXT("delegate:call:/Script/Engine.PrimitiveComponent:OnComponentBeginOverlap");
	Result.SelectedSpawnerClass = TEXT("UBlueprintNodeSpawner");
	Result.SpawnedNodeClass = TEXT("UK2Node_CallDelegate");
	Result.PinDefaultLinkReadbackSummary = TEXT("pin.call_arg.bSweep.default=true;pin.call_arg.bSweep.linked_source_pin=bSource");

	const TSharedRef<FJsonObject> Json =
		FBlueprintHelperGraphWriteCapabilityMetrics::ToDebugBundleFailureSummary(Result);
	const TArray<TSharedPtr<FJsonValue>>* FactKeys = nullptr;
	TestTrue(TEXT("event delegate readback fact keys are exposed"), Json->TryGetArrayField(TEXT("event_delegate_readback_fact_keys"), FactKeys));
	bool bHasCorrelationKey = false;
	bool bHasCallArgDefault = false;
	if (FactKeys)
	{
		for (const TSharedPtr<FJsonValue>& KeyValue : *FactKeys)
		{
			const FString Key = KeyValue.IsValid() ? KeyValue->AsString() : FString();
			bHasCorrelationKey |= Key == TEXT("compile_diagnostic_correlation_key");
			bHasCallArgDefault |= Key == TEXT("pin.call_arg.<name>.default");
		}
	}
	TestTrue(TEXT("readback fact keys include diagnostic correlation"), bHasCorrelationKey);
	TestTrue(TEXT("readback fact keys include call arg defaults"), bHasCallArgDefault);
	TestFalse(TEXT("debug bundle does not publish delegate atomic review target"), Json->HasField(TEXT("delegate_atomic_target")));
	return true;
}

#endif
