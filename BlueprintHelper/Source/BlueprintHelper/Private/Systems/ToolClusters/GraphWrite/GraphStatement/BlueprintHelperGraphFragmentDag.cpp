#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagUtils.h"

namespace
{
static bool TryReadStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, FString& OutValue)
{
	return Object.IsValid() && Object->TryGetStringField(FieldName, OutValue) && !OutValue.IsEmpty();
}

static bool TryReadBoolField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, bool& OutValue)
{
	return Object.IsValid() && Object->TryGetBoolField(FieldName, OutValue);
}

static EBlueprintHelperGraphFragmentPortDirection ParseFragmentPortDirection(const FString& Direction)
{
	const FString Normalized = Direction.TrimStartAndEnd().ToLower();
	if (Normalized == TEXT("exec_input"))
	{
		return EBlueprintHelperGraphFragmentPortDirection::ExecInput;
	}
	if (Normalized == TEXT("exec_output"))
	{
		return EBlueprintHelperGraphFragmentPortDirection::ExecOutput;
	}
	if (Normalized == TEXT("data_input"))
	{
		return EBlueprintHelperGraphFragmentPortDirection::DataInput;
	}
	if (Normalized == TEXT("data_output"))
	{
		return EBlueprintHelperGraphFragmentPortDirection::DataOutput;
	}
	return EBlueprintHelperGraphFragmentPortDirection::Unknown;
}
}

TSharedRef<FJsonObject> FBlueprintHelperGraphFragmentDiagnostic::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!Code.IsEmpty()) Json->SetStringField(TEXT("code"), Code);
	if (!Path.IsEmpty()) Json->SetStringField(TEXT("path"), Path);
	if (!Message.IsEmpty()) Json->SetStringField(TEXT("message"), Message);
	Json->SetStringField(TEXT("severity"), FBlueprintHelperGraphFragmentDagUtils::SeverityToString(Severity));
	return Json;
}

bool FBlueprintHelperGraphFragmentRef::IsValid() const
{
	return !FBlueprintHelperGraphFragmentDagUtils::NormalizeFragmentId(FragmentId).IsEmpty();
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

FBlueprintHelperGraphFragmentPinTypeRef FBlueprintHelperGraphFragmentPinTypeRef::FromJson(
	const TSharedPtr<FJsonObject>& Object)
{
	FBlueprintHelperGraphFragmentPinTypeRef Result;
	if (!Object.IsValid())
	{
		return Result;
	}

	TryReadStringField(Object, TEXT("category"), Result.Category);
	if (!TryReadStringField(Object, TEXT("subcategory"), Result.SubCategory))
	{
		TryReadStringField(Object, TEXT("sub_category"), Result.SubCategory);
	}
	if (!TryReadStringField(Object, TEXT("object_path"), Result.ObjectPath))
	{
		TryReadStringField(Object, TEXT("sub_category_object_path"), Result.ObjectPath);
	}
	if (!TryReadStringField(Object, TEXT("container_type"), Result.ContainerType))
	{
		TryReadStringField(Object, TEXT("container"), Result.ContainerType);
	}
	TryReadStringField(Object, TEXT("value_type"), Result.ValueType);
	TryReadBoolField(Object, TEXT("is_reference"), Result.bIsReference);
	TryReadBoolField(Object, TEXT("is_const"), Result.bIsConst);
	return Result;
}

TSharedRef<FJsonObject> FBlueprintHelperGraphFragmentLayoutRef::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("kind"), FBlueprintHelperGraphFragmentDagUtils::LayoutKindToString(Kind));
	if (Row != INDEX_NONE) Json->SetNumberField(TEXT("row"), Row);
	if (Column != INDEX_NONE) Json->SetNumberField(TEXT("column"), Column);
	if (bHasPosition) Json->SetObjectField(TEXT("position"), FBlueprintHelperGraphFragmentDagUtils::Vector2DToJson(Position));
	if (bHasSize) Json->SetObjectField(TEXT("size"), FBlueprintHelperGraphFragmentDagUtils::Vector2DToJson(Size));
	if (Hints.Num() > 0) Json->SetArrayField(TEXT("hints"), FBlueprintHelperGraphFragmentDagUtils::StringMapToJsonArray(Hints));
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
	if (Metadata.Num() > 0) Json->SetArrayField(TEXT("metadata"), FBlueprintHelperGraphFragmentDagUtils::StringMapToJsonArray(Metadata));
	return Json;
}

bool FBlueprintHelperGraphFragmentEndpointRef::IsValid() const
{
	return !FBlueprintHelperGraphFragmentDagUtils::NormalizeFragmentId(FragmentId).IsEmpty() && !PortId.TrimStartAndEnd().IsEmpty();
}

TSharedRef<FJsonObject> FBlueprintHelperGraphFragmentEndpointRef::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!FragmentId.IsEmpty()) Json->SetStringField(TEXT("fragment_id"), FragmentId);
	if (!PortId.IsEmpty()) Json->SetStringField(TEXT("port_id"), PortId);
	if (!PinName.IsEmpty()) Json->SetStringField(TEXT("pin_name"), PinName);
	if (!Type.IsEmpty()) Json->SetStringField(TEXT("type"), Type);
	Json->SetStringField(TEXT("direction"), FBlueprintHelperGraphFragmentDagUtils::DirectionToString(Direction));
	if (PinType.IsValid()) Json->SetObjectField(TEXT("pin_type"), PinType.ToJson());
	return Json;
}

FBlueprintHelperGraphFragmentEndpointRef FBlueprintHelperGraphFragmentEndpointRef::FromJson(
	const TSharedPtr<FJsonObject>& Object)
{
	FBlueprintHelperGraphFragmentEndpointRef Result;
	if (!Object.IsValid())
	{
		return Result;
	}

	if (!TryReadStringField(Object, TEXT("fragment_id"), Result.FragmentId))
	{
		TryReadStringField(Object, TEXT("fragment"), Result.FragmentId);
	}
	if (!TryReadStringField(Object, TEXT("port_id"), Result.PortId))
	{
		TryReadStringField(Object, TEXT("port"), Result.PortId);
	}
	TryReadStringField(Object, TEXT("pin_name"), Result.PinName);
	TryReadStringField(Object, TEXT("type"), Result.Type);

	FString DirectionString;
	if (TryReadStringField(Object, TEXT("direction"), DirectionString))
	{
		Result.Direction = ParseFragmentPortDirection(DirectionString);
	}

	FString PinTypeCategory;
	if (TryReadStringField(Object, TEXT("pin_type"), PinTypeCategory))
	{
		Result.PinType.Category = PinTypeCategory;
	}
	else
	{
		const TSharedPtr<FJsonObject>* PinTypeObject = nullptr;
		if (Object->TryGetObjectField(TEXT("pin_type"), PinTypeObject) && PinTypeObject)
		{
			Result.PinType = FBlueprintHelperGraphFragmentPinTypeRef::FromJson(*PinTypeObject);
		}
	}
	return Result;
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
	Json->SetArrayField(TEXT("entries"), FBlueprintHelperGraphFragmentDagUtils::EndpointsToJsonArray(Entries));
	Json->SetArrayField(TEXT("exits"), FBlueprintHelperGraphFragmentDagUtils::EndpointsToJsonArray(Exits));
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
	const FString NormalizedFragmentId = FBlueprintHelperGraphFragmentDagUtils::NormalizeFragmentId(FragmentId);
	for (const FBlueprintHelperGraphFragmentRef& Fragment : Fragments)
	{
		if (FBlueprintHelperGraphFragmentDagUtils::NormalizeFragmentId(Fragment.FragmentId).Equals(NormalizedFragmentId, ESearchCase::CaseSensitive))
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
		Json->SetArrayField(TEXT("metadata"), FBlueprintHelperGraphFragmentDagUtils::StringMapToJsonArray(Metadata));
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
