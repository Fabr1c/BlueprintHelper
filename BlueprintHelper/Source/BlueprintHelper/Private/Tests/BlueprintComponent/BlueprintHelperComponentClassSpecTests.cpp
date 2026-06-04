#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentClassResolver.h"
#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"

#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

class FBlueprintHelperComponentClassSpecTestsLocalUtils
{
public:
	static FString MakeUniqueObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static UBlueprint* MakeActorBlueprint(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperComponentClassSpec/%s"),
			*MakeUniqueObjectName(Prefix)));
		if (!Package)
		{
			return nullptr;
		}

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*MakeUniqueObjectName(TEXT("BP_ComponentClassSpec")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperComponentClassSpecTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static UBlueprint* MakeSceneComponentBlueprint(const FString& Prefix)
	{
		const FString BlueprintName = MakeUniqueObjectName(TEXT("BP_ComponentClassSpecScene"));
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperComponentClassSpec/%s"),
			*BlueprintName));
		if (!Package)
		{
			return nullptr;
		}

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			USceneComponent::StaticClass(),
			Package,
			*BlueprintName,
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperComponentClassSpecTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static USCS_Node* FindNodeByName(UBlueprint* Blueprint, const FString& ComponentName)
	{
		if (!Blueprint || !Blueprint->SimpleConstructionScript)
		{
			return nullptr;
		}

		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->GetVariableName().ToString() == ComponentName)
			{
				return Node;
			}
		}
		return nullptr;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentClassSpecResolvesScriptPathTest,
	"BlueprintHelper.Component.ClassSpec.ResolvesScriptPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentClassSpecResolvesScriptPathTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperComponentClassResolveResult ResolveResult;
	TestTrue(TEXT("resolver accepts script component class"),
		FBlueprintHelperComponentClassResolver::ResolveActorComponentClass(
			TEXT("/Script/Engine.SceneComponent"),
			ResolveResult));
	TestEqual(TEXT("resolver returns SceneComponent"),
		ResolveResult.ResolvedClass,
		USceneComponent::StaticClass());

	UBlueprint* Blueprint = FBlueprintHelperComponentClassSpecTestsLocalUtils::MakeActorBlueprint(TEXT("ScriptPath"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperComponentService ComponentService(Resolver);
	FBlueprintHelperAddComponentRequest Request;
	Request.AssetPath = Blueprint->GetPathName();
	Request.ComponentName = TEXT("ScriptPathSceneComponent");
	Request.ComponentClass = TEXT("/Script/Engine.SceneComponent");

	const FBlueprintHelperToolResultBase Result = ComponentService.AddComponent(Request);
	TestTrue(TEXT("add component succeeds with script path"), Result.bOk);
	TestEqual(TEXT("add component applies"), Result.Status, EBlueprintHelperToolStatus::Applied);

	USCS_Node* Node = FBlueprintHelperComponentClassSpecTestsLocalUtils::FindNodeByName(
		Blueprint,
		TEXT("ScriptPathSceneComponent"));
	TestNotNull(TEXT("resolved component node exists"), Node);
	TestEqual(TEXT("node template class is SceneComponent"),
		Node && Node->ComponentTemplate ? Node->ComponentTemplate->GetClass() : nullptr,
		USceneComponent::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentClassSpecResolvesEngineShortNameTest,
	"BlueprintHelper.Component.ClassSpec.ResolvesEngineShortName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentClassSpecResolvesEngineShortNameTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperComponentClassResolveResult ResolveResult;
	TestTrue(TEXT("resolver accepts Engine short name with Component suffix fallback"),
		FBlueprintHelperComponentClassResolver::ResolveActorComponentClass(
			TEXT("Scene"),
			ResolveResult));
	TestEqual(TEXT("resolver returns SceneComponent from short name"),
		ResolveResult.ResolvedClass,
		USceneComponent::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentClassSpecResolvesBlueprintGeneratedClassTest,
	"BlueprintHelper.Component.ClassSpec.ResolvesBlueprintGeneratedClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentClassSpecResolvesBlueprintGeneratedClassTest::RunTest(const FString& Parameters)
{
	UBlueprint* ComponentBlueprint =
		FBlueprintHelperComponentClassSpecTestsLocalUtils::MakeSceneComponentBlueprint(TEXT("GeneratedClass"));
	TestNotNull(TEXT("component Blueprint is created"), ComponentBlueprint);
	UClass* GeneratedClass = ComponentBlueprint ? ComponentBlueprint->GeneratedClass.Get() : nullptr;
	TestNotNull(TEXT("component Blueprint generated class exists"), GeneratedClass);
	if (!ComponentBlueprint || !GeneratedClass)
	{
		return false;
	}

	FBlueprintHelperComponentClassResolveResult ResolveResult;
	TestTrue(TEXT("resolver accepts /Game generated class path"),
		FBlueprintHelperComponentClassResolver::ResolveActorComponentClass(
			GeneratedClass->GetPathName(),
			ResolveResult));
	TestEqual(TEXT("resolver returns generated component class"),
		ResolveResult.ResolvedClass,
		GeneratedClass);

	FBlueprintHelperComponentClassResolveResult PackageOnlyResolveResult;
	TestTrue(TEXT("resolver accepts /Game package path and adds _C candidate"),
		FBlueprintHelperComponentClassResolver::ResolveActorComponentClass(
			ComponentBlueprint->GetOutermost()->GetName(),
			PackageOnlyResolveResult));
	TestEqual(TEXT("resolver returns generated component class from package path"),
		PackageOnlyResolveResult.ResolvedClass,
		GeneratedClass);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentClassSpecRejectsNonActorComponentTest,
	"BlueprintHelper.Component.ClassSpec.RejectsNonActorComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentClassSpecRejectsNonActorComponentTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperComponentClassResolveResult ResolveResult;
	TestFalse(TEXT("resolver rejects AActor class"),
		FBlueprintHelperComponentClassResolver::ResolveActorComponentClass(
			TEXT("/Script/Engine.Actor"),
			ResolveResult));
	TestEqual(TEXT("resolver reports unsupported class"),
		ResolveResult.ErrorCode,
		FString(TEXT("unsupported_component_class")));

	UBlueprint* Blueprint = FBlueprintHelperComponentClassSpecTestsLocalUtils::MakeActorBlueprint(TEXT("RejectNonComponent"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperComponentService ComponentService(Resolver);
	FBlueprintHelperAddComponentRequest Request;
	Request.AssetPath = Blueprint->GetPathName();
	Request.ComponentName = TEXT("InvalidActorComponent");
	Request.ComponentClass = TEXT("/Script/Engine.Actor");

	const FBlueprintHelperToolResultBase Result = ComponentService.AddComponent(Request);
	TestFalse(TEXT("add component rejects non ActorComponent class"), Result.bOk);
	TestEqual(TEXT("error code is unsupported_component_class"),
		Result.Error.IsSet() ? Result.Error->Code : FString(),
		FString(TEXT("unsupported_component_class")));
	TestNull(TEXT("invalid component was not created"),
		FBlueprintHelperComponentClassSpecTestsLocalUtils::FindNodeByName(Blueprint, TEXT("InvalidActorComponent")));

	FBlueprintHelperAddComponentRequest DryRunRequest = Request;
	DryRunRequest.ComponentName = TEXT("InvalidActorComponentDryRun");
	DryRunRequest.bDryRun = true;
	const FBlueprintHelperToolResultBase DryRunResult = ComponentService.AddComponent(DryRunRequest);
	TestFalse(TEXT("preview rejects non ActorComponent class"), DryRunResult.bOk);
	TestEqual(TEXT("preview error code is unsupported_component_class"),
		DryRunResult.Error.IsSet() ? DryRunResult.Error->Code : FString(),
		FString(TEXT("unsupported_component_class")));
	TestNull(TEXT("dry-run invalid component was not created"),
		FBlueprintHelperComponentClassSpecTestsLocalUtils::FindNodeByName(Blueprint, TEXT("InvalidActorComponentDryRun")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentClassSpecRejectsNonSceneParentAttachmentTest,
	"BlueprintHelper.Component.ClassSpec.RejectsNonSceneParentAttachment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentClassSpecRejectsNonSceneParentAttachmentTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperComponentClassSpecTestsLocalUtils::MakeActorBlueprint(TEXT("RejectParent"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperComponentService ComponentService(Resolver);
	FBlueprintHelperAddComponentRequest ParentRequest;
	ParentRequest.AssetPath = Blueprint->GetPathName();
	ParentRequest.ComponentName = TEXT("NonSceneParent");
	ParentRequest.ComponentClass = TEXT("/Script/Engine.ApplicationLifecycleComponent");
	const FBlueprintHelperToolResultBase ParentResult = ComponentService.AddComponent(ParentRequest);
	TestTrue(TEXT("non-scene ActorComponent subclass can be added as unparented component"), ParentResult.bOk);

	FBlueprintHelperAddComponentRequest ChildRequest;
	ChildRequest.AssetPath = Blueprint->GetPathName();
	ChildRequest.ComponentName = TEXT("SceneChild");
	ChildRequest.ComponentClass = TEXT("/Script/Engine.SceneComponent");
	ChildRequest.ParentComponent = TEXT("NonSceneParent");

	const FBlueprintHelperToolResultBase ChildResult = ComponentService.AddComponent(ChildRequest);
	TestFalse(TEXT("scene child rejects non-scene parent"), ChildResult.bOk);
	TestEqual(TEXT("error code is component_parent_not_scene"),
		ChildResult.Error.IsSet() ? ChildResult.Error->Code : FString(),
		FString(TEXT("component_parent_not_scene")));
	TestNull(TEXT("scene child was not created"),
		FBlueprintHelperComponentClassSpecTestsLocalUtils::FindNodeByName(Blueprint, TEXT("SceneChild")));
	return true;
}

#endif
