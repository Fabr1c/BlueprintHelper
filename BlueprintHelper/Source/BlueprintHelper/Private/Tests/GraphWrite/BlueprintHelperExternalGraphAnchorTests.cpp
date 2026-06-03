#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorResolver.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorService.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperMergeExternalFlowService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicGroupBuilder.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonPathService.h"
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

	static TSharedRef<FJsonObject> MakeStringLiteralExpression(const FString& Value)
	{
		TSharedRef<FJsonObject> Literal = MakeShared<FJsonObject>();
		Literal->SetStringField(TEXT("kind"), TEXT("literal"));
		Literal->SetStringField(TEXT("value_type"), TEXT("string"));
		Literal->SetStringField(TEXT("value"), Value);
		return Literal;
	}

	static TSharedRef<FJsonObject> MakeIntLiteralExpression(int32 Value)
	{
		TSharedRef<FJsonObject> Literal = MakeShared<FJsonObject>();
		Literal->SetStringField(TEXT("kind"), TEXT("literal"));
		Literal->SetStringField(TEXT("value_type"), TEXT("int"));
		Literal->SetNumberField(TEXT("value"), Value);
		return Literal;
	}

	static TSharedRef<FJsonObject> MakeBoolLiteralExpression(bool bValue)
	{
		TSharedRef<FJsonObject> Literal = MakeShared<FJsonObject>();
		Literal->SetStringField(TEXT("kind"), TEXT("literal"));
		Literal->SetStringField(TEXT("value_type"), TEXT("bool"));
		Literal->SetBoolField(TEXT("value"), bValue);
		return Literal;
	}

	static TSharedRef<FJsonObject> MakePrintStringLogicSpec(const FString& Message)
	{
		TSharedRef<FJsonObject> Statement = MakeShared<FJsonObject>();
		Statement->SetStringField(TEXT("kind"), TEXT("call"));
		Statement->SetStringField(TEXT("target"), TEXT("PrintString"));

		TSharedRef<FJsonObject> Args = MakeShared<FJsonObject>();
		Args->SetObjectField(TEXT("InString"), MakeStringLiteralExpression(Message));
		Statement->SetObjectField(TEXT("args"), Args);

		TArray<TSharedPtr<FJsonValue>> Statements;
		Statements.Add(MakeShared<FJsonValueObject>(Statement));

		TSharedRef<FJsonObject> LogicSpec = MakeShared<FJsonObject>();
		LogicSpec->SetStringField(TEXT("schema"), TEXT("BlueprintLogicSpec.v2"));
		LogicSpec->SetArrayField(TEXT("statements"), Statements);
		return LogicSpec;
	}

	static TSharedRef<FJsonObject> MakePrintStringWithUnconsumedPureDataLogicSpec()
	{
		TSharedRef<FJsonObject> PrintStatement = MakeShared<FJsonObject>();
		PrintStatement->SetStringField(TEXT("kind"), TEXT("call"));
		PrintStatement->SetStringField(TEXT("target"), TEXT("PrintString"));

		TSharedRef<FJsonObject> PrintArgs = MakeShared<FJsonObject>();
		PrintArgs->SetObjectField(TEXT("InString"), MakeStringLiteralExpression(TEXT("anchor resolved body")));
		PrintStatement->SetObjectField(TEXT("args"), PrintArgs);

		TSharedRef<FJsonObject> PureStatement = MakeShared<FJsonObject>();
		PureStatement->SetStringField(TEXT("kind"), TEXT("call"));
		PureStatement->SetStringField(TEXT("target"), TEXT("/Script/Engine.KismetMathLibrary:InRange_IntInt"));
		PureStatement->SetStringField(TEXT("value_type"), TEXT("bool"));
		PureStatement->SetStringField(TEXT("result_symbol"), TEXT("UnusedBool"));

		TSharedRef<FJsonObject> PureArgs = MakeShared<FJsonObject>();
		PureArgs->SetObjectField(TEXT("Value"), MakeIntLiteralExpression(1));
		PureArgs->SetObjectField(TEXT("Min"), MakeIntLiteralExpression(0));
		PureArgs->SetObjectField(TEXT("Max"), MakeIntLiteralExpression(2));
		PureArgs->SetObjectField(TEXT("InclusiveMin"), MakeBoolLiteralExpression(true));
		PureArgs->SetObjectField(TEXT("InclusiveMax"), MakeBoolLiteralExpression(true));
		PureStatement->SetObjectField(TEXT("args"), PureArgs);

		TArray<TSharedPtr<FJsonValue>> Statements;
		Statements.Add(MakeShared<FJsonValueObject>(PrintStatement));
		Statements.Add(MakeShared<FJsonValueObject>(PureStatement));

		TSharedRef<FJsonObject> LogicSpec = MakeShared<FJsonObject>();
		LogicSpec->SetStringField(TEXT("schema"), TEXT("BlueprintLogicSpec.v2"));
		LogicSpec->SetArrayField(TEXT("statements"), Statements);
		return LogicSpec;
	}

	static TSharedRef<FJsonObject> MakeMergeExternalFlowPayloadWithAnchorObject(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const TSharedRef<FJsonObject>& AnchorJson,
		const FString& InsertStrategy,
		const TArray<FString>& SequenceOrder,
		const TSharedRef<FJsonObject>& BodySpec,
		bool bDryRun);

	static TSharedRef<FJsonObject> MakeMergeExternalFlowPayloadWithAnchorObject(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const TSharedRef<FJsonObject>& AnchorJson,
		const FString& InsertStrategy,
		const TArray<FString>& SequenceOrder)
	{
		return MakeMergeExternalFlowPayloadWithAnchorObject(
			Blueprint,
			Graph,
			AnchorJson,
			InsertStrategy,
			SequenceOrder,
			MakeEmptyLogicSpec(),
			true);
	}

	static TSharedRef<FJsonObject> MakeMergeExternalFlowPayload(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const FBlueprintHelperExternalGraphAnchor& Anchor,
		const TArray<FString>& SequenceOrder)
	{
		return MakeMergeExternalFlowPayloadWithAnchorObject(
			Blueprint,
			Graph,
			Anchor.ToJson(),
			TEXT("branch_fork"),
			SequenceOrder);
	}

	static TSharedRef<FJsonObject> MakeMergeExternalFlowPayloadWithAnchorObject(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const TSharedRef<FJsonObject>& AnchorJson,
		const FString& InsertStrategy,
		const TArray<FString>& SequenceOrder,
		const TSharedRef<FJsonObject>& BodySpec,
		bool bDryRun)
	{
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), Blueprint ? Blueprint->GetPathName() : FString());
		Target->SetStringField(TEXT("graph"), Graph ? Graph->GetName() : FString());

		TSharedRef<FJsonObject> Inserted = MakeShared<FJsonObject>();
		Inserted->SetStringField(TEXT("block_id"), TEXT("ExternalMergeTest"));
		Inserted->SetObjectField(TEXT("body"), BodySpec);

		TArray<TSharedPtr<FJsonValue>> SequenceValues;
		for (const FString& Item : SequenceOrder)
		{
			SequenceValues.Add(MakeShared<FJsonValueString>(Item));
		}

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("target"), Target);
		Payload->SetStringField(TEXT("insert_strategy"), InsertStrategy);
		Payload->SetObjectField(TEXT("anchor"), AnchorJson);
		Payload->SetObjectField(TEXT("inserted"), Inserted);
		Payload->SetArrayField(TEXT("sequence_order"), SequenceValues);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);
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

	static UK2Node_CallFunction* FindCallFunctionNode(UEdGraph* Graph, const FName FunctionName)
	{
		if (!Graph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
			{
				if (CallNode->GetFunctionName() == FunctionName)
				{
					return CallNode;
				}
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
	FBlueprintHelperLogicJsonPathService PathService;
	const FBlueprintHelperMergeExternalFlowService Service(Resolver, BlockIdService, OwnershipService, PathService);

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMergeExternalFlowExecuteAllowsAnchorResolvedBodyConnectivityTest,
	"BlueprintHelper.GraphWrite.ExternalAnchor.MergeExternalFlowExecuteAllowsAnchorResolvedBodyConnectivity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperMergeExternalFlowExecuteAllowsAnchorResolvedBodyConnectivityTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperExternalGraphAnchorTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("MergeExternalFlowAnchorResolvedConnectivity"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CustomEvent* EventNode = AddCustomEventNode(Graph, TEXT("OpenDoor"));
	UEdGraphPin* SourcePin = FindExecPin(EventNode, EGPD_Output);
	if (!Blueprint || !Graph || !EventNode || !SourcePin)
	{
		return false;
	}

	FString Error;
	FBlueprintHelperExternalGraphAnchor Anchor;
	TestTrue(TEXT("boundary anchor builds"), BuildBoundaryAnchor(Blueprint, Graph, SourcePin, Anchor, Error));

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperLogicJsonPathService PathService;
	const FBlueprintHelperMergeExternalFlowService Service(Resolver, BlockIdService, OwnershipService, PathService);

	const FBlueprintHelperToolResultBase Result = Service.Execute(MakeMergeExternalFlowPayloadWithAnchorObject(
		Blueprint,
		Graph,
		Anchor.ToJson(),
		TEXT("append_after"),
		{},
		MakePrintStringLogicSpec(TEXT("anchor resolved body")),
		false));
	TestTrue(TEXT("merge external flow execute succeeds"), Result.bOk);
	if (!Result.bOk)
	{
		AddError(FString::Printf(
			TEXT("merge_external_flow failed: %s - %s"),
			Result.Error.IsSet() ? *Result.Error->Code : TEXT("no_error_code"),
			Result.Error.IsSet() ? *Result.Error->Message : TEXT("no_error_message")));
		return false;
	}

	UK2Node_CallFunction* PrintStringNode = FindCallFunctionNode(Graph, FName(TEXT("PrintString")));
	TestNotNull(TEXT("PrintString body node generated"), PrintStringNode);
	if (!PrintStringNode)
	{
		return false;
	}

	UEdGraphPin* BodyEntryPin = FindExecPin(PrintStringNode, EGPD_Input);
	TestNotNull(TEXT("PrintString execute input"), BodyEntryPin);
	if (!BodyEntryPin)
	{
		return false;
	}

	TestTrue(TEXT("anchor exec links to inserted body entry"),
		SourcePin->LinkedTo.Contains(BodyEntryPin) && BodyEntryPin->LinkedTo.Contains(SourcePin));

	FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(EventNode->GetOutermost());
	TestTrue(TEXT("external anchor event remains user-authored"),
		MetaData.GetValue(EventNode, TEXT("BlueprintHelperOwned")).IsEmpty());
	TestTrue(TEXT("external anchor event does not receive inserted block id"),
		MetaData.GetValue(EventNode, TEXT("BlueprintHelperBlockId")).IsEmpty());

	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	TestFalse(TEXT("merged graph compiles"), Blueprint->Status == BS_Error);
	return Blueprint->Status != BS_Error;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMergeExternalFlowExecuteKeepsNonAnchorConnectivityBlockedTest,
	"BlueprintHelper.GraphWrite.ExternalAnchor.MergeExternalFlowExecuteKeepsNonAnchorConnectivityBlocked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperMergeExternalFlowExecuteKeepsNonAnchorConnectivityBlockedTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperExternalGraphAnchorTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("MergeExternalFlowNonAnchorConnectivityBlocked"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CustomEvent* EventNode = AddCustomEventNode(Graph, TEXT("OpenDoor"));
	UEdGraphPin* SourcePin = FindExecPin(EventNode, EGPD_Output);
	if (!Blueprint || !Graph || !EventNode || !SourcePin)
	{
		return false;
	}

	FString Error;
	FBlueprintHelperExternalGraphAnchor Anchor;
	TestTrue(TEXT("boundary anchor builds"), BuildBoundaryAnchor(Blueprint, Graph, SourcePin, Anchor, Error));

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperLogicJsonPathService PathService;
	const FBlueprintHelperMergeExternalFlowService Service(Resolver, BlockIdService, OwnershipService, PathService);

	const int32 NodeCountBefore = Graph->Nodes.Num();
	const FBlueprintHelperToolResultBase Result = Service.Execute(MakeMergeExternalFlowPayloadWithAnchorObject(
		Blueprint,
		Graph,
		Anchor.ToJson(),
		TEXT("append_after"),
		{},
		MakePrintStringWithUnconsumedPureDataLogicSpec(),
		false));
	TestFalse(TEXT("non-anchor connectivity issue remains blocked"), Result.bOk);
	const FString ErrorCode = Result.Error.IsSet() ? Result.Error->Code : FString();
	const FString ErrorMessage = Result.Error.IsSet() ? Result.Error->Message : FString();
	TestTrue(TEXT("blocked error is a graph connectivity generation failure"),
		ErrorCode == TEXT("node_create_failed") || ErrorCode == TEXT("graphwrite_connectivity_failed"));
	TestTrue(TEXT("blocked error message reports connectivity validation failure"),
		ErrorMessage.Contains(TEXT("GraphWrite connectivity validation failed")));
	TestEqual(TEXT("anchor links remain rolled back"), SourcePin->LinkedTo.Num(), 0);
	TestEqual(TEXT("failed body leaves no residual graph nodes"), Graph->Nodes.Num(), NodeCountBefore);
	TestNull(TEXT("failed body is removed"), FindCallFunctionNode(Graph, FName(TEXT("PrintString"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMergeExternalFlowLogicJsonSelectorResolvesNodeRefTest,
	"BlueprintHelper.GraphWrite.ExternalAnchor.MergeExternalFlowLogicJsonSelectorResolvesNodeRef",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperMergeExternalFlowLogicJsonSelectorResolvesNodeRefTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperExternalGraphAnchorTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("MergeExternalFlowLogicJsonSelector"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CallFunction* FirstCallNode = AddDestroyActorCallNode(Graph);
	UK2Node_CallFunction* SecondCallNode = AddDestroyActorCallNode(Graph);
	UEdGraphPin* SecondExecOut = FindExecPin(SecondCallNode, EGPD_Output);
	TestNotNull(TEXT("first duplicate call node exists"), FirstCallNode);
	TestNotNull(TEXT("second duplicate call node exists"), SecondCallNode);
	TestNotNull(TEXT("second duplicate call node output exec exists"), SecondExecOut);
	if (!Blueprint || !Graph || !FirstCallNode || !SecondCallNode || !SecondExecOut)
	{
		return false;
	}

	const int32 SecondNodeIndex = Graph->Nodes.Find(SecondCallNode);
	TestTrue(TEXT("second duplicate node is in graph index"), SecondNodeIndex != INDEX_NONE);
	if (SecondNodeIndex == INDEX_NONE)
	{
		return false;
	}

	TSharedRef<FJsonObject> Selector = MakeShared<FJsonObject>();
	Selector->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.LogicJsonAnchorSelector.v1"));
	Selector->SetStringField(TEXT("asset_path"), Blueprint->GetPathName());
	Selector->SetStringField(TEXT("graph"), Graph->GetName());
	Selector->SetStringField(TEXT("entry_name"), TEXT("ReloadTips"));
	Selector->SetStringField(TEXT("node_ref"), FString::Printf(TEXT("nodes[%d]"), SecondNodeIndex));
	Selector->SetStringField(TEXT("pin_ref"), SecondExecOut->PinName.ToString());

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperLogicJsonPathService PathService;
	const FBlueprintHelperMergeExternalFlowService Service(Resolver, BlockIdService, OwnershipService, PathService);

	const FBlueprintHelperToolResultBase Result = Service.Execute(MakeMergeExternalFlowPayloadWithAnchorObject(
		Blueprint,
		Graph,
		Selector,
		TEXT("append_after"),
		{}));
	TestTrue(TEXT("selector dry-run passes"), Result.bOk);
	if (!Result.bOk || !Result.Data.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* Relation = nullptr;
	TestTrue(TEXT("dry-run returns boundary relation"),
		Result.Data->TryGetObjectField(TEXT("external_boundary_relation"), Relation) && Relation && Relation->IsValid());
	if (!Relation || !Relation->IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* Anchor = nullptr;
	TestTrue(TEXT("boundary relation returns resolved anchor"),
		(*Relation)->TryGetObjectField(TEXT("anchor"), Anchor) && Anchor && Anchor->IsValid());
	if (!Anchor || !Anchor->IsValid())
	{
		return false;
	}

	FString Schema;
	FString NodeGuid;
	FString PinName;
	FString Fingerprint;
	(*Anchor)->TryGetStringField(TEXT("schema"), Schema);
	(*Anchor)->TryGetStringField(TEXT("node_guid"), NodeGuid);
	(*Anchor)->TryGetStringField(TEXT("pin_name"), PinName);
	(*Anchor)->TryGetStringField(TEXT("fingerprint"), Fingerprint);
	TestEqual(TEXT("selector resolves to ExternalGraphAnchor schema"),
		Schema,
		FString(FBlueprintHelperExternalGraphAnchor::SchemaString));
	TestEqual(TEXT("selector resolves graph index to second duplicate node"),
		NodeGuid,
		SecondCallNode->NodeGuid.ToString(EGuidFormats::Digits));
	TestEqual(TEXT("selector resolves requested pin"), PinName, SecondExecOut->PinName.ToString());
	TestFalse(TEXT("resolved anchor has fingerprint"), Fingerprint.IsEmpty());
	TestFalse(TEXT("resolved anchor does not leak node_ref"), (*Anchor)->HasField(TEXT("node_ref")));
	return true;
}

#endif
