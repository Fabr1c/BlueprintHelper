// UE 5.3 fallback definitions for non-exported UEdGraphNode_Comment members.

#include "Runtime/Launch/Resources/Version.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 6

#include "EdGraphNode_Comment.h"
#include "UObject/UnrealType.h"

UEdGraphNode_Comment::UEdGraphNode_Comment(const FObjectInitializer& ObjectInitializer)
	: UEdGraphNode(ObjectInitializer)
{
	NodeWidth = 400;
	NodeHeight = 100;
	FontSize = 18;
	CommentColor = FLinearColor::White;
	bColorCommentBubble = false;
	MoveMode = ECommentBoxMode::GroupMovement;

	bCommentBubblePinned = true;
	bCommentBubbleVisible = true;
	bCommentBubbleVisible_InDetailsPanel = true;
	bCanResizeNode = true;
	bCanRenameNode = true;
	CommentDepth = -1;
}

void UEdGraphNode_Comment::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UEdGraphNode_Comment, bCommentBubbleVisible_InDetailsPanel))
	{
		bCommentBubbleVisible = bCommentBubbleVisible_InDetailsPanel;
		bCommentBubblePinned = bCommentBubbleVisible_InDetailsPanel;
	}

	UEdGraphNode::PostEditChangeProperty(PropertyChangedEvent);
}

bool UEdGraphNode_Comment::IsSelectedInEditor() const
{
	return UEdGraphNode::IsSelectedInEditor();
}

#endif
