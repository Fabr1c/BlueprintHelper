#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGraphWriteProjectedEvidenceQueryService.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionDatabaseProjectionService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperAssetActionProjectionService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionContextUtils.h"

static constexpr const TCHAR* OperationName = TEXT("project_graphwrite_spawner_evidence");

FBlueprintHelperToolResultBase FBlueprintHelperGraphWriteProjectedEvidenceQueryService::Project(
	const TSharedPtr<FJsonObject>& Payload)
{
	if (!Payload.IsValid())
	{
		return UGraphWriteActionContextUtils::MakeFailure(TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("payload is required."));
	}

	const FString AssetPath = UGraphWriteActionContextUtils::ReadStringField(Payload, TEXT("asset_path"));
	const FString GraphName = UGraphWriteActionContextUtils::ReadStringField(Payload, TEXT("graph_name"));
	if (AssetPath.IsEmpty())
	{
		return UGraphWriteActionContextUtils::MakeFailure(TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("asset_path is required."), TEXT("asset_path"));
	}
	if (GraphName.IsEmpty())
	{
		return UGraphWriteActionContextUtils::MakeFailure(TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("graph_name is required."), TEXT("graph_name"));
	}

	const TArray<TSharedPtr<FJsonValue>>* Requests = nullptr;
	if (!Payload->TryGetArrayField(TEXT("requests"), Requests) || !Requests)
	{
		return UGraphWriteActionContextUtils::MakeFailure(TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("requests array is required."), TEXT("requests"));
	}

	FBlueprintHelperGraphTarget Target;
	Target.BlueprintPath = AssetPath;
	Target.GraphName = GraphName;

	FBlueprintHelperDiagnosticSet Diagnostics;
	FBlueprintHelperGraphResolver Resolver;
	UBlueprint* Blueprint = Resolver.ResolveBlueprint(Target, Diagnostics);
	UEdGraph* Graph = Blueprint ? Resolver.ResolveGraph(Target, Diagnostics) : nullptr;
	if (!Blueprint || !Graph)
	{
		return UGraphWriteActionContextUtils::MakeFailure(
			TEXT("target_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			FString::Printf(TEXT("Unable to resolve Blueprint graph '%s' on asset '%s'."), *GraphName, *AssetPath),
			!Blueprint ? TEXT("asset_path") : TEXT("graph_name"));
	}

	bool bAllResolved = true;
	TArray<TSharedPtr<FJsonValue>> Items;
	for (const TSharedPtr<FJsonValue>& RequestValue : *Requests)
	{
		const TSharedPtr<FJsonObject>* RequestObject = nullptr;
		if (!RequestValue.IsValid() || !RequestValue->TryGetObject(RequestObject) || !RequestObject || !RequestObject->IsValid())
		{
			bAllResolved = false;
			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(TEXT("status"), TEXT("failed"));
			Item->SetStringField(TEXT("message"), TEXT("projection request must be an object."));
			Items.Add(MakeShared<FJsonValueObject>(Item));
			continue;
		}
		Items.Add(MakeShared<FJsonValueObject>(UGraphWriteActionContextUtils::ProjectRequestItem(Blueprint, Graph, *RequestObject, bAllResolved)));
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		OperationName,
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	FBlueprintHelperTargetRef TargetRef;
	TargetRef.AssetPath = AssetPath;
	TargetRef.TargetType = EBlueprintHelperTargetType::Graph;
	TargetRef.Graph = GraphName;
	Result.Target = TargetRef;

	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.GraphWriteProjectedSpawnerEvidencePack.v1"));
	Data->SetStringField(TEXT("asset_path"), AssetPath);
	Data->SetStringField(TEXT("graph_name"), GraphName);
	Data->SetBoolField(TEXT("all_resolved"), bAllResolved);
	Data->SetArrayField(TEXT("items"), Items);
	Result.Data = Data;
	return Result;
}
