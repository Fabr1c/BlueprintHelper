#pragma once

#include "CoreMinimal.h"

class FJsonObject;

enum class EBlueprintHelperGraphEventTaxonomy : uint8
{
	Unknown,
	CustomEvent,
	NativeEvent,
	OverrideEvent
};

struct FBlueprintHelperGraphEventReference
{
	FString Kind;
	FString Name;
	FString GraphName;
	EBlueprintHelperGraphEventTaxonomy Taxonomy = EBlueprintHelperGraphEventTaxonomy::Unknown;
	FString SourceCluster;
	FString SignatureEvidenceId;
	TMap<FString, FString> Metadata;

	bool HasSignatureEvidence() const
	{
		return !SourceCluster.TrimStartAndEnd().IsEmpty()
			&& !SignatureEvidenceId.TrimStartAndEnd().IsEmpty();
	}
};

class FBlueprintHelperGraphEventReferenceUtils
{
public:
	static FString TaxonomyToString(EBlueprintHelperGraphEventTaxonomy Taxonomy);
	static EBlueprintHelperGraphEventTaxonomy ParseTaxonomy(const FString& Value);
	static bool TryReadEntryReference(const TSharedPtr<FJsonObject>& EntryObject, FBlueprintHelperGraphEventReference& OutReference);
	static void WriteMetadata(const FBlueprintHelperGraphEventReference& Reference, TMap<FString, FString>& OutMetadata);
	static bool IsSignatureOwnedTaxonomy(EBlueprintHelperGraphEventTaxonomy Taxonomy);
};
