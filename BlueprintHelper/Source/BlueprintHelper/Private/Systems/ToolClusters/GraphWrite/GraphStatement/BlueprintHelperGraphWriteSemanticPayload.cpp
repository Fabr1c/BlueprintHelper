#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphWriteSemanticPayload.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

const TCHAR* FBlueprintHelperGraphWriteSemanticPayload::SchemaName()
{
	return TEXT("BlueprintHelper.GraphWriteSemanticPayload");
}

TSharedRef<FJsonObject> FBlueprintHelperGraphWriteSemanticPayload::ToJsonObject() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), SchemaName());
	Root->SetStringField(TEXT("version"), TEXT("1.0"));
	Root->SetStringField(TEXT("mode"), Mode);

	TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), TargetAssetPath);
	Target->SetStringField(TEXT("graph"), TargetGraph);
	Root->SetObjectField(TEXT("target"), Target);

	TSharedRef<FJsonObject> Options = MakeShared<FJsonObject>();
	Options->SetBoolField(TEXT("compile"), bCompile);
	Options->SetBoolField(TEXT("save"), bSave);
	Options->SetBoolField(TEXT("strict"), bStrict);
	Options->SetBoolField(TEXT("dry_run"), bDryRun);
	Options->SetBoolField(TEXT("create_missing_variables"), bCreateMissingVariables);
	Options->SetBoolField(TEXT("reconstruct_existing_nodes"), bReconstructExistingNodes);
	Root->SetObjectField(TEXT("options"), Options);

	if (LogicSpec.IsValid())
	{
		Root->SetObjectField(TEXT("logic_spec"), LogicSpec);
	}

	return Root;
}

FString FBlueprintHelperGraphWriteSemanticPayload::ToJsonString() const
{
	FString JsonText;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	FJsonSerializer::Serialize(ToJsonObject(), Writer);
	return JsonText;
}
