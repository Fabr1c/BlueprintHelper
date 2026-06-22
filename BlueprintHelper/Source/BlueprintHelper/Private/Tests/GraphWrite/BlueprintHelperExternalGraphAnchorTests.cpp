#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorResolver.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorFingerprintService.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorService.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperMergeExternalFlowService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicGroupBuilder.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonPathService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintTextConverter.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "AssetRegistry/AssetRegistryModule.h"
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
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

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

	static UK2Node_CallFunction* AddCallFunctionNode(UEdGraph* Graph, UFunction* Function, const int32 NodePosX, const int32 NodePosY)
	{
		if (!Graph || !Function)
		{
			return nullptr;
		}

		UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
		Graph->AddNode(CallNode, true, false);
		CallNode->CreateNewGuid();
		CallNode->SetFromFunction(Function);
		CallNode->PostPlacedNewNode();
		CallNode->AllocateDefaultPins();
		CallNode->NodePosX = NodePosX;
		CallNode->NodePosY = NodePosY;
		return CallNode;
	}

	static UK2Node_CallFunction* AddPrintStringNode(UEdGraph* Graph, const int32 NodePosX, const int32 NodePosY)
	{
		return AddCallFunctionNode(
			Graph,
			UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString)),
			NodePosX,
			NodePosY);
	}

	static UK2Node_CallFunction* AddMakeLiteralStringNode(UEdGraph* Graph, const FString& Value, const int32 NodePosX, const int32 NodePosY)
	{
		UK2Node_CallFunction* Node = AddCallFunctionNode(
			Graph,
			UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, MakeLiteralString)),
			NodePosX,
			NodePosY);
		if (Node)
		{
			if (UEdGraphPin* ValuePin = Node->FindPin(TEXT("Value")))
			{
				ValuePin->DefaultValue = Value;
			}
		}
		return Node;
	}

	static UEdGraphPin* FindPinByName(UEdGraphNode* Node, const FString& PinName)
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

	static void ClearBlueprintHelperOwnershipMetadata(UEdGraphNode* Node)
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
	}

	static FString CompactNodeKey(const UEdGraphNode* Node)
	{
		const FString Guid = Node ? Node->NodeGuid.ToString(EGuidFormats::Digits) : FString();
		return Guid.Len() <= 8 ? Guid : Guid.Left(8);
	}

	static FString CompactPinKey(const UEdGraphPin* Pin)
	{
		FString Cleaned;
		const FString Raw = Pin ? Pin->PinName.ToString() : FString();
		Cleaned.Reserve(Raw.Len());
		for (const TCHAR Ch : Raw)
		{
			if (FChar::IsAlnum(Ch) || Ch == TCHAR('_'))
			{
				Cleaned.AppendChar(Ch);
			}
		}
		if (Cleaned.Len() > 16)
		{
			Cleaned = Cleaned.Left(16);
		}
		return Cleaned.IsEmpty() ? FString(TEXT("pin")) : Cleaned;
	}

	static FString CompactPinKindPrefix(const UEdGraphPin* Pin)
	{
		return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
			? FString(TEXT("e"))
			: FString(TEXT("d"));
	}

	static FString CompactLinkKind(const UEdGraphPin* SourcePin)
	{
		return SourcePin && SourcePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
			? FString(TEXT("exec"))
			: FString(TEXT("data"));
	}

	static TSharedRef<FJsonObject> MakeCompactAnchorJson(const FString& AnchorType, const FString& AnchorRef)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("anchor_type"), AnchorType);
		Json->SetStringField(TEXT("anchor_ref"), AnchorRef);
		return Json;
	}

	static FString BuildCompactNodeRef(const UEdGraphNode* Node)
	{
		const FBlueprintHelperExternalGraphAnchorFingerprintService FingerprintService;
		return FString::Printf(
			TEXT("xnode:v1:%s#%s"),
			*CompactNodeKey(Node),
			*FingerprintService.BuildCompactNodeFingerprint(Node));
	}

	static FString BuildCompactPinRef(const UEdGraphPin* Pin)
	{
		const UEdGraphNode* Node = Pin ? Pin->GetOwningNode() : nullptr;
		const FBlueprintHelperExternalGraphAnchorFingerprintService FingerprintService;
		return FString::Printf(
			TEXT("xpin:v1:%s:%s.%s#%s"),
			*CompactPinKindPrefix(Pin),
			*CompactNodeKey(Node),
			*CompactPinKey(Pin),
			*FingerprintService.BuildCompactPinFingerprint(Pin));
	}

	static FString BuildCompactLinkRef(const UEdGraphPin* SourcePin, const UEdGraphPin* TargetPin)
	{
		const UEdGraphNode* SourceNode = SourcePin ? SourcePin->GetOwningNode() : nullptr;
		const UEdGraphNode* TargetNode = TargetPin ? TargetPin->GetOwningNode() : nullptr;
		const FString LinkKind = CompactLinkKind(SourcePin);
		const FString KindPrefix = LinkKind == TEXT("exec") ? TEXT("e") : TEXT("d");
		const FBlueprintHelperExternalGraphAnchorFingerprintService FingerprintService;
		return FString::Printf(
			TEXT("xlink:v1:%s:%s.%s>%s.%s#%s"),
			*KindPrefix,
			*CompactNodeKey(SourceNode),
			*CompactPinKey(SourcePin),
			*CompactNodeKey(TargetNode),
			*CompactPinKey(TargetPin),
			*FingerprintService.BuildLinkFingerprint(SourcePin, TargetPin, LinkKind));
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

	static bool BuildBodyEntryAnchor(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		UEdGraphNode* Node,
		FBlueprintHelperExternalGraphAnchor& OutAnchor,
		FString& OutError)
	{
		const FBlueprintHelperExternalGraphAnchorService Service;
		return Service.BuildBodyEntryAnchor(
			Blueprint ? Blueprint->GetPathName() : TEXT(""),
			Graph ? Graph->GetName() : TEXT(""),
			Node,
			OutAnchor,
			OutError);
	}

	static UBlueprint* LoadOrCreateExternalLinkPatchFixture()
	{
		const FString PackageName = TEXT("/Game/BlueprintHelperExternalLinkPatch/BP_ExternalLinkPatchFixture");
		const FString AssetName = TEXT("BP_ExternalLinkPatchFixture");
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName);
		if (UBlueprint* ExistingBlueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath))
		{
			return ExistingBlueprint;
		}

		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			return nullptr;
		}

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*AssetName,
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperExternalGraphAnchorTests"));
		if (Blueprint)
		{
			FAssetRegistryModule::AssetCreated(Blueprint);
		}
		return Blueprint;
	}

	static void ResetGraphNodes(UBlueprint* Blueprint, UEdGraph* Graph)
	{
		if (!Blueprint || !Graph)
		{
			return;
		}

		TArray<UEdGraphNode*> Nodes = Graph->Nodes;
		for (UEdGraphNode* Node : Nodes)
		{
			if (Node)
			{
				FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
			}
		}
	}

	static bool SaveFixtureBlueprint(UBlueprint* Blueprint, FString& OutError)
	{
		if (!Blueprint)
		{
			OutError = TEXT("fixture save failed: blueprint is null.");
			return false;
		}

		UPackage* Package = Blueprint->GetOutermost();
		if (!Package)
		{
			OutError = TEXT("fixture save failed: package is null.");
			return false;
		}

		Package->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(),
			FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(Package, Blueprint, *Filename, SaveArgs))
		{
			OutError = FString::Printf(TEXT("fixture save failed: %s"), *Filename);
			return false;
		}
		Package->SetDirtyFlag(false);
		return true;
	}

	static bool PrepareExternalLinkPatchFixture(FString& OutError)
	{
		UBlueprint* Blueprint = LoadOrCreateExternalLinkPatchFixture();
		UEdGraph* Graph = GetEventGraph(Blueprint);
		if (!Blueprint || !Graph)
		{
			OutError = TEXT("fixture prepare failed: blueprint or EventGraph missing.");
			return false;
		}

		ResetGraphNodes(Blueprint, Graph);

		UK2Node_CustomEvent* EventNode = AddCustomEventNode(Graph, TEXT("ExternalLinkPatchEntry"));
		UK2Node_CallFunction* PrintNode = AddPrintStringNode(Graph, 520, 0);
		UK2Node_CallFunction* BeforeValueNode = AddMakeLiteralStringNode(Graph, TEXT("before external link patch"), 160, 120);
		UK2Node_CallFunction* AfterValueNode = AddMakeLiteralStringNode(Graph, TEXT("after external link patch"), 160, 300);
		if (!EventNode || !PrintNode || !BeforeValueNode || !AfterValueNode)
		{
			OutError = TEXT("fixture prepare failed: node creation failed.");
			return false;
		}
		ClearBlueprintHelperOwnershipMetadata(EventNode);
		ClearBlueprintHelperOwnershipMetadata(PrintNode);
		ClearBlueprintHelperOwnershipMetadata(BeforeValueNode);
		ClearBlueprintHelperOwnershipMetadata(AfterValueNode);
		EventNode->NodePosX = 0;
		EventNode->NodePosY = 0;

		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		UEdGraphPin* EventThen = FindExecPin(EventNode, EGPD_Output);
		UEdGraphPin* PrintExecute = FindExecPin(PrintNode, EGPD_Input);
		UEdGraphPin* PrintInString = FindPinByName(PrintNode, TEXT("InString"));
		UEdGraphPin* BeforeReturnValue = FindPinByName(BeforeValueNode, TEXT("ReturnValue"));
		if (!Schema || !EventThen || !PrintExecute || !PrintInString || !BeforeReturnValue)
		{
			OutError = TEXT("fixture prepare failed: required pins missing.");
			return false;
		}

		if (!Schema->TryCreateConnection(EventThen, PrintExecute))
		{
			OutError = TEXT("fixture prepare failed: exec connection failed.");
			return false;
		}
		if (!Schema->TryCreateConnection(BeforeReturnValue, PrintInString))
		{
			OutError = TEXT("fixture prepare failed: data connection failed.");
			return false;
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		return SaveFixtureBlueprint(Blueprint, OutError);
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

	static const FBlueprintHelperLogicNode* FindLogicNodeWithExternalAnchorRef(
		const FBlueprintHelperLogicJsonPayload& Payload,
		const FString& AnchorRef)
	{
		auto MatchesAnchorRef = [&AnchorRef](const FBlueprintHelperLogicNode& Node)
		{
			FString CandidateRef;
			if (Node.ExternalAnchor.IsValid()
				&& Node.ExternalAnchor->TryGetStringField(TEXT("anchor_ref"), CandidateRef)
				&& CandidateRef.Equals(AnchorRef, ESearchCase::IgnoreCase))
			{
				return true;
			}
			for (const TSharedPtr<FJsonObject>& Anchor : Node.ExternalAnchors)
			{
				if (Anchor.IsValid()
					&& Anchor->TryGetStringField(TEXT("anchor_ref"), CandidateRef)
					&& CandidateRef.Equals(AnchorRef, ESearchCase::IgnoreCase))
				{
					return true;
				}
			}
			return false;
		};

		for (const FBlueprintHelperLogicGroup& Group : Payload.Groups)
		{
			for (const FBlueprintHelperLogicNode& Node : Group.Nodes)
			{
				if (MatchesAnchorRef(Node))
				{
					return &Node;
				}
			}
		}
		for (const FBlueprintHelperLogicNode& Node : Payload.Nodes)
		{
			if (MatchesAnchorRef(Node))
			{
				return &Node;
			}
		}
		return nullptr;
	}

	static const FBlueprintHelperLogicNode* FindLogicNodeWithExternalNodeGuid(
		const FBlueprintHelperLogicJsonPayload& Payload,
		const FString& NodeGuid)
	{
		auto MatchesNodeGuid = [&NodeGuid](const FBlueprintHelperLogicNode& Node)
		{
			FString CandidateGuid;
			if (Node.ExternalAnchor.IsValid()
				&& Node.ExternalAnchor->TryGetStringField(TEXT("node_guid"), CandidateGuid)
				&& CandidateGuid.Equals(NodeGuid, ESearchCase::IgnoreCase))
			{
				return true;
			}
			for (const TSharedPtr<FJsonObject>& Anchor : Node.ExternalAnchors)
			{
				if (Anchor.IsValid()
					&& Anchor->TryGetStringField(TEXT("node_guid"), CandidateGuid)
					&& CandidateGuid.Equals(NodeGuid, ESearchCase::IgnoreCase))
				{
					return true;
				}
			}
			return false;
		};

		for (const FBlueprintHelperLogicGroup& Group : Payload.Groups)
		{
			for (const FBlueprintHelperLogicNode& Node : Group.Nodes)
			{
				if (MatchesNodeGuid(Node))
				{
					return &Node;
				}
			}
		}
		for (const FBlueprintHelperLogicNode& Node : Payload.Nodes)
		{
			if (MatchesNodeGuid(Node))
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

	static bool HasExportedPinForLiteralStringNode(
		const TSharedPtr<FJsonObject>& Root,
		const FString& LiteralValue,
		const FString& PinName,
		const FString& Direction,
		const bool bConnected)
	{
		const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
		if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("nodes"), Nodes) || !Nodes)
		{
			return false;
		}

		for (const TSharedPtr<FJsonValue>& NodeValue : *Nodes)
		{
			const TSharedPtr<FJsonObject>* NodeObj = nullptr;
			if (!NodeValue.IsValid()
				|| !NodeValue->TryGetObject(NodeObj)
				|| !NodeObj
				|| !NodeObj->IsValid())
			{
				continue;
			}

			FString FunctionName;
			if (!(*NodeObj)->TryGetStringField(TEXT("function_name"), FunctionName)
				|| !FunctionName.Equals(TEXT("MakeLiteralString"), ESearchCase::IgnoreCase))
			{
				continue;
			}

			const TSharedPtr<FJsonObject>* InputDefaults = nullptr;
			const TSharedPtr<FJsonObject>* ValueDefault = nullptr;
			FString ValueText;
			if (!(*NodeObj)->TryGetObjectField(TEXT("input_defaults"), InputDefaults)
				|| !InputDefaults
				|| !InputDefaults->IsValid()
				|| !(*InputDefaults)->TryGetObjectField(TEXT("Value"), ValueDefault)
				|| !ValueDefault
				|| !ValueDefault->IsValid()
				|| !(*ValueDefault)->TryGetStringField(TEXT("value"), ValueText)
				|| !ValueText.Equals(LiteralValue, ESearchCase::CaseSensitive))
			{
				continue;
			}

			const TArray<TSharedPtr<FJsonValue>>* Pins = nullptr;
			if (!(*NodeObj)->TryGetArrayField(TEXT("pins"), Pins) || !Pins)
			{
				return false;
			}

			for (const TSharedPtr<FJsonValue>& PinValue : *Pins)
			{
				const TSharedPtr<FJsonObject>* PinObj = nullptr;
				if (!PinValue.IsValid()
					|| !PinValue->TryGetObject(PinObj)
					|| !PinObj
					|| !PinObj->IsValid())
				{
					continue;
				}

				FString ActualPinName;
				FString ActualDirection;
				bool bActualConnected = true;
				if ((*PinObj)->TryGetStringField(TEXT("pin_ref"), ActualPinName)
					&& (*PinObj)->TryGetStringField(TEXT("direction"), ActualDirection)
					&& (*PinObj)->TryGetBoolField(TEXT("connected"), bActualConnected)
					&& ActualPinName.Equals(PinName, ESearchCase::IgnoreCase)
					&& ActualDirection.Equals(Direction, ESearchCase::IgnoreCase)
					&& bActualConnected == bConnected)
				{
					return true;
				}
			}
		}

		return false;
	}

	static bool HasExportedCompactLink(
		const TSharedPtr<FJsonObject>& Root,
		const FString& LinkKind,
		const FString& FromPin,
		const FString& ToPin)
	{
		const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
		if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("links"), Links) || !Links)
		{
			return false;
		}

		for (const TSharedPtr<FJsonValue>& LinkValue : *Links)
		{
			const TSharedPtr<FJsonObject>* LinkObj = nullptr;
			if (!LinkValue.IsValid()
				|| !LinkValue->TryGetObject(LinkObj)
				|| !LinkObj
				|| !LinkObj->IsValid())
			{
				continue;
			}

			FString ActualKind;
			FString ActualFromPin;
			FString ActualToPin;
			FString AnchorType;
			FString AnchorRef;
			if ((*LinkObj)->TryGetStringField(TEXT("kind"), ActualKind)
				&& (*LinkObj)->TryGetStringField(TEXT("from_pin"), ActualFromPin)
				&& (*LinkObj)->TryGetStringField(TEXT("to_pin"), ActualToPin)
				&& (*LinkObj)->TryGetStringField(TEXT("anchor_type"), AnchorType)
				&& (*LinkObj)->TryGetStringField(TEXT("anchor_ref"), AnchorRef)
				&& ActualKind.Equals(LinkKind, ESearchCase::IgnoreCase)
				&& ActualFromPin.Equals(FromPin, ESearchCase::IgnoreCase)
				&& ActualToPin.Equals(ToPin, ESearchCase::IgnoreCase)
				&& AnchorType.Equals(TEXT("external_link"), ESearchCase::IgnoreCase)
				&& AnchorRef.StartsWith(LinkKind.Equals(TEXT("exec"), ESearchCase::IgnoreCase)
					? TEXT("xlink:v1:e:")
					: TEXT("xlink:v1:d:")))
			{
				return true;
			}
		}
		return false;
	}

	static bool LogicJsonHasCompactLink(
		const FBlueprintHelperLogicJsonPayload& Payload,
		const FString& LinkKind,
		const FString& FromPin,
		const FString& ToPin)
	{
		auto NodeHasLink = [&LinkKind, &FromPin, &ToPin](const FBlueprintHelperLogicNode& Node)
		{
			for (const FBlueprintHelperLogicLink& Link : Node.Links)
			{
				if (Link.AnchorType.Equals(TEXT("external_link"), ESearchCase::IgnoreCase)
					&& Link.AnchorRef.StartsWith(LinkKind.Equals(TEXT("exec"), ESearchCase::IgnoreCase)
						? TEXT("xlink:v1:e:")
						: TEXT("xlink:v1:d:"))
					&& Link.AnchorKind.Equals(LinkKind, ESearchCase::IgnoreCase)
					&& Link.FromPin.Equals(FromPin, ESearchCase::IgnoreCase)
					&& Link.ToPin.Equals(ToPin, ESearchCase::IgnoreCase))
				{
					return true;
				}
			}
			return false;
		};

		for (const FBlueprintHelperLogicGroup& Group : Payload.Groups)
		{
			for (const FBlueprintHelperLogicNode& Node : Group.Nodes)
			{
				if (NodeHasLink(Node))
				{
					return true;
				}
			}
		}
		for (const FBlueprintHelperLogicNode& Node : Payload.Nodes)
		{
			if (NodeHasLink(Node))
			{
				return true;
			}
		}
		return false;
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
	FBlueprintHelperExternalCompactAnchorPinAndLinkRoundTripTest,
	"BlueprintHelper.GraphWrite.ExternalAnchor.CompactPinAndLinkRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperExternalCompactAnchorPinAndLinkRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperExternalGraphAnchorTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("CompactPinAndLinkRoundTrip"));
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
	FBlueprintHelperExternalCompactAnchor CompactPin;
	TestTrue(TEXT("compact pin parses"),
		FBlueprintHelperExternalCompactAnchor::FromJson(
			MakeCompactAnchorJson(TEXT("external_pin"), BuildCompactPinRef(SourcePin)),
			CompactPin,
			Error));
	TestEqual(TEXT("compact pin type"),
		static_cast<int32>(CompactPin.Type),
		static_cast<int32>(EBlueprintHelperExternalCompactAnchorType::Pin));
	TestEqual(TEXT("compact pin node key"), CompactPin.NodeKey, CompactNodeKey(SourcePin->GetOwningNode()));
	TestEqual(TEXT("compact pin pin key"), CompactPin.PinKey, CompactPinKey(SourcePin));

	UEdGraphPin* ResolvedPin = nullptr;
	const FBlueprintHelperExternalGraphAnchorResolver Resolver;
	TestTrue(TEXT("compact pin resolves"),
		Resolver.ResolveCompactPin(Blueprint->GetPathName(), Graph->GetName(), CompactPin, ResolvedPin, Error));
	TestTrue(TEXT("resolved compact pin"), ResolvedPin == SourcePin);

	FBlueprintHelperExternalCompactAnchor StalePin = CompactPin;
	StalePin.Fingerprint = TEXT("stale");
	TestFalse(TEXT("stale compact pin is rejected"),
		Resolver.ResolveCompactPin(Blueprint->GetPathName(), Graph->GetName(), StalePin, ResolvedPin, Error));
	TestEqual(TEXT("stale compact pin error"), Error, FString(TEXT("external_anchor_stale")));

	FBlueprintHelperExternalCompactAnchor CompactLink;
	TestTrue(TEXT("compact link parses"),
		FBlueprintHelperExternalCompactAnchor::FromJson(
			MakeCompactAnchorJson(TEXT("external_link"), BuildCompactLinkRef(SourcePin, TargetPin)),
			CompactLink,
			Error));
	TestEqual(TEXT("compact link type"),
		static_cast<int32>(CompactLink.Type),
		static_cast<int32>(EBlueprintHelperExternalCompactAnchorType::Link));
	TestEqual(TEXT("compact link source node key"),
		CompactLink.SourceNodeKey,
		CompactNodeKey(SourcePin->GetOwningNode()));
	TestEqual(TEXT("compact link target pin key"),
		CompactLink.TargetPinKey,
		CompactPinKey(TargetPin));

	FBlueprintHelperExternalGraphLinkResolution ResolvedLink;
	TestTrue(TEXT("compact link resolves"),
		Resolver.ResolveCompactLink(Blueprint->GetPathName(), Graph->GetName(), CompactLink, ResolvedLink, Error));
	TestTrue(TEXT("resolved link source pin"), ResolvedLink.SourcePin == SourcePin);
	TestTrue(TEXT("resolved link target pin"), ResolvedLink.TargetPin == TargetPin);
	TestEqual(TEXT("resolved link kind"), ResolvedLink.LinkKind, FString(TEXT("exec")));

	FBlueprintHelperExternalCompactAnchor StaleLink = CompactLink;
	StaleLink.Fingerprint = TEXT("stale");
	TestFalse(TEXT("stale compact link is rejected"),
		Resolver.ResolveCompactLink(Blueprint->GetPathName(), Graph->GetName(), StaleLink, ResolvedLink, Error));
	TestEqual(TEXT("stale compact link error"), Error, FString(TEXT("external_anchor_stale")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExternalGraphAnchorPrepareExternalLinkPatchFixtureTest,
	"BlueprintHelper.GraphWrite.ExternalAnchor.E2EFixture.PrepareExternalLinkPatchFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperExternalGraphAnchorPrepareExternalLinkPatchFixtureTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperExternalGraphAnchorTests;

	FString Error;
	TestTrue(TEXT("external link patch fixture is prepared"), PrepareExternalLinkPatchFixture(Error));
	if (!Error.IsEmpty())
	{
		AddInfo(Error);
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(
		nullptr,
		TEXT("/Game/BlueprintHelperExternalLinkPatch/BP_ExternalLinkPatchFixture.BP_ExternalLinkPatchFixture"));
	TestNotNull(TEXT("fixture blueprint can be loaded"), Blueprint);
	UEdGraph* Graph = GetEventGraph(Blueprint);
	TestNotNull(TEXT("fixture EventGraph exists"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> ExportedGraph = FBlueprintToTextConverter::ConvertGraphToJsonObject(Graph);
	TestTrue(
		TEXT("fixture readback exposes unlinked replacement output pin"),
		HasExportedPinForLiteralStringNode(
			ExportedGraph,
			TEXT("after external link patch"),
			TEXT("ReturnValue"),
			TEXT("output"),
			false));
	TestTrue(
		TEXT("fixture raw export exposes compact data external_link"),
		HasExportedCompactLink(ExportedGraph, TEXT("data"), TEXT("ReturnValue"), TEXT("InString")));
	TestTrue(
		TEXT("fixture raw export exposes compact exec external_link"),
		HasExportedCompactLink(ExportedGraph, TEXT("exec"), TEXT("then"), TEXT("execute")));

	FBlueprintHelperLogicGroupBuilder Builder;
	const FBlueprintHelperLogicJsonPayload Payload = Builder.BuildGroups(
		ExportedGraph,
		Blueprint->GetPathName(),
		Graph->GetName(),
		EBlueprintHelperLogicScope::TargetGraph);
	TestTrue(
		TEXT("fixture LogicJson preserves compact data external_link"),
		LogicJsonHasCompactLink(Payload, TEXT("data"), TEXT("ReturnValue"), TEXT("InString")));
	TestTrue(
		TEXT("fixture LogicJson preserves compact exec external_link"),
		LogicJsonHasCompactLink(Payload, TEXT("exec"), TEXT("then"), TEXT("execute")));
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

	EventNode->NodeComment = TEXT("Designer node note");

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
	TestEqual(TEXT("raw export projects node_comment"),
		(*ExportedNode)->GetStringField(TEXT("node_comment")),
		FString(TEXT("Designer node note")));
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

	const FString EventNodeGuid = EventNode->NodeGuid.ToString(EGuidFormats::Digits);
	const FBlueprintHelperLogicNode* LogicNode = FindLogicNodeWithExternalNodeGuid(Payload, EventNodeGuid);
	TestNotNull(TEXT("logic node with event external anchor"), LogicNode);
	if (!LogicNode)
	{
		return false;
	}

	const TSharedRef<FJsonObject> LogicNodeJson = LogicNode->ToJson();
	TestTrue(TEXT("LogicJson preserves external_anchor"), LogicNodeJson->HasField(TEXT("external_anchor")));
	const TSharedPtr<FJsonObject>* LogicNodeExternalAnchor = nullptr;
	TestTrue(TEXT("LogicJson node external anchor is object"),
		LogicNodeJson->TryGetObjectField(TEXT("external_anchor"), LogicNodeExternalAnchor) &&
		LogicNodeExternalAnchor &&
		LogicNodeExternalAnchor->IsValid());
	if (LogicNodeExternalAnchor && LogicNodeExternalAnchor->IsValid())
	{
		TestEqual(TEXT("LogicJson external anchor matches event node"),
			(*LogicNodeExternalAnchor)->GetStringField(TEXT("node_guid")),
			EventNodeGuid);
	}
	TestEqual(TEXT("LogicJson projects node_comment"),
		LogicNodeJson->GetStringField(TEXT("node_comment")),
		FString(TEXT("Designer node note")));
	TestFalse(TEXT("LogicJson does not synthesize block_id"), Payload.Groups[0].ToJson()->HasField(TEXT("block_id")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExternalGraphAnchorBodyEntryAllowsOwnedEntryTest,
	"BlueprintHelper.GraphWrite.ExternalAnchor.BodyEntryAllowsOwnedEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperExternalGraphAnchorBodyEntryAllowsOwnedEntryTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperExternalGraphAnchorTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("BodyEntryAllowsOwnedEntry"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CustomEvent* EventNode = AddCustomEventNode(Graph, TEXT("BH_BodyEntry"));
	if (!Blueprint || !Graph || !EventNode)
	{
		return false;
	}

	FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(EventNode->GetOutermost());
	MetaData.SetValue(EventNode, TEXT("BlueprintHelperOwned"), TEXT("true"));

	FString Error;
	FBlueprintHelperExternalGraphAnchor NodeAnchor;
	TestFalse(TEXT("ordinary node anchor rejects owned nodes"),
		BuildNodeAnchor(Blueprint, Graph, EventNode, NodeAnchor, Error));
	TestEqual(TEXT("ordinary node anchor rejection code"),
		Error,
		FString(TEXT("external_anchor_owned_node_not_supported")));

	Error.Reset();
	FBlueprintHelperExternalGraphAnchor BodyEntryAnchor;
	TestTrue(TEXT("body entry anchor allows owned entry nodes"),
		BuildBodyEntryAnchor(Blueprint, Graph, EventNode, BodyEntryAnchor, Error));
	TestEqual(TEXT("body entry semantic role"),
		FBlueprintHelperExternalGraphAnchor::RoleToString(BodyEntryAnchor.SemanticRole),
		FString(TEXT("body_entry")));
	TestEqual(TEXT("body entry node guid"),
		BodyEntryAnchor.NodeGuid,
		EventNode->NodeGuid.ToString(EGuidFormats::Digits));
	TestFalse(TEXT("body entry fingerprint exists"), BodyEntryAnchor.Fingerprint.IsEmpty());
	TestEqual(TEXT("body entry stable name"),
		BodyEntryAnchor.StableName,
		FString(TEXT("BH_BodyEntry")));
	TestEqual(TEXT("body entry kind"),
		BodyEntryAnchor.EntryKind,
		FString(TEXT("custom_event")));
	TestEqual(TEXT("body entry member name"),
		BodyEntryAnchor.MemberName,
		FString(TEXT("BH_BodyEntry")));
	TestEqual(TEXT("body entry function name"),
		BodyEntryAnchor.FunctionName,
		FString(TEXT("BH_BodyEntry")));
	TestFalse(TEXT("body entry display name exists"), BodyEntryAnchor.DisplayName.IsEmpty());

	const TSharedRef<FJsonObject> BodyEntryJson = BodyEntryAnchor.ToJson();
	FString StableName;
	FString EntryKind;
	FString MemberName;
	FString FunctionName;
	FString DisplayName;
	TestTrue(TEXT("body entry json has stable_name"),
		BodyEntryJson->TryGetStringField(TEXT("stable_name"), StableName));
	TestTrue(TEXT("body entry json has entry_kind"),
		BodyEntryJson->TryGetStringField(TEXT("entry_kind"), EntryKind));
	TestTrue(TEXT("body entry json has member_name"),
		BodyEntryJson->TryGetStringField(TEXT("member_name"), MemberName));
	TestTrue(TEXT("body entry json has function_name"),
		BodyEntryJson->TryGetStringField(TEXT("function_name"), FunctionName));
	TestTrue(TEXT("body entry json has display_name"),
		BodyEntryJson->TryGetStringField(TEXT("display_name"), DisplayName));
	TestEqual(TEXT("body entry json stable name"), StableName, BodyEntryAnchor.StableName);
	TestEqual(TEXT("body entry json entry kind"), EntryKind, BodyEntryAnchor.EntryKind);
	TestEqual(TEXT("body entry json member name"), MemberName, BodyEntryAnchor.MemberName);
	TestEqual(TEXT("body entry json function name"), FunctionName, BodyEntryAnchor.FunctionName);
	TestEqual(TEXT("body entry json display name"), DisplayName, BodyEntryAnchor.DisplayName);

	FBlueprintHelperExternalGraphAnchor RoundTripAnchor;
	FString RoundTripError;
	TestTrue(TEXT("body entry anchor json round trips"),
		FBlueprintHelperExternalGraphAnchor::FromJson(BodyEntryJson, RoundTripAnchor, RoundTripError));
	TestEqual(TEXT("round-trip stable name"), RoundTripAnchor.StableName, BodyEntryAnchor.StableName);
	TestEqual(TEXT("round-trip entry kind"), RoundTripAnchor.EntryKind, BodyEntryAnchor.EntryKind);
	TestEqual(TEXT("round-trip member name"), RoundTripAnchor.MemberName, BodyEntryAnchor.MemberName);
	TestEqual(TEXT("round-trip function name"), RoundTripAnchor.FunctionName, BodyEntryAnchor.FunctionName);
	TestEqual(TEXT("round-trip display name"), RoundTripAnchor.DisplayName, BodyEntryAnchor.DisplayName);
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
	FBlueprintHelperMergeExternalFlowInsertBetweenResolvesExternalLinkAnchorTest,
	"BlueprintHelper.GraphWrite.ExternalAnchor.MergeExternalFlowInsertBetweenResolvesExternalLinkAnchor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperMergeExternalFlowInsertBetweenResolvesExternalLinkAnchorTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperExternalGraphAnchorTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("MergeExternalFlowExternalLinkAnchor"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CustomEvent* EventNode = AddCustomEventNode(Graph, TEXT("OpenDoorWithLinkAnchor"));
	UK2Node_CallFunction* OriginalNode = AddDestroyActorCallNode(Graph);
	UEdGraphPin* SourcePin = FindExecPin(EventNode, EGPD_Output);
	UEdGraphPin* OriginalInputPin = FindExecPin(OriginalNode, EGPD_Input);
	TestTrue(TEXT("initial exec link exists"), ConnectExecPins(SourcePin, OriginalInputPin));
	if (!Blueprint || !Graph || !EventNode || !OriginalNode || !SourcePin || !OriginalInputPin)
	{
		return false;
	}

	const FString LinkRef = BuildCompactLinkRef(SourcePin, OriginalInputPin);
	TSharedRef<FJsonObject> LinkAnchor = MakeCompactAnchorJson(TEXT("external_link"), LinkRef);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperLogicJsonPathService PathService;
	const FBlueprintHelperMergeExternalFlowService Service(Resolver, BlockIdService, OwnershipService, PathService);

	const FBlueprintHelperToolResultBase Result = Service.Execute(MakeMergeExternalFlowPayloadWithAnchorObject(
		Blueprint,
		Graph,
		LinkAnchor,
		TEXT("insert_between"),
		{},
		MakePrintStringLogicSpec(TEXT("inserted via link anchor")),
		false));
	TestTrue(TEXT("merge external flow insert_between succeeds"), Result.bOk);
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
	UEdGraphPin* BodyExitPin = FindExecPin(PrintStringNode, EGPD_Output);
	TestNotNull(TEXT("PrintString execute input"), BodyEntryPin);
	TestNotNull(TEXT("PrintString then output"), BodyExitPin);
	if (!BodyEntryPin || !BodyExitPin)
	{
		return false;
	}

	TestFalse(TEXT("original direct exec link removed"), SourcePin->LinkedTo.Contains(OriginalInputPin));
	TestTrue(TEXT("anchor exec links to inserted body entry"),
		SourcePin->LinkedTo.Contains(BodyEntryPin) && BodyEntryPin->LinkedTo.Contains(SourcePin));
	TestTrue(TEXT("inserted body exit links to original successor"),
		BodyExitPin->LinkedTo.Contains(OriginalInputPin) && OriginalInputPin->LinkedTo.Contains(BodyExitPin));

	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	TestFalse(TEXT("insert_between graph compiles"), Blueprint->Status == BS_Error);
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
