#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphSnapshotService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintTextConverter.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonPathService.h"
#include "Misc/AutomationTest.h"
#include "Shared/Services/BlueprintHelperAgentImportService.h"
#include "Systems/ToolClusters/AssetFactory/BlueprintHelperAssetFactoryService.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"
#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentService.h"
#include "Shared/Services/BlueprintHelperBlueprintStructureService.h"
#include "Systems/ToolClusters/BlueprintVariables/BlueprintHelperBlueprintVariableService.h"
#include "Systems/ToolClusters/CleanupOwnership/BlueprintHelperCleanupBlueprintHelperBlockService.h"
#include "Systems/ToolClusters/CleanupOwnership/BlueprintHelperConvertBlockToUserOwnedService.h"
#include "Systems/ToolClusters/CleanupOwnership/BlueprintHelperRollbackCleanupTransactionService.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "Systems/ToolClusters/DataTable/BlueprintHelperDataTableService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperAppendBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.h"
#include "Systems/Debug/BlueprintHelperAssetBrowseService.h"
#include "Systems/Debug/BlueprintHelperCompileAssetService.h"
#include "Systems/Debug/BlueprintHelperCompileService.h"
#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"
#include "Systems/Transactions/BlueprintHelperTransactionJournalService.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
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

	TSharedRef<FJsonObject> MakeAppendExecutePayload(const FString& AssetPath, const FString& GraphName)
	{
		TSharedRef<FJsonObject> Payload = MakeAppendPreviewPayload(AssetPath, GraphName);
		Payload->SetBoolField(TEXT("dry_run"), false);
		Payload->SetStringField(TEXT("feature_name"), TEXT("SmokeFeature"));
		return Payload;
	}

	TSharedRef<FJsonObject> MakeAppendReuseExistingEntryExecutePayload(const FString& AssetPath, const FString& GraphName)
	{
		TSharedRef<FJsonObject> Payload = MakeAppendExecutePayload(AssetPath, GraphName);
		Payload->SetBoolField(TEXT("reuse_existing_entries"), true);

		TArray<TSharedPtr<FJsonValue>> Nodes;
		TSharedRef<FJsonObject> EntryNode = MakeShared<FJsonObject>();
		EntryNode->SetStringField(TEXT("id"), TEXT("entry_01"));
		EntryNode->SetStringField(TEXT("kind"), TEXT("custom_event"));
		EntryNode->SetStringField(TEXT("name"), TEXT("SmokeCustomEvent"));
		Nodes.Add(MakeShared<FJsonValueObject>(EntryNode));

		TSharedRef<FJsonObject> CallNode = MakeShared<FJsonObject>();
		CallNode->SetStringField(TEXT("id"), TEXT("print_01"));
		CallNode->SetStringField(TEXT("kind"), TEXT("call"));
		CallNode->SetStringField(TEXT("function"), TEXT("PrintString"));
		Nodes.Add(MakeShared<FJsonValueObject>(CallNode));
		Payload->SetArrayField(TEXT("nodes"), Nodes);

		TArray<TSharedPtr<FJsonValue>> Links;
		TSharedRef<FJsonObject> ExecLink = MakeShared<FJsonObject>();
		ExecLink->SetStringField(TEXT("kind"), TEXT("exec"));
		ExecLink->SetStringField(TEXT("from"), TEXT("entry_01.then"));
		ExecLink->SetStringField(TEXT("to"), TEXT("print_01.execute"));
		Links.Add(MakeShared<FJsonValueObject>(ExecLink));
		Payload->SetArrayField(TEXT("links"), Links);

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

	void AssertNodeHasOwnershipMetadata(
		FAutomationTestBase& Test,
		UEdGraphNode* Node,
		const FString& BlockId,
		const FString& TransactionId,
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

		FMetaData& MetaData = Package->GetMetaData();
		Test.TestTrue(TEXT("metadata marks node as BlueprintHelper owned"),
			MetaData.GetValue(Node, TEXT("BlueprintHelperOwned")) == FString(TEXT("true")));
		Test.TestTrue(TEXT("metadata keeps block id"),
			MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId")) == BlockId);
		Test.TestTrue(TEXT("metadata keeps transaction id"),
			MetaData.GetValue(Node, TEXT("BlueprintHelperTransactionId")) == TransactionId);
		Test.TestTrue(TEXT("metadata keeps feature name"),
			MetaData.GetValue(Node, TEXT("BlueprintHelperFeatureName")) == FeatureName);
		Test.TestTrue(TEXT("metadata omits legacy tool field"),
			MetaData.GetValue(Node, TEXT("BlueprintHelperTool")).IsEmpty());
	}

	UEdGraph* FindUbergraphPageByName(UBlueprint* Blueprint, const FString& GraphName)
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

	int32 CountCustomEventsByName(UEdGraph* Graph, const FString& EventName)
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
		FBlueprintHelperPropertyReflectionService PropertyReflectionService;
		FBlueprintHelperCleanupBlueprintHelperBlockService CleanupBlockService;
		FBlueprintHelperRollbackCleanupTransactionService RollbackCleanupService;
		FBlueprintHelperConvertBlockToUserOwnedService ConvertBlockService;
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
			, CleanupBlockService(Resolver, JournalService)
			, RollbackCleanupService(Resolver, JournalService)
			, ConvertBlockService(Resolver, OwnershipService, JournalService)
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
				PropertyReflectionService,
				CleanupBlockService,
				RollbackCleanupService,
				ConvertBlockService,
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
	FBlueprintHelperGraphWriteOwnershipWritesMetadataWithoutManagedCommentTest,
	"BlueprintHelper.GraphWrite.Ownership.WritesMetadataWithoutManagedComment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteOwnershipWritesMetadataWithoutManagedCommentTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGraphWriteTestBlueprint(TEXT("OwnershipMetadataOnly"));
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

	EventNode->NodeComment = TEXT("Designer note");
	FMetaData& PreWriteMetaData = EventNode->GetOutermost()->GetMetaData();
	PreWriteMetaData.SetValue(EventNode, TEXT("BlueprintHelperTool"), TEXT("legacy_graph_write"));

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
	TestEqual(TEXT("user node comment is preserved"), EventNode->NodeComment, FString(TEXT("Designer note")));
	TestFalse(TEXT("comment omits block_id"), EventNode->NodeComment.Contains(TEXT("block_id=")));
	TestFalse(TEXT("comment omits transaction id"), EventNode->NodeComment.Contains(TEXT("tx=")));
	TestFalse(TEXT("comment omits tool field"), EventNode->NodeComment.Contains(TEXT("tool=")));

	AssertNodeHasOwnershipMetadata(
		*this,
		EventNode,
		TEXT("EventGraph_SmokeCustomEvent"),
		TEXT("tx_test_001"),
		TEXT("SmokeFeature"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteAppendOwnershipWritesMetadataWithoutManagedCommentTest,
	"BlueprintHelper.GraphWrite.Append.OwnershipWritesMetadataWithoutManagedComment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteAppendOwnershipWritesMetadataWithoutManagedCommentTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGraphWriteTestBlueprint(TEXT("AppendOwnershipMetadata"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

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

	const FString GraphName = TEXT("BH_AppendOwnershipMetadata");
	const FBlueprintHelperToolResultBase Result = AppendService.Execute(
		MakeAppendExecutePayload(Blueprint->GetPathName(), GraphName));

	TestTrue(TEXT("append write succeeds"), Result.bOk);
	TestEqual(TEXT("append write status is applied"), Result.Status, EBlueprintHelperToolStatus::Applied);

	FString TransactionId;
	const TSharedPtr<FJsonObject>* WriteRef = nullptr;
	TestTrue(TEXT("append result exposes write_ref"),
		Result.Data.IsValid() && Result.Data->TryGetObjectField(TEXT("write_ref"), WriteRef));
	TestTrue(TEXT("append result exposes transaction id"),
		WriteRef && WriteRef->IsValid() && (*WriteRef)->TryGetStringField(TEXT("transaction_id"), TransactionId));
	const TSharedPtr<FJsonObject>* AppendResult = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* BlockRefs = nullptr;
	FString BlockRef;
	TestTrue(TEXT("append result exposes append_result"),
		Result.Data.IsValid() && Result.Data->TryGetObjectField(TEXT("append_result"), AppendResult));
	TestTrue(TEXT("append result exposes block refs"),
		AppendResult && AppendResult->IsValid() && (*AppendResult)->TryGetArrayField(TEXT("block_refs"), BlockRefs));
	TestTrue(TEXT("append result exposes first block ref"),
		BlockRefs && BlockRefs->Num() > 0 && (*BlockRefs)[0].IsValid() && (*BlockRefs)[0]->TryGetString(BlockRef));

	UEdGraph* Graph = FindUbergraphPageByName(Blueprint, GraphName);
	TestNotNull(TEXT("append graph exists"), Graph);
	TestTrue(TEXT("append graph has created nodes"), Graph && Graph->Nodes.Num() > 0);
	if (!Graph || Graph->Nodes.Num() == 0)
	{
		return false;
	}

	const FString ExpectedBlockId = BlockIdService.MakeFullBlockId(
		GraphName,
		BlockRef);
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		AssertNodeHasOwnershipMetadata(*this, Node, ExpectedBlockId, TransactionId, TEXT("SmokeFeature"));
		TestFalse(TEXT("append-created node comment omits block_id"),
			Node && Node->NodeComment.Contains(TEXT("block_id=")));
		TestFalse(TEXT("append-created node comment omits transaction id"),
			Node && Node->NodeComment.Contains(TEXT("tx=")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteAppendReusesSignatureEntryTest,
	"BlueprintHelper.GraphWrite.Append.ReusesSignatureEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteAppendReusesSignatureEntryTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGraphWriteTestBlueprint(TEXT("AppendReusesSignatureEntry"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* EntryNode = AddGraphWriteCustomEvent(Graph, TEXT("SmokeCustomEvent"));
	TestNotNull(TEXT("signature-created custom event exists"), EntryNode);
	if (!EntryNode)
	{
		return false;
	}

	const int32 EventCountBefore = CountCustomEventsByName(Graph, TEXT("SmokeCustomEvent"));

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
		MakeAppendReuseExistingEntryExecutePayload(Blueprint->GetPathName(), Graph->GetName()));

	TestTrue(TEXT("append reusing signature entry succeeds"), Result.bOk);
	TestEqual(TEXT("append reuse write status is applied"), Result.Status, EBlueprintHelperToolStatus::Applied);
	TestEqual(TEXT("append reuse does not duplicate custom event"),
		CountCustomEventsByName(Graph, TEXT("SmokeCustomEvent")),
		EventCountBefore);
	TestTrue(TEXT("append reuse connects existing custom event to imported body"),
		ExportHasExecLinkFromCustomEventToFunction(Graph, TEXT("SmokeCustomEvent"), TEXT("PrintString")));

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
