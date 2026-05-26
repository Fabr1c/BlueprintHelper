#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "UObject/UnrealType.h"

namespace
{
static FMulticastDelegateProperty* FindOverlapDelegate()
{
	return FindFProperty<FMulticastDelegateProperty>(
		UPrimitiveComponent::StaticClass(),
		TEXT("OnComponentBeginOverlap"));
}

static FBlueprintHelperActionResolutionRequest MakeEvidenceRequest()
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Delegate;
	Request.Semantic.Query = TEXT("OnComponentBeginOverlap");

	if (FMulticastDelegateProperty* DelegateProperty = FindOverlapDelegate())
	{
		const UClass* OwnerClass = Cast<UClass>(DelegateProperty->GetOwnerStruct());
		Request.ContextEvidence.Add(TEXT("event_delegate.delegate_name"), DelegateProperty->GetName());
		Request.ContextEvidence.Add(TEXT("event_delegate.delegate_owner_class_path"), OwnerClass ? OwnerClass->GetPathName() : TEXT(""));
		Request.ContextEvidence.Add(TEXT("event_delegate.delegate_property_name"), DelegateProperty->GetName());
		Request.ContextEvidence.Add(TEXT("event_delegate.delegate_property_path"), DelegateProperty->GetPathName());
		Request.ContextEvidence.Add(TEXT("event_delegate.delegate_signature_function_path"), DelegateProperty->SignatureFunction ? DelegateProperty->SignatureFunction->GetPathName() : TEXT(""));
	}

	if (UFunction* HandlerFunction = AActor::StaticClass()->FindFunctionByName(TEXT("K2_DestroyActor")))
	{
		Request.ContextEvidence.Add(TEXT("event_delegate.handler_name"), TEXT("K2_DestroyActor"));
		Request.ContextEvidence.Add(TEXT("event_delegate.handler_scope_class_path"), AActor::StaticClass()->GetPathName());
		Request.ContextEvidence.Add(TEXT("event_delegate.handler_function_path"), HandlerFunction->GetPathName());
		Request.ContextEvidence.Add(TEXT("event_delegate.handler_source_cluster"), TEXT("BlueprintSignature"));
		Request.ContextEvidence.Add(TEXT("event_delegate.signature_evidence_id"), TEXT("signature:handler:K2_DestroyActor"));
	}

	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_kind"), TEXT("component_ref"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_evidence_id"), TEXT("component_ref:CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_path"), TEXT("CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.operation"), TEXT("bind"));
	return Request;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateEvidenceStableErrorsTest,
	"BlueprintHelper.GraphWrite.EventDelegate.Evidence.StableErrors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateEvidenceStableErrorsTest::RunTest(const FString& Parameters)
{
	auto ExpectError = [this](FBlueprintHelperActionResolutionRequest Request, const FString& Expected)
	{
		FBlueprintHelperEventDelegateUseSiteEvidence Evidence;
		FString Detail;
		FString Message;
		const bool bRead = FBlueprintHelperEventDelegateUseSiteEvidenceReader::TryRead(
			Request,
			Request.Semantic.Kind,
			Evidence,
			Detail,
			Message);
		TestFalse(*FString::Printf(TEXT("%s read fails"), *Expected), bRead);
		TestEqual(*FString::Printf(TEXT("%s detail"), *Expected), Detail, Expected);
	};

	{
		FBlueprintHelperActionResolutionRequest Request = MakeEvidenceRequest();
		Request.ContextEvidence.Remove(TEXT("event_delegate.delegate_property_path"));
		ExpectError(Request, TEXT("missing_delegate_property_evidence"));
	}
	{
		FBlueprintHelperActionResolutionRequest Request = MakeEvidenceRequest();
		Request.ContextEvidence.Remove(TEXT("event_delegate.binding_object_kind"));
		ExpectError(Request, TEXT("missing_binding_object_evidence"));
	}
	{
		FBlueprintHelperActionResolutionRequest Request = MakeEvidenceRequest();
		Request.ContextEvidence.Remove(TEXT("event_delegate.handler_function_path"));
		ExpectError(Request, TEXT("missing_handler_evidence"));
	}
	{
		FBlueprintHelperActionResolutionRequest Request = MakeEvidenceRequest();
		Request.ContextEvidence.Remove(TEXT("event_delegate.signature_evidence_id"));
		ExpectError(Request, TEXT("missing_signature_evidence"));
	}
	{
		FBlueprintHelperActionResolutionRequest Request = MakeEvidenceRequest();
		Request.ContextEvidence.Add(TEXT("event_delegate.operation"), TEXT("unknown"));
		ExpectError(Request, TEXT("invalid_delegate_operation"));
	}
	{
		FBlueprintHelperActionResolutionRequest Request = MakeEvidenceRequest();
		Request.ContextEvidence.Add(TEXT("event_delegate.operation"), TEXT("clear"));
		Request.ContextEvidence.Add(TEXT("event_delegate.unbind_mode"), TEXT("all"));
		ExpectError(Request, TEXT("handler_not_allowed_for_clear"));
	}
	{
		FBlueprintHelperActionResolutionRequest Request = MakeEvidenceRequest();
		Request.ContextEvidence.Add(TEXT("event_delegate.operation"), TEXT("unbind"));
		Request.ContextEvidence.Remove(TEXT("event_delegate.handler_name"));
		ExpectError(Request, TEXT("handler_required_for_unbind"));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateEvidenceCoreFieldGuardTest,
	"BlueprintHelper.GraphWrite.EventDelegate.Evidence.CoreFieldGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateEvidenceCoreFieldGuardTest::RunTest(const FString& Parameters)
{
	const FString CorePath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectPluginsDir() / TEXT("BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"));
	const FString ContextTypesPath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectPluginsDir() / TEXT("BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h"));

	FString CoreText;
	FString ContextText;
	TestTrue(TEXT("read action core"), FFileHelper::LoadFileToString(CoreText, *CorePath));
	TestTrue(TEXT("read action context types"), FFileHelper::LoadFileToString(ContextText, *ContextTypesPath));
	const FString Combined = CoreText + ContextText;
	TestFalse(TEXT("no AssignAutoAttachedEventPolicy core field"), Combined.Contains(TEXT("AssignAutoAttachedEventPolicy")));
	TestFalse(TEXT("no AttachedCustomEventName core field"), Combined.Contains(TEXT("AttachedCustomEventName")));
	return true;
}

#endif
