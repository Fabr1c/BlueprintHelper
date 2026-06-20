#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SceneComponent.h"
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
#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "UObject/Package.h"

class FBlueprintHelperComponentMutationHierarchyTestsLocalUtils
{
public:
	static FString MakeUniqueObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static UBlueprint* MakeActorBlueprint(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperComponentHierarchy/%s"),
			*MakeUniqueObjectName(Prefix)));
		if (!Package)
		{
			return nullptr;
		}

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*MakeUniqueObjectName(TEXT("BP_ComponentHierarchy")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperComponentMutationHierarchyTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static UBlueprint* MakeCharacterBlueprint(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperComponentHierarchy/%s"),
			*MakeUniqueObjectName(Prefix)));
		if (!Package)
		{
			return nullptr;
		}

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ACharacter::StaticClass(),
			Package,
			*MakeUniqueObjectName(TEXT("BP_ComponentHierarchyCharacter")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperComponentMutationHierarchyTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static UBlueprint* MakeChildActorBlueprint(UBlueprint* ParentBlueprint, const FString& Prefix)
	{
		if (!ParentBlueprint || !ParentBlueprint->GeneratedClass)
		{
			return nullptr;
		}

		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperComponentHierarchy/%s"),
			*MakeUniqueObjectName(Prefix)));
		if (!Package)
		{
			return nullptr;
		}

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentBlueprint->GeneratedClass,
			Package,
			*MakeUniqueObjectName(TEXT("BP_ComponentHierarchyChild")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperComponentMutationHierarchyTests"));
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

	static bool AddComponent(
		FAutomationTestBase& Test,
		FBlueprintHelperComponentService& ComponentService,
		UBlueprint* Blueprint,
		const FString& ComponentName,
		const FString& ComponentClass = TEXT("SceneComponent"),
		const FString& ParentComponent = TEXT(""))
	{
		FBlueprintHelperAddComponentRequest Request;
		Request.AssetPath = Blueprint ? Blueprint->GetPathName() : TEXT("");
		Request.ComponentName = ComponentName;
		Request.ComponentClass = ComponentClass;
		Request.ParentComponent = ParentComponent;
		const FBlueprintHelperToolResultBase Result = ComponentService.AddComponent(Request);
		Test.TestTrue(FString::Printf(TEXT("add component succeeds: %s"), *ComponentName), Result.bOk);
		return Result.bOk;
	}

	static TSharedPtr<FJsonObject> GetResultComponent(const FBlueprintHelperToolResultBase& Result)
	{
		const TSharedPtr<FJsonObject>* Component = nullptr;
		if (Result.Data.IsValid() && Result.Data->TryGetObjectField(TEXT("component"), Component) && Component)
		{
			return *Component;
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
			const TSharedPtr<FJsonObject> Component = ComponentValue.IsValid()
				? ComponentValue->AsObject()
				: nullptr;
			if (!Component.IsValid())
			{
				continue;
			}

			bool bIsNative = false;
			if (Component->TryGetBoolField(TEXT("is_native"), bIsNative) && bIsNative)
			{
				return Component;
			}
		}

		return nullptr;
	}

	static FString ReadComponentName(const TSharedPtr<FJsonObject>& Component)
	{
		FString ComponentName;
		if (Component.IsValid())
		{
			Component->TryGetStringField(TEXT("component_name"), ComponentName);
		}
		return ComponentName;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentHierarchyRenamesOwnedComponentTest,
	"BlueprintHelper.Component.Hierarchy.RenamesOwnedComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentHierarchyRenamesOwnedComponentTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::MakeActorBlueprint(TEXT("Rename"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperComponentService ComponentService(Resolver);
	if (!FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::AddComponent(*this, ComponentService, Blueprint, TEXT("RenameSource")))
	{
		return false;
	}

	FBlueprintHelperRenameComponentRequest DryRunRequest;
	DryRunRequest.AssetPath = Blueprint->GetPathName();
	DryRunRequest.ComponentName = TEXT("RenameSource");
	DryRunRequest.NewComponentName = TEXT("RenameTargetPreview");
	DryRunRequest.bDryRun = true;
	const FBlueprintHelperToolResultBase DryRunResult = ComponentService.RenameComponent(DryRunRequest);
	TestTrue(TEXT("rename dry-run succeeds"), DryRunResult.bOk);
	TestNotNull(TEXT("dry-run does not create target node"),
		FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::FindNodeByName(Blueprint, TEXT("RenameSource")));
	TestNull(TEXT("dry-run leaves new name absent"),
		FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::FindNodeByName(Blueprint, TEXT("RenameTargetPreview")));

	FBlueprintHelperRenameComponentRequest Request;
	Request.AssetPath = Blueprint->GetPathName();
	Request.ComponentName = TEXT("RenameSource");
	Request.NewComponentName = TEXT("RenameTarget");
	const FBlueprintHelperToolResultBase Result = ComponentService.RenameComponent(Request);
	TestTrue(TEXT("rename execute succeeds"), Result.bOk);
	TestNull(TEXT("old component name removed"),
		FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::FindNodeByName(Blueprint, TEXT("RenameSource")));
	TestNotNull(TEXT("new component name exists"),
		FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::FindNodeByName(Blueprint, TEXT("RenameTarget")));

	const TSharedPtr<FJsonObject>* BeforeComponent = nullptr;
	const TSharedPtr<FJsonObject>* AfterComponent = nullptr;
	TestTrue(TEXT("rename emits before_component"),
		Result.Data.IsValid() && Result.Data->TryGetObjectField(TEXT("before_component"), BeforeComponent));
	TestTrue(TEXT("rename emits after_component"),
		Result.Data.IsValid() && Result.Data->TryGetObjectField(TEXT("after_component"), AfterComponent));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentHierarchyReparentsOwnedComponentTest,
	"BlueprintHelper.Component.Hierarchy.ReparentsOwnedComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentHierarchyReparentsOwnedComponentTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::MakeActorBlueprint(TEXT("Reparent"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperComponentService ComponentService(Resolver);
	if (!FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::AddComponent(*this, ComponentService, Blueprint, TEXT("ParentA"), TEXT("SceneComponent"), TEXT("DefaultSceneRoot")) ||
		!FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::AddComponent(*this, ComponentService, Blueprint, TEXT("ParentB"), TEXT("SceneComponent"), TEXT("DefaultSceneRoot")) ||
		!FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::AddComponent(*this, ComponentService, Blueprint, TEXT("ChildMesh"), TEXT("StaticMeshComponent"), TEXT("ParentA")))
	{
		return false;
	}

	FBlueprintHelperReparentComponentRequest Request;
	Request.AssetPath = Blueprint->GetPathName();
	Request.ComponentName = TEXT("ChildMesh");
	Request.NewParentComponent = TEXT("ParentB");
	Request.SocketName = TEXT("DoorSocket");
	Request.AttachRule = EBlueprintHelperAttachRule::SnapToTarget;
	Request.TransformPolicy = EBlueprintHelperComponentTransformPolicy::ResetRelative;
	const FBlueprintHelperToolResultBase Result = ComponentService.ReparentComponent(Request);
	TestTrue(TEXT("reparent execute succeeds"), Result.bOk);

	USCS_Node* ChildNode = FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::FindNodeByName(Blueprint, TEXT("ChildMesh"));
	USCS_Node* ParentBNode = FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::FindNodeByName(Blueprint, TEXT("ParentB"));
	TestNotNull(TEXT("child still exists"), ChildNode);
	TestEqual(TEXT("child parent is updated"), Blueprint->SimpleConstructionScript->FindParentNode(ChildNode), ParentBNode);
	TestEqual(TEXT("socket name is updated"), ChildNode ? ChildNode->AttachToName.ToString() : FString(), FString(TEXT("DoorSocket")));

	const TSharedPtr<FJsonObject>* HierarchyChange = nullptr;
	TestTrue(TEXT("reparent emits hierarchy_change"),
		Result.Data.IsValid() && Result.Data->TryGetObjectField(TEXT("hierarchy_change"), HierarchyChange));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentHierarchySetRootPromotesRootTest,
	"BlueprintHelper.Component.Hierarchy.SetRootPromotesRoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentHierarchySetRootPromotesRootTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::MakeActorBlueprint(TEXT("SetRoot"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperComponentService ComponentService(Resolver);
	if (!FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::AddComponent(*this, ComponentService, Blueprint, TEXT("NewRoot"), TEXT("SceneComponent"), TEXT("DefaultSceneRoot")))
	{
		return false;
	}

	FBlueprintHelperSetRootComponentRequest Request;
	Request.AssetPath = Blueprint->GetPathName();
	Request.ComponentName = TEXT("NewRoot");
	Request.OldRootPolicy = EBlueprintHelperComponentOldRootPolicy::RemoveDefaultSceneRootWhenEmpty;
	const FBlueprintHelperToolResultBase Result = ComponentService.SetRootComponent(Request);
	TestTrue(TEXT("set root succeeds"), Result.bOk);

	USCS_Node* NewRootNode = FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::FindNodeByName(Blueprint, TEXT("NewRoot"));
	TestNotNull(TEXT("new root node exists"), NewRootNode);
	TestNull(TEXT("new root has no parent"),
		NewRootNode && Blueprint->SimpleConstructionScript ? Blueprint->SimpleConstructionScript->FindParentNode(NewRootNode) : nullptr);

	int32 RootCount = 0;
	for (USCS_Node* RootNode : Blueprint->SimpleConstructionScript->GetRootNodes())
	{
		if (RootNode)
		{
			++RootCount;
		}
	}
	TestEqual(TEXT("exactly one root remains"), RootCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentHierarchyRemovePolicyBlocksWhenChildrenExistTest,
	"BlueprintHelper.Component.Hierarchy.RemovePolicyBlocksWhenChildrenExist",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentHierarchyRemovePolicyBlocksWhenChildrenExistTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::MakeActorBlueprint(TEXT("RemoveBlock"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperComponentService ComponentService(Resolver);
	if (!FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::AddComponent(*this, ComponentService, Blueprint, TEXT("RemoveParent"), TEXT("SceneComponent"), TEXT("DefaultSceneRoot")) ||
		!FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::AddComponent(*this, ComponentService, Blueprint, TEXT("RemoveChild"), TEXT("SceneComponent"), TEXT("RemoveParent")))
	{
		return false;
	}

	FBlueprintHelperRemoveComponentRequest Request;
	Request.AssetPath = Blueprint->GetPathName();
	Request.ComponentName = TEXT("RemoveParent");
	const FBlueprintHelperToolResultBase Result = ComponentService.RemoveComponent(Request);
	TestFalse(TEXT("block_if_children rejects parent remove"), Result.bOk);
	TestTrue(TEXT("remove_component_blocked error"),
		Result.Error.IsSet() && Result.Error->Code == TEXT("remove_component_blocked"));
	TestNotNull(TEXT("parent remains"),
		FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::FindNodeByName(Blueprint, TEXT("RemoveParent")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentHierarchyRemovePolicyPromotesChildrenTest,
	"BlueprintHelper.Component.Hierarchy.RemovePolicyPromotesChildren",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentHierarchyRemovePolicyPromotesChildrenTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::MakeActorBlueprint(TEXT("RemovePromote"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperComponentService ComponentService(Resolver);
	if (!FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::AddComponent(*this, ComponentService, Blueprint, TEXT("PromoteParent"), TEXT("SceneComponent"), TEXT("DefaultSceneRoot")) ||
		!FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::AddComponent(*this, ComponentService, Blueprint, TEXT("PromoteChild"), TEXT("SceneComponent"), TEXT("PromoteParent")))
	{
		return false;
	}

	FBlueprintHelperRemoveComponentRequest Request;
	Request.AssetPath = Blueprint->GetPathName();
	Request.ComponentName = TEXT("PromoteParent");
	Request.DeletePolicy = EBlueprintHelperComponentDeletePolicy::PromoteChildren;
	const FBlueprintHelperToolResultBase Result = ComponentService.RemoveComponent(Request);
	TestTrue(TEXT("promote_children remove succeeds"), Result.bOk);
	TestNull(TEXT("removed parent no longer exists"),
		FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::FindNodeByName(Blueprint, TEXT("PromoteParent")));
	USCS_Node* ChildNode = FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::FindNodeByName(Blueprint, TEXT("PromoteChild"));
	USCS_Node* DefaultRootNode = FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::FindNodeByName(Blueprint, TEXT("DefaultSceneRoot"));
	TestNotNull(TEXT("child remains"), ChildNode);
	TestEqual(TEXT("child moved to removed parent parent"),
		Blueprint->SimpleConstructionScript->FindParentNode(ChildNode),
		DefaultRootNode);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentHierarchyExistingMismatchBlocksTest,
	"BlueprintHelper.Component.Hierarchy.ExistingMismatchBlocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentHierarchyExistingMismatchBlocksTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::MakeActorBlueprint(TEXT("ExistingMismatch"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperComponentService ComponentService(Resolver);
	if (!FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::AddComponent(
		*this,
		ComponentService,
		Blueprint,
		TEXT("MismatchComponent"),
		TEXT("SceneComponent")))
	{
		return false;
	}

	FBlueprintHelperAddComponentRequest Request;
	Request.AssetPath = Blueprint->GetPathName();
	Request.ComponentName = TEXT("MismatchComponent");
	Request.ComponentClass = TEXT("StaticMeshComponent");
	Request.NameCollisionPolicy = EBlueprintHelperComponentNameCollisionPolicy::BlockIfClassMismatch;
	Request.bDryRun = true;
	const FBlueprintHelperToolResultBase Result = ComponentService.AddComponent(Request);

	TestFalse(TEXT("existing class mismatch is blocked"), Result.bOk);
	TestTrue(TEXT("component_existing_mismatch error is reported"),
		Result.Error.IsSet() && Result.Error->Code == TEXT("component_existing_mismatch"));

	const TSharedPtr<FJsonObject>* NameCollision = nullptr;
	TestTrue(TEXT("name collision evidence is present"),
		Result.Data.IsValid() && Result.Data->TryGetObjectField(TEXT("name_collision"), NameCollision));
	if (NameCollision && NameCollision->IsValid())
	{
		FString ExistingClass;
		FString ExpectedClass;
		TestTrue(TEXT("existing class evidence present"), (*NameCollision)->TryGetStringField(TEXT("existing_component_class"), ExistingClass));
		TestTrue(TEXT("expected class evidence present"), (*NameCollision)->TryGetStringField(TEXT("expected_component_class"), ExpectedClass));
		TestTrue(TEXT("existing class names SceneComponent"), ExistingClass.Contains(TEXT("SceneComponent")));
		TestTrue(TEXT("expected class names StaticMeshComponent"), ExpectedClass.Contains(TEXT("StaticMeshComponent")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentHierarchyBlocksInheritedNativeMutationTest,
	"BlueprintHelper.Component.Hierarchy.BlocksInheritedNativeMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentHierarchyBlocksInheritedNativeMutationTest::RunTest(const FString& Parameters)
{
	UBlueprint* ParentBlueprint = FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::MakeActorBlueprint(TEXT("OwnedGateParent"));
	TestNotNull(TEXT("parent Blueprint is created"), ParentBlueprint);
	if (!ParentBlueprint)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperComponentService ComponentService(Resolver);
	if (!FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::AddComponent(
		*this,
		ComponentService,
		ParentBlueprint,
		TEXT("ParentOwnedComponent"),
		TEXT("SceneComponent")))
	{
		return false;
	}

	UBlueprint* ChildBlueprint = FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::MakeChildActorBlueprint(
		ParentBlueprint,
		TEXT("OwnedGateChild"));
	TestNotNull(TEXT("child Blueprint is created"), ChildBlueprint);
	if (!ChildBlueprint)
	{
		return false;
	}

	FBlueprintHelperRenameComponentRequest Request;
	Request.AssetPath = ChildBlueprint->GetPathName();
	Request.ComponentName = TEXT("ParentOwnedComponent");
	Request.NewComponentName = TEXT("RenamedParentOwnedComponent");
	Request.bDryRun = true;
	const FBlueprintHelperToolResultBase Result = ComponentService.RenameComponent(Request);
	TestFalse(TEXT("inherited component rename is blocked by readback facts"), Result.bOk);
	TestTrue(TEXT("not owned SCS error is reported"),
		Result.Error.IsSet() && Result.Error->Code == TEXT("component_not_owned_scs"));

	const TSharedPtr<FJsonObject> Component = FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::GetResultComponent(Result);
	TestTrue(TEXT("blocked component evidence is present"), Component.IsValid());
	if (Component.IsValid())
	{
		bool bIsOwnedSCS = true;
		bool bIsInherited = false;
		TestTrue(TEXT("component evidence carries owned flag"), Component->TryGetBoolField(TEXT("is_owned_scs"), bIsOwnedSCS));
		TestTrue(TEXT("component evidence carries inherited flag"), Component->TryGetBoolField(TEXT("is_inherited"), bIsInherited));
		TestFalse(TEXT("inherited component is not owned SCS"), bIsOwnedSCS);
		TestTrue(TEXT("inherited flag is true"), bIsInherited);
	}

	UBlueprint* CharacterBlueprint = FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::MakeCharacterBlueprint(TEXT("NativeGate"));
	TestNotNull(TEXT("character Blueprint is created"), CharacterBlueprint);
	if (!CharacterBlueprint)
	{
		return false;
	}

	FBlueprintHelperReadComponentsRequest ReadRequest;
	ReadRequest.AssetPath = CharacterBlueprint->GetPathName();
	const FBlueprintHelperToolResultBase ReadResult = ComponentService.ReadComponents(ReadRequest);
	TestTrue(TEXT("native read components succeeds"), ReadResult.bOk);

	const TSharedPtr<FJsonObject> NativeReadback =
		FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::FindFirstNativeComponent(ReadResult);
	TestTrue(TEXT("native component readback exists"), NativeReadback.IsValid());
	if (!NativeReadback.IsValid())
	{
		return false;
	}

	FString NativeComponentName;
	TestTrue(TEXT("native component name is present"), NativeReadback->TryGetStringField(TEXT("component_name"), NativeComponentName));
	TestFalse(TEXT("native component name is non-empty"), NativeComponentName.IsEmpty());

	FBlueprintHelperRenameComponentRequest NativeRenameRequest;
	NativeRenameRequest.AssetPath = CharacterBlueprint->GetPathName();
	NativeRenameRequest.ComponentName = NativeComponentName;
	NativeRenameRequest.NewComponentName = TEXT("RenamedNativeComponent");
	NativeRenameRequest.bDryRun = true;
	const FBlueprintHelperToolResultBase NativeRenameResult = ComponentService.RenameComponent(NativeRenameRequest);
	TestFalse(TEXT("native component rename is blocked by readback facts"), NativeRenameResult.bOk);
	TestTrue(TEXT("native not owned SCS error is reported"),
		NativeRenameResult.Error.IsSet() && NativeRenameResult.Error->Code == TEXT("component_not_owned_scs"));
	TestTrue(TEXT("native structural mutation reports blocked boundary"),
		NativeRenameResult.Error.IsSet() && NativeRenameResult.Error->BlockedBoundary.IsSet());
	if (NativeRenameResult.Error.IsSet() && NativeRenameResult.Error->BlockedBoundary.IsSet())
	{
		TestEqual(TEXT("blocked boundary id"),
			NativeRenameResult.Error->BlockedBoundary->BoundaryId,
			FString(TEXT("component_tree_owned_scs_only")));
		TestEqual(TEXT("blocked native origin"),
			NativeRenameResult.Error->BlockedBoundary->Origin,
			FString(TEXT("native")));
		TestEqual(TEXT("blocked structural operation"),
			NativeRenameResult.Error->BlockedBoundary->BlockedOperation,
			FString(TEXT("rename_component")));
		TestFalse(TEXT("structural mutation does not suggest class-default route"),
			NativeRenameResult.Error->SuggestedRoute.IsSet());
	}

	const TSharedPtr<FJsonObject> NativeComponent =
		FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::GetResultComponent(NativeRenameResult);
	TestTrue(TEXT("blocked native component evidence is present"), NativeComponent.IsValid());
	if (NativeComponent.IsValid())
	{
		bool bIsOwnedSCS = true;
		bool bIsInherited = false;
		bool bIsNative = false;
		TestTrue(TEXT("native evidence carries owned flag"), NativeComponent->TryGetBoolField(TEXT("is_owned_scs"), bIsOwnedSCS));
		TestTrue(TEXT("native evidence carries inherited flag"), NativeComponent->TryGetBoolField(TEXT("is_inherited"), bIsInherited));
		TestTrue(TEXT("native evidence carries native flag"), NativeComponent->TryGetBoolField(TEXT("is_native"), bIsNative));
		TestFalse(TEXT("native component is not owned SCS"), bIsOwnedSCS);
		TestTrue(TEXT("native inherited flag is true"), bIsInherited);
		TestTrue(TEXT("native flag is true"), bIsNative);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentHierarchySuggestsClassDefaultRouteForNativePropertyTest,
	"BlueprintHelper.Component.Hierarchy.SuggestsClassDefaultRouteForNativeProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentHierarchySuggestsClassDefaultRouteForNativePropertyTest::RunTest(const FString& Parameters)
{
	UBlueprint* CharacterBlueprint = FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::MakeCharacterBlueprint(TEXT("NativePropertyRoute"));
	TestNotNull(TEXT("character Blueprint is created"), CharacterBlueprint);
	if (!CharacterBlueprint)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperComponentService ComponentService(Resolver);

	FBlueprintHelperReadComponentsRequest ReadRequest;
	ReadRequest.AssetPath = CharacterBlueprint->GetPathName();
	const FBlueprintHelperToolResultBase ReadResult = ComponentService.ReadComponents(ReadRequest);
	TestTrue(TEXT("native read components succeeds"), ReadResult.bOk);

	const TSharedPtr<FJsonObject> NativeReadback =
		FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::FindFirstNativeComponent(ReadResult);
	TestTrue(TEXT("native component readback exists"), NativeReadback.IsValid());
	const FString NativeComponentName =
		FBlueprintHelperComponentMutationHierarchyTestsLocalUtils::ReadComponentName(NativeReadback);
	TestFalse(TEXT("native component name is non-empty"), NativeComponentName.IsEmpty());
	if (NativeComponentName.IsEmpty())
	{
		return false;
	}

	FBlueprintHelperSetComponentPropertiesRequest Request;
	Request.AssetPath = CharacterBlueprint->GetPathName();
	Request.ComponentName = NativeComponentName;
	Request.bDryRun = true;
	FBlueprintHelperComponentPropertySetting Setting;
	Setting.PropertyPath = TEXT("bAutoActivate");
	Setting.Value = MakeShared<FJsonValueBoolean>(false);
	Request.Settings.Add(Setting);

	const FBlueprintHelperToolResultBase Result = ComponentService.SetComponentProperties(Request);
	TestFalse(TEXT("native component property mutation is blocked"), Result.bOk);
	TestTrue(TEXT("native component property reports owned-SCS boundary"),
		Result.Error.IsSet() && Result.Error->Code == TEXT("component_not_owned_scs"));
	TestTrue(TEXT("native component property includes suggested route"),
		Result.Error.IsSet() && Result.Error->SuggestedRoute.IsSet());
	TestTrue(TEXT("native component property includes blocked boundary"),
		Result.Error.IsSet() && Result.Error->BlockedBoundary.IsSet());

	if (Result.Error.IsSet() && Result.Error->SuggestedRoute.IsSet())
	{
		const FString ExpectedPathHint = NativeComponentName + TEXT(".bAutoActivate");
		TestEqual(TEXT("route id"),
			Result.Error->SuggestedRoute->RouteId,
			FString(TEXT("blueprint_class_settings.class_default")));
		TestEqual(TEXT("route family"),
			Result.Error->SuggestedRoute->Family,
			FString(TEXT("blueprint_class_settings")));
		TestEqual(TEXT("route write mode"),
			Result.Error->SuggestedRoute->WriteMode,
			FString(TEXT("class_settings.edit")));
		TestEqual(TEXT("route cluster"),
			Result.Error->SuggestedRoute->ClusterId,
			FString(TEXT("class_settings")));
		TestEqual(TEXT("route operation"),
			Result.Error->SuggestedRoute->OperationId,
			FString(TEXT("set_class_default")));
		TestEqual(TEXT("route template"),
			Result.Error->SuggestedRoute->TemplateId,
			FString(TEXT("blueprint_class_settings_class_default")));
		TestEqual(TEXT("route task type"),
			Result.Error->SuggestedRoute->TaskType,
			FString(TEXT("edit_blueprint_class_settings")));
		TestEqual(TEXT("route reason"),
			Result.Error->SuggestedRoute->Reason,
			FString(TEXT("native_component_default_property")));
		TestEqual(TEXT("route applies_when"),
			Result.Error->SuggestedRoute->AppliesWhen,
			FString(TEXT("intent_is_default_property_write")));
		TestEqual(TEXT("route property path hint"),
			Result.Error->SuggestedRoute->PropertyPathHint,
			ExpectedPathHint);
		TestEqual(TEXT("route property path hints count"),
			Result.Error->SuggestedRoute->PropertyPathHints.Num(),
			1);
		if (Result.Error->SuggestedRoute->PropertyPathHints.Num() > 0)
		{
			TestEqual(TEXT("route first property path hint"),
				Result.Error->SuggestedRoute->PropertyPathHints[0],
				ExpectedPathHint);
		}
	}

	const TSharedPtr<FJsonObject>* DataSuggestedRoute = nullptr;
	TestTrue(TEXT("data includes suggested_route"),
		Result.Data.IsValid() && Result.Data->TryGetObjectField(TEXT("suggested_route"), DataSuggestedRoute));
	const TSharedPtr<FJsonObject>* DataBlockedBoundary = nullptr;
	TestTrue(TEXT("data includes blocked_boundary"),
		Result.Data.IsValid() && Result.Data->TryGetObjectField(TEXT("blocked_boundary"), DataBlockedBoundary));
	return true;
}

#endif
