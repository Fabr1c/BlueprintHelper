#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalBodySnapshotService.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorService.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperReplaceExternalBodyService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalDependentsAnalysisService.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Shared/Review/BlueprintHelperReviewTargetKindRegistry.h"
#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.h"
#include "Systems/Review/BlueprintHelperReviewBaselineSnapshotService.h"
#include "Systems/Review/Utils/BlueprintHelperReviewSnapshotRestoreService.h"

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
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BlueprintHelperReplaceExternalBodyTests
{
	static FString MakeTestObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static UBlueprint* MakeBlueprint(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperExternalBody/%s"),
			*MakeTestObjectName(Prefix)));
		Package->SetDirtyFlag(false);

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*MakeTestObjectName(TEXT("BP_ExternalBody")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperReplaceExternalBodyTests"));
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

	static UK2Node_CallFunction* AddCallFunctionNode(UEdGraph* Graph, UFunction* Function)
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
		return CallNode;
	}

	static UK2Node_CallFunction* AddDestroyActorCallNode(UEdGraph* Graph)
	{
		return AddCallFunctionNode(
			Graph,
			AActor::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(AActor, K2_DestroyActor)));
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

	static bool ConnectPins(UEdGraphPin* FromPin, UEdGraphPin* ToPin)
	{
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		return FromPin && ToPin && Schema && Schema->TryCreateConnection(FromPin, ToPin);
	}

	static bool ConnectExec(UEdGraphNode* FromNode, UEdGraphNode* ToNode)
	{
		return ConnectPins(FindExecPin(FromNode, EGPD_Output), FindExecPin(ToNode, EGPD_Input));
	}

	static bool ForceConnectPins(UEdGraphPin* FromPin, UEdGraphPin* ToPin)
	{
		if (!FromPin || !ToPin)
		{
			return false;
		}
		FBlueprintHelperVersionCompat::MakePinLinkTo(FromPin, ToPin, true);
		return FromPin->LinkedTo.Contains(ToPin) && ToPin->LinkedTo.Contains(FromPin);
	}

	static int32 CountCallFunctionNodes(UEdGraph* Graph, const FName FunctionName)
	{
		int32 Count = 0;
		if (!Graph)
		{
			return Count;
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
			if (CallNode && CallNode->GetFunctionName() == FunctionName)
			{
				++Count;
			}
		}
		return Count;
	}

	static int32 CountCustomEventsByName(UEdGraph* Graph, const FString& EventName)
	{
		int32 Count = 0;
		if (!Graph)
		{
			return Count;
		}
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

	static bool EntryExecLinksToFunction(UEdGraphNode* EntryNode, const FName FunctionName)
	{
		UEdGraphPin* ExecOut = FindExecPin(EntryNode, EGPD_Output);
		if (!ExecOut)
		{
			return false;
		}
		for (UEdGraphPin* LinkedPin : ExecOut->LinkedTo)
		{
			const UK2Node_CallFunction* CallNode = LinkedPin ? Cast<UK2Node_CallFunction>(LinkedPin->GetOwningNode()) : nullptr;
			if (CallNode && CallNode->GetFunctionName() == FunctionName)
			{
				return true;
			}
		}
		return false;
	}

	static TSharedRef<FJsonObject> MakeStringLiteralExpression(const FString& Value)
	{
		TSharedRef<FJsonObject> Literal = MakeShared<FJsonObject>();
		Literal->SetStringField(TEXT("kind"), TEXT("literal"));
		Literal->SetStringField(TEXT("value_type"), TEXT("string"));
		Literal->SetStringField(TEXT("value"), Value);
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

	static TSharedRef<FJsonObject> MakeVariableGetExpression(const FString& VariableName)
	{
		TSharedRef<FJsonObject> Expression = MakeShared<FJsonObject>();
		Expression->SetStringField(TEXT("kind"), TEXT("get"));
		Expression->SetStringField(TEXT("target"), VariableName);
		return Expression;
	}

	static TSharedRef<FJsonObject> MakeVariableSetLogicSpec(const FString& TargetVariable, const FString& SourceVariable)
	{
		TSharedRef<FJsonObject> Statement = MakeShared<FJsonObject>();
		Statement->SetStringField(TEXT("kind"), TEXT("field"));
		Statement->SetStringField(TEXT("field_operation"), TEXT("set"));
		Statement->SetStringField(TEXT("field_scope"), TEXT("variable"));
		Statement->SetStringField(TEXT("target"), TargetVariable);
		Statement->SetObjectField(TEXT("value"), MakeVariableGetExpression(SourceVariable));

		TArray<TSharedPtr<FJsonValue>> Statements;
		Statements.Add(MakeShared<FJsonValueObject>(Statement));

		TSharedRef<FJsonObject> LogicSpec = MakeShared<FJsonObject>();
		LogicSpec->SetStringField(TEXT("schema"), TEXT("BlueprintLogicSpec.v2"));
		LogicSpec->SetArrayField(TEXT("statements"), Statements);
		return LogicSpec;
	}

	static bool BuildBodyEntryAnchor(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		UEdGraphNode* EntryNode,
		FBlueprintHelperExternalGraphAnchor& OutAnchor,
		FString& OutError)
	{
		const FBlueprintHelperExternalGraphAnchorService AnchorService;
		if (!AnchorService.BuildNodeAnchor(
			Blueprint ? Blueprint->GetPathName() : FString(),
			Graph ? Graph->GetName() : FString(),
			EntryNode,
			OutAnchor,
			OutError))
		{
			return false;
		}
		OutAnchor.SemanticRole = EBlueprintHelperExternalGraphAnchorRole::BodyEntry;
		return true;
	}

	static FString CaptureBodyFingerprint(UEdGraph* Graph, UEdGraphNode* EntryNode)
	{
		const FBlueprintHelperExternalBodySnapshotService SnapshotService;
		FBlueprintHelperExternalBodySnapshot Snapshot;
		FString Error;
		return SnapshotService.CaptureBody(Graph, EntryNode, Snapshot, Error)
			? Snapshot.BodyFingerprint
			: FString();
	}

	static TSharedRef<FJsonObject> MakeReplacePayload(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const FBlueprintHelperExternalGraphAnchor& Anchor,
		const FString& ExpectedBodyFingerprint,
		const FString& Scope,
		bool bDryRun)
	{
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), Blueprint ? Blueprint->GetPathName() : FString());
		Target->SetStringField(TEXT("graph"), Graph ? Graph->GetName() : FString());

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("target"), Target);
		Payload->SetStringField(TEXT("scope"), Scope);
		Payload->SetObjectField(TEXT("anchor"), Anchor.ToJson());
		Payload->SetObjectField(TEXT("body"), MakePrintStringLogicSpec(TEXT("replacement body")));
		Payload->SetStringField(TEXT("expected_body_fingerprint"), ExpectedBodyFingerprint);
		Payload->SetBoolField(TEXT("require_full_dry_run"), true);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);
		Payload->SetStringField(TEXT("feature_name"), TEXT("ReplaceExternalBodyTest"));
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeReplacePayloadWithBody(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const FBlueprintHelperExternalGraphAnchor& Anchor,
		const FString& ExpectedBodyFingerprint,
		const FString& Scope,
		const TSharedRef<FJsonObject>& Body,
		bool bDryRun)
	{
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), Blueprint ? Blueprint->GetPathName() : FString());
		Target->SetStringField(TEXT("graph"), Graph ? Graph->GetName() : FString());

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("target"), Target);
		Payload->SetStringField(TEXT("scope"), Scope);
		Payload->SetObjectField(TEXT("anchor"), Anchor.ToJson());
		Payload->SetObjectField(TEXT("body"), Body);
		Payload->SetStringField(TEXT("expected_body_fingerprint"), ExpectedBodyFingerprint);
		Payload->SetBoolField(TEXT("require_full_dry_run"), true);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);
		Payload->SetStringField(TEXT("feature_name"), TEXT("ReplaceExternalBodyVariableSetTest"));
		return Payload;
	}

	static void AddFloatMemberVariable(UBlueprint* Blueprint, const FString& VariableName)
	{
		FEdGraphPinType PinType;
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
		FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VariableName), PinType);
	}

	struct FExternalBodyFixture
	{
		UBlueprint* Blueprint = nullptr;
		UEdGraph* Graph = nullptr;
		UK2Node_CustomEvent* EntryNode = nullptr;
		UK2Node_CallFunction* BodyNode = nullptr;
		FBlueprintHelperExternalGraphAnchor Anchor;
		FString BodyFingerprint;
	};

	static FExternalBodyFixture MakeExternalBodyFixture(const FString& TestName)
	{
		FExternalBodyFixture Fixture;
		Fixture.Blueprint = MakeBlueprint(TestName);
		Fixture.Graph = GetEventGraph(Fixture.Blueprint);
		Fixture.EntryNode = AddCustomEventNode(Fixture.Graph, TEXT("ReplaceExternalBodyEntry"));
		Fixture.BodyNode = AddDestroyActorCallNode(Fixture.Graph);
		ConnectExec(Fixture.EntryNode, Fixture.BodyNode);

		FString Error;
		BuildBodyEntryAnchor(Fixture.Blueprint, Fixture.Graph, Fixture.EntryNode, Fixture.Anchor, Error);
		Fixture.BodyFingerprint = CaptureBodyFingerprint(Fixture.Graph, Fixture.EntryNode);
		return Fixture;
	}

	static FBlueprintHelperReplaceExternalBodyService MakeService(
		FBlueprintHelperBlockIdService& BlockIdService,
		FBlueprintHelperOwnershipService& OwnershipService,
		FBlueprintHelperExternalBodySnapshotService& SnapshotService,
		FBlueprintHelperExternalDependentsAnalysisService& DependentsAnalysisService)
	{
		return FBlueprintHelperReplaceExternalBodyService(
			BlockIdService,
			OwnershipService,
			SnapshotService,
			DependentsAnalysisService);
	}

	static UEdGraphNode* AddGenericNodeWithPins(UEdGraph* Graph)
	{
		if (!Graph)
		{
			return nullptr;
		}

		UEdGraphNode* Node = NewObject<UEdGraphNode>(Graph);
		Graph->AddNode(Node, true, false);
		Node->CreateNewGuid();
		FEdGraphPinType ExecPinType;
		ExecPinType.PinCategory = UEdGraphSchema_K2::PC_Exec;
		FEdGraphPinType BoolPinType;
		BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		Node->CreatePin(EGPD_Input, ExecPinType, FName(TEXT("execute")));
		Node->CreatePin(EGPD_Output, ExecPinType, FName(TEXT("then")));
		Node->CreatePin(EGPD_Input, BoolPinType, FName(TEXT("input_value")));
		Node->CreatePin(EGPD_Output, BoolPinType, FName(TEXT("value")));
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExternalBodySnapshotIncludesUpstreamDataDependenciesTest,
	"BlueprintHelper.GraphWrite.ExternalBodyReplace.SnapshotIncludesUpstreamDataDependencies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperExternalBodySnapshotIncludesUpstreamDataDependenciesTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperReplaceExternalBodyTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("SnapshotIncludesUpstreamDataDependencies"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CustomEvent* EntryNode = AddCustomEventNode(Graph, TEXT("ExternalBodyDataDependencyEntry"));
	UEdGraphNode* BodyNode = AddGenericNodeWithPins(Graph);
	UEdGraphNode* DataProducerNode = AddGenericNodeWithPins(Graph);
	UEdGraphNode* UpstreamDataProducerNode = AddGenericNodeWithPins(Graph);
	if (!Blueprint || !Graph || !EntryNode || !BodyNode || !DataProducerNode || !UpstreamDataProducerNode)
	{
		return false;
	}

	TestTrue(TEXT("body is reachable from entry"),
		ForceConnectPins(FindExecPin(EntryNode, EGPD_Output), FindExecPin(BodyNode, EGPD_Input)));
	TestTrue(TEXT("body consumes upstream data producer"),
		ForceConnectPins(FindPinByName(DataProducerNode, TEXT("value")), FindPinByName(BodyNode, TEXT("input_value"))));
	TestTrue(TEXT("upstream data producer is recursive"),
		ForceConnectPins(FindPinByName(UpstreamDataProducerNode, TEXT("value")), FindPinByName(DataProducerNode, TEXT("input_value"))));

	FBlueprintHelperExternalBodySnapshotService SnapshotService;
	FBlueprintHelperExternalBodySnapshot Snapshot;
	FString SnapshotError;
	TestTrue(TEXT("snapshot captures body"),
		SnapshotService.CaptureBody(Graph, EntryNode, Snapshot, SnapshotError));
	TestEqual(TEXT("snapshot has no error"), SnapshotError, FString());

	const FString BodyNodeGuid = BodyNode->NodeGuid.ToString(EGuidFormats::Digits);
	const FString DataProducerGuid = DataProducerNode->NodeGuid.ToString(EGuidFormats::Digits);
	const FString UpstreamDataProducerGuid = UpstreamDataProducerNode->NodeGuid.ToString(EGuidFormats::Digits);
	TestTrue(TEXT("exec body node is captured"), Snapshot.BodyNodeGuids.Contains(BodyNodeGuid));
	TestTrue(TEXT("direct upstream data producer is captured"), Snapshot.BodyNodeGuids.Contains(DataProducerGuid));
	TestTrue(TEXT("recursive upstream data producer is captured"), Snapshot.BodyNodeGuids.Contains(UpstreamDataProducerGuid));
	TestEqual(TEXT("upstream data links are internal to the body"), Snapshot.BodyToExternalLinks.Num(), 0);

	FBlueprintHelperExternalDependentsAnalysisService DependentsAnalysisService;
	FBlueprintHelperExternalDependentsAnalysis Analysis;
	FString AnalysisError;
	TestTrue(TEXT("dependents analysis succeeds"),
		DependentsAnalysisService.Analyze(Graph, Snapshot, Analysis, AnalysisError));
	TestTrue(TEXT("upstream data dependency body is supported"), Analysis.bSupported);
	TestEqual(TEXT("analysis has no unsupported dependents"), Analysis.UnsupportedDependents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReplaceExternalBodyPreviewListsExactPlanTest,
	"BlueprintHelper.GraphWrite.ExternalBodyReplace.PreviewListsExactPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReplaceExternalBodyPreviewListsExactPlanTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperReplaceExternalBodyTests;

	FExternalBodyFixture Fixture = MakeExternalBodyFixture(TEXT("PreviewListsExactPlan"));
	if (!Fixture.Blueprint || !Fixture.Graph || !Fixture.EntryNode || !Fixture.BodyNode)
	{
		return false;
	}

	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperExternalBodySnapshotService SnapshotService;
	FBlueprintHelperExternalDependentsAnalysisService DependentsAnalysisService;
	FBlueprintHelperReplaceExternalBodyService Service = MakeService(
		BlockIdService,
		OwnershipService,
		SnapshotService,
		DependentsAnalysisService);

	const FBlueprintHelperToolResultBase Result = Service.Execute(MakeReplacePayload(
		Fixture.Blueprint,
		Fixture.Graph,
		Fixture.Anchor,
		Fixture.BodyFingerprint,
		TEXT("custom_event_body"),
		true));

	TestTrue(TEXT("preview ok"), Result.bOk);
	TestNotNull(TEXT("dry-run data exists"), Result.Data.Get());
	TestEqual(TEXT("dry-run leaves original body"), CountCallFunctionNodes(Fixture.Graph, GET_FUNCTION_NAME_CHECKED(AActor, K2_DestroyActor)), 1);
	TestEqual(TEXT("dry-run does not create replacement body"), CountCallFunctionNodes(Fixture.Graph, GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString)), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReplaceExternalBodyExecutePreservesEntryTest,
	"BlueprintHelper.GraphWrite.ExternalBodyReplace.CustomEventExecutePreservesEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReplaceExternalBodyExecutePreservesEntryTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperReplaceExternalBodyTests;

	FExternalBodyFixture Fixture = MakeExternalBodyFixture(TEXT("CustomEventExecutePreservesEntry"));
	if (!Fixture.Blueprint || !Fixture.Graph || !Fixture.EntryNode || !Fixture.BodyNode)
	{
		return false;
	}

	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperExternalBodySnapshotService SnapshotService;
	FBlueprintHelperExternalDependentsAnalysisService DependentsAnalysisService;
	FBlueprintHelperReplaceExternalBodyService Service = MakeService(
		BlockIdService,
		OwnershipService,
		SnapshotService,
		DependentsAnalysisService);

	const FBlueprintHelperToolResultBase Result = Service.Execute(MakeReplacePayload(
		Fixture.Blueprint,
		Fixture.Graph,
		Fixture.Anchor,
		Fixture.BodyFingerprint,
		TEXT("custom_event_body"),
		false));

	TestTrue(TEXT("execute ok"), Result.bOk);
	TestEqual(TEXT("entry custom event is preserved"), CountCustomEventsByName(Fixture.Graph, TEXT("ReplaceExternalBodyEntry")), 1);
	TestEqual(TEXT("old body removed"), CountCallFunctionNodes(Fixture.Graph, GET_FUNCTION_NAME_CHECKED(AActor, K2_DestroyActor)), 0);
	TestEqual(TEXT("replacement body added"), CountCallFunctionNodes(Fixture.Graph, GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString)), 1);
	TestTrue(TEXT("entry exec is reconnected to replacement body"),
		EntryExecLinksToFunction(Fixture.EntryNode, GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString)));

	FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Fixture.EntryNode->GetOutermost());
	TestTrue(TEXT("external entry is not adopted as BlueprintHelper-owned"),
		MetaData.GetValue(Fixture.EntryNode, TEXT("BlueprintHelperOwned")).IsEmpty());
	TestTrue(TEXT("external entry has no replacement block id"),
		MetaData.GetValue(Fixture.EntryNode, TEXT("BlueprintHelperBlockId")).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReplaceExternalBodyExecuteAcceptsVariableSetBodyTest,
	"BlueprintHelper.GraphWrite.ExternalBodyReplace.CustomEventExecuteAcceptsVariableSetBody",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReplaceExternalBodyExecuteAcceptsVariableSetBodyTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperReplaceExternalBodyTests;

	FExternalBodyFixture Fixture = MakeExternalBodyFixture(TEXT("CustomEventExecuteAcceptsVariableSetBody"));
	if (!Fixture.Blueprint || !Fixture.Graph || !Fixture.EntryNode || !Fixture.BodyNode)
	{
		return false;
	}

	AddFloatMemberVariable(Fixture.Blueprint, TEXT("CurrentHealth"));
	AddFloatMemberVariable(Fixture.Blueprint, TEXT("MaxHealth"));

	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperExternalBodySnapshotService SnapshotService;
	FBlueprintHelperExternalDependentsAnalysisService DependentsAnalysisService;
	FBlueprintHelperReplaceExternalBodyService Service = MakeService(
		BlockIdService,
		OwnershipService,
		SnapshotService,
		DependentsAnalysisService);

	const FBlueprintHelperToolResultBase Result = Service.Execute(MakeReplacePayloadWithBody(
		Fixture.Blueprint,
		Fixture.Graph,
		Fixture.Anchor,
		Fixture.BodyFingerprint,
		TEXT("custom_event_body"),
		MakeVariableSetLogicSpec(TEXT("CurrentHealth"), TEXT("MaxHealth")),
		false));

	if (!Result.bOk && Result.Error.IsSet())
	{
		AddError(FString::Printf(
			TEXT("replace_external_body failed with code=%s message=%s"),
			*Result.Error->Code,
			*Result.Error->Message));
	}
	TestTrue(TEXT("execute ok"), Result.bOk);
	TestEqual(TEXT("old body removed"), CountCallFunctionNodes(Fixture.Graph, GET_FUNCTION_NAME_CHECKED(AActor, K2_DestroyActor)), 0);
	TestTrue(TEXT("entry exec links to replacement variable set"), FindExecPin(Fixture.EntryNode, EGPD_Output)->LinkedTo.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReplaceExternalBodyRejectsWholeGraphScopeTest,
	"BlueprintHelper.GraphWrite.ExternalBodyReplace.RejectsWholeGraphScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReplaceExternalBodyRejectsWholeGraphScopeTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperReplaceExternalBodyTests;

	FExternalBodyFixture Fixture = MakeExternalBodyFixture(TEXT("RejectsWholeGraphScope"));
	if (!Fixture.Blueprint || !Fixture.Graph || !Fixture.EntryNode)
	{
		return false;
	}

	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperExternalBodySnapshotService SnapshotService;
	FBlueprintHelperExternalDependentsAnalysisService DependentsAnalysisService;
	FBlueprintHelperReplaceExternalBodyService Service = MakeService(
		BlockIdService,
		OwnershipService,
		SnapshotService,
		DependentsAnalysisService);

	const FBlueprintHelperToolResultBase Result = Service.Execute(MakeReplacePayload(
		Fixture.Blueprint,
		Fixture.Graph,
		Fixture.Anchor,
		Fixture.BodyFingerprint,
		TEXT("graph"),
		false));

	TestFalse(TEXT("graph scope is rejected"), Result.bOk);
	TestEqual(TEXT("scope error code"),
		Result.Error.IsSet() ? Result.Error->Code : FString(),
		FString(TEXT("replace_external_body_scope_unsupported")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReplaceExternalBodyRejectsStaleBodyFingerprintTest,
	"BlueprintHelper.GraphWrite.ExternalBodyReplace.RejectsStaleBodyFingerprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReplaceExternalBodyRejectsStaleBodyFingerprintTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperReplaceExternalBodyTests;

	FExternalBodyFixture Fixture = MakeExternalBodyFixture(TEXT("RejectsStaleBodyFingerprint"));
	if (!Fixture.Blueprint || !Fixture.Graph || !Fixture.EntryNode)
	{
		return false;
	}

	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperExternalBodySnapshotService SnapshotService;
	FBlueprintHelperExternalDependentsAnalysisService DependentsAnalysisService;
	FBlueprintHelperReplaceExternalBodyService Service = MakeService(
		BlockIdService,
		OwnershipService,
		SnapshotService,
		DependentsAnalysisService);

	const FBlueprintHelperToolResultBase Result = Service.Execute(MakeReplacePayload(
		Fixture.Blueprint,
		Fixture.Graph,
		Fixture.Anchor,
		TEXT("stale"),
		TEXT("custom_event_body"),
		false));

	TestFalse(TEXT("stale body fingerprint is rejected"), Result.bOk);
	TestEqual(TEXT("fingerprint error code"),
		Result.Error.IsSet() ? Result.Error->Code : FString(),
		FString(TEXT("expected_body_fingerprint_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReplaceExternalBodyRejectsUnsupportedDependentsTest,
	"BlueprintHelper.GraphWrite.ExternalBodyReplace.RejectsUnsupportedDependents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReplaceExternalBodyRejectsUnsupportedDependentsTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperReplaceExternalBodyTests;

	UBlueprint* Blueprint = MakeBlueprint(TEXT("RejectsUnsupportedDependents"));
	UEdGraph* Graph = GetEventGraph(Blueprint);
	UK2Node_CustomEvent* EntryNode = AddCustomEventNode(Graph, TEXT("ExternalDependentEntry"));
	UEdGraphNode* BodyNode = AddGenericNodeWithPins(Graph);
	UEdGraphNode* ExternalNode = AddGenericNodeWithPins(Graph);
	if (!Blueprint || !Graph || !EntryNode || !BodyNode || !ExternalNode)
	{
		return false;
	}

	TestTrue(TEXT("body is reachable from entry"),
		ForceConnectPins(FindExecPin(EntryNode, EGPD_Output), FindExecPin(BodyNode, EGPD_Input)));
	TestTrue(TEXT("body has external dependent data link"),
		ForceConnectPins(FindPinByName(BodyNode, TEXT("value")), FindPinByName(ExternalNode, TEXT("input_value"))));

	FBlueprintHelperExternalGraphAnchor Anchor;
	FString Error;
	TestTrue(TEXT("body entry anchor builds"), BuildBodyEntryAnchor(Blueprint, Graph, EntryNode, Anchor, Error));
	const FString BodyFingerprint = CaptureBodyFingerprint(Graph, EntryNode);

	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperExternalBodySnapshotService SnapshotService;
	FBlueprintHelperExternalDependentsAnalysisService DependentsAnalysisService;
	FBlueprintHelperReplaceExternalBodyService Service = MakeService(
		BlockIdService,
		OwnershipService,
		SnapshotService,
		DependentsAnalysisService);

	const FBlueprintHelperToolResultBase Result = Service.Execute(MakeReplacePayload(
		Blueprint,
		Graph,
		Anchor,
		BodyFingerprint,
		TEXT("custom_event_body"),
		false));

	TestFalse(TEXT("unsupported dependent is rejected"), Result.bOk);
	TestEqual(TEXT("dependent error code"),
		Result.Error.IsSet() ? Result.Error->Code : FString(),
		FString(TEXT("unsupported_external_dependents")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewExternalBodyRejectRestoresEvidenceBeforeBodyTest,
	"BlueprintHelper.Review.ExternalBody.RejectRestoresEvidenceBeforeBody",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewExternalBodyRejectRestoresEvidenceBeforeBodyTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperReplaceExternalBodyTests;

	FExternalBodyFixture Fixture = MakeExternalBodyFixture(TEXT("RejectRestoresEvidenceBeforeBody"));
	if (!Fixture.Blueprint || !Fixture.Graph || !Fixture.EntryNode || !Fixture.BodyNode)
	{
		return false;
	}

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Fixture.Blueprint->GetPathName();
	Target.Surface = EBlueprintHelperReviewSurface::Graph;
	Target.GraphName = Fixture.Graph->GetName();
	Target.TargetKind = TEXT("graph_external_body");
	Target.TargetKey = FString::Printf(
		TEXT("graph_external_body:%s:node:%s:scope:custom_event_body"),
		*Fixture.Graph->GetName(),
		*Fixture.EntryNode->NodeGuid.ToString(EGuidFormats::Digits));
	Target.VisualGroupKey = TEXT("graph_external_body|review_restore");
	Target.Ownership = TEXT("external_user_authored");
	Target.NodeGuid = Fixture.EntryNode->NodeGuid.ToString(EGuidFormats::Digits);
	Target.PropertyPath = TEXT("custom_event_body");

	FBlueprintHelperReviewBaselineSnapshotService BaselineSnapshotService;
	FString BeforeSnapshotJson;
	FString BeforeSnapshotHash;
	FString SnapshotError;
	TestTrue(TEXT("before snapshot captured"),
		BaselineSnapshotService.CaptureTargetSnapshot(Target, BeforeSnapshotJson, BeforeSnapshotHash, SnapshotError));
	Target.BeforeSnapshotJson = BeforeSnapshotJson;

	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperExternalBodySnapshotService SnapshotService;
	FBlueprintHelperExternalDependentsAnalysisService DependentsAnalysisService;
	FBlueprintHelperReplaceExternalBodyService Service = MakeService(
		BlockIdService,
		OwnershipService,
		SnapshotService,
		DependentsAnalysisService);
	TestTrue(TEXT("replace external body succeeds"),
		Service.Execute(MakeReplacePayload(
			Fixture.Blueprint,
			Fixture.Graph,
			Fixture.Anchor,
			Fixture.BodyFingerprint,
			TEXT("custom_event_body"),
			false)).bOk);
	TestEqual(TEXT("replacement body exists before reject"), CountCallFunctionNodes(Fixture.Graph, GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString)), 1);

	FString RestoreError;
	TestTrue(TEXT("snapshot restore succeeds"),
		FBlueprintHelperReviewSnapshotRestoreService::ExecuteSnapshotRestore(Target, RestoreError));
	TestEqual(TEXT("replacement body removed by reject"), CountCallFunctionNodes(Fixture.Graph, GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString)), 0);
	TestEqual(TEXT("original body restored by reject"), CountCallFunctionNodes(Fixture.Graph, GET_FUNCTION_NAME_CHECKED(AActor, K2_DestroyActor)), 1);
	TestTrue(TEXT("entry exec restored to original body"),
		EntryExecLinksToFunction(Fixture.EntryNode, GET_FUNCTION_NAME_CHECKED(AActor, K2_DestroyActor)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeReplaceExternalBodyBuildsReviewEvidenceTest,
	"BlueprintHelper.TaskRuntime.GraphWrite.ReplaceExternalBody.BuildsExternalBodyReviewEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimeReplaceExternalBodyBuildsReviewEvidenceTest::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_External"));
	Target->SetStringField(TEXT("graph"), TEXT("EventGraph"));
	Payload->SetObjectField(TEXT("target"), Target);
	Payload->SetStringField(TEXT("scope"), TEXT("custom_event_body"));

	TSharedRef<FJsonObject> Anchor = MakeShared<FJsonObject>();
	Anchor->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.ExternalGraphAnchor.v1"));
	Anchor->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_External"));
	Anchor->SetStringField(TEXT("graph_name"), TEXT("EventGraph"));
	Anchor->SetStringField(TEXT("node_guid"), TEXT("11111111222233334444555555555555"));
	Anchor->SetStringField(TEXT("node_class"), TEXT("/Script/BlueprintGraph.K2Node_CustomEvent"));
	Anchor->SetStringField(TEXT("semantic_role"), TEXT("body_entry"));
	Anchor->SetStringField(TEXT("fingerprint"), TEXT("fingerprint"));
	Payload->SetObjectField(TEXT("anchor"), Anchor);

	FBlueprintHelperTaskRuntimeLoweredStep Step;
	Step.Capability = TEXT("graph_write");
	Step.AdapterOperation = TEXT("replace_external_body");
	Step.Payload = Payload;

	FBlueprintHelperWriteReviewEvidence Evidence;
	const bool bBuilt = FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(
		Step,
		FBlueprintHelperToolResultBuilder::Applied(Step.AdapterOperation, TEXT("trace_replace_external_body_review")),
		TEXT("archive_external_body"),
		TEXT("task_external_body"),
		9,
		Evidence);

	TestTrue(TEXT("replace_external_body builds Review evidence"), bBuilt);
	TestEqual(TEXT("operation kind is replace_external_body"),
		Evidence.OperationKind,
		FString(TEXT("replace_external_body")));
	TestEqual(TEXT("one atomic target is emitted"), Evidence.AtomicTargets.Num(), 1);
	if (Evidence.AtomicTargets.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewAtomicTarget& TargetEvidence = Evidence.AtomicTargets[0];
	TestEqual(TEXT("target kind"),
		TargetEvidence.TargetKind,
		FString(TEXT("graph_external_body")));
	TestEqual(TEXT("handler kind is external body"),
		static_cast<int32>(FBlueprintHelperReviewTargetKindRegistry::GetHandlerKind(TargetEvidence.TargetKind)),
		static_cast<int32>(EBlueprintHelperReviewTargetHandlerKind::GraphExternalBody));
	TestEqual(TEXT("ownership is external"),
		TargetEvidence.Ownership,
		FString(TEXT("external_user_authored")));
	TestEqual(TEXT("node guid is preserved"),
		TargetEvidence.NodeGuid,
		FString(TEXT("11111111222233334444555555555555")));
	TestEqual(TEXT("replace scope is preserved"),
		TargetEvidence.PropertyPath,
		FString(TEXT("custom_event_body")));
	TestTrue(TEXT("external body does not aggregate as graph body"),
		!FBlueprintHelperReviewTargetKindRegistry::ShouldAggregateAsGraphBody(TargetEvidence));
	return true;
}

#endif
