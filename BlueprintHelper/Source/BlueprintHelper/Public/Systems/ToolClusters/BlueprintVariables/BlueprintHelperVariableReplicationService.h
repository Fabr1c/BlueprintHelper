#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/BlueprintVariables/BlueprintHelperVariableReplicationTypes.h"

class FJsonObject;
class UBlueprint;
struct FBPVariableDescription;

class BLUEPRINTHELPER_API FBlueprintHelperVariableReplicationService
{
public:
	static bool TryParseRequest(
		const TSharedPtr<FJsonObject>& ValueObject,
		const FName VariableName,
		FBlueprintHelperVariableReplicationRequest& OutRequest,
		FBlueprintHelperVariableReplicationError& OutError);

	static bool ApplyToMemberVariable(
		UBlueprint* Blueprint,
		const FName VariableName,
		FBPVariableDescription& Variable,
		const FBlueprintHelperVariableReplicationRequest& Request,
		bool& bOutChanged,
		FBlueprintHelperVariableReplicationError& OutError);

	static FBlueprintHelperVariableReplicationFacts ReadMemberVariableFacts(
		const UBlueprint& Blueprint,
		const FBPVariableDescription& Variable);

	static FString ModeToString(EBlueprintHelperVariableReplicationMode Mode);
	static FString ConditionToString(ELifetimeCondition Condition);
	static FString ConditionToEngineName(ELifetimeCondition Condition);
	static bool TryStringToCondition(const FString& Value, ELifetimeCondition& OutCondition);
	static bool TryStringToMode(const FString& Value, EBlueprintHelperVariableReplicationMode& OutMode);
};
