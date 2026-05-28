// BlueprintHelper shared safety profile resolver

#include "Systems/Config/BlueprintHelperSafetyProfileResolver.h"

#include "Systems/Config/BlueprintHelperRuntimeSettingResolver.h"
#include "Systems/Config/Utils/BlueprintHelperConfigUtils.h"

EBlueprintHelperSafetyProfile FBlueprintHelperSafetyProfileResolver::ResolveSafetyProfile()
{
	const FString ActiveProfile = FBlueprintHelperRuntimeSettingResolver::GetString(TEXT("active_profile"), TEXT("default"));
	const FString SafetyProfile = FBlueprintHelperRuntimeSettingResolver::GetString(
		UBlueprintHelperConfigUtils::BuildActiveProfileSafetyPath(ActiveProfile),
		TEXT("standard"));
	return UBlueprintHelperConfigUtils::ParseSafetyProfile(SafetyProfile);
}

FString FBlueprintHelperSafetyProfileResolver::ResolveSafetyProfileString()
{
	return SafetyProfileToString(ResolveSafetyProfile());
}

bool FBlueprintHelperSafetyProfileResolver::IsPreviewRequired()
{
	return FBlueprintHelperRuntimeSettingResolver::GetBool(TEXT("safety.preview_required"), true);
}

bool FBlueprintHelperSafetyProfileResolver::IsWriteApprovalRequired()
{
	const bool bDefaultWriteApprovalRequired = ResolveSafetyProfile() != EBlueprintHelperSafetyProfile::AutoRepair;
	return FBlueprintHelperRuntimeSettingResolver::GetBool(
		TEXT("safety.write_approval_required"),
		bDefaultWriteApprovalRequired);
}

bool FBlueprintHelperSafetyProfileResolver::IsApprovalBypassEnabled()
{
	const EBlueprintHelperSafetyProfile Profile = ResolveSafetyProfile();
	if (Profile == EBlueprintHelperSafetyProfile::ReadOnly)
	{
		return false;
	}

	const bool bDefaultApprovalBypass = Profile == EBlueprintHelperSafetyProfile::AutoRepair;
	const bool bApprovalBypass = FBlueprintHelperRuntimeSettingResolver::GetBool(
		TEXT("safety.approval_bypass"),
		bDefaultApprovalBypass);
	return bApprovalBypass || !IsWriteApprovalRequired();
}

bool FBlueprintHelperSafetyProfileResolver::IsAutoRepair()
{
	return ResolveSafetyProfile() == EBlueprintHelperSafetyProfile::AutoRepair;
}
