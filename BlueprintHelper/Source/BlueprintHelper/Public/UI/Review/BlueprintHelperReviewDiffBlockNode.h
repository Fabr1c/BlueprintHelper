// Transient graph-space Review diff block node.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "Input/Reply.h"
#include "Templates/Function.h"

#include "BlueprintHelperReviewDiffBlockNode.generated.h"

class SGraphNode;

UCLASS()
class UBlueprintHelperReviewDiffBlockNode : public UEdGraphNode
{
	GENERATED_BODY()

public:
	FString ChangeId;
	FString DisplayLabel;
	FLinearColor DiffColor = FLinearColor::Transparent;
	bool bHighlighted = false;
	TFunction<FReply(const FString&)> OnAccept;
	TFunction<FReply(const FString&)> OnReject;

	void Configure(
		const FString& InChangeId,
		const FString& InDisplayLabel,
		const FLinearColor& InDiffColor,
		bool bInHighlighted,
		TFunction<FReply(const FString&)> InOnAccept,
		TFunction<FReply(const FString&)> InOnReject);

	virtual void AllocateDefaultPins() override {}
	virtual bool CanUserDeleteNode() const override { return false; }
	virtual bool CanDuplicateNode() const override { return false; }
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;
};
