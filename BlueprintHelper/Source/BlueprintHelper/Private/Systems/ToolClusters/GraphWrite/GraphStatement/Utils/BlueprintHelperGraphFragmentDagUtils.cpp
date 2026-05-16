// BlueprintHelper GraphStatement BlueprintHelperGraphFragmentDagUtils implementation.

#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

FString FBlueprintHelperGraphFragmentDagUtils::NormalizeFragmentId(const FString& FragmentId)
{
	return FragmentId.TrimStartAndEnd();
}
const TCHAR* FBlueprintHelperGraphFragmentDagUtils::SeverityToString(const EBlueprintHelperGraphFragmentDiagnosticSeverity Severity)
{
	switch (Severity)
	{
	case EBlueprintHelperGraphFragmentDiagnosticSeverity::Info:
		return TEXT("info");
	case EBlueprintHelperGraphFragmentDiagnosticSeverity::Warning:
		return TEXT("warning");
	default:
		return TEXT("error");
	}
}
const TCHAR* FBlueprintHelperGraphFragmentDagUtils::DirectionToString(const EBlueprintHelperGraphFragmentPortDirection Direction)
{
	switch (Direction)
	{
	case EBlueprintHelperGraphFragmentPortDirection::ExecInput:
		return TEXT("exec_input");
	case EBlueprintHelperGraphFragmentPortDirection::ExecOutput:
		return TEXT("exec_output");
	case EBlueprintHelperGraphFragmentPortDirection::DataInput:
		return TEXT("data_input");
	case EBlueprintHelperGraphFragmentPortDirection::DataOutput:
		return TEXT("data_output");
	default:
		return TEXT("unknown");
	}
}
const TCHAR* FBlueprintHelperGraphFragmentDagUtils::LayoutKindToString(const EBlueprintHelperGraphFragmentLayoutKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperGraphFragmentLayoutKind::Statement:
		return TEXT("statement");
	case EBlueprintHelperGraphFragmentLayoutKind::Expression:
		return TEXT("expression");
	case EBlueprintHelperGraphFragmentLayoutKind::Join:
		return TEXT("join");
	default:
		return TEXT("unknown");
	}
}
TSharedRef<FJsonObject> FBlueprintHelperGraphFragmentDagUtils::Vector2DToJson(const FVector2D& Value)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("x"), Value.X);
	Json->SetNumberField(TEXT("y"), Value.Y);
	return Json;
}
TArray<TSharedPtr<FJsonValue>> FBlueprintHelperGraphFragmentDagUtils::StringMapToJsonArray(const TMap<FString, FString>& Values)
{
	TArray<FString> Keys;
	Values.GetKeys(Keys);
	Keys.Sort();

	TArray<TSharedPtr<FJsonValue>> Entries;
	for (const FString& Key : Keys)
	{
		const FString* Value = Values.Find(Key);
		if (!Value || Key.IsEmpty())
		{
			continue;
		}

		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("key"), Key);
		Entry->SetStringField(TEXT("value"), *Value);
		Entries.Add(MakeShared<FJsonValueObject>(Entry));
	}
	return Entries;
}
TArray<TSharedPtr<FJsonValue>> FBlueprintHelperGraphFragmentDagUtils::EndpointsToJsonArray(const TArray<FBlueprintHelperGraphFragmentEndpointRef>& Endpoints)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FBlueprintHelperGraphFragmentEndpointRef& Endpoint : Endpoints)
	{
		Values.Add(MakeShared<FJsonValueObject>(Endpoint.ToJson()));
	}
	return Values;
}
