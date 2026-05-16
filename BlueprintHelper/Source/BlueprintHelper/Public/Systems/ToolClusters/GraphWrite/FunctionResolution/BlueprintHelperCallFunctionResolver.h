#pragma once

#include "CoreMinimal.h"
#include "Math/Vector2D.h"
#include "Templates/SubclassOf.h"

class UBlueprint;
class UBlueprintNodeSpawner;
class UEdGraph;
class UFunction;
class UK2Node;
class UK2Node_CallFunction;

enum class EBlueprintHelperCallFunctionResolveStatus : uint8
{
	Resolved,
	Ambiguous,
	NotFound,
	Blocked
};

struct FBlueprintHelperCallFunctionCandidate
{
	FString StableId;
	FString OwnerClassPath;
	FString NativeFunctionName;
	FString DisplayName;
	FString Category;
	FString NodeClassPath;
	FString MatchReason;
	int32 Score = 0;
	bool bGraphCompatible = false;
	bool bFromActionDatabase = false;
	TWeakObjectPtr<UFunction> Function;
	TSubclassOf<UK2Node_CallFunction> NodeClass;
	TWeakObjectPtr<UBlueprintNodeSpawner> NodeSpawner;
};

struct FBlueprintHelperCallFunctionResolveRequest
{
	UBlueprint* Blueprint = nullptr;
	UEdGraph* Graph = nullptr;
	FString Query;
	FString SearchMode;
	FString AmbiguityPolicy;
	TArray<FString> CategoryPriority;
	TArray<FString> ArgumentNames;
	bool bAllowFuzzyUnique = true;
	int32 MaxCandidates = 8;
};

struct FBlueprintHelperCallFunctionResolveResult
{
	EBlueprintHelperCallFunctionResolveStatus Status = EBlueprintHelperCallFunctionResolveStatus::NotFound;
	FString ErrorCode;
	FString Message;
	FBlueprintHelperCallFunctionCandidate Selected;
	TArray<FBlueprintHelperCallFunctionCandidate> Candidates;
	TArray<FString> CandidateFunctions;

	bool IsResolved() const
	{
		return Status == EBlueprintHelperCallFunctionResolveStatus::Resolved && Selected.Function.IsValid();
	}
};

class BLUEPRINTHELPER_API FBlueprintHelperCallFunctionResolver
{
public:
	static FBlueprintHelperCallFunctionResolveResult Resolve(const FBlueprintHelperCallFunctionResolveRequest& Request);
	static FString MakeStableId(const UFunction* Function);
	static bool TryParseQualifiedQuery(const FString& Query, FString& OutOwner, FString& OutFunction);
	static UK2Node* SpawnResolvedNode(UEdGraph* Graph, const FBlueprintHelperCallFunctionCandidate& Candidate, const FVector2D& Location, FString& OutError);
};
