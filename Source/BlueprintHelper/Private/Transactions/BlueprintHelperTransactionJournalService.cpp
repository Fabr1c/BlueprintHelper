// BlueprintHelper Service Layer — Transaction Journal 服务实现

#include "Transactions/BlueprintHelperTransactionJournalService.h"
#include "Structure/BlueprintHelperAppendGraphTypes.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/DateTime.h"

FString FBlueprintHelperTransactionJournalService::GenerateTransactionId() const
{
	const FDateTime Now = FDateTime::UtcNow();
	return FString::Printf(TEXT("tx_%lld%03d"),
		static_cast<int64>(Now.ToUnixTimestamp()),
		Now.GetMillisecond());
}

FString FBlueprintHelperTransactionJournalService::GetJournalRootPath() const
{
	return FPaths::ProjectSavedDir() / TEXT("BlueprintHelper") / TEXT("Transactions") / TEXT("Active");
}

FString FBlueprintHelperTransactionJournalService::GetReviewRootPath() const
{
	return FPaths::ProjectSavedDir() / TEXT("BlueprintHelper") / TEXT("Review");
}

bool FBlueprintHelperTransactionJournalService::WriteAppendJournal(
	const FBlueprintHelperAppendJournalRecord& Record,
	FString& OutError) const
{
	const FString JournalDir = GetJournalRootPath();
	if (!IFileManager::Get().DirectoryExists(*JournalDir))
	{
		IFileManager::Get().MakeDirectory(*JournalDir, true);
	}

	const FString ReviewDir = GetReviewRootPath();
	if (!IFileManager::Get().DirectoryExists(*ReviewDir))
	{
		IFileManager::Get().MakeDirectory(*ReviewDir, true);
	}

	// 写入 Active Journal
	const FString JournalPath = JournalDir / FString::Printf(TEXT("%s.json"), *Record.TransactionId);
	{
		FString JournalJson;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JournalJson);
		if (!FJsonSerializer::Serialize(Record.ToJson(), Writer))
		{
			OutError = TEXT("Journal 记录 JSON 序列化失败。");
			return false;
		}

		if (!FFileHelper::SaveStringToFile(JournalJson, *JournalPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FString::Printf(TEXT("无法写入 Journal 文件：%s"), *JournalPath);
			return false;
		}
	}

	// 写入 Review Store 副本
	const FString ReviewPath = ReviewDir / FString::Printf(TEXT("%s.json"), *Record.TransactionId);
	{
		FString ReviewJson;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ReviewJson);
		if (!FJsonSerializer::Serialize(Record.ToJson(), Writer))
		{
			OutError = TEXT("Review 记录 JSON 序列化失败。");
			return false;
		}

		if (!FFileHelper::SaveStringToFile(ReviewJson, *ReviewPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FString::Printf(TEXT("无法写入 Review 文件：%s"), *ReviewPath);
			return false;
		}
	}

	return true;
}
