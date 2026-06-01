#include "Systems/Debug/BlueprintHelperScreenshotCaptureService.h"

#include "BlueprintEditorModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "GraphEditor.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "SGraphPanel.h"
#include "Systems/Debug/BlueprintHelperDebugCaseStoreService.h"
#include "Systems/Debug/BlueprintHelperScreenshotSettings.h"
#include "UnrealClient.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"

struct FBlueprintHelperFocusedGraphScreenshotContext
{
	TSharedPtr<IBlueprintEditor> BlueprintEditor;
	TSharedPtr<SGraphEditor> GraphEditor;
	SGraphPanel* GraphPanel = nullptr;
	UEdGraph* Graph = nullptr;
};

class FBlueprintHelperGraphScreenshotViewRestore
{
public:
	explicit FBlueprintHelperGraphScreenshotViewRestore(const TSharedPtr<SGraphEditor>& InGraphEditor)
		: GraphEditor(InGraphEditor)
	{
		if (!GraphEditor.IsValid())
		{
			return;
		}
		OriginalSelection = GraphEditor->GetSelectedNodes();
		GraphEditor->GetViewLocation(OriginalLocation, OriginalZoomAmount);
		GraphEditor->GetViewBookmark(OriginalBookmarkId);
		bHasState = true;
	}

	~FBlueprintHelperGraphScreenshotViewRestore()
	{
		Restore();
	}

	void Restore()
	{
		if (!bHasState || !GraphEditor.IsValid())
		{
			return;
		}

		GraphEditor->ClearSelectionSet();
		for (UObject* Object : OriginalSelection)
		{
			if (UEdGraphNode* Node = Cast<UEdGraphNode>(Object))
			{
				GraphEditor->SetNodeSelection(Node, true);
			}
		}
		GraphEditor->SetViewLocation(OriginalLocation, OriginalZoomAmount, OriginalBookmarkId);
		bHasState = false;
	}

private:
	TSharedPtr<SGraphEditor> GraphEditor;
	FGraphPanelSelectionSet OriginalSelection;
	FVector2f OriginalLocation = FVector2f::ZeroVector;
	float OriginalZoomAmount = 1.0f;
	FGuid OriginalBookmarkId;
	bool bHasState = false;
};

class FBlueprintHelperScreenshotCaptureServiceLocalUtils
{
public:
	static FBlueprintHelperScreenshotCaptureResult Failure(
		const FString& Code,
		const FString& Message,
		EBlueprintHelperScreenshotTarget Target)
	{
		FBlueprintHelperScreenshotCaptureResult Result;
		Result.bSuccess = false;
		Result.ErrorCode = Code;
		Result.Message = Message;
		Result.Target = BlueprintHelperScreenshotTargetToString(Target);
		Result.CreatedAtUtc = FDateTime::UtcNow().ToIso8601();
		return Result;
	}

	static FString BuildTimestamp()
	{
		return FString::Printf(
			TEXT("%s_%s"),
			*FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),
			*FGuid::NewGuid().ToString(EGuidFormats::Short));
	}

	static FString MakeRelativePath(const FString& AbsolutePath)
	{
		FString RelativePath = AbsolutePath;
		const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		FPaths::MakePathRelativeTo(RelativePath, *ProjectDir);
		FPaths::NormalizeFilename(RelativePath);
		return RelativePath;
	}

	static FString SanitizeLabelText(const FString& Source)
	{
		FString Sanitized;
		for (const TCHAR Ch : Source)
		{
			if (FChar::IsAlnum(Ch) || Ch == TEXT('_') || Ch == TEXT('-'))
			{
				Sanitized.AppendChar(Ch);
			}
		}
		return Sanitized;
	}

	static FBlueprintHelperGraphScreenshotCaptureResult GraphFailure(
		const FString& Code,
		const FString& Message)
	{
		FBlueprintHelperGraphScreenshotCaptureResult Result;
		Result.bSuccess = false;
		Result.ErrorCode = Code;
		Result.Message = Message;
		return Result;
	}

	static bool TryFindFocusedGraphEditor(
		UEdGraph* PreferredGraph,
		FBlueprintHelperFocusedGraphScreenshotContext& OutContext)
	{
		OutContext = FBlueprintHelperFocusedGraphScreenshotContext();
		if (!FModuleManager::Get().IsModuleLoaded(TEXT("Kismet")))
		{
			return false;
		}

		FBlueprintEditorModule& BlueprintEditorModule =
			FModuleManager::LoadModuleChecked<FBlueprintEditorModule>(TEXT("Kismet"));
		for (const TSharedRef<IBlueprintEditor>& BlueprintEditor : BlueprintEditorModule.GetBlueprintEditors())
		{
			UEdGraph* FocusedGraph = PreferredGraph ? PreferredGraph : BlueprintEditor->GetFocusedGraph();
			if (!FocusedGraph)
			{
				continue;
			}

			TSharedPtr<SGraphEditor> GraphEditor = BlueprintEditor->OpenGraphAndBringToFront(FocusedGraph, true);
			if (!GraphEditor.IsValid())
			{
				continue;
			}

			SGraphPanel* GraphPanel = GraphEditor->GetGraphPanel();
			if (!GraphPanel)
			{
				continue;
			}

			OutContext.BlueprintEditor = BlueprintEditor;
			OutContext.GraphEditor = GraphEditor;
			OutContext.GraphPanel = GraphPanel;
			OutContext.Graph = FocusedGraph;
			return true;
		}
		return false;
	}

	static void CollectSelectedGraphNodes(
		const TSharedPtr<SGraphEditor>& GraphEditor,
		UEdGraph* Graph,
		const TArray<UEdGraphNode*>& PreferredNodes,
		TArray<UEdGraphNode*>& OutNodes)
	{
		OutNodes.Reset();
		for (UEdGraphNode* Node : PreferredNodes)
		{
			if (Node && (!Graph || Node->GetGraph() == Graph) && !OutNodes.Contains(Node))
			{
				OutNodes.Add(Node);
			}
		}
		if (OutNodes.Num() > 0)
		{
			return;
		}

		if (!GraphEditor.IsValid())
		{
			return;
		}

		for (UObject* Object : GraphEditor->GetSelectedNodes())
		{
			UEdGraphNode* Node = Cast<UEdGraphNode>(Object);
			if (Node && (!Graph || Node->GetGraph() == Graph) && !OutNodes.Contains(Node))
			{
				OutNodes.Add(Node);
			}
		}

		if (OutNodes.Num() == 0 && Graph)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node && !OutNodes.Contains(Node))
				{
					OutNodes.Add(Node);
				}
			}
		}
	}

	static void SelectNodes(
		const TSharedPtr<SGraphEditor>& GraphEditor,
		const TArray<UEdGraphNode*>& Nodes)
	{
		if (!GraphEditor.IsValid())
		{
			return;
		}
		GraphEditor->ClearSelectionSet();
		for (UEdGraphNode* Node : Nodes)
		{
			if (Node)
			{
				GraphEditor->SetNodeSelection(Node, true);
			}
		}
		GraphEditor->ZoomToFit(true);
	}

	static void PumpSlateForGraphWidget(const TSharedRef<SWidget>& Widget)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return;
		}

		FSlateApplication& SlateApp = FSlateApplication::Get();
		TSharedPtr<SWindow> Window = SlateApp.FindWidgetWindow(Widget);
		for (int32 TickIndex = 0; TickIndex < 4; ++TickIndex)
		{
			SlateApp.Tick(ESlateTickType::All);
			if (Window.IsValid())
			{
				SlateApp.ForceRedrawWindow(Window.ToSharedRef());
			}
		}
	}
};

FBlueprintHelperScreenshotCaptureResult FBlueprintHelperScreenshotCaptureService::Capture(
	const FBlueprintHelperScreenshotCaptureRequest& Request) const
{
	switch (Request.Target)
	{
	case EBlueprintHelperScreenshotTarget::ActiveViewport:
		return CaptureActiveViewport(Request);
	case EBlueprintHelperScreenshotTarget::ActiveWindow:
	default:
		return CaptureActiveWindow(Request);
	}
}

FBlueprintHelperGraphScreenshotCaptureResult FBlueprintHelperScreenshotCaptureService::CaptureFocusedGraph(
	const FBlueprintHelperGraphScreenshotCaptureRequest& Request) const
{
	if (!FSlateApplication::IsInitialized())
	{
		return FBlueprintHelperScreenshotCaptureServiceLocalUtils::GraphFailure(
			TEXT("slate_not_initialized"),
			TEXT("Slate application is not initialized."));
	}

	FBlueprintHelperFocusedGraphScreenshotContext Context;
	if (!FBlueprintHelperScreenshotCaptureServiceLocalUtils::TryFindFocusedGraphEditor(Request.Graph, Context))
	{
		return FBlueprintHelperScreenshotCaptureServiceLocalUtils::GraphFailure(
			TEXT("focused_graph_editor_not_found"),
			TEXT("No focused Blueprint graph editor is available for graph screenshot capture."));
	}

	TArray<UEdGraphNode*> SelectedNodes;
	FBlueprintHelperScreenshotCaptureServiceLocalUtils::CollectSelectedGraphNodes(
		Context.GraphEditor,
		Context.Graph,
		Request.Nodes,
		SelectedNodes);
	if (SelectedNodes.Num() == 0)
	{
		return FBlueprintHelperScreenshotCaptureServiceLocalUtils::GraphFailure(
			TEXT("graph_nodes_not_found"),
			TEXT("The focused Graph has no nodes available for screenshot capture."));
	}

	FBlueprintHelperGraphScreenshotCaptureResult Result;
	Result.bSuccess = true;
	Result.GraphName = Context.Graph ? Context.Graph->GetName() : FString();
	Result.SelectedNodeCount = SelectedNodes.Num();

	FBlueprintHelperGraphScreenshotViewRestore Restore(Context.GraphEditor);
	const int32 MaxNodesPerImage = FMath::Clamp(Request.MaxNodesPerImage, 1, 64);
	const int32 TileCount = FMath::Max(1, FMath::DivideAndRoundUp(SelectedNodes.Num(), MaxNodesPerImage));
	const FString BaseLabel = SanitizeFileLabel(Request.Label, TEXT("graph"));

	TSharedRef<SWidget> GraphWidget = StaticCastSharedRef<SWidget>(Context.GraphEditor.ToSharedRef());
	if (Context.GraphPanel)
	{
		GraphWidget = StaticCastSharedRef<SWidget>(Context.GraphPanel->AsShared());
	}
	for (int32 TileIndex = 0; TileIndex < TileCount; ++TileIndex)
	{
		const int32 StartIndex = TileIndex * MaxNodesPerImage;
		const int32 EndIndex = FMath::Min(StartIndex + MaxNodesPerImage, SelectedNodes.Num());
		TArray<UEdGraphNode*> TileNodes;
		for (int32 NodeIndex = StartIndex; NodeIndex < EndIndex; ++NodeIndex)
		{
			TileNodes.Add(SelectedNodes[NodeIndex]);
		}

		FBlueprintHelperScreenshotCaptureServiceLocalUtils::SelectNodes(Context.GraphEditor, TileNodes);
		FBlueprintHelperScreenshotCaptureServiceLocalUtils::PumpSlateForGraphWidget(GraphWidget);

		TArray<FColor> Pixels;
		FIntVector Size(0, 0, 0);
		if (!FSlateApplication::Get().TakeScreenshot(GraphWidget, Pixels, Size) ||
			Pixels.Num() == 0 || Size.X <= 0 || Size.Y <= 0)
		{
			return FBlueprintHelperScreenshotCaptureServiceLocalUtils::GraphFailure(
				TEXT("graph_widget_screenshot_failed"),
				TEXT("Graph widget screenshot failed."));
		}

		FBlueprintHelperScreenshotCaptureRequest TileRequest;
		TileRequest.Target = EBlueprintHelperScreenshotTarget::GraphPanel;
		TileRequest.Label = FString::Printf(TEXT("%s_%03d"), *BaseLabel, TileIndex + 1);
		TileRequest.TileIndex = TileIndex;
		TileRequest.TileCount = TileCount;
		TileRequest.NodeCount = TileNodes.Num();
		TileRequest.ViewLabel = FString::Printf(TEXT("%s tile %d/%d"), *Result.GraphName, TileIndex + 1, TileCount);

		FBlueprintHelperScreenshotCaptureResult TileResult =
			SavePixels(TileRequest, Pixels, Size.X, Size.Y);
		if (!TileResult.bSuccess)
		{
			return FBlueprintHelperScreenshotCaptureServiceLocalUtils::GraphFailure(
				TileResult.ErrorCode,
				TileResult.Message);
		}
		Result.Screenshots.Add(TileResult);
	}

	Result.ScreenshotCount = Result.Screenshots.Num();
	Result.Message = FString::Printf(
		TEXT("Captured %d graph screenshot PNG(s)."),
		Result.ScreenshotCount);
	return Result;
}

FString FBlueprintHelperScreenshotCaptureService::BuildOutputDirectory()
{
	const FBlueprintHelperScreenshotSettings Settings = FBlueprintHelperScreenshotSettings::Load();
	FString Directory = FBlueprintHelperDebugCaseStoreService::GetDebugRootDir() / Settings.OutputDir;
	FPaths::NormalizeDirectoryName(Directory);
	FPaths::CollapseRelativeDirectories(Directory);
	return Directory;
}

FString FBlueprintHelperScreenshotCaptureService::SanitizeFileLabel(
	const FString& Label,
	const FString& Fallback)
{
	FString Sanitized = FBlueprintHelperScreenshotCaptureServiceLocalUtils::SanitizeLabelText(Label);
	if (Sanitized.IsEmpty())
	{
		Sanitized = FBlueprintHelperScreenshotCaptureServiceLocalUtils::SanitizeLabelText(Fallback);
	}
	if (Sanitized.IsEmpty())
	{
		Sanitized = TEXT("editor");
	}
	return Sanitized.Left(80);
}

FBlueprintHelperScreenshotCaptureResult FBlueprintHelperScreenshotCaptureService::CaptureActiveWindow(
	const FBlueprintHelperScreenshotCaptureRequest& Request) const
{
	if (!FSlateApplication::IsInitialized())
	{
		return FBlueprintHelperScreenshotCaptureServiceLocalUtils::Failure(
			TEXT("slate_not_initialized"),
			TEXT("Slate application is not initialized."),
			Request.Target);
	}

	FSlateApplication& SlateApp = FSlateApplication::Get();
	TSharedPtr<SWindow> Window = SlateApp.GetActiveTopLevelWindow();
	if (!Window.IsValid())
	{
		TArray<TSharedRef<SWindow>> VisibleWindows;
		SlateApp.GetAllVisibleWindowsOrdered(VisibleWindows);
		if (VisibleWindows.Num() > 0)
		{
			Window = VisibleWindows.Last();
		}
	}
	if (!Window.IsValid())
	{
		return FBlueprintHelperScreenshotCaptureServiceLocalUtils::Failure(
			TEXT("active_window_not_found"),
			TEXT("No active Slate window is available for screenshot capture."),
			Request.Target);
	}

	TArray<FColor> Pixels;
	FIntVector Size(0, 0, 0);
	if (!SlateApp.TakeScreenshot(Window.ToSharedRef(), Pixels, Size) || Pixels.Num() == 0 || Size.X <= 0 || Size.Y <= 0)
	{
		return FBlueprintHelperScreenshotCaptureServiceLocalUtils::Failure(
			TEXT("slate_screenshot_failed"),
			TEXT("Slate active window screenshot failed."),
			Request.Target);
	}

	return SavePixels(Request, Pixels, Size.X, Size.Y);
}

FBlueprintHelperScreenshotCaptureResult FBlueprintHelperScreenshotCaptureService::CaptureActiveViewport(
	const FBlueprintHelperScreenshotCaptureRequest& Request) const
{
	if (!GEditor)
	{
		return FBlueprintHelperScreenshotCaptureServiceLocalUtils::Failure(
			TEXT("editor_not_available"),
			TEXT("GEditor is not available."),
			Request.Target);
	}

	FViewport* Viewport = GEditor->GetActiveViewport();
	if (!Viewport)
	{
		return FBlueprintHelperScreenshotCaptureServiceLocalUtils::Failure(
			TEXT("active_viewport_not_found"),
			TEXT("No active editor viewport is available for screenshot capture."),
			Request.Target);
	}

	const FIntPoint Size = Viewport->GetSizeXY();
	if (Size.X <= 0 || Size.Y <= 0)
	{
		return FBlueprintHelperScreenshotCaptureServiceLocalUtils::Failure(
			TEXT("active_viewport_empty"),
			TEXT("The active editor viewport has an empty size."),
			Request.Target);
	}

	TArray<FColor> Pixels;
	if (!Viewport->ReadPixels(Pixels) || Pixels.Num() == 0)
	{
		return FBlueprintHelperScreenshotCaptureServiceLocalUtils::Failure(
			TEXT("viewport_read_pixels_failed"),
			TEXT("Reading pixels from the active editor viewport failed."),
			Request.Target);
	}

	return SavePixels(Request, Pixels, Size.X, Size.Y);
}

FBlueprintHelperScreenshotCaptureResult FBlueprintHelperScreenshotCaptureService::SavePixels(
	const FBlueprintHelperScreenshotCaptureRequest& Request,
	const TArray<FColor>& Pixels,
	int32 Width,
	int32 Height) const
{
	if (Pixels.Num() == 0 || Width <= 0 || Height <= 0)
	{
		return FBlueprintHelperScreenshotCaptureServiceLocalUtils::Failure(
			TEXT("empty_screenshot"),
			TEXT("Screenshot pixel buffer is empty."),
			Request.Target);
	}

	const FBlueprintHelperScreenshotSettings Settings = FBlueprintHelperScreenshotSettings::Load();
	const FString Label = SanitizeFileLabel(Request.Label, Settings.FilenamePrefix);
	const FString OutputDir = BuildOutputDirectory();
	if (!IFileManager::Get().DirectoryExists(*OutputDir) &&
		!IFileManager::Get().MakeDirectory(*OutputDir, true))
	{
		return FBlueprintHelperScreenshotCaptureServiceLocalUtils::Failure(
			TEXT("output_dir_create_failed"),
			FString::Printf(TEXT("Failed to create screenshot output directory: %s"), *OutputDir),
			Request.Target);
	}

	const FString FileName = FString::Printf(
		TEXT("%s_%s.png"),
		*Label,
		*FBlueprintHelperScreenshotCaptureServiceLocalUtils::BuildTimestamp());
	const FString AbsolutePath = OutputDir / FileName;

	TArray64<uint8> PngBytes;
	FImageUtils::PNGCompressImageArray(
		Width,
		Height,
		TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()),
		PngBytes);
	if (PngBytes.Num() == 0 || !FFileHelper::SaveArrayToFile(PngBytes, *AbsolutePath))
	{
		return FBlueprintHelperScreenshotCaptureServiceLocalUtils::Failure(
			TEXT("png_write_failed"),
			FString::Printf(TEXT("Failed to write screenshot PNG: %s"), *AbsolutePath),
			Request.Target);
	}

	FBlueprintHelperScreenshotCaptureResult Result;
	Result.bSuccess = true;
	Result.Message = TEXT("Screenshot captured.");
	Result.AbsolutePath = AbsolutePath;
	Result.RelativePath = FBlueprintHelperScreenshotCaptureServiceLocalUtils::MakeRelativePath(AbsolutePath);
	Result.Width = Width;
	Result.Height = Height;
	Result.Target = BlueprintHelperScreenshotTargetToString(Request.Target);
	Result.TileIndex = Request.TileIndex;
	Result.TileCount = Request.TileCount;
	Result.NodeCount = Request.NodeCount;
	Result.ViewLabel = Request.ViewLabel;
	Result.CreatedAtUtc = FDateTime::UtcNow().ToIso8601();
	return Result;
}
