// BlueprintHelper settings presenter.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Config/BlueprintHelperSettingStore.h"

class BLUEPRINTHELPER_API FBlueprintHelperSettingsPresenter
{
public:
	const FBlueprintHelperSettingView& Reload();
	const FBlueprintHelperSettingView& EnsureProjectSetting();
	const FBlueprintHelperSettingView& GetView() const;

private:
	FBlueprintHelperSettingView View;
};
