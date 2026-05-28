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
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionContextUtils.h"

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
		UGraphWriteActionContextUtils::AddCapabilityFact(Field, TEXT("field.member_name"), Field.Name);
		UGraphWriteActionContextUtils::AddCapabilityGuidFact(Field, TEXT("field.member_guid"), Variable.VarGuid);
		UGraphWriteActionContextUtils::AddCapabilityFact(Field, TEXT("field.is_blueprint_member"), TEXT("true"));
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
		Field.PinContainerType = UGraphWriteActionContextUtils::ContainerTypeToString(Variable.VarType.ContainerType);
		Field.bReadable = true;
		Field.bWritable = true;
		if (UClass* VariableObjectClass = Cast<UClass>(Variable.VarType.PinSubCategoryObject.Get()))
		{
			if (VariableObjectClass->IsChildOf(UActorComponent::StaticClass()))
			{
				Field.bComponent = true;
				UGraphWriteActionContextUtils::AddCapabilityFact(Field, TEXT("field.component_name"), Field.Name);
				UGraphWriteActionContextUtils::AddCapabilityGuidFact(Field, TEXT("field.component_guid"), Variable.VarGuid);
				UGraphWriteActionContextUtils::AddCapabilityFact(Field, TEXT("field.component_owner_class"), Field.OwnerClassPath);
				UGraphWriteActionContextUtils::AddCapabilityFact(Field, TEXT("field.component_kind"), TEXT("blueprint_member_variable"));
			}
		}
		if (UScriptStruct* StructType = Cast<UScriptStruct>(Variable.VarType.PinSubCategoryObject.Get()))
		{
			UGraphWriteActionContextUtils::AddCapabilityFact(Field, TEXT("field.struct_type"), StructType->GetPathName());
		}
		Snapshot.Fields.Add(MoveTemp(Field));

		if (UClass* VariableObjectClass = Cast<UClass>(Variable.VarType.PinSubCategoryObject.Get()))
		{
			if (VariableObjectClass->IsChildOf(UActorComponent::StaticClass()))
			{
				UGraphWriteActionContextUtils::CaptureDelegateFields(VariableObjectClass, Snapshot);
			}
		}
	}

	UGraphWriteActionContextUtils::CaptureClassFields(Blueprint->SkeletonGeneratedClass, Snapshot);
	UGraphWriteActionContextUtils::CaptureClassFields(Blueprint->GeneratedClass, Snapshot);
	UGraphWriteActionContextUtils::CaptureFunctionLocalVariables(Blueprint, Snapshot);
	UGraphWriteActionContextUtils::CaptureFunctionInputParameters(Blueprint, Snapshot);
}
