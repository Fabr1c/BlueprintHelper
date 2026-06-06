#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphWriteConnectivityContext.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void FBlueprintHelperGraphWriteConnectivityContextBuilder::AddContextNodeRefs(
	FBlueprintGraphWriteConnectivityValidationInput& Out,
	const TArray<FString>& Refs,
	const TArray<UEdGraphNode*>& Nodes)
{
	const int32 Count = FMath::Min(Refs.Num(), Nodes.Num());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!Refs[Index].IsEmpty() && Nodes[Index])
		{
			Out.NodeRefs.Add(Refs[Index], Nodes[Index]);
		}
	}
}

FBlueprintGraphWriteConnectivityValidationInput FBlueprintHelperGraphWriteConnectivityContextBuilder::Build(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphWriteConnectivityContextInput& Input)
{
	FBlueprintGraphWriteConnectivityValidationInput Out;
	Out.TargetGraph = TargetGraph;
	Out.BoundaryModel.RuntimeAdapterId = Input.RuntimeAdapterId;
	Out.BoundaryModel.TaskSpecStrategy = Input.TaskSpecStrategy;
	Out.BoundaryModel.TargetAssetPath = Input.TargetAssetPath;
	Out.BoundaryModel.GraphName = Input.GraphName;
	Out.BoundaryModel.GraphFamily = Input.GraphFamily;
	Out.BoundaryModel.BodyKind = Input.BodyKind;
	Out.BoundaryModel.EntryNodeRefs = Input.EntryNodeRefs;
	Out.BoundaryModel.ExitNodeRefs = Input.ExitNodeRefs;
	Out.BoundaryModel.ProtectedNodeRefs = Input.ProtectedNodeRefs;
	Out.BoundaryModel.PureDataConsumptionPolicy = Input.PureDataPolicy;
	Out.BoundaryModel.AllowedIsolatedNodePolicy = Input.IsolatedNodePolicy;
	Out.ConnectivityPolicy = FBlueprintHelperGraphConnectivityPolicyUtils::FromBoundaryModel(Out.BoundaryModel);
	AddContextNodeRefs(Out, Input.EntryNodeRefs, Input.EntryNodes);
	AddContextNodeRefs(Out, Input.ExitNodeRefs, Input.ExitNodes);
	AddContextNodeRefs(Out, Input.ProtectedNodeRefs, Input.ProtectedNodes);
	return Out;
}

FBlueprintGraphWriteConnectivityValidationInput FBlueprintHelperGraphWriteConnectivityContextBuilder::BuildSemanticGenerationContext(
	UEdGraph* TargetGraph,
	const FString& RuntimeAdapterId,
	const FString& TaskSpecStrategy,
	const FString& GraphWriteJsonText,
	const FString& TargetAssetPath)
{
	FBlueprintHelperGraphWriteConnectivityContextInput ContextInput;
	ContextInput.RuntimeAdapterId = RuntimeAdapterId;
	ContextInput.TaskSpecStrategy = TaskSpecStrategy;
	ContextInput.TargetAssetPath = TargetAssetPath;
	ContextInput.GraphName = TargetGraph ? TargetGraph->GetName() : TEXT("");
	ContextInput.GraphFamily = TEXT("k2");
	ContextInput.BodyKind = EBlueprintHelperGraphBodyKind::Unknown;
	ContextInput.EntryNodeRefs.Add(MakeSemanticEntryRefFromGraphWriteJsonText(GraphWriteJsonText));
	return Build(TargetGraph, ContextInput);
}

FString FBlueprintHelperGraphWriteConnectivityContextBuilder::MakeSemanticEntryRefFromLogicSpec(
	const TSharedPtr<FJsonObject>& LogicSpecObject)
{
	const TSharedPtr<FJsonObject>* EntryObject = nullptr;
	if (!LogicSpecObject.IsValid() ||
		!LogicSpecObject->TryGetObjectField(TEXT("entry"), EntryObject) ||
		!EntryObject ||
		!EntryObject->IsValid())
	{
		return TEXT("semantic_entry");
	}

	FString EntryName;
	(*EntryObject)->TryGetStringField(TEXT("name"), EntryName);
	FString EntryId;
	(*EntryObject)->TryGetStringField(TEXT("id"), EntryId);
	if (!EntryId.IsEmpty())
	{
		return EntryId;
	}
	return EntryName.IsEmpty() ? TEXT("semantic_entry") : EntryName + TEXT("_entry");
}

FString FBlueprintHelperGraphWriteConnectivityContextBuilder::MakeSemanticEntryRefFromGraphWriteJsonText(
	const FString& GraphWriteJsonText)
{
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(GraphWriteJsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return TEXT("semantic_entry");
	}

	const TSharedPtr<FJsonObject>* LogicSpecObject = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("logic_spec"), LogicSpecObject) ||
		!LogicSpecObject ||
		!LogicSpecObject->IsValid())
	{
		return TEXT("semantic_entry");
	}
	return MakeSemanticEntryRefFromLogicSpec(*LogicSpecObject);
}
