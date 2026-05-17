// BlueprintHelper Review baseline snapshot service utilities implementation.

#include "Systems/Review/Utils/BlueprintHelperReviewBaselineSnapshotServiceUtils.h"

#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UObjectBaseUtility.h"

namespace
{
	static bool ShouldOmitCanonicalReviewSnapshotField(const FString& Key)
	{
		return Key.Equals(TEXT("captured_at"), ESearchCase::CaseSensitive)
			|| Key.Equals(TEXT("warnings"), ESearchCase::CaseSensitive)
			|| Key.Equals(TEXT("debug"), ESearchCase::CaseSensitive)
			|| Key.Equals(TEXT("debug_only"), ESearchCase::CaseSensitive);
	}

	static void AppendCanonicalJsonString(const FString& Value, FString& Out)
	{
		Out.AppendChar(TEXT('"'));
		for (const TCHAR Ch : Value)
		{
			switch (Ch)
			{
			case TEXT('"'):
				Out += TEXT("\\\"");
				break;
			case TEXT('\\'):
				Out += TEXT("\\\\");
				break;
			case TEXT('\b'):
				Out += TEXT("\\b");
				break;
			case TEXT('\f'):
				Out += TEXT("\\f");
				break;
			case TEXT('\n'):
				Out += TEXT("\\n");
				break;
			case TEXT('\r'):
				Out += TEXT("\\r");
				break;
			case TEXT('\t'):
				Out += TEXT("\\t");
				break;
			default:
				if (Ch < 0x20)
				{
					Out += FString::Printf(TEXT("\\u%04x"), static_cast<uint32>(Ch));
				}
				else
				{
					Out.AppendChar(Ch);
				}
				break;
			}
		}
		Out.AppendChar(TEXT('"'));
	}

	static void AppendCanonicalJsonValue(const TSharedPtr<FJsonValue>& Value, FString& Out);

	static void AppendCanonicalJsonObject(const TSharedPtr<FJsonObject>& Object, FString& Out)
	{
		if (!Object.IsValid())
		{
			Out += TEXT("null");
			return;
		}

		TArray<FString> Keys;
		Object->Values.GetKeys(Keys);
		Keys.RemoveAll([](const FString& Key)
		{
			return ShouldOmitCanonicalReviewSnapshotField(Key);
		});
		Keys.Sort();

		Out.AppendChar(TEXT('{'));
		bool bFirst = true;
		for (const FString& Key : Keys)
		{
			const TSharedPtr<FJsonValue>* FieldValue = Object->Values.Find(Key);
			if (!FieldValue)
			{
				continue;
			}

			if (!bFirst)
			{
				Out.AppendChar(TEXT(','));
			}
			bFirst = false;
			AppendCanonicalJsonString(Key, Out);
			Out.AppendChar(TEXT(':'));
			AppendCanonicalJsonValue(*FieldValue, Out);
		}
		Out.AppendChar(TEXT('}'));
	}

	static void AppendCanonicalJsonValue(const TSharedPtr<FJsonValue>& Value, FString& Out)
	{
		if (!Value.IsValid() || Value->Type == EJson::Null)
		{
			Out += TEXT("null");
			return;
		}

		switch (Value->Type)
		{
		case EJson::String:
			AppendCanonicalJsonString(Value->AsString(), Out);
			break;
		case EJson::Number:
			Out += LexToString(Value->AsNumber());
			break;
		case EJson::Boolean:
			Out += Value->AsBool() ? TEXT("true") : TEXT("false");
			break;
		case EJson::Array:
			Out.AppendChar(TEXT('['));
			{
				bool bFirst = true;
				for (const TSharedPtr<FJsonValue>& ArrayValue : Value->AsArray())
				{
					if (!bFirst)
					{
						Out.AppendChar(TEXT(','));
					}
					bFirst = false;
					AppendCanonicalJsonValue(ArrayValue, Out);
				}
			}
			Out.AppendChar(TEXT(']'));
			break;
		case EJson::Object:
			AppendCanonicalJsonObject(Value->AsObject(), Out);
			break;
		default:
			Out += TEXT("null");
			break;
		}
	}
}

TArray<TSharedPtr<FJsonValue>> FBlueprintHelperReviewBaselineSnapshotServiceUtils::MakeStringArray(
	const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	for (const FString& Value : Values)
	{
		Result.Add(MakeShared<FJsonValueString>(Value));
	}
	return Result;
}

FString FBlueprintHelperReviewBaselineSnapshotServiceUtils::GetObjectPathNameSafe(const UObject* Object)
{
	return Object ? Object->GetPathName() : FString();
}

FString FBlueprintHelperReviewBaselineSnapshotServiceUtils::GetObjectClassPathNameSafe(
	const UObject* Object)
{
	return Object && Object->GetClass() ? Object->GetClass()->GetPathName() : FString();
}

FString FBlueprintHelperReviewBaselineSnapshotServiceUtils::SerializeJsonObject(
	const TSharedRef<FJsonObject>& Json)
{
	FString Serialized;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	FJsonSerializer::Serialize(Json, Writer);
	return Serialized;
}

FString FBlueprintHelperReviewBaselineSnapshotServiceUtils::SerializeJsonObjectCanonical(
	const TSharedRef<FJsonObject>& Json)
{
	FString Serialized;
	AppendCanonicalJsonObject(Json, Serialized);
	return Serialized;
}

FString FBlueprintHelperReviewBaselineSnapshotServiceUtils::ExtractTargetName(
	const FBlueprintHelperReviewAtomicTarget& Target)
{
	if (!Target.PropertyPath.IsEmpty())
	{
		return Target.PropertyPath;
	}
	if (!Target.ComponentPath.IsEmpty())
	{
		return Target.ComponentPath;
	}

	int32 LastColon = INDEX_NONE;
	if (Target.TargetKey.FindLastChar(TEXT(':'), LastColon))
	{
		return Target.TargetKey.Mid(LastColon + 1);
	}
	return Target.DisplayLabel;
}

UObject* FBlueprintHelperReviewBaselineSnapshotServiceUtils::ResolveClassDefaultSnapshotObject(
	UObject* Asset)
{
	UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
	if (!Blueprint)
	{
		return Asset;
	}

	UClass* DefaultClass = Blueprint->GeneratedClass
		? Blueprint->GeneratedClass
		: Blueprint->SkeletonGeneratedClass;
	return DefaultClass ? DefaultClass->GetDefaultObject() : nullptr;
}

void FBlueprintHelperReviewBaselineSnapshotServiceUtils::SplitWidgetPropertyTarget(
	const FString& TargetName,
	FString& OutWidgetName,
	FString& OutPropertyName)
{
	OutWidgetName = TargetName;
	OutPropertyName.Reset();

	FString Left;
	FString Right;
	if (TargetName.Split(TEXT("."), &Left, &Right) && !Left.IsEmpty())
	{
		OutWidgetName = Left;
		OutPropertyName = Right;
	}
}

FString FBlueprintHelperReviewBaselineSnapshotServiceUtils::FindScsParentComponentName(
	const UBlueprint* Blueprint,
	const USCS_Node* ChildNode)
{
	if (!Blueprint || !Blueprint->SimpleConstructionScript || !ChildNode)
	{
		return FString();
	}

	for (const USCS_Node* CandidateParent : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (!CandidateParent)
		{
			continue;
		}
		for (const USCS_Node* CandidateChild : CandidateParent->GetChildNodes())
		{
			if (CandidateChild == ChildNode)
			{
				return CandidateParent->GetVariableName().ToString();
			}
		}
	}
	return FString();
}

FString FBlueprintHelperReviewBaselineSnapshotServiceUtils::PinDirectionToString(
	EEdGraphPinDirection Direction)
{
	return Direction == EGPD_Output ? TEXT("output") : TEXT("input");
}

void FBlueprintHelperReviewBaselineSnapshotServiceUtils::AppendGraphs(
	TArray<UEdGraph*>& OutGraphs,
	const TArray<UEdGraph*>& InGraphs)
{
	for (UEdGraph* Graph : InGraphs)
	{
		if (Graph)
		{
			OutGraphs.Add(Graph);
		}
	}
}
