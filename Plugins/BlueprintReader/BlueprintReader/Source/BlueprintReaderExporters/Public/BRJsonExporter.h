#pragma once

#include "CoreMinimal.h"
#include "BPExportModel.h"

// Renders a FBRExportModel into the canonical *.bp.json structural dump (TZ_BlueprintReader.md section 6.1).
class BLUEPRINTREADEREXPORTERS_API FBRJsonExporter
{
public:
	static FString ToJsonString(const FBRExportModel& Model);
	static bool ExportToFile(const FBRExportModel& Model, const FString& FilePath);

	// Aggregates cross-references from several exported Blueprints into ProjectBlueprintGraph.json (section 6.3).
	static FString ProjectSummaryToJsonString(const TArray<FBRExportModel>& Models);
	static bool ExportProjectSummaryToFile(const TArray<FBRExportModel>& Models, const FString& FilePath);
};
