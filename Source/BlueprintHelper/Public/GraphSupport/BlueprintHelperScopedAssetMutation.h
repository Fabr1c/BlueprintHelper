// BlueprintHelper Service Layer — scoped asset mutation helper

#pragma once

#include "CoreMinimal.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"

class BLUEPRINTHELPER_API FBlueprintHelperScopedAssetMutation
{
public:
	FBlueprintHelperScopedAssetMutation(const FText& TransactionText, UObject* InPrimaryObject = nullptr)
		: Transaction(TransactionText)
		, PrimaryObject(InPrimaryObject)
	{
		if (PrimaryObject)
		{
			PrimaryPackage = PrimaryObject->GetOutermost();
			bPrimaryPackageWasDirty = PrimaryPackage ? PrimaryPackage->IsDirty() : false;
		}

		if (PrimaryObject)
		{
			PrimaryObject->Modify();
		}
	}

	~FBlueprintHelperScopedAssetMutation()
	{
		if (!bCommitted && !bRolledBack)
		{
			Rollback();
		}
	}

	FBlueprintHelperScopedAssetMutation(const FBlueprintHelperScopedAssetMutation&) = delete;
	FBlueprintHelperScopedAssetMutation& operator=(const FBlueprintHelperScopedAssetMutation&) = delete;

	void Modify(UObject* Object) const
	{
		if (Object)
		{
			Object->Modify();
		}
	}

	void Commit()
	{
		if (bRolledBack)
		{
			return;
		}

		bCommitted = true;
		if (PrimaryObject)
		{
			PrimaryObject->MarkPackageDirty();
		}
	}

	void Rollback()
	{
		if (bCommitted || bRolledBack)
		{
			return;
		}

		bRolledBack = true;
		Transaction.Cancel();
		RestorePrimaryPackageDirtyState();
	}

	bool WasCommitted() const { return bCommitted; }
	bool WasRolledBack() const { return bRolledBack; }

	void RestorePrimaryPackageDirtyState() const
	{
		if (PrimaryPackage)
		{
			PrimaryPackage->SetDirtyFlag(bPrimaryPackageWasDirty);
		}
	}

private:
	FScopedTransaction Transaction;
	UObject* PrimaryObject = nullptr;
	UPackage* PrimaryPackage = nullptr;
	bool bPrimaryPackageWasDirty = false;
	bool bCommitted = false;
	bool bRolledBack = false;
};
