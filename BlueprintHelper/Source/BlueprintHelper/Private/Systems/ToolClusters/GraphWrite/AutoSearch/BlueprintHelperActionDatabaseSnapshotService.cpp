// GraphWrite AutoSearch ActionDatabase pure-data snapshot service.

#include "Systems/ToolClusters/GraphWrite/AutoSearch/BlueprintHelperActionDatabaseSnapshotService.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintNodeSpawner.h"
#include "HAL/PlatformTime.h"
#include "Misc/Crc.h"

class FBlueprintHelperActionDatabaseSnapshotServiceLocalUtils
{
public:
	static FString ObjectPath(const UObject* Object)
	{
		return Object ? Object->GetPathName() : FString();
	}

	static FString ObjectName(const UObject* Object)
	{
		return Object ? Object->GetName() : FString();
	}

	static FString BuildStableText(
		const FString& OwnerPath,
		const FString& NodeClassPath,
		const FString& SpawnerClassPath,
		const FString& SpawnerName)
	{
		return FString::Printf(
			TEXT("owner=%s|node=%s|spawner_class=%s|spawner=%s"),
			*OwnerPath,
			*NodeClassPath,
			*SpawnerClassPath,
			*SpawnerName);
	}

	static FString HashStableText(const FString& StableText)
	{
		return FString::Printf(TEXT("%08x"), FCrc::StrCrc32(*StableText));
	}
};

FBlueprintHelperActionDatabaseSnapshot FBlueprintHelperActionDatabaseSnapshotService::BuildSnapshotOnGameThread()
{
	check(IsInGameThread());

	FBlueprintHelperActionDatabaseSnapshot Snapshot;
	Snapshot.Generation = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Snapshot.BuiltAtUtc = FDateTime::UtcNow();

	FBlueprintActionDatabase& Database = FBlueprintActionDatabase::Get();
	Database.RefreshAll();

	const FBlueprintActionDatabase::FActionRegistry& Registry = Database.GetAllActions();
	for (const TPair<FObjectKey, FBlueprintActionDatabase::FActionList>& Pair : Registry)
	{
		const UObject* OwnerObject = Pair.Key.ResolveObjectPtr();
		const FString OwnerPath = FBlueprintHelperActionDatabaseSnapshotServiceLocalUtils::ObjectPath(OwnerObject);
		const FString OwnerShort = FBlueprintHelperActionDatabaseSnapshotServiceLocalUtils::ObjectName(OwnerObject);

		for (const TObjectPtr<UBlueprintNodeSpawner>& SpawnerPtr : Pair.Value)
		{
			UBlueprintNodeSpawner* Spawner = SpawnerPtr.Get();
			if (!Spawner)
			{
				continue;
			}

			const UClass* NodeClass = Spawner->NodeClass.Get();
			const UClass* SpawnerClass = Spawner->GetClass();
			const FString NodeClassPath = FBlueprintHelperActionDatabaseSnapshotServiceLocalUtils::ObjectPath(NodeClass);
			const FString SpawnerClassPath = FBlueprintHelperActionDatabaseSnapshotServiceLocalUtils::ObjectPath(SpawnerClass);
			const FString SpawnerName = Spawner->GetName();
			const FString StableText = FBlueprintHelperActionDatabaseSnapshotServiceLocalUtils::BuildStableText(
				OwnerPath,
				NodeClassPath,
				SpawnerClassPath,
				SpawnerName);

			FBlueprintHelperActionDatabaseSnapshotEntry Entry;
			Entry.OwnerPath = OwnerPath;
			Entry.OwnerShort = OwnerShort;
			Entry.NodeClassPath = NodeClassPath;
			Entry.SpawnerClassPath = SpawnerClassPath;
			Entry.StableId = FBlueprintHelperActionDatabaseSnapshotServiceLocalUtils::HashStableText(StableText);
			Entry.SpawnerSignatureHash = Entry.StableId;
			Entry.DisplayName = NodeClass ? NodeClass->GetDisplayNameText().ToString() : FString();
			Entry.MenuName = !SpawnerName.IsEmpty() ? SpawnerName : SpawnerClassPath;
			Entry.ActionFamily = TEXT("action_database");
			if (!OwnerShort.IsEmpty())
			{
				Entry.Keywords.Add(OwnerShort);
			}
			if (!NodeClassPath.IsEmpty())
			{
				Entry.Keywords.Add(NodeClassPath);
			}
			if (!SpawnerClassPath.IsEmpty())
			{
				Entry.Keywords.Add(SpawnerClassPath);
			}

			Snapshot.Entries.Add(MoveTemp(Entry));
			++Snapshot.SpawnerCount;
		}
	}

	Snapshot.EntryCount = Snapshot.Entries.Num();
	bDirty = false;
	return Snapshot;
}

void FBlueprintHelperActionDatabaseSnapshotService::MarkDirty()
{
	bDirty = true;
}

bool FBlueprintHelperActionDatabaseSnapshotService::IsDirty() const
{
	return bDirty;
}
