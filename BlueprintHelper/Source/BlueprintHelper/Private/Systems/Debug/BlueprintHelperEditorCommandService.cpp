// BlueprintHelper editor command service.

#include "Systems/Debug/BlueprintHelperEditorCommandService.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "FileHelpers.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/StringFormatArg.h"
#include "PlayInEditorDataTypes.h"
#include "UObject/UObjectIterator.h"

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
		const bool bSaved = FEditorFileUtils::SaveDirtyPackages(
			/*bPromptUserToSave=*/ false,
			/*bSaveMapPackages=*/ true,
			/*bSaveContentPackages=*/ true);
		if (!bSaved)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BlueprintHelper] CloseEditor: some dirty packages failed to save; shutdown will still be requested."));
		}
	}

	// Do not call CloseAllAssetEditors here. Some Blueprint/asset editor teardown
	// paths can assert while the Bridge request is still unwinding. Save first,
	// then schedule a single editor quit command so the caller can receive a
	// response before the Bridge connection drops.
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
	{
		if (GEngine)
		{
			GEngine->DeferredCommands.Add(TEXT("QUIT_EDITOR"));
		}
		return false;
	}), 0.25f);

	Result.bSuccess = true;
	Result.Message = bSaveAll
		? TEXT("Saved dirty packages and scheduled delayed editor shutdown.")
		: TEXT("Scheduled delayed editor shutdown without saving.");
	return Result;
}