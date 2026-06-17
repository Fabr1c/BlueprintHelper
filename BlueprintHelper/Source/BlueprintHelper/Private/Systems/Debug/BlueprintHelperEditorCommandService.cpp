// BlueprintHelper editor command service.

#include "Systems/Debug/BlueprintHelperEditorCommandService.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "FileHelpers.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/StringFormatArg.h"
#include "PlayInEditorDataTypes.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Systems/Editor/BlueprintHelperEditorCloseSafetyGate.h"
#include "Systems/Debug/BlueprintHelperEditorCloseModalCoordinator.h"
#include "Systems/SourceControl/BlueprintHelperSourceControlService.h"
#include "UObject/UObjectIterator.h"

#if BLUEPRINTHELPER_UE_HAS_STRING_OUTPUT_DEVICE
#include "Misc/StringOutputDevice.h"
#endif

static TSharedRef<FJsonObject> BlueprintHelperBuildModalDismissJson(
	const FString& RequestId,
	const FBlueprintHelperEditorModalDismissResult& ModalDismissResult)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("request_id"), RequestId);
	Json->SetBoolField(TEXT("had_active_modal"), ModalDismissResult.bHadActiveModal);
	Json->SetNumberField(TEXT("dismissed_modal_windows"), ModalDismissResult.DismissedCount);
	Json->SetBoolField(TEXT("has_remaining_modal"), ModalDismissResult.bHasRemainingModal);
	if (ModalDismissResult.bHasRemainingModal)
	{
		Json->SetStringField(TEXT("remaining_modal_title"), ModalDismissResult.RemainingModalTitle);
	}

	TArray<TSharedPtr<FJsonValue>> TitleValues;
	for (const FString& Title : ModalDismissResult.DismissedModalTitles)
	{
		TitleValues.Add(MakeShared<FJsonValueString>(Title));
	}
	Json->SetArrayField(TEXT("dismissed_modal_titles"), TitleValues);
	return Json;
}

FBlueprintHelperCommandResult FBlueprintHelperEditorCommandService::Undo() const
{
	FBlueprintHelperCommandResult Result;

	if (!GEditor)
	{
		Result.ErrorMessage = TEXT("GEditor is not available.");
		return Result;
	}

	if (GEditor->IsTransactionActive())
	{
		Result.ErrorMessage = TEXT("Cannot undo while a transaction is active.");
		return Result;
	}

	const bool bUndone = GEditor->UndoTransaction();
	if (!bUndone)
	{
		Result.ErrorMessage = TEXT("No transaction was undone.");
		return Result;
	}

	Result.bSuccess = true;
	Result.Message = TEXT("Undo succeeded.");
	return Result;
}

FBlueprintHelperCommandResult FBlueprintHelperEditorCommandService::Redo() const
{
	FBlueprintHelperCommandResult Result;

	if (!GEditor)
	{
		Result.ErrorMessage = TEXT("GEditor is not available.");
		return Result;
	}

	if (GEditor->IsTransactionActive())
	{
		Result.ErrorMessage = TEXT("Cannot redo while a transaction is active.");
		return Result;
	}

	const bool bRedone = GEditor->RedoTransaction();
	if (!bRedone)
	{
		Result.ErrorMessage = TEXT("No transaction was redone.");
		return Result;
	}

	Result.bSuccess = true;
	Result.Message = TEXT("Redo succeeded.");
	return Result;
}

FBlueprintHelperCommandResult FBlueprintHelperEditorCommandService::PlayInEditor() const
{
	FBlueprintHelperCommandResult Result;

	if (!GEditor)
	{
		Result.ErrorMessage = TEXT("GEditor is not available.");
		return Result;
	}

	if (GEditor->IsPlaySessionInProgress())
	{
		Result.ErrorMessage = TEXT("A PIE session is already running.");
		return Result;
	}

	FRequestPlaySessionParams Params;
	GEditor->RequestPlaySession(Params);

	Result.bSuccess = true;
	Result.Message = TEXT("PIE start was requested.");
	return Result;
}

FBlueprintHelperCommandResult FBlueprintHelperEditorCommandService::StopPIE() const
{
	FBlueprintHelperCommandResult Result;

	if (!GEditor)
	{
		Result.ErrorMessage = TEXT("GEditor is not available.");
		return Result;
	}

	if (!GEditor->IsPlayingSessionInEditor())
	{
		Result.ErrorMessage = TEXT("No PIE session is running.");
		return Result;
	}

	GEditor->RequestEndPlayMap();

	Result.bSuccess = true;
	Result.Message = TEXT("PIE stop was requested.");
	return Result;
}

UClass* FBlueprintHelperEditorCommandService::ResolveParentClass(
	const FString& ClassName,
	FString& OutError)
{
	if (ClassName.IsEmpty())
	{
		OutError = TEXT("Parent class name cannot be empty.");
		return nullptr;
	}

	UClass* Found = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::ExactClass, ELogVerbosity::NoLogging);
	if (Found)
	{
		return Found;
	}

	Found = FindFirstObject<UClass>(*(TEXT("A") + ClassName), EFindFirstObjectOptions::ExactClass, ELogVerbosity::NoLogging);
	if (Found)
	{
		return Found;
	}

	Found = FindFirstObject<UClass>(*(TEXT("U") + ClassName), EFindFirstObjectOptions::ExactClass, ELogVerbosity::NoLogging);
	if (Found)
	{
		return Found;
	}

	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->GetName() == ClassName || It->GetName() == (TEXT("A") + ClassName) || It->GetName() == (TEXT("U") + ClassName))
		{
			return *It;
		}
	}

	OutError = FString::Printf(TEXT("Parent class not found: %s"), *ClassName);
	return nullptr;
}

FBlueprintHelperCreateBlueprintResult FBlueprintHelperEditorCommandService::CreateBlueprint(
	const FString& AssetPath,
	const FString& ParentClassName) const
{
	FBlueprintHelperCreateBlueprintResult Result;

	if (AssetPath.IsEmpty())
	{
		Result.ErrorMessage = TEXT("asset_path cannot be empty.");
		return Result;
	}

	FString ClassError;
	UClass* ParentClass = ResolveParentClass(ParentClassName, ClassError);
	if (!ParentClass)
	{
		Result.ErrorMessage = ClassError;
		return Result;
	}

	if (!FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Cannot create a Blueprint based on %s."), *ParentClass->GetName());
		return Result;
	}

	const FString PackagePath = AssetPath;
	const FString AssetName = FPackageName::GetShortName(AssetPath);

	if (StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Asset already exists: %s"), *AssetPath);
		return Result;
	}

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to create package: %s"), *PackagePath);
		return Result;
	}

	UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(
		ParentClass,
		Package,
		FName(*AssetName),
		BPTYPE_Normal);
	if (!NewBP)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to create Blueprint: %s"), *AssetPath);
		return Result;
	}

	FAssetRegistryModule::AssetCreated(NewBP);
	Package->MarkPackageDirty();

	Result.bSuccess = true;
	Result.AssetPath = NewBP->GetPathName();
	Result.BlueprintName = AssetName;
	Result.ParentClassName = ParentClass->GetName();
	return Result;
}

FBlueprintHelperCommandResult FBlueprintHelperEditorCommandService::ExecConsoleCommand(
	const FString& Command) const
{
	FBlueprintHelperCommandResult Result;

	if (Command.IsEmpty())
	{
		Result.ErrorMessage = TEXT("command cannot be empty.");
		return Result;
	}

	if (!GEditor)
	{
		Result.ErrorMessage = TEXT("GEditor is not available.");
		return Result;
	}

	FStringOutputDevice OutputDevice;
	UWorld* World = GEditor->GetEditorWorldContext().World();
	GEditor->Exec(World, *Command, OutputDevice);

	Result.bSuccess = true;
	Result.Message = OutputDevice.IsEmpty() ? TEXT("Command executed without output.") : *OutputDevice;
	return Result;
}

FBlueprintHelperCommandResult FBlueprintHelperEditorCommandService::DismissEditorDialogs() const
{
	FBlueprintHelperCommandResult Result;

	const FString DismissRequestId = TEXT("dismiss_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FBlueprintHelperEditorModalDismissResult ModalDismissResult =
		FBlueprintHelperEditorCloseModalCoordinator::Get().DismissActiveModalWindowsFromGameThread(DismissRequestId);

	Result.Data = BlueprintHelperBuildModalDismissJson(DismissRequestId, ModalDismissResult);
	if (ModalDismissResult.bHasRemainingModal)
	{
		Result.ErrorMessage = TEXT("An editor modal dialog is still active after dismiss was requested.");
		Result.Message = TEXT("Close the remaining modal dialog manually, then retry the lifecycle command.");
		return Result;
	}

	Result.bSuccess = true;
	Result.Message = ModalDismissResult.DismissedCount > 0
		? FString::Printf(TEXT("Dismissed %d editor modal dialog(s)."), ModalDismissResult.DismissedCount)
		: TEXT("No editor modal dialog was active.");
	return Result;
}

FBlueprintHelperCommandResult FBlueprintHelperEditorCommandService::CloseEditor(bool bSaveAll) const
{
	FBlueprintHelperCommandResult Result;

	if (!GEditor)
	{
		Result.ErrorMessage = TEXT("GEditor is not available.");
		return Result;
	}

	if (bSaveAll)
	{
		TSharedPtr<FJsonObject> SourceControlJson;
		TArray<UPackage*> DirtyPackages;
		FEditorFileUtils::GetDirtyPackages(DirtyPackages);
		const TArray<FString> DirtyPackageInputs = FBlueprintHelperSourceControlService::InputsFromDirtyPackages(DirtyPackages);
		if (!DirtyPackageInputs.IsEmpty())
		{
			const FBlueprintHelperSourceControlService SourceControlService;
			const FBlueprintHelperEditorCloseSafetyGate CloseSafetyGate;
			FBlueprintHelperSourceControlResult SourceControlResult = SourceControlService.QueryStatus(DirtyPackageInputs, /*bUpdateStatus=*/ true);
			if (CloseSafetyGate.ShouldAttemptAutoCheckout(SourceControlResult))
			{
				SourceControlResult = SourceControlService.Checkout(DirtyPackageInputs, /*bUpdateStatus=*/ true);
			}
			const FBlueprintHelperEditorCloseSafetyGateResult CloseSafetyGateResult = CloseSafetyGate.EvaluateDirtyPackageStatus(SourceControlResult);
			if (!CloseSafetyGateResult.bCanProceed)
			{
				Result.ErrorMessage = SourceControlResult.AgentMessage.IsEmpty()
					? TEXT("Source-control checkout is required before closing the editor with save_all=true.")
					: SourceControlResult.AgentMessage;
				Result.Message = CloseSafetyGateResult.Message;
				Result.Data = CloseSafetyGateResult.ToJson();
				return Result;
			}
			SourceControlJson = SourceControlResult.ToJson();
		}

		const bool bSaved = FEditorFileUtils::SaveDirtyPackages(
			/*bPromptUserToSave=*/ false,
			/*bSaveMapPackages=*/ true,
			/*bSaveContentPackages=*/ true);
		if (!bSaved)
		{
			Result.ErrorMessage = TEXT("Some dirty packages failed to save; editor shutdown was not requested.");
			Result.Message = TEXT("Check source-control or file-system state, then retry blueprint_close_editor.");
			Result.Data = SourceControlJson;
			UE_LOG(LogTemp, Warning, TEXT("[BlueprintHelper] CloseEditor: some dirty packages failed to save; shutdown was not requested."));
			return Result;
		}
		Result.Data = SourceControlJson;
	}

	// Do not call CloseAllAssetEditors here. Some Blueprint/asset editor teardown
	// paths can assert while the Bridge request is still unwinding. Save first,
	// then schedule the MainFrame close command so Slate/asset editor tabs get
	// their normal CanCloseManager teardown before the engine exit request.
	// QUIT_EDITOR goes straight to UUnrealEdEngine::CloseEditor and can bypass
	// enough tab teardown to trip BlueprintEditor PreviewScene assertions.
	if (!bSaveAll)
	{
		const int32 DiscardedPackageCount = FBlueprintHelperEditorCommandService::DiscardAllDirtyPackagesForClose();
		if (DiscardedPackageCount > 0)
		{
			UE_LOG(LogTemp, Display, TEXT("[BlueprintHelper] CloseEditor: discarded %d dirty package(s) before no-save shutdown."), DiscardedPackageCount);
		}
	}

	const FString CloseRequestId = TEXT("close_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FBlueprintHelperEditorModalDismissResult ModalDismissResult =
		FBlueprintHelperEditorCloseModalCoordinator::Get().DismissActiveModalWindowsFromGameThread(CloseRequestId);
	if (ModalDismissResult.DismissedCount > 0 || ModalDismissResult.bHadActiveModal || ModalDismissResult.bHasRemainingModal)
	{
		if (!Result.Data.IsValid())
		{
			Result.Data = MakeShared<FJsonObject>();
		}
		Result.Data->SetObjectField(TEXT("modal_dialogs"), BlueprintHelperBuildModalDismissJson(CloseRequestId, ModalDismissResult));
	}
	if (ModalDismissResult.bHasRemainingModal)
	{
		Result.ErrorMessage = TEXT("Editor shutdown was not requested because a modal dialog is still active.");
		Result.Message = TEXT("Call blueprint_dismiss_editor_dialogs while Bridge is responsive, call blueprint_close_editor_dialogs if the modal blocks Bridge, or close the modal dialog manually, then retry blueprint_close_editor.");
		return Result;
	}

	if (GEngine)
	{
		GEngine->DeferredCommands.AddUnique(TEXT("CLOSE_SLATE_MAINFRAME"));
	}

	Result.bSuccess = true;
	Result.Message = bSaveAll
		? TEXT("Saved dirty packages and requested editor shutdown.")
		: TEXT("Requested editor shutdown and discarded dirty packages without saving.");
	return Result;
}

int32 FBlueprintHelperEditorCommandService::DiscardDirtyPackages(TConstArrayView<UPackage*> Packages)
{
	int32 DiscardedPackageCount = 0;
	for (UPackage* Package : Packages)
	{
		if (!IsValid(Package) || !Package->IsDirty())
		{
			continue;
		}

		Package->SetDirtyFlag(false);
		++DiscardedPackageCount;
	}

	return DiscardedPackageCount;
}

int32 FBlueprintHelperEditorCommandService::DiscardAllDirtyPackagesForClose()
{
	TArray<UPackage*> DirtyPackages;
	FEditorFileUtils::GetDirtyPackages(DirtyPackages);
	return DiscardDirtyPackages(DirtyPackages);
}
