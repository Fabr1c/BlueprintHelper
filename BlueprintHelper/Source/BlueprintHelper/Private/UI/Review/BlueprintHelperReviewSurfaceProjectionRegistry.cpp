// BlueprintHelper Review surface projection registry implementation.

#include "UI/Review/BlueprintHelperReviewSurfaceProjectionRegistry.h"
#include "UI/Review/BlueprintHelperReviewGenericSurfaceProjectionAdapter.h"

TSharedRef<FBlueprintHelperReviewSurfaceProjectionRegistry> FBlueprintHelperReviewSurfaceProjectionRegistry::CreateDefault()
{
	TSharedRef<FBlueprintHelperReviewSurfaceProjectionRegistry> Registry =
		MakeShared<FBlueprintHelperReviewSurfaceProjectionRegistry>();
	Registry->RegisterBuiltInAdapters();
	return Registry;
}

bool FBlueprintHelperReviewSurfaceProjectionRegistry::RegisterProjectionAdapter(
	const TSharedRef<IBlueprintHelperReviewSurfaceProjectionAdapter>& Adapter,
	TArray<FBlueprintHelperDiagnosticItem>& OutDiagnostics)
{
	const FString Key = MakeProjectionKey(Adapter->GetAssetKind(), Adapter->GetSurfaceKind(), Adapter->GetTargetKind());
	if (Key.IsEmpty())
	{
		OutDiagnostics.Add(MakeDiagnostic(TEXT("missing_surface_projection_key"), TEXT("Surface projection key is empty.")));
		return false;
	}
	if (AdaptersByProjectionKey.Contains(Key))
	{
		OutDiagnostics.Add(MakeDiagnostic(
			TEXT("duplicate_surface_projection_adapter"),
			FString::Printf(TEXT("Surface projection adapter already registered for '%s'."), *Key)));
		return false;
	}

	AdaptersByProjectionKey.Add(Key, Adapter);
	return true;
}

FBlueprintHelperReviewSurfaceProjectionLookup FBlueprintHelperReviewSurfaceProjectionRegistry::FindProjectionAdapter(
	const FBlueprintHelperReviewTargetIdentity& Identity) const
{
	FBlueprintHelperReviewSurfaceProjectionLookup Lookup;
	Lookup.ProjectionKey = MakeProjectionKey(Identity.AssetKind, Identity.SurfaceKind, Identity.TargetKind);
	const TSharedPtr<IBlueprintHelperReviewSurfaceProjectionAdapter>* Adapter =
		AdaptersByProjectionKey.Find(Lookup.ProjectionKey);
	if (!Adapter && !Identity.AssetKind.IsEmpty())
	{
		Lookup.ProjectionKey = MakeProjectionKey(FString(), Identity.SurfaceKind, Identity.TargetKind);
		Adapter = AdaptersByProjectionKey.Find(Lookup.ProjectionKey);
	}
	if (Adapter)
	{
		Lookup.bAvailable = Adapter->IsValid();
		Lookup.Adapter = *Adapter;
		Lookup.Message = Lookup.bAvailable ? TEXT("available") : TEXT("surface_projection_adapter_invalid");
		return Lookup;
	}

	Lookup.Message = FString::Printf(TEXT("surface_projection_adapter_unavailable:%s"), *Lookup.ProjectionKey);
	Lookup.Diagnostics.Add(MakeDiagnostic(TEXT("surface_projection_adapter_unavailable"), Lookup.Message));
	return Lookup;
}

TArray<FBlueprintHelperReviewSurfaceDiffProjectionModel> FBlueprintHelperReviewSurfaceProjectionRegistry::ProjectVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Change,
	const FString& AssetKind,
	const FString& SurfaceKind) const
{
	TArray<FBlueprintHelperReviewSurfaceDiffProjectionModel> Models;
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		FBlueprintHelperReviewTargetIdentity Identity =
			FBlueprintHelperReviewTargetIdentity::FromAtomicTarget(Change, Target);
		Identity.AssetKind = AssetKind;
		Identity.SurfaceKind = SurfaceKind;
		const FBlueprintHelperReviewSurfaceProjectionLookup Lookup = FindProjectionAdapter(Identity);
		if (!Lookup.bAvailable || !Lookup.Adapter.IsValid())
		{
			continue;
		}

		Models.Append(Lookup.Adapter->Project(Change).DiffModels);
	}
	return Models;
}

void FBlueprintHelperReviewSurfaceProjectionRegistry::RegisterBuiltInAdapters()
{
	TArray<FBlueprintHelperDiagnosticItem> Diagnostics;
	const TArray<TPair<FString, FString>> BuiltIns =
	{
		{ TEXT("graph"), TEXT("graph_node") },
		{ TEXT("graph"), TEXT("graph_block") },
		{ TEXT("graph"), TEXT("graph_external_boundary") },
		{ TEXT("graph"), TEXT("graph_external_link") },
		{ TEXT("graph"), TEXT("graph_external_node") },
		{ TEXT("graph"), TEXT("graph_external_body") },
		{ TEXT("my_blueprint"), TEXT("blueprint_variable") },
		{ TEXT("my_blueprint"), TEXT("variable_default") },
		{ TEXT("my_blueprint"), TEXT("signature") },
		{ TEXT("my_blueprint"), TEXT("dispatcher") },
		{ TEXT("my_blueprint"), TEXT("delegate") },
		{ TEXT("my_blueprint"), TEXT("function") },
		{ TEXT("my_blueprint"), TEXT("macro") },
		{ TEXT("components"), TEXT("component") },
		{ TEXT("umg_widget_tree"), TEXT("umg_widget_tree") },
		{ TEXT("umg_widget_tree"), TEXT("umg_widget") },
		{ TEXT("umg_widget_tree"), TEXT("umg_widget_property") },
		{ TEXT("details"), TEXT("component") },
		{ TEXT("details"), TEXT("blueprint_variable") },
		{ TEXT("details"), TEXT("variable_default") },
		{ TEXT("details"), TEXT("signature") },
		{ TEXT("details"), TEXT("dispatcher") },
		{ TEXT("details"), TEXT("delegate") },
		{ TEXT("details"), TEXT("function") },
		{ TEXT("details"), TEXT("macro") },
		{ TEXT("details"), TEXT("class_setting") },
		{ TEXT("details"), TEXT("class_setting_interface") },
		{ TEXT("details"), TEXT("class_default_property") },
		{ TEXT("data_table"), TEXT("datatable_row") },
		{ TEXT("data_table"), TEXT("asset_factory") },
		{ TEXT("data_asset"), TEXT("struct_field") },
		{ TEXT("data_asset"), TEXT("structure_field") },
		{ TEXT("data_asset"), TEXT("data_asset_property") },
		{ TEXT("data_asset"), TEXT("object_property") },
		{ TEXT("data_asset"), TEXT("asset_factory") },
		{ TEXT("umg_widget_tree"), TEXT("asset_factory") },
		{ TEXT("material"), TEXT("asset_factory") },
		{ TEXT("material"), TEXT("material_expression") },
		{ TEXT("material"), TEXT("material_expression_link") },
		{ TEXT("material"), TEXT("material_output_link") }
	};

	for (const TPair<FString, FString>& BuiltIn : BuiltIns)
	{
		RegisterProjectionAdapter(
			MakeShared<FBlueprintHelperReviewGenericSurfaceProjectionAdapter>(
				FString(),
				BuiltIn.Key,
				BuiltIn.Value),
			Diagnostics);
	}
}

FString FBlueprintHelperReviewSurfaceProjectionRegistry::MakeProjectionKey(
	const FString& AssetKind,
	const FString& SurfaceKind,
	const FString& TargetKind)
{
	const FString NormalizedAssetKind = NormalizeKeyPart(AssetKind);
	const FString NormalizedSurfaceKind = NormalizeKeyPart(SurfaceKind);
	const FString NormalizedTargetKind = NormalizeKeyPart(TargetKind);
	if (NormalizedSurfaceKind.IsEmpty() || NormalizedTargetKind.IsEmpty())
	{
		return FString();
	}
	return FString::Printf(
		TEXT("%s|%s|%s"),
		*NormalizedAssetKind,
		*NormalizedSurfaceKind,
		*NormalizedTargetKind);
}

FString FBlueprintHelperReviewSurfaceProjectionRegistry::NormalizeKeyPart(const FString& Value)
{
	FString Normalized = Value;
	Normalized.TrimStartAndEndInline();
	Normalized.ToLowerInline();
	return Normalized;
}

FBlueprintHelperDiagnosticItem FBlueprintHelperReviewSurfaceProjectionRegistry::MakeDiagnostic(
	const FString& Code,
	const FString& Message)
{
	FBlueprintHelperDiagnosticItem Diagnostic;
	Diagnostic.Code = Code;
	Diagnostic.Message = Message;
	Diagnostic.Severity = EBlueprintHelperDiagnosticSeverity::Warning;
	return Diagnostic;
}
