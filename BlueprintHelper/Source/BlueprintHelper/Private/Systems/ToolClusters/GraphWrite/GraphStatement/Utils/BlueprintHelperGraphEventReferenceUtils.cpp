#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphEventReferenceUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.h"
#include "GraphWriteGraphStatementUtils.h"

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

	OutReference.Kind = UGraphWriteGraphStatementUtils::ReadStringField(EntryObject, TEXT("kind"));
	OutReference.Name = UGraphWriteGraphStatementUtils::ReadStringField(EntryObject, TEXT("name"));
	OutReference.GraphName = UGraphWriteGraphStatementUtils::ReadStringField(EntryObject, TEXT("graph"));
	OutReference.SourceCluster = UGraphWriteGraphStatementUtils::ReadStringField(EntryObject, TEXT("source_cluster"));
	OutReference.SignatureEvidenceId = UGraphWriteGraphStatementUtils::ReadStringField(EntryObject, TEXT("signature_evidence_id"));

	FString TaxonomyText = UGraphWriteGraphStatementUtils::ReadStringField(EntryObject, TEXT("event_taxonomy"));
	if (TaxonomyText.IsEmpty() && OutReference.Kind.Equals(TEXT("custom_event"), ESearchCase::IgnoreCase))
	{
		TaxonomyText = TEXT("custom_event");
	}
	OutReference.Taxonomy = ParseTaxonomy(TaxonomyText);

	UGraphWriteGraphStatementUtils::ReadStringMapField(EntryObject, TEXT("context_evidence"), OutReference.Metadata);

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
	UGraphWriteGraphStatementUtils::AddMetadataIfPresent(OutMetadata, TEXT("entry_kind"), Reference.Kind);
	UGraphWriteGraphStatementUtils::AddMetadataIfPresent(OutMetadata, TEXT("event_name"), Reference.Name);
	UGraphWriteGraphStatementUtils::AddMetadataIfPresent(OutMetadata, TEXT("graph_name"), Reference.GraphName);
	UGraphWriteGraphStatementUtils::AddMetadataIfPresent(OutMetadata, TEXT("event_taxonomy"), TaxonomyToString(Reference.Taxonomy));
	UGraphWriteGraphStatementUtils::AddMetadataIfPresent(OutMetadata, TEXT("source_cluster"), Reference.SourceCluster);
	UGraphWriteGraphStatementUtils::AddMetadataIfPresent(OutMetadata, TEXT("signature_evidence_id"), Reference.SignatureEvidenceId);
}

bool FBlueprintHelperGraphEventReferenceUtils::IsSignatureOwnedTaxonomy(EBlueprintHelperGraphEventTaxonomy Taxonomy)
{
	return Taxonomy == EBlueprintHelperGraphEventTaxonomy::CustomEvent
		|| Taxonomy == EBlueprintHelperGraphEventTaxonomy::NativeEvent
		|| Taxonomy == EBlueprintHelperGraphEventTaxonomy::OverrideEvent;
}
