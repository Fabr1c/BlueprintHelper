#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextSnapshotBuilder.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Components/ActorComponent.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Class.h"
#include "UObject/FieldIterator.h"
#include "UObject/UnrealType.h"

namespace
{
static bool SameFieldSnapshotIdentity(
	const FBlueprintHelperActionContextFieldSnapshot& Field,
	const FString& Name,
	const FString& OwnerClassPath,
	const FString& FieldPath)
{
	return Field.Name == Name
		&& Field.OwnerClassPath == OwnerClassPath
		&& (Field.FieldPath == FieldPath || Field.FieldPath.IsEmpty() || FieldPath.IsEmpty());
}

static FBlueprintHelperActionContextFieldSnapshot& FindOrAddFieldSnapshot(
	FBlueprintHelperActionContextSnapshot& Snapshot,
	const FString& Name,
	const FString& OwnerClassPath,
	const FString& FieldPath)
{
	if (FBlueprintHelperActionContextFieldSnapshot* Existing = Snapshot.Fields.FindByPredicate(
		[&Name, &OwnerClassPath, &FieldPath](const FBlueprintHelperActionContextFieldSnapshot& Field)
		{
			return SameFieldSnapshotIdentity(Field, Name, OwnerClassPath, FieldPath);
		}))
	{
		return *Existing;
	}

	FBlueprintHelperActionContextFieldSnapshot& Added = Snapshot.Fields.AddDefaulted_GetRef();
	Added.Name = Name;
	Added.OwnerClassPath = OwnerClassPath;
	Added.FieldPath = FieldPath;
	return Added;
}

static void CaptureDelegateFields(const UClass* Class, FBlueprintHelperActionContextSnapshot& Snapshot)
{
	if (!Class)
	{
		return;
	}

	for (TFieldIterator<FMulticastDelegateProperty> PropertyIt(Class, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		const FMulticastDelegateProperty* DelegateProperty = *PropertyIt;
		if (!DelegateProperty)
		{
			continue;
		}

		const FString OwnerClassPath = DelegateProperty->GetOwnerClass()
			? DelegateProperty->GetOwnerClass()->GetPathName()
			: Class->GetPathName();
		FBlueprintHelperActionContextFieldSnapshot& Field = FindOrAddFieldSnapshot(
			Snapshot,
			DelegateProperty->GetName(),
			OwnerClassPath,
			DelegateProperty->GetPathName());

		Field.PinCategory = TEXT("delegate");
		Field.bReadable = false;
		Field.bWritable = true;
		Field.bMulticastDelegate = true;
		Field.bBlueprintAssignable = DelegateProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable);
		Field.bBlueprintCallable = DelegateProperty->HasAnyPropertyFlags(CPF_BlueprintCallable);
		Field.DelegateSignatureFunctionPath = DelegateProperty->SignatureFunction
			? DelegateProperty->SignatureFunction->GetPathName()
			: FString();
	}
}

static void CaptureClassFields(const UClass* Class, FBlueprintHelperActionContextSnapshot& Snapshot)
{
	if (!Class)
	{
		return;
	}

	for (TFieldIterator<FProperty> PropertyIt(Class, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		const FProperty* Property = *PropertyIt;
		if (!Property)
		{
			continue;
		}

		const FString OwnerClassPath = Property->GetOwnerClass()
			? Property->GetOwnerClass()->GetPathName()
			: Class->GetPathName();
		FBlueprintHelperActionContextFieldSnapshot& Field = FindOrAddFieldSnapshot(
			Snapshot,
			Property->GetName(),
			OwnerClassPath,
			Property->GetPathName());

		Field.bReadable = true;
		Field.bWritable = true;

		if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
		{
			Field.PinCategory = TEXT("object");
			Field.PinSubCategoryObjectPath = ObjectProperty->PropertyClass
				? ObjectProperty->PropertyClass->GetPathName()
				: FString();
			Field.bComponent = ObjectProperty->PropertyClass
				&& ObjectProperty->PropertyClass->IsChildOf(UActorComponent::StaticClass());
			if (Field.bComponent)
			{
				CaptureDelegateFields(ObjectProperty->PropertyClass, Snapshot);
			}
		}

		if (const FMulticastDelegateProperty* DelegateProperty = CastField<FMulticastDelegateProperty>(Property))
		{
			Field.PinCategory = TEXT("delegate");
			Field.bReadable = false;
			Field.bWritable = true;
			Field.bMulticastDelegate = true;
			Field.bBlueprintAssignable = DelegateProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable);
			Field.bBlueprintCallable = DelegateProperty->HasAnyPropertyFlags(CPF_BlueprintCallable);
			Field.DelegateSignatureFunctionPath = DelegateProperty->SignatureFunction
				? DelegateProperty->SignatureFunction->GetPathName()
				: FString();
		}
	}
}
}

FBlueprintHelperActionContextSnapshot FBlueprintHelperActionContextSnapshotBuilder::BuildSnapshot(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const TArray<FBlueprintHelperActionContextDemand>& Demands,
	const FBlueprintHelperActionContextRevisionToken& Revision)
{
	(void)Demands;

	FBlueprintHelperActionContextSnapshot Snapshot;
	Snapshot.Revision = Revision;

	if (!ensureMsgf(IsInGameThread(), TEXT("ActionContext snapshot must be captured on the game thread.")))
	{
		return Snapshot;
	}

	Snapshot.Graph = CaptureGraph(Blueprint, Graph);
	CaptureFields(Blueprint, Snapshot);
	return Snapshot;
}

FBlueprintHelperActionContextGraphSnapshot FBlueprintHelperActionContextSnapshotBuilder::CaptureGraph(
	UBlueprint* Blueprint,
	UEdGraph* Graph)
{
	FBlueprintHelperActionContextGraphSnapshot GraphSnapshot;
	if (!Blueprint || !Graph)
	{
		return GraphSnapshot;
	}

	GraphSnapshot.AssetPath = Blueprint->GetPathName();
	GraphSnapshot.GraphName = Graph->GetName();
	GraphSnapshot.BlueprintClassPath = Blueprint->GeneratedClass
		? Blueprint->GeneratedClass->GetPathName()
		: FString();
	GraphSnapshot.SchemaClassPath = Graph->GetSchema()
		? Graph->GetSchema()->GetClass()->GetPathName()
		: FString();
	GraphSnapshot.GraphType = FBlueprintEditorUtils::IsEventGraph(Graph) ? TEXT("event_graph") : TEXT("graph");
	GraphSnapshot.FunctionName = FBlueprintEditorUtils::IsEventGraph(Graph) ? FString() : Graph->GetName();
	GraphSnapshot.bImpureAllowed = !FBlueprintEditorUtils::IsGraphReadOnly(Graph);
	GraphSnapshot.bLatentAllowed = FBlueprintEditorUtils::IsEventGraph(Graph);
	return GraphSnapshot;
}

void FBlueprintHelperActionContextSnapshotBuilder::CaptureFields(
	UBlueprint* Blueprint,
	FBlueprintHelperActionContextSnapshot& Snapshot)
{
	if (!Blueprint)
	{
		return;
	}

	for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		FBlueprintHelperActionContextFieldSnapshot Field;
		Field.Name = Variable.VarName.ToString();
		Field.OwnerClassPath = Blueprint->GeneratedClass
			? Blueprint->GeneratedClass->GetPathName()
			: FString();
		Field.FieldPath = Field.OwnerClassPath.IsEmpty()
			? Field.Name
			: Field.OwnerClassPath + TEXT(".") + Field.Name;
		Field.PinCategory = Variable.VarType.PinCategory.ToString();
		Field.PinSubCategory = Variable.VarType.PinSubCategory.ToString();
		Field.PinSubCategoryObjectPath = Variable.VarType.PinSubCategoryObject.Get()
			? Variable.VarType.PinSubCategoryObject->GetPathName()
			: FString();
		Field.bReadable = true;
		Field.bWritable = true;
		Snapshot.Fields.Add(MoveTemp(Field));

		if (UClass* VariableObjectClass = Cast<UClass>(Variable.VarType.PinSubCategoryObject.Get()))
		{
			if (VariableObjectClass->IsChildOf(UActorComponent::StaticClass()))
			{
				CaptureDelegateFields(VariableObjectClass, Snapshot);
			}
		}
	}

	CaptureClassFields(Blueprint->SkeletonGeneratedClass, Snapshot);
	CaptureClassFields(Blueprint->GeneratedClass, Snapshot);
}
