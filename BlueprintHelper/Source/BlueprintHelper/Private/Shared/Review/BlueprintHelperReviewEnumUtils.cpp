// BlueprintHelper Review enum parsing utilities implementation.

#include "Shared/Review/BlueprintHelperReviewEnumUtils.h"

struct FBlueprintHelperReviewStatusParseRule
{
	const TCHAR* Token;
	EBlueprintHelperReviewChangeStatus Status;
};

struct FBlueprintHelperReviewKindParseRule
{
	const TCHAR* Token;
	EBlueprintHelperReviewChangeKind Kind;
};

struct FBlueprintHelperReviewStorageStatusParseRule
{
	const TCHAR* Token;
	EBlueprintHelperReviewStorageStatus Status;
};

struct FBlueprintHelperReviewSurfaceParseRule
{
	const TCHAR* Token;
	EBlueprintHelperReviewSurface Surface;
};

static FString BlueprintHelperReviewNormalizeEnumToken(FString Value)
{
	Value.TrimStartAndEndInline();
	Value.ToLowerInline();
	return Value;
}

EBlueprintHelperReviewChangeStatus FBlueprintHelperReviewEnumUtils::ParseChangeStatus(const FString& Status)
{
	static const FBlueprintHelperReviewStatusParseRule Rules[] =
	{
		{ TEXT("accepted"), EBlueprintHelperReviewChangeStatus::Accepted },
		{ TEXT("rejected"), EBlueprintHelperReviewChangeStatus::Rejected },
		{ TEXT("needs_action"), EBlueprintHelperReviewChangeStatus::NeedsAction },
		{ TEXT("superseded"), EBlueprintHelperReviewChangeStatus::Superseded },
		{ TEXT("reject_failed"), EBlueprintHelperReviewChangeStatus::RejectFailed },
	};

	const FString Token = BlueprintHelperReviewNormalizeEnumToken(Status);
	for (const FBlueprintHelperReviewStatusParseRule& Rule : Rules)
	{
		if (Token.Equals(Rule.Token, ESearchCase::IgnoreCase))
		{
			return Rule.Status;
		}
	}
	return EBlueprintHelperReviewChangeStatus::Pending;
}

EBlueprintHelperReviewChangeKind FBlueprintHelperReviewEnumUtils::ParseChangeKind(const FString& ChangeKind)
{
	static const FBlueprintHelperReviewKindParseRule Rules[] =
	{
		{ TEXT("added"), EBlueprintHelperReviewChangeKind::Added },
		{ TEXT("removed"), EBlueprintHelperReviewChangeKind::Removed },
		{ TEXT("renamed"), EBlueprintHelperReviewChangeKind::Renamed },
		{ TEXT("variable_modified"), EBlueprintHelperReviewChangeKind::VariableModified },
		{ TEXT("signature_modified"), EBlueprintHelperReviewChangeKind::SignatureModified },
	};

	const FString Token = BlueprintHelperReviewNormalizeEnumToken(ChangeKind);
	for (const FBlueprintHelperReviewKindParseRule& Rule : Rules)
	{
		if (Token.Equals(Rule.Token, ESearchCase::IgnoreCase))
		{
			return Rule.Kind;
		}
	}
	return EBlueprintHelperReviewChangeKind::Modified;
}

EBlueprintHelperReviewStorageStatus FBlueprintHelperReviewEnumUtils::ParseStorageStatus(const FString& Status)
{
	static const FBlueprintHelperReviewStorageStatusParseRule Rules[] =
	{
		{ TEXT("compacted"), EBlueprintHelperReviewStorageStatus::Compacted },
	};

	const FString Token = BlueprintHelperReviewNormalizeEnumToken(Status);
	for (const FBlueprintHelperReviewStorageStatusParseRule& Rule : Rules)
	{
		if (Token.Equals(Rule.Token, ESearchCase::IgnoreCase))
		{
			return Rule.Status;
		}
	}
	return EBlueprintHelperReviewStorageStatus::Active;
}

EBlueprintHelperReviewSurface FBlueprintHelperReviewEnumUtils::ParseSurface(const FString& Surface)
{
	static const FBlueprintHelperReviewSurfaceParseRule Rules[] =
	{
		{ TEXT("components"), EBlueprintHelperReviewSurface::Components },
		{ TEXT("my_blueprint"), EBlueprintHelperReviewSurface::MyBlueprint },
		{ TEXT("details"), EBlueprintHelperReviewSurface::Details },
		{ TEXT("umg_widget_tree"), EBlueprintHelperReviewSurface::UMGWidgetTree },
		{ TEXT("umg"), EBlueprintHelperReviewSurface::UMGWidgetTree },
		{ TEXT("umg_widget"), EBlueprintHelperReviewSurface::UMGWidgetTree },
		{ TEXT("data_table"), EBlueprintHelperReviewSurface::DataTable },
		{ TEXT("datatable"), EBlueprintHelperReviewSurface::DataTable },
		{ TEXT("data_asset"), EBlueprintHelperReviewSurface::DataAsset },
		{ TEXT("object_details"), EBlueprintHelperReviewSurface::DataAsset },
		{ TEXT("graph"), EBlueprintHelperReviewSurface::Graph },
	};

	const FString Token = BlueprintHelperReviewNormalizeEnumToken(Surface);
	for (const FBlueprintHelperReviewSurfaceParseRule& Rule : Rules)
	{
		if (Token.Equals(Rule.Token, ESearchCase::IgnoreCase))
		{
			return Rule.Surface;
		}
	}
	return EBlueprintHelperReviewSurface::Unknown;
}
