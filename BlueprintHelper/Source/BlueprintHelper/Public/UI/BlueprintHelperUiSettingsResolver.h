// BlueprintHelper UI runtime settings resolver.

#pragma once

#include "CoreMinimal.h"
#include "UI/BlueprintHelperUiSettings.h"

class BLUEPRINTHELPER_API FBlueprintHelperUiSettingsResolver
{
public:
	static FBlueprintHelperMainWindowSettings LoadMainWindowSettings();
	static FBlueprintHelperNotificationSettings LoadNotificationSettings();
	static FBlueprintHelperTaskSpecWorkbenchSettings LoadTaskSpecWorkbenchSettings();
	static FBlueprintHelperLayoutRuleEditorSettings LoadLayoutRuleEditorSettings();
};