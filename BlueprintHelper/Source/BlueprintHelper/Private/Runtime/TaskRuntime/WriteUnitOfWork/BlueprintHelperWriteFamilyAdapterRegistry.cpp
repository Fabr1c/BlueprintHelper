#include "Runtime/TaskRuntime/WriteUnitOfWork/BlueprintHelperWriteFamilyAdapterRegistry.h"

#include "Runtime/TaskRuntime/WriteUnitOfWork/Adapters/BlueprintHelperAssetFactoryFamilyAdapter.h"
#include "Runtime/TaskRuntime/WriteUnitOfWork/Adapters/BlueprintHelperBlueprintComponentFamilyAdapter.h"
#include "Runtime/TaskRuntime/WriteUnitOfWork/Adapters/BlueprintHelperBlueprintVariablesFamilyAdapter.h"
#include "Runtime/TaskRuntime/WriteUnitOfWork/Adapters/BlueprintHelperClassSettingsFamilyAdapter.h"
#include "Runtime/TaskRuntime/WriteUnitOfWork/Adapters/BlueprintHelperDataTableFamilyAdapter.h"
#include "Runtime/TaskRuntime/WriteUnitOfWork/Adapters/BlueprintHelperGraphWriteFamilyAdapter.h"
#include "Runtime/TaskRuntime/WriteUnitOfWork/Adapters/BlueprintHelperObjectPropertyFamilyAdapter.h"
#include "Runtime/TaskRuntime/WriteUnitOfWork/Adapters/BlueprintHelperSignatureFamilyAdapter.h"
#include "Runtime/TaskRuntime/WriteUnitOfWork/Adapters/BlueprintHelperUMGWidgetFamilyAdapter.h"

class FBlueprintHelperWriteFamilyAdapterCatalog
{
public:
	static const TArray<TSharedPtr<IBlueprintHelperWriteFamilyAdapter>>& Get()
	{
		static const TArray<TSharedPtr<IBlueprintHelperWriteFamilyAdapter>> Adapters = {
			MakeShared<FBlueprintHelperGraphWriteFamilyAdapter>(),
			MakeShared<FBlueprintHelperAssetFactoryFamilyAdapter>(),
			MakeShared<FBlueprintHelperSignatureFamilyAdapter>(),
			MakeShared<FBlueprintHelperBlueprintVariablesFamilyAdapter>(),
			MakeShared<FBlueprintHelperClassSettingsFamilyAdapter>(),
			MakeShared<FBlueprintHelperBlueprintComponentFamilyAdapter>(),
			MakeShared<FBlueprintHelperObjectPropertyFamilyAdapter>(),
			MakeShared<FBlueprintHelperDataTableFamilyAdapter>(),
			MakeShared<FBlueprintHelperUMGWidgetFamilyAdapter>()
		};
		return Adapters;
	}
};

const TArray<TSharedPtr<IBlueprintHelperWriteFamilyAdapter>>&
FBlueprintHelperWriteFamilyAdapterRegistry::GetAdapters()
{
	return FBlueprintHelperWriteFamilyAdapterCatalog::Get();
}

bool FBlueprintHelperWriteFamilyAdapterRegistry::TryFindByWriteFamily(
	const FString& WriteFamily,
	TSharedPtr<IBlueprintHelperWriteFamilyAdapter>& OutAdapter)
{
	for (const TSharedPtr<IBlueprintHelperWriteFamilyAdapter>& Adapter : GetAdapters())
	{
		if (Adapter.IsValid() && Adapter->GetWriteFamily().Equals(WriteFamily, ESearchCase::IgnoreCase))
		{
			OutAdapter = Adapter;
			return true;
		}
	}
	OutAdapter.Reset();
	return false;
}

bool FBlueprintHelperWriteFamilyAdapterRegistry::TryFindByRuntimeAdapterId(
	const FString& RuntimeAdapterId,
	TSharedPtr<IBlueprintHelperWriteFamilyAdapter>& OutAdapter)
{
	for (const TSharedPtr<IBlueprintHelperWriteFamilyAdapter>& Adapter : GetAdapters())
	{
		if (Adapter.IsValid() && Adapter->GetRuntimeAdapterId().Equals(RuntimeAdapterId, ESearchCase::IgnoreCase))
		{
			OutAdapter = Adapter;
			return true;
		}
	}
	OutAdapter.Reset();
	return false;
}

TArray<FString> FBlueprintHelperWriteFamilyAdapterRegistry::GetRegisteredWriteFamilies()
{
	TArray<FString> WriteFamilies;
	for (const TSharedPtr<IBlueprintHelperWriteFamilyAdapter>& Adapter : GetAdapters())
	{
		if (Adapter.IsValid())
		{
			WriteFamilies.Add(Adapter->GetWriteFamily());
		}
	}
	return WriteFamilies;
}
