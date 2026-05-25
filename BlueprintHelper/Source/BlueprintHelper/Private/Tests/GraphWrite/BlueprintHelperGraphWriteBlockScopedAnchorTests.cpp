#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_IfThenElse.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonPathService.h"
#include "Misc/AutomationTest.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

class FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils
{
public:
	static FString MakeBlockScopedAnchorTestObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static UBlueprint* MakeBlockScopedAnchorTestBlueprint(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperGraphWrite/%s"),
			*MakeBlockScopedAnchorTestObjectName(Prefix)));
		Package->SetDirtyFlag(false);

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*MakeBlockScopedAnchorTestObjectName(TEXT("BP_BlockScopedAnchor")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperGraphWriteBlockScopedAnchorTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static UK2Node_CustomEvent* AddCustomEventNode(UEdGraph* Graph, const FString& EventName)
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

	static UK2Node_IfThenElse* AddBranchNode(UEdGraph* Graph)
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

	static UK2Node_CallFunction* AddNativeFunctionCallNode(UEdGraph* Graph, const FName FunctionName)
	{
		if (!Graph)
		{
			return nullptr;
		}

		UFunction* Function = AActor::StaticClass()->FindFunctionByName(FunctionName);
		if (!Function)
		{
			return nullptr;
		}

		UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
		Graph->AddNode(CallNode, true, false);
		CallNode->CreateNewGuid();
		CallNode->SetFromFunction(Function);
		CallNode->PostPlacedNewNode();
		CallNode->AllocateDefaultPins();
		return CallNode;
	}

	static UEdGraphPin* FindPinByName(UEdGraphNode* Node, const TCHAR* PinName)
	{
		if (!Node)
		{
			return nullptr;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
			{
				return Pin;
			}
		}
		return nullptr;
	}

	static UEdGraphPin* FindExecPin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
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

	static bool ConnectExecPins(UEdGraphPin* FromPin, UEdGraphPin* ToPin)
	{
		if (!FromPin || !ToPin)
		{
			return false;
		}

		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		return Schema && Schema->TryCreateConnection(FromPin, ToPin);
	}

	static void MarkNodeAsBlueprintHelperOwned(UEdGraphNode* Node, const FString& BlockId)
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

	static void MarkNodeWithLegacyBlueprintHelperComment(UEdGraphNode* Node, const FString& BlockId)
	{
		if (!Node)
		{
			return;
		}

		if (UPackage* Package = Node->GetOutermost())
		{
			FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
			MetaData.RemoveValue(Node, TEXT("BlueprintHelperOwned"));
			MetaData.RemoveValue(Node, TEXT("BlueprintHelperBlockId"));
			MetaData.RemoveValue(Node, TEXT("BlueprintHelperFeatureName"));
			MetaData.RemoveValue(Node, TEXT("BlueprintHelperTool"));
		}

		Node->NodeComment = FString::Printf(TEXT("[BlueprintHelper]\nblock_id=%s"), *BlockId);
	}

	struct FBlockScopedGraph
	{
		UBlueprint* Blueprint = nullptr;
		UEdGraph* Graph = nullptr;
		UK2Node_CustomEvent* FirstUnownedEvent = nullptr;
		UK2Node_CallFunction* SecondUnownedCall = nullptr;
		UK2Node_CustomEvent* OwnedEntry = nullptr;
		UK2Node_IfThenElse* OwnedBranch = nullptr;
		FString BlockId;
	};

	static FBlockScopedGraph MakeBlockScopedGraph(const FString& Prefix)
	{
		FBlockScopedGraph Result;
		Result.Blueprint = MakeBlockScopedAnchorTestBlueprint(Prefix);
		if (!Result.Blueprint || Result.Blueprint->UbergraphPages.Num() == 0)
		{
			return Result;
		}

		Result.Graph = Result.Blueprint->UbergraphPages[0];
		Result.BlockId = FString::Printf(TEXT("%s_BlockScoped0"), *Result.Graph->GetName());
		Result.FirstUnownedEvent = AddCustomEventNode(Result.Graph, TEXT("UnownedBeforeBlock"));
		Result.SecondUnownedCall = AddNativeFunctionCallNode(Result.Graph, GET_FUNCTION_NAME_CHECKED(AActor, K2_DestroyActor));
		Result.OwnedEntry = AddCustomEventNode(Result.Graph, TEXT("OwnedBlockEntry"));
		Result.OwnedBranch = AddBranchNode(Result.Graph);

		MarkNodeAsBlueprintHelperOwned(Result.OwnedEntry, Result.BlockId);
		MarkNodeAsBlueprintHelperOwned(Result.OwnedBranch, Result.BlockId);
		return Result;
	}

	static TSharedRef<FJsonObject> MakePatchPayload(
		const FBlockScopedGraph& Fixture,
		const FString& PatchType,
		const FString& PatchScope,
		const FString& NodeRef,
		const FString& PinRef,
		const TSharedRef<FJsonObject>& Patch)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();

		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), Fixture.Blueprint->GetPathName());
		Target->SetStringField(TEXT("graph"), Fixture.Graph->GetName());
		Target->SetStringField(TEXT("patch_scope"), PatchScope);
		Payload->SetObjectField(TEXT("target"), Target);

		TSharedRef<FJsonObject> PatchedRef = MakeShared<FJsonObject>();
		PatchedRef->SetStringField(TEXT("block_id"), Fixture.BlockId);
		PatchedRef->SetStringField(TEXT("group_entry_node_path"), Fixture.OwnedEntry->GetName());
		PatchedRef->SetStringField(TEXT("node_ref"), NodeRef);
		if (!PinRef.IsEmpty())
		{
			PatchedRef->SetStringField(TEXT("pin_ref"), PinRef);
		}
		Payload->SetStringField(TEXT("patch_type"), PatchType);
		Payload->SetObjectField(TEXT("patched_ref"), PatchedRef);
		Payload->SetObjectField(TEXT("patch"), Patch);
		Payload->SetBoolField(TEXT("dry_run"), false);
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeSetPinDefaultPayload(const FBlockScopedGraph& Fixture)
	{
		TSharedRef<FJsonObject> Patch = MakeShared<FJsonObject>();
		Patch->SetStringField(TEXT("value"), TEXT("true"));
		return MakePatchPayload(
			Fixture,
			TEXT("set_pin_default"),
			TEXT("pin_default"),
			TEXT("nodes[1]"),
			TEXT("Condition"),
			Patch);
	}

	static TSharedRef<FJsonObject> MakeSetNodeCommentPayload(const FBlockScopedGraph& Fixture)
	{
		TSharedRef<FJsonObject> Patch = MakeShared<FJsonObject>();
		Patch->SetStringField(TEXT("comment"), TEXT("Updated block entry comment"));
		return MakePatchPayload(
			Fixture,
			TEXT("set_node_comment"),
			TEXT("node_comment"),
			TEXT("nodes[0]"),
			TEXT(""),
			Patch);
	}

	static TSharedRef<FJsonObject> MakeInsertFlowPayload(
		const FBlockScopedGraph& Fixture,
		const FString& FunctionName = TEXT("K2_DestroyActor"),
		bool bDryRun = false)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();

		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), Fixture.Blueprint->GetPathName());
		Target->SetStringField(TEXT("graph"), Fixture.Graph->GetName());
		Target->SetStringField(TEXT("merge_scope"), TEXT("function_call"));
		Target->SetStringField(TEXT("insert_strategy"), TEXT("append_after"));
		Payload->SetObjectField(TEXT("target"), Target);

		TSharedRef<FJsonObject> Anchor = MakeShared<FJsonObject>();
		Anchor->SetStringField(TEXT("block_id"), Fixture.BlockId);
		Anchor->SetStringField(TEXT("group_entry_node_path"), Fixture.OwnedEntry->GetName());
		Anchor->SetStringField(TEXT("node_ref"), TEXT("nodes[0]"));
		Anchor->SetStringField(TEXT("pin_ref"), TEXT("Then"));
		Payload->SetObjectField(TEXT("anchor"), Anchor);

		TSharedRef<FJsonObject> Inserted = MakeShared<FJsonObject>();
		Inserted->SetStringField(TEXT("function"), FunctionName);
		Payload->SetObjectField(TEXT("inserted"), Inserted);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeInsertCustomEventCallPayload(
		const FBlockScopedGraph& Fixture,
		const FString& EventName,
		bool bDryRun = false)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();

		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), Fixture.Blueprint->GetPathName());
		Target->SetStringField(TEXT("graph"), Fixture.Graph->GetName());
		Target->SetStringField(TEXT("merge_scope"), TEXT("custom_event_call"));
		Target->SetStringField(TEXT("insert_strategy"), TEXT("append_after"));
		Payload->SetObjectField(TEXT("target"), Target);

		TSharedRef<FJsonObject> Anchor = MakeShared<FJsonObject>();
		Anchor->SetStringField(TEXT("block_id"), Fixture.BlockId);
		Anchor->SetStringField(TEXT("group_entry_node_path"), Fixture.OwnedEntry->GetName());
		Anchor->SetStringField(TEXT("node_ref"), TEXT("nodes[0]"));
		Anchor->SetStringField(TEXT("pin_ref"), TEXT("Then"));
		Payload->SetObjectField(TEXT("anchor"), Anchor);

		TSharedRef<FJsonObject> Inserted = MakeShared<FJsonObject>();
		Inserted->SetStringField(TEXT("custom_event"), EventName);
		Payload->SetObjectField(TEXT("inserted"), Inserted);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeInsertBetweenFlowPayload(const FBlockScopedGraph& Fixture)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();

		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), Fixture.Blueprint->GetPathName());
		Target->SetStringField(TEXT("graph"), Fixture.Graph->GetName());
		Target->SetStringField(TEXT("merge_scope"), TEXT("function_call"));
		Target->SetStringField(TEXT("insert_strategy"), TEXT("insert_between"));
		Payload->SetObjectField(TEXT("target"), Target);

		TSharedRef<FJsonObject> Anchor = MakeShared<FJsonObject>();
		Anchor->SetStringField(TEXT("block_id"), Fixture.BlockId);
		Anchor->SetStringField(TEXT("group_entry_node_path"), Fixture.OwnedEntry->GetName());
		Anchor->SetStringField(TEXT("node_ref"), TEXT("nodes[0]"));
		Anchor->SetStringField(TEXT("pin_ref"), TEXT("Then"));
		Payload->SetObjectField(TEXT("anchor"), Anchor);

		TSharedRef<FJsonObject> Inserted = MakeShared<FJsonObject>();
		Inserted->SetStringField(TEXT("function"), TEXT("K2_SetActorLocation"));
		Payload->SetObjectField(TEXT("inserted"), Inserted);
		Payload->SetBoolField(TEXT("dry_run"), false);
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeBranchForkOwnedBlockCallPayload(
		const FBlockScopedGraph& Fixture,
		const FString& InsertedBlockId,
		bool bDryRun = false,
		bool bAllowCompileBeforeCall = false)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();

		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), Fixture.Blueprint->GetPathName());
		Target->SetStringField(TEXT("graph"), Fixture.Graph->GetName());
		Target->SetStringField(TEXT("merge_scope"), TEXT("owned_block_call"));
		Target->SetStringField(TEXT("insert_strategy"), TEXT("branch_fork"));
		Payload->SetObjectField(TEXT("target"), Target);

		TSharedRef<FJsonObject> Anchor = MakeShared<FJsonObject>();
		Anchor->SetStringField(TEXT("block_id"), Fixture.BlockId);
		Anchor->SetStringField(TEXT("group_entry_node_path"), Fixture.OwnedEntry->GetName());
		Anchor->SetStringField(TEXT("node_ref"), TEXT("nodes[0]"));
		Anchor->SetStringField(TEXT("pin_ref"), TEXT("Then"));
		Payload->SetObjectField(TEXT("anchor"), Anchor);

		TSharedRef<FJsonObject> Inserted = MakeShared<FJsonObject>();
		Inserted->SetStringField(TEXT("block_id"), InsertedBlockId);
		Payload->SetObjectField(TEXT("inserted"), Inserted);

		TArray<TSharedPtr<FJsonValue>> SequenceOrder;
		SequenceOrder.Add(MakeShared<FJsonValueString>(TEXT("inserted_logic")));
		SequenceOrder.Add(MakeShared<FJsonValueString>(TEXT("original_successor")));
		Payload->SetArrayField(TEXT("sequence_order"), SequenceOrder);
		Payload->SetBoolField(TEXT("allow_compile_before_call"), bAllowCompileBeforeCall);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);
		return Payload;
	}

	static UK2Node_CallFunction* FindCallFunctionNode(UEdGraph* Graph, const FName FunctionName)
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

	static UK2Node_ExecutionSequence* FindSequenceNode(UEdGraph* Graph)
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

	static TArray<UEdGraphPin*> FindExecPins(UEdGraphNode* Node, EEdGraphPinDirection Direction)
	{
		TArray<UEdGraphPin*> Pins;
		if (!Node)
		{
			return Pins;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				Pins.Add(Pin);
			}
		}
		return Pins;
	}

	static bool GetMergeDryRunStatus(
		const FBlueprintHelperToolResultBase& Result,
		FString& OutDryRunResult,
		bool& bOutCanExecute)
	{
		OutDryRunResult.Reset();
		bOutCanExecute = true;

		if (!Result.Data.IsValid())
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* DryRunObject = nullptr;
		if (!Result.Data->TryGetObjectField(TEXT("dry_run"), DryRunObject) ||
			!DryRunObject ||
			!DryRunObject->IsValid())
		{
			return false;
		}

		(*DryRunObject)->TryGetStringField(TEXT("result"), OutDryRunResult);
		(*DryRunObject)->TryGetBoolField(TEXT("can_execute"), bOutCanExecute);
		return true;
	}

	static bool GetFirstMergeDryRunError(
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

		const TSharedPtr<FJsonObject>* DryRunObject = nullptr;
		if (!Result.Data->TryGetObjectField(TEXT("dry_run"), DryRunObject) ||
			!DryRunObject ||
			!DryRunObject->IsValid())
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Errors = nullptr;
		if (!(*DryRunObject)->TryGetArrayField(TEXT("errors"), Errors) ||
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

};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchSetPinDefaultBlockScopedAnchorTest,
	"BlueprintHelper.GraphWrite.BlockScopedAnchors.PatchSetPinDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchSetPinDefaultBlockScopedAnchorTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FBlockScopedGraph Fixture = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeBlockScopedGraph(TEXT("PatchSetPinDefaultBlockScoped"));
	TestNotNull(TEXT("test blueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("test graph is available"), Fixture.Graph);
	TestNotNull(TEXT("owned branch node is created"), Fixture.OwnedBranch);
	if (!Fixture.Blueprint || !Fixture.Graph || !Fixture.OwnedBranch)
	{
		return false;
	}

	UEdGraphPin* ConditionPin = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindPinByName(Fixture.OwnedBranch, TEXT("Condition"));
	TestNotNull(TEXT("owned branch has Condition pin"), ConditionPin);
	if (!ConditionPin)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeSetPinDefaultPayload(Fixture));

	TestTrue(TEXT("set_pin_default resolves block-local nodes[1]"), Result.bOk);
	TestEqual(TEXT("owned branch condition default is changed"), ConditionPin->DefaultValue, FString(TEXT("true")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchSetNodeCommentBlockScopedAnchorTest,
	"BlueprintHelper.GraphWrite.BlockScopedAnchors.PatchSetNodeComment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchSetNodeCommentBlockScopedAnchorTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FBlockScopedGraph Fixture = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeBlockScopedGraph(TEXT("PatchSetNodeCommentBlockScoped"));
	TestNotNull(TEXT("test blueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("owned entry node is created"), Fixture.OwnedEntry);
	TestNotNull(TEXT("unowned leading node is created"), Fixture.FirstUnownedEvent);
	if (!Fixture.Blueprint || !Fixture.OwnedEntry || !Fixture.FirstUnownedEvent)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	TestFalse(TEXT("metadata-owned entry does not need legacy block_id comment"),
		Fixture.OwnedEntry->NodeComment.Contains(TEXT("block_id=")));
	const FBlueprintHelperToolResultBase Result = PatchService.Execute(FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeSetNodeCommentPayload(Fixture));

	TestTrue(TEXT("set_node_comment resolves block-local nodes[0]"), Result.bOk);
	TestEqual(TEXT("owned entry comment is changed"), Fixture.OwnedEntry->NodeComment, FString(TEXT("Updated block entry comment")));
	TestFalse(TEXT("whole-graph nodes[0] was not patched"), Fixture.FirstUnownedEvent->NodeComment == FString(TEXT("Updated block entry comment")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchResolvesLegacyManagedCommentFallbackTest,
	"BlueprintHelper.GraphWrite.BlockScopedAnchors.LegacyManagedCommentFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchResolvesLegacyManagedCommentFallbackTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FBlockScopedGraph Fixture = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeBlockScopedGraph(TEXT("PatchLegacyManagedCommentFallback"));
	TestNotNull(TEXT("fixture graph exists"), Fixture.Graph);
	TestNotNull(TEXT("owned entry exists"), Fixture.OwnedEntry);
	TestNotNull(TEXT("owned branch exists"), Fixture.OwnedBranch);
	if (!Fixture.Graph || !Fixture.OwnedEntry || !Fixture.OwnedBranch)
	{
		return false;
	}

	FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MarkNodeWithLegacyBlueprintHelperComment(Fixture.OwnedEntry, Fixture.BlockId);
	FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MarkNodeWithLegacyBlueprintHelperComment(Fixture.OwnedBranch, Fixture.BlockId);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeSetNodeCommentPayload(Fixture));

	TestTrue(TEXT("legacy comment fallback patch succeeds"), Result.bOk);
	TestEqual(TEXT("legacy comment fallback resolves owned entry"), Fixture.OwnedEntry->NodeComment, FString(TEXT("Updated block entry comment")));
	TestFalse(TEXT("whole-graph nodes[0] was not patched"), Fixture.FirstUnownedEvent->NodeComment == FString(TEXT("Updated block entry comment")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchIgnoresLegacyCommentWhenMetadataPresentTest,
	"BlueprintHelper.GraphWrite.BlockScopedAnchors.MetadataWinsOverLegacyComment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchIgnoresLegacyCommentWhenMetadataPresentTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FBlockScopedGraph Fixture = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeBlockScopedGraph(TEXT("PatchMetadataWinsOverLegacyComment"));
	TestNotNull(TEXT("fixture graph exists"), Fixture.Graph);
	TestNotNull(TEXT("owned entry exists"), Fixture.OwnedEntry);
	TestNotNull(TEXT("owned branch exists"), Fixture.OwnedBranch);
	if (!Fixture.Graph || !Fixture.OwnedEntry || !Fixture.OwnedBranch)
	{
		return false;
	}

	const FString LegacyBlockId = FString::Printf(TEXT("%s_Legacy"), *Fixture.BlockId);
	const FString StaleLegacyComment = FString::Printf(TEXT("[BlueprintHelper]\nblock_id=%s"), *LegacyBlockId);
	Fixture.OwnedEntry->NodeComment = StaleLegacyComment;
	Fixture.OwnedBranch->NodeComment = StaleLegacyComment;
	Fixture.BlockId = LegacyBlockId;

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeSetNodeCommentPayload(Fixture));

	TestFalse(TEXT("legacy comment is ignored while ownership metadata is present"), Result.bOk);
	TestEqual(TEXT("metadata-owned node keeps stale comment when legacy block is rejected"),
		Fixture.OwnedEntry->NodeComment,
		StaleLegacyComment);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteMergeInsertFlowBlockScopedAnchorTest,
	"BlueprintHelper.GraphWrite.BlockScopedAnchors.MergeInsertFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteMergeInsertFlowBlockScopedAnchorTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FBlockScopedGraph Fixture = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeBlockScopedGraph(TEXT("MergeInsertFlowBlockScoped"));
	TestNotNull(TEXT("test blueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("owned entry node is created"), Fixture.OwnedEntry);
	TestNotNull(TEXT("unowned leading node is created"), Fixture.FirstUnownedEvent);
	if (!Fixture.Blueprint || !Fixture.OwnedEntry || !Fixture.FirstUnownedEvent)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperMergeBlueprintGraphService MergeService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = MergeService.Execute(FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeInsertFlowPayload(Fixture));

	UEdGraphPin* OwnedThenPin = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindPinByName(Fixture.OwnedEntry, TEXT("Then"));
	UEdGraphPin* UnownedThenPin = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindPinByName(Fixture.FirstUnownedEvent, TEXT("Then"));
	TestTrue(TEXT("insert_flow resolves block-local anchor nodes[0]"), Result.bOk);
	TestTrue(TEXT("owned anchor Then pin receives inserted flow"), OwnedThenPin && OwnedThenPin->LinkedTo.Num() == 1);
	TestTrue(TEXT("whole-graph nodes[0] was not used as anchor"), !UnownedThenPin || UnownedThenPin->LinkedTo.Num() == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteMergeInsertFlowDisplayNameFunctionCallTest,
	"BlueprintHelper.GraphWrite.BlockScopedAnchors.MergeInsertFlowDisplayNameFunctionCall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteMergeInsertFlowDisplayNameFunctionCallTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FBlockScopedGraph Fixture = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeBlockScopedGraph(TEXT("MergeInsertFlowDisplayNameFunctionCall"));
	TestNotNull(TEXT("test blueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("owned entry node is created"), Fixture.OwnedEntry);
	TestNotNull(TEXT("unowned leading node is created"), Fixture.FirstUnownedEvent);
	if (!Fixture.Blueprint || !Fixture.OwnedEntry || !Fixture.FirstUnownedEvent)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperMergeBlueprintGraphService MergeService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = MergeService.Execute(
		FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeInsertFlowPayload(Fixture, TEXT("Print String")));

	UEdGraphPin* OwnedThenPin = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindPinByName(Fixture.OwnedEntry, TEXT("Then"));
	UEdGraphPin* UnownedThenPin = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindPinByName(Fixture.FirstUnownedEvent, TEXT("Then"));
	UK2Node_CallFunction* InsertedCall = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindCallFunctionNode(Fixture.Graph, FName(TEXT("PrintString")));
	UEdGraphPin* InsertedExecIn = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindExecPin(InsertedCall, EGPD_Input);

	TestTrue(TEXT("display-name call_function resolves through merge"), Result.bOk);
	if (!Result.bOk && Result.Error.IsSet())
	{
		TestFalse(TEXT("display-name resolve failure message is diagnosable"), Result.Error->Message.IsEmpty());
	}
	TestNotNull(TEXT("PrintString call node is created"), InsertedCall);
	TestTrue(TEXT("owned anchor Then pin links to resolved PrintString"), OwnedThenPin && InsertedExecIn && OwnedThenPin->LinkedTo.Contains(InsertedExecIn));
	TestTrue(TEXT("whole-graph nodes[0] was not used as anchor"), !UnownedThenPin || UnownedThenPin->LinkedTo.Num() == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteMergeInsertFlowQualifiedFunctionCallTest,
	"BlueprintHelper.GraphWrite.BlockScopedAnchors.MergeInsertFlowQualifiedFunctionCall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteMergeInsertFlowQualifiedFunctionCallTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FBlockScopedGraph Fixture = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeBlockScopedGraph(TEXT("MergeInsertFlowQualifiedFunctionCall"));
	TestNotNull(TEXT("test blueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("owned entry node is created"), Fixture.OwnedEntry);
	TestNotNull(TEXT("unowned leading node is created"), Fixture.FirstUnownedEvent);
	if (!Fixture.Blueprint || !Fixture.OwnedEntry || !Fixture.FirstUnownedEvent)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperMergeBlueprintGraphService MergeService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = MergeService.Execute(
		FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeInsertFlowPayload(
			Fixture,
			TEXT("/Script/Engine.KismetSystemLibrary:PrintString")));

	UEdGraphPin* OwnedThenPin = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindPinByName(Fixture.OwnedEntry, TEXT("Then"));
	UEdGraphPin* UnownedThenPin = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindPinByName(Fixture.FirstUnownedEvent, TEXT("Then"));
	UK2Node_CallFunction* InsertedCall = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindCallFunctionNode(Fixture.Graph, FName(TEXT("PrintString")));
	UEdGraphPin* InsertedExecIn = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindExecPin(InsertedCall, EGPD_Input);

	TestTrue(TEXT("qualified call_function resolves through merge"), Result.bOk);
	if (!Result.bOk && Result.Error.IsSet())
	{
		TestFalse(TEXT("qualified resolve failure message is diagnosable"), Result.Error->Message.IsEmpty());
	}
	TestNotNull(TEXT("PrintString call node is created"), InsertedCall);
	TestTrue(TEXT("owned anchor Then pin links to qualified PrintString"), OwnedThenPin && InsertedExecIn && OwnedThenPin->LinkedTo.Contains(InsertedExecIn));
	TestTrue(TEXT("whole-graph nodes[0] was not used as anchor"), !UnownedThenPin || UnownedThenPin->LinkedTo.Num() == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteMergeMemberPrefixBlocksTest,
	"BlueprintHelper.GraphWrite.BlockScopedAnchors.MergeMemberPrefixBlocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteMergeMemberPrefixBlocksTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FBlockScopedGraph Fixture = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeBlockScopedGraph(TEXT("MergeMemberPrefixBlocks"));
	TestNotNull(TEXT("test blueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("owned entry node is created"), Fixture.OwnedEntry);
	if (!Fixture.Blueprint || !Fixture.OwnedEntry)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperMergeBlueprintGraphService MergeService(Resolver, PathService);
	const int32 NodeCountBeforePreview = Fixture.Graph->Nodes.Num();

	const FBlueprintHelperToolResultBase Result = MergeService.Execute(
		FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeInsertFlowPayload(
			Fixture,
			TEXT("DoorMesh.AddAngularImpulseInDegrees"),
			true));

	FString DryRunResult;
	bool bCanExecute = true;
	TestTrue(TEXT("dry-run status is present"), FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::GetMergeDryRunStatus(Result, DryRunResult, bCanExecute));
	TestFalse(TEXT("member-prefix function call preview is blocked"), Result.bOk);
	TestEqual(TEXT("dry-run result is blocked"), DryRunResult, FString(TEXT("blocked")));
	TestFalse(TEXT("dry-run cannot execute"), bCanExecute);
	TestTrue(TEXT("member-prefix block returns top-level error"),
		Result.Error.IsSet() &&
		Result.Error->Code == TEXT("inserted_logic_not_found") &&
		Result.Error->Message.Contains(TEXT("explicit member prefix")));
	TestEqual(TEXT("dry-run does not leave preview callable nodes behind"), Fixture.Graph->Nodes.Num(), NodeCountBeforePreview);

	FString FirstErrorCode;
	FString FirstErrorMessage;
	TestTrue(TEXT("dry-run error carries resolver diagnostic"),
		FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::GetFirstMergeDryRunError(Result, FirstErrorCode, FirstErrorMessage));
	TestEqual(TEXT("dry-run error code is stable"), FirstErrorCode, FString(TEXT("inserted_logic_not_found")));
	TestTrue(TEXT("dry-run error message names resolver block"),
		FirstErrorMessage.Contains(TEXT("explicit member prefix")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteMergeInsertFlowCustomEventCallDryRunTest,
	"BlueprintHelper.GraphWrite.BlockScopedAnchors.MergeInsertFlowCustomEventCallDryRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteMergeInsertFlowCustomEventCallDryRunTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FBlockScopedGraph Fixture =
		FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeBlockScopedGraph(TEXT("MergeInsertFlowCustomEventCallDryRun"));
	TestNotNull(TEXT("test blueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("owned entry node is created"), Fixture.OwnedEntry);
	if (!Fixture.Blueprint || !Fixture.OwnedEntry)
	{
		return false;
	}

	const FString EventName = TEXT("MergeCallableDryRunCustomEvent");
	UK2Node_CustomEvent* InsertedEvent =
		FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::AddCustomEventNode(Fixture.Graph, EventName);
	TestNotNull(TEXT("custom event target is created"), InsertedEvent);
	if (!InsertedEvent)
	{
		return false;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Fixture.Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Fixture.Blueprint);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperMergeBlueprintGraphService MergeService(Resolver, PathService);
	const int32 NodeCountBeforePreview = Fixture.Graph->Nodes.Num();

	const FBlueprintHelperToolResultBase Result = MergeService.Execute(
		FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeInsertCustomEventCallPayload(
			Fixture,
			EventName,
			true));

	FString DryRunResult;
	bool bCanExecute = false;
	TestTrue(TEXT("custom event dry-run status is present"),
		FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::GetMergeDryRunStatus(Result, DryRunResult, bCanExecute));
	TestTrue(TEXT("custom_event_call dry-run can execute"), Result.bOk);
	TestEqual(TEXT("custom_event_call dry-run passed"), DryRunResult, FString(TEXT("passed")));
	TestTrue(TEXT("custom_event_call dry-run can execute flag is true"), bCanExecute);
	TestEqual(TEXT("custom_event_call dry-run does not leave preview callable nodes behind"), Fixture.Graph->Nodes.Num(), NodeCountBeforePreview);
	TestNull(TEXT("custom event dry-run does not create call node"),
		FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindCallFunctionNode(Fixture.Graph, FName(*EventName)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteMergeInsertFlowCustomEventCallTest,
	"BlueprintHelper.GraphWrite.BlockScopedAnchors.MergeInsertFlowCustomEventCall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteMergeInsertFlowCustomEventCallTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FBlockScopedGraph Fixture =
		FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeBlockScopedGraph(TEXT("MergeInsertFlowCustomEventCall"));
	TestNotNull(TEXT("test blueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("owned entry node is created"), Fixture.OwnedEntry);
	if (!Fixture.Blueprint || !Fixture.OwnedEntry)
	{
		return false;
	}

	const FString EventName = TEXT("MergeCallableInsertedCustomEvent");
	UK2Node_CustomEvent* InsertedEvent =
		FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::AddCustomEventNode(Fixture.Graph, EventName);
	TestNotNull(TEXT("custom event target is created"), InsertedEvent);
	if (!InsertedEvent)
	{
		return false;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Fixture.Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Fixture.Blueprint);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperMergeBlueprintGraphService MergeService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = MergeService.Execute(
		FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeInsertCustomEventCallPayload(
			Fixture,
			EventName));

	UEdGraphPin* OwnedThenPin = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindPinByName(Fixture.OwnedEntry, TEXT("Then"));
	UK2Node_CallFunction* InsertedCall =
		FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindCallFunctionNode(Fixture.Graph, FName(*EventName));
	UEdGraphPin* InsertedExecIn =
		FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindExecPin(InsertedCall, EGPD_Input);

	TestTrue(TEXT("custom_event_call resolves through merge callable path"), Result.bOk);
	if (!Result.bOk && Result.Error.IsSet())
	{
		TestFalse(TEXT("custom event resolve failure is diagnosable"), Result.Error->Message.IsEmpty());
	}
	TestNotNull(TEXT("custom event call node is created"), InsertedCall);
	TestTrue(TEXT("owned anchor links to custom event call"), OwnedThenPin && InsertedExecIn && OwnedThenPin->LinkedTo.Contains(InsertedExecIn));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteMergeInsertBetweenBlockScopedAnchorTest,
	"BlueprintHelper.GraphWrite.BlockScopedAnchors.MergeInsertBetween",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteMergeInsertBetweenBlockScopedAnchorTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FBlockScopedGraph Fixture = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeBlockScopedGraph(TEXT("MergeInsertBetweenBlockScoped"));
	TestNotNull(TEXT("test blueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("owned entry node is created"), Fixture.OwnedEntry);
	TestNotNull(TEXT("owned branch node is created"), Fixture.OwnedBranch);
	if (!Fixture.Blueprint || !Fixture.OwnedEntry || !Fixture.OwnedBranch)
	{
		return false;
	}

	UEdGraphPin* OwnedThenPin = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindPinByName(Fixture.OwnedEntry, TEXT("Then"));
	UEdGraphPin* BranchExecIn = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindExecPin(Fixture.OwnedBranch, EGPD_Input);
	TestTrue(TEXT("fixture starts with owned entry linked to owned branch"), FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::ConnectExecPins(OwnedThenPin, BranchExecIn));
	TestTrue(TEXT("owned entry has one original successor before merge"), OwnedThenPin && OwnedThenPin->LinkedTo.Num() == 1);
	if (!OwnedThenPin || !BranchExecIn || OwnedThenPin->LinkedTo.Num() != 1)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperMergeBlueprintGraphService MergeService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = MergeService.Execute(FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeInsertBetweenFlowPayload(Fixture));

	UK2Node_CallFunction* InsertedCall = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindCallFunctionNode(Fixture.Graph, GET_FUNCTION_NAME_CHECKED(AActor, K2_SetActorLocation));
	UEdGraphPin* InsertedExecIn = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindExecPin(InsertedCall, EGPD_Input);
	UEdGraphPin* InsertedExecOut = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindExecPin(InsertedCall, EGPD_Output);

	TestTrue(TEXT("insert_between executes through block-local anchor"), Result.bOk);
	TestNotNull(TEXT("inserted function call node is created"), InsertedCall);
	TestTrue(TEXT("owned anchor now links to inserted function"), OwnedThenPin && InsertedExecIn && OwnedThenPin->LinkedTo.Contains(InsertedExecIn));
	TestTrue(TEXT("inserted function links to original successor"), InsertedExecOut && BranchExecIn && InsertedExecOut->LinkedTo.Contains(BranchExecIn));
	TestFalse(TEXT("old direct link is replaced"), OwnedThenPin && BranchExecIn && OwnedThenPin->LinkedTo.Contains(BranchExecIn));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteMergeBranchForkOwnedBlockCallTest,
	"BlueprintHelper.GraphWrite.BlockScopedAnchors.MergeBranchForkOwnedBlockCall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteMergeBranchForkOwnedBlockCallTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FBlockScopedGraph Fixture = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeBlockScopedGraph(TEXT("MergeBranchForkOwnedBlockCall"));
	TestNotNull(TEXT("test blueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("owned entry node is created"), Fixture.OwnedEntry);
	TestNotNull(TEXT("owned branch node is created"), Fixture.OwnedBranch);
	if (!Fixture.Blueprint || !Fixture.OwnedEntry || !Fixture.OwnedBranch)
	{
		return false;
	}

	UEdGraphPin* OwnedThenPin = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindPinByName(Fixture.OwnedEntry, TEXT("Then"));
	UEdGraphPin* BranchExecIn = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindExecPin(Fixture.OwnedBranch, EGPD_Input);
	TestTrue(TEXT("fixture starts with owned entry linked to owned branch"), FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::ConnectExecPins(OwnedThenPin, BranchExecIn));
	TestTrue(TEXT("owned entry has one original successor before branch fork"), OwnedThenPin && OwnedThenPin->LinkedTo.Num() == 1);
	if (!OwnedThenPin || !BranchExecIn || OwnedThenPin->LinkedTo.Num() != 1)
	{
		return false;
	}

	const FString InsertedBlockId = FString::Printf(TEXT("%s_InsertedBlockScoped0"), *Fixture.Graph->GetName());
	UK2Node_CustomEvent* InsertedOwnedEntry = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::AddCustomEventNode(Fixture.Graph, TEXT("OwnedInsertedBlock"));
	TestNotNull(TEXT("inserted owned block entry is created"), InsertedOwnedEntry);
	if (!InsertedOwnedEntry)
	{
		return false;
	}
	FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MarkNodeAsBlueprintHelperOwned(InsertedOwnedEntry, InsertedBlockId);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Fixture.Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Fixture.Blueprint);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperMergeBlueprintGraphService MergeService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = MergeService.Execute(FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeBranchForkOwnedBlockCallPayload(Fixture, InsertedBlockId));

	UK2Node_ExecutionSequence* SequenceNode = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindSequenceNode(Fixture.Graph);
	UK2Node_CallFunction* InsertedCall = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindCallFunctionNode(Fixture.Graph, FName(TEXT("OwnedInsertedBlock")));
	UEdGraphPin* SequenceExecIn = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindExecPin(SequenceNode, EGPD_Input);
	const TArray<UEdGraphPin*> SequenceThenPins = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindExecPins(SequenceNode, EGPD_Output);
	UEdGraphPin* InsertedExecIn = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindExecPin(InsertedCall, EGPD_Input);

	TestTrue(TEXT("branch_fork executes through owned block call"), Result.bOk);
	if (!Result.bOk && Result.Error.IsSet())
	{
		TestFalse(TEXT("branch_fork failure message is diagnosable"), Result.Error->Message.IsEmpty());
	}
	TestNotNull(TEXT("branch_fork creates a sequence node"), SequenceNode);
	TestNotNull(TEXT("owned block call node is created"), InsertedCall);
	TestTrue(TEXT("owned anchor now links to sequence input"), OwnedThenPin && SequenceExecIn && OwnedThenPin->LinkedTo.Contains(SequenceExecIn));
	TestTrue(TEXT("sequence has at least two Then outputs"), SequenceThenPins.Num() >= 2);
	if (SequenceThenPins.Num() >= 2)
	{
		TestTrue(TEXT("first branch calls inserted owned block"), InsertedExecIn && SequenceThenPins[0]->LinkedTo.Contains(InsertedExecIn));
		TestTrue(TEXT("second branch preserves original successor"), BranchExecIn && SequenceThenPins[1]->LinkedTo.Contains(BranchExecIn));
	}
	TestFalse(TEXT("old direct link is replaced by sequence"), OwnedThenPin && BranchExecIn && OwnedThenPin->LinkedTo.Contains(BranchExecIn));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteMergeBranchForkMissingOwnedBlockPreviewBlocksTest,
	"BlueprintHelper.GraphWrite.BlockScopedAnchors.MergeBranchForkMissingOwnedBlockPreviewBlocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteMergeBranchForkMissingOwnedBlockPreviewBlocksTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FBlockScopedGraph Fixture = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeBlockScopedGraph(TEXT("MergeBranchForkMissingOwnedBlockPreviewBlocks"));
	TestNotNull(TEXT("test blueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("owned entry node is created"), Fixture.OwnedEntry);
	TestNotNull(TEXT("owned branch node is created"), Fixture.OwnedBranch);
	if (!Fixture.Blueprint || !Fixture.OwnedEntry || !Fixture.OwnedBranch)
	{
		return false;
	}

	UEdGraphPin* OwnedThenPin = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindPinByName(Fixture.OwnedEntry, TEXT("Then"));
	UEdGraphPin* BranchExecIn = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindExecPin(Fixture.OwnedBranch, EGPD_Input);
	TestTrue(TEXT("fixture starts with one original successor"), FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::ConnectExecPins(OwnedThenPin, BranchExecIn));
	if (!OwnedThenPin || !BranchExecIn || OwnedThenPin->LinkedTo.Num() != 1)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperMergeBlueprintGraphService MergeService(Resolver, PathService);

	const FString MissingBlockId = FString::Printf(TEXT("%s_MissingInsertedBlock0"), *Fixture.Graph->GetName());
	const int32 NodeCountBeforePreview = Fixture.Graph->Nodes.Num();
	const FBlueprintHelperToolResultBase Result = MergeService.Execute(
		FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeBranchForkOwnedBlockCallPayload(Fixture, MissingBlockId, true));

	FString DryRunResult;
	bool bCanExecute = true;
	TestTrue(TEXT("dry-run status is present"), FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::GetMergeDryRunStatus(Result, DryRunResult, bCanExecute));
	TestFalse(TEXT("missing inserted block preview is blocked"), Result.bOk);
	TestEqual(TEXT("dry-run result is blocked"), DryRunResult, FString(TEXT("blocked")));
	TestFalse(TEXT("dry-run cannot execute"), bCanExecute);
	TestTrue(TEXT("missing inserted block error is surfaced"),
		Result.Error.IsSet() &&
		Result.Error->Code == TEXT("inserted_logic_not_found") &&
		!Result.Error->Message.IsEmpty());
	TestEqual(TEXT("missing owned block preview does not leave nodes behind"), Fixture.Graph->Nodes.Num(), NodeCountBeforePreview);
	TestNull(TEXT("missing owned block preview does not create sequence node"),
		FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindSequenceNode(Fixture.Graph));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteMergeBranchForkUncompiledOwnedBlockCallTest,
	"BlueprintHelper.GraphWrite.BlockScopedAnchors.MergeBranchForkUncompiledOwnedBlockCall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteMergeBranchForkUncompiledOwnedBlockCallTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FBlockScopedGraph Fixture = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeBlockScopedGraph(TEXT("MergeBranchForkUncompiledOwnedBlockCall"));
	TestNotNull(TEXT("test blueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("owned entry node is created"), Fixture.OwnedEntry);
	TestNotNull(TEXT("owned branch node is created"), Fixture.OwnedBranch);
	if (!Fixture.Blueprint || !Fixture.OwnedEntry || !Fixture.OwnedBranch)
	{
		return false;
	}

	UEdGraphPin* OwnedThenPin = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindPinByName(Fixture.OwnedEntry, TEXT("Then"));
	UEdGraphPin* BranchExecIn = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindExecPin(Fixture.OwnedBranch, EGPD_Input);
	TestTrue(TEXT("fixture starts with owned entry linked to owned branch"), FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::ConnectExecPins(OwnedThenPin, BranchExecIn));
	TestTrue(TEXT("owned entry has one original successor before branch fork"), OwnedThenPin && OwnedThenPin->LinkedTo.Num() == 1);
	if (!OwnedThenPin || !BranchExecIn || OwnedThenPin->LinkedTo.Num() != 1)
	{
		return false;
	}

	const FString InsertedBlockId = FString::Printf(TEXT("%s_UncompiledInsertedBlock0"), *Fixture.Graph->GetName());
	const FString InsertedEventName = TEXT("OwnedUncompiledInsertedBlock");
	UK2Node_CustomEvent* InsertedOwnedEntry = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::AddCustomEventNode(Fixture.Graph, InsertedEventName);
	TestNotNull(TEXT("inserted owned block entry is created"), InsertedOwnedEntry);
	if (!InsertedOwnedEntry)
	{
		return false;
	}
	FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MarkNodeAsBlueprintHelperOwned(InsertedOwnedEntry, InsertedBlockId);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Fixture.Blueprint);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperMergeBlueprintGraphService MergeService(Resolver, PathService);

	const FBlueprintHelperToolResultBase Result = MergeService.Execute(
		FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::MakeBranchForkOwnedBlockCallPayload(Fixture, InsertedBlockId, false, true));

	UK2Node_ExecutionSequence* SequenceNode = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindSequenceNode(Fixture.Graph);
	UK2Node_CallFunction* InsertedCall = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindCallFunctionNode(Fixture.Graph, FName(*InsertedEventName));
	UEdGraphPin* SequenceExecIn = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindExecPin(SequenceNode, EGPD_Input);
	const TArray<UEdGraphPin*> SequenceThenPins = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindExecPins(SequenceNode, EGPD_Output);
	UEdGraphPin* InsertedExecIn = FBlueprintHelperGraphWriteBlockScopedAnchorTestsLocalUtils::FindExecPin(InsertedCall, EGPD_Input);

	TestTrue(TEXT("uncompiled owned block call compiles and executes"), Result.bOk);
	if (!Result.bOk && Result.Error.IsSet())
	{
		TestFalse(TEXT("branch_fork failure message is diagnosable"), Result.Error->Message.IsEmpty());
	}
	TestNotNull(TEXT("branch_fork creates a sequence node"), SequenceNode);
	TestNotNull(TEXT("owned block call node is created"), InsertedCall);
	TestTrue(TEXT("owned anchor now links to sequence input"), OwnedThenPin && SequenceExecIn && OwnedThenPin->LinkedTo.Contains(SequenceExecIn));
	TestTrue(TEXT("sequence has at least two Then outputs"), SequenceThenPins.Num() >= 2);
	if (SequenceThenPins.Num() >= 2)
	{
		TestTrue(TEXT("first branch calls inserted owned block"), InsertedExecIn && SequenceThenPins[0]->LinkedTo.Contains(InsertedExecIn));
		TestTrue(TEXT("second branch preserves original successor"), BranchExecIn && SequenceThenPins[1]->LinkedTo.Contains(BranchExecIn));
	}
	TestFalse(TEXT("old direct link is replaced by sequence"), OwnedThenPin && BranchExecIn && OwnedThenPin->LinkedTo.Contains(BranchExecIn));
	return true;
}

#endif
