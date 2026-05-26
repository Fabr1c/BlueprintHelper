#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextSnapshotBuilder.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Components/ActorComponent.h"
#include "Engine/Blueprint.h"
#include "K2Node_FunctionEntry.h"
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
	Added.CapabilityFacts.Add(TEXT("field.member_name"), Name);
	return Added;
}

static void AddCapabilityFact(
	FBlueprintHelperActionContextFieldSnapshot& Field,
	const FString& Key,
	const FString& Value)
{
	const FString CleanValue = Value.TrimStartAndEnd();
	if (!Key.IsEmpty() && !CleanValue.IsEmpty())
	{
		Field.CapabilityFacts.FindOrAdd(Key, CleanValue);
	}
}

static void AddCapabilityGuidFact(
	FBlueprintHelperActionContextFieldSnapshot& Field,
	const FString& Key,
	const FGuid& Value)
{
	if (Value.IsValid())
	{
		AddCapabilityFact(Field, Key, Value.ToString(EGuidFormats::Digits));
	}
}

static FString ContainerTypeToString(const EPinContainerType ContainerType)
{
	switch (ContainerType)
	{
	case EPinContainerType::Array:
		return TEXT("array");
	case EPinContainerType::Set:
		return TEXT("set");
	case EPinContainerType::Map:
		return TEXT("map");
	default:
		break;
	}
	return FString();
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
		Field.Kind = TEXT("delegate");
		AddCapabilityFact(Field, TEXT("field.member_name"), DelegateProperty->GetName());
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
		Field.Kind = Property->GetOwnerClass() == Class ? TEXT("member") : TEXT("inherited_or_native");
		AddCapabilityFact(Field, TEXT("field.member_name"), Property->GetName());
		AddCapabilityFact(Field, TEXT("field.is_inherited_or_native"), Property->GetOwnerClass() != Class ? TEXT("true") : TEXT("false"));

		if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			Field.PinCategory = TEXT("object");
			Field.PinSubCategoryObjectPath = ObjectProperty->PropertyClass
				? ObjectProperty->PropertyClass->GetPathName()
				: FString();
			Field.bComponent = ObjectProperty->PropertyClass
				&& ObjectProperty->PropertyClass->IsChildOf(UActorComponent::StaticClass());
			if (Field.bComponent)
			{
				AddCapabilityFact(Field, TEXT("field.component_name"), Field.Name);
				AddCapabilityFact(Field, TEXT("field.component_owner_class"), OwnerClassPath);
				AddCapabilityFact(Field, TEXT("field.component_kind"), TEXT("native_or_inherited_property"));
			}
			if (Field.bComponent)
			{
				CaptureDelegateFields(ObjectProperty->PropertyClass, Snapshot);
			}
		}

		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			AddCapabilityFact(Field, TEXT("field.struct_type"), StructProperty->Struct ? StructProperty->Struct->GetPathName() : FString());
		}

		if (const FMulticastDelegateProperty* DelegateProperty = CastField<FMulticastDelegateProperty>(Property))
		{
			Field.PinCategory = TEXT("delegate");
			Field.Kind = TEXT("delegate");
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

static void CaptureFunctionLocalVariables(UBlueprint* Blueprint, FBlueprintHelperActionContextSnapshot& Snapshot)
{
	if (!Blueprint)
	{
		return;
	}

	TArray<UEdGraph*> FunctionGraphs;
	Blueprint->GetAllGraphs(FunctionGraphs);
	for (UEdGraph* Graph : FunctionGraphs)
	{
		if (!Graph || !FBlueprintEditorUtils::DoesSupportLocalVariables(Graph))
		{
			continue;
		}

		TArray<UK2Node_FunctionEntry*> EntryNodes;
		Graph->GetNodesOfClass<UK2Node_FunctionEntry>(EntryNodes);
		for (UK2Node_FunctionEntry* EntryNode : EntryNodes)
		{
			if (!EntryNode)
			{
				continue;
			}

			for (const FBPVariableDescription& LocalVar : EntryNode->LocalVariables)
			{
				FBlueprintHelperActionContextFieldSnapshot Field;
				Field.Name = LocalVar.VarName.ToString();
				Field.Kind = TEXT("local");
				AddCapabilityFact(Field, TEXT("field.member_name"), Field.Name);
				AddCapabilityFact(Field, TEXT("field.local_scope"), Graph->GetName());
				AddCapabilityFact(Field, TEXT("field.function_name"), Graph->GetName());
				AddCapabilityGuidFact(Field, TEXT("field.member_guid"), LocalVar.VarGuid);
				AddCapabilityFact(Field, TEXT("field.is_local_variable"), TEXT("true"));
				Field.PinCategory = LocalVar.VarType.PinCategory.ToString();
				Field.PinSubCategory = LocalVar.VarType.PinSubCategory.ToString();
				Field.PinSubCategoryObjectPath = LocalVar.VarType.PinSubCategoryObject.Get()
					? LocalVar.VarType.PinSubCategoryObject->GetPathName()
					: FString();
				Field.PinContainerType = ContainerTypeToString(LocalVar.VarType.ContainerType);
				Snapshot.Fields.Add(MoveTemp(Field));
			}
		}
	}
}

static void CaptureFunctionInputParameters(UBlueprint* Blueprint, FBlueprintHelperActionContextSnapshot& Snapshot)
{
	if (!Blueprint || !Blueprint->SkeletonGeneratedClass)
	{
		return;
	}

	for (TFieldIterator<UFunction> FunctionIt(Blueprint->SkeletonGeneratedClass, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
	{
		UFunction* Function = *FunctionIt;
		if (!Function)
		{
			continue;
		}

		for (TFieldIterator<FProperty> PropIt(Function); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (!Property ||
				!Property->HasAnyPropertyFlags(CPF_Parm) ||
				Property->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm))
			{
				continue;
			}

			FBlueprintHelperActionContextFieldSnapshot Field;
			Field.Name = Property->GetName();
			Field.Kind = TEXT("function_param");
			AddCapabilityFact(Field, TEXT("field.member_name"), Field.Name);
			AddCapabilityFact(Field, TEXT("field.function_name"), Function->GetName());
			AddCapabilityFact(Field, TEXT("field.local_scope"), Function->GetName());
			AddCapabilityFact(Field, TEXT("field.param_flags"), TEXT("FUNC_Parm"));
			Field.OwnerClassPath = Blueprint->SkeletonGeneratedClass->GetPathName();
			Field.FieldPath = Function->GetPathName() + TEXT(".") + Field.Name;
			AddCapabilityFact(Field, TEXT("field.is_function_parameter"), TEXT("true"));
			Snapshot.Fields.Add(MoveTemp(Field));
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
		Field.Kind = TEXT("member");
		AddCapabilityFact(Field, TEXT("field.member_name"), Field.Name);
		AddCapabilityGuidFact(Field, TEXT("field.member_guid"), Variable.VarGuid);
		AddCapabilityFact(Field, TEXT("field.is_blueprint_member"), TEXT("true"));
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
		Field.PinContainerType = ContainerTypeToString(Variable.VarType.ContainerType);
		Field.bReadable = true;
		Field.bWritable = true;
		if (UClass* VariableObjectClass = Cast<UClass>(Variable.VarType.PinSubCategoryObject.Get()))
		{
			if (VariableObjectClass->IsChildOf(UActorComponent::StaticClass()))
			{
				Field.bComponent = true;
				AddCapabilityFact(Field, TEXT("field.component_name"), Field.Name);
				AddCapabilityGuidFact(Field, TEXT("field.component_guid"), Variable.VarGuid);
				AddCapabilityFact(Field, TEXT("field.component_owner_class"), Field.OwnerClassPath);
				AddCapabilityFact(Field, TEXT("field.component_kind"), TEXT("blueprint_member_variable"));
			}
		}
		if (UScriptStruct* StructType = Cast<UScriptStruct>(Variable.VarType.PinSubCategoryObject.Get()))
		{
			AddCapabilityFact(Field, TEXT("field.struct_type"), StructType->GetPathName());
		}
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
	CaptureFunctionLocalVariables(Blueprint, Snapshot);
	CaptureFunctionInputParameters(Blueprint, Snapshot);
}
