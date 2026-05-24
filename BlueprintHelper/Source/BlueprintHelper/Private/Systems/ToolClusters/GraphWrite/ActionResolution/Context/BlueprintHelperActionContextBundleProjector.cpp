#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.h"

namespace
{
static void AppendPinType(FString& Stable, const FBlueprintHelperCallFunctionPinType& PinType)
{
	Stable += PinType.Category;
	Stable += TEXT(".");
	Stable += PinType.SubCategory;
	Stable += TEXT(".");
	Stable += PinType.ObjectPath;
	Stable += TEXT(".");
	Stable += PinType.ContainerType;
	Stable += TEXT(".");
	Stable += PinType.bIsReference ? TEXT("ref") : TEXT("value");
	Stable += TEXT(".");
	Stable += PinType.bIsConst ? TEXT("const") : TEXT("mutable");
}

static FString StableHashString(const FString& Stable)
{
	return LexToString(GetTypeHash(Stable));
}

static FString BuildSemanticConstraintsHash(const FBlueprintHelperActionSemanticConstraints& Semantic)
{
	FString Stable;
	Stable += FBlueprintHelperActionResolutionCore::SemanticKindToString(Semantic.Kind);
	Stable += TEXT("|");
	Stable += FBlueprintHelperActionResolutionCore::SemanticFamilyToString(Semantic.SemanticFamily);
	Stable += TEXT("|");
	Stable += FBlueprintHelperActionResolutionCore::TypeOperationToString(Semantic.TypeOperation);
	Stable += TEXT("|");
	Stable += Semantic.Query;
	Stable += TEXT("|");
	Stable += Semantic.StableId;
	Stable += TEXT("|");
	Stable += Semantic.TargetPath;
	Stable += TEXT("|");
	Stable += Semantic.PropertyPath;
	Stable += TEXT("|");
	Stable += Semantic.FieldOperation;
	Stable += TEXT("|");
	Stable += Semantic.FieldScope;
	Stable += TEXT("|");
	Stable += Semantic.FunctionOperation;
	Stable += TEXT("|");
	Stable += Semantic.TransformOperation;
	Stable += TEXT("|");
	Stable += Semantic.ScheduleOperation;
	Stable += TEXT("|");
	Stable += Semantic.TypeName;
	Stable += TEXT("|");
	Stable += Semantic.StructPath;
	Stable += TEXT("|");
	Stable += Semantic.TypeStructureId;
	Stable += TEXT("|");
	Stable += Semantic.SearchMode;
	Stable += TEXT("|");
	Stable += Semantic.AmbiguityPolicy;
	Stable += TEXT("|");
	Stable += Semantic.TargetObjectType;
	Stable += TEXT("|");
	Stable += Semantic.ExpectedReturnType;
	Stable += TEXT("|");
	AppendPinType(Stable, Semantic.TargetObjectPinType);
	Stable += TEXT("|");
	AppendPinType(Stable, Semantic.ExpectedReturnPinType);

	TArray<FString> ArgumentTypeKeys;
	Semantic.ArgumentTypes.GetKeys(ArgumentTypeKeys);
	ArgumentTypeKeys.Sort();
	for (const FString& Key : ArgumentTypeKeys)
	{
		Stable += TEXT("|arg:");
		Stable += Key;
		Stable += TEXT("=");
		Stable += Semantic.ArgumentTypes.FindRef(Key);
	}

	TArray<FString> ArgumentPinTypeKeys;
	Semantic.ArgumentPinTypes.GetKeys(ArgumentPinTypeKeys);
	ArgumentPinTypeKeys.Sort();
	for (const FString& Key : ArgumentPinTypeKeys)
	{
		Stable += TEXT("|pin:");
		Stable += Key;
		Stable += TEXT("=");
		AppendPinType(Stable, Semantic.ArgumentPinTypes.FindRef(Key));
	}

	return StableHashString(Stable);
}

static FString BuildProjectedContextHash(const FBlueprintHelperResolvedActionContextBundle& Bundle, const FBlueprintHelperResolvedActionContext& Context)
{
	FString Stable;
	Stable += Bundle.Revision.AssetPath;
	Stable += TEXT("|");
	Stable += Bundle.Revision.GraphName;
	Stable += TEXT("|");
	Stable += Bundle.Revision.TaskRunId;
	Stable += TEXT("|");
	Stable += Bundle.Revision.PlanHash;
	Stable += TEXT("|");
	Stable += LexToString(Bundle.Revision.BlueprintRevision);
	Stable += TEXT("|");
	Stable += LexToString(Bundle.Revision.GraphRevision);
	Stable += TEXT("|");
	Stable += Context.StatementId;
	Stable += TEXT("|");
	Stable += FBlueprintHelperActionResolutionCore::ClusterKindToString(Context.ClusterKind);
	Stable += TEXT("|");
	Stable += Context.GraphName;

	TArray<FString> EvidenceKeys;
	Context.Evidence.GetKeys(EvidenceKeys);
	EvidenceKeys.Sort();
	for (const FString& Key : EvidenceKeys)
	{
		Stable += TEXT("|ev:");
		Stable += Key;
		Stable += TEXT("=");
		Stable += Context.Evidence.FindRef(Key);
	}

	return StableHashString(Stable);
}
}

bool FBlueprintHelperActionContextBundleProjector::TryBuildRequest(
	const FBlueprintHelperResolvedActionContextBundle& Bundle,
	const FString& StatementId,
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	FBlueprintHelperActionResolutionRequest& OutRequest,
	FString& OutError)
{
	const FBlueprintHelperResolvedActionContext* Context = Bundle.FindByStatementId(StatementId);
	if (!Context)
	{
		OutError = FString::Printf(TEXT("action_context_not_found:%s"), *StatementId);
		return false;
	}

	if (!Blueprint || !Graph)
	{
		OutError = TEXT("action_context_missing_blueprint_or_graph");
		return false;
	}

	if (Context->ClusterKind == EBlueprintHelperSpawnerClusterKind::Unknown
		|| Context->Semantic.Kind == EBlueprintHelperActionSemanticKind::Unknown)
	{
		OutError = FString::Printf(TEXT("action_context_unresolved_semantic:%s"), *StatementId);
		return false;
	}

	OutRequest = FBlueprintHelperActionResolutionRequest();
	OutRequest.ClusterKind = Context->ClusterKind;
	OutRequest.Blueprint = Blueprint;
	OutRequest.TargetGraph = Graph;
	OutRequest.StatementId = Context->StatementId;
	OutRequest.SemanticConstraintsHash = BuildSemanticConstraintsHash(Context->Semantic);
	OutRequest.ProjectedContextHash = BuildProjectedContextHash(Bundle, *Context);
	OutRequest.ContextEvidence = Context->Evidence;
	OutRequest.Semantic = Context->Semantic;
	return true;
}
