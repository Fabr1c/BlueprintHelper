#include "Systems/Debug/BlueprintHelperScreenshotSettings.h"

#include "Misc/Paths.h"
#include "Systems/Config/BlueprintHelperRuntimeSettingResolver.h"

class FBlueprintHelperScreenshotSettingsLocalUtils
{
public:
	static FString NormalizeRelativeDir(FString Value, const FString& DefaultValue)
	{
		Value.TrimStartAndEndInline();
		FPaths::NormalizeDirectoryName(Value);
		FPaths::CollapseRelativeDirectories(Value);
		if (Value.IsEmpty() ||
			FPaths::IsDrive(Value) ||
			FPaths::IsRelative(Value) == false ||
			Value.Contains(TEXT("..")))
		{
			return DefaultValue;
		}
		return Value;
	}

	static FString NormalizePrefix(FString Value)
	{
		Value.TrimStartAndEndInline();
		FString Sanitized;
		for (const TCHAR Ch : Value)
		{
			if (FChar::IsAlnum(Ch) || Ch == TEXT('_') || Ch == TEXT('-') || Ch == TEXT('.'))
			{
				Sanitized.AppendChar(Ch);
			}
		}
		if (Sanitized.IsEmpty())
		{
			return TEXT("editor");
		}
		return Sanitized.Left(48);
	}

	static int32 ClampGraphMaxNodesPerImage(int32 Value)
	{
		return FMath::Clamp(Value, 1, 64);
	}
};

FBlueprintHelperScreenshotSettings FBlueprintHelperScreenshotSettings::Load()
{
	FBlueprintHelperScreenshotSettings Settings;
	Settings.OutputDir = FBlueprintHelperScreenshotSettingsLocalUtils::NormalizeRelativeDir(
		FBlueprintHelperRuntimeSettingResolver::GetString(
			TEXT("debug.screenshot.output_dir"),
			Settings.OutputDir),
		TEXT("Screenshots"));
	Settings.DefaultCaptureTarget = BlueprintHelperParseScreenshotTarget(
		FBlueprintHelperRuntimeSettingResolver::GetString(
			TEXT("debug.screenshot.default_capture_target"),
			BlueprintHelperScreenshotTargetToString(Settings.DefaultCaptureTarget)));
	Settings.FilenamePrefix = FBlueprintHelperScreenshotSettingsLocalUtils::NormalizePrefix(
		FBlueprintHelperRuntimeSettingResolver::GetString(
			TEXT("debug.screenshot.filename_prefix"),
			Settings.FilenamePrefix));
	Settings.GraphMaxNodesPerImage = FBlueprintHelperScreenshotSettingsLocalUtils::ClampGraphMaxNodesPerImage(
		FBlueprintHelperRuntimeSettingResolver::GetInt(
			TEXT("debug.screenshot.graph_max_nodes_per_image"),
			Settings.GraphMaxNodesPerImage));
	return Settings;
}
