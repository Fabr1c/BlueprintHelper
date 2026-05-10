// BlueprintHelper Review readable text utilities.

#include "UI/Review/BlueprintHelperReviewReadableTextUtils.h"

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
	if (Change.bIsAssetLifecycleRoot)
	{
		return true;
	}

	return Change.AtomicTargets.ContainsByPredicate(
		[](const FBlueprintHelperReviewAtomicTarget& Target)
		{
			return Target.TargetKind.Equals(TEXT("asset_factory"), ESearchCase::IgnoreCase)
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
		return TEXT("Widget Blueprint 璧勪骇");
	}
	if (LowerAssetName.StartsWith(TEXT("dt_")) || CombinedDescriptor.Contains(TEXT("data_table")))
	{
		return TEXT("DataTable 璧勪骇");
	}
	if (LowerAssetName.StartsWith(TEXT("st_")) || CombinedDescriptor.Contains(TEXT("structure")))
	{
		return TEXT("Structure 璧勪骇");
	}
	if (LowerAssetName.StartsWith(TEXT("da_")) || CombinedDescriptor.Contains(TEXT("data_asset")))
	{
		return TEXT("DataAsset 璧勪骇");
	}
	if (LowerAssetName.StartsWith(TEXT("bp_")) || LowerAssetName.StartsWith(TEXT("bpi_")))
	{
		return TEXT("钃濆浘璧勪骇");
	}
	return TEXT("UObject 璧勪骇");
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
	const FString TargetKind = Target ? Target->TargetKind.ToLower() : FString();
	const EBlueprintHelperReviewSurface Surface = Target ? Target->Surface : EBlueprintHelperReviewSurface::Unknown;

	if (IsAssetFactoryChange(Change))
	{
		return GetAssetFactoryReadableSuffix(Change);
	}
	if (Surface == EBlueprintHelperReviewSurface::UMGWidgetTree)
	{
		return FString();
	}
	if (TargetKind.Contains(TEXT("datatable_row")) || Surface == EBlueprintHelperReviewSurface::DataTable)
	{
		return TEXT("行");
	}
	if (TargetKind.Contains(TEXT("component")) || Surface == EBlueprintHelperReviewSurface::Components)
	{
		return TEXT("缁勪欢");
	}
	if (TargetKind.Contains(TEXT("signature")))
	{
		return TEXT("绛惧悕");
	}
	if (TargetKind.Contains(TEXT("variable"))
		|| TargetKind.Contains(TEXT("property"))
		|| Surface == EBlueprintHelperReviewSurface::DataAsset)
	{
		return TEXT("鍙橀噺");
	}
	return FString();
}

FString FBlueprintHelperReviewReadableTextUtils::GetReadableChangeVerb(EBlueprintHelperReviewChangeKind ChangeKind)
{
	switch (ChangeKind)
	{
	case EBlueprintHelperReviewChangeKind::Added:
		return TEXT("新增了");
	case EBlueprintHelperReviewChangeKind::Removed:
		return TEXT("删除了");
	case EBlueprintHelperReviewChangeKind::Renamed:
		return TEXT("閲嶅懡鍚嶄簡");
	default:
		return TEXT("修改了");
	}
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
