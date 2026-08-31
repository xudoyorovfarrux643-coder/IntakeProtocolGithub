#pragma once

#include "CoreMinimal.h"
#include "BPExportModel.h"

// Renders a FBRExportModel into a readable *.bp.md pseudocode narrative, meant to be
// consumed by an LLM for code review (TZ_BlueprintReader.md section 6.2).
// Operates purely on FBRExportModel - no Unreal Editor/Kismet dependency.
class BLUEPRINTREADEREXPORTERS_API FBRMarkdownExporter
{
public:
	static FString ToMarkdownString(const FBRExportModel& Model);
	static bool ExportToFile(const FBRExportModel& Model, const FString& FilePath);

	// Aggregates cross-references from several exported Blueprints into ProjectBlueprintGraph.md (section 6.3).
	static FString ProjectSummaryToMarkdownString(const TArray<FBRExportModel>& Models);
	static bool ExportProjectSummaryToFile(const TArray<FBRExportModel>& Models, const FString& FilePath);
};
