#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorService.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorFingerprintService.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperPatchExternalGraphService.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BlueprintHelperPatchExternalGraphTests
{
	static FString MakeTestObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static UBlueprint* MakeBlueprint(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperExternalPatch/%s"),
			*MakeTestObjectName(Prefix)));
		Package->SetDirtyFlag(false);

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*MakeTestObjectName(TEXT("BP_ExternalPatch")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperPatchExternalGraphTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static UEdGraph* GetEventGraph(UBlueprint* Blueprint)
	{
		return Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
	}

	static UK2Node_CallFunction* AddPrintStringNode(UEdGraph* Graph)
	{
		if (!Graph)
		{
			return nullptr;
		}

		UFunction* Function = UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString));
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

	static bool BuildNodeAnchor(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		UEdGraphNode* Node,
		FBlueprintHelperExternalGraphAnchor& OutAnchor,
		FString& OutError)
	{
		const FBlueprintHelperExternalGraphAnchorService Service;
		return Service.BuildNodeAnchor(
			Blueprint ? Blueprint->GetPathName() : FString(),
			Graph ? Graph->GetName() : FString(),
			Node,
			OutAnchor,
			OutError);
	}

	static FString CompactNodeKey(const UEdGraphNode* Node)
	{
		const FString Guid = Node ? Node->NodeGuid.ToString(EGuidFormats::Digits) : FString();
		return Guid.Len() <= 8 ? Guid : Guid.Left(8);
	}

	static FString BuildCompactNodeRef(const UEdGraphNode* Node)
	{
		const FBlueprintHelperExternalGraphAnchorFingerprintService FingerprintService;
		return FString::Printf(
			TEXT("xnode:v1:%s#%s"),
			*CompactNodeKey(Node),
			*FingerprintService.BuildCompactNodeFingerprint(Node));
	}

	static TSharedRef<FJsonObject> MakeCompactNodeAnchorJson(const UEdGraphNode* Node)
	{
		TSharedRef<FJsonObject> Anchor = MakeShared<FJsonObject>();
		Anchor->SetStringField(TEXT("anchor_type"), TEXT("external_node"));
		Anchor->SetStringField(TEXT("anchor_ref"), BuildCompactNodeRef(Node));
		return Anchor;
	}

	static TSharedRef<FJsonObject> MakePatchPayload(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const FString& PatchType,
		const FBlueprintHelperExternalGraphAnchor& Anchor,
		const FString& Value,
		const FString& ExpectedOldValue,
		bool bDryRun,
		const FString& PropertyDescriptorId = FString())
	{
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), Blueprint ? Blueprint->GetPathName() : FString());
		Target->SetStringField(TEXT("graph"), Graph ? Graph->GetName() : FString());

		TSharedRef<FJsonObject> ExpectedOldState = MakeShared<FJsonObject>();
		ExpectedOldState->SetStringField(TEXT("value"), ExpectedOldValue);

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("target"), Target);
		Payload->SetStringField(TEXT("patch_type"), PatchType);
		Payload->SetObjectField(TEXT("anchor"), Anchor.ToJson());
		if (!PropertyDescriptorId.IsEmpty())
		{
			Payload->SetStringField(TEXT("property_descriptor_id"), PropertyDescriptorId);
		}
		Payload->SetStringField(TEXT("value"), Value);
		Payload->SetObjectField(TEXT("expected_old_state"), ExpectedOldState);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeCompactPatchPayload(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const FString& PatchType,
		UEdGraphNode* Node,
		const FString& Value,
		const FString& ExpectedOldValue,
		bool bDryRun,
		const FString& PropertyDescriptorId = FString())
	{
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), Blueprint ? Blueprint->GetPathName() : FString());
		Target->SetStringField(TEXT("graph"), Graph ? Graph->GetName() : FString());

		TSharedRef<FJsonObject> ExpectedOldState = MakeShared<FJsonObject>();
		ExpectedOldState->SetStringField(TEXT("value"), ExpectedOldValue);

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("target"), Target);
		Payload->SetStringField(TEXT("patch_type"), PatchType);
		Payload->SetObjectField(TEXT("anchor"), MakeCompactNodeAnchorJson(Node));
		if (!PropertyDescriptorId.IsEmpty())
		{
			Payload->SetStringField(TEXT("property_descriptor_id"), PropertyDescriptorId);
		}
		Payload->SetStringField(TEXT("value"), Value);
		Payload->SetObjectField(TEXT("expected_old_state"), ExpectedOldState);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);
		return Payload;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExternalPatchPinDefaultPreviewTest,
	"BlueprintHelper.GraphWrite.ExternalPatch.PinDefaultPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperExternalPatchPinDefaultPreviewTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperPatchExternalGraphTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("PinDefaultPreview"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CallFunction* Node = AddPrintStringNode(Graph);
	UEdGraphPin* InStringPin = FindPinByName(Node, TEXT("InString"));
	TestNotNull(TEXT("pin exists"), InStringPin);
	if (!Blueprint || !Graph || !Node || !InStringPin)
	{
		return false;
	}

	InStringPin->DefaultValue = TEXT("before");

	FString Error;
	FBlueprintHelperExternalGraphAnchor Anchor;
	TestTrue(TEXT("node anchor builds"), BuildNodeAnchor(Blueprint, Graph, Node, Anchor, Error));
	Anchor.PinName = InStringPin->PinName.ToString();
	Anchor.PinDirection = TEXT("input");

	const FBlueprintHelperPatchExternalGraphService Service;
	const FBlueprintHelperToolResultBase Result = Service.Execute(MakePatchPayload(
		Blueprint,
		Graph,
		TEXT("set_external_pin_default"),
		Anchor,
		TEXT("after"),
		TEXT("before"),
		true));

	TestTrue(TEXT("preview ok"), Result.bOk);
	TestEqual(TEXT("pin default unchanged"), InStringPin->DefaultValue, FString(TEXT("before")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExternalPatchPinDefaultExecuteTest,
	"BlueprintHelper.GraphWrite.ExternalPatch.PinDefaultExecute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperExternalPatchPinDefaultExecuteTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperPatchExternalGraphTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("PinDefaultExecute"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CallFunction* Node = AddPrintStringNode(Graph);
	UEdGraphPin* InStringPin = FindPinByName(Node, TEXT("InString"));
	if (!Blueprint || !Graph || !Node || !InStringPin)
	{
		return false;
	}

	InStringPin->DefaultValue = TEXT("before");

	FString Error;
	FBlueprintHelperExternalGraphAnchor Anchor;
	TestTrue(TEXT("node anchor builds"), BuildNodeAnchor(Blueprint, Graph, Node, Anchor, Error));
	Anchor.PinName = InStringPin->PinName.ToString();
	Anchor.PinDirection = TEXT("input");

	const FBlueprintHelperPatchExternalGraphService Service;
	const FBlueprintHelperToolResultBase Result = Service.Execute(MakePatchPayload(
		Blueprint,
		Graph,
		TEXT("set_external_pin_default"),
		Anchor,
		TEXT("after"),
		TEXT("before"),
		false));

	TestTrue(TEXT("execute ok"), Result.bOk);
	TestEqual(TEXT("pin default changed"), InStringPin->DefaultValue, FString(TEXT("after")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExternalPatchNodeCommentExecuteTest,
	"BlueprintHelper.GraphWrite.ExternalPatch.NodeCommentExecute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperExternalPatchNodeCommentExecuteTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperPatchExternalGraphTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("NodeCommentExecute"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CallFunction* Node = AddPrintStringNode(Graph);
	if (!Blueprint || !Graph || !Node)
	{
		return false;
	}

	Node->NodeComment = TEXT("before");

	FString Error;
	FBlueprintHelperExternalGraphAnchor Anchor;
	TestTrue(TEXT("node anchor builds"), BuildNodeAnchor(Blueprint, Graph, Node, Anchor, Error));

	const FBlueprintHelperPatchExternalGraphService Service;
	const FBlueprintHelperToolResultBase Result = Service.Execute(MakePatchPayload(
		Blueprint,
		Graph,
		TEXT("set_external_node_comment"),
		Anchor,
		TEXT("after"),
		TEXT("before"),
		false));

	TestTrue(TEXT("execute ok"), Result.bOk);
	TestEqual(TEXT("comment changed"), Node->NodeComment, FString(TEXT("after")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExternalPatchNodePropertyDescriptorExecuteTest,
	"BlueprintHelper.GraphWrite.ExternalPatch.NodePropertyDescriptorExecute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperExternalPatchNodePropertyDescriptorExecuteTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperPatchExternalGraphTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("NodePropertyDescriptorExecute"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CallFunction* Node = AddPrintStringNode(Graph);
	if (!Blueprint || !Graph || !Node)
	{
		return false;
	}

	Node->NodeComment = TEXT("before");

	FString Error;
	FBlueprintHelperExternalGraphAnchor Anchor;
	TestTrue(TEXT("node anchor builds"), BuildNodeAnchor(Blueprint, Graph, Node, Anchor, Error));

	const FBlueprintHelperPatchExternalGraphService Service;
	const FBlueprintHelperToolResultBase Result = Service.Execute(MakePatchPayload(
		Blueprint,
		Graph,
		TEXT("set_external_node_property"),
		Anchor,
		TEXT("after descriptor"),
		TEXT("before"),
		false,
		TEXT("k2.node.comment")));

	TestTrue(TEXT("execute ok"), Result.bOk);
	TestEqual(TEXT("descriptor-backed comment changed"), Node->NodeComment, FString(TEXT("after descriptor")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExternalPatchNodePropertyDescriptorCompactAnchorExecuteTest,
	"BlueprintHelper.GraphWrite.ExternalPatch.NodePropertyDescriptorCompactAnchorExecute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperExternalPatchNodePropertyDescriptorCompactAnchorExecuteTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperPatchExternalGraphTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("NodePropertyDescriptorCompactAnchorExecute"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CallFunction* Node = AddPrintStringNode(Graph);
	if (!Blueprint || !Graph || !Node)
	{
		return false;
	}

	Node->NodeComment = TEXT("before");

	const FBlueprintHelperPatchExternalGraphService Service;
	const FBlueprintHelperToolResultBase Result = Service.Execute(MakeCompactPatchPayload(
		Blueprint,
		Graph,
		TEXT("set_external_node_property"),
		Node,
		TEXT("after compact descriptor"),
		TEXT("before"),
		false,
		TEXT("k2.node.comment")));

	TestTrue(TEXT("execute ok"), Result.bOk);
	TestEqual(TEXT("descriptor-backed comment changed"), Node->NodeComment, FString(TEXT("after compact descriptor")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExternalPatchRejectsReservedNodePropertyDescriptorTest,
	"BlueprintHelper.GraphWrite.ExternalPatch.RejectsReservedNodePropertyDescriptor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperExternalPatchRejectsReservedNodePropertyDescriptorTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperPatchExternalGraphTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("RejectsReservedNodePropertyDescriptor"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CallFunction* Node = AddPrintStringNode(Graph);
	if (!Blueprint || !Graph || !Node)
	{
		return false;
	}

	FString Error;
	FBlueprintHelperExternalGraphAnchor Anchor;
	TestTrue(TEXT("node anchor builds"), BuildNodeAnchor(Blueprint, Graph, Node, Anchor, Error));

	const FBlueprintHelperPatchExternalGraphService Service;
	const FBlueprintHelperToolResultBase Result = Service.Execute(MakePatchPayload(
		Blueprint,
		Graph,
		TEXT("set_external_node_property"),
		Anchor,
		TEXT("reserved"),
		Node->NodeComment,
		false,
		TEXT("k2.call.function_target")));

	TestFalse(TEXT("reserved descriptor rejected"), Result.bOk);
	TestEqual(TEXT("reserved descriptor code"),
		Result.Error.IsSet() ? Result.Error->Code : FString(),
		FString(TEXT("external_property_descriptor_not_allowed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExternalPatchRejectsStaleAnchorTest,
	"BlueprintHelper.GraphWrite.ExternalPatch.RejectsStaleAnchor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperExternalPatchRejectsStaleAnchorTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperPatchExternalGraphTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("RejectsStaleAnchor"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CallFunction* Node = AddPrintStringNode(Graph);
	if (!Blueprint || !Graph || !Node)
	{
		return false;
	}

	FString Error;
	FBlueprintHelperExternalGraphAnchor Anchor;
	TestTrue(TEXT("node anchor builds"), BuildNodeAnchor(Blueprint, Graph, Node, Anchor, Error));
	Anchor.Fingerprint = TEXT("stale");

	const FBlueprintHelperPatchExternalGraphService Service;
	const FBlueprintHelperToolResultBase Result = Service.Execute(MakePatchPayload(
		Blueprint,
		Graph,
		TEXT("set_external_node_comment"),
		Anchor,
		TEXT("after"),
		Node->NodeComment,
		false));

	TestFalse(TEXT("stale anchor rejected"), Result.bOk);
	TestEqual(TEXT("stale anchor code"),
		Result.Error.IsSet() ? Result.Error->Code : FString(),
		FString(TEXT("external_anchor_fingerprint_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExternalPatchRejectsExpectedOldStateMismatchTest,
	"BlueprintHelper.GraphWrite.ExternalPatch.RejectsExpectedOldStateMismatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperExternalPatchRejectsExpectedOldStateMismatchTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperPatchExternalGraphTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("RejectsExpectedOldStateMismatch"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CallFunction* Node = AddPrintStringNode(Graph);
	if (!Blueprint || !Graph || !Node)
	{
		return false;
	}

	Node->NodeComment = TEXT("current");

	FString Error;
	FBlueprintHelperExternalGraphAnchor Anchor;
	TestTrue(TEXT("node anchor builds"), BuildNodeAnchor(Blueprint, Graph, Node, Anchor, Error));

	const FBlueprintHelperPatchExternalGraphService Service;
	const FBlueprintHelperToolResultBase Result = Service.Execute(MakePatchPayload(
		Blueprint,
		Graph,
		TEXT("set_external_node_comment"),
		Anchor,
		TEXT("after"),
		TEXT("before"),
		false));

	TestFalse(TEXT("mismatch rejected"), Result.bOk);
	TestEqual(TEXT("mismatch code"),
		Result.Error.IsSet() ? Result.Error->Code : FString(),
		FString(TEXT("expected_old_state_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExternalPatchRejectsLayoutMutationTest,
	"BlueprintHelper.GraphWrite.ExternalPatch.RejectsLayoutMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperExternalPatchRejectsLayoutMutationTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperPatchExternalGraphTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("RejectsLayoutMutation"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CallFunction* Node = AddPrintStringNode(Graph);
	if (!Blueprint || !Graph || !Node)
	{
		return false;
	}

	FString Error;
	FBlueprintHelperExternalGraphAnchor Anchor;
	TestTrue(TEXT("node anchor builds"), BuildNodeAnchor(Blueprint, Graph, Node, Anchor, Error));

	const FBlueprintHelperPatchExternalGraphService Service;
	const FBlueprintHelperToolResultBase Result = Service.Execute(MakePatchPayload(
		Blueprint,
		Graph,
		TEXT("set_node_position"),
		Anchor,
		TEXT("10,10"),
		Node->NodeComment,
		false));

	TestFalse(TEXT("layout mutation rejected"), Result.bOk);
	TestEqual(TEXT("layout mutation code"),
		Result.Error.IsSet() ? Result.Error->Code : FString(),
		FString(TEXT("unsupported_external_patch_type")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExternalPatchRejectsLinkMutationTest,
	"BlueprintHelper.GraphWrite.ExternalPatch.RejectsLinkMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperExternalPatchRejectsLinkMutationTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperPatchExternalGraphTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("RejectsLinkMutation"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CallFunction* Node = AddPrintStringNode(Graph);
	if (!Blueprint || !Graph || !Node)
	{
		return false;
	}

	FString Error;
	FBlueprintHelperExternalGraphAnchor Anchor;
	TestTrue(TEXT("node anchor builds"), BuildNodeAnchor(Blueprint, Graph, Node, Anchor, Error));

	const FBlueprintHelperPatchExternalGraphService Service;
	const FBlueprintHelperToolResultBase Result = Service.Execute(MakePatchPayload(
		Blueprint,
		Graph,
		TEXT("connect_pins"),
		Anchor,
		TEXT(""),
		Node->NodeComment,
		false));

	TestFalse(TEXT("link mutation rejected"), Result.bOk);
	TestEqual(TEXT("link mutation code"),
		Result.Error.IsSet() ? Result.Error->Code : FString(),
		FString(TEXT("unsupported_external_patch_type")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExternalPatchDoesNotWriteOwnershipMetadataTest,
	"BlueprintHelper.GraphWrite.ExternalPatch.DoesNotWriteOwnershipMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperExternalPatchDoesNotWriteOwnershipMetadataTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperPatchExternalGraphTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("DoesNotWriteOwnershipMetadata"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CallFunction* Node = AddPrintStringNode(Graph);
	if (!Blueprint || !Graph || !Node)
	{
		return false;
	}

	FString Error;
	FBlueprintHelperExternalGraphAnchor Anchor;
	TestTrue(TEXT("node anchor builds"), BuildNodeAnchor(Blueprint, Graph, Node, Anchor, Error));

	const FBlueprintHelperPatchExternalGraphService Service;
	const FBlueprintHelperToolResultBase Result = Service.Execute(MakePatchPayload(
		Blueprint,
		Graph,
		TEXT("set_external_node_comment"),
		Anchor,
		TEXT("after"),
		Node->NodeComment,
		false));

	FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Node->GetOutermost());
	TestTrue(TEXT("execute ok"), Result.bOk);
	TestTrue(TEXT("external patch does not write BlueprintHelperOwned"),
		MetaData.GetValue(Node, TEXT("BlueprintHelperOwned")).IsEmpty());
	TestTrue(TEXT("external patch does not write BlueprintHelperBlockId"),
		MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId")).IsEmpty());
	return true;
}

#endif
