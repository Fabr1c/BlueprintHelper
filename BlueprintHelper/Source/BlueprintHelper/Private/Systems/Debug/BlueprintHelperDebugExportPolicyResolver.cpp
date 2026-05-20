// BlueprintHelper debug export policy resolver implementation.

#include "Systems/Debug/BlueprintHelperDebugExportPolicyResolver.h"

#include "Systems/Config/BlueprintHelperRuntimeSettingResolver.h"

FBlueprintHelperDebugExportPolicy FBlueprintHelperDebugExportPolicyResolver::Load()
{
	FBlueprintHelperDebugExportPolicy Policy;
	Policy.ExportProfile = FBlueprintHelperRuntimeSettingResolver::GetString(
		TEXT("debug.export_profile"),
		Policy.ExportProfile);
	Policy.ExportProfile.TrimStartAndEndInline();
	if (Policy.ExportProfile.IsEmpty())
	{
		Policy.ExportProfile = TEXT("standard");
	}
	Policy.bContainsFullSettings = FBlueprintHelperRuntimeSettingResolver::GetBool(
		TEXT("debug.contains_full_settings"),
		Policy.bContainsFullSettings);
	return Policy;
}
