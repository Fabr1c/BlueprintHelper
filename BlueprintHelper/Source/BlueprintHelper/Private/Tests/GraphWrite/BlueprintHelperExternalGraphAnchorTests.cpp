#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorResolver.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorService.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperMergeExternalFlowService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicGroupBuilder.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintTextConverter.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BlueprintHelperExternalGraphAnchorTests
{
	static FString MakeTestObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static UBlueprint* MakeBlueprint(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperExternalAnchor/%s"),
			*MakeTestObjectName(Prefix)));
		Package->SetDirtyFlag(false);

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*MakeTestObjectName(TEXT("BP_ExternalAnchor")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperExternalGraphAnchorTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static UEdGraph* GetEventGraph(UBlueprint* Blueprint)
	{
		return Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
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

	static UK2Node_CallFunction* AddDestroyActorCallNode(UEdGraph* Graph)
	{
		if (!Graph)
		{
			return nullptr;
		}

		UFunction* Function = AActor::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(AActor, K2_DestroyActor));
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
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		return FromPin && ToPin && Schema && Schema->TryCreateConnection(FromPin, ToPin);
	}

	static TSharedRef<FJsonObject> MakeEmptyLogicSpec()
	{
		TSharedRef<FJsonObject> LogicSpec = MakeShared<FJsonObject>();
		LogicSpec->SetStringField(TEXT("schema"), TEXT("BlueprintLogicSpec.v1"));
		LogicSpec->SetArrayField(TEXT("statements"), {});
		return LogicSpec;
	}

	static TSharedRef<FJsonObject> MakeMergeExternalFlowPayload(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const FBlueprintHelperExternalGraphAnchor& Anchor,
		const TArray<FString>& SequenceOrder)
	{
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), Blueprint ? Blueprint->GetPathName() : FString());
		Target->SetStringField(TEXT("graph"), Graph ? Graph->GetName() : FString());

		TSharedRef<FJsonObject> Inserted = MakeShared<FJsonObject>();
		Inserted->SetStringField(TEXT("block_id"), TEXT("ExternalMergeTest"));
		Inserted->SetObjectField(TEXT("body"), MakeEmptyLogicSpec());

		TArray<TSharedPtr<FJsonValue>> SequenceValues;
		for (const FString& Item : SequenceOrder)
		{
			SequenceValues.Add(MakeShared<FJsonValueString>(Item));
		}

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("target"), Target);
		Payload->SetStringField(TEXT("insert_strategy"), TEXT("branch_fork"));
		Payload->SetObjectField(TEXT("anchor"), Anchor.ToJson());
		Payload->SetObjectField(TEXT("inserted"), Inserted);
		Payload->SetArrayField(TEXT("sequence_order"), SequenceValues);
		Payload->SetBoolField(TEXT("dry_run"), true);
		return Payload;
	}

	static bool BuildNodeAnchor(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		UEdGraphNode* Node,
		FBlueprintHelperExternalGraphAnchor& OutAnchor,
		FString& OutError)
	{
		const FBlueprintHelperExternalGraphAnchorService Service;
		return Service.BuildNodeAnchor(
			Blueprint ? Blueprint->GetPathName() : TEXT(""),
			Graph ? Graph->GetName() : TEXT(""),
			Node,
			OutAnchor,
			OutError);
	}

	static bool BuildBoundaryAnchor(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		UEdGraphPin* SourcePin,
		FBlueprintHelperExternalGraphAnchor& OutAnchor,
		FString& OutError)
	{
		const FBlueprintHelperExternalGraphAnchorService Service;
		return Service.BuildExecBoundaryAnchor(
			Blueprint ? Blueprint->GetPathName() : TEXT(""),
			Graph ? Graph->GetName() : TEXT(""),
			SourcePin,
			OutAnchor,
			OutError);
	}

	static const TSharedPtr<FJsonObject>* FindExportedNodeObjectByGuid(
		const TSharedPtr<FJsonObject>& Root,
		const FString& NodeGuid)
	{
		const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
		if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("nodes"), Nodes) || !Nodes)
		{
			return nullptr;
		}

		for (const TSharedPtr<FJsonValue>& NodeValue : *Nodes)
		{
			const TSharedPtr<FJsonObject>* NodeObj = nullptr;
			if (!NodeValue.IsValid() || !NodeValue->TryGetObject(NodeObj) || !NodeObj || !NodeObj->IsValid())
			{
				continue;
			}

			FString CandidateGuid;
			if ((*NodeObj)->TryGetStringField(TEXT("node_guid"), CandidateGuid)
				&& CandidateGuid.Equals(NodeGuid, ESearchCase::IgnoreCase))
			{
				return NodeObj;
			}
		}
		return nullptr;
	}

	static const FBlueprintHelperLogicNode* FindLogicNodeWithExternalAnchor(
		const FBlueprintHelperLogicJsonPayload& Payload)
	{
		for (const FBlueprintHelperLogicGroup& Group : Payload.Groups)
		{
			for (const FBlueprintHelperLogicNode& Node : Group.Nodes)
			{
				if (Node.ExternalAnchor.IsValid())
				{
					return &Node;
				}
			}
		}
		for (const FBlueprintHelperLogicNode& Node : Payload.Nodes)
		{
			if (Node.ExternalAnchor.IsValid())
			{
				return &Node;
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExternalGraphAnchorNodeRoundTripTest,
	"BlueprintHelper.GraphWrite.ExternalAnchor.NodeRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperExternalGraphAnchorNodeRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperExternalGraphAnchorTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("NodeRoundTrip"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CustomEvent* EventNode = AddCustomEventNode(Graph, TEXT("OpenDoor"));
	TestNotNull(TEXT("event node exists"), EventNode);
	if (!Blueprint || !Graph || !EventNode)
	{
		return false;
	}

	FString Error;
	FBlueprintHelperExternalGraphAnchor Anchor;
	TestTrue(TEXT("node anchor builds"), BuildNodeAnchor(Blueprint, Graph, EventNode, Anchor, Error));
	TestEqual(TEXT("schema"), Anchor.Schema, FString(FBlueprintHelperExternalGraphAnchor::SchemaString));
	TestEqual(TEXT("asset path"), Anchor.AssetPath, Blueprint->GetPathName());
	TestEqual(TEXT("graph name"), Anchor.GraphName, Graph->GetName());
	TestEqual(TEXT("node guid"), Anchor.NodeGuid, EventNode->NodeGuid.ToString(EGuidFormats::Digits));
	TestFalse(TEXT("node fingerprint populated"), Anchor.Fingerprint.IsEmpty());

	UEdGraphNode* ResolvedNode = nullptr;
	const FBlueprintHelperExternalGraphAnchorResolver Resolver;
	TestTrue(TEXT("node anchor resolves"), Resolver.ResolveNode(Anchor, ResolvedNode, Error));
	TestTrue(TEXT("resolved node"), ResolvedNode == static_cast<UEdGraphNode*>(EventNode));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExternalGraphAnchorExecBoundaryRoundTripTest,
	"BlueprintHelper.GraphWrite.ExternalAnchor.ExecBoundaryRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperExternalGraphAnchorExecBoundaryRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperExternalGraphAnchorTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("ExecBoundaryRoundTrip"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CustomEvent* EventNode = AddCustomEventNode(Graph, TEXT("OpenDoor"));
	UK2Node_CallFunction* CallNode = AddDestroyActorCallNode(Graph);
	UEdGraphPin* SourcePin = FindExecPin(EventNode, EGPD_Output);
	UEdGraphPin* TargetPin = FindExecPin(CallNode, EGPD_Input);
	TestTrue(TEXT("exec pins connect"), ConnectExecPins(SourcePin, TargetPin));
	if (!Blueprint || !Graph || !SourcePin || !TargetPin)
	{
		return false;
	}

	FString Error;
	FBlueprintHelperExternalGraphAnchor Anchor;
	TestTrue(TEXT("boundary anchor builds"), BuildBoundaryAnchor(Blueprint, Graph, SourcePin, Anchor, Error));
	TestTrue(TEXT("boundary role"), Anchor.SemanticRole == EBlueprintHelperExternalGraphAnchorRole::ExecBoundary);
	TestEqual(TEXT("pin name"), Anchor.PinName, SourcePin->PinName.ToString());
	TestEqual(TEXT("pin direction"), Anchor.PinDirection, FString(TEXT("output")));

	UEdGraphPin* ResolvedPin = nullptr;
	const FBlueprintHelperExternalGraphAnchorResolver Resolver;
	TestTrue(TEXT("boundary anchor resolves"), Resolver.ResolvePin(Anchor, ResolvedPin, Error));
	TestTrue(TEXT("resolved pin"), ResolvedPin == SourcePin);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExternalGraphAnchorRejectsStaleNodeTest,
	"BlueprintHelper.GraphWrite.ExternalAnchor.RejectsStaleNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperExternalGraphAnchorRejectsStaleNodeTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperExternalGraphAnchorTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("RejectsStaleNode"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CustomEvent* EventNode = AddCustomEventNode(Graph, TEXT("OpenDoor"));
	if (!Blueprint || !Graph || !EventNode)
	{
		return false;
	}

	FString Error;
	FBlueprintHelperExternalGraphAnchor Anchor;
	TestTrue(TEXT("node anchor builds"), BuildNodeAnchor(Blueprint, Graph, EventNode, Anchor, Error));
	Anchor.Fingerprint = TEXT("stale");

	UEdGraphNode* ResolvedNode = nullptr;
	const FBlueprintHelperExternalGraphAnchorResolver Resolver;
	TestFalse(TEXT("stale node anchor rejected"), Resolver.ResolveNode(Anchor, ResolvedNode, Error));
	TestEqual(TEXT("stale node error code"), Error, FString(TEXT("external_anchor_fingerprint_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExternalGraphAnchorRejectsStaleBoundaryLinkTest,
	"BlueprintHelper.GraphWrite.ExternalAnchor.RejectsStaleBoundaryLink",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperExternalGraphAnchorRejectsStaleBoundaryLinkTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperExternalGraphAnchorTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("RejectsStaleBoundaryLink"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CustomEvent* EventNode = AddCustomEventNode(Graph, TEXT("OpenDoor"));
	UEdGraphPin* SourcePin = FindExecPin(EventNode, EGPD_Output);
	if (!Blueprint || !Graph || !SourcePin)
	{
		return false;
	}

	FString Error;
	FBlueprintHelperExternalGraphAnchor Anchor;
	TestTrue(TEXT("boundary anchor builds before link change"), BuildBoundaryAnchor(Blueprint, Graph, SourcePin, Anchor, Error));

	UK2Node_CallFunction* CallNode = AddDestroyActorCallNode(Graph);
	UEdGraphPin* TargetPin = FindExecPin(CallNode, EGPD_Input);
	TestTrue(TEXT("exec pins connect after anchor"), ConnectExecPins(SourcePin, TargetPin));

	UEdGraphPin* ResolvedPin = nullptr;
	const FBlueprintHelperExternalGraphAnchorResolver Resolver;
	TestFalse(TEXT("stale boundary anchor rejected"), Resolver.ResolvePin(Anchor, ResolvedPin, Error));
	TestEqual(TEXT("stale boundary error code"), Error, FString(TEXT("external_anchor_fingerprint_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExternalGraphAnchorDoesNotUseDisplayNameTest,
	"BlueprintHelper.GraphWrite.ExternalAnchor.DoesNotUseDisplayName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperExternalGraphAnchorDoesNotUseDisplayNameTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperExternalGraphAnchorTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("DoesNotUseDisplayName"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CustomEvent* EventNode = AddCustomEventNode(Graph, TEXT("OpenDoor"));
	if (!Blueprint || !Graph || !EventNode)
	{
		return false;
	}

	FString Error;
	FBlueprintHelperExternalGraphAnchor Anchor;
	TestTrue(TEXT("node anchor builds"), BuildNodeAnchor(Blueprint, Graph, EventNode, Anchor, Error));
	const FString InitialFingerprint = Anchor.Fingerprint;

	EventNode->CustomFunctionName = FName(TEXT("RenamedDoorEvent"));
	EventNode->NodeComment = TEXT("Display text changed after read");

	FBlueprintHelperExternalGraphAnchor NewAnchor;
	TestTrue(TEXT("renamed node anchor rebuilds"), BuildNodeAnchor(Blueprint, Graph, EventNode, NewAnchor, Error));
	TestEqual(TEXT("display name does not affect fingerprint"), NewAnchor.Fingerprint, InitialFingerprint);

	UEdGraphNode* ResolvedNode = nullptr;
	const FBlueprintHelperExternalGraphAnchorResolver Resolver;
	TestTrue(TEXT("anchor still resolves after display name change"), Resolver.ResolveNode(Anchor, ResolvedNode, Error));
	TestTrue(TEXT("resolved node"), ResolvedNode == static_cast<UEdGraphNode*>(EventNode));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExternalGraphAnchorDoesNotWriteOwnershipMetadataTest,
	"BlueprintHelper.GraphWrite.ExternalAnchor.DoesNotWriteOwnershipMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperExternalGraphAnchorDoesNotWriteOwnershipMetadataTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperExternalGraphAnchorTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("DoesNotWriteOwnershipMetadata"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CustomEvent* EventNode = AddCustomEventNode(Graph, TEXT("OpenDoor"));
	if (!Blueprint || !Graph || !EventNode)
	{
		return false;
	}

	FString Error;
	FBlueprintHelperExternalGraphAnchor Anchor;
	TestTrue(TEXT("node anchor builds"), BuildNodeAnchor(Blueprint, Graph, EventNode, Anchor, Error));

	FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(EventNode->GetOutermost());
	TestTrue(TEXT("external anchor does not write BlueprintHelperOwned"),
		MetaData.GetValue(EventNode, TEXT("BlueprintHelperOwned")).IsEmpty());
	TestTrue(TEXT("external anchor does not write BlueprintHelperBlockId"),
		MetaData.GetValue(EventNode, TEXT("BlueprintHelperBlockId")).IsEmpty());

	const TSharedPtr<FJsonObject> RawGraph = FBlueprintToTextConverter::ConvertGraphToJsonObject(Graph);
	const TSharedPtr<FJsonObject>* ExportedNode = FindExportedNodeObjectByGuid(
		RawGraph,
		EventNode->NodeGuid.ToString(EGuidFormats::Digits));
	TestNotNull(TEXT("exported node object"), ExportedNode);
	if (!ExportedNode || !ExportedNode->IsValid())
	{
		return false;
	}

	TestTrue(TEXT("raw export has external_anchor"), (*ExportedNode)->HasField(TEXT("external_anchor")));
	TestFalse(TEXT("raw export has no ownership metadata"), (*ExportedNode)->HasField(TEXT("metadata")));

	FBlueprintHelperLogicGroupBuilder Builder;
	const FBlueprintHelperLogicJsonPayload Payload = Builder.BuildGroups(
		RawGraph,
		Blueprint->GetPathName(),
		Graph->GetName(),
		EBlueprintHelperLogicScope::TargetGraph);
	TestEqual(TEXT("logic payload has one group"), Payload.Groups.Num(), 1);
	if (Payload.Groups.Num() != 1 || Payload.Groups[0].Nodes.Num() == 0)
	{
		return false;
	}

	const FBlueprintHelperLogicNode* LogicNode = FindLogicNodeWithExternalAnchor(Payload);
	TestNotNull(TEXT("logic node with external anchor"), LogicNode);
	if (!LogicNode)
	{
		return false;
	}

	const TSharedRef<FJsonObject> LogicNodeJson = LogicNode->ToJson();
	TestTrue(TEXT("LogicJson preserves external_anchor"), LogicNodeJson->HasField(TEXT("external_anchor")));
	TestFalse(TEXT("LogicJson does not synthesize block_id"), Payload.Groups[0].ToJson()->HasField(TEXT("block_id")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMergeExternalFlowRejectsDuplicateSequenceOrderTest,
	"BlueprintHelper.GraphWrite.ExternalAnchor.MergeExternalFlowRejectsDuplicateSequenceOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperMergeExternalFlowRejectsDuplicateSequenceOrderTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperExternalGraphAnchorTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("MergeExternalFlowDuplicateSequenceOrder"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CustomEvent* EventNode = AddCustomEventNode(Graph, TEXT("OpenDoor"));
	UEdGraphPin* SourcePin = FindExecPin(EventNode, EGPD_Output);
	if (!Blueprint || !Graph || !SourcePin)
	{
		return false;
	}

	FString Error;
	FBlueprintHelperExternalGraphAnchor Anchor;
	TestTrue(TEXT("boundary anchor builds"), BuildBoundaryAnchor(Blueprint, Graph, SourcePin, Anchor, Error));

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	const FBlueprintHelperMergeExternalFlowService Service(Resolver, BlockIdService, OwnershipService);

	const FBlueprintHelperToolResultBase DuplicateInserted = Service.Execute(MakeMergeExternalFlowPayload(
		Blueprint,
		Graph,
		Anchor,
		{TEXT("inserted_logic"), TEXT("inserted_logic")}));
	TestFalse(TEXT("duplicate inserted_logic sequence_order is rejected"), DuplicateInserted.bOk);
	TestEqual(TEXT("duplicate inserted_logic error code"),
		DuplicateInserted.Error.IsSet() ? DuplicateInserted.Error->Code : FString(),
		FString(TEXT("sequence_order_invalid")));

	const FBlueprintHelperToolResultBase DuplicateOriginal = Service.Execute(MakeMergeExternalFlowPayload(
		Blueprint,
		Graph,
		Anchor,
		{TEXT("inserted_logic"), TEXT("original_successor"), TEXT("original_successor")}));
	TestFalse(TEXT("duplicate original_successor sequence_order is rejected"), DuplicateOriginal.bOk);
	TestEqual(TEXT("duplicate original_successor error code"),
		DuplicateOriginal.Error.IsSet() ? DuplicateOriginal.Error->Code : FString(),
		FString(TEXT("sequence_order_invalid")));
	return true;
}

#endif
