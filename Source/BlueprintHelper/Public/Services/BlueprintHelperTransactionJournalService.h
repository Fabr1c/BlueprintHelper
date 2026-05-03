// BlueprintHelper Service Layer — Transaction Journal 服务

#pragma once

#include "CoreMinimal.h"
#include "Services/BlueprintHelperAppendGraphTypes.h"

/**
 * Transaction Journal 服务。
 * 负责生成 TransactionId 和写入 Append Journal 记录。
 * Agent-facing 结果只返回 write_ref（transaction_id + journal_recorded）。
 */
class BLUEPRINTHELPER_API FBlueprintHelperTransactionJournalService
{
public:
	/** 生成唯一 TransactionId。 */
	FString GenerateTransactionId() const;

	/** 写入 Append Journal 记录到 Saved/ 目录。 */
	bool WriteAppendJournal(
		const FBlueprintHelperAppendJournalRecord& Record,
		FString& OutError) const;

private:
	/** 获取 Journal 落盘根目录。 */
	FString GetJournalRootPath() const;

	/** 获取 Review Store 根目录。 */
	FString GetReviewRootPath() const;
};
