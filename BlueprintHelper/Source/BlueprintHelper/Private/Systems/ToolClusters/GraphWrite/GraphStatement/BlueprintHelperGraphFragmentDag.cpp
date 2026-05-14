#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
static FString NormalizeFragmentId(const FString& FragmentId)
{
	return FragmentId.TrimStartAndEnd();
}

static const TCHAR* SeverityToString(const EBlueprintHelperGraphFragmentDiagnosticSeverity Severity)
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

static const TCHAR* DirectionToString(const EBlueprintHelperGraphFragmentPortDirection Direction)
{
	switch (Direction)
	{
	case EBlueprintHelperGraphFragmentPortDirection::ExecInput:
		return TEXT("exec_input");
	case EBlueprintHelperGraphFragmentPortDirection::ExecOutput:
		return TEXT("exec_output");
	case EBlueprintHelperGraphFragmentPortDirection::DataInput:
		return TEXT("data_input");
	case EBlueprintHelperGraphFragmentPortDirection::DataOutput:
		return TEXT("data_output");
	default:
		return TEXT("unknown");
	}
}

static const TCHAR* LayoutKindToString(const EBlueprintHelperGraphFragmentLayoutKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperGraphFragmentLayoutKind::Statement:
		return TEXT("statement");
	case EBlueprintHelperGraphFragmentLayoutKind::Expression:
		return TEXT("expression");
	case EBlueprintHelperGraphFragmentLayoutKind::Join:
		return TEXT("join");
	default:
		return TEXT("unknown");
	}
}

static TSharedRef<FJsonObject> Vector2DToJson(const FVector2D& Value)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("x"), Value.X);
	Json->SetNumberField(TEXT("y"), Value.Y);
	return Json;
}

static TArray<TSharedPtr<FJsonValue>> StringMapToJsonArray(const TMap<FString, FString>& Values)
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

static TArray<TSharedPtr<FJsonValue>> EndpointsToJsonArray(const TArray<FBlueprintHelperGraphFragmentEndpointRef>& Endpoints)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FBlueprintHelperGraphFragmentEndpointRef& Endpoint : Endpoints)
	{
		Values.Add(MakeShared<FJsonValueObject>(Endpoint.ToJson()));
	}
	return Values;
}
}

TSharedRef<FJsonObject> FBlueprintHelperGraphFragmentDiagnostic::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!Code.IsEmpty()) Json->SetStringField(TEXT("code"), Code);
	if (!Path.IsEmpty()) Json->SetStringField(TEXT("path"), Path);
	if (!Message.IsEmpty()) Json->SetStringField(TEXT("message"), Message);
	Json->SetStringField(TEXT("severity"), SeverityToString(Severity));
	return Json;
}

bool FBlueprintHelperGraphFragmentRef::IsValid() const
{
	return !NormalizeFragmentId(FragmentId).IsEmpty();
}

bool FBlueprintHelperGraphFragmentPinTypeRef::IsValid() const
{
	return !Category.TrimStartAndEnd().IsEmpty()
		|| !SubCategory.TrimStartAndEnd().IsEmpty()
		|| !ObjectPath.TrimStartAndEnd().IsEmpty()
		|| !ContainerType.TrimStartAndEnd().IsEmpty()
		|| !ValueType.TrimStartAndEnd().IsEmpty();
}

TSharedRef<FJsonObject> FBlueprintHelperGraphFragmentPinTypeRef::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!Category.IsEmpty()) Json->SetStringField(TEXT("category"), Category);
	if (!SubCategory.IsEmpty()) Json->SetStringField(TEXT("subcategory"), SubCategory);
	if (!ObjectPath.IsEmpty()) Json->SetStringField(TEXT("object_path"), ObjectPath);
	if (!ContainerType.IsEmpty()) Json->SetStringField(TEXT("container_type"), ContainerType);
	if (!ValueType.IsEmpty()) Json->SetStringField(TEXT("value_type"), ValueType);
	Json->SetBoolField(TEXT("is_reference"), bIsReference);
	Json->SetBoolField(TEXT("is_const"), bIsConst);
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperGraphFragmentLayoutRef::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("kind"), LayoutKindToString(Kind));
	if (Row != INDEX_NONE) Json->SetNumberField(TEXT("row"), Row);
	if (Column != INDEX_NONE) Json->SetNumberField(TEXT("column"), Column);
	if (bHasPosition) Json->SetObjectField(TEXT("position"), Vector2DToJson(Position));
	if (bHasSize) Json->SetObjectField(TEXT("size"), Vector2DToJson(Size));
	if (Hints.Num() > 0) Json->SetArrayField(TEXT("hints"), StringMapToJsonArray(Hints));
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperGraphFragmentRef::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!FragmentId.IsEmpty()) Json->SetStringField(TEXT("fragment_id"), FragmentId);
	if (!SourceStatementId.IsEmpty()) Json->SetStringField(TEXT("source_statement_id"), SourceStatementId);
	if (!Path.IsEmpty()) Json->SetStringField(TEXT("path"), Path);
	if (!Kind.IsEmpty()) Json->SetStringField(TEXT("kind"), Kind);
	Json->SetObjectField(TEXT("layout"), Layout.ToJson());
	if (Metadata.Num() > 0) Json->SetArrayField(TEXT("metadata"), StringMapToJsonArray(Metadata));
	return Json;
}

bool FBlueprintHelperGraphFragmentEndpointRef::IsValid() const
{
	return !NormalizeFragmentId(FragmentId).IsEmpty() && !PortId.TrimStartAndEnd().IsEmpty();
}

TSharedRef<FJsonObject> FBlueprintHelperGraphFragmentEndpointRef::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!FragmentId.IsEmpty()) Json->SetStringField(TEXT("fragment_id"), FragmentId);
	if (!PortId.IsEmpty()) Json->SetStringField(TEXT("port_id"), PortId);
	if (!PinName.IsEmpty()) Json->SetStringField(TEXT("pin_name"), PinName);
	if (!Type.IsEmpty()) Json->SetStringField(TEXT("type"), Type);
	Json->SetStringField(TEXT("direction"), DirectionToString(Direction));
	if (PinType.IsValid()) Json->SetObjectField(TEXT("pin_type"), PinType.ToJson());
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperGraphFragmentExecEdge::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!EdgeId.IsEmpty()) Json->SetStringField(TEXT("edge_id"), EdgeId);
	Json->SetObjectField(TEXT("from"), From.ToJson());
	Json->SetObjectField(TEXT("to"), To.ToJson());
	if (!Path.IsEmpty()) Json->SetStringField(TEXT("path"), Path);
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperGraphFragmentDataEdge::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!EdgeId.IsEmpty()) Json->SetStringField(TEXT("edge_id"), EdgeId);
	Json->SetObjectField(TEXT("from"), From.ToJson());
	Json->SetObjectField(TEXT("to"), To.ToJson());
	if (!SymbolId.IsEmpty()) Json->SetStringField(TEXT("symbol_id"), SymbolId);
	if (!Path.IsEmpty()) Json->SetStringField(TEXT("path"), Path);
	return Json;
}

bool FBlueprintHelperGraphFragmentEntryExitRefs::HasEntry() const
{
	return Entries.ContainsByPredicate([](const FBlueprintHelperGraphFragmentEndpointRef& Entry)
	{
		return Entry.IsValid();
	});
}

bool FBlueprintHelperGraphFragmentEntryExitRefs::HasExit() const
{
	return Exits.ContainsByPredicate([](const FBlueprintHelperGraphFragmentEndpointRef& Exit)
	{
		return Exit.IsValid();
	});
}

TSharedRef<FJsonObject> FBlueprintHelperGraphFragmentEntryExitRefs::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetArrayField(TEXT("entries"), EndpointsToJsonArray(Entries));
	Json->SetArrayField(TEXT("exits"), EndpointsToJsonArray(Exits));
	return Json;
}

void FBlueprintHelperGraphFragmentDag::Reset()
{
	*this = FBlueprintHelperGraphFragmentDag();
}

bool FBlueprintHelperGraphFragmentDag::IsEmpty() const
{
	return Fragments.Num() == 0 && ExecEdges.Num() == 0 && DataEdges.Num() == 0;
}

bool FBlueprintHelperGraphFragmentDag::HasErrors() const
{
	return Diagnostics.ContainsByPredicate([](const FBlueprintHelperGraphFragmentDiagnostic& Diagnostic)
	{
		return Diagnostic.Severity == EBlueprintHelperGraphFragmentDiagnosticSeverity::Error;
	});
}

bool FBlueprintHelperGraphFragmentDag::TryFindFragment(
	const FString& FragmentId,
	FBlueprintHelperGraphFragmentRef& OutFragment) const
{
	const FString NormalizedFragmentId = NormalizeFragmentId(FragmentId);
	for (const FBlueprintHelperGraphFragmentRef& Fragment : Fragments)
	{
		if (NormalizeFragmentId(Fragment.FragmentId).Equals(NormalizedFragmentId, ESearchCase::CaseSensitive))
		{
			OutFragment = Fragment;
			return true;
		}
	}

	return false;
}

TSharedRef<FJsonObject> FBlueprintHelperGraphFragmentDag::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), Schema.IsEmpty() ? TEXT("BlueprintHelperGraphFragmentDag.v1") : Schema);

	TArray<TSharedPtr<FJsonValue>> FragmentValues;
	for (const FBlueprintHelperGraphFragmentRef& Fragment : Fragments)
	{
		FragmentValues.Add(MakeShared<FJsonValueObject>(Fragment.ToJson()));
	}
	Json->SetArrayField(TEXT("fragments"), FragmentValues);

	TArray<TSharedPtr<FJsonValue>> ExecEdgeValues;
	for (const FBlueprintHelperGraphFragmentExecEdge& Edge : ExecEdges)
	{
		ExecEdgeValues.Add(MakeShared<FJsonValueObject>(Edge.ToJson()));
	}
	Json->SetArrayField(TEXT("exec_edges"), ExecEdgeValues);

	TArray<TSharedPtr<FJsonValue>> DataEdgeValues;
	for (const FBlueprintHelperGraphFragmentDataEdge& Edge : DataEdges)
	{
		DataEdgeValues.Add(MakeShared<FJsonValueObject>(Edge.ToJson()));
	}
	Json->SetArrayField(TEXT("data_edges"), DataEdgeValues);

	Json->SetObjectField(TEXT("entry_exit_refs"), EntryExitRefs.ToJson());

	TArray<TSharedPtr<FJsonValue>> DiagnosticValues;
	for (const FBlueprintHelperGraphFragmentDiagnostic& Diagnostic : Diagnostics)
	{
		DiagnosticValues.Add(MakeShared<FJsonValueObject>(Diagnostic.ToJson()));
	}
	Json->SetArrayField(TEXT("diagnostics"), DiagnosticValues);

	if (Metadata.Num() > 0)
	{
		Json->SetArrayField(TEXT("metadata"), StringMapToJsonArray(Metadata));
	}

	return Json;
}

void FBlueprintHelperGraphFragmentDag::AddDiagnostic(
	const FString& Code,
	const FString& Path,
	const FString& Message,
	EBlueprintHelperGraphFragmentDiagnosticSeverity Severity)
{
	FBlueprintHelperGraphFragmentDiagnostic Diagnostic;
	Diagnostic.Code = Code;
	Diagnostic.Path = Path;
	Diagnostic.Message = Message;
	Diagnostic.Severity = Severity;
	Diagnostics.Add(MoveTemp(Diagnostic));
}
