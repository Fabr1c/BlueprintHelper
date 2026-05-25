#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphEventReferenceUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.h"

namespace
{
static FString ReadStringField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName)
{
	FString Value;
	if (Object.IsValid() && FieldName)
	{
		Object->TryGetStringField(FieldName, Value);
	}
	return Value.TrimStartAndEnd();
}

static void ReadStringMapField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	TMap<FString, FString>& OutMap)
{
	const TSharedPtr<FJsonObject>* MapObject = nullptr;
	if (!Object.IsValid()
		|| !FieldName
		|| !Object->TryGetObjectField(FieldName, MapObject)
		|| !MapObject
		|| !MapObject->IsValid())
	{
		return;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*MapObject)->Values)
	{
		if (!Pair.Key.IsEmpty())
		{
			OutMap.Add(Pair.Key, FBlueprintHelperGraphSemanticIRUtils::JsonValueToString(Pair.Value).TrimStartAndEnd());
		}
	}
}

static void AddMetadataIfPresent(
	TMap<FString, FString>& OutMetadata,
	const FString& Key,
	const FString& Value)
{
	const FString CleanValue = Value.TrimStartAndEnd();
	if (!Key.IsEmpty() && !CleanValue.IsEmpty())
	{
		OutMetadata.Add(Key, CleanValue);
	}
}
}

FString FBlueprintHelperGraphEventReferenceUtils::TaxonomyToString(EBlueprintHelperGraphEventTaxonomy Taxonomy)
{
	switch (Taxonomy)
	{
	case EBlueprintHelperGraphEventTaxonomy::CustomEvent:
		return TEXT("custom_event");
	case EBlueprintHelperGraphEventTaxonomy::NativeEvent:
		return TEXT("native_event");
	case EBlueprintHelperGraphEventTaxonomy::OverrideEvent:
		return TEXT("override_event");
	default:
		break;
	}
	return FString();
}

EBlueprintHelperGraphEventTaxonomy FBlueprintHelperGraphEventReferenceUtils::ParseTaxonomy(const FString& Value)
{
	const FString Token = Value.TrimStartAndEnd().ToLower();
	if (Token == TEXT("custom_event"))
	{
		return EBlueprintHelperGraphEventTaxonomy::CustomEvent;
	}
	if (Token == TEXT("native_event"))
	{
		return EBlueprintHelperGraphEventTaxonomy::NativeEvent;
	}
	if (Token == TEXT("override_event"))
	{
		return EBlueprintHelperGraphEventTaxonomy::OverrideEvent;
	}
	return EBlueprintHelperGraphEventTaxonomy::Unknown;
}

bool FBlueprintHelperGraphEventReferenceUtils::TryReadEntryReference(
	const TSharedPtr<FJsonObject>& EntryObject,
	FBlueprintHelperGraphEventReference& OutReference)
{
	OutReference = FBlueprintHelperGraphEventReference();
	if (!EntryObject.IsValid())
	{
		return false;
	}

	OutReference.Kind = ReadStringField(EntryObject, TEXT("kind"));
	OutReference.Name = ReadStringField(EntryObject, TEXT("name"));
	OutReference.GraphName = ReadStringField(EntryObject, TEXT("graph"));
	OutReference.SourceCluster = ReadStringField(EntryObject, TEXT("source_cluster"));
	OutReference.SignatureEvidenceId = ReadStringField(EntryObject, TEXT("signature_evidence_id"));

	FString TaxonomyText = ReadStringField(EntryObject, TEXT("event_taxonomy"));
	if (TaxonomyText.IsEmpty() && OutReference.Kind.Equals(TEXT("custom_event"), ESearchCase::IgnoreCase))
	{
		TaxonomyText = TEXT("custom_event");
	}
	OutReference.Taxonomy = ParseTaxonomy(TaxonomyText);

	ReadStringMapField(EntryObject, TEXT("context_evidence"), OutReference.Metadata);

	if (OutReference.SourceCluster.IsEmpty())
	{
		OutReference.SourceCluster = OutReference.Metadata.FindRef(TEXT("source_cluster")).TrimStartAndEnd();
	}
	if (OutReference.SignatureEvidenceId.IsEmpty())
	{
		OutReference.SignatureEvidenceId = OutReference.Metadata.FindRef(TEXT("signature_evidence_id")).TrimStartAndEnd();
	}

	WriteMetadata(OutReference, OutReference.Metadata);
	return !OutReference.Kind.IsEmpty() || !OutReference.Name.IsEmpty();
}

void FBlueprintHelperGraphEventReferenceUtils::WriteMetadata(
	const FBlueprintHelperGraphEventReference& Reference,
	TMap<FString, FString>& OutMetadata)
{
	AddMetadataIfPresent(OutMetadata, TEXT("entry_kind"), Reference.Kind);
	AddMetadataIfPresent(OutMetadata, TEXT("event_name"), Reference.Name);
	AddMetadataIfPresent(OutMetadata, TEXT("graph_name"), Reference.GraphName);
	AddMetadataIfPresent(OutMetadata, TEXT("event_taxonomy"), TaxonomyToString(Reference.Taxonomy));
	AddMetadataIfPresent(OutMetadata, TEXT("source_cluster"), Reference.SourceCluster);
	AddMetadataIfPresent(OutMetadata, TEXT("signature_evidence_id"), Reference.SignatureEvidenceId);
}

bool FBlueprintHelperGraphEventReferenceUtils::IsSignatureOwnedTaxonomy(EBlueprintHelperGraphEventTaxonomy Taxonomy)
{
	return Taxonomy == EBlueprintHelperGraphEventTaxonomy::CustomEvent
		|| Taxonomy == EBlueprintHelperGraphEventTaxonomy::NativeEvent
		|| Taxonomy == EBlueprintHelperGraphEventTaxonomy::OverrideEvent;
}
