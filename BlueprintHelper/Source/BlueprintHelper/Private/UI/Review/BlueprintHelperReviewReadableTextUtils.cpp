// BlueprintHelper Review readable text utilities.

#include "UI/Review/BlueprintHelperReviewReadableTextUtils.h"

#include "Shared/Review/BlueprintHelperReviewTargetKindRegistry.h"

FString FBlueprintHelperReviewReadableTextUtils::ExtractReadableTail(FString Text)
{
	Text.TrimStartAndEndInline();
	if (Text.IsEmpty())
	{
		return Text;
	}

	int32 DelimiterIndex = INDEX_NONE;
	if (Text.FindLastChar(TEXT(':'), DelimiterIndex)
		|| Text.FindLastChar(TEXT('/'), DelimiterIndex)
		|| Text.FindLastChar(TEXT('.'), DelimiterIndex))
	{
		Text = Text.Mid(DelimiterIndex + 1);
	}
	Text.TrimStartAndEndInline();

	if (Text.EndsWith(TEXT(" Widget"), ESearchCase::IgnoreCase))
	{
		Text.LeftChopInline(7);
	}
	if (Text.EndsWith(TEXT(" Row"), ESearchCase::IgnoreCase))
	{
		Text.LeftChopInline(4);
	}
	Text.TrimStartAndEndInline();
	return Text;
}

FString FBlueprintHelperReviewReadableTextUtils::ExtractAssetShortNameFromPath(FString AssetPath)
{
	AssetPath.TrimStartAndEndInline();
	if (AssetPath.IsEmpty())
	{
		return FString();
	}

	int32 DotIndex = INDEX_NONE;
	if (AssetPath.FindLastChar(TEXT('.'), DotIndex))
	{
		AssetPath = AssetPath.Left(DotIndex);
	}

	int32 SlashIndex = INDEX_NONE;
	if (AssetPath.FindLastChar(TEXT('/'), SlashIndex))
	{
		AssetPath = AssetPath.Mid(SlashIndex + 1);
	}
	AssetPath.TrimStartAndEndInline();
	return AssetPath;
}

FString FBlueprintHelperReviewReadableTextUtils::StripEncodedPackagePrefix(FString Text)
{
	Text.TrimStartAndEndInline();
	if (Text.IsEmpty())
	{
		return Text;
	}

	static const TCHAR* KnownAssetPrefixes[] =
	{
		TEXT("_BP_"),
		TEXT("_WBP_"),
		TEXT("_DT_"),
		TEXT("_ST_"),
		TEXT("_DA_"),
		TEXT("_BPI_")
	};
	for (const TCHAR* Prefix : KnownAssetPrefixes)
	{
		const int32 PrefixIndex = Text.Find(Prefix, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		if (PrefixIndex != INDEX_NONE)
		{
			return Text.Mid(PrefixIndex + 1);
		}
	}

	return Text;
}

bool FBlueprintHelperReviewReadableTextUtils::IsAssetFactoryChange(const FBlueprintHelperReviewVisibleChange& Change)
{
	return Change.AtomicTargets.ContainsByPredicate(
		[](const FBlueprintHelperReviewAtomicTarget& Target)
		{
			return FBlueprintHelperReviewTargetKindRegistry::IsAssetFactoryTargetKind(Target.TargetKind)
				|| Target.TargetKey.StartsWith(TEXT("asset_factory:"), ESearchCase::IgnoreCase);
		});
}

FString FBlueprintHelperReviewReadableTextUtils::GetAssetFactoryReadableName(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	const FString AssetName = ExtractAssetShortNameFromPath(Change.AssetPath);
	if (!AssetName.IsEmpty())
	{
		return AssetName;
	}

	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		for (const FString& Candidate : { Target.DisplayLabel, Target.TargetKey, Target.PropertyPath })
		{
			const FString Tail = StripEncodedPackagePrefix(ExtractReadableTail(Candidate));
			if (!Tail.IsEmpty()
				&& !Tail.Equals(TEXT("create_asset"), ESearchCase::IgnoreCase)
				&& !Tail.Equals(TEXT("asset_factory"), ESearchCase::IgnoreCase))
			{
				return Tail;
			}
		}
	}

	return StripEncodedPackagePrefix(ExtractReadableTail(
		Change.DisplayLabel.IsEmpty() ? Change.LocationKey : Change.DisplayLabel));
}

FString FBlueprintHelperReviewReadableTextUtils::GetAssetFactoryReadableSuffix(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	const FString AssetName = GetAssetFactoryReadableName(Change);
	const FString LowerAssetName = AssetName.ToLower();
	FString CombinedDescriptor = Change.AssetPath;
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		CombinedDescriptor += TEXT("|") + Target.TargetKey;
		CombinedDescriptor += TEXT("|") + Target.DisplayLabel;
		CombinedDescriptor += TEXT("|") + Target.TargetKind;
	}
	CombinedDescriptor.ToLowerInline();

	if (LowerAssetName.StartsWith(TEXT("wbp_")) || CombinedDescriptor.Contains(TEXT("widget")))
	{
		return TEXT("Widget Blueprint \u8d44\u4ea7");
	}
	if (LowerAssetName.StartsWith(TEXT("dt_")) || CombinedDescriptor.Contains(TEXT("data_table")))
	{
		return TEXT("DataTable \u8d44\u4ea7");
	}
	if (LowerAssetName.StartsWith(TEXT("st_")) || CombinedDescriptor.Contains(TEXT("structure")))
	{
		return TEXT("Structure \u8d44\u4ea7");
	}
	if (LowerAssetName.StartsWith(TEXT("da_")) || CombinedDescriptor.Contains(TEXT("data_asset")))
	{
		return TEXT("DataAsset \u8d44\u4ea7");
	}
	if (LowerAssetName.StartsWith(TEXT("bp_")) || LowerAssetName.StartsWith(TEXT("bpi_")))
	{
		return TEXT("\u84dd\u56fe\u8d44\u4ea7");
	}
	return TEXT("UObject \u8d44\u4ea7");
}

FString FBlueprintHelperReviewReadableTextUtils::GetReadableTargetName(const FBlueprintHelperReviewVisibleChange& Change)
{
	if (IsAssetFactoryChange(Change))
	{
		return GetAssetFactoryReadableName(Change);
	}

	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		if ((Target.Surface == EBlueprintHelperReviewSurface::UMGWidgetTree
			|| Target.Surface == EBlueprintHelperReviewSurface::DataTable)
			&& !Target.TargetKey.IsEmpty())
		{
			return ExtractReadableTail(Target.TargetKey);
		}
		if (!Target.PropertyPath.IsEmpty())
		{
			return ExtractReadableTail(Target.PropertyPath);
		}
		if (Target.Surface == EBlueprintHelperReviewSurface::Components
			&& !Target.ComponentPath.IsEmpty())
		{
			return ExtractReadableTail(Target.ComponentPath);
		}
		if (!Target.TargetKey.IsEmpty())
		{
			return ExtractReadableTail(Target.TargetKey);
		}
		if (!Target.DisplayLabel.IsEmpty())
		{
			return ExtractReadableTail(Target.DisplayLabel);
		}
	}

	return ExtractReadableTail(Change.DisplayLabel.IsEmpty() ? Change.LocationKey : Change.DisplayLabel);
}

FString FBlueprintHelperReviewReadableTextUtils::GetReadableTargetSuffix(const FBlueprintHelperReviewVisibleChange& Change)
{
	const FBlueprintHelperReviewAtomicTarget* Target = Change.AtomicTargets.Num() > 0
		? &Change.AtomicTargets[0]
		: nullptr;
	const EBlueprintHelperReviewTargetHandlerKind HandlerKind = Target
		? FBlueprintHelperReviewTargetKindRegistry::GetHandlerKind(Target->TargetKind)
		: EBlueprintHelperReviewTargetHandlerKind::Unsupported;
	const EBlueprintHelperReviewSurface Surface = Target ? Target->Surface : EBlueprintHelperReviewSurface::Unknown;

	if (IsAssetFactoryChange(Change))
	{
		return GetAssetFactoryReadableSuffix(Change);
	}
	if (Surface == EBlueprintHelperReviewSurface::UMGWidgetTree)
	{
		return FString();
	}
	if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::DataTableRow
		|| Surface == EBlueprintHelperReviewSurface::DataTable)
	{
		return TEXT("\u884c");
	}
	if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::Component
		|| Surface == EBlueprintHelperReviewSurface::Components)
	{
		return TEXT("\u7ec4\u4ef6");
	}
	if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::Signature)
	{
		return TEXT("\u7b7e\u540d");
	}
	if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::BlueprintVariable
		|| HandlerKind == EBlueprintHelperReviewTargetHandlerKind::ObjectProperty
		|| HandlerKind == EBlueprintHelperReviewTargetHandlerKind::UMGWidgetProperty
		|| Surface == EBlueprintHelperReviewSurface::DataAsset)
	{
		return TEXT("\u53d8\u91cf");
	}
	return FString();
}

FString FBlueprintHelperReviewReadableTextUtils::GetReadableChangeVerb(EBlueprintHelperReviewChangeKind ChangeKind)
{
	struct FBlueprintHelperReviewChangeKindVerb
	{
		EBlueprintHelperReviewChangeKind Kind;
		const TCHAR* Verb;
	};

	static const FBlueprintHelperReviewChangeKindVerb VerbByKind[] =
	{
		{ EBlueprintHelperReviewChangeKind::Added, TEXT("\u65b0\u589e\u4e86") },
		{ EBlueprintHelperReviewChangeKind::Removed, TEXT("\u5220\u9664\u4e86") },
		{ EBlueprintHelperReviewChangeKind::Renamed, TEXT("\u91cd\u547d\u540d\u4e86") }
	};

	for (const FBlueprintHelperReviewChangeKindVerb& Entry : VerbByKind)
	{
		if (Entry.Kind == ChangeKind)
		{
			return Entry.Verb;
		}
	}
	return TEXT("\u4fee\u6539\u4e86");
}

FString FBlueprintHelperReviewReadableTextUtils::GetReviewListTargetText(
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item,
	EBlueprintHelperReviewSurface Surface)
{
	if (!Item.IsValid())
	{
		return FString();
	}

	for (const FBlueprintHelperReviewAtomicTarget& Target : Item->AtomicTargets)
	{
		if (Target.Surface != Surface)
		{
			continue;
		}
		if (!Target.PropertyPath.IsEmpty())
		{
			return Target.PropertyPath;
		}
		if (!Target.ComponentPath.IsEmpty())
		{
			return Target.ComponentPath;
		}
		if (!Target.TargetKey.IsEmpty())
		{
			return Target.TargetKey;
		}
		if (!Target.DisplayLabel.IsEmpty())
		{
			return Target.DisplayLabel;
		}
	}

	return Item->LocationKey.IsEmpty() ? Item->DisplayLabel : Item->LocationKey;
}

FString FBlueprintHelperReviewReadableTextUtils::BuildReadableChangeTitle(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	const FString TargetName = GetReadableTargetName(Change);
	const FString Verb = GetReadableChangeVerb(Change.ChangeKind);
	const FString Suffix = GetReadableTargetSuffix(Change);
	if (TargetName.IsEmpty())
	{
		return Change.DisplayLabel.IsEmpty() ? Change.ChangeId : Change.DisplayLabel;
	}
	return FString::Printf(TEXT("%s[%s]%s"), *Verb, *TargetName, *Suffix);
}
