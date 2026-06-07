#include "Runtime/TaskRuntime/WriteUnitOfWork/BlueprintHelperWriteFamilyAdapterRegistry.h"

#include "Runtime/TaskRuntime/WriteContracts/BlueprintHelperWriteFamilyDescriptor.h"

class FBlueprintHelperStaticWriteFamilyAdapter final : public IBlueprintHelperWriteFamilyAdapter
{
public:
	explicit FBlueprintHelperStaticWriteFamilyAdapter(
		const FBlueprintHelperWriteFamilyDescriptor& InDescriptor)
		: Descriptor(InDescriptor)
	{
	}

	FString GetWriteFamily() const override
	{
		return Descriptor.WriteFamily;
	}

	FString GetRuntimeAdapterId() const override
	{
		return Descriptor.RuntimeAdapterId;
	}

	bool BuildUnitOfWorkRequest(
		const FBlueprintHelperAcceptedPayloadModel& AcceptedPayload,
		FBlueprintHelperWriteUnitOfWorkRequest& OutRequest,
		FBlueprintHelperToolError& OutError) const override
	{
		if (Descriptor.WriteFamily.IsEmpty() || Descriptor.RuntimeAdapterId.IsEmpty())
		{
			OutError.Code = TEXT("write_family_descriptor_invalid");
			OutError.Stage = EBlueprintHelperToolStage::ParseInput;
			OutError.Message = TEXT("Write-family descriptor must declare write_family and runtime_adapter_id.");
			OutError.Field = TEXT("write_family_descriptor");
			OutError.bRetryable = false;
			OutError.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
			return false;
		}

		OutRequest.AcceptedPayload = AcceptedPayload;
		OutRequest.Descriptor = Descriptor;
		return true;
	}

private:
	FBlueprintHelperWriteFamilyDescriptor Descriptor;
};

class FBlueprintHelperWriteFamilyAdapterCatalog
{
public:
	static const TArray<TSharedPtr<IBlueprintHelperWriteFamilyAdapter>>& Get()
	{
		static const TArray<TSharedPtr<IBlueprintHelperWriteFamilyAdapter>> Adapters = BuildAdapters();
		return Adapters;
	}

private:
	static TArray<TSharedPtr<IBlueprintHelperWriteFamilyAdapter>> BuildAdapters()
	{
		TArray<TSharedPtr<IBlueprintHelperWriteFamilyAdapter>> Adapters;
		for (const FBlueprintHelperWriteFamilyDescriptor& Descriptor :
			FBlueprintHelperWriteFamilyDescriptorRegistry::GetKnownDescriptors())
		{
			if (Descriptor.Status == EBlueprintHelperWriteFamilyCapabilityStatus::Active)
			{
				Adapters.Add(MakeShared<FBlueprintHelperStaticWriteFamilyAdapter>(Descriptor));
			}
		}
		return Adapters;
	}
};

bool FBlueprintHelperWriteFamilyAdapterRegistry::TryFindByWriteFamily(
	const FString& WriteFamily,
	TSharedPtr<IBlueprintHelperWriteFamilyAdapter>& OutAdapter)
{
	for (const TSharedPtr<IBlueprintHelperWriteFamilyAdapter>& Adapter :
		FBlueprintHelperWriteFamilyAdapterCatalog::Get())
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

TArray<FString> FBlueprintHelperWriteFamilyAdapterRegistry::GetRegisteredWriteFamilies()
{
	TArray<FString> WriteFamilies;
	for (const TSharedPtr<IBlueprintHelperWriteFamilyAdapter>& Adapter :
		FBlueprintHelperWriteFamilyAdapterCatalog::Get())
	{
		if (Adapter.IsValid())
		{
			WriteFamilies.Add(Adapter->GetWriteFamily());
		}
	}
	return WriteFamilies;
}
