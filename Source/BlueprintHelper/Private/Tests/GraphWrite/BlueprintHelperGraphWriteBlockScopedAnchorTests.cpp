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
#include "K2Node_IfThenElse.h"
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

	const FBlueprintHelperToolResultBase Result = PatchService.Execute(MakeSetNodeCommentPayload(Fixture));

	TestTrue(TEXT("set_node_comment resolves block-local nodes[0]"), Result.bOk);
	TestEqual(TEXT("owned entry comment is changed"), Fixture.OwnedEntry->NodeComment, FString(TEXT("Updated block entry comment")));
	TestFalse(TEXT("whole-graph nodes[0] was not patched"), Fixture.FirstUnownedEvent->NodeComment == FString(TEXT("Updated block entry comment")));
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

#endif
