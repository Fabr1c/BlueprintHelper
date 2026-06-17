// Registry for DebugCase export projections.

#include "Systems/Debug/BlueprintHelperDebugCaseProjectionRegistry.h"

#include "Systems/Debug/BlueprintHelperDebugCaseBuiltinProjectionAdapter.h"

FBlueprintHelperDebugCaseProjectionRegistry FBlueprintHelperDebugCaseProjectionRegistry::BuildDefault()
{
	FBlueprintHelperDebugCaseProjectionRegistry Registry;
	Registry.RegisterAdapter(MakeUnique<FBlueprintHelperDebugCaseBuiltinProjectionAdapter>(
		EBlueprintHelperDebugCaseBuiltinProjectionKind::EvidenceSummary));
	Registry.RegisterAdapter(MakeUnique<FBlueprintHelperDebugCaseBuiltinProjectionAdapter>(
		EBlueprintHelperDebugCaseBuiltinProjectionKind::FragmentSummary));
	Registry.RegisterAdapter(MakeUnique<FBlueprintHelperDebugCaseBuiltinProjectionAdapter>(
		EBlueprintHelperDebugCaseBuiltinProjectionKind::ReviewSummary));
	Registry.RegisterAdapter(MakeUnique<FBlueprintHelperDebugCaseBuiltinProjectionAdapter>(
		EBlueprintHelperDebugCaseBuiltinProjectionKind::DebugCaseSummary));
	return Registry;
}

void FBlueprintHelperDebugCaseProjectionRegistry::RegisterAdapter(
	TUniquePtr<IBlueprintHelperDebugCaseProjectionAdapter>&& Adapter)
{
	if (Adapter.IsValid())
	{
		Adapters.Add(MoveTemp(Adapter));
	}
}

bool FBlueprintHelperDebugCaseProjectionRegistry::BuildProjection(
	const FBlueprintHelperDebugCase& DebugCase,
	const FBlueprintHelperDebugCaseProjectionContext& Context,
	FBlueprintHelperDebugCaseProjectionResult& OutResult,
	FString* OutError) const
{
	OutResult = FBlueprintHelperDebugCaseProjectionResult();
	for (const TUniquePtr<IBlueprintHelperDebugCaseProjectionAdapter>& Adapter : Adapters)
	{
		if (!Adapter.IsValid())
		{
			continue;
		}
		if (!Adapter->BuildArtifacts(DebugCase, Context, OutResult, OutError))
		{
			return false;
		}
	}
	if (OutError)
	{
		OutError->Reset();
	}
	return true;
}

int32 FBlueprintHelperDebugCaseProjectionRegistry::GetAdapterCount() const
{
	return Adapters.Num();
}
