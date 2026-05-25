#pragma once

#include "CoreMinimal.h"

class FJsonObject;

enum class EBlueprintHelperGraphFragmentDiagnosticSeverity : uint8
{
	Info,
	Warning,
	Error
};

enum class EBlueprintHelperGraphFragmentEdgeKind : uint8
{
	Unknown,
	Exec,
	Data
};

enum class EBlueprintHelperGraphFragmentPortDirection : uint8
{
	Unknown,
	ExecInput,
	ExecOutput,
	DataInput,
	DataOutput
};

enum class EBlueprintHelperGraphFragmentLayoutKind : uint8
{
	Unknown,
	Statement,
	Expression,
	Join
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphFragmentDiagnostic
{
	FString Code;
	FString Path;
	FString Message;
	EBlueprintHelperGraphFragmentDiagnosticSeverity Severity = EBlueprintHelperGraphFragmentDiagnosticSeverity::Error;

	TSharedRef<FJsonObject> ToJson() const;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphFragmentPinTypeRef
{
	FString Category;
	FString SubCategory;
	FString ObjectPath;
	FString ContainerType;
	FString ValueType;
	bool bIsReference = false;
	bool bIsConst = false;

	bool IsValid() const;
	TSharedRef<FJsonObject> ToJson() const;
	static FBlueprintHelperGraphFragmentPinTypeRef FromJson(const TSharedPtr<FJsonObject>& Object);
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphFragmentLayoutRef
{
	// DEPRECATED_LAYOUT: fragment layout metadata is retained for legacy debug evidence only.
	// It must not become a layout solver input; the UE-side GraphLayout system owns layout rules.
	EBlueprintHelperGraphFragmentLayoutKind Kind = EBlueprintHelperGraphFragmentLayoutKind::Unknown;
	int32 Row = INDEX_NONE;
	int32 Column = INDEX_NONE;
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;
	bool bHasPosition = false;
	bool bHasSize = false;
	TMap<FString, FString> Hints;

	TSharedRef<FJsonObject> ToJson() const;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphFragmentRef
{
	FString FragmentId;
	FString SourceStatementId;
	FString Path;
	FString Kind;
	FBlueprintHelperGraphFragmentLayoutRef Layout;
	TMap<FString, FString> Metadata;

	bool IsValid() const;
	TSharedRef<FJsonObject> ToJson() const;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphFragmentEndpointRef
{
	FString FragmentId;
	FString PortId;
	FString PinName;
	FString Type;
	EBlueprintHelperGraphFragmentPortDirection Direction = EBlueprintHelperGraphFragmentPortDirection::Unknown;
	FBlueprintHelperGraphFragmentPinTypeRef PinType;

	bool IsValid() const;
	TSharedRef<FJsonObject> ToJson() const;
	static FBlueprintHelperGraphFragmentEndpointRef FromJson(const TSharedPtr<FJsonObject>& Object);
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphFragmentExecEdge
{
	FString EdgeId;
	FBlueprintHelperGraphFragmentEndpointRef From;
	FBlueprintHelperGraphFragmentEndpointRef To;
	FString Path;

	TSharedRef<FJsonObject> ToJson() const;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphFragmentDataEdge
{
	FString EdgeId;
	FBlueprintHelperGraphFragmentEndpointRef From;
	FBlueprintHelperGraphFragmentEndpointRef To;
	FString SymbolId;
	FString Path;

	TSharedRef<FJsonObject> ToJson() const;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphFragmentEntryExitRefs
{
	TArray<FBlueprintHelperGraphFragmentEndpointRef> Entries;
	TArray<FBlueprintHelperGraphFragmentEndpointRef> Exits;

	bool HasEntry() const;
	bool HasExit() const;
	TSharedRef<FJsonObject> ToJson() const;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphFragmentDag
{
	FString Schema;
	TArray<FBlueprintHelperGraphFragmentRef> Fragments;
	TArray<FBlueprintHelperGraphFragmentExecEdge> ExecEdges;
	TArray<FBlueprintHelperGraphFragmentDataEdge> DataEdges;
	FBlueprintHelperGraphFragmentEntryExitRefs EntryExitRefs;
	TArray<FBlueprintHelperGraphFragmentDiagnostic> Diagnostics;
	TMap<FString, FString> Metadata;

	void Reset();
	bool IsEmpty() const;
	bool HasErrors() const;
	bool TryFindFragment(const FString& FragmentId, FBlueprintHelperGraphFragmentRef& OutFragment) const;
	TSharedRef<FJsonObject> ToJson() const;
	void AddDiagnostic(
		const FString& Code,
		const FString& Path,
		const FString& Message,
		EBlueprintHelperGraphFragmentDiagnosticSeverity Severity = EBlueprintHelperGraphFragmentDiagnosticSeverity::Error);
};
