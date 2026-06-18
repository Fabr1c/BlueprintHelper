// BlueprintHelper Review Material presenter.

#include "UI/Review/BlueprintHelperReviewMaterialPresenter.h"

#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialExpression.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialGraphOwnershipService.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UI/Review/BlueprintHelperReviewPresenterWidgetUtils.h"
#include "UI/Review/BlueprintHelperReviewRowHighlightModel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Views/SListView.h"

class FBlueprintHelperReviewMaterialPresenterPrivate
{
public:
	static FString MakeReviewKeySegment(const FString& Value)
	{
		FString Result = Value;
		Result.TrimStartAndEndInline();
		if (Result.IsEmpty())
		{
			return TEXT("unknown");
		}

		for (TCHAR& Character : Result)
		{
			if (!FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('-') && Character != TEXT('.'))
			{
				Character = TEXT('_');
			}
		}
		return Result;
	}

	static FString ReadOwnershipMetadata(const UMaterialExpression* Expression, const TCHAR* Key)
	{
		if (!Expression || !Key)
		{
			return FString();
		}
#if WITH_METADATA
		if (UPackage* Package = Expression->GetPackage())
		{
			return FBlueprintHelperVersionCompat::GetPackageMetaData(Package).GetValue(Expression, Key);
		}
#endif
		return FString();
	}

	static FString GetExpressionNodeKey(const UMaterialExpression* Expression)
	{
		const FString NodeKey = ReadOwnershipMetadata(
			Expression,
			FBlueprintHelperMaterialGraphOwnershipService::NodeKeyMetadataKey());
		if (!NodeKey.IsEmpty())
		{
			return NodeKey;
		}
		return Expression
			? Expression->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphensLower)
			: FString();
	}

	static FString GetExpressionBlockId(const UMaterialExpression* Expression)
	{
		return ReadOwnershipMetadata(
			Expression,
			FBlueprintHelperMaterialGraphOwnershipService::BlockIdMetadataKey());
	}

	static FString GetExpressionClassName(const UMaterialExpression* Expression)
	{
		return Expression && Expression->GetClass()
			? Expression->GetClass()->GetName()
			: FString(TEXT("<unknown>"));
	}

	static FString GetExpressionOutputPinName(UMaterialExpression* Expression, int32 OutputIndex)
	{
		if (!Expression)
		{
			return FString();
		}
		const TArray<FExpressionOutput>& Outputs = Expression->GetOutputs();
		if (!Outputs.IsValidIndex(OutputIndex))
		{
			return FString();
		}
		const FName OutputName = Outputs[OutputIndex].OutputName;
		return OutputName.IsNone() ? FString(TEXT("Output")) : OutputName.ToString();
	}

	static TSharedRef<FBlueprintHelperReviewDataAssetRowItem> MakeRow(
		const FString& Label,
		const FString& Value,
		const FString& SearchText,
		int32 Depth,
		bool bIsSection = false)
	{
		TSharedRef<FBlueprintHelperReviewDataAssetRowItem> Row = MakeShared<FBlueprintHelperReviewDataAssetRowItem>();
		Row->Label = Label;
		Row->Value = Value;
		Row->SearchText = SearchText;
		Row->Depth = Depth;
		Row->bIsSection = bIsSection;
		return Row;
	}

	static void AddAlias(
		const TSharedRef<FBlueprintHelperReviewDataAssetRowItem>& Row,
		const FString& Alias)
	{
		if (!Alias.IsEmpty())
		{
			Row->SearchAliases.AddUnique(Alias);
		}
	}

	static void AddMaterialSummaryRow(
		const FBlueprintHelperReviewAssetContext& Context,
		FBlueprintHelperReviewMaterialPresenterState& State,
		const UMaterial* Material)
	{
		const FString AssetName = FBlueprintHelperReviewPresenterWidgetUtils::GetAssetShortName(Context.AssetPath);
		TSharedRef<FBlueprintHelperReviewDataAssetRowItem> Row = MakeRow(
			FString::Printf(TEXT("Material: %s"), *AssetName),
			Material && Material->GetClass() ? Material->GetClass()->GetName() : FString(TEXT("<unknown>")),
			FString::Printf(TEXT("asset_factory:material material %s %s"), *AssetName, *Context.AssetPath),
			0,
			true);
		AddAlias(Row, TEXT("material"));
		AddAlias(Row, AssetName);
		AddAlias(Row, Context.AssetPath);
		State.Rows.Add(Row);
	}

	static void AddExpressionRows(
		UMaterial* Material,
		FBlueprintHelperReviewMaterialPresenterState& State)
	{
		if (!Material)
		{
			return;
		}

		for (UMaterialExpression* Expression : Material->GetExpressions())
		{
			if (!Expression)
			{
				continue;
			}

			const FString NodeKey = GetExpressionNodeKey(Expression);
			const FString BlockId = GetExpressionBlockId(Expression);
			const FString ClassName = GetExpressionClassName(Expression);
			const FString Description = Expression->Desc;
			const FString ExpressionTargetKey = FString::Printf(
				TEXT("material_expression:%s"),
				*MakeReviewKeySegment(NodeKey));
			TSharedRef<FBlueprintHelperReviewDataAssetRowItem> Row = MakeRow(
				NodeKey.IsEmpty() ? ClassName : NodeKey,
				Description.IsEmpty() ? ClassName : FString::Printf(TEXT("%s  %s"), *ClassName, *Description),
				ExpressionTargetKey,
				1);
			AddAlias(Row, NodeKey);
			AddAlias(Row, BlockId);
			AddAlias(Row, ClassName);
			AddAlias(Row, Description);
			AddAlias(Row, Expression->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
			State.Rows.Add(Row);
		}
	}

	static void AddExpressionInputLinkRows(
		UMaterial* Material,
		FBlueprintHelperReviewMaterialPresenterState& State)
	{
		if (!Material)
		{
			return;
		}

		for (UMaterialExpression* Expression : Material->GetExpressions())
		{
			if (!Expression)
			{
				continue;
			}

			const FString ToNodeKey = GetExpressionNodeKey(Expression);
			for (int32 InputIndex = 0; InputIndex < FBlueprintHelperVersionCompat::CountMaterialExpressionInputs(Expression); ++InputIndex)
			{
				const FExpressionInput* Input = Expression->GetInput(InputIndex);
				if (!Input || !Input->Expression)
				{
					continue;
				}

				const FString FromNodeKey = GetExpressionNodeKey(Input->Expression);
				const FString FromPin = GetExpressionOutputPinName(Input->Expression, Input->OutputIndex);
				const FString ToPin = Expression->GetInputName(InputIndex).ToString();
				const FString TargetKey = FString::Printf(
					TEXT("material_link:%s:%s:%s:%s"),
					*MakeReviewKeySegment(FromNodeKey),
					*MakeReviewKeySegment(FromPin),
					*MakeReviewKeySegment(ToNodeKey),
					*MakeReviewKeySegment(ToPin));
				TSharedRef<FBlueprintHelperReviewDataAssetRowItem> Row = MakeRow(
					FString::Printf(TEXT("Input: %s.%s"), *ToNodeKey, *ToPin),
					FString::Printf(TEXT("%s.%s"), *FromNodeKey, *FromPin),
					TargetKey,
					2);
				AddAlias(Row, FromNodeKey);
				AddAlias(Row, FromPin);
				AddAlias(Row, ToNodeKey);
				AddAlias(Row, ToPin);
				State.Rows.Add(Row);
			}
		}
	}

	static void AddMaterialOutputLinkRows(
		UMaterial* Material,
		FBlueprintHelperReviewMaterialPresenterState& State)
	{
		if (!Material)
		{
			return;
		}

		const EMaterialProperty MaterialProperties[] =
		{
			MP_BaseColor,
			MP_Metallic,
			MP_Specular,
			MP_Roughness,
			MP_EmissiveColor,
			MP_Opacity,
			MP_OpacityMask,
			MP_Normal,
			MP_WorldPositionOffset
		};

		for (EMaterialProperty MaterialProperty : MaterialProperties)
		{
			UMaterialExpression* Expression = UMaterialEditingLibrary::GetMaterialPropertyInputNode(
				Material,
				MaterialProperty);
			if (!Expression)
			{
				continue;
			}

			const FString FromNodeKey = GetExpressionNodeKey(Expression);
			const FString FromPin = UMaterialEditingLibrary::GetMaterialPropertyInputNodeOutputName(
				Material,
				MaterialProperty);
			const FString ToPin = FMaterialAttributeDefinitionMap::GetAttributeName(MaterialProperty);
			const FString TargetKey = FString::Printf(
				TEXT("material_link:%s:%s:%s:%s"),
				*MakeReviewKeySegment(FromNodeKey),
				*MakeReviewKeySegment(FromPin),
				*MakeReviewKeySegment(TEXT("$material_output")),
				*MakeReviewKeySegment(ToPin));
			TSharedRef<FBlueprintHelperReviewDataAssetRowItem> Row = MakeRow(
				FString::Printf(TEXT("Output: %s"), *ToPin),
				FString::Printf(TEXT("%s.%s"), *FromNodeKey, *FromPin),
				TargetKey,
				1);
			AddAlias(Row, FromNodeKey);
			AddAlias(Row, FromPin);
			AddAlias(Row, ToPin);
			AddAlias(Row, TEXT("$material_output"));
			State.Rows.Add(Row);
		}
	}
};

bool FBlueprintHelperReviewMaterialPresenter::ShouldShowChange(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	return FBlueprintHelperReviewPresenterWidgetUtils::ShouldShowIndependentSurfaceChange(
		Change,
		EBlueprintHelperReviewSurface::Material,
		{
			TEXT("material_expression"),
			TEXT("material_expression_link"),
			TEXT("material_output_link"),
			TEXT("material_instance"),
			TEXT("material_instance_parameter"),
			TEXT("asset_factory")
		});
}

TSharedRef<SWidget> FBlueprintHelperReviewMaterialPresenter::BuildContent(
	const FBlueprintHelperReviewAssetContext& Context,
	FBlueprintHelperReviewMaterialPresenterState& State,
	FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated)
{
	State.Rows.Reset();
	State.ListView.Reset();

	UMaterial* Material = Context.Material.Get();
	if (Material)
	{
		FBlueprintHelperReviewMaterialPresenterPrivate::AddMaterialSummaryRow(Context, State, Material);
		FBlueprintHelperReviewMaterialPresenterPrivate::AddExpressionRows(Material, State);
		FBlueprintHelperReviewMaterialPresenterPrivate::AddExpressionInputLinkRows(Material, State);
		FBlueprintHelperReviewMaterialPresenterPrivate::AddMaterialOutputLinkRows(Material, State);
	}

	if (State.Rows.Num() == 0)
	{
		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("Asset: %s"), *Context.AssetPath));
		Lines.Add(FString::Printf(TEXT("Kind: %s"), BlueprintHelperReviewAssetKindToString(Context.AssetKind)));
		Lines.Add(TEXT("Material rows: unavailable"));
		return FBlueprintHelperReviewPresenterWidgetUtils::BuildSummaryPanel(
			TEXT("Material Graph"),
			Lines,
			Context.AssetPath,
			EBlueprintHelperReviewSurface::Material,
			OnGeometryInvalidated);
	}

	const FString AssetPath = Context.AssetPath;
	TSharedRef<SListView<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>>> ListView =
		SAssignNew(State.ListView, SListView<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>>)
		.ListItemsSource(&State.Rows)
		.SelectionMode(ESelectionMode::None)
		.OnGenerateRow_Lambda([AssetPath, OnGeometryInvalidated](
			TSharedPtr<FBlueprintHelperReviewDataAssetRowItem> Item,
			const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow>
		{
			return FBlueprintHelperReviewPresenterWidgetUtils::GenerateDataAssetRow(
				Item,
				OwnerTable,
				AssetPath,
				OnGeometryInvalidated,
				EBlueprintHelperReviewSurface::Material);
		});

	return SNew(SBorder)
		.Padding(8.0f)
		[
			ListView
		];
}

TSharedRef<SWidget> FBlueprintHelperReviewMaterialPresenter::BuildOverlay(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
{
	return FBlueprintHelperReviewRowHighlightModel::BuildRowHighlightOverlay(
		Args,
		EBlueprintHelperReviewSurface::Material,
		&FBlueprintHelperReviewMaterialPresenter::ShouldShowChange);
}
