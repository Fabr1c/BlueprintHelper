#pragma once

#include "CoreMinimal.h"

enum class EBlueprintHelperLogicOutputFormat : uint8
{
	LogicJson,
	Markdown
};

enum class EBlueprintHelperLogicDetailLevel : uint8
{
	Brief,
	Normal,
	Debug
};

struct FBlueprintHelperLogicOptions
{
	EBlueprintHelperLogicOutputFormat Format = EBlueprintHelperLogicOutputFormat::LogicJson;
	EBlueprintHelperLogicDetailLevel DetailLevel = EBlueprintHelperLogicDetailLevel::Normal;
	bool bIncludeDataDependencies = true;
	bool bIncludeOrphanNodes = true;
	bool bIncludeNodeIds = false;
	bool bIncludePositions = false;
	bool bIncludeRawNodeTypes = false;
};

struct FBlueprintHelperLogicResult
{
	bool bSuccess = false;
	FString OutputText;
	FString ErrorMessage;
	int32 NodeCount = 0;
	int32 ExecLinkCount = 0;
	int32 DataLinkCount = 0;
	int32 EntryPointCount = 0;
	int32 OrphanNodeCount = 0;
};

class BLUEPRINTHELPER_API FBlueprintHelperLogicProcessor
{
public:
	static FBlueprintHelperLogicResult ProcessRawJson(
		const FString& RawJsonText,
		const FBlueprintHelperLogicOptions& Options);
};
