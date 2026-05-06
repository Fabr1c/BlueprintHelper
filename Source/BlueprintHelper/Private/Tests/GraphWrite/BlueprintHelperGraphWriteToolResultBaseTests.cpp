#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "GraphSupport/BlueprintHelperBlockIdService.h"
#include "GraphSupport/BlueprintHelperGraphResolver.h"
#include "GraphSupport/BlueprintHelperGraphSnapshotService.h"
#include "GraphSupport/BlueprintHelperOwnershipService.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "GraphWrite/BlueprintTextConverter.h"
#include "Logic/BlueprintHelperLogicJsonPathService.h"
#include "Misc/AutomationTest.h"
#include "Services/BlueprintHelperAgentImportService.h"
#include "Services/AssetFactory/BlueprintHelperAssetFactoryService.h"
#include "Services/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"
#include "Services/BlueprintComponent/BlueprintHelperComponentService.h"
#include "Services/BlueprintHelperBlueprintStructureService.h"
#include "Services/BlueprintVariables/BlueprintHelperBlueprintVariableService.h"
#include "Services/DataTable/BlueprintHelperDataTableService.h"
#include "Services/GraphWrite/BlueprintHelperAppendBlueprintGraphService.h"
#include "Services/GraphWrite/BlueprintHelperMergeBlueprintGraphService.h"
#include "Services/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h"
#include "Services/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.h"
#include "Services/RuntimeDiagnostics/BlueprintHelperAssetBrowseService.h"
#include "Services/RuntimeDiagnostics/BlueprintHelperCompileAssetService.h"
#include "Services/RuntimeDiagnostics/BlueprintHelperCompileService.h"
#include "Services/UMGWidget/BlueprintHelperWidgetService.h"
#include "TaskRuntime/BlueprintHelperTaskRuntimeService.h"
#include "Transactions/Transactions/BlueprintHelperTransactionJournalService.h"
#include "UObject/Class.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace
{
	FString MakeGraphWriteTestObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	UPackage* MakeGraphWriteTestPackage(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperGraphWrite/%s"),
			*MakeGraphWriteTestObjectName(Prefix)));
		Package->SetDirtyFlag(false);
		return Package;
	}

	UBlueprint* MakeGraphWriteTestBlueprint(const FString& Prefix)
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

	UEdGraph* AddGraphWriteFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName)
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

	TSharedRef<FJsonObject> MakeAppendPreviewPayload(const FString& AssetPath, const FString& GraphName)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Target->SetStringField(TEXT("graph"), GraphName);
		Payload->SetObjectField(TEXT("target"), Target);
		Payload->SetBoolField(TEXT("dry_run"), true);

		TSharedRef<FJsonObject> EntryNode = MakeShared<FJsonObject>();
		EntryNode->SetStringField(TEXT("id"), TEXT("entry_01"));
		EntryNode->SetStringField(TEXT("kind"), TEXT("custom_event"));
		EntryNode->SetStringField(TEXT("name"), TEXT("SmokeCustomEvent"));

		TArray<TSharedPtr<FJsonValue>> Nodes;
		Nodes.Add(MakeShared<FJsonValueObject>(EntryNode));
		Payload->SetArrayField(TEXT("nodes"), Nodes);
		Payload->SetArrayField(TEXT("links"), {});
		return Payload;
	}

	TSharedRef<FJsonObject> MakeReplacementNode()
	{
		TSharedRef<FJsonObject> Node = MakeShared<FJsonObject>();
		Node->SetStringField(TEXT("id"), TEXT("replacement_01"));
		Node->SetStringField(TEXT("kind"), TEXT("call"));
		Node->SetStringField(TEXT("function"), TEXT("PrintString"));
		return Node;
	}

	TSharedRef<FJsonObject> MakeReplacePreviewPayload(const FString& AssetPath, const FString& GraphName)
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

		TArray<TSharedPtr<FJsonValue>> Nodes;
		Nodes.Add(MakeShared<FJsonValueObject>(MakeReplacementNode()));
		TSharedRef<FJsonObject> Replacement = MakeShared<FJsonObject>();
		Replacement->SetArrayField(TEXT("nodes"), Nodes);
		Replacement->SetArrayField(TEXT("links"), {});
		Payload->SetObjectField(TEXT("replacement"), Replacement);

		TSharedRef<FJsonObject> Options = MakeShared<FJsonObject>();
		Options->SetBoolField(TEXT("dry_run"), true);
		Payload->SetObjectField(TEXT("options"), Options);
		return Payload;
	}

	TSharedRef<FJsonObject> MakeReplaceExecutePayload(const FString& AssetPath, const FString& GraphName)
	{
		TSharedRef<FJsonObject> Payload = MakeReplacePreviewPayload(AssetPath, GraphName);

		const TSharedPtr<FJsonObject>* Options = nullptr;
		if (Payload->TryGetObjectField(TEXT("options"), Options) && Options && Options->IsValid())
		{
			(*Options)->SetBoolField(TEXT("dry_run"), false);
		}
		return Payload;
	}

	UK2Node_CustomEvent* AddGraphWriteCustomEvent(UEdGraph* Graph, const FString& EventName)
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

	UK2Node_CallFunction* AddGraphWritePrintStringCall(UEdGraph* Graph)
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

	UEdGraphPin* FindFirstExecPin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
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

	bool ConnectFirstExecPins(UEdGraphNode* FromNode, UEdGraphNode* ToNode)
	{
		UEdGraphPin* FromPin = FindFirstExecPin(FromNode, EGPD_Output);
		UEdGraphPin* ToPin = FindFirstExecPin(ToNode, EGPD_Input);
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		return FromPin && ToPin && Schema && Schema->TryCreateConnection(FromPin, ToPin);
	}

	void MarkGraphWriteNodeAsBlueprintHelperOwned(UEdGraphNode* Node, const FString& BlockId)
	{
		if (!Node)
		{
			return;
		}

		if (UPackage* Package = Node->GetOutermost())
		{
			FMetaData& MetaData = Package->GetMetaData();
			MetaData.SetValue(Node, TEXT("BlueprintHelperOwned"), TEXT("true"));
			MetaData.SetValue(Node, TEXT("BlueprintHelperBlockId"), *BlockId);
		}
	}

	bool NodeHasBlueprintHelperBlockId(UEdGraphNode* Node, const FString& BlockId)
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

		FMetaData& MetaData = Package->GetMetaData();
		return MetaData.GetValue(Node, TEXT("BlueprintHelperOwned")) == TEXT("true") &&
			MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId")) == BlockId;
	}

	bool ExportHasExecLinkFromCustomEventToFunction(
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

	TSharedRef<FJsonObject> MakePatchPreviewPayload(const FString& AssetPath, const FString& GraphName)
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

	TSharedRef<FJsonObject> MakeMergePreviewPayload(const FString& AssetPath, const FString& GraphName)
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

	void AssertBlockedDryRunFailure(
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

	TSharedRef<FJsonObject> MakeGraphWriteTaskPlanPayload(
		const FString& AssetPath,
		const FString& GraphName,
		const TSharedRef<FJsonObject>& Op)
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
		ExecutionPolicy->SetBoolField(TEXT("should_compile"), false);
		ExecutionPolicy->SetBoolField(TEXT("should_save"), false);
		TaskPlan->SetObjectField(TEXT("execution_policy"), ExecutionPolicy);

		TArray<TSharedPtr<FJsonValue>> Steps;
		Steps.Add(MakeShared<FJsonValueObject>(Step));
		TaskPlan->SetArrayField(TEXT("steps"), Steps);

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("task_plan"), TaskPlan);
		return Payload;
	}

	TSharedRef<FJsonObject> MakeReplaceBodyOp()
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("replace_body"));
		Op->SetStringField(TEXT("replace_scope"), TEXT("custom_event_body"));

		TSharedRef<FJsonObject> Selector = MakeShared<FJsonObject>();
		Selector->SetStringField(TEXT("entry_name"), TEXT("SmokeCustomEvent"));
		Op->SetObjectField(TEXT("selector"), Selector);

		TArray<TSharedPtr<FJsonValue>> Nodes;
		Nodes.Add(MakeShared<FJsonValueObject>(MakeReplacementNode()));
		TSharedRef<FJsonObject> Replacement = MakeShared<FJsonObject>();
		Replacement->SetArrayField(TEXT("nodes"), Nodes);
		Replacement->SetArrayField(TEXT("links"), {});
		Op->SetObjectField(TEXT("replacement"), Replacement);
		return Op;
	}

	TSharedRef<FJsonObject> MakeSetPinDefaultOp()
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

	TSharedRef<FJsonObject> MakeInsertFlowOp()
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

	struct FGraphWriteRuntimeHarness
	{
		FBlueprintHelperGraphResolver Resolver;
		FBlueprintHelperCompileService CompileService;
		FBlueprintHelperAssetBrowseService AssetBrowseService;
		FBlueprintHelperAgentImportService AgentImportService;
		FBlueprintHelperBlockIdService BlockIdService;
		FBlueprintHelperOwnershipService OwnershipService;
		FBlueprintHelperTransactionJournalService JournalService;
		FBlueprintHelperAppendBlueprintGraphService AppendGraphService;
		FBlueprintHelperGraphSnapshotService SnapshotService;
		FBlueprintHelperReplaceBlueprintGraphService ReplaceGraphService;
		FBlueprintHelperLogicJsonPathService PathService;
		FBlueprintHelperPatchBlueprintGraphService PatchGraphService;
		FBlueprintHelperMergeBlueprintGraphService MergeGraphService;
		FBlueprintHelperBlueprintStructureService StructureService;
		FBlueprintHelperBlueprintVariableService VariableService;
		FBlueprintHelperAssetFactoryService AssetFactoryService;
		FBlueprintHelperComponentService ComponentService;
		FBlueprintHelperClassSettingsService ClassSettingsService;
		FBlueprintHelperWidgetService WidgetService;
		FBlueprintHelperDataTableService DataTableService;
		FBlueprintHelperCompileAssetService CompileAssetService;
		FBlueprintHelperTaskRuntimeService RuntimeService;

		FGraphWriteRuntimeHarness()
			: CompileService(Resolver)
			, AgentImportService(Resolver, CompileService, AssetBrowseService)
			, AppendGraphService(Resolver, AgentImportService, BlockIdService, OwnershipService, JournalService)
			, ReplaceGraphService(Resolver, AgentImportService, BlockIdService, OwnershipService, JournalService, SnapshotService)
			, PatchGraphService(Resolver, PathService, JournalService)
			, MergeGraphService(Resolver, PathService, JournalService)
			, StructureService(Resolver)
			, VariableService(Resolver, StructureService)
			, ComponentService(Resolver)
			, ClassSettingsService(Resolver)
			, CompileAssetService(CompileService)
			, RuntimeService(
				AppendGraphService,
				ReplaceGraphService,
				PatchGraphService,
				MergeGraphService,
				VariableService,
				StructureService,
				AssetFactoryService,
				ComponentService,
				ClassSettingsService,
				WidgetService,
				DataTableService,
				CompileAssetService,
				AssetBrowseService)
		{
		}
	};

	void AssertRuntimePreviewReachedGraphWriteService(
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
		Test.TestEqual(TEXT("dry_run.can_execute matches child preflight"), bCanExecute, bExpectedCanExecute);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteAppendBlockedDryRunErrorEnvelopeTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.AppendBlockedDryRunErrorEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteAppendBlockedDryRunErrorEnvelopeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGraphWriteTestBlueprint(TEXT("AppendBlockedDryRun"));
	UEdGraph* FunctionGraph = AddGraphWriteFunctionGraph(Blueprint, TEXT("CalculateSmokeValue"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	TestNotNull(TEXT("function graph is created"), FunctionGraph);
	if (!Blueprint || !FunctionGraph)
	{
		return false;
	}

	const int32 NodeCountBefore = FunctionGraph->Nodes.Num();
	const bool bDirtyBefore = Blueprint->GetOutermost()->IsDirty();

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperAssetBrowseService AssetBrowseService;
	FBlueprintHelperCompileService CompileService(Resolver);
	FBlueprintHelperAgentImportService AgentImportService(Resolver, CompileService, AssetBrowseService);
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperTransactionJournalService JournalService;
	FBlueprintHelperAppendBlueprintGraphService AppendService(
		Resolver,
		AgentImportService,
		BlockIdService,
		OwnershipService,
		JournalService);

	const FBlueprintHelperToolResultBase Result = AppendService.Execute(
		MakeAppendPreviewPayload(Blueprint->GetPathName(), FunctionGraph->GetName()));

	AssertBlockedDryRunFailure(
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
	UBlueprint* Blueprint = MakeGraphWriteTestBlueprint(TEXT("ReplaceBlockedDryRun"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	const int32 UbergraphCountBefore = Blueprint->UbergraphPages.Num();
	const bool bDirtyBefore = Blueprint->GetOutermost()->IsDirty();

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperAssetBrowseService AssetBrowseService;
	FBlueprintHelperCompileService CompileService(Resolver);
	FBlueprintHelperAgentImportService AgentImportService(Resolver, CompileService, AssetBrowseService);
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperTransactionJournalService JournalService;
	FBlueprintHelperGraphSnapshotService SnapshotService;
	FBlueprintHelperReplaceBlueprintGraphService ReplaceService(
		Resolver,
		AgentImportService,
		BlockIdService,
		OwnershipService,
		JournalService,
		SnapshotService);

	const FBlueprintHelperToolResultBase Result = ReplaceService.Execute(
		MakeReplacePreviewPayload(Blueprint->GetPathName(), TEXT("MissingGraph")));

	AssertBlockedDryRunFailure(
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
	FBlueprintHelperGraphWritePatchBlockedDryRunErrorEnvelopeTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.PatchBlockedDryRunErrorEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchBlockedDryRunErrorEnvelopeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGraphWriteTestBlueprint(TEXT("PatchBlockedDryRun"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	const int32 UbergraphCountBefore = Blueprint->UbergraphPages.Num();
	const bool bDirtyBefore = Blueprint->GetOutermost()->IsDirty();

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperTransactionJournalService JournalService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService, JournalService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(
		MakePatchPreviewPayload(Blueprint->GetPathName(), TEXT("MissingGraph")));

	AssertBlockedDryRunFailure(
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
	FBlueprintHelperGraphWriteMergeBlockedDryRunErrorEnvelopeTest,
	"BlueprintHelper.GraphWrite.ToolResultBase.MergeBlockedDryRunErrorEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteMergeBlockedDryRunErrorEnvelopeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGraphWriteTestBlueprint(TEXT("MergeBlockedDryRun"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	const int32 UbergraphCountBefore = Blueprint->UbergraphPages.Num();
	const bool bDirtyBefore = Blueprint->GetOutermost()->IsDirty();

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperTransactionJournalService JournalService;
	FBlueprintHelperMergeBlueprintGraphService MergeService(Resolver, PathService, JournalService);

	const FBlueprintHelperToolResultBase Result = MergeService.Execute(
		MakeMergePreviewPayload(Blueprint->GetPathName(), TEXT("MissingGraph")));

	AssertBlockedDryRunFailure(
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
	UBlueprint* Blueprint = MakeGraphWriteTestBlueprint(TEXT("LogicJsonNodeIndexRef"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	AddGraphWriteCustomEvent(Graph, TEXT("IndexRefFirst"));
	AddGraphWriteCustomEvent(Graph, TEXT("IndexRefSecond"));

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
	UBlueprint* Blueprint = MakeGraphWriteTestBlueprint(TEXT("ReplaceReconnectsEntryExec"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* EntryNode = AddGraphWriteCustomEvent(Graph, TEXT("SmokeCustomEvent"));
	TestNotNull(TEXT("custom event entry is created"), EntryNode);
	if (!EntryNode)
	{
		return false;
	}

	UK2Node_CallFunction* OldPrintNode = AddGraphWritePrintStringCall(Graph);
	TestNotNull(TEXT("old PrintString body node is created"), OldPrintNode);
	TestTrue(TEXT("old custom event body is linked before replace"),
		OldPrintNode && ConnectFirstExecPins(EntryNode, OldPrintNode));
	const FString BlockId = FString::Printf(TEXT("%s_%s"), *Graph->GetName(), TEXT("SmokeCustomEvent"));
	MarkGraphWriteNodeAsBlueprintHelperOwned(EntryNode, BlockId);
	MarkGraphWriteNodeAsBlueprintHelperOwned(OldPrintNode, BlockId);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperAssetBrowseService AssetBrowseService;
	FBlueprintHelperCompileService CompileService(Resolver);
	FBlueprintHelperAgentImportService AgentImportService(Resolver, CompileService, AssetBrowseService);
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperTransactionJournalService JournalService;
	FBlueprintHelperGraphSnapshotService SnapshotService;
	FBlueprintHelperReplaceBlueprintGraphService ReplaceService(
		Resolver,
		AgentImportService,
		BlockIdService,
		OwnershipService,
		JournalService,
		SnapshotService);

	const FBlueprintHelperToolResultBase Result = ReplaceService.Execute(
		MakeReplaceExecutePayload(Blueprint->GetPathName(), Graph->GetName()));

	TestTrue(TEXT("replace custom event body succeeds"), Result.bOk);
	TestEqual(TEXT("replace status is applied"), Result.Status, EBlueprintHelperToolStatus::Applied);

	UEdGraphPin* EntryExecOut = FindFirstExecPin(EntryNode, EGPD_Output);
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
	TestTrue(TEXT("replacement body node keeps BlueprintHelper ownership"), NodeHasBlueprintHelperBlockId(ReplacementPrintNode, BlockId));
	TestTrue(TEXT("exported graph contains event to replacement PrintString exec link"),
		ExportHasExecLinkFromCustomEventToFunction(Graph, TEXT("SmokeCustomEvent"), TEXT("PrintString")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteOwnershipCommentOmitsToolFieldTest,
	"BlueprintHelper.GraphWrite.Ownership.CommentOmitsToolField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteOwnershipCommentOmitsToolFieldTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGraphWriteTestBlueprint(TEXT("OwnershipCommentOmitsTool"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* EventNode = AddGraphWriteCustomEvent(Graph, TEXT("SmokeCustomEvent"));
	TestNotNull(TEXT("custom event is created"), EventNode);
	if (!EventNode)
	{
		return false;
	}

	FBlueprintHelperOwnershipService OwnershipService;
	FString Error;
	const bool bWritten = OwnershipService.WriteNodeOwnership(
		Blueprint,
		EventNode,
		TEXT("EventGraph_SmokeCustomEvent"),
		TEXT("tx_test_001"),
		TEXT("SmokeFeature"),
		Error);

	TestTrue(TEXT("ownership writes successfully"), bWritten);
	TestTrue(TEXT("comment keeps block_id"), EventNode->NodeComment.Contains(TEXT("block_id=EventGraph_SmokeCustomEvent")));
	TestTrue(TEXT("comment keeps transaction id"), EventNode->NodeComment.Contains(TEXT("tx=tx_test_001")));
	TestFalse(TEXT("comment omits tool field"), EventNode->NodeComment.Contains(TEXT("tool=")));

	UPackage* Package = EventNode->GetOutermost();
	TestNotNull(TEXT("node package exists"), Package);
	if (Package)
	{
		FMetaData& MetaData = Package->GetMetaData();
		TestTrue(TEXT("metadata keeps block id"), MetaData.GetValue(EventNode, TEXT("BlueprintHelperBlockId")) == FString(TEXT("EventGraph_SmokeCustomEvent")));
		TestTrue(TEXT("metadata keeps transaction id"), MetaData.GetValue(EventNode, TEXT("BlueprintHelperTransactionId")) == FString(TEXT("tx_test_001")));
		TestTrue(TEXT("metadata omits tool field"), MetaData.GetValue(EventNode, TEXT("BlueprintHelperTool")).IsEmpty());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteTaskRuntimeReplaceCustomEventBodyReconnectsEntryExecTest,
	"BlueprintHelper.GraphWrite.TaskRuntime.Replace.CustomEventBodyReconnectsEntryExec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteTaskRuntimeReplaceCustomEventBodyReconnectsEntryExecTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGraphWriteTestBlueprint(TEXT("RuntimeReplaceReconnectsEntryExec"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* EntryNode = AddGraphWriteCustomEvent(Graph, TEXT("SmokeCustomEvent"));
	UK2Node_CallFunction* OldPrintNode = AddGraphWritePrintStringCall(Graph);
	TestNotNull(TEXT("custom event entry is created"), EntryNode);
	TestNotNull(TEXT("old PrintString body node is created"), OldPrintNode);
	TestTrue(TEXT("old custom event body is linked before runtime replace"),
		EntryNode && OldPrintNode && ConnectFirstExecPins(EntryNode, OldPrintNode));
	if (!EntryNode || !OldPrintNode)
	{
		return false;
	}

	FGraphWriteRuntimeHarness Harness;
	const FBlueprintHelperToolResultBase Result = Harness.RuntimeService.ExecuteTaskPlan(
		MakeGraphWriteTaskPlanPayload(Blueprint->GetPathName(), Graph->GetName(), MakeReplaceBodyOp()));

	TestTrue(TEXT("runtime replace custom event body succeeds"), Result.bOk);
	TestEqual(TEXT("runtime replace status is applied"), Result.Status, EBlueprintHelperToolStatus::Applied);

	UEdGraphPin* EntryExecOut = FindFirstExecPin(EntryNode, EGPD_Output);
	TestNotNull(TEXT("custom event has output exec pin after runtime replace"), EntryExecOut);
	TestTrue(TEXT("custom event output exec is linked after runtime replace"),
		EntryExecOut && EntryExecOut->LinkedTo.Num() > 0);
	TestTrue(TEXT("exported graph contains runtime event to replacement PrintString exec link"),
		ExportHasExecLinkFromCustomEventToFunction(Graph, TEXT("SmokeCustomEvent"), TEXT("PrintString")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteTaskRuntimeReplacePatchMergeDryRunEnvelopeTest,
	"BlueprintHelper.GraphWrite.TaskRuntime.ReplacePatchMergeDryRunEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteTaskRuntimeReplacePatchMergeDryRunEnvelopeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGraphWriteTestBlueprint(TEXT("TaskRuntimeGraphWriteDryRun"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	const FString AssetPath = Blueprint->GetPathName();
	const FString GraphName = Blueprint->UbergraphPages[0]->GetName();

	FGraphWriteRuntimeHarness Harness;

	const FBlueprintHelperToolResultBase ReplacePreview = Harness.RuntimeService.PreviewTaskPlan(
		MakeGraphWriteTaskPlanPayload(AssetPath, GraphName, MakeReplaceBodyOp()));
	AssertRuntimePreviewReachedGraphWriteService(
		*this,
		ReplacePreview,
		TEXT("replace_blueprint_graph"),
		true);

	const FBlueprintHelperToolResultBase PatchPreview = Harness.RuntimeService.PreviewTaskPlan(
		MakeGraphWriteTaskPlanPayload(AssetPath, GraphName, MakeSetPinDefaultOp()));
	AssertRuntimePreviewReachedGraphWriteService(
		*this,
		PatchPreview,
		TEXT("patch_blueprint_graph"),
		false);

	const FBlueprintHelperToolResultBase MergePreview = Harness.RuntimeService.PreviewTaskPlan(
		MakeGraphWriteTaskPlanPayload(AssetPath, GraphName, MakeInsertFlowOp()));
	AssertRuntimePreviewReachedGraphWriteService(
		*this,
		MergePreview,
		TEXT("merge_blueprint_graph"),
		false);

	return true;
}

#endif
