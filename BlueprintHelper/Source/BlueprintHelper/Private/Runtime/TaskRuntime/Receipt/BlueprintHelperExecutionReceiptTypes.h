// BlueprintHelper execution receipt protocol constants.

#pragma once

#include "CoreMinimal.h"

class FBlueprintHelperExecutionReceiptFields
{
public:
	static const TCHAR* Schema();
	static const TCHAR* SchemaField();
	static const TCHAR* ReceiptId();
	static const TCHAR* RequestId();
	static const TCHAR* CliRunId();
	static const TCHAR* PreviewId();
	static const TCHAR* TaskRunId();
	static const TCHAR* TaskSpecHash();
	static const TCHAR* TaskPlanHash();
	static const TCHAR* PolicyHash();
	static const TCHAR* Status();
	static const TCHAR* Receipt();
	static const TCHAR* JournalRef();
	static const TCHAR* ReadbackRef();
	static const TCHAR* CreatedAt();
	static const TCHAR* UpdatedAt();
	static const TCHAR* TargetAssets();
};
