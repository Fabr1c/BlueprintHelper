// BlueprintHelper Utils -- TaskSpec/ReadContext workbench 工具函数库

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UI/TaskSpecWorkbench/BlueprintHelperTaskSpecWorkbenchData.h"
#include "Shared/GraphWrite/BlueprintHelperCallFunctionCandidateTypes.h"
#include "BlueprintHelperTaskSpecWorkbenchUtils.generated.h"

struct FBlueprintHelperCallFunctionCandidate;
struct FBlueprintHelperCallFunctionResolveResult;

/** Description of a CallFunction statement for candidate resolution. */
struct FCallStatementDescriptor
{
	FString CardId;
	FString Path;
	FString Query;
	FString SearchMode;
	FString AmbiguityPolicy;
	TArray<FString> CategoryPriority;
	TArray<FString> ArgumentNames;
	TMap<FString, FString> ArgumentTypes;
};

UCLASS()
class BLUEPRINTHELPER_API UBlueprintHelperTaskSpecWorkbenchUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static FString SanitizeIdSegment(const FString& Value);

	static bool TryDeserializeJsonObject(const FString& SourceText, TSharedPtr<FJsonObject>& OutObject, FString& OutError);

	static FString SerializeJsonObject(const TSharedPtr<FJsonObject>& Object);

	static FString JsonValueToDisplayString(const TSharedPtr<FJsonValue>& Value);

	static FString ReadStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName);

	static void ReadStringArrayField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, TArray<FString>& OutValues);

	static bool IsCallStatementKind(const FString& Kind);

	static bool IsGraphStatementKind(const FString& Kind);

	static FString ResolveCallQuery(const TSharedPtr<FJsonObject>& Object, const FString& Kind);

	static FString ResolveStatementTitle(const TSharedPtr<FJsonObject>& Object, const FString& Kind);

	static FString ResolveStatementDetail(const TSharedPtr<FJsonObject>& Object);

	static FString InferExpressionType(const TSharedPtr<FJsonValue>& Value);

	static void ReadCallMatchOptions(const TSharedPtr<FJsonObject>& Object, FCallStatementDescriptor& Descriptor);

	static void ReadCallArguments(const TSharedPtr<FJsonObject>& Object, FCallStatementDescriptor& Descriptor);

	static void CollectCallStatementsFromValue(const TSharedPtr<FJsonValue>& Value, const FString& Path, TArray<FCallStatementDescriptor>& OutCalls);

	static void CollectCallStatementsFromObject(const TSharedPtr<FJsonObject>& Object, const FString& Path, TArray<FCallStatementDescriptor>& OutCalls);

	static FBlueprintHelperCallFunctionCandidateRowModel MakeRowFromCandidateInfo(const FBlueprintHelperCallFunctionCandidateInfo& Info, int32 Index);

	static FBlueprintHelperCallFunctionCandidateInfo MakeCandidateInfo(const FBlueprintHelperCallFunctionCandidate& Candidate);

	static FString ResolveStatusText(const FBlueprintHelperCallFunctionResolveResult& Result);

	static void AddPreviewBlock(FBlueprintHelperTaskSpecPreviewModel& Model, EBlueprintHelperTaskSpecPreviewBlockKind Kind, const FString& SourcePath, const FString& Title, const FString& Detail);

	static void CollectPreviewBlocksFromValue(const TSharedPtr<FJsonValue>& Value, const FString& Path, FBlueprintHelperTaskSpecPreviewModel& Model);

	static void CollectPreviewBlocksFromObject(const TSharedPtr<FJsonObject>& Object, const FString& Path, FBlueprintHelperTaskSpecPreviewModel& Model);

	static void AddTopLevelNonGraphBlock(const TSharedPtr<FJsonObject>& RootObject, const TCHAR* FieldName, FBlueprintHelperTaskSpecPreviewModel& Model);
};
