#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentFacts.h"
#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

class FBlueprintHelperComponentReadbackIdentityTestsLocalUtils
{
public:
	static FString MakeUniqueObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static UBlueprint* MakeActorBlueprint(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperComponentReadback/%s"),
			*MakeUniqueObjectName(Prefix)));
		if (!Package)
		{
			return nullptr;
		}

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*MakeUniqueObjectName(TEXT("BP_ComponentReadback")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperComponentReadbackIdentityTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static UBlueprint* MakeCharacterBlueprint(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperComponentReadback/%s"),
			*MakeUniqueObjectName(Prefix)));
		if (!Package)
		{
			return nullptr;
		}

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ACharacter::StaticClass(),
			Package,
			*MakeUniqueObjectName(TEXT("BP_ComponentReadbackCharacter")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperComponentReadbackIdentityTests"));
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

	static TSharedPtr<FJsonObject> FindFirstNativeComponent(const FBlueprintHelperToolResultBase& Result)
	{
		if (!Result.Data.IsValid())
		{
			return nullptr;
		}

		const TArray<TSharedPtr<FJsonValue>>* Components = nullptr;
		if (!Result.Data->TryGetArrayField(TEXT("components"), Components) || !Components)
		{
			return nullptr;
		}

		for (const TSharedPtr<FJsonValue>& ComponentValue : *Components)
		{
			const TSharedPtr<FJsonObject> ComponentObject = ComponentValue.IsValid()
				? ComponentValue->AsObject()
				: nullptr;
			if (!ComponentObject.IsValid())
			{
				continue;
			}

			bool bIsNative = false;
			if (ComponentObject->TryGetBoolField(TEXT("is_native"), bIsNative) && bIsNative)
			{
				return ComponentObject;
			}
		}

		return nullptr;
	}

	static bool AddComponent(
		FAutomationTestBase& Test,
		FBlueprintHelperComponentService& ComponentService,
		UBlueprint* Blueprint,
		const FString& ComponentName,
		const FString& ComponentClass,
		const FString& ParentComponent = FString())
	{
		FBlueprintHelperAddComponentRequest Request;
		Request.AssetPath = Blueprint ? Blueprint->GetPathName() : FString();
		Request.ComponentName = ComponentName;
		Request.ComponentClass = ComponentClass;
		Request.ParentComponent = ParentComponent;
		Request.NameCollisionPolicy = EBlueprintHelperComponentNameCollisionPolicy::FailIfExists;

		const FBlueprintHelperToolResultBase Result = ComponentService.AddComponent(Request);
		Test.TestTrue(FString::Printf(TEXT("add component %s succeeds"), *ComponentName), Result.bOk);
		Test.TestEqual(FString::Printf(TEXT("add component %s applies"), *ComponentName), Result.Status, EBlueprintHelperToolStatus::Applied);
		return Result.bOk;
	}

	static TSharedPtr<FJsonObject> FindReadbackComponent(
		const FBlueprintHelperToolResultBase& Result,
		const FString& ComponentName)
	{
		if (!Result.Data.IsValid())
		{
			return nullptr;
		}

		const TArray<TSharedPtr<FJsonValue>>* Components = nullptr;
		if (!Result.Data->TryGetArrayField(TEXT("components"), Components) || !Components)
		{
			return nullptr;
		}

		for (const TSharedPtr<FJsonValue>& ComponentValue : *Components)
		{
			const TSharedPtr<FJsonObject> ComponentObject = ComponentValue.IsValid()
				? ComponentValue->AsObject()
				: nullptr;
			if (!ComponentObject.IsValid())
			{
				continue;
			}

			FString FoundName;
			if (ComponentObject->TryGetStringField(TEXT("component_name"), FoundName) && FoundName == ComponentName)
			{
				return ComponentObject;
			}
		}

		return nullptr;
	}

	static bool ReadBoolField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, bool DefaultValue = false)
	{
		bool bValue = DefaultValue;
		if (Object.IsValid())
		{
			Object->TryGetBoolField(FieldName, bValue);
		}
		return bValue;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentReadbackExposesComponentTemplateIdentityTest,
	"BlueprintHelper.Component.Readback.ExposesComponentTemplateIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentReadbackExposesComponentTemplateIdentityTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::MakeActorBlueprint(TEXT("Identity"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperComponentService ComponentService(Resolver);
	if (!FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::AddComponent(
		*this,
		ComponentService,
		Blueprint,
		TEXT("IdentityRoot"),
		TEXT("SceneComponent")))
	{
		return false;
	}

	USCS_Node* Node = FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::FindNodeByName(Blueprint, TEXT("IdentityRoot"));
	TestNotNull(TEXT("IdentityRoot node exists"), Node);
	TestNotNull(TEXT("IdentityRoot template exists"), Node ? Node->ComponentTemplate.Get() : nullptr);
	if (!Node || !Node->ComponentTemplate)
	{
		return false;
	}

	FBlueprintHelperReadComponentsRequest ReadRequest;
	ReadRequest.AssetPath = Blueprint->GetPathName();
	const FBlueprintHelperToolResultBase Result = ComponentService.ReadComponents(ReadRequest);
	TestTrue(TEXT("read components succeeds"), Result.bOk);
	TestTrue(TEXT("read components has data"), Result.Data.IsValid());

	const TSharedPtr<FJsonObject> Component = FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::FindReadbackComponent(Result, TEXT("IdentityRoot"));
	TestTrue(TEXT("IdentityRoot readback exists"), Component.IsValid());
	if (!Component.IsValid())
	{
		return false;
	}

	FString ComponentId;
	FString ComponentTemplatePath;
	FString ClassPath;
	FString Revision;
	FString Fingerprint;
	TestTrue(TEXT("component_id is present"), Component->TryGetStringField(TEXT("component_id"), ComponentId));
	TestTrue(TEXT("component_template_path is present"), Component->TryGetStringField(TEXT("component_template_path"), ComponentTemplatePath));
	TestTrue(TEXT("class_path is present"), Component->TryGetStringField(TEXT("class_path"), ClassPath));
	TestTrue(TEXT("readback_revision is present"), Component->TryGetStringField(TEXT("readback_revision"), Revision));
	TestTrue(TEXT("readback_fingerprint is present"), Component->TryGetStringField(TEXT("readback_fingerprint"), Fingerprint));

	TestTrue(TEXT("component_id includes asset path"), ComponentId.Contains(Blueprint->GetPathName()));
	TestTrue(TEXT("component_id includes SCS component name"), ComponentId.Contains(TEXT("IdentityRoot")));
	TestEqual(TEXT("component_template_path matches template object"), ComponentTemplatePath, Node->ComponentTemplate->GetPathName());
	TestEqual(TEXT("class_path matches template class path"), ClassPath, Node->ComponentTemplate->GetClass()->GetPathName());
	TestFalse(TEXT("readback_revision is non-empty"), Revision.IsEmpty());
	TestFalse(TEXT("readback_fingerprint is non-empty"), Fingerprint.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentReadbackKeepsParentChildrenConsistentTest,
	"BlueprintHelper.Component.Readback.KeepsParentChildrenConsistent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentReadbackKeepsParentChildrenConsistentTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::MakeActorBlueprint(TEXT("Hierarchy"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperComponentService ComponentService(Resolver);
	if (!FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::AddComponent(*this, ComponentService, Blueprint, TEXT("HierarchyRoot"), TEXT("SceneComponent")) ||
		!FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::AddComponent(*this, ComponentService, Blueprint, TEXT("HierarchyMesh"), TEXT("StaticMeshComponent"), TEXT("HierarchyRoot")))
	{
		return false;
	}

	FBlueprintHelperReadComponentsRequest ReadRequest;
	ReadRequest.AssetPath = Blueprint->GetPathName();
	const FBlueprintHelperToolResultBase Result = ComponentService.ReadComponents(ReadRequest);
	TestTrue(TEXT("read components succeeds"), Result.bOk);

	const TSharedPtr<FJsonObject> Root = FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::FindReadbackComponent(Result, TEXT("HierarchyRoot"));
	const TSharedPtr<FJsonObject> Child = FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::FindReadbackComponent(Result, TEXT("HierarchyMesh"));
	TestTrue(TEXT("root readback exists"), Root.IsValid());
	TestTrue(TEXT("child readback exists"), Child.IsValid());
	if (!Root.IsValid() || !Child.IsValid())
	{
		return false;
	}

	FString ChildParent;
	TestTrue(TEXT("child parent field exists"), Child->TryGetStringField(TEXT("parent"), ChildParent));
	TestEqual(TEXT("child parent matches root"), ChildParent, FString(TEXT("HierarchyRoot")));

	const TArray<TSharedPtr<FJsonValue>>* Children = nullptr;
	TestTrue(TEXT("root children field exists"), Root->TryGetArrayField(TEXT("children"), Children));
	TestEqual(TEXT("root has one child"), Children ? Children->Num() : 0, 1);
	if (!Children || Children->Num() != 1)
	{
		return false;
	}
	TestEqual(TEXT("root child name matches child"), (*Children)[0]->AsString(), FString(TEXT("HierarchyMesh")));
	TestTrue(TEXT("relative_transform object is present"), Child->HasTypedField<EJson::Object>(TEXT("relative_transform")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentReadbackExposesOwnershipAndCapabilityFlagsTest,
	"BlueprintHelper.Component.Readback.ExposesOwnershipAndCapabilityFlags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentReadbackExposesOwnershipAndCapabilityFlagsTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::MakeActorBlueprint(TEXT("Flags"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperComponentService ComponentService(Resolver);
	if (!FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::AddComponent(
		*this,
		ComponentService,
		Blueprint,
		TEXT("FlagRoot"),
		TEXT("SceneComponent")))
	{
		return false;
	}

	FBlueprintHelperReadComponentsRequest ReadRequest;
	ReadRequest.AssetPath = Blueprint->GetPathName();
	const FBlueprintHelperToolResultBase Result = ComponentService.ReadComponents(ReadRequest);
	TestTrue(TEXT("read components succeeds"), Result.bOk);

	const TSharedPtr<FJsonObject> Component = FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::FindReadbackComponent(Result, TEXT("FlagRoot"));
	TestTrue(TEXT("FlagRoot readback exists"), Component.IsValid());
	if (!Component.IsValid())
	{
		return false;
	}

	TestTrue(TEXT("owned SCS flag is true"), FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::ReadBoolField(Component, TEXT("is_owned_scs")));
	TestFalse(TEXT("inherited flag is false"), FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::ReadBoolField(Component, TEXT("is_inherited"), true));
	TestFalse(TEXT("native flag is false"), FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::ReadBoolField(Component, TEXT("is_native"), true));
	TestTrue(TEXT("root flag is true"), FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::ReadBoolField(Component, TEXT("is_root")));
	TestFalse(TEXT("default scene root flag is false for custom root"), FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::ReadBoolField(Component, TEXT("is_default_scene_root"), true));
	TestTrue(TEXT("can_rename flag is true"), FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::ReadBoolField(Component, TEXT("can_rename")));
	TestTrue(TEXT("can_reparent flag is true"), FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::ReadBoolField(Component, TEXT("can_reparent")));
	TestTrue(TEXT("can_delete flag is true"), FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::ReadBoolField(Component, TEXT("can_delete")));
	TestTrue(TEXT("selected_defaults object is present"), Component->HasTypedField<EJson::Object>(TEXT("selected_defaults")));

	UBlueprint* CharacterBlueprint = FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::MakeCharacterBlueprint(TEXT("NativeFlags"));
	TestNotNull(TEXT("character Blueprint is created"), CharacterBlueprint);
	if (!CharacterBlueprint)
	{
		return false;
	}

	FBlueprintHelperReadComponentsRequest NativeReadRequest;
	NativeReadRequest.AssetPath = CharacterBlueprint->GetPathName();
	const FBlueprintHelperToolResultBase NativeResult = ComponentService.ReadComponents(NativeReadRequest);
	TestTrue(TEXT("native read components succeeds"), NativeResult.bOk);

	const TSharedPtr<FJsonObject> NativeComponent =
		FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::FindFirstNativeComponent(NativeResult);
	TestTrue(TEXT("native component readback exists"), NativeComponent.IsValid());
	if (!NativeComponent.IsValid())
	{
		return false;
	}

	FString NativeComponentName;
	FString NativeTemplatePath;
	FString NativeClassPath;
	TestTrue(TEXT("native component name is present"), NativeComponent->TryGetStringField(TEXT("component_name"), NativeComponentName));
	TestTrue(TEXT("native template path is present"), NativeComponent->TryGetStringField(TEXT("component_template_path"), NativeTemplatePath));
	TestTrue(TEXT("native class path is present"), NativeComponent->TryGetStringField(TEXT("class_path"), NativeClassPath));
	TestFalse(TEXT("native component name is non-empty"), NativeComponentName.IsEmpty());
	TestFalse(TEXT("native template path is non-empty"), NativeTemplatePath.IsEmpty());
	TestFalse(TEXT("native class path is non-empty"), NativeClassPath.IsEmpty());
	TestFalse(TEXT("native component is not owned SCS"), FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::ReadBoolField(NativeComponent, TEXT("is_owned_scs"), true));
	TestTrue(TEXT("native component is inherited"), FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::ReadBoolField(NativeComponent, TEXT("is_inherited")));
	TestTrue(TEXT("native component flag is true"), FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::ReadBoolField(NativeComponent, TEXT("is_native")));
	TestFalse(TEXT("native component cannot rename"), FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::ReadBoolField(NativeComponent, TEXT("can_rename"), true));
	TestFalse(TEXT("native component cannot reparent"), FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::ReadBoolField(NativeComponent, TEXT("can_reparent"), true));
	TestFalse(TEXT("native component cannot delete"), FBlueprintHelperComponentReadbackIdentityTestsLocalUtils::ReadBoolField(NativeComponent, TEXT("can_delete"), true));
	return true;
}

#endif
