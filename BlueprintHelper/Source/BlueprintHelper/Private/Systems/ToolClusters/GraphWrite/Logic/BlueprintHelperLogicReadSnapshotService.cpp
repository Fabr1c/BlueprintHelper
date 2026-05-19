#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadSnapshotService.h"

#include "Shared/Services/BlueprintHelperExportService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"

class FBlueprintHelperLogicReadSnapshotServiceLocalUtils
{
public:
	static const TMap<EBlueprintHelperTargetType, EBlueprintHelperLogicScope>& TargetScopeMap()
	{
		static const TMap<EBlueprintHelperTargetType, EBlueprintHelperLogicScope> Map = {
			{EBlueprintHelperTargetType::Blueprint, EBlueprintHelperLogicScope::Blueprint},
			{EBlueprintHelperTargetType::Graph, EBlueprintHelperLogicScope::TargetGraph},
			{EBlueprintHelperTargetType::Function, EBlueprintHelperLogicScope::TargetFunction},
			{EBlueprintHelperTargetType::Event, EBlueprintHelperLogicScope::TargetEvent},
			{EBlueprintHelperTargetType::CustomEvent, EBlueprintHelperLogicScope::TargetCustomEvent},
			{EBlueprintHelperTargetType::Block, EBlueprintHelperLogicScope::TargetBlock},
		};
		return Map;
	}

	static const TSet<EBlueprintHelperLogicScope>& TargetEntryScopes()
	{
		static const TSet<EBlueprintHelperLogicScope> Scopes = {
			EBlueprintHelperLogicScope::TargetFunction,
			EBlueprintHelperLogicScope::TargetEvent,
			EBlueprintHelperLogicScope::TargetCustomEvent,
		};
		return Scopes;
	}

	static const TMap<EBlueprintHelperLogicScope, EBlueprintHelperExportScope>& ExportScopeMap()
	{
		static const TMap<EBlueprintHelperLogicScope, EBlueprintHelperExportScope> Map = {
			{EBlueprintHelperLogicScope::Blueprint, EBlueprintHelperExportScope::FullBlueprint},
		};
		return Map;
	}
};

FBlueprintHelperLogicReadSnapshotService::FBlueprintHelperLogicReadSnapshotService()
{
	OwnedGraphResolver = MakeUnique<FBlueprintHelperGraphResolver>();
	OwnedExportService = MakeUnique<FBlueprintHelperExportService>(*OwnedGraphResolver);
	ExportService = OwnedExportService.Get();
}

FBlueprintHelperLogicReadSnapshotService::FBlueprintHelperLogicReadSnapshotService(
	const FBlueprintHelperExportService& InExportService)
	: ExportService(&InExportService)
{
}

FBlueprintHelperLogicReadSnapshotService::~FBlueprintHelperLogicReadSnapshotService() = default;

bool FBlueprintHelperLogicReadSnapshotService::BuildSnapshot(
	const FBlueprintHelperTargetRef& Target,
	FBlueprintHelperLogicReadSnapshot& OutSnapshot,
	FString& OutError,
	FBlueprintHelperLogicReadRequestSnapshotCache* RequestCache) const
{
	OutSnapshot = FBlueprintHelperLogicReadSnapshot();
	OutError.Reset();

	const EBlueprintHelperLogicScope Scope = TargetTypeToScope(Target.TargetType);
	const FBlueprintHelperLogicReadSnapshotCacheKey CacheKey = MakeCacheKey(Target, Scope);
	if (RequestCache && RequestCache->TryGet(CacheKey, OutSnapshot))
	{
		return true;
	}

	if (!ExportService)
	{
		OutError = TEXT("Logic read snapshot service has no export service.");
		return false;
	}

	FBlueprintHelperExportRequest ExportReq;
	ExportReq.Target.BlueprintPath = Target.AssetPath;
	if (!Target.Graph.IsEmpty())
	{
		ExportReq.Target.GraphName = Target.Graph;
	}
	ExportReq.Scope = IsTargetEntryScope(Scope) && Target.Graph.IsEmpty()
		? EBlueprintHelperExportScope::FullBlueprint
		: ScopeToExportScope(Scope);

	const FBlueprintHelperExportResult ExportResult = ExportService->Export(ExportReq);

	OutSnapshot.Target = Target;
	OutSnapshot.Scope = Scope;
	OutSnapshot.ExportScope = ExportReq.Scope;
	OutSnapshot.AssetPath = Target.AssetPath;
	OutSnapshot.GraphName = Target.Graph.IsEmpty() ? TEXT("EventGraph") : Target.Graph;
	OutSnapshot.TargetEntryName = GetTargetEntryName(Target, Scope);
	OutSnapshot.bTargetEntryScope = IsTargetEntryScope(Scope);
	OutSnapshot.bExportSucceeded = ExportResult.bSuccess && ExportResult.JsonObject.IsValid();
	OutSnapshot.RawJsonObject = ExportResult.JsonObject;

	if (!OutSnapshot.bExportSucceeded)
	{
		OutError = TEXT("Logic read snapshot export failed.");
		return false;
	}

	if (RequestCache)
	{
		RequestCache->Put(CacheKey, OutSnapshot);
	}
	return true;
}

FBlueprintHelperLogicReadSnapshotCacheKey FBlueprintHelperLogicReadSnapshotService::MakeCacheKey(
	const FBlueprintHelperTargetRef& Target,
	EBlueprintHelperLogicScope Scope)
{
	FBlueprintHelperLogicReadSnapshotCacheKey Key;
	Key.AssetPath = Target.AssetPath;
	Key.GraphName = Target.Graph.IsEmpty() ? TEXT("EventGraph") : Target.Graph;
	Key.Scope = LogicScopeToString(Scope);
	Key.ReadDetail = TEXT("default");
	Key.SchemaVersion = TEXT("LogicReadSnapshot.v1");
	return Key;
}

EBlueprintHelperLogicScope FBlueprintHelperLogicReadSnapshotService::TargetTypeToScope(
	EBlueprintHelperTargetType Type)
{
	if (const EBlueprintHelperLogicScope* Scope =
		FBlueprintHelperLogicReadSnapshotServiceLocalUtils::TargetScopeMap().Find(Type))
	{
		return *Scope;
	}
	return EBlueprintHelperLogicScope::TargetGraph;
}

EBlueprintHelperExportScope FBlueprintHelperLogicReadSnapshotService::ScopeToExportScope(
	EBlueprintHelperLogicScope Scope)
{
	if (const EBlueprintHelperExportScope* ExportScope =
		FBlueprintHelperLogicReadSnapshotServiceLocalUtils::ExportScopeMap().Find(Scope))
	{
		return *ExportScope;
	}
	return EBlueprintHelperExportScope::SingleGraph;
}

bool FBlueprintHelperLogicReadSnapshotService::IsTargetEntryScope(
	EBlueprintHelperLogicScope Scope)
{
	return FBlueprintHelperLogicReadSnapshotServiceLocalUtils::TargetEntryScopes().Contains(Scope);
}

FString FBlueprintHelperLogicReadSnapshotService::GetTargetEntryName(
	const FBlueprintHelperTargetRef& Target,
	EBlueprintHelperLogicScope Scope)
{
	if (Scope == EBlueprintHelperLogicScope::TargetFunction)
	{
		return Target.Function;
	}
	if (Scope == EBlueprintHelperLogicScope::TargetEvent ||
		Scope == EBlueprintHelperLogicScope::TargetCustomEvent)
	{
		return Target.Event;
	}
	return TEXT("");
}
