#pragma once
#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"
#include "GraphWriteTestingUtils.generated.h"

class UFunction;
class UEdGraphNode;
class UK2Node_CallFunction;
struct FBlueprintHelperCallFunctionCandidateInfo;
struct FBlueprintHelperActionResolutionResult;
struct FBlueprintHelperContainerActionReadbackExpectation;
struct FBlueprintHelperContainerActionSpec;
struct FBlueprintHelperContainerActionRoleBinding;

USTRUCT()
struct BLUEPRINTHELPER_API FExpectedContainerActionPinType
{
	GENERATED_BODY()

	FString CategoryToken;
	EPinContainerType ContainerType = EPinContainerType::None;
	FString TerminalCategoryToken;
};

UCLASS()
class BLUEPRINTHELPER_API UGraphWriteTestingUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// From BlueprintHelperOpCoverageReadbackVerifier.cpp
	static bool Fail(FString& OutFailure, const FString& Message);
	static const FBlueprintHelperCallFunctionCandidateInfo* FirstCandidate(const FBlueprintHelperActionResolutionResult& Result);
	static const FBlueprintHelperCallFunctionCandidateInfo* FindSelectedCandidate(const FBlueprintHelperActionResolutionResult& Result);
	static bool FunctionHasInputPin(const UFunction* Function, const FString& PinName);
	static bool FunctionHasReturnPin(const UFunction* Function);

	// From BlueprintHelperGenericOpsReadbackVerifier.cpp (overloaded Fail differs by parameters)
	static bool Fail(const TCHAR* Code, const FString& Message, FString& OutFailureCode, FString& OutFailure);
	static FString FactValue(const FBlueprintHelperCallFunctionCandidateInfo& Candidate, const FString& Key);
	static bool HasPinNamed(const UEdGraphNode* Node, const FString& PinName, const TOptional<EEdGraphPinDirection> Direction);

	// From BlueprintHelperContainerActionReadbackVerifier.cpp
	static FString NormalizeToken(const FString& Value);
	static FString MakeOperationId(const FBlueprintHelperContainerActionReadbackExpectation& Expectation);
	static FString ExtractFunctionName(const FString& FunctionQuery);
	static EPinContainerType ExpectedContainerType(const FString& ContainerKind);
	static FName CategoryForTypeToken(const FString& TypeToken);
	static FString ExpectedElementType(const FBlueprintHelperContainerActionReadbackExpectation& Expectation);
	static bool TryBuildExpectedRolePinType(
		const FBlueprintHelperContainerActionReadbackExpectation& Expectation,
		const FString& RoleName,
		FExpectedContainerActionPinType& OutType);
	static bool TryBuildExpectedResultPinType(
		const FBlueprintHelperContainerActionReadbackExpectation& Expectation,
		FExpectedContainerActionPinType& OutType);
	static bool PinMatchesExpectedType(
		const UEdGraphPin* Pin,
		const FExpectedContainerActionPinType& Expected,
		FString& OutReason);
	static bool TextMentionsTarget(const FString& Text, const FString& TargetName);
	static bool PinOrOwnerMentionsTarget(const UEdGraphPin* Pin, const FString& TargetName);
	static UEdGraphPin* FindPinByName(UK2Node_CallFunction* Node, const FString& PinName);
	static const FBlueprintHelperContainerActionRoleBinding* FindRoleBinding(
		const FBlueprintHelperContainerActionSpec& Spec,
		const FString& Role);
	static bool IsWildcardPin(const UEdGraphPin* Pin);
	static bool TargetPinLinksToVariable(const UEdGraphPin* TargetPin, const FString& TargetName);
	static bool HasNonWildcardOutput(UK2Node_CallFunction* Node);
	static UEdGraphPin* FindFirstNonWildcardOutput(UK2Node_CallFunction* Node);
	static bool HasLinkedExecFlow(UK2Node_CallFunction* Node);
	static UK2Node_CallFunction* FindContainerActionNode(const UEdGraph* Graph, const FString& FunctionName);
};
