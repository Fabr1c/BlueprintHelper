// BlueprintHelper GraphStatement BlueprintHelperGraphFragmentEvidenceUtils implementation.

#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentEvidenceUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

FString FBlueprintHelperGraphFragmentEvidenceUtils::NormalizeEvidenceId(const FString& Value)
{
	return Value.TrimStartAndEnd();
}
TArray<TSharedPtr<FJsonValue>> FBlueprintHelperGraphFragmentEvidenceUtils::StringArrayToJson(const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> JsonValues;
	for (const FString& Value : Values)
	{
		if (!Value.IsEmpty())
		{
			JsonValues.Add(MakeShared<FJsonValueString>(Value));
		}
	}
	return JsonValues;
}
TArray<TSharedPtr<FJsonValue>> FBlueprintHelperGraphFragmentEvidenceUtils::StringMapToJsonArray(const TMap<FString, FString>& Values)
{
	TArray<FString> Keys;
	Values.GetKeys(Keys);
	Keys.Sort();

	TArray<TSharedPtr<FJsonValue>> Entries;
	for (const FString& Key : Keys)
	{
		const FString* Value = Values.Find(Key);
		if (!Value || Key.IsEmpty())
		{
			continue;
		}

		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("key"), Key);
		Entry->SetStringField(TEXT("value"), *Value);
		Entries.Add(MakeShared<FJsonValueObject>(Entry));
	}
	return Entries;
}
FString FBlueprintHelperGraphFragmentEvidenceUtils::FragmentDiagnosticSeverityToString(const EBlueprintHelperGraphFragmentDiagnosticSeverity Severity)
{
	switch (Severity)
	{
	case EBlueprintHelperGraphFragmentDiagnosticSeverity::Info:
		return TEXT("info");
	case EBlueprintHelperGraphFragmentDiagnosticSeverity::Warning:
		return TEXT("warning");
	default:
		return TEXT("error");
	}
}
bool FBlueprintHelperGraphFragmentEvidenceUtils::IsEmptyOrWhitespace(const FString& Value)
{
	return NormalizeEvidenceId(Value).IsEmpty();
}
void FBlueprintHelperGraphFragmentEvidenceUtils::AddUniqueTrimmed(TArray<FString>& Values, TSet<FString>& SeenValues, const FString& Value)
{
	const FString NormalizedValue = NormalizeEvidenceId(Value);
	if (NormalizedValue.IsEmpty() || SeenValues.Contains(NormalizedValue))
	{
		return;
	}

	SeenValues.Add(NormalizedValue);
	Values.Add(NormalizedValue);
}
void FBlueprintHelperGraphFragmentEvidenceUtils::AppendMetadata(TMap<FString, FString>& Target, const TMap<FString, FString>& Source)
{
	for (const TPair<FString, FString>& Pair : Source)
	{
		Target.Add(Pair.Key, Pair.Value);
	}
}
bool FBlueprintHelperGraphFragmentEvidenceUtils::TryReadMetadata(
	const TMap<FString, FString>& Metadata,
	const FString& Key,
	FString& OutValue)
{
	if (const FString* Value = Metadata.Find(Key))
	{
		OutValue = Value->TrimStartAndEnd();
		return !OutValue.IsEmpty();
	}

	return false;
}
FString FBlueprintHelperGraphFragmentEvidenceUtils::ReadFirstMetadata(
	const TMap<FString, FString>& Metadata,
	const TArray<FString>& Keys)
{
	for (const FString& Key : Keys)
	{
		FString Value;
		if (TryReadMetadata(Metadata, Key, Value))
		{
			return Value;
		}
	}

	return FString();
}
bool FBlueprintHelperGraphFragmentEvidenceUtils::IsDebugMetadataKey(const FString& Key)
{
	return Key.StartsWith(TEXT("debug."), ESearchCase::IgnoreCase)
		|| Key.StartsWith(TEXT("debug_"), ESearchCase::IgnoreCase)
		|| Key.StartsWith(TEXT("trace."), ESearchCase::IgnoreCase)
		|| Key.StartsWith(TEXT("trace_"), ESearchCase::IgnoreCase);
}
void FBlueprintHelperGraphFragmentEvidenceUtils::AppendDebugMetadataFromDag(
	TMap<FString, FString>& OutDebugMetadata,
	const TMap<FString, FString>& DagMetadata)
{
	for (const TPair<FString, FString>& Pair : DagMetadata)
	{
		if (IsDebugMetadataKey(Pair.Key))
		{
			OutDebugMetadata.Add(Pair.Key, Pair.Value);
		}
	}
}
FString FBlueprintHelperGraphFragmentEvidenceUtils::ChooseReviewScopeName(
	EBlueprintHelperGraphFragmentEvidenceReviewScopeKind ScopeKind,
	const TMap<FString, FString>& Metadata)
{
	const FString ExplicitName = ReadFirstMetadata(Metadata, {
		TEXT("review_scope_name"),
		TEXT("scope_name")
	});
	if (!ExplicitName.IsEmpty())
	{
		return ExplicitName;
	}

	switch (ScopeKind)
	{
	case EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Function:
		return ReadFirstMetadata(Metadata, { TEXT("function_name"), TEXT("function") });
	case EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Event:
		return ReadFirstMetadata(Metadata, { TEXT("event_name"), TEXT("event"), TEXT("custom_event_name") });
	case EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Macro:
		return ReadFirstMetadata(Metadata, { TEXT("macro_name"), TEXT("macro") });
	default:
		break;
	}

	return ReadFirstMetadata(Metadata, { TEXT("graph_name"), TEXT("graph") });
}
EBlueprintHelperGraphFragmentEvidenceReviewScopeKind FBlueprintHelperGraphFragmentEvidenceUtils::ChooseReviewScopeKind(
	const TMap<FString, FString>& Metadata)
{
	const FString ScopeKindValue = ReadFirstMetadata(Metadata, {
		TEXT("review_scope_kind"),
		TEXT("scope_kind"),
		TEXT("graph_kind"),
		TEXT("graph_type")
	});

	EBlueprintHelperGraphFragmentEvidenceReviewScopeKind ScopeKind =
		FBlueprintHelperGraphFragmentEvidenceBuilder::ParseReviewScopeKind(ScopeKindValue);
	if (ScopeKind != EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Unknown)
	{
		return ScopeKind;
	}

	if (!ReadFirstMetadata(Metadata, { TEXT("function_name"), TEXT("function") }).IsEmpty())
	{
		return EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Function;
	}

	if (!ReadFirstMetadata(Metadata, { TEXT("event_name"), TEXT("event"), TEXT("custom_event_name") }).IsEmpty())
	{
		return EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Event;
	}

	if (!ReadFirstMetadata(Metadata, { TEXT("macro_name"), TEXT("macro") }).IsEmpty())
	{
		return EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Macro;
	}

	return EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Unknown;
}
FBlueprintHelperGraphFragmentEvidenceReviewScope FBlueprintHelperGraphFragmentEvidenceUtils::MakeReviewScopeFromDagMetadata(
	const FBlueprintHelperGraphFragmentDag& Dag)
{
	FBlueprintHelperGraphFragmentEvidenceReviewScope Scope;
	Scope.Metadata = Dag.Metadata;
	AppendDebugMetadataFromDag(Scope.DebugMetadata, Dag.Metadata);

	Scope.ScopeKind = ChooseReviewScopeKind(Dag.Metadata);
	Scope.ScopeName = ChooseReviewScopeName(Scope.ScopeKind, Dag.Metadata);
	Scope.GraphName = ReadFirstMetadata(Dag.Metadata, { TEXT("graph_name"), TEXT("graph") });
	Scope.FunctionName = ReadFirstMetadata(Dag.Metadata, { TEXT("function_name"), TEXT("function") });
	Scope.EventName = ReadFirstMetadata(Dag.Metadata, { TEXT("event_name"), TEXT("event"), TEXT("custom_event_name") });
	Scope.MacroName = ReadFirstMetadata(Dag.Metadata, { TEXT("macro_name"), TEXT("macro") });

	if (!TryReadMetadata(Dag.Metadata, TEXT("review_scope_id"), Scope.ScopeId)
		&& !TryReadMetadata(Dag.Metadata, TEXT("scope_id"), Scope.ScopeId))
	{
		Scope.ScopeId = FBlueprintHelperGraphFragmentEvidenceBuilder::MakeReviewScopeId(
			Scope.ScopeKind,
			Scope.ScopeName,
			Scope.GraphName);
	}

	return Scope;
}
FBlueprintHelperGraphFragmentEvidenceReviewScope FBlueprintHelperGraphFragmentEvidenceUtils::MakeReviewScope(
	const FBlueprintHelperGraphFragmentDag& Dag,
	const FBlueprintHelperGraphFragmentEvidenceBuildOptions& Options)
{
	FBlueprintHelperGraphFragmentEvidenceReviewScope Scope =
		Options.bUseReviewScopeHint ? Options.ReviewScopeHint : MakeReviewScopeFromDagMetadata(Dag);

	if (Scope.ScopeKind == EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Unknown)
	{
		Scope.ScopeKind = ChooseReviewScopeKind(Dag.Metadata);
	}

	if (Scope.ScopeName.IsEmpty())
	{
		Scope.ScopeName = ChooseReviewScopeName(Scope.ScopeKind, Dag.Metadata);
	}

	if (Scope.GraphName.IsEmpty())
	{
		Scope.GraphName = ReadFirstMetadata(Dag.Metadata, { TEXT("graph_name"), TEXT("graph") });
	}

	if (Scope.FunctionName.IsEmpty())
	{
		Scope.FunctionName = ReadFirstMetadata(Dag.Metadata, { TEXT("function_name"), TEXT("function") });
	}

	if (Scope.EventName.IsEmpty())
	{
		Scope.EventName = ReadFirstMetadata(Dag.Metadata, { TEXT("event_name"), TEXT("event"), TEXT("custom_event_name") });
	}

	if (Scope.MacroName.IsEmpty())
	{
		Scope.MacroName = ReadFirstMetadata(Dag.Metadata, { TEXT("macro_name"), TEXT("macro") });
	}

	if (Scope.ScopeId.IsEmpty())
	{
		Scope.ScopeId = FBlueprintHelperGraphFragmentEvidenceBuilder::MakeReviewScopeId(
			Scope.ScopeKind,
			Scope.ScopeName,
			Scope.GraphName);
	}

	AppendMetadata(Scope.Metadata, Dag.Metadata);
	AppendDebugMetadataFromDag(Scope.DebugMetadata, Dag.Metadata);
	AppendDebugMetadataFromDag(Scope.DebugMetadata, Options.Metadata);
	AppendMetadata(Scope.DebugMetadata, Options.DebugMetadata);

	return Scope;
}
const FBlueprintHelperGraphFragmentRef* FBlueprintHelperGraphFragmentEvidenceUtils::FindFragmentByPath(
	const FBlueprintHelperGraphFragmentDag& Dag,
	const FString& Path)
{
	const FString NormalizedPath = NormalizeEvidenceId(Path);
	if (NormalizedPath.IsEmpty())
	{
		return nullptr;
	}

	const FBlueprintHelperGraphFragmentRef* BestMatch = nullptr;
	int32 BestPathLength = 0;

	for (const FBlueprintHelperGraphFragmentRef& Fragment : Dag.Fragments)
	{
		if (NormalizedPath.Equals(NormalizeEvidenceId(Fragment.FragmentId), ESearchCase::CaseSensitive)
			|| NormalizedPath.Equals(NormalizeEvidenceId(Fragment.SourceStatementId), ESearchCase::CaseSensitive))
		{
			return &Fragment;
		}

		const FString FragmentPath = NormalizeEvidenceId(Fragment.Path);
		if (FragmentPath.IsEmpty())
		{
			continue;
		}

		const bool bExactMatch = NormalizedPath.Equals(FragmentPath, ESearchCase::CaseSensitive);
		const bool bChildPath = NormalizedPath.StartsWith(FragmentPath + TEXT("."), ESearchCase::CaseSensitive)
			|| NormalizedPath.StartsWith(FragmentPath + TEXT("["), ESearchCase::CaseSensitive);
		if ((bExactMatch || bChildPath) && FragmentPath.Len() > BestPathLength)
		{
			BestMatch = &Fragment;
			BestPathLength = FragmentPath.Len();
		}
	}

	return BestMatch;
}
FString FBlueprintHelperGraphFragmentEvidenceUtils::MakeBundleId(
	const FBlueprintHelperGraphFragmentDag& Dag,
	const FBlueprintHelperGraphFragmentEvidenceReviewScope& Scope)
{
	FString BundleId = ReadFirstMetadata(Dag.Metadata, {
		TEXT("evidence_bundle_id"),
		TEXT("bundle_id"),
		TEXT("trace_id")
	});
	if (!BundleId.IsEmpty())
	{
		return BundleId;
	}

	if (!Scope.ScopeId.IsEmpty())
	{
		return FString::Printf(TEXT("fragment_evidence:%s"), *Scope.ScopeId);
	}

	if (Dag.Fragments.Num() > 0 && !IsEmptyOrWhitespace(Dag.Fragments[0].FragmentId))
	{
		return FString::Printf(TEXT("fragment_evidence:fragment:%s"), *NormalizeEvidenceId(Dag.Fragments[0].FragmentId));
	}

	return TEXT("fragment_evidence:empty");
}
void FBlueprintHelperGraphFragmentEvidenceUtils::FillScopeCoverage(
	FBlueprintHelperGraphFragmentEvidenceReviewScope& Scope,
	const FBlueprintHelperGraphFragmentEvidenceBundle& Bundle)
{
	TSet<FString> SourceStatementIdSet;
	TSet<FString> FragmentIdSet;

	for (const FString& SourceStatementId : Scope.SourceStatementIds)
	{
		SourceStatementIdSet.Add(NormalizeEvidenceId(SourceStatementId));
	}

	for (const FString& FragmentId : Scope.FragmentIds)
	{
		FragmentIdSet.Add(NormalizeEvidenceId(FragmentId));
	}

	for (const FString& SourceStatementId : Bundle.SourceStatementIds)
	{
		AddUniqueTrimmed(Scope.SourceStatementIds, SourceStatementIdSet, SourceStatementId);
	}

	for (const FString& FragmentId : Bundle.FragmentIds)
	{
		AddUniqueTrimmed(Scope.FragmentIds, FragmentIdSet, FragmentId);
	}

	Scope.Diagnostics = Bundle.Diagnostics;
}
