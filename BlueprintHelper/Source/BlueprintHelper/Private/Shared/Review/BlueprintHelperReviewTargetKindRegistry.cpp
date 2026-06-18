// BlueprintHelper Review target kind registry implementation.

#include "Shared/Review/BlueprintHelperReviewTargetKindRegistry.h"

struct FBlueprintHelperReviewSurfaceAliasRule
{
	const TCHAR* Token;
	EBlueprintHelperReviewSurface Surface;
};

static FString BlueprintHelperReviewNormalizeRegistryToken(FString Value)
{
	Value.TrimStartAndEndInline();
	Value.ToLowerInline();
	return Value;
}

static bool BlueprintHelperReviewTextContainsToken(const FString& NormalizedText, const TCHAR* Token)
{
	return Token && NormalizedText.Contains(Token);
}

static const FBlueprintHelperReviewTargetKindDefinition* BlueprintHelperReviewFindExactDefinition(
	const FString& NormalizedTargetKind)
{
	static const FBlueprintHelperReviewTargetKindDefinition Definitions[] =
	{
		{ TEXT("graph_block"), EBlueprintHelperReviewSurface::Graph, false, TEXT("graph_block"), EBlueprintHelperReviewTargetHandlerKind::GraphBlock },
		{ TEXT("graph_external_boundary"), EBlueprintHelperReviewSurface::Graph, false, TEXT("graph_external_boundary"), EBlueprintHelperReviewTargetHandlerKind::GraphExternalBoundary },
		{ TEXT("graph_external_link"), EBlueprintHelperReviewSurface::Graph, false, TEXT("graph_external_link"), EBlueprintHelperReviewTargetHandlerKind::GraphExternalLink },
		{ TEXT("graph_external_node"), EBlueprintHelperReviewSurface::Graph, false, TEXT("graph_external_node"), EBlueprintHelperReviewTargetHandlerKind::GraphExternalNode },
		{ TEXT("graph_external_body"), EBlueprintHelperReviewSurface::Graph, false, TEXT("graph_external_body"), EBlueprintHelperReviewTargetHandlerKind::GraphExternalBody },
		{ TEXT("graph_node"), EBlueprintHelperReviewSurface::Graph, false, TEXT("graph_node"), EBlueprintHelperReviewTargetHandlerKind::GraphNode },
		{ TEXT("graph_pin"), EBlueprintHelperReviewSurface::Graph, false, TEXT("graph_pin"), EBlueprintHelperReviewTargetHandlerKind::GraphNode },
		{ TEXT("graph_link"), EBlueprintHelperReviewSurface::Graph, false, TEXT("graph_link"), EBlueprintHelperReviewTargetHandlerKind::GraphNode },
		{ TEXT("material_expression"), EBlueprintHelperReviewSurface::Material, false, TEXT("material_expression"), EBlueprintHelperReviewTargetHandlerKind::MaterialGraph },
		{ TEXT("material_expression_link"), EBlueprintHelperReviewSurface::Material, false, TEXT("material_expression_link"), EBlueprintHelperReviewTargetHandlerKind::MaterialGraph },
		{ TEXT("material_output_link"), EBlueprintHelperReviewSurface::Material, false, TEXT("material_output_link"), EBlueprintHelperReviewTargetHandlerKind::MaterialGraph },
		{ TEXT("material_instance"), EBlueprintHelperReviewSurface::Material, false, TEXT("material_instance"), EBlueprintHelperReviewTargetHandlerKind::MaterialInstance },
		{ TEXT("material_instance_parameter"), EBlueprintHelperReviewSurface::Material, false, TEXT("material_parameter"), EBlueprintHelperReviewTargetHandlerKind::MaterialInstance },
		{ TEXT("component"), EBlueprintHelperReviewSurface::Components, true, TEXT("component"), EBlueprintHelperReviewTargetHandlerKind::Component },
		{ TEXT("blueprint_variable"), EBlueprintHelperReviewSurface::MyBlueprint, true, TEXT("variable"), EBlueprintHelperReviewTargetHandlerKind::BlueprintVariable },
		{ TEXT("variable_default"), EBlueprintHelperReviewSurface::MyBlueprint, true, TEXT("variable"), EBlueprintHelperReviewTargetHandlerKind::BlueprintVariable },
		{ TEXT("signature"), EBlueprintHelperReviewSurface::MyBlueprint, true, TEXT("signature"), EBlueprintHelperReviewTargetHandlerKind::Signature },
		{ TEXT("dispatcher"), EBlueprintHelperReviewSurface::MyBlueprint, true, TEXT("dispatcher"), EBlueprintHelperReviewTargetHandlerKind::Signature },
		{ TEXT("delegate"), EBlueprintHelperReviewSurface::MyBlueprint, true, TEXT("delegate"), EBlueprintHelperReviewTargetHandlerKind::Signature },
		{ TEXT("function"), EBlueprintHelperReviewSurface::MyBlueprint, true, TEXT("function"), EBlueprintHelperReviewTargetHandlerKind::Signature },
		{ TEXT("macro"), EBlueprintHelperReviewSurface::MyBlueprint, true, TEXT("macro"), EBlueprintHelperReviewTargetHandlerKind::Signature },
		{ TEXT("class_setting"), EBlueprintHelperReviewSurface::Details, true, TEXT("class_setting"), EBlueprintHelperReviewTargetHandlerKind::ObjectProperty },
		{ TEXT("class_setting_interface"), EBlueprintHelperReviewSurface::Details, true, TEXT("interface"), EBlueprintHelperReviewTargetHandlerKind::ObjectProperty },
		{ TEXT("class_default_property"), EBlueprintHelperReviewSurface::Details, true, TEXT("property"), EBlueprintHelperReviewTargetHandlerKind::ObjectProperty },
		{ TEXT("umg_widget_tree"), EBlueprintHelperReviewSurface::UMGWidgetTree, false, TEXT("widget_tree"), EBlueprintHelperReviewTargetHandlerKind::UMGWidget },
		{ TEXT("umg_widget"), EBlueprintHelperReviewSurface::UMGWidgetTree, false, TEXT("widget"), EBlueprintHelperReviewTargetHandlerKind::UMGWidget },
		{ TEXT("umg_widget_property"), EBlueprintHelperReviewSurface::UMGWidgetTree, true, TEXT("widget_property"), EBlueprintHelperReviewTargetHandlerKind::UMGWidgetProperty },
		{ TEXT("slot_property"), EBlueprintHelperReviewSurface::UMGWidgetTree, true, TEXT("slot_property"), EBlueprintHelperReviewTargetHandlerKind::UMGWidgetProperty },
		{ TEXT("widget_variable"), EBlueprintHelperReviewSurface::UMGWidgetTree, true, TEXT("widget_variable"), EBlueprintHelperReviewTargetHandlerKind::UMGWidgetProperty },
		{ TEXT("datatable_row"), EBlueprintHelperReviewSurface::DataTable, false, TEXT("row"), EBlueprintHelperReviewTargetHandlerKind::DataTableRow },
		{ TEXT("struct_field"), EBlueprintHelperReviewSurface::DataAsset, false, TEXT("field"), EBlueprintHelperReviewTargetHandlerKind::StructField },
		{ TEXT("structure_field"), EBlueprintHelperReviewSurface::DataAsset, false, TEXT("field"), EBlueprintHelperReviewTargetHandlerKind::StructField },
		{ TEXT("data_asset_property"), EBlueprintHelperReviewSurface::DataAsset, true, TEXT("property"), EBlueprintHelperReviewTargetHandlerKind::ObjectProperty },
		{ TEXT("object_property"), EBlueprintHelperReviewSurface::DataAsset, true, TEXT("property"), EBlueprintHelperReviewTargetHandlerKind::ObjectProperty },
		{ TEXT("asset_factory"), EBlueprintHelperReviewSurface::Unknown, false, TEXT("asset"), EBlueprintHelperReviewTargetHandlerKind::AssetFactory }
	};

	for (const FBlueprintHelperReviewTargetKindDefinition& Definition : Definitions)
	{
		if (NormalizedTargetKind.Equals(Definition.TargetKind, ESearchCase::IgnoreCase))
		{
			return &Definition;
		}
	}
	return nullptr;
}

static EBlueprintHelperReviewSurface BlueprintHelperReviewResolveSurfaceByAlias(const FString& NormalizedText)
{
	static const FBlueprintHelperReviewSurfaceAliasRule Rules[] =
	{
		{ TEXT("graph_block"), EBlueprintHelperReviewSurface::Graph },
		{ TEXT("graph_external_boundary"), EBlueprintHelperReviewSurface::Graph },
		{ TEXT("graph_external_link"), EBlueprintHelperReviewSurface::Graph },
		{ TEXT("graph_external_node"), EBlueprintHelperReviewSurface::Graph },
		{ TEXT("graph_external_body"), EBlueprintHelperReviewSurface::Graph },
		{ TEXT("graph_node"), EBlueprintHelperReviewSurface::Graph },
		{ TEXT("graph_pin"), EBlueprintHelperReviewSurface::Graph },
		{ TEXT("graph_link"), EBlueprintHelperReviewSurface::Graph },
		{ TEXT("material_expression"), EBlueprintHelperReviewSurface::Material },
		{ TEXT("material_output_link"), EBlueprintHelperReviewSurface::Material },
		{ TEXT("material_expression_link"), EBlueprintHelperReviewSurface::Material },
		{ TEXT("material_instance"), EBlueprintHelperReviewSurface::Material },
		{ TEXT("material_instance_parameter"), EBlueprintHelperReviewSurface::Material },
		{ TEXT("material"), EBlueprintHelperReviewSurface::Material },
		{ TEXT("graph:"), EBlueprintHelperReviewSurface::Graph },
		{ TEXT("node:"), EBlueprintHelperReviewSurface::Graph },
		{ TEXT("pin:"), EBlueprintHelperReviewSurface::Graph },
		{ TEXT("component"), EBlueprintHelperReviewSurface::Components },
		{ TEXT("my_blueprint"), EBlueprintHelperReviewSurface::MyBlueprint },
		{ TEXT("blueprint_variable"), EBlueprintHelperReviewSurface::MyBlueprint },
		{ TEXT("signature"), EBlueprintHelperReviewSurface::MyBlueprint },
		{ TEXT("dispatcher"), EBlueprintHelperReviewSurface::MyBlueprint },
		{ TEXT("delegate"), EBlueprintHelperReviewSurface::MyBlueprint },
		{ TEXT("function"), EBlueprintHelperReviewSurface::MyBlueprint },
		{ TEXT("macro"), EBlueprintHelperReviewSurface::MyBlueprint },
		{ TEXT("umg_widget"), EBlueprintHelperReviewSurface::UMGWidgetTree },
		{ TEXT("slot_property"), EBlueprintHelperReviewSurface::UMGWidgetTree },
		{ TEXT("widget_variable"), EBlueprintHelperReviewSurface::UMGWidgetTree },
		{ TEXT("widget_tree"), EBlueprintHelperReviewSurface::UMGWidgetTree },
		{ TEXT("widgetblueprint"), EBlueprintHelperReviewSurface::UMGWidgetTree },
		{ TEXT("widget_blueprint"), EBlueprintHelperReviewSurface::UMGWidgetTree },
		{ TEXT("datatable"), EBlueprintHelperReviewSurface::DataTable },
		{ TEXT("data_table"), EBlueprintHelperReviewSurface::DataTable },
		{ TEXT("data_asset_property"), EBlueprintHelperReviewSurface::DataAsset },
		{ TEXT("dataasset"), EBlueprintHelperReviewSurface::DataAsset },
		{ TEXT("data_asset"), EBlueprintHelperReviewSurface::DataAsset },
		{ TEXT("structure"), EBlueprintHelperReviewSurface::DataAsset },
		{ TEXT("struct_field"), EBlueprintHelperReviewSurface::DataAsset },
		{ TEXT("object_property"), EBlueprintHelperReviewSurface::DataAsset }
	};

	for (const FBlueprintHelperReviewSurfaceAliasRule& Rule : Rules)
	{
		if (BlueprintHelperReviewTextContainsToken(NormalizedText, Rule.Token))
		{
			return Rule.Surface;
		}
	}
	return EBlueprintHelperReviewSurface::Unknown;
}

EBlueprintHelperReviewSurface FBlueprintHelperReviewTargetKindRegistry::NormalizeSurfaceForTarget(
	EBlueprintHelperReviewSurface ExplicitSurface,
	const FString& TargetKind,
	const FString& TargetKey,
	const FString& VisualGroupKey,
	const FString& LocationKey)
{
	const FString NormalizedTargetKind = BlueprintHelperReviewNormalizeRegistryToken(TargetKind);
	if (const FBlueprintHelperReviewTargetKindDefinition* Definition = FindDefinition(NormalizedTargetKind))
	{
		if (Definition->Surface != EBlueprintHelperReviewSurface::Unknown)
		{
			return Definition->Surface;
		}
	}

	const FString CombinedText = BlueprintHelperReviewNormalizeRegistryToken(
		TargetKind + TEXT(" ") + TargetKey + TEXT(" ") + VisualGroupKey + TEXT(" ") + LocationKey);
	const EBlueprintHelperReviewSurface AliasSurface = BlueprintHelperReviewResolveSurfaceByAlias(CombinedText);
	if (AliasSurface != EBlueprintHelperReviewSurface::Unknown)
	{
		return AliasSurface;
	}

	if (ExplicitSurface != EBlueprintHelperReviewSurface::Unknown)
	{
		return ExplicitSurface;
	}
	return EBlueprintHelperReviewSurface::Details;
}

bool FBlueprintHelperReviewTargetKindRegistry::CanRouteToDetails(const FString& TargetKind)
{
	const FString NormalizedTargetKind = BlueprintHelperReviewNormalizeRegistryToken(TargetKind);
	if (const FBlueprintHelperReviewTargetKindDefinition* Definition = FindDefinition(NormalizedTargetKind))
	{
		return Definition->bCanRouteToDetails;
	}

	static const TCHAR* DetailAliases[] =
	{
		TEXT("class_default"),
		TEXT("blueprint_default"),
		TEXT("blueprint_setting"),
		TEXT("blueprint_variable"),
		TEXT("variable"),
		TEXT("component"),
		TEXT("object_property"),
		TEXT("property"),
		TEXT("class_setting"),
		TEXT("blueprint_class"),
		TEXT("interface"),
		TEXT("signature"),
		TEXT("dispatcher"),
		TEXT("variable_default")
	};

	for (const TCHAR* Alias : DetailAliases)
	{
		if (BlueprintHelperReviewTextContainsToken(NormalizedTargetKind, Alias))
		{
			return true;
		}
	}
	return false;
}

bool FBlueprintHelperReviewTargetKindRegistry::IsComponentTargetKind(const FString& TargetKind)
{
	const FString NormalizedTargetKind = BlueprintHelperReviewNormalizeRegistryToken(TargetKind);
	if (const FBlueprintHelperReviewTargetKindDefinition* Definition = FindDefinition(NormalizedTargetKind))
	{
		return Definition->Surface == EBlueprintHelperReviewSurface::Components;
	}
	return BlueprintHelperReviewTextContainsToken(NormalizedTargetKind, TEXT("component"));
}

bool FBlueprintHelperReviewTargetKindRegistry::IsPropertyTargetKind(const FString& TargetKind)
{
	const FString NormalizedTargetKind = BlueprintHelperReviewNormalizeRegistryToken(TargetKind);
	if (const FBlueprintHelperReviewTargetKindDefinition* Definition = FindDefinition(NormalizedTargetKind))
	{
		return FString(Definition->DisplayKind).Contains(TEXT("property"));
	}
	return BlueprintHelperReviewTextContainsToken(NormalizedTargetKind, TEXT("property"));
}

bool FBlueprintHelperReviewTargetKindRegistry::IsClassDefaultPropertyTargetKind(const FString& TargetKind)
{
	return BlueprintHelperReviewNormalizeRegistryToken(TargetKind).Equals(
		TEXT("class_default_property"),
		ESearchCase::IgnoreCase);
}

bool FBlueprintHelperReviewTargetKindRegistry::IsAssetFactoryTargetKind(const FString& TargetKind)
{
	return GetHandlerKind(TargetKind) == EBlueprintHelperReviewTargetHandlerKind::AssetFactory;
}

bool FBlueprintHelperReviewTargetKindRegistry::IsGraphNodeTarget(
	const FString& TargetKind,
	const FString& TargetKey)
{
	const FString NormalizedTargetKind = BlueprintHelperReviewNormalizeRegistryToken(TargetKind);
	const FString NormalizedTargetKey = BlueprintHelperReviewNormalizeRegistryToken(TargetKey);
	return NormalizedTargetKind.Equals(TEXT("graph_node"), ESearchCase::IgnoreCase)
		|| BlueprintHelperReviewTextContainsToken(NormalizedTargetKey, TEXT(":node:"));
}

bool FBlueprintHelperReviewTargetKindRegistry::IsGraphBlockTarget(
	const FString& TargetKind,
	const FString& TargetKey)
{
	const FString NormalizedTargetKind = BlueprintHelperReviewNormalizeRegistryToken(TargetKind);
	const FString NormalizedTargetKey = BlueprintHelperReviewNormalizeRegistryToken(TargetKey);
	return NormalizedTargetKind.Equals(TEXT("graph_block"), ESearchCase::IgnoreCase)
		|| BlueprintHelperReviewTextContainsToken(NormalizedTargetKey, TEXT(":block:"));
}

bool FBlueprintHelperReviewTargetKindRegistry::ShouldAggregateAsGraphBody(
	const FBlueprintHelperReviewAtomicTarget& Target)
{
	if (Target.Surface != EBlueprintHelperReviewSurface::Graph || Target.GraphName.IsEmpty())
	{
		return false;
	}

	const FString TargetKindLower = BlueprintHelperReviewNormalizeRegistryToken(Target.TargetKind);
	const FString TargetKeyLower = BlueprintHelperReviewNormalizeRegistryToken(Target.TargetKey);
	const FString GroupLower = BlueprintHelperReviewNormalizeRegistryToken(Target.VisualGroupKey);
	if (TargetKindLower.Equals(TEXT("graph_external_boundary"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	if (TargetKindLower.Equals(TEXT("graph_external_link"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	if (TargetKindLower.Equals(TEXT("graph_external_node"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	if (TargetKindLower.Equals(TEXT("graph_external_body"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	if (IsGraphBlockTarget(Target.TargetKind, Target.TargetKey)
		|| BlueprintHelperReviewTextContainsToken(TargetKeyLower, TEXT(":block:"))
		|| BlueprintHelperReviewTextContainsToken(GroupLower, TEXT(":block:")))
	{
		return false;
	}

	static const TCHAR* NonGraphBodyTokens[] =
	{
		TEXT("component"),
		TEXT("variable"),
		TEXT("property")
	};

	for (const TCHAR* Token : NonGraphBodyTokens)
	{
		if (BlueprintHelperReviewTextContainsToken(TargetKindLower, Token)
			|| BlueprintHelperReviewTextContainsToken(GroupLower, Token))
		{
			return false;
		}
	}
	return true;
}

EBlueprintHelperReviewSurface FBlueprintHelperReviewTargetKindRegistry::ResolveAssetFactorySurface(
	const FString& AssetType)
{
	struct FBlueprintHelperReviewAssetFactorySurfaceRule
	{
		const TCHAR* AssetType;
		EBlueprintHelperReviewSurface Surface;
	};

	static const FBlueprintHelperReviewAssetFactorySurfaceRule Rules[] =
	{
		{ TEXT("data_table"), EBlueprintHelperReviewSurface::DataTable },
		{ TEXT("datatable"), EBlueprintHelperReviewSurface::DataTable },
		{ TEXT("widget_blueprint"), EBlueprintHelperReviewSurface::UMGWidgetTree },
		{ TEXT("widgetblueprint"), EBlueprintHelperReviewSurface::UMGWidgetTree },
		{ TEXT("structure"), EBlueprintHelperReviewSurface::DataAsset },
		{ TEXT("struct"), EBlueprintHelperReviewSurface::DataAsset },
		{ TEXT("data_asset"), EBlueprintHelperReviewSurface::DataAsset },
		{ TEXT("dataasset"), EBlueprintHelperReviewSurface::DataAsset },
		{ TEXT("blueprint_class"), EBlueprintHelperReviewSurface::DataAsset },
		{ TEXT("blueprint_interface"), EBlueprintHelperReviewSurface::DataAsset },
		{ TEXT("material"), EBlueprintHelperReviewSurface::Material },
		{ TEXT("mat"), EBlueprintHelperReviewSurface::Material },
		{ TEXT("input_action"), EBlueprintHelperReviewSurface::DataAsset },
		{ TEXT("input_mapping_context"), EBlueprintHelperReviewSurface::DataAsset }
	};

	const FString NormalizedAssetType = BlueprintHelperReviewNormalizeRegistryToken(AssetType);
	for (const FBlueprintHelperReviewAssetFactorySurfaceRule& Rule : Rules)
	{
		if (NormalizedAssetType.Equals(Rule.AssetType, ESearchCase::IgnoreCase))
		{
			return Rule.Surface;
		}
	}
	return EBlueprintHelperReviewSurface::DataAsset;
}

bool FBlueprintHelperReviewTargetKindRegistry::IsStructureAssetType(const FString& AssetType)
{
	const FString NormalizedAssetType = BlueprintHelperReviewNormalizeRegistryToken(AssetType);
	static const TCHAR* StructureAliases[] =
	{
		TEXT("structure"),
		TEXT("struct")
	};

	for (const TCHAR* Alias : StructureAliases)
	{
		if (NormalizedAssetType.Equals(Alias, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

EBlueprintHelperReviewTargetHandlerKind FBlueprintHelperReviewTargetKindRegistry::GetHandlerKind(
	const FString& TargetKind)
{
	if (const FBlueprintHelperReviewTargetKindDefinition* Definition = FindDefinition(TargetKind))
	{
		return Definition->HandlerKind;
	}
	return EBlueprintHelperReviewTargetHandlerKind::Unsupported;
}

bool FBlueprintHelperReviewTargetKindRegistry::SupportsSnapshotRestore(const FString& TargetKind)
{
	const FString NormalizedTargetKind = BlueprintHelperReviewNormalizeRegistryToken(TargetKind);
	if (NormalizedTargetKind.Equals(TEXT("graph_node"), ESearchCase::IgnoreCase)
		|| NormalizedTargetKind.Equals(TEXT("graph_block"), ESearchCase::IgnoreCase)
		|| NormalizedTargetKind.Equals(TEXT("graph_external_boundary"), ESearchCase::IgnoreCase)
		|| NormalizedTargetKind.Equals(TEXT("graph_external_link"), ESearchCase::IgnoreCase)
		|| NormalizedTargetKind.Equals(TEXT("graph_external_node"), ESearchCase::IgnoreCase)
		|| NormalizedTargetKind.Equals(TEXT("graph_external_body"), ESearchCase::IgnoreCase))
	{
		return true;
	}

	static const EBlueprintHelperReviewTargetHandlerKind RestorableKinds[] =
	{
		EBlueprintHelperReviewTargetHandlerKind::BlueprintVariable,
		EBlueprintHelperReviewTargetHandlerKind::Component,
		EBlueprintHelperReviewTargetHandlerKind::DataTableRow,
		EBlueprintHelperReviewTargetHandlerKind::StructField,
		EBlueprintHelperReviewTargetHandlerKind::ObjectProperty,
		EBlueprintHelperReviewTargetHandlerKind::MaterialGraph,
		EBlueprintHelperReviewTargetHandlerKind::MaterialInstance,
		EBlueprintHelperReviewTargetHandlerKind::Signature,
		EBlueprintHelperReviewTargetHandlerKind::UMGWidget,
		EBlueprintHelperReviewTargetHandlerKind::UMGWidgetProperty,
		EBlueprintHelperReviewTargetHandlerKind::AssetFactory
	};

	const EBlueprintHelperReviewTargetHandlerKind HandlerKind = GetHandlerKind(TargetKind);
	for (const EBlueprintHelperReviewTargetHandlerKind RestorableKind : RestorableKinds)
	{
		if (HandlerKind == RestorableKind)
		{
			return true;
		}
	}
	return false;
}

const FBlueprintHelperReviewTargetKindDefinition* FBlueprintHelperReviewTargetKindRegistry::FindDefinition(
	const FString& TargetKind)
{
	return BlueprintHelperReviewFindExactDefinition(BlueprintHelperReviewNormalizeRegistryToken(TargetKind));
}

EBlueprintHelperReviewSurface BlueprintHelperReviewNormalizeSurfaceForTarget(
	EBlueprintHelperReviewSurface Surface,
	const FString& TargetKind,
	const FString& TargetKey,
	const FString& VisualGroupKey,
	const FString& LocationKey)
{
	return FBlueprintHelperReviewTargetKindRegistry::NormalizeSurfaceForTarget(
		Surface,
		TargetKind,
		TargetKey,
		VisualGroupKey,
		LocationKey);
}

FString BlueprintHelperReviewNormalizeLocation(const FBlueprintHelperReviewVisibleChange& Change)
{
	FString Location = Change.LocationKey;
	if (Location.IsEmpty())
	{
		Location = Change.DisplayLabel;
	}
	Location.ToLowerInline();
	return Location;
}

int32 BlueprintHelperReviewCountSurfaceTargets(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface)
{
	int32 Count = 0;
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		if (Target.Surface == Surface)
		{
			++Count;
		}
	}
	return Count;
}

bool BlueprintHelperReviewTargetKindCanRouteToDetails(const FString& TargetKind)
{
	return FBlueprintHelperReviewTargetKindRegistry::CanRouteToDetails(TargetKind);
}

int32 BlueprintHelperReviewCountDetailsTargets(const FBlueprintHelperReviewVisibleChange& Change)
{
	int32 Count = 0;
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		if (Target.Surface == EBlueprintHelperReviewSurface::Details)
		{
			++Count;
			continue;
		}

		if (Target.Surface != EBlueprintHelperReviewSurface::DataAsset
			&& Target.Surface != EBlueprintHelperReviewSurface::DataTable
			&& Target.Surface != EBlueprintHelperReviewSurface::UMGWidgetTree
			&& Target.Surface != EBlueprintHelperReviewSurface::Material
			&& FBlueprintHelperReviewTargetKindRegistry::CanRouteToDetails(Target.TargetKind))
		{
			++Count;
		}
	}
	return Count;
}

bool BlueprintHelperReviewHasExplicitTargets(const FBlueprintHelperReviewVisibleChange& Change)
{
	return Change.AtomicTargets.Num() > 0;
}

bool BlueprintHelperReviewShouldShowOnSurface(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface)
{
	if (BlueprintHelperReviewHasExplicitTargets(Change))
	{
		return Surface == EBlueprintHelperReviewSurface::Details
			? BlueprintHelperReviewCountDetailsTargets(Change) > 0
			: BlueprintHelperReviewCountSurfaceTargets(Change, Surface) > 0;
	}

	return false;
}

bool BlueprintHelperReviewShouldShowInComponents(const FBlueprintHelperReviewVisibleChange& Change)
{
	return BlueprintHelperReviewShouldShowOnSurface(Change, EBlueprintHelperReviewSurface::Components);
}

bool BlueprintHelperReviewShouldShowInGraph(const FBlueprintHelperReviewVisibleChange& Change)
{
	return BlueprintHelperReviewShouldShowOnSurface(Change, EBlueprintHelperReviewSurface::Graph);
}

bool BlueprintHelperReviewShouldShowInDetails(const FBlueprintHelperReviewVisibleChange& Change)
{
	return BlueprintHelperReviewShouldShowOnSurface(Change, EBlueprintHelperReviewSurface::Details);
}

bool BlueprintHelperReviewShouldShowInUMGWidgetTree(const FBlueprintHelperReviewVisibleChange& Change)
{
	return BlueprintHelperReviewShouldShowOnSurface(Change, EBlueprintHelperReviewSurface::UMGWidgetTree);
}

bool BlueprintHelperReviewShouldShowInDataTable(const FBlueprintHelperReviewVisibleChange& Change)
{
	return BlueprintHelperReviewShouldShowOnSurface(Change, EBlueprintHelperReviewSurface::DataTable);
}

bool BlueprintHelperReviewShouldShowInDataAsset(const FBlueprintHelperReviewVisibleChange& Change)
{
	return BlueprintHelperReviewShouldShowOnSurface(Change, EBlueprintHelperReviewSurface::DataAsset);
}

bool BlueprintHelperReviewShouldShowInMaterial(const FBlueprintHelperReviewVisibleChange& Change)
{
	return BlueprintHelperReviewShouldShowOnSurface(Change, EBlueprintHelperReviewSurface::Material);
}

bool BlueprintHelperReviewShouldShowInMyBlueprint(const FBlueprintHelperReviewVisibleChange& Change)
{
	return BlueprintHelperReviewShouldShowOnSurface(Change, EBlueprintHelperReviewSurface::MyBlueprint);
}
