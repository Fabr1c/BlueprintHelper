#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlanBuilder.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphJsonParser.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeUtility.h"

static void BlueprintGraphMutationPlanBuilderReadString(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	FString& OutValue)
{
	if (Object.IsValid() && OutValue.IsEmpty())
	{
		Object->TryGetStringField(FieldName, OutValue);
	}
}

static void BlueprintGraphMutationPlanBuilderReadDefaults(
	const TSharedPtr<FJsonObject>& NodeObject,
	TMap<FString, FString>& OutDefaultValues)
{
	const TSharedPtr<FJsonObject>* DefaultsObject = nullptr;
	if (!NodeObject.IsValid())
	{
		return;
	}

	if (!NodeObject->TryGetObjectField(TEXT("defaults"), DefaultsObject) || !DefaultsObject || !DefaultsObject->IsValid())
	{
		NodeObject->TryGetObjectField(TEXT("default_values"), DefaultsObject);
	}
	if (!DefaultsObject || !DefaultsObject->IsValid())
	{
		return;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*DefaultsObject)->Values)
	{
		OutDefaultValues.Add(Pair.Key, FBlueprintGraphJsonParser::ConvertJsonValueToString(Pair.Value));
	}
}

static FParsedNode BlueprintGraphMutationPlanBuilderParseNode(const TSharedPtr<FJsonObject>& NodeObject)
{
	FParsedNode ParsedNode;
	if (!NodeObject.IsValid())
	{
		return ParsedNode;
	}

	BlueprintGraphMutationPlanBuilderReadString(NodeObject, TEXT("id"), ParsedNode.Id);
	BlueprintGraphMutationPlanBuilderReadString(NodeObject, TEXT("node_id"), ParsedNode.Id);
	BlueprintGraphMutationPlanBuilderReadString(NodeObject, TEXT("type"), ParsedNode.SourceType);
	BlueprintGraphMutationPlanBuilderReadString(NodeObject, TEXT("kind"), ParsedNode.SourceType);
	ParsedNode.NodeType = FBlueprintGraphJsonParser::ResolveNodeType(NodeObject);
	ParsedNode.FunctionName = FBlueprintGraphJsonParser::ResolveNodeFunctionName(NodeObject);
	NodeObject->TryGetNumberField(TEXT("x"), ParsedNode.X);
	NodeObject->TryGetNumberField(TEXT("y"), ParsedNode.Y);
	ParsedNode.VariableReference = FBlueprintGraphJsonParser::ResolveVariableReference(NodeObject);
	ParsedNode.MacroReference = FBlueprintGraphJsonParser::ResolveMacroReference(NodeObject);
	ParsedNode.EventReference = FBlueprintGraphJsonParser::ResolveEventReference(NodeObject);
	ParsedNode.DelegateReference = FBlueprintGraphJsonParser::ResolveDelegateReference(NodeObject);
	ParsedNode.ContainerReference = FBlueprintGraphJsonParser::ResolveContainerReference(NodeObject);
	ParsedNode.StructReference = FBlueprintGraphJsonParser::ResolveStructReference(NodeObject);
	ParsedNode.CastReference = FBlueprintGraphJsonParser::ResolveCastReference(NodeObject);
	ParsedNode.SpawnReference = FBlueprintGraphJsonParser::ResolveSpawnReference(NodeObject);
	ParsedNode.FormatTextReference = FBlueprintGraphJsonParser::ResolveFormatTextReference(NodeObject);
	ParsedNode.LiteralReference = FBlueprintGraphJsonParser::ResolveLiteralReference(NodeObject);
	ParsedNode.ComponentBoundEventReference = FBlueprintGraphJsonParser::ResolveComponentBoundEventReference(NodeObject);
	ParsedNode.CommentReference = FBlueprintGraphJsonParser::ResolveCommentReference(NodeObject);
	ParsedNode.EnhancedInputActionReference = FBlueprintGraphJsonParser::ResolveEnhancedInputActionReference(NodeObject);
	ParsedNode.SwitchReference = FBlueprintGraphJsonParser::ResolveSwitchReference(NodeObject);
	ParsedNode.SelectReference = FBlueprintGraphJsonParser::ResolveSelectReference(NodeObject);
	BlueprintGraphMutationPlanBuilderReadDefaults(NodeObject, ParsedNode.DefaultValues);
	return ParsedNode;
}

static FParsedLink BlueprintGraphMutationPlanBuilderParseLink(const TSharedPtr<FJsonObject>& LinkObject)
{
	FParsedLink ParsedLink;
	if (!LinkObject.IsValid())
	{
		return ParsedLink;
	}

	BlueprintGraphMutationPlanBuilderReadString(LinkObject, TEXT("from"), ParsedLink.FromId);
	BlueprintGraphMutationPlanBuilderReadString(LinkObject, TEXT("from_id"), ParsedLink.FromId);
	BlueprintGraphMutationPlanBuilderReadString(LinkObject, TEXT("from_pin"), ParsedLink.FromPin);
	BlueprintGraphMutationPlanBuilderReadString(LinkObject, TEXT("to"), ParsedLink.ToId);
	BlueprintGraphMutationPlanBuilderReadString(LinkObject, TEXT("to_id"), ParsedLink.ToId);
	BlueprintGraphMutationPlanBuilderReadString(LinkObject, TEXT("to_pin"), ParsedLink.ToPin);
	return ParsedLink;
}

bool FBlueprintGraphMutationPlanBuilder::BuildFromGraphJson(
	const TSharedPtr<FJsonObject>& GraphJsonObject,
	FBlueprintGraphMutationPlan& OutPlan,
	TArray<FBlueprintGeneratorDiagnostic>& OutDiagnostics)
{
	OutPlan = FBlueprintGraphMutationPlan();
	OutDiagnostics.Empty();
	if (!GraphJsonObject.IsValid())
	{
		OutDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
			TEXT("graph_json_invalid"),
			TEXT("graph"),
			TEXT(""),
			TEXT("Graph JSON object is invalid.")));
		return false;
	}

	GraphJsonObject->TryGetStringField(TEXT("graph_name"), OutPlan.GraphName);
	if (OutPlan.GraphName.IsEmpty())
	{
		OutPlan.GraphName = TEXT("EventGraph");
	}

	const TArray<TSharedPtr<FJsonValue>>* NodeValues = nullptr;
	if (GraphJsonObject->TryGetArrayField(TEXT("nodes"), NodeValues) && NodeValues)
	{
		for (const TSharedPtr<FJsonValue>& NodeValue : *NodeValues)
		{
			OutPlan.Nodes.Add(MakeNodePlanFromParsedNode(BlueprintGraphMutationPlanBuilderParseNode(NodeValue->AsObject())));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* LinkValues = nullptr;
	if (GraphJsonObject->TryGetArrayField(TEXT("links"), LinkValues) && LinkValues)
	{
		for (const TSharedPtr<FJsonValue>& LinkValue : *LinkValues)
		{
			OutPlan.Links.Add(MakeLinkPlanFromParsedLink(BlueprintGraphMutationPlanBuilderParseLink(LinkValue->AsObject())));
		}
	}

	return OutPlan.IsValid() || OutPlan.Links.Num() > 0;
}

FBlueprintGraphMutationNodePlan FBlueprintGraphMutationPlanBuilder::MakeNodePlanFromParsedNode(
	const FParsedNode& ParsedNode)
{
	FBlueprintGraphMutationNodePlan Plan;
	Plan.NodeId = ParsedNode.Id;
	Plan.NodeType = ParsedNode.NodeType;
	Plan.FunctionName = ParsedNode.FunctionName;
	Plan.ResolvedCallFunctionStableId = ParsedNode.ResolvedCallFunctionStableId;
	Plan.ParsedNode = ParsedNode;
	Plan.DefaultValues = ParsedNode.DefaultValues;
	return Plan;
}

FBlueprintGraphMutationLinkPlan FBlueprintGraphMutationPlanBuilder::MakeLinkPlanFromParsedLink(
	const FParsedLink& ParsedLink)
{
	FBlueprintGraphMutationLinkPlan Plan;
	Plan.FromId = ParsedLink.FromId;
	Plan.FromPin = ParsedLink.FromPin;
	Plan.ToId = ParsedLink.ToId;
	Plan.ToPin = ParsedLink.ToPin;
	return Plan;
}
