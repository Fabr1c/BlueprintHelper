#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Components/Widget.h"
#include "WidgetBlueprint.h"

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
#define BLUEPRINTHELPER_UE_HAS_WIDGET_VARIABLE_GUID_EVENTS 1
#else
#define BLUEPRINTHELPER_UE_HAS_WIDGET_VARIABLE_GUID_EVENTS 0
#endif

class FBlueprintHelperWidgetVersionCompat
{
public:
	static FORCEINLINE void RegisterWidgetVariable(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget)
	{
#if WITH_EDITORONLY_DATA
		if (!WidgetBlueprint || !Widget)
		{
			return;
		}

#if BLUEPRINTHELPER_UE_HAS_WIDGET_VARIABLE_GUID_EVENTS
		if (!WidgetBlueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
		{
			WidgetBlueprint->OnVariableAdded(Widget->GetFName());
		}
#else
		Widget->bIsVariable = true;
#endif
#endif
	}

	static FORCEINLINE void UnregisterWidgetVariable(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget)
	{
#if WITH_EDITORONLY_DATA
		if (!WidgetBlueprint || !Widget)
		{
			return;
		}

#if BLUEPRINTHELPER_UE_HAS_WIDGET_VARIABLE_GUID_EVENTS
		if (WidgetBlueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
		{
			WidgetBlueprint->OnVariableRemoved(Widget->GetFName());
		}
#else
		Widget->bIsVariable = false;
#endif
#endif
	}
};
