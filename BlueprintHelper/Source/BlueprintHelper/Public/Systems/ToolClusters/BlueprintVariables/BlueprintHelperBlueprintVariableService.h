// BlueprintHelper Service Layer 。Blueprint Variable / Default / Local Variable 服务

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintVariables/BlueprintHelperBlueprintVariableTypes.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FBlueprintHelperGraphResolver;
class FBlueprintHelperBlueprintStructureService;
class FJsonObject;

class BLUEPRINTHELPER_API FBlueprintHelperBlueprintVariableService
{
public:
	FBlueprintHelperBlueprintVariableService(
		const FBlueprintHelperGraphResolver& InResolver,
		const FBlueprintHelperBlueprintStructureService& InStructureService);

	// ─── Member Variables ───
	FBlueprintHelperToolResultBase ReadMemberVariables(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase AddMemberVariable(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase AddMemberVariables(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase SetMemberVariableProperties(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase RemoveMemberVariable(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase RemoveMemberVariables(const TSharedPtr<FJsonObject>& Payload) const;

	// ─── Member Defaults ───
	FBlueprintHelperToolResultBase ReadMemberDefaults(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase SetMemberDefault(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase SetMemberDefaults(const TSharedPtr<FJsonObject>& Payload) const;

	// ─── Local Variables ───
	FBlueprintHelperToolResultBase ReadLocalVariables(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase AddLocalVariable(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase AddLocalVariables(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase SetLocalVariableProperties(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase RemoveLocalVariable(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase RemoveLocalVariables(const TSharedPtr<FJsonObject>& Payload) const;

private:
	const FBlueprintHelperGraphResolver& Resolver;
	const FBlueprintHelperBlueprintStructureService& StructureService;

	FBlueprintHelperMemberVariableItem ConvertToMemberItem(const struct FBlueprintHelperVariableInfo& Info) const;
};
