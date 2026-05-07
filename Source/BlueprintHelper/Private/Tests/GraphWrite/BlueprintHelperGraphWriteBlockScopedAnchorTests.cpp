#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "GraphSupport/BlueprintHelperGraphResolver.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_IfThenElse.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logic/BlueprintHelperLogicJsonPathService.h"
#include "Misc/AutomationTest.h"
#include "Services/GraphWrite/BlueprintHelperMergeBlueprintGraphService.h"
#include "Services/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h"
#include "Transactions/Transactions/BlueprintHelperTransactionJournalService.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace
{
	FString MakeBlockScopedAnchorTestObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	UBlueprint* MakeBlockScopedAnchorTestBlueprint(const FString& Prefix)
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

	UK2Node_CustomEvent* AddCustomEventNode(UEdGraph* Graph, const FString& EventName)
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

	UK2Node_IfThenElse* AddBranchNode(UEdGraph* Graph)
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

	UK2Node_CallFunction* AddNativeFunctionCallNode(UEdGraph* Graph, const FName FunctionName)
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

	UEdGraphPin* FindPinByName(UEdGraphNode* Node, const TCHAR* PinName)
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

	UEdGraphPin* FindExecPin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
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

	bool ConnectExecPins(UEdGraphPin* FromPin, UEdGraphPin* ToPin)
	{
		if (!FromPin || !ToPin)
		{
			return false;
		}

		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		return Schema && Schema->TryCreateConnection(FromPin, ToPin);
	}

	void MarkNodeAsBlueprintHelperOwned(UEdGraphNode* Node, const FString& BlockId)
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

	void MarkNodeWithLegacyBlueprintHelperComment(UEdGraphNode* Node, const FString& BlockId)
	{
		if (!Node)
		{
			return;
		}

		if (UPackage* Package = Node->GetOutermost())
		{
			FMetaData& MetaData = Package->GetMetaData();
			MetaData.RemoveValue(Node, TEXT("BlueprintHelperOwned"));
			MetaData.RemoveValue(Node, TEXT("BlueprintHelperBlockId"));
			MetaData.RemoveValue(Node, TEXT("BlueprintHelperTransactionId"));
			MetaData.RemoveValue(Node, TEXT("BlueprintHelperFeatureName"));
			MetaData.RemoveValue(Node, TEXT("BlueprintHelperTool"));
		}

		Node->NodeComment = FString::Printf(TEXT("[BlueprintHelper]\nblock_id=%s\ntx=legacy_tx"), *BlockId);
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

	FBlockScopedGraph MakeBlockScopedGraph(const FString& Prefix)
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

	TSharedRef<FJsonObject> MakePatchPayload(
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

	TSharedRef<FJsonObject> MakeSetPinDefaultPayload(const FBlockScopedGraph& Fixture)
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

	TSharedRef<FJsonObject> MakeSetNodeCommentPayload(const FBlockScopedGraph& Fixture)
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

	TSharedRef<FJsonObject> MakeInsertFlowPayload(const FBlockScopedGraph& Fixture)
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
		Inserted->SetStringField(TEXT("function"), TEXT("K2_DestroyActor"));
		Payload->SetObjectField(TEXT("inserted"), Inserted);
		Payload->SetBoolField(TEXT("dry_run"), false);
		return Payload;
	}

	TSharedRef<FJsonObject> MakeInsertBetweenFlowPayload(const FBlockScopedGraph& Fixture)
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

	TSharedRef<FJsonObject> MakeBranchForkOwnedBlockCallPayload(
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

	UK2Node_CallFunction* FindCallFunctionNode(UEdGraph* Graph, const FName FunctionName)
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

	UK2Node_ExecutionSequence* FindSequenceNode(UEdGraph* Graph)
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

	TArray<UEdGraphPin*> FindExecPins(UEdGraphNode* Node, EEdGraphPinDirection Direction)
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

	bool GetMergeDryRunStatus(
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePatchSetPinDefaultBlockScopedAnchorTest,
	"BlueprintHelper.GraphWrite.BlockScopedAnchors.PatchSetPinDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePatchSetPinDefaultBlockScopedAnchorTest::RunTest(const FString& Parameters)
{
	const FBlockScopedGraph Fixture = MakeBlockScopedGraph(TEXT("PatchSetPinDefaultBlockScoped"));
	TestNotNull(TEXT("test blueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("test graph is available"), Fixture.Graph);
	TestNotNull(TEXT("owned branch node is created"), Fixture.OwnedBranch);
	if (!Fixture.Blueprint || !Fixture.Graph || !Fixture.OwnedBranch)
	{
		return false;
	}

	UEdGraphPin* ConditionPin = FindPinByName(Fixture.OwnedBranch, TEXT("Condition"));
	TestNotNull(TEXT("owned branch has Condition pin"), ConditionPin);
	if (!ConditionPin)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperTransactionJournalService JournalService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService, JournalService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(MakeSetPinDefaultPayload(Fixture));

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
	const FBlockScopedGraph Fixture = MakeBlockScopedGraph(TEXT("PatchSetNodeCommentBlockScoped"));
	TestNotNull(TEXT("test blueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("owned entry node is created"), Fixture.OwnedEntry);
	TestNotNull(TEXT("unowned leading node is created"), Fixture.FirstUnownedEvent);
	if (!Fixture.Blueprint || !Fixture.OwnedEntry || !Fixture.FirstUnownedEvent)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperTransactionJournalService JournalService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService, JournalService);

	TestFalse(TEXT("metadata-owned entry does not need legacy block_id comment"),
		Fixture.OwnedEntry->NodeComment.Contains(TEXT("block_id=")));
	const FBlueprintHelperToolResultBase Result = PatchService.Execute(MakeSetNodeCommentPayload(Fixture));

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
	const FBlockScopedGraph Fixture = MakeBlockScopedGraph(TEXT("PatchLegacyManagedCommentFallback"));
	TestNotNull(TEXT("fixture graph exists"), Fixture.Graph);
	TestNotNull(TEXT("owned entry exists"), Fixture.OwnedEntry);
	TestNotNull(TEXT("owned branch exists"), Fixture.OwnedBranch);
	if (!Fixture.Graph || !Fixture.OwnedEntry || !Fixture.OwnedBranch)
	{
		return false;
	}

	MarkNodeWithLegacyBlueprintHelperComment(Fixture.OwnedEntry, Fixture.BlockId);
	MarkNodeWithLegacyBlueprintHelperComment(Fixture.OwnedBranch, Fixture.BlockId);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperTransactionJournalService JournalService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService, JournalService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(MakeSetNodeCommentPayload(Fixture));

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
	FBlockScopedGraph Fixture = MakeBlockScopedGraph(TEXT("PatchMetadataWinsOverLegacyComment"));
	TestNotNull(TEXT("fixture graph exists"), Fixture.Graph);
	TestNotNull(TEXT("owned entry exists"), Fixture.OwnedEntry);
	TestNotNull(TEXT("owned branch exists"), Fixture.OwnedBranch);
	if (!Fixture.Graph || !Fixture.OwnedEntry || !Fixture.OwnedBranch)
	{
		return false;
	}

	const FString LegacyBlockId = FString::Printf(TEXT("%s_Legacy"), *Fixture.BlockId);
	const FString StaleLegacyComment = FString::Printf(TEXT("[BlueprintHelper]\nblock_id=%s\ntx=stale_legacy_tx"), *LegacyBlockId);
	Fixture.OwnedEntry->NodeComment = StaleLegacyComment;
	Fixture.OwnedBranch->NodeComment = StaleLegacyComment;
	Fixture.BlockId = LegacyBlockId;

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperTransactionJournalService JournalService;
	FBlueprintHelperPatchBlueprintGraphService PatchService(Resolver, PathService, JournalService);

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(MakeSetNodeCommentPayload(Fixture));

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
	const FBlockScopedGraph Fixture = MakeBlockScopedGraph(TEXT("MergeInsertFlowBlockScoped"));
	TestNotNull(TEXT("test blueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("owned entry node is created"), Fixture.OwnedEntry);
	TestNotNull(TEXT("unowned leading node is created"), Fixture.FirstUnownedEvent);
	if (!Fixture.Blueprint || !Fixture.OwnedEntry || !Fixture.FirstUnownedEvent)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperTransactionJournalService JournalService;
	FBlueprintHelperMergeBlueprintGraphService MergeService(Resolver, PathService, JournalService);

	const FBlueprintHelperToolResultBase Result = MergeService.Execute(MakeInsertFlowPayload(Fixture));

	UEdGraphPin* OwnedThenPin = FindPinByName(Fixture.OwnedEntry, TEXT("Then"));
	UEdGraphPin* UnownedThenPin = FindPinByName(Fixture.FirstUnownedEvent, TEXT("Then"));
	TestTrue(TEXT("insert_flow resolves block-local anchor nodes[0]"), Result.bOk);
	TestTrue(TEXT("owned anchor Then pin receives inserted flow"), OwnedThenPin && OwnedThenPin->LinkedTo.Num() == 1);
	TestTrue(TEXT("whole-graph nodes[0] was not used as anchor"), !UnownedThenPin || UnownedThenPin->LinkedTo.Num() == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteMergeInsertBetweenBlockScopedAnchorTest,
	"BlueprintHelper.GraphWrite.BlockScopedAnchors.MergeInsertBetween",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteMergeInsertBetweenBlockScopedAnchorTest::RunTest(const FString& Parameters)
{
	const FBlockScopedGraph Fixture = MakeBlockScopedGraph(TEXT("MergeInsertBetweenBlockScoped"));
	TestNotNull(TEXT("test blueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("owned entry node is created"), Fixture.OwnedEntry);
	TestNotNull(TEXT("owned branch node is created"), Fixture.OwnedBranch);
	if (!Fixture.Blueprint || !Fixture.OwnedEntry || !Fixture.OwnedBranch)
	{
		return false;
	}

	UEdGraphPin* OwnedThenPin = FindPinByName(Fixture.OwnedEntry, TEXT("Then"));
	UEdGraphPin* BranchExecIn = FindExecPin(Fixture.OwnedBranch, EGPD_Input);
	TestTrue(TEXT("fixture starts with owned entry linked to owned branch"), ConnectExecPins(OwnedThenPin, BranchExecIn));
	TestTrue(TEXT("owned entry has one original successor before merge"), OwnedThenPin && OwnedThenPin->LinkedTo.Num() == 1);
	if (!OwnedThenPin || !BranchExecIn || OwnedThenPin->LinkedTo.Num() != 1)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperTransactionJournalService JournalService;
	FBlueprintHelperMergeBlueprintGraphService MergeService(Resolver, PathService, JournalService);

	const FBlueprintHelperToolResultBase Result = MergeService.Execute(MakeInsertBetweenFlowPayload(Fixture));

	UK2Node_CallFunction* InsertedCall = FindCallFunctionNode(Fixture.Graph, GET_FUNCTION_NAME_CHECKED(AActor, K2_SetActorLocation));
	UEdGraphPin* InsertedExecIn = FindExecPin(InsertedCall, EGPD_Input);
	UEdGraphPin* InsertedExecOut = FindExecPin(InsertedCall, EGPD_Output);

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
	const FBlockScopedGraph Fixture = MakeBlockScopedGraph(TEXT("MergeBranchForkOwnedBlockCall"));
	TestNotNull(TEXT("test blueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("owned entry node is created"), Fixture.OwnedEntry);
	TestNotNull(TEXT("owned branch node is created"), Fixture.OwnedBranch);
	if (!Fixture.Blueprint || !Fixture.OwnedEntry || !Fixture.OwnedBranch)
	{
		return false;
	}

	UEdGraphPin* OwnedThenPin = FindPinByName(Fixture.OwnedEntry, TEXT("Then"));
	UEdGraphPin* BranchExecIn = FindExecPin(Fixture.OwnedBranch, EGPD_Input);
	TestTrue(TEXT("fixture starts with owned entry linked to owned branch"), ConnectExecPins(OwnedThenPin, BranchExecIn));
	TestTrue(TEXT("owned entry has one original successor before branch fork"), OwnedThenPin && OwnedThenPin->LinkedTo.Num() == 1);
	if (!OwnedThenPin || !BranchExecIn || OwnedThenPin->LinkedTo.Num() != 1)
	{
		return false;
	}

	const FString InsertedBlockId = FString::Printf(TEXT("%s_InsertedBlockScoped0"), *Fixture.Graph->GetName());
	UK2Node_CustomEvent* InsertedOwnedEntry = AddCustomEventNode(Fixture.Graph, TEXT("OwnedInsertedBlock"));
	TestNotNull(TEXT("inserted owned block entry is created"), InsertedOwnedEntry);
	if (!InsertedOwnedEntry)
	{
		return false;
	}
	MarkNodeAsBlueprintHelperOwned(InsertedOwnedEntry, InsertedBlockId);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Fixture.Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Fixture.Blueprint);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperTransactionJournalService JournalService;
	FBlueprintHelperMergeBlueprintGraphService MergeService(Resolver, PathService, JournalService);

	const FBlueprintHelperToolResultBase Result = MergeService.Execute(MakeBranchForkOwnedBlockCallPayload(Fixture, InsertedBlockId));

	UK2Node_ExecutionSequence* SequenceNode = FindSequenceNode(Fixture.Graph);
	UK2Node_CallFunction* InsertedCall = FindCallFunctionNode(Fixture.Graph, FName(TEXT("OwnedInsertedBlock")));
	UEdGraphPin* SequenceExecIn = FindExecPin(SequenceNode, EGPD_Input);
	const TArray<UEdGraphPin*> SequenceThenPins = FindExecPins(SequenceNode, EGPD_Output);
	UEdGraphPin* InsertedExecIn = FindExecPin(InsertedCall, EGPD_Input);

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
	const FBlockScopedGraph Fixture = MakeBlockScopedGraph(TEXT("MergeBranchForkMissingOwnedBlockPreviewBlocks"));
	TestNotNull(TEXT("test blueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("owned entry node is created"), Fixture.OwnedEntry);
	TestNotNull(TEXT("owned branch node is created"), Fixture.OwnedBranch);
	if (!Fixture.Blueprint || !Fixture.OwnedEntry || !Fixture.OwnedBranch)
	{
		return false;
	}

	UEdGraphPin* OwnedThenPin = FindPinByName(Fixture.OwnedEntry, TEXT("Then"));
	UEdGraphPin* BranchExecIn = FindExecPin(Fixture.OwnedBranch, EGPD_Input);
	TestTrue(TEXT("fixture starts with one original successor"), ConnectExecPins(OwnedThenPin, BranchExecIn));
	if (!OwnedThenPin || !BranchExecIn || OwnedThenPin->LinkedTo.Num() != 1)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperTransactionJournalService JournalService;
	FBlueprintHelperMergeBlueprintGraphService MergeService(Resolver, PathService, JournalService);

	const FString MissingBlockId = FString::Printf(TEXT("%s_MissingInsertedBlock0"), *Fixture.Graph->GetName());
	const FBlueprintHelperToolResultBase Result = MergeService.Execute(
		MakeBranchForkOwnedBlockCallPayload(Fixture, MissingBlockId, true));

	FString DryRunResult;
	bool bCanExecute = true;
	TestTrue(TEXT("dry-run status is present"), GetMergeDryRunStatus(Result, DryRunResult, bCanExecute));
	TestFalse(TEXT("missing inserted block preview is blocked"), Result.bOk);
	TestEqual(TEXT("dry-run result is blocked"), DryRunResult, FString(TEXT("blocked")));
	TestFalse(TEXT("dry-run cannot execute"), bCanExecute);
	TestTrue(TEXT("missing inserted block error is surfaced"),
		Result.Error.IsSet() &&
		Result.Error->Code == TEXT("inserted_logic_not_found") &&
		!Result.Error->Message.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteMergeBranchForkUncompiledOwnedBlockCallTest,
	"BlueprintHelper.GraphWrite.BlockScopedAnchors.MergeBranchForkUncompiledOwnedBlockCall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteMergeBranchForkUncompiledOwnedBlockCallTest::RunTest(const FString& Parameters)
{
	const FBlockScopedGraph Fixture = MakeBlockScopedGraph(TEXT("MergeBranchForkUncompiledOwnedBlockCall"));
	TestNotNull(TEXT("test blueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("owned entry node is created"), Fixture.OwnedEntry);
	TestNotNull(TEXT("owned branch node is created"), Fixture.OwnedBranch);
	if (!Fixture.Blueprint || !Fixture.OwnedEntry || !Fixture.OwnedBranch)
	{
		return false;
	}

	UEdGraphPin* OwnedThenPin = FindPinByName(Fixture.OwnedEntry, TEXT("Then"));
	UEdGraphPin* BranchExecIn = FindExecPin(Fixture.OwnedBranch, EGPD_Input);
	TestTrue(TEXT("fixture starts with owned entry linked to owned branch"), ConnectExecPins(OwnedThenPin, BranchExecIn));
	TestTrue(TEXT("owned entry has one original successor before branch fork"), OwnedThenPin && OwnedThenPin->LinkedTo.Num() == 1);
	if (!OwnedThenPin || !BranchExecIn || OwnedThenPin->LinkedTo.Num() != 1)
	{
		return false;
	}

	const FString InsertedBlockId = FString::Printf(TEXT("%s_UncompiledInsertedBlock0"), *Fixture.Graph->GetName());
	const FString InsertedEventName = TEXT("OwnedUncompiledInsertedBlock");
	UK2Node_CustomEvent* InsertedOwnedEntry = AddCustomEventNode(Fixture.Graph, InsertedEventName);
	TestNotNull(TEXT("inserted owned block entry is created"), InsertedOwnedEntry);
	if (!InsertedOwnedEntry)
	{
		return false;
	}
	MarkNodeAsBlueprintHelperOwned(InsertedOwnedEntry, InsertedBlockId);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Fixture.Blueprint);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperLogicJsonPathService PathService;
	FBlueprintHelperTransactionJournalService JournalService;
	FBlueprintHelperMergeBlueprintGraphService MergeService(Resolver, PathService, JournalService);

	const FBlueprintHelperToolResultBase Result = MergeService.Execute(
		MakeBranchForkOwnedBlockCallPayload(Fixture, InsertedBlockId, false, true));

	UK2Node_ExecutionSequence* SequenceNode = FindSequenceNode(Fixture.Graph);
	UK2Node_CallFunction* InsertedCall = FindCallFunctionNode(Fixture.Graph, FName(*InsertedEventName));
	UEdGraphPin* SequenceExecIn = FindExecPin(SequenceNode, EGPD_Input);
	const TArray<UEdGraphPin*> SequenceThenPins = FindExecPins(SequenceNode, EGPD_Output);
	UEdGraphPin* InsertedExecIn = FindExecPin(InsertedCall, EGPD_Input);

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
