// BlueprintHelper shared safety profile resolver

#include "Systems/Config/BlueprintHelperSafetyProfileResolver.h"

#include "Systems/Config/BlueprintHelperRuntimeSettingResolver.h"

namespace
{
FString BuildActiveProfileSafetyPath(const FString& ActiveProfile)
{
	const FString SanitizedProfile = ActiveProfile.IsEmpty() ? FString(TEXT("default")) : ActiveProfile;
	return FString::Printf(TEXT("profiles.%s.safety_profile"), *SanitizedProfile);
}

EBlueprintHelperSafetyProfile ParseSafetyProfile(const FString& Profile)
{
	if (Profile.Equals(TEXT("readonly"), ESearchCase::IgnoreCase)
		|| Profile.Equals(TEXT("read_only"), ESearchCase::IgnoreCase)
		|| Profile.Equals(TEXT("read-only"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperSafetyProfile::ReadOnly;
	}
	if (Profile.Equals(TEXT("standard"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperSafetyProfile::Standard;
	}
	if (Profile.Equals(TEXT("autorepair"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperSafetyProfile::AutoRepair;
	}
	return EBlueprintHelperSafetyProfile::Conservative;
}
}

EBlueprintHelperSafetyProfile FBlueprintHelperSafetyProfileResolver::ResolveSafetyProfile()
{
	const FString ActiveProfile = FBlueprintHelperRuntimeSettingResolver::GetString(TEXT("active_profile"), TEXT("default"));
	const FString SafetyProfile = FBlueprintHelperRuntimeSettingResolver::GetString(
		BuildActiveProfileSafetyPath(ActiveProfile),
		TEXT("standard"));
	return ParseSafetyProfile(SafetyProfile);
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
