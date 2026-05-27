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

namespace
{
static constexpr const TCHAR* OperationName = TEXT("project_graphwrite_spawner_evidence");

static FBlueprintHelperToolResultBase MakeFailure(
	const FString& Code,
	EBlueprintHelperToolStage Stage,
	const FString& Message,
	const FString& Field = FString())
{
	FBlueprintHelperToolError Error;
	Error.Code = Code;
	Error.Stage = Stage;
	Error.Message = Message;
	Error.Field = Field;
	return FBlueprintHelperToolResultBuilder::Failure(
		OperationName,
		FBlueprintHelperToolResultBuilder::GenerateTraceId(),
		Error);
}

static FString ReadStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	FString Value;
	if (Object.IsValid())
	{
		Object->TryGetStringField(FieldName, Value);
	}
	return Value.TrimStartAndEnd();
}

static TArray<FString> ReadQueries(const TSharedPtr<FJsonObject>& Object)
{
	TArray<FString> Result;
	const TArray<TSharedPtr<FJsonValue>>* QueryValues = nullptr;
	if (Object.IsValid() && Object->TryGetArrayField(TEXT("queries"), QueryValues) && QueryValues)
	{
		for (const TSharedPtr<FJsonValue>& QueryValue : *QueryValues)
		{
			FString Query;
			if (QueryValue.IsValid() && QueryValue->TryGetString(Query))
			{
				Query = Query.TrimStartAndEnd();
				if (!Query.IsEmpty())
				{
					Result.Add(Query);
				}
			}
		}
	}

	const FString SingleQuery = ReadStringField(Object, TEXT("query"));
	if (!SingleQuery.IsEmpty())
	{
		Result.Insert(SingleQuery, 0);
	}
	return Result;
}

static TSharedRef<FJsonObject> MakeEvidenceJson(const FBlueprintHelperProjectedAssetActionEvidence& Evidence)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("asset_action_stable_id"), Evidence.StableId);
	Json->SetStringField(TEXT("asset_action_node_class"), Evidence.NodeClassPath);
	Json->SetStringField(TEXT("asset_action_spawner_signature"), Evidence.SpawnerSignature);
	Json->SetStringField(TEXT("asset_action_owner_path"), Evidence.OwnerPath);
	if (!Evidence.Query.IsEmpty()) { Json->SetStringField(TEXT("asset_action_query"), Evidence.Query); }
	if (!Evidence.MenuName.IsEmpty()) { Json->SetStringField(TEXT("asset_action_menu_name"), Evidence.MenuName); }
	if (!Evidence.Category.IsEmpty()) { Json->SetStringField(TEXT("asset_action_category"), Evidence.Category); }
	return Json;
}

static TSharedRef<FJsonObject> MakeEvidenceJson(const FBlueprintHelperProjectedScheduleActionEvidence& Evidence)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schedule_action_stable_id"), Evidence.StableId);
	Json->SetStringField(TEXT("schedule_node_class"), Evidence.NodeClassPath);
	Json->SetStringField(TEXT("schedule_spawner_signature"), Evidence.SpawnerSignature);
	Json->SetStringField(TEXT("schedule_owner_path"), Evidence.OwnerPath);
	if (!Evidence.Query.IsEmpty()) { Json->SetStringField(TEXT("schedule_query"), Evidence.Query); }
	if (!Evidence.MenuName.IsEmpty()) { Json->SetStringField(TEXT("schedule_menu_name"), Evidence.MenuName); }
	if (!Evidence.Category.IsEmpty()) { Json->SetStringField(TEXT("schedule_category"), Evidence.Category); }
	if (!Evidence.DelegatePinName.IsEmpty()) { Json->SetStringField(TEXT("schedule_delegate_pin_name"), Evidence.DelegatePinName); }
	if (!Evidence.GraphLatentAllowed.IsEmpty()) { Json->SetStringField(TEXT("graph_latent_allowed"), Evidence.GraphLatentAllowed); }
	return Json;
}

static TSharedRef<FJsonObject> MakeItemBase(
	const TSharedPtr<FJsonObject>& Request,
	const FString& Kind)
{
	TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
	Item->SetStringField(TEXT("request_id"), ReadStringField(Request, TEXT("request_id")));
	Item->SetStringField(TEXT("operation_id"), ReadStringField(Request, TEXT("operation_id")));
	Item->SetStringField(TEXT("projection_kind"), Kind);
	return Item;
}

static TSharedRef<FJsonObject> MakeItemFailure(
	const TSharedPtr<FJsonObject>& Request,
	const FString& Kind,
	const FString& Message)
{
	TSharedRef<FJsonObject> Item = MakeItemBase(Request, Kind);
	Item->SetStringField(TEXT("status"), TEXT("failed"));
	Item->SetStringField(TEXT("message"), Message);
	return Item;
}

static TSharedRef<FJsonObject> MakeItemSuccess(
	const TSharedPtr<FJsonObject>& Request,
	const FString& Kind,
	const TSharedRef<FJsonObject>& Evidence,
	const FString& Message)
{
	TSharedRef<FJsonObject> Item = MakeItemBase(Request, Kind);
	Item->SetStringField(TEXT("status"), TEXT("resolved"));
	Item->SetStringField(TEXT("message"), Message);
	Item->SetObjectField(TEXT("evidence"), Evidence);
	return Item;
}

static bool TryProjectExactAssetAction(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FBlueprintHelperAssetActionProjectedCandidate& Candidate,
	FBlueprintHelperProjectedAssetActionEvidence& OutEvidence)
{
	FBlueprintHelperAssetActionProjectionRequest ExactRequest;
	ExactRequest.Blueprint = Blueprint;
	ExactRequest.TargetGraph = Graph;
	ExactRequest.RequiredEvidence.StableId = Candidate.StableId;
	ExactRequest.RequiredEvidence.NodeClassPath = Candidate.NodeClassPath;
	ExactRequest.RequiredEvidence.SpawnerSignature = Candidate.SpawnerSignature;
	ExactRequest.RequiredEvidence.OwnerPath = Candidate.OwnerPath;
	ExactRequest.RequiredEvidence.Query = Candidate.Query;
	ExactRequest.RequiredEvidence.MenuName = Candidate.MenuName;
	ExactRequest.RequiredEvidence.Category = Candidate.Category;

	const FBlueprintHelperAssetActionProjectionResult ExactProjection =
		FBlueprintHelperAssetActionProjectionService::Project(ExactRequest);
	if (ExactProjection.Status != EBlueprintHelperActionResolutionStatus::Resolved ||
		ExactProjection.Candidates.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperAssetActionProjectedCandidate& ExactCandidate = ExactProjection.Candidates[0];
	OutEvidence.StableId = ExactCandidate.StableId;
	OutEvidence.NodeClassPath = ExactCandidate.NodeClassPath;
	OutEvidence.SpawnerSignature = ExactCandidate.SpawnerSignature;
	OutEvidence.OwnerPath = ExactCandidate.OwnerPath;
	OutEvidence.Query = ExactCandidate.Query;
	OutEvidence.MenuName = ExactCandidate.MenuName;
	OutEvidence.Category = ExactCandidate.Category;
	return OutEvidence.HasProjectedIdentity();
}

static bool TryProjectAssetActionEvidence(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const TArray<FString>& Queries,
	FBlueprintHelperProjectedAssetActionEvidence& OutEvidence,
	FString& OutMessage)
{
	for (const FString& Query : Queries)
	{
		FBlueprintHelperAssetActionProjectionRequest Request;
		Request.Blueprint = Blueprint;
		Request.TargetGraph = Graph;
		Request.RequiredEvidence.Query = Query;
		Request.Query = Query;

		const FBlueprintHelperAssetActionProjectionResult Projection =
			FBlueprintHelperAssetActionProjectionService::Project(Request);
		if ((Projection.Status == EBlueprintHelperActionResolutionStatus::Resolved ||
			Projection.Status == EBlueprintHelperActionResolutionStatus::Ambiguous) &&
			Projection.Candidates.Num() > 0 &&
			TryProjectExactAssetAction(Blueprint, Graph, Projection.Candidates[0], OutEvidence))
		{
			OutMessage = FString::Printf(TEXT("asset_action projected from query '%s'."), *Query);
			return true;
		}
		OutMessage = Projection.Message;
	}
	return false;
}

static bool TryProjectExactSchedule(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FBlueprintHelperActionDatabaseProjectedCandidate& Candidate,
	FBlueprintHelperProjectedScheduleActionEvidence& OutEvidence)
{
	FBlueprintHelperActionDatabaseProjectionRequest ExactRequest;
	ExactRequest.Blueprint = Blueprint;
	ExactRequest.TargetGraph = Graph;
	ExactRequest.RequiredEvidence.StableId = Candidate.StableId;
	ExactRequest.RequiredEvidence.NodeClassPath = Candidate.NodeClassPath;
	ExactRequest.RequiredEvidence.SpawnerSignature = Candidate.SpawnerSignature;
	ExactRequest.RequiredEvidence.OwnerPath = Candidate.OwnerPath;
	ExactRequest.RequiredEvidence.Query = Candidate.Query;
	ExactRequest.RequiredEvidence.MenuName = Candidate.MenuName;
	ExactRequest.RequiredEvidence.Category = Candidate.Category;
	ExactRequest.ErrorPrefix = TEXT("schedule");

	const FBlueprintHelperActionDatabaseProjectionResult ExactProjection =
		FBlueprintHelperActionDatabaseProjectionService::Project(ExactRequest);
	if (ExactProjection.Status != EBlueprintHelperActionResolutionStatus::Resolved ||
		ExactProjection.Candidates.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperActionDatabaseProjectedCandidate& ExactCandidate = ExactProjection.Candidates[0];
	OutEvidence.StableId = ExactCandidate.StableId;
	OutEvidence.NodeClassPath = ExactCandidate.NodeClassPath;
	OutEvidence.SpawnerSignature = ExactCandidate.SpawnerSignature;
	OutEvidence.OwnerPath = ExactCandidate.OwnerPath;
	OutEvidence.Query = ExactCandidate.Query;
	OutEvidence.MenuName = ExactCandidate.MenuName;
	OutEvidence.Category = ExactCandidate.Category;
	return OutEvidence.HasProjectedIdentity();
}

static bool TryProjectScheduleEvidence(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const TArray<FString>& Queries,
	FBlueprintHelperProjectedScheduleActionEvidence& OutEvidence,
	FString& OutMessage)
{
	for (const FString& Query : Queries)
	{
		FBlueprintHelperActionDatabaseProjectionRequest Request;
		Request.Blueprint = Blueprint;
		Request.TargetGraph = Graph;
		Request.RequiredEvidence.Query = Query;
		Request.Query = Query;
		Request.ErrorPrefix = TEXT("schedule");

		const FBlueprintHelperActionDatabaseProjectionResult Projection =
			FBlueprintHelperActionDatabaseProjectionService::Project(Request);
		if ((Projection.Status == EBlueprintHelperActionResolutionStatus::Resolved ||
			Projection.Status == EBlueprintHelperActionResolutionStatus::Ambiguous) &&
			Projection.Candidates.Num() > 0 &&
			TryProjectExactSchedule(Blueprint, Graph, Projection.Candidates[0], OutEvidence))
		{
			OutMessage = FString::Printf(TEXT("schedule projected from query '%s'."), *Query);
			return true;
		}
		OutMessage = Projection.Message;
	}
	return false;
}

static TSharedRef<FJsonObject> ProjectRequestItem(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const TSharedPtr<FJsonObject>& Request,
	bool& bAllResolved)
{
	const FString Kind = ReadStringField(Request, TEXT("projection_kind")).IsEmpty()
		? ReadStringField(Request, TEXT("kind"))
		: ReadStringField(Request, TEXT("projection_kind"));
	const TArray<FString> Queries = ReadQueries(Request);
	if (Kind.IsEmpty())
	{
		bAllResolved = false;
		return MakeItemFailure(Request, TEXT("unknown"), TEXT("projection request requires kind or projection_kind."));
	}
	if (Queries.Num() == 0)
	{
		bAllResolved = false;
		return MakeItemFailure(Request, Kind, TEXT("projection request requires query or queries."));
	}

	if (Kind.Equals(TEXT("asset_action"), ESearchCase::IgnoreCase))
	{
		FBlueprintHelperProjectedAssetActionEvidence Evidence;
		FString Message;
		if (TryProjectAssetActionEvidence(Blueprint, Graph, Queries, Evidence, Message))
		{
			return MakeItemSuccess(Request, TEXT("asset_action"), MakeEvidenceJson(Evidence), Message);
		}
		bAllResolved = false;
		return MakeItemFailure(Request, TEXT("asset_action"), Message.IsEmpty() ? TEXT("asset_action projection failed.") : Message);
	}

	if (Kind.Equals(TEXT("schedule"), ESearchCase::IgnoreCase))
	{
		FBlueprintHelperProjectedScheduleActionEvidence Evidence;
		FString Message;
		if (TryProjectScheduleEvidence(Blueprint, Graph, Queries, Evidence, Message))
		{
			const FString GraphLatentAllowed = ReadStringField(Request, TEXT("graph_latent_allowed"));
			if (!GraphLatentAllowed.IsEmpty())
			{
				Evidence.GraphLatentAllowed = GraphLatentAllowed;
			}
			return MakeItemSuccess(Request, TEXT("schedule"), MakeEvidenceJson(Evidence), Message);
		}
		bAllResolved = false;
		return MakeItemFailure(Request, TEXT("schedule"), Message.IsEmpty() ? TEXT("schedule projection failed.") : Message);
	}

	bAllResolved = false;
	return MakeItemFailure(Request, Kind, FString::Printf(TEXT("unsupported projection kind '%s'."), *Kind));
}
}

FBlueprintHelperToolResultBase FBlueprintHelperGraphWriteProjectedEvidenceQueryService::Project(
	const TSharedPtr<FJsonObject>& Payload)
{
	if (!Payload.IsValid())
	{
		return MakeFailure(TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("payload is required."));
	}

	const FString AssetPath = ReadStringField(Payload, TEXT("asset_path"));
	const FString GraphName = ReadStringField(Payload, TEXT("graph_name"));
	if (AssetPath.IsEmpty())
	{
		return MakeFailure(TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("asset_path is required."), TEXT("asset_path"));
	}
	if (GraphName.IsEmpty())
	{
		return MakeFailure(TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("graph_name is required."), TEXT("graph_name"));
	}

	const TArray<TSharedPtr<FJsonValue>>* Requests = nullptr;
	if (!Payload->TryGetArrayField(TEXT("requests"), Requests) || !Requests)
	{
		return MakeFailure(TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("requests array is required."), TEXT("requests"));
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
		return MakeFailure(
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
		Items.Add(MakeShared<FJsonValueObject>(ProjectRequestItem(Blueprint, Graph, *RequestObject, bAllResolved)));
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
