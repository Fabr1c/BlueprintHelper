// BlueprintHelper Review adapter registry implementation.

#include "Systems/Review/BlueprintHelperReviewAdapterRegistry.h"
#include "Systems/Review/BlueprintHelperReviewAssetFactoryRestoreAdapter.h"
#include "Systems/Review/BlueprintHelperReviewMaterialInstanceEvidenceAdapter.h"
#include "Systems/Review/BlueprintHelperReviewMaterialInstanceRestoreAdapter.h"
#include "Systems/Review/BlueprintHelperReviewSnapshotRestoreAdapter.h"

TSharedRef<FBlueprintHelperReviewAdapterRegistry> FBlueprintHelperReviewAdapterRegistry::CreateDefault()
{
	TSharedRef<FBlueprintHelperReviewAdapterRegistry> Registry = MakeShared<FBlueprintHelperReviewAdapterRegistry>();
	Registry->RegisterBuiltInAdapters();
	return Registry;
}

bool FBlueprintHelperReviewAdapterRegistry::RegisterEvidenceAdapter(
	const TSharedRef<IBlueprintHelperReviewEvidenceAdapter>& Adapter,
	TArray<FBlueprintHelperReviewAdapterRegistryDiagnostic>& OutDiagnostics)
{
	const FString Key = NormalizeTargetKind(Adapter->GetTargetKind());
	if (Key.IsEmpty())
	{
		OutDiagnostics.Add(MakeDiagnostic(
			TEXT("missing_evidence_target_kind"),
			TEXT("Evidence adapter target kind is empty.")));
		return false;
	}
	if (EvidenceAdaptersByTargetKind.Contains(Key))
	{
		OutDiagnostics.Add(MakeDiagnostic(
			TEXT("duplicate_evidence_adapter"),
			FString::Printf(TEXT("Evidence adapter already registered for target kind '%s'."), *Key)));
		return false;
	}

	EvidenceAdaptersByTargetKind.Add(Key, Adapter);
	return true;
}

bool FBlueprintHelperReviewAdapterRegistry::RegisterRestoreAdapter(
	const TSharedRef<IBlueprintHelperReviewRestoreAdapter>& Adapter,
	TArray<FBlueprintHelperReviewAdapterRegistryDiagnostic>& OutDiagnostics)
{
	const FString Key = NormalizeTargetKind(Adapter->GetTargetKind());
	if (Key.IsEmpty())
	{
		OutDiagnostics.Add(MakeDiagnostic(
			TEXT("missing_restore_target_kind"),
			TEXT("Restore adapter target kind is empty.")));
		return false;
	}
	if (RestoreAdaptersByTargetKind.Contains(Key))
	{
		OutDiagnostics.Add(MakeDiagnostic(
			TEXT("duplicate_restore_adapter"),
			FString::Printf(TEXT("Restore adapter already registered for target kind '%s'."), *Key)));
		return false;
	}

	RestoreAdaptersByTargetKind.Add(Key, Adapter);
	return true;
}

FBlueprintHelperReviewEvidenceAdapterLookup FBlueprintHelperReviewAdapterRegistry::FindEvidenceAdapter(
	const FString& TargetKind) const
{
	FBlueprintHelperReviewEvidenceAdapterLookup Lookup;
	Lookup.TargetKind = NormalizeTargetKind(TargetKind);
	if (const TSharedPtr<IBlueprintHelperReviewEvidenceAdapter>* Adapter = EvidenceAdaptersByTargetKind.Find(Lookup.TargetKind))
	{
		Lookup.bAvailable = Adapter->IsValid();
		Lookup.Adapter = *Adapter;
		Lookup.Message = Lookup.bAvailable ? TEXT("available") : TEXT("evidence_adapter_invalid");
		return Lookup;
	}

	Lookup.Message = FString::Printf(TEXT("evidence_adapter_unavailable:%s"), *Lookup.TargetKind);
	Lookup.Diagnostics.Add(MakeDiagnostic(TEXT("evidence_adapter_unavailable"), Lookup.Message));
	return Lookup;
}

FBlueprintHelperReviewRestoreAdapterLookup FBlueprintHelperReviewAdapterRegistry::FindRestoreAdapter(
	const FString& TargetKind) const
{
	FBlueprintHelperReviewRestoreAdapterLookup Lookup;
	Lookup.TargetKind = NormalizeTargetKind(TargetKind);
	if (const TSharedPtr<IBlueprintHelperReviewRestoreAdapter>* Adapter = RestoreAdaptersByTargetKind.Find(Lookup.TargetKind))
	{
		Lookup.bAvailable = Adapter->IsValid();
		Lookup.Adapter = *Adapter;
		Lookup.Message = Lookup.bAvailable ? TEXT("available") : TEXT("restore_adapter_invalid");
		return Lookup;
	}

	Lookup.Message = FString::Printf(TEXT("restore_adapter_unavailable:%s"), *Lookup.TargetKind);
	Lookup.Diagnostics.Add(MakeDiagnostic(TEXT("restore_adapter_unavailable"), Lookup.Message));
	return Lookup;
}

void FBlueprintHelperReviewAdapterRegistry::RegisterBuiltInAdapters()
{
	TArray<FBlueprintHelperReviewAdapterRegistryDiagnostic> Diagnostics;
	RegisterEvidenceAdapter(MakeShared<FBlueprintHelperReviewMaterialInstanceEvidenceAdapter>(TEXT("material_instance")), Diagnostics);
	RegisterEvidenceAdapter(MakeShared<FBlueprintHelperReviewMaterialInstanceEvidenceAdapter>(TEXT("material_instance_parameter")), Diagnostics);
	RegisterRestoreAdapter(MakeShared<FBlueprintHelperReviewMaterialInstanceRestoreAdapter>(TEXT("material_instance")), Diagnostics);
	RegisterRestoreAdapter(MakeShared<FBlueprintHelperReviewMaterialInstanceRestoreAdapter>(TEXT("material_instance_parameter")), Diagnostics);
	RegisterRestoreAdapter(MakeShared<FBlueprintHelperReviewAssetFactoryRestoreAdapter>(), Diagnostics);
	for (const FString& TargetKind : FBlueprintHelperReviewSnapshotRestoreAdapter::GetSupportedTargetKinds())
	{
		RegisterRestoreAdapter(MakeShared<FBlueprintHelperReviewSnapshotRestoreAdapter>(TargetKind), Diagnostics);
	}
}

FString FBlueprintHelperReviewAdapterRegistry::NormalizeTargetKind(const FString& TargetKind)
{
	FString Normalized = TargetKind;
	Normalized.TrimStartAndEndInline();
	Normalized.ToLowerInline();
	return Normalized;
}

FBlueprintHelperReviewAdapterRegistryDiagnostic FBlueprintHelperReviewAdapterRegistry::MakeDiagnostic(
	const FString& Code,
	const FString& Message)
{
	FBlueprintHelperReviewAdapterRegistryDiagnostic Diagnostic;
	Diagnostic.Code = Code;
	Diagnostic.Message = Message;
	return Diagnostic;
}
