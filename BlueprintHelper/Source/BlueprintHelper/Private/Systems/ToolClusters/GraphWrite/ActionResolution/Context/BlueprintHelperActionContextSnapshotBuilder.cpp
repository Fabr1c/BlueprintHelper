#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextSnapshotBuilder.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Class.h"

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
		Field.PinCategory = Variable.VarType.PinCategory.ToString();
		Field.PinSubCategory = Variable.VarType.PinSubCategory.ToString();
		Field.PinSubCategoryObjectPath = Variable.VarType.PinSubCategoryObject.Get()
			? Variable.VarType.PinSubCategoryObject->GetPathName()
			: FString();
		Field.bReadable = true;
		Field.bWritable = true;
		Snapshot.Fields.Add(MoveTemp(Field));
	}
}
