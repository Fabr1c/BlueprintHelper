#pragma once

#include "CoreMinimal.h"
#include "Math/Vector2D.h"
#include "Shared/GraphWrite/BlueprintHelperCallFunctionCandidateTypes.h"
#include "Templates/SubclassOf.h"

class UBlueprint;
class UBlueprintNodeSpawner;
class UClass;
class UEdGraph;
class UEdGraphSchema;
class UFunction;
class UK2Node;
class UK2Node_CallFunction;

struct FBlueprintHelperCallFunctionPinType
{
	FString Category;
	FString SubCategory;
	FString ObjectPath;
	FString ContainerType;
	bool bIsReference = false;
	bool bIsConst = false;

	bool IsValid() const
	{
		return !Category.IsEmpty() || !ObjectPath.IsEmpty();
	}
};

struct FBlueprintHelperK2CallContext
{
	UBlueprint* Blueprint = nullptr;
	UEdGraph* Graph = nullptr;
	const UEdGraphSchema* Schema = nullptr;
	UClass* SelfClass = nullptr;
	FString GraphKind;
	TArray<FString> ArgumentNames;
	TMap<FString, FString> ArgumentTypes;
	TMap<FString, FBlueprintHelperCallFunctionPinType> ArgumentPinTypes;
	FString TargetObjectType;
	FBlueprintHelperCallFunctionPinType TargetObjectPinType;
	FString ExpectedReturnType;
	FBlueprintHelperCallFunctionPinType ExpectedReturnPinType;
};

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
	FString ReturnType;
	FString WorldContextPin;
	FString TargetObjectPin;
	TArray<FString> InputPins;
	TMap<FString, FString> InputPinTypes;
	FString MismatchReason;
	int32 Score = 0;
	bool bGraphCompatible = false;
	bool bFromActionDatabase = false;
	bool bBlueprintCallable = false;
	bool bBlueprintPure = false;
	bool bLatent = false;
	bool bRequiresWorldContext = false;
	bool bCustomThunk = false;
	bool bHasArrayParm = false;
	bool bHasArrayTypeDependentParams = false;
	bool bDeterminesOutputType = false;
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
	TMap<FString, FString> ArgumentTypes;
	TMap<FString, FBlueprintHelperCallFunctionPinType> ArgumentPinTypes;
	FString TargetObjectType;
	FBlueprintHelperCallFunctionPinType TargetObjectPinType;
	FString ExpectedReturnType;
	FBlueprintHelperCallFunctionPinType ExpectedReturnPinType;
	FBlueprintHelperK2CallContext Context;
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
	TArray<FBlueprintHelperCallFunctionCandidateInfo> CandidateFunctions;

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
