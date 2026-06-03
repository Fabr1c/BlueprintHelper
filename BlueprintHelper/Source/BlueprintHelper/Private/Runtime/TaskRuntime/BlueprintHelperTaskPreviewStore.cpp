// BlueprintHelper Task Runtime - editor lifecycle preview store

#include "Runtime/TaskRuntime/BlueprintHelperTaskPreviewStore.h"

#include "Dom/JsonObject.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FBlueprintHelperTaskPreviewStore::FBlueprintHelperTaskPreviewStore(
	int32 InMaxEntries,
	FTimespan InTimeToLive)
	: MaxEntries(InMaxEntries)
	, TimeToLive(InTimeToLive)
{
}

FString FBlueprintHelperTaskPreviewStore::Store(const FBlueprintHelperTaskPreviewStoreCreateRequest& Request)
{
	PruneExpired();

	const FDateTime Now = FDateTime::UtcNow();
	FString Token = GenerateToken();
	while (Entries.Contains(Token))
	{
		Token = GenerateToken();
	}

	FEntry Entry;
	Entry.Token = Token;
	Entry.TaskSpecHash = Request.TaskSpecHash;
	Entry.TaskPlanHash = Request.TaskPlanHash;
	Entry.ExecutionPolicyHash = Request.ExecutionPolicyHash;
	Entry.AssetStateHash = Request.AssetStateHash;
	Entry.ActionContextRevisionManifestHash = Request.ActionContextRevisionManifestHash;
	Entry.ActionContextRevisionManifestJson = CloneJsonObject(Request.ActionContextRevisionManifestJson);
	Entry.CreatedAtIso = Now.ToIso8601();
	Entry.ExpiresAtUtc = Now + TimeToLive;
	Entry.LastAccessedAtUtc = Now;
	Entry.bPassed = Request.bPassed;
	Entry.TaskPlan = CloneTaskPlan(Request.TaskPlan);

	Entries.Add(Token, MoveTemp(Entry));
	TrimToBounds();
	return Token;
}

FBlueprintHelperTaskPreviewStoreResolveResult FBlueprintHelperTaskPreviewStore::Resolve(
	const FString& Token,
	const FString& TaskSpecHash)
{
	PruneExpired();

	FBlueprintHelperTaskPreviewStoreResolveResult Result;
	if (!IsTokenFormatValid(Token))
	{
		Result.ErrorCode = TEXT("preview_token_mismatch");
		Result.ErrorMessage = TEXT("preview_token must be a 32-character hex string.");
		Result.ErrorField = TEXT("preview_token");
		return Result;
	}

	FEntry* Entry = Entries.Find(Token);
	if (!Entry)
	{
		Result.ErrorCode = TEXT("preview_token_missing");
		Result.ErrorMessage = TEXT("Preview token is not available in the current Editor session.");
		Result.ErrorField = TEXT("preview_token");
		return Result;
	}

	if (Entry->TaskSpecHash != TaskSpecHash)
	{
		Result.ErrorCode = TEXT("preview_token_mismatch");
		Result.ErrorMessage = TEXT("preview_token does not match the current TaskSpec hash.");
		Result.ErrorField = TEXT("task_spec_hash");
		return Result;
	}

	Entry->LastAccessedAtUtc = FDateTime::UtcNow();
	Result.bOk = true;
	Result.bPassed = Entry->bPassed;
	Result.TaskPlan = CloneTaskPlan(Entry->TaskPlan);
	Result.AssetStateHash = Entry->AssetStateHash;
	Result.ActionContextRevisionManifestHash = Entry->ActionContextRevisionManifestHash;
	Result.ActionContextRevisionManifestJson = CloneJsonObject(Entry->ActionContextRevisionManifestJson);
	return Result;
}

FString FBlueprintHelperTaskPreviewStore::GenerateToken() const
{
	return FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
}

bool FBlueprintHelperTaskPreviewStore::IsTokenFormatValid(const FString& Token) const
{
	if (Token.Len() != 32)
	{
		return false;
	}

	for (const TCHAR Ch : Token)
	{
		const bool bHex =
			(Ch >= TCHAR('0') && Ch <= TCHAR('9')) ||
			(Ch >= TCHAR('a') && Ch <= TCHAR('f')) ||
			(Ch >= TCHAR('A') && Ch <= TCHAR('F'));
		if (!bHex)
		{
			return false;
		}
	}
	return true;
}

void FBlueprintHelperTaskPreviewStore::PruneExpired()
{
	const FDateTime Now = FDateTime::UtcNow();
	TArray<FString> ExpiredTokens;
	for (const TPair<FString, FEntry>& Pair : Entries)
	{
		if (Pair.Value.ExpiresAtUtc <= Now)
		{
			ExpiredTokens.Add(Pair.Key);
		}
	}
	for (const FString& Token : ExpiredTokens)
	{
		Entries.Remove(Token);
	}
}

void FBlueprintHelperTaskPreviewStore::TrimToBounds()
{
	while (Entries.Num() > MaxEntries)
	{
		FString OldestToken;
		FDateTime OldestAccess = FDateTime::MaxValue();
		for (const TPair<FString, FEntry>& Pair : Entries)
		{
			if (Pair.Value.LastAccessedAtUtc < OldestAccess)
			{
				OldestAccess = Pair.Value.LastAccessedAtUtc;
				OldestToken = Pair.Key;
			}
		}
		if (OldestToken.IsEmpty())
		{
			return;
		}
		Entries.Remove(OldestToken);
	}
}

TSharedPtr<FJsonObject> FBlueprintHelperTaskPreviewStore::CloneTaskPlan(const TSharedPtr<FJsonObject>& Source) const
{
	return CloneJsonObject(Source);
}

TSharedPtr<FJsonObject> FBlueprintHelperTaskPreviewStore::CloneJsonObject(const TSharedPtr<FJsonObject>& Source) const
{
	if (!Source.IsValid())
	{
		return nullptr;
	}

	FString Serialized;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	if (!FJsonSerializer::Serialize(Source.ToSharedRef(), Writer))
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> Cloned;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Serialized);
	if (!FJsonSerializer::Deserialize(Reader, Cloned))
	{
		return nullptr;
	}
	return Cloned;
}
