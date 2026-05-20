// BlueprintHelper settings presenter implementation.

#include "UI/Settings/BlueprintHelperSettingsPresenter.h"

const FBlueprintHelperSettingView& FBlueprintHelperSettingsPresenter::Reload()
{
	View = FBlueprintHelperSettingStore::Load();
	return View;
}

const FBlueprintHelperSettingView& FBlueprintHelperSettingsPresenter::EnsureProjectSetting()
{
	FString Path;
	FString Error;
	if (!FBlueprintHelperSettingStore::EnsureProjectSetting(Path, Error))
	{
		View = FBlueprintHelperSettingStore::Load();
		View.ErrorText = Error;
		View.StatusText = Error;
		return View;
	}

	View = FBlueprintHelperSettingStore::Load();
	View.StatusText = FString::Printf(TEXT("Project setting ready: %s"), *Path);
	return View;
}

const FBlueprintHelperSettingView& FBlueprintHelperSettingsPresenter::GetView() const
{
	return View;
}
