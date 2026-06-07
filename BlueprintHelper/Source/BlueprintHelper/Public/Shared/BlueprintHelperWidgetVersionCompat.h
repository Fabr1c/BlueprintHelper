#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6 && ENGINE_MINOR_VERSION <= 7
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

	static FORCEINLINE void UnregisterWidgetVariableByName(UWidgetBlueprint* WidgetBlueprint, const FName& WidgetName)
	{
#if WITH_EDITORONLY_DATA
		if (!WidgetBlueprint || WidgetName.IsNone())
		{
			return;
		}

#if BLUEPRINTHELPER_UE_HAS_WIDGET_VARIABLE_GUID_EVENTS
		if (WidgetBlueprint->WidgetVariableNameToGuidMap.Contains(WidgetName))
		{
			WidgetBlueprint->OnVariableRemoved(WidgetName);
		}
#endif
#endif
	}

	static FORCEINLINE bool RetireSourceWidget(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget, FString& OutError)
	{
		if (!WidgetBlueprint || !Widget)
		{
			return true;
		}

		TArray<UWidget*> WidgetsToRetire;
		WidgetsToRetire.Add(Widget);

		TArray<UWidget*> ChildWidgets;
		UWidgetTree::GetChildWidgets(Widget, ChildWidgets);
		WidgetsToRetire.Append(ChildWidgets);

		TSet<UWidget*> SeenWidgets;
		TArray<TPair<UWidget*, FName>> RetireEntries;
		for (UWidget* Candidate : WidgetsToRetire)
		{
			if (!Candidate || SeenWidgets.Contains(Candidate))
			{
				continue;
			}

			SeenWidgets.Add(Candidate);
			RetireEntries.Emplace(Candidate, Candidate->GetFName());
		}

		for (const TPair<UWidget*, FName>& Entry : RetireEntries)
		{
			UWidget* Candidate = Entry.Key;
			if (!Candidate)
			{
				continue;
			}

			Candidate->SetFlags(RF_Transactional);
			Candidate->Modify();
			if (!Candidate->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional))
			{
				OutError = FString::Printf(TEXT("Could not retire source widget '%s'."), *Entry.Value.ToString());
				return false;
			}
		}

		for (const TPair<UWidget*, FName>& Entry : RetireEntries)
		{
			UnregisterWidgetVariableByName(WidgetBlueprint, Entry.Value);
		}

		return true;
	}
};
