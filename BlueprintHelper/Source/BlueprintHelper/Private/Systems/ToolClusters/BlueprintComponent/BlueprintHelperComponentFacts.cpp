#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentFacts.h"

#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "Misc/SecureHash.h"

class FBlueprintHelperComponentFactsLocalUtils
{
public:
	static TSharedRef<FJsonObject> MakeVectorJson(const FVector& Vector)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("x"), Vector.X);
		Json->SetNumberField(TEXT("y"), Vector.Y);
		Json->SetNumberField(TEXT("z"), Vector.Z);
		return Json;
	}

	static TSharedRef<FJsonObject> MakeRotatorJson(const FRotator& Rotator)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("pitch"), Rotator.Pitch);
		Json->SetNumberField(TEXT("yaw"), Rotator.Yaw);
		Json->SetNumberField(TEXT("roll"), Rotator.Roll);
		return Json;
	}

	static TSharedRef<FJsonObject> MakeTransformJson(const UActorComponent* ComponentTemplate)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		const USceneComponent* SceneComponent = Cast<USceneComponent>(ComponentTemplate);
		const FTransform Transform = SceneComponent ? SceneComponent->GetRelativeTransform() : FTransform::Identity;
		Json->SetObjectField(TEXT("location"), MakeVectorJson(Transform.GetLocation()));
		Json->SetObjectField(TEXT("rotation"), MakeRotatorJson(Transform.Rotator()));
		Json->SetObjectField(TEXT("scale"), MakeVectorJson(Transform.GetScale3D()));
		return Json;
	}

	static TSharedRef<FJsonObject> MakeSelectedDefaultsJson(const UActorComponent* ComponentTemplate)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		if (!ComponentTemplate)
		{
			return Json;
		}

		Json->SetStringField(TEXT("component_class"), ComponentTemplate->GetClass()->GetPathName());
		if (const USceneComponent* SceneComponent = Cast<USceneComponent>(ComponentTemplate))
		{
			Json->SetObjectField(TEXT("relative_transform"), MakeTransformJson(SceneComponent));
			Json->SetStringField(TEXT("mobility"), MobilityToString(SceneComponent->Mobility));
		}
		return Json;
	}

	static FString GetShortComponentClassName(const UClass* ComponentClass)
	{
		if (!ComponentClass)
		{
			return TEXT("");
		}

		FString Name = ComponentClass->GetName();
		Name.RemoveFromEnd(TEXT("_C"));
		return Name;
	}

	static const TCHAR* MobilityToString(EComponentMobility::Type Mobility)
	{
		switch (Mobility)
		{
		case EComponentMobility::Static:
			return TEXT("static");
		case EComponentMobility::Stationary:
			return TEXT("stationary");
		case EComponentMobility::Movable:
			return TEXT("movable");
		default:
			return TEXT("unknown");
		}
	}

	static FString MakeComponentId(const UBlueprint& Blueprint, const USCS_Node& Node)
	{
		const USimpleConstructionScript* OwningSCS = Node.GetSCS();
		const UObject* Origin = OwningSCS ? OwningSCS->GetOuter() : nullptr;
		const FString OriginPath = Origin ? Origin->GetPathName() : Blueprint.GetPathName();
		return FString::Printf(TEXT("%s::SCS::%s"), *OriginPath, *Node.GetVariableName().ToString());
	}

	static FString MakeNativeComponentId(const UBlueprint& Blueprint, const AActor& NativeOwner, const UActorComponent& Component)
	{
		const UClass* OwnerClass = NativeOwner.GetClass();
		const FString OriginPath = OwnerClass ? OwnerClass->GetPathName() : Blueprint.GetPathName();
		return FString::Printf(TEXT("%s::Native::%s"), *OriginPath, *Component.GetFName().ToString());
	}

	static FString MakeFingerprint(const FBlueprintHelperComponentInfo& Info)
	{
		FString Source;
		Source += Info.ComponentId;
		Source += TEXT("|");
		Source += Info.ClassPath;
		Source += TEXT("|");
		Source += Info.ComponentTemplatePath;
		Source += TEXT("|");
		Source += Info.ParentComponent;
		Source += TEXT("|");
		Source += Info.SocketName;
		Source += TEXT("|");
		Source += Info.bIsRoot ? TEXT("root") : TEXT("non_root");
		Source += TEXT("|");
		Source += Info.bIsDefaultSceneRoot ? TEXT("default_root") : TEXT("custom_component");
		Source += TEXT("|");
		Source += Info.bIsOwnedSCS ? TEXT("owned_scs") : TEXT("not_owned_scs");
		Source += TEXT("|");
		Source += Info.bIsInherited ? TEXT("inherited") : TEXT("local");
		Source += TEXT("|");
		Source += Info.bIsNative ? TEXT("native") : TEXT("scs");
		for (const FString& Child : Info.Children)
		{
			Source += TEXT("|child:");
			Source += Child;
		}
		return FMD5::HashAnsiString(*Source);
	}

	static bool IsRootNode(const UBlueprint& Blueprint, const USCS_Node& Node)
	{
		const USimpleConstructionScript* SCS = Node.GetSCS();
		if (!SCS)
		{
			return false;
		}

		for (const USCS_Node* RootNode : SCS->GetRootNodes())
		{
			if (RootNode == &Node)
			{
				return true;
			}
		}
		return false;
	}

	static FString GetParentName(const UBlueprint& Blueprint, const USCS_Node& Node)
	{
		const USimpleConstructionScript* SCS = Node.GetSCS();
		if (!SCS)
		{
			return TEXT("");
		}

		const USCS_Node* ParentNode = SCS->FindParentNode(const_cast<USCS_Node*>(&Node));
		return ParentNode ? ParentNode->GetVariableName().ToString() : TEXT("");
	}

	static bool IsDefaultSceneRoot(const UBlueprint& Blueprint, const USCS_Node& Node)
	{
		const USimpleConstructionScript* SCS = Node.GetSCS();
		if (!SCS)
		{
			return false;
		}

		const USCS_Node* DefaultSceneRootNode = SCS->GetDefaultSceneRootNode();
		return DefaultSceneRootNode == &Node;
	}

	static void AddUniqueNodeFacts(
		const UBlueprint& TargetBlueprint,
		const TArray<USCS_Node*>& Nodes,
		TSet<const USCS_Node*>& SeenNodes,
		TSet<FString>& SeenComponentNames,
		TArray<FBlueprintHelperComponentInfo>& OutFacts)
	{
		for (USCS_Node* Node : Nodes)
		{
			if (!Node || !Node->ComponentTemplate || SeenNodes.Contains(Node))
			{
				continue;
			}
			SeenNodes.Add(Node);
			FBlueprintHelperComponentInfo Info = FBlueprintHelperComponentFacts::BuildReadbackFact(TargetBlueprint, *Node);
			SeenComponentNames.Add(Info.ComponentName);
			OutFacts.Add(MoveTemp(Info));
		}
	}

	static void AddNativeComponentFacts(
		const UBlueprint& TargetBlueprint,
		const UClass* NativeActorClass,
		const TSet<FString>& SeenComponentNames,
		TArray<FBlueprintHelperComponentInfo>& OutFacts)
	{
		const AActor* NativeOwner = NativeActorClass
			? Cast<AActor>(NativeActorClass->GetDefaultObject())
			: nullptr;
		if (!NativeOwner)
		{
			return;
		}

		TArray<UActorComponent*> NativeComponents;
		NativeOwner->GetComponents(NativeComponents);
		for (UActorComponent* Component : NativeComponents)
		{
			if (!Component || SeenComponentNames.Contains(Component->GetFName().ToString()))
			{
				continue;
			}
			OutFacts.Add(FBlueprintHelperComponentFacts::BuildNativeReadbackFact(TargetBlueprint, *NativeOwner, *Component));
		}
	}

	static const UClass* FindNativeActorClass(const UBlueprint& Blueprint)
	{
		const UClass* ClassCursor = Blueprint.GeneratedClass
			? Blueprint.GeneratedClass->GetSuperClass()
			: Blueprint.ParentClass.Get();
		while (ClassCursor)
		{
			if (!Cast<UBlueprintGeneratedClass>(ClassCursor))
			{
				return ClassCursor;
			}
			ClassCursor = ClassCursor->GetSuperClass();
		}
		return nullptr;
	}
};

TArray<FBlueprintHelperComponentInfo> FBlueprintHelperComponentFacts::BuildReadbackFacts(const UBlueprint& Blueprint)
{
	TArray<FBlueprintHelperComponentInfo> Facts;
	if (!Blueprint.SimpleConstructionScript)
	{
		return Facts;
	}

	TSet<const USCS_Node*> SeenNodes;
	TSet<FString> SeenComponentNames;
	FBlueprintHelperComponentFactsLocalUtils::AddUniqueNodeFacts(
		Blueprint,
		Blueprint.SimpleConstructionScript->GetAllNodes(),
		SeenNodes,
		SeenComponentNames,
		Facts);

	UClass* ParentClass = Blueprint.GeneratedClass
		? Blueprint.GeneratedClass->GetSuperClass()
		: Blueprint.ParentClass.Get();
	while (ParentClass)
	{
		if (const UBlueprintGeneratedClass* ParentBPGC = Cast<UBlueprintGeneratedClass>(ParentClass))
		{
			if (ParentBPGC->SimpleConstructionScript)
			{
				FBlueprintHelperComponentFactsLocalUtils::AddUniqueNodeFacts(
					Blueprint,
					ParentBPGC->SimpleConstructionScript->GetAllNodes(),
					SeenNodes,
					SeenComponentNames,
					Facts);
			}
		}
		ParentClass = ParentClass->GetSuperClass();
	}

	FBlueprintHelperComponentFactsLocalUtils::AddNativeComponentFacts(
		Blueprint,
		FBlueprintHelperComponentFactsLocalUtils::FindNativeActorClass(Blueprint),
		SeenComponentNames,
		Facts);
	return Facts;
}

FBlueprintHelperComponentInfo FBlueprintHelperComponentFacts::BuildReadbackFact(const UBlueprint& Blueprint, const USCS_Node& Node)
{
	FBlueprintHelperComponentInfo Info;
	const UActorComponent* ComponentTemplate = Node.ComponentTemplate;

	Info.bHasReadbackFacts = true;
	Info.ComponentName = Node.GetVariableName().ToString();
	Info.ComponentClass = ComponentTemplate ? FBlueprintHelperComponentFactsLocalUtils::GetShortComponentClassName(ComponentTemplate->GetClass()) : TEXT("");
	Info.ClassPath = ComponentTemplate && ComponentTemplate->GetClass() ? ComponentTemplate->GetClass()->GetPathName() : TEXT("");
	Info.ComponentTemplatePath = ComponentTemplate ? ComponentTemplate->GetPathName() : TEXT("");
	Info.ComponentId = FBlueprintHelperComponentFactsLocalUtils::MakeComponentId(Blueprint, Node);
	Info.ParentComponent = FBlueprintHelperComponentFactsLocalUtils::GetParentName(Blueprint, Node);
	Info.SocketName = Node.AttachToName.IsNone() ? TEXT("") : Node.AttachToName.ToString();
	Info.RelativeTransform = FBlueprintHelperComponentFactsLocalUtils::MakeTransformJson(ComponentTemplate);
	Info.SelectedDefaults = FBlueprintHelperComponentFactsLocalUtils::MakeSelectedDefaultsJson(ComponentTemplate);
	Info.ReadbackRevision = TEXT("BlueprintComponentFacts.v1");
	Info.bIsRoot = FBlueprintHelperComponentFactsLocalUtils::IsRootNode(Blueprint, Node);
	Info.bIsDefaultSceneRoot = FBlueprintHelperComponentFactsLocalUtils::IsDefaultSceneRoot(Blueprint, Node);
	Info.bIsOwnedSCS = Node.GetSCS() == Blueprint.SimpleConstructionScript;
	Info.bIsInherited = !Info.bIsOwnedSCS;
	Info.bIsNative = false;
	Info.bCanDelete = Info.bIsOwnedSCS && !Info.bIsDefaultSceneRoot && Node.GetChildNodes().Num() == 0;
	Info.bCanRename = Info.bIsOwnedSCS && !Info.bIsDefaultSceneRoot;
	Info.bCanReparent = Info.bIsOwnedSCS && !Info.bIsDefaultSceneRoot;

	for (const USCS_Node* Child : Node.GetChildNodes())
	{
		if (Child)
		{
			Info.Children.Add(Child->GetVariableName().ToString());
		}
	}

	Info.ReadbackFingerprint = FBlueprintHelperComponentFactsLocalUtils::MakeFingerprint(Info);
	return Info;
}

FBlueprintHelperComponentInfo FBlueprintHelperComponentFacts::BuildNativeReadbackFact(
	const UBlueprint& Blueprint,
	const AActor& NativeOwner,
	const UActorComponent& ComponentTemplate)
{
	FBlueprintHelperComponentInfo Info;

	Info.bHasReadbackFacts = true;
	Info.ComponentName = ComponentTemplate.GetFName().ToString();
	Info.ComponentClass = FBlueprintHelperComponentFactsLocalUtils::GetShortComponentClassName(ComponentTemplate.GetClass());
	Info.ClassPath = ComponentTemplate.GetClass() ? ComponentTemplate.GetClass()->GetPathName() : TEXT("");
	Info.ComponentTemplatePath = ComponentTemplate.GetPathName();
	Info.ComponentId = FBlueprintHelperComponentFactsLocalUtils::MakeNativeComponentId(Blueprint, NativeOwner, ComponentTemplate);
	Info.RelativeTransform = FBlueprintHelperComponentFactsLocalUtils::MakeTransformJson(&ComponentTemplate);
	Info.SelectedDefaults = FBlueprintHelperComponentFactsLocalUtils::MakeSelectedDefaultsJson(&ComponentTemplate);
	Info.ReadbackRevision = TEXT("BlueprintComponentFacts.v1");
	Info.bIsOwnedSCS = false;
	Info.bIsInherited = true;
	Info.bIsNative = true;
	Info.bCanDelete = false;
	Info.bCanRename = false;
	Info.bCanReparent = false;

	if (const USceneComponent* SceneComponent = Cast<USceneComponent>(&ComponentTemplate))
	{
		const USceneComponent* Parent = SceneComponent->GetAttachParent();
		Info.ParentComponent = Parent ? Parent->GetFName().ToString() : TEXT("");
		Info.SocketName = SceneComponent->GetAttachSocketName().IsNone()
			? TEXT("")
			: SceneComponent->GetAttachSocketName().ToString();
		Info.bIsRoot = NativeOwner.GetRootComponent() == SceneComponent;
		for (const USceneComponent* Child : SceneComponent->GetAttachChildren())
		{
			if (Child)
			{
				Info.Children.Add(Child->GetFName().ToString());
			}
		}
	}

	Info.ReadbackFingerprint = FBlueprintHelperComponentFactsLocalUtils::MakeFingerprint(Info);
	return Info;
}
