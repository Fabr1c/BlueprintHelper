#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Shared/Services/BlueprintHelperBlueprintStructureService.h"
#include "Systems/ToolClusters/BlueprintSignature/BlueprintHelperSignatureService.h"
#include "UObject/Package.h"

class FBlueprintHelperSignatureServiceTestsLocalUtils
{
public:
	static FString MakeSignatureServiceTestObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static UBlueprint* MakeSignatureServiceActorBlueprint(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperSignature/%s"),
			*MakeSignatureServiceTestObjectName(Prefix)));
		Package->SetDirtyFlag(false);

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*MakeSignatureServiceTestObjectName(TEXT("BP_SignatureService")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperSignatureServiceTests"));
		if (Blueprint)
		{
			// Keep service unit tests isolated from unrelated dirty project Blueprints queued for skeleton compile.
			Blueprint->Status = BS_BeingCreated;
		}
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static void SuppressExternalSmokeAssetCompileErrors(FAutomationTestBase& Test)
	{
		// These signature tests mutate transient Blueprints. In a dirty project, UE may flush
		// unrelated loaded smoke assets during structural Blueprint updates; keep that noise
		// from failing tests that only assert the transient asset under test.
		FBlueprintHelperVersionCompat::AddExpectedErrorPlainCompat(
			Test,
			TEXT("BlueprintHelperCliSmoke"),
			EAutomationExpectedErrorFlags::Contains,
			-1);
	}

	static UEdGraph* FindSignatureFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName)
	{
		if (!Blueprint)
		{
			return nullptr;
		}

		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Graph && Graph->GetName() == FunctionName)
			{
				return Graph;
			}
		}
		return nullptr;
	}

	static UK2Node_FunctionEntry* FindSignatureFunctionEntry(UEdGraph* Graph)
	{
		if (!Graph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
			{
				return Entry;
			}
		}
		return nullptr;
	}

	static UK2Node_FunctionResult* FindSignatureFunctionResult(UEdGraph* Graph)
	{
		if (!Graph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_FunctionResult* Result = Cast<UK2Node_FunctionResult>(Node))
			{
				return Result;
			}
		}
		return nullptr;
	}

	static UK2Node_CustomEvent* FindSignatureCustomEvent(UEdGraph* Graph, const FString& EventName)
	{
		if (!Graph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node);
			if (CustomEvent && CustomEvent->CustomFunctionName.ToString() == EventName)
			{
				return CustomEvent;
			}
		}
		return nullptr;
	}

	static UFunction* ResolveSignatureEventDeclarationFunction(UFunction* EventFunction)
	{
		while (EventFunction && EventFunction->GetSuperFunction())
		{
			EventFunction = EventFunction->GetSuperFunction();
		}
		return EventFunction;
	}

	static UClass* ResolveSignatureEventDeclarationClass(UFunction* EventFunction, UClass* FallbackSignatureClass)
	{
		UClass* EventSignatureClass = EventFunction ? EventFunction->GetOwnerClass() : nullptr;
		if (!EventSignatureClass)
		{
			EventSignatureClass = FallbackSignatureClass;
		}
		return EventSignatureClass ? EventSignatureClass->GetAuthoritativeClass() : nullptr;
	}

	static UK2Node_Event* FindSignatureOverrideEvent(UBlueprint* Blueprint, const FString& EventName)
	{
		if (!Blueprint)
		{
			return nullptr;
		}

		const FName EventFName(*EventName);
		UFunction* EventFunction = nullptr;
		UClass* const SignatureClass = FBlueprintEditorUtils::GetOverrideFunctionClass(
			Blueprint,
			EventFName,
			&EventFunction);
		if (!SignatureClass || !EventFunction)
		{
			return nullptr;
		}
		UFunction* const EventDeclarationFunction = ResolveSignatureEventDeclarationFunction(EventFunction);
		UClass* const EventSignatureClass = ResolveSignatureEventDeclarationClass(EventDeclarationFunction, SignatureClass);
		if (!EventDeclarationFunction || !EventSignatureClass)
		{
			return nullptr;
		}

		if (UK2Node_Event* ExistingEvent = FBlueprintEditorUtils::FindOverrideForFunction(Blueprint, EventSignatureClass, EventFName))
		{
			return ExistingEvent;
		}

		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (!Graph)
			{
				continue;
			}

			for (UEdGraphNode* Node : Graph->Nodes)
			{
				UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
				if (!EventNode ||
					!EventNode->bOverrideFunction ||
					EventNode->EventReference.GetMemberName() != EventFName)
				{
					continue;
				}

				const UClass* MemberParentClass = EventNode->EventReference.GetMemberParentClass(EventNode->GetBlueprintClassFromNode());
				if (MemberParentClass &&
					(MemberParentClass->IsChildOf(EventSignatureClass) || EventSignatureClass->IsChildOf(MemberParentClass)))
				{
					return EventNode;
				}
			}
		}

		return nullptr;
	}

	static bool HasUserDefinedPin(UK2Node_CustomEvent* EventNode, const FString& PinName)
	{
		if (!EventNode)
		{
			return false;
		}

		return EventNode->UserDefinedPins.ContainsByPredicate(
			[&PinName](const TSharedPtr<FUserPinInfo>& Pin)
			{
				return Pin.IsValid() && Pin->PinName == FName(*PinName);
			});
	}

	static bool HasSignatureDataSchema(const FBlueprintHelperToolResultBase& Result)
	{
		FString Schema;
		return Result.Data.IsValid() &&
			Result.Data->TryGetStringField(TEXT("schema"), Schema) &&
			Schema == TEXT("BlueprintSignature.v1");
	}

	static TSharedPtr<FJsonValue> MakeSignatureParamValue(const FString& Name, const FString& Category)
	{
		TSharedRef<FJsonObject> PinType = MakeShared<FJsonObject>();
		PinType->SetStringField(TEXT("category"), Category);

		TSharedRef<FJsonObject> Param = MakeShared<FJsonObject>();
		Param->SetStringField(TEXT("name"), Name);
		Param->SetObjectField(TEXT("pin_type"), PinType);
		return MakeShared<FJsonValueObject>(Param);
	}

	static FBlueprintHelperEnsureFunctionSignatureRequest MakeEnsureFunctionRequest(
		UBlueprint* Blueprint,
		const FString& FunctionName,
		bool bDryRun)
	{
		FBlueprintHelperEnsureFunctionSignatureRequest Request;
		Request.AssetPath = Blueprint ? Blueprint->GetPathName() : TEXT("");
		Request.FunctionName = FunctionName;
		Request.NameCollisionPolicy = TEXT("reuse_if_exists");
		Request.bDryRun = bDryRun;
		Request.Inputs.Add(MakeSignatureParamValue(TEXT("bPressed"), TEXT("bool")));
		Request.Outputs.Add(MakeSignatureParamValue(TEXT("bHandled"), TEXT("bool")));
		return Request;
	}

	static FBlueprintHelperEnsureCustomEventSignatureRequest MakeEnsureCustomEventRequest(
		UBlueprint* Blueprint,
		const FString& GraphName,
		const FString& EventName,
		bool bDryRun)
	{
		FBlueprintHelperEnsureCustomEventSignatureRequest Request;
		Request.AssetPath = Blueprint ? Blueprint->GetPathName() : TEXT("");
		Request.GraphName = GraphName;
		Request.EventName = EventName;
		Request.NameCollisionPolicy = TEXT("reuse_if_exists");
		Request.bDryRun = bDryRun;
		Request.Inputs.Add(MakeSignatureParamValue(TEXT("bPressed"), TEXT("bool")));
		return Request;
	}

	static FBlueprintHelperRemoveSignatureRequest MakeRemoveCustomEventSignatureRequest(
		UBlueprint* Blueprint,
		const FString& GraphName,
		const FString& EventName,
		bool bDryRun)
	{
		FBlueprintHelperRemoveSignatureRequest Request;
		Request.AssetPath = Blueprint ? Blueprint->GetPathName() : TEXT("");
		Request.GraphName = GraphName;
		Request.SignatureName = EventName;
		Request.SignatureKind = TEXT("custom_event");
		Request.bDryRun = bDryRun;
		return Request;
	}

	static FBlueprintHelperRemoveSignatureRequest MakeRemoveSignatureRequest(
		UBlueprint* Blueprint,
		const FString& SignatureKind,
		const FString& SignatureName,
		bool bDryRun)
	{
		FBlueprintHelperRemoveSignatureRequest Request;
		Request.AssetPath = Blueprint ? Blueprint->GetPathName() : TEXT("");
		Request.SignatureKind = SignatureKind;
		Request.SignatureName = SignatureName;
		Request.bDryRun = bDryRun;
		Request.bRequireReferenceContext = true;
		Request.ExecutePolicy = TEXT("blocked_preflight");
		return Request;
	}

	static FBlueprintHelperEnsureEventDispatcherSignatureRequest MakeEnsureEventDispatcherRequest(
		UBlueprint* Blueprint,
		const FString& DispatcherName,
		bool bDryRun)
	{
		FBlueprintHelperEnsureEventDispatcherSignatureRequest Request;
		Request.AssetPath = Blueprint ? Blueprint->GetPathName() : TEXT("");
		Request.DispatcherName = DispatcherName;
		Request.NameCollisionPolicy = TEXT("reuse_if_exists");
		Request.bDryRun = bDryRun;
		Request.Inputs.Add(MakeSignatureParamValue(TEXT("bIsOpen"), TEXT("bool")));
		return Request;
	}

	static FBlueprintHelperEnsureOverrideEventSignatureRequest MakeEnsureOverrideEventRequest(
		UBlueprint* Blueprint,
		const FString& EventName,
		bool bDryRun)
	{
		FBlueprintHelperEnsureOverrideEventSignatureRequest Request;
		Request.AssetPath = Blueprint ? Blueprint->GetPathName() : TEXT("");
		Request.EventName = EventName;
		Request.EventKind = TEXT("native_event");
		Request.GraphName = TEXT("EventGraph");
		Request.bDryRun = bDryRun;
		return Request;
	}

};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSignatureServiceEnsureFunctionDryRunTest,
	"BlueprintHelper.Signature.Service.EnsureFunctionDryRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSignatureServiceEnsureFunctionDryRunTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperSignatureServiceTestsLocalUtils::MakeSignatureServiceActorBlueprint(TEXT("DryRun"));
	TestNotNull(TEXT("test Blueprint exists"), Blueprint);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlueprintStructureService StructureService(Resolver);
	FBlueprintHelperSignatureService SignatureService(StructureService);

	const FString FunctionName = TEXT("BH_DryRunSignatureFunction");
	const int32 FunctionCountBefore = Blueprint ? Blueprint->FunctionGraphs.Num() : 0;

	const FBlueprintHelperToolResultBase Result = SignatureService.EnsureFunction(
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeEnsureFunctionRequest(Blueprint, FunctionName, true));

	TestTrue(TEXT("dry-run succeeds"), Result.bOk);
	TestEqual(TEXT("operation"), Result.Operation, FString(TEXT("ensure_function")));
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperToolStatus::DryRun);
	TestFalse(TEXT("dry-run does not modify"), Result.bModified);
	TestTrue(TEXT("dry-run uses signature data schema"), FBlueprintHelperSignatureServiceTestsLocalUtils::HasSignatureDataSchema(Result));
	TestEqual(TEXT("dry-run keeps function graph count"), Blueprint ? Blueprint->FunctionGraphs.Num() : 0, FunctionCountBefore);
	TestNull(TEXT("dry-run does not create function graph"), FBlueprintHelperSignatureServiceTestsLocalUtils::FindSignatureFunctionGraph(Blueprint, FunctionName));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSignatureServiceEnsureFunctionExecuteTest,
	"BlueprintHelper.Signature.Service.EnsureFunctionExecute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSignatureServiceEnsureFunctionExecuteTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperSignatureServiceTestsLocalUtils::SuppressExternalSmokeAssetCompileErrors(*this);

	UBlueprint* Blueprint = FBlueprintHelperSignatureServiceTestsLocalUtils::MakeSignatureServiceActorBlueprint(TEXT("Execute"));
	TestNotNull(TEXT("test Blueprint exists"), Blueprint);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlueprintStructureService StructureService(Resolver);
	FBlueprintHelperSignatureService SignatureService(StructureService);

	const FString FunctionName = TEXT("BH_CreateSignatureFunction");
	const FBlueprintHelperToolResultBase Result = SignatureService.EnsureFunction(
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeEnsureFunctionRequest(Blueprint, FunctionName, false));

	TestTrue(TEXT("execute succeeds"), Result.bOk);
	TestEqual(TEXT("operation"), Result.Operation, FString(TEXT("ensure_function")));
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperToolStatus::Applied);
	TestTrue(TEXT("execute modifies"), Result.bModified);
	TestTrue(TEXT("execute uses signature data schema"), FBlueprintHelperSignatureServiceTestsLocalUtils::HasSignatureDataSchema(Result));

	UEdGraph* FunctionGraph = FBlueprintHelperSignatureServiceTestsLocalUtils::FindSignatureFunctionGraph(Blueprint, FunctionName);
	TestNotNull(TEXT("function graph created"), FunctionGraph);

	UK2Node_FunctionEntry* Entry = FBlueprintHelperSignatureServiceTestsLocalUtils::FindSignatureFunctionEntry(FunctionGraph);
	TestNotNull(TEXT("function entry exists"), Entry);
	TestTrue(TEXT("input pin added"), Entry && Entry->UserDefinedPins.ContainsByPredicate(
		[](const TSharedPtr<FUserPinInfo>& Pin)
		{
			return Pin.IsValid() && Pin->PinName == FName(TEXT("bPressed"));
		}));

	UK2Node_FunctionResult* FunctionResult = FBlueprintHelperSignatureServiceTestsLocalUtils::FindSignatureFunctionResult(FunctionGraph);
	TestNotNull(TEXT("function result exists"), FunctionResult);
	TestTrue(TEXT("output pin added"), FunctionResult && FunctionResult->UserDefinedPins.ContainsByPredicate(
		[](const TSharedPtr<FUserPinInfo>& Pin)
		{
			return Pin.IsValid() && Pin->PinName == FName(TEXT("bHandled"));
		}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSignatureServiceEnsureFunctionReuseTest,
	"BlueprintHelper.Signature.Service.EnsureFunctionReuse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSignatureServiceEnsureFunctionReuseTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperSignatureServiceTestsLocalUtils::SuppressExternalSmokeAssetCompileErrors(*this);

	UBlueprint* Blueprint = FBlueprintHelperSignatureServiceTestsLocalUtils::MakeSignatureServiceActorBlueprint(TEXT("Reuse"));
	TestNotNull(TEXT("test Blueprint exists"), Blueprint);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlueprintStructureService StructureService(Resolver);
	FBlueprintHelperSignatureService SignatureService(StructureService);

	const FString FunctionName = TEXT("BH_ReuseSignatureFunction");
	const FBlueprintHelperToolResultBase FirstResult = SignatureService.EnsureFunction(
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeEnsureFunctionRequest(Blueprint, FunctionName, false));
	TestTrue(TEXT("first ensure succeeds"), FirstResult.bOk);

	const int32 FunctionCountAfterFirst = Blueprint ? Blueprint->FunctionGraphs.Num() : 0;
	const FBlueprintHelperToolResultBase SecondResult = SignatureService.EnsureFunction(
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeEnsureFunctionRequest(Blueprint, FunctionName, false));

	TestTrue(TEXT("second ensure succeeds"), SecondResult.bOk);
	TestEqual(TEXT("second ensure is no-op"), SecondResult.Status, EBlueprintHelperToolStatus::NoOp);
	TestFalse(TEXT("second ensure does not modify"), SecondResult.bModified);
	TestEqual(TEXT("second ensure does not add graph"), Blueprint ? Blueprint->FunctionGraphs.Num() : 0, FunctionCountAfterFirst);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSignatureServiceEnsureCustomEventDryRunTest,
	"BlueprintHelper.Signature.Service.EnsureCustomEventDryRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSignatureServiceEnsureCustomEventDryRunTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperSignatureServiceTestsLocalUtils::MakeSignatureServiceActorBlueprint(TEXT("CustomEventDryRun"));
	TestNotNull(TEXT("test Blueprint exists"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0)
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	TestNotNull(TEXT("test EventGraph exists"), Graph);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlueprintStructureService StructureService(Resolver);
	FBlueprintHelperSignatureService SignatureService(StructureService);

	const FString EventName = TEXT("BH_DryRunCustomEvent");
	const int32 NodeCountBefore = Graph ? Graph->Nodes.Num() : 0;
	const FBlueprintHelperToolResultBase Result = SignatureService.EnsureCustomEvent(
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeEnsureCustomEventRequest(Blueprint, Graph->GetName(), EventName, true));

	TestTrue(TEXT("custom event dry-run succeeds"), Result.bOk);
	TestEqual(TEXT("operation"), Result.Operation, FString(TEXT("ensure_custom_event")));
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperToolStatus::DryRun);
	TestFalse(TEXT("dry-run does not modify"), Result.bModified);
	TestTrue(TEXT("dry-run uses signature data schema"), FBlueprintHelperSignatureServiceTestsLocalUtils::HasSignatureDataSchema(Result));
	TestEqual(TEXT("dry-run keeps graph node count"), Graph ? Graph->Nodes.Num() : 0, NodeCountBefore);
	TestNull(TEXT("dry-run does not create custom event"), FBlueprintHelperSignatureServiceTestsLocalUtils::FindSignatureCustomEvent(Graph, EventName));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSignatureServiceEnsureCustomEventExecuteTest,
	"BlueprintHelper.Signature.Service.EnsureCustomEventExecute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSignatureServiceEnsureCustomEventExecuteTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperSignatureServiceTestsLocalUtils::SuppressExternalSmokeAssetCompileErrors(*this);

	UBlueprint* Blueprint = FBlueprintHelperSignatureServiceTestsLocalUtils::MakeSignatureServiceActorBlueprint(TEXT("CustomEventExecute"));
	TestNotNull(TEXT("test Blueprint exists"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0)
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	TestNotNull(TEXT("test EventGraph exists"), Graph);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlueprintStructureService StructureService(Resolver);
	FBlueprintHelperSignatureService SignatureService(StructureService);

	const FString EventName = TEXT("BH_CreateCustomEvent");
	const FBlueprintHelperToolResultBase Result = SignatureService.EnsureCustomEvent(
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeEnsureCustomEventRequest(Blueprint, Graph->GetName(), EventName, false));

	TestTrue(TEXT("custom event execute succeeds"), Result.bOk);
	TestEqual(TEXT("operation"), Result.Operation, FString(TEXT("ensure_custom_event")));
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperToolStatus::Applied);
	TestTrue(TEXT("execute modifies"), Result.bModified);
	TestTrue(TEXT("execute uses signature data schema"), FBlueprintHelperSignatureServiceTestsLocalUtils::HasSignatureDataSchema(Result));

	UK2Node_CustomEvent* EventNode = FBlueprintHelperSignatureServiceTestsLocalUtils::FindSignatureCustomEvent(Graph, EventName);
	TestNotNull(TEXT("custom event node created"), EventNode);
	TestTrue(TEXT("input pin added"), FBlueprintHelperSignatureServiceTestsLocalUtils::HasUserDefinedPin(EventNode, TEXT("bPressed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSignatureServiceEnsureCustomEventReuseTest,
	"BlueprintHelper.Signature.Service.EnsureCustomEventReuse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSignatureServiceEnsureCustomEventReuseTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperSignatureServiceTestsLocalUtils::SuppressExternalSmokeAssetCompileErrors(*this);

	UBlueprint* Blueprint = FBlueprintHelperSignatureServiceTestsLocalUtils::MakeSignatureServiceActorBlueprint(TEXT("CustomEventReuse"));
	TestNotNull(TEXT("test Blueprint exists"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0)
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlueprintStructureService StructureService(Resolver);
	FBlueprintHelperSignatureService SignatureService(StructureService);

	const FString EventName = TEXT("BH_ReuseCustomEvent");
	const FBlueprintHelperToolResultBase FirstResult = SignatureService.EnsureCustomEvent(
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeEnsureCustomEventRequest(Blueprint, Graph->GetName(), EventName, false));
	TestTrue(TEXT("first custom event ensure succeeds"), FirstResult.bOk);

	const int32 NodeCountAfterFirst = Graph ? Graph->Nodes.Num() : 0;
	const FBlueprintHelperToolResultBase SecondResult = SignatureService.EnsureCustomEvent(
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeEnsureCustomEventRequest(Blueprint, Graph->GetName(), EventName, false));

	TestTrue(TEXT("second custom event ensure succeeds"), SecondResult.bOk);
	TestEqual(TEXT("second custom event ensure is no-op"), SecondResult.Status, EBlueprintHelperToolStatus::NoOp);
	TestFalse(TEXT("second custom event ensure does not modify"), SecondResult.bModified);
	TestEqual(TEXT("second custom event ensure does not add node"), Graph ? Graph->Nodes.Num() : 0, NodeCountAfterFirst);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSignatureServiceRemoveSignatureDryRunBlockedTest,
	"BlueprintHelper.Signature.Service.RemoveSignatureDryRunBlocked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSignatureServiceRemoveSignatureDryRunBlockedTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperSignatureServiceTestsLocalUtils::MakeSignatureServiceActorBlueprint(TEXT("RemoveSignatureDryRun"));
	TestNotNull(TEXT("test Blueprint exists"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0)
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const int32 NodeCountBefore = Graph ? Graph->Nodes.Num() : 0;

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlueprintStructureService StructureService(Resolver);
	FBlueprintHelperSignatureService SignatureService(StructureService);

	const FBlueprintHelperToolResultBase Result = SignatureService.RemoveSignature(
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeRemoveCustomEventSignatureRequest(Blueprint, Graph->GetName(), TEXT("BH_RemoveCustomEvent"), true));

	TestFalse(TEXT("remove signature dry-run is blocked"), Result.bOk);
	TestEqual(TEXT("remove signature reports failed status"), Result.Status, EBlueprintHelperToolStatus::Failed);
	TestTrue(TEXT("remove signature uses signature data schema"), FBlueprintHelperSignatureServiceTestsLocalUtils::HasSignatureDataSchema(Result));
	TestTrue(TEXT("remove signature has error"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("remove signature blocked policy code"), Result.Error->Code, FString(TEXT("signature_remove_blocked_by_policy")));
	}

	const TSharedPtr<FJsonObject>* DryRunObject = nullptr;
	TestTrue(TEXT("remove signature exposes dry_run object"),
		Result.Data.IsValid() && Result.Data->TryGetObjectField(TEXT("dry_run"), DryRunObject) && DryRunObject && DryRunObject->IsValid());
	if (DryRunObject && DryRunObject->IsValid())
	{
		FString DryRunResult;
		TestTrue(TEXT("remove dry_run carries blocked result"), (*DryRunObject)->TryGetStringField(TEXT("result"), DryRunResult));
		TestEqual(TEXT("remove dry_run is blocked"), DryRunResult, FString(TEXT("blocked")));
	}

	const TSharedPtr<FJsonObject>* RemoveResultObject = nullptr;
	TestTrue(TEXT("remove signature exposes result object"),
		Result.Data.IsValid() && Result.Data->TryGetObjectField(TEXT("remove_signature_result"), RemoveResultObject) &&
		RemoveResultObject && RemoveResultObject->IsValid());
	if (RemoveResultObject && RemoveResultObject->IsValid())
	{
		const TSharedPtr<FJsonObject>* ReferenceContextRequest = nullptr;
		TestTrue(TEXT("remove signature carries reference context request"),
			(*RemoveResultObject)->TryGetObjectField(TEXT("reference_context_request"), ReferenceContextRequest) &&
			ReferenceContextRequest && ReferenceContextRequest->IsValid());
		if (ReferenceContextRequest && ReferenceContextRequest->IsValid())
		{
			FString TargetType;
			FString ResolutionPolicy;
			TestTrue(TEXT("reference context request has target type"), (*ReferenceContextRequest)->TryGetStringField(TEXT("target_type"), TargetType));
			TestEqual(TEXT("custom event remove uses custom_event reference target"), TargetType, FString(TEXT("custom_event")));
			TestTrue(TEXT("reference context request has ue_only policy"), (*ReferenceContextRequest)->TryGetStringField(TEXT("resolution_policy"), ResolutionPolicy));
			TestEqual(TEXT("remove reference context uses ue_only"), ResolutionPolicy, FString(TEXT("ue_only")));
		}
	}
	TestEqual(TEXT("remove signature dry-run does not mutate graph"), Graph ? Graph->Nodes.Num() : 0, NodeCountBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSignatureServiceRemoveFunctionExecuteIfUnreferencedTest,
	"BlueprintHelper.Signature.Service.RemoveFunctionExecuteIfUnreferenced",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSignatureServiceRemoveFunctionExecuteIfUnreferencedTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperSignatureServiceTestsLocalUtils::SuppressExternalSmokeAssetCompileErrors(*this);

	UBlueprint* Blueprint = FBlueprintHelperSignatureServiceTestsLocalUtils::MakeSignatureServiceActorBlueprint(TEXT("RemoveFunctionExecute"));
	TestNotNull(TEXT("test Blueprint exists"), Blueprint);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlueprintStructureService StructureService(Resolver);
	FBlueprintHelperSignatureService SignatureService(StructureService);

	const FString FunctionName = TEXT("BH_RemoveFunctionWhenSafe");
	const FBlueprintHelperToolResultBase EnsureResult = SignatureService.EnsureFunction(
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeEnsureFunctionRequest(Blueprint, FunctionName, false));
	TestTrue(TEXT("function ensure succeeds"), EnsureResult.bOk);
	TestNotNull(TEXT("function graph exists before remove"),
		FBlueprintHelperSignatureServiceTestsLocalUtils::FindSignatureFunctionGraph(Blueprint, FunctionName));

	FBlueprintHelperRemoveSignatureRequest RemoveRequest =
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeRemoveSignatureRequest(Blueprint, TEXT("function"), FunctionName, false);
	RemoveRequest.ExecutePolicy = TEXT("execute_if_unreferenced");

	const FBlueprintHelperToolResultBase RemoveResult = SignatureService.RemoveSignature(RemoveRequest);

	TestTrue(TEXT("remove function succeeds"), RemoveResult.bOk);
	TestEqual(TEXT("remove function is applied"), RemoveResult.Status, EBlueprintHelperToolStatus::Applied);
	TestTrue(TEXT("remove function modifies"), RemoveResult.bModified);
	TestNull(TEXT("function graph removed"),
		FBlueprintHelperSignatureServiceTestsLocalUtils::FindSignatureFunctionGraph(Blueprint, FunctionName));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSignatureServiceRemoveCustomEventExecuteIfUnreferencedTest,
	"BlueprintHelper.Signature.Service.RemoveCustomEventExecuteIfUnreferenced",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSignatureServiceRemoveCustomEventExecuteIfUnreferencedTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperSignatureServiceTestsLocalUtils::SuppressExternalSmokeAssetCompileErrors(*this);

	UBlueprint* Blueprint = FBlueprintHelperSignatureServiceTestsLocalUtils::MakeSignatureServiceActorBlueprint(TEXT("RemoveCustomEventExecute"));
	TestNotNull(TEXT("test Blueprint exists"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0)
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlueprintStructureService StructureService(Resolver);
	FBlueprintHelperSignatureService SignatureService(StructureService);

	const FString EventName = TEXT("BH_RemoveCustomEventWhenSafe");
	const FBlueprintHelperToolResultBase EnsureResult = SignatureService.EnsureCustomEvent(
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeEnsureCustomEventRequest(Blueprint, Graph->GetName(), EventName, false));
	TestTrue(TEXT("custom event ensure succeeds"), EnsureResult.bOk);
	TestNotNull(TEXT("custom event exists before remove"),
		FBlueprintHelperSignatureServiceTestsLocalUtils::FindSignatureCustomEvent(Graph, EventName));

	FBlueprintHelperRemoveSignatureRequest RemoveRequest =
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeRemoveCustomEventSignatureRequest(Blueprint, Graph->GetName(), EventName, false);
	RemoveRequest.ExecutePolicy = TEXT("execute_if_unreferenced");

	const FBlueprintHelperToolResultBase RemoveResult = SignatureService.RemoveSignature(RemoveRequest);

	TestTrue(TEXT("remove custom event succeeds"), RemoveResult.bOk);
	TestEqual(TEXT("remove custom event is applied"), RemoveResult.Status, EBlueprintHelperToolStatus::Applied);
	TestTrue(TEXT("remove custom event modifies"), RemoveResult.bModified);
	TestNull(TEXT("custom event removed"),
		FBlueprintHelperSignatureServiceTestsLocalUtils::FindSignatureCustomEvent(Graph, EventName));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSignatureServiceRemoveEventDispatcherDryRunBlockedTest,
	"BlueprintHelper.Signature.Service.RemoveEventDispatcherDryRunBlocked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSignatureServiceRemoveEventDispatcherDryRunBlockedTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperSignatureServiceTestsLocalUtils::MakeSignatureServiceActorBlueprint(TEXT("RemoveDispatcherDryRun"));
	TestNotNull(TEXT("test Blueprint exists"), Blueprint);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlueprintStructureService StructureService(Resolver);
	FBlueprintHelperSignatureService SignatureService(StructureService);

	const FBlueprintHelperToolResultBase Result = SignatureService.RemoveSignature(
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeRemoveSignatureRequest(Blueprint, TEXT("event_dispatcher"), TEXT("OnDoorOpened"), true));

	TestFalse(TEXT("event dispatcher remove signature dry-run is blocked"), Result.bOk);
	TestEqual(TEXT("remove signature reports failed status"), Result.Status, EBlueprintHelperToolStatus::Failed);
	TestTrue(TEXT("remove signature uses signature data schema"), FBlueprintHelperSignatureServiceTestsLocalUtils::HasSignatureDataSchema(Result));
	TestTrue(TEXT("remove signature has error"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("event dispatcher remove blocked code"), Result.Error->Code, FString(TEXT("signature_remove_blocked_by_policy")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSignatureServiceRejectsRemoveWithoutReferenceContextTest,
	"BlueprintHelper.Signature.Service.RejectsRemoveWithoutReferenceContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSignatureServiceRejectsRemoveWithoutReferenceContextTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperSignatureServiceTestsLocalUtils::MakeSignatureServiceActorBlueprint(TEXT("RemoveNeedsReferenceContext"));
	TestNotNull(TEXT("test Blueprint exists"), Blueprint);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlueprintStructureService StructureService(Resolver);
	FBlueprintHelperSignatureService SignatureService(StructureService);

	FBlueprintHelperRemoveSignatureRequest Request =
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeRemoveSignatureRequest(Blueprint, TEXT("event_dispatcher"), TEXT("OnDoorOpened"), true);
	Request.bRequireReferenceContext = false;

	const FBlueprintHelperToolResultBase Result = SignatureService.RemoveSignature(Request);

	TestFalse(TEXT("remove without reference context is rejected"), Result.bOk);
	TestTrue(TEXT("remove without reference context has error"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("remove reference context error code"), Result.Error->Code, FString(TEXT("invalid_signature_remove_policy")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSignatureServiceRemoveNativeEventDryRunBlockedTest,
	"BlueprintHelper.Signature.Service.RemoveNativeEventDryRunBlocked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSignatureServiceRemoveNativeEventDryRunBlockedTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperSignatureServiceTestsLocalUtils::MakeSignatureServiceActorBlueprint(TEXT("RemoveNativeEventDryRun"));
	TestNotNull(TEXT("test Blueprint exists"), Blueprint);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlueprintStructureService StructureService(Resolver);
	FBlueprintHelperSignatureService SignatureService(StructureService);

	const FBlueprintHelperToolResultBase Result = SignatureService.RemoveSignature(
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeRemoveSignatureRequest(Blueprint, TEXT("native_event"), TEXT("ReceiveBeginPlay"), true));

	TestFalse(TEXT("native event remove signature dry-run is blocked"), Result.bOk);
	TestEqual(TEXT("remove signature reports failed status"), Result.Status, EBlueprintHelperToolStatus::Failed);
	TestTrue(TEXT("remove signature uses signature data schema"), FBlueprintHelperSignatureServiceTestsLocalUtils::HasSignatureDataSchema(Result));
	TestTrue(TEXT("remove signature has error"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("native event remove blocked code"), Result.Error->Code, FString(TEXT("signature_remove_blocked_by_policy")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSignatureServiceEnsureEventDispatcherDryRunTest,
	"BlueprintHelper.Signature.Service.EnsureEventDispatcherDryRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSignatureServiceEnsureEventDispatcherDryRunTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperSignatureServiceTestsLocalUtils::MakeSignatureServiceActorBlueprint(TEXT("EventDispatcherDryRun"));
	TestNotNull(TEXT("test Blueprint exists"), Blueprint);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlueprintStructureService StructureService(Resolver);
	FBlueprintHelperSignatureService SignatureService(StructureService);

	const FString DispatcherName = TEXT("BH_OnDoorOpened");
	const int32 DispatcherCountBefore = Blueprint ? Blueprint->DelegateSignatureGraphs.Num() : 0;

	const FBlueprintHelperToolResultBase Result = SignatureService.EnsureEventDispatcher(
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeEnsureEventDispatcherRequest(Blueprint, DispatcherName, true));

	TestTrue(TEXT("dispatcher dry-run succeeds"), Result.bOk);
	TestEqual(TEXT("operation"), Result.Operation, FString(TEXT("ensure_event_dispatcher")));
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperToolStatus::DryRun);
	TestFalse(TEXT("dry-run does not modify"), Result.bModified);
	TestTrue(TEXT("dry-run uses signature data schema"), FBlueprintHelperSignatureServiceTestsLocalUtils::HasSignatureDataSchema(Result));
	TestEqual(TEXT("dry-run keeps dispatcher graph count"), Blueprint ? Blueprint->DelegateSignatureGraphs.Num() : 0, DispatcherCountBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSignatureServiceRejectsDispatcherMutationPolicyTest,
	"BlueprintHelper.Signature.Service.RejectsDispatcherMutationPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSignatureServiceRejectsDispatcherMutationPolicyTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperSignatureServiceTestsLocalUtils::MakeSignatureServiceActorBlueprint(TEXT("DispatcherMutationPolicy"));
	TestNotNull(TEXT("test Blueprint exists"), Blueprint);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlueprintStructureService StructureService(Resolver);
	FBlueprintHelperSignatureService SignatureService(StructureService);

	FBlueprintHelperEnsureEventDispatcherSignatureRequest Request =
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeEnsureEventDispatcherRequest(Blueprint, TEXT("BH_OnDoorOpened"), true);
	Request.SignatureMismatchPolicy = TEXT("mutate");

	const FBlueprintHelperToolResultBase Result = SignatureService.EnsureEventDispatcher(Request);

	TestFalse(TEXT("unsupported dispatcher mutation policy is rejected"), Result.bOk);
	TestTrue(TEXT("dispatcher policy result has error"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("dispatcher policy error code"), Result.Error->Code, FString(TEXT("invalid_event_dispatcher_mutation_policy")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSignatureServiceEnsureOverrideEventDryRunBlockedTest,
	"BlueprintHelper.Signature.Service.EnsureOverrideEventDryRunBlocked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSignatureServiceEnsureOverrideEventDryRunBlockedTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperSignatureServiceTestsLocalUtils::MakeSignatureServiceActorBlueprint(TEXT("OverrideEventDryRun"));
	TestNotNull(TEXT("test Blueprint exists"), Blueprint);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlueprintStructureService StructureService(Resolver);
	FBlueprintHelperSignatureService SignatureService(StructureService);

	const FBlueprintHelperToolResultBase Result = SignatureService.EnsureOverrideEvent(
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeEnsureOverrideEventRequest(Blueprint, TEXT("ReceiveBeginPlay"), true));

	TestFalse(TEXT("override event dry-run is blocked"), Result.bOk);
	TestEqual(TEXT("operation"), Result.Operation, FString(TEXT("ensure_override_event")));
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperToolStatus::Failed);
	TestFalse(TEXT("blocked preflight does not modify"), Result.bModified);
	TestTrue(TEXT("override event uses signature data schema"), FBlueprintHelperSignatureServiceTestsLocalUtils::HasSignatureDataSchema(Result));
	TestTrue(TEXT("override event has error"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("override event blocked policy code"), Result.Error->Code, FString(TEXT("override_event_signature_blocked_by_policy")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSignatureServiceEnsureOverrideEventCreateIfMissingDryRunTest,
	"BlueprintHelper.Signature.Service.EnsureOverrideEventCreateIfMissingDryRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSignatureServiceEnsureOverrideEventCreateIfMissingDryRunTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperSignatureServiceTestsLocalUtils::MakeSignatureServiceActorBlueprint(TEXT("OverrideEventCreateDryRun"));
	TestNotNull(TEXT("test Blueprint exists"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0)
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const int32 NodeCountBefore = Graph ? Graph->Nodes.Num() : 0;

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlueprintStructureService StructureService(Resolver);
	FBlueprintHelperSignatureService SignatureService(StructureService);

	const FString EventName = TEXT("ReceiveAnyDamage");
	TestNull(TEXT("override event is initially missing"),
		FBlueprintHelperSignatureServiceTestsLocalUtils::FindSignatureOverrideEvent(Blueprint, EventName));

	FBlueprintHelperEnsureOverrideEventSignatureRequest Request =
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeEnsureOverrideEventRequest(Blueprint, EventName, true);
	Request.GraphName = Graph->GetName();
	Request.ExecutePolicy = TEXT("create_if_missing");

	const FBlueprintHelperToolResultBase Result = SignatureService.EnsureOverrideEvent(Request);

	TestTrue(TEXT("override event create dry-run succeeds"), Result.bOk);
	TestEqual(TEXT("operation"), Result.Operation, FString(TEXT("ensure_override_event")));
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperToolStatus::DryRun);
	TestFalse(TEXT("dry-run does not modify"), Result.bModified);
	TestTrue(TEXT("override event uses signature data schema"), FBlueprintHelperSignatureServiceTestsLocalUtils::HasSignatureDataSchema(Result));
	TestEqual(TEXT("dry-run does not add event node"), Graph ? Graph->Nodes.Num() : 0, NodeCountBefore);
	TestNull(TEXT("dry-run does not create override event"), FBlueprintHelperSignatureServiceTestsLocalUtils::FindSignatureOverrideEvent(Blueprint, EventName));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSignatureServiceEnsureOverrideEventCreateIfMissingExecuteTest,
	"BlueprintHelper.Signature.Service.EnsureOverrideEventCreateIfMissingExecute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSignatureServiceEnsureOverrideEventCreateIfMissingExecuteTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperSignatureServiceTestsLocalUtils::SuppressExternalSmokeAssetCompileErrors(*this);

	UBlueprint* Blueprint = FBlueprintHelperSignatureServiceTestsLocalUtils::MakeSignatureServiceActorBlueprint(TEXT("OverrideEventCreateExecute"));
	TestNotNull(TEXT("test Blueprint exists"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0)
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	TestNotNull(TEXT("test EventGraph exists"), Graph);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlueprintStructureService StructureService(Resolver);
	FBlueprintHelperSignatureService SignatureService(StructureService);

	const FString EventName = TEXT("ReceiveAnyDamage");
	TestNull(TEXT("override event is initially missing"),
		FBlueprintHelperSignatureServiceTestsLocalUtils::FindSignatureOverrideEvent(Blueprint, EventName));

	FBlueprintHelperEnsureOverrideEventSignatureRequest Request =
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeEnsureOverrideEventRequest(Blueprint, EventName, false);
	Request.GraphName = Graph->GetName();
	Request.ExecutePolicy = TEXT("create_if_missing");

	const FBlueprintHelperToolResultBase Result = SignatureService.EnsureOverrideEvent(Request);

	TestTrue(TEXT("override event create executes"), Result.bOk);
	TestEqual(TEXT("operation"), Result.Operation, FString(TEXT("ensure_override_event")));
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperToolStatus::Applied);
	TestTrue(TEXT("execute modifies"), Result.bModified);
	TestTrue(TEXT("override event uses signature data schema"), FBlueprintHelperSignatureServiceTestsLocalUtils::HasSignatureDataSchema(Result));

	UK2Node_Event* EventNode = FBlueprintHelperSignatureServiceTestsLocalUtils::FindSignatureOverrideEvent(Blueprint, EventName);
	TestNotNull(TEXT("override event node exists"), EventNode);
	TestTrue(TEXT("override function flag set"), EventNode && EventNode->bOverrideFunction);

	const int32 NodeCountAfterFirst = Graph ? Graph->Nodes.Num() : 0;
	const FBlueprintHelperToolResultBase SecondResult = SignatureService.EnsureOverrideEvent(Request);
	TestTrue(TEXT("second override event ensure succeeds"), SecondResult.bOk);
	TestEqual(TEXT("second override event ensure is no-op"), SecondResult.Status, EBlueprintHelperToolStatus::NoOp);
	TestFalse(TEXT("second override event ensure does not modify"), SecondResult.bModified);
	TestEqual(TEXT("second override event ensure does not add node"), Graph ? Graph->Nodes.Num() : 0, NodeCountAfterFirst);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSignatureServiceMigratesEventDispatcherWhenUnreferencedTest,
	"BlueprintHelper.Signature.Service.MigratesEventDispatcherWhenUnreferenced",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSignatureServiceMigratesEventDispatcherWhenUnreferencedTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperSignatureServiceTestsLocalUtils::SuppressExternalSmokeAssetCompileErrors(*this);

	UBlueprint* Blueprint = FBlueprintHelperSignatureServiceTestsLocalUtils::MakeSignatureServiceActorBlueprint(TEXT("DispatcherMigration"));
	TestNotNull(TEXT("test Blueprint exists"), Blueprint);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlueprintStructureService StructureService(Resolver);
	FBlueprintHelperSignatureService SignatureService(StructureService);

	const FString DispatcherName = TEXT("BH_OnDoorOpenedMigration");
	const FBlueprintHelperToolResultBase EnsureResult = SignatureService.EnsureEventDispatcher(
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeEnsureEventDispatcherRequest(Blueprint, DispatcherName, false));
	TestTrue(TEXT("initial dispatcher ensure succeeds"), EnsureResult.bOk);

	FBlueprintHelperEnsureEventDispatcherSignatureRequest MigrationRequest =
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeEnsureEventDispatcherRequest(Blueprint, DispatcherName, false);
	MigrationRequest.SignatureMismatchPolicy = TEXT("migrate_if_unreferenced");
	MigrationRequest.Inputs.Reset();
	MigrationRequest.Inputs.Add(FBlueprintHelperSignatureServiceTestsLocalUtils::MakeSignatureParamValue(TEXT("Count"), TEXT("int")));

	const FBlueprintHelperToolResultBase MigrationResult = SignatureService.EnsureEventDispatcher(MigrationRequest);

	TestTrue(TEXT("dispatcher migration succeeds"), MigrationResult.bOk);
	TestEqual(TEXT("dispatcher migration is applied"), MigrationResult.Status, EBlueprintHelperToolStatus::Applied);
	TestTrue(TEXT("dispatcher migration modifies"), MigrationResult.bModified);

	FBlueprintHelperGraphTarget ListTarget;
	ListTarget.BlueprintPath = Blueprint->GetPathName();
	const FBlueprintHelperListDispatchersResult Dispatchers =
		StructureService.ListEventDispatchers(ListTarget);
	TestTrue(TEXT("dispatcher list succeeds"), Dispatchers.bSuccess);
	const FBlueprintHelperEventDispatcherInfo* MigratedDispatcher = Dispatchers.Dispatchers.FindByPredicate(
		[&DispatcherName](const FBlueprintHelperEventDispatcherInfo& Dispatcher)
		{
			return Dispatcher.Name == DispatcherName;
		});
	TestNotNull(TEXT("migrated dispatcher exists"), MigratedDispatcher);
	if (MigratedDispatcher)
	{
		TestEqual(TEXT("migrated dispatcher has one param"), MigratedDispatcher->Params.Num(), 1);
		TestTrue(TEXT("migrated dispatcher carries Count int"),
			MigratedDispatcher->Params.Contains(TEXT("Count:int")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSignatureServiceRejectsOverrideExecutePolicyTest,
	"BlueprintHelper.Signature.Service.RejectsOverrideExecutePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSignatureServiceRejectsOverrideExecutePolicyTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperSignatureServiceTestsLocalUtils::MakeSignatureServiceActorBlueprint(TEXT("OverrideExecutePolicy"));
	TestNotNull(TEXT("test Blueprint exists"), Blueprint);

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlueprintStructureService StructureService(Resolver);
	FBlueprintHelperSignatureService SignatureService(StructureService);

	FBlueprintHelperEnsureOverrideEventSignatureRequest Request =
		FBlueprintHelperSignatureServiceTestsLocalUtils::MakeEnsureOverrideEventRequest(Blueprint, TEXT("ReceiveBeginPlay"), true);
	Request.ExecutePolicy = TEXT("execute");

	const FBlueprintHelperToolResultBase Result = SignatureService.EnsureOverrideEvent(Request);

	TestFalse(TEXT("unsupported override execute policy is rejected"), Result.bOk);
	TestTrue(TEXT("override policy result has error"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("override policy error code"), Result.Error->Code, FString(TEXT("invalid_override_event_execute_policy")));
	}
	return true;
}

#endif
