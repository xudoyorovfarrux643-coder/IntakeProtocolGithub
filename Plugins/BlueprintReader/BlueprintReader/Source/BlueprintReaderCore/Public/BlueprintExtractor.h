#pragma once

#include "CoreMinimal.h"
#include "BPExportModel.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
struct FEdGraphPinType;

// Walks a UBlueprint's variables, components, and graphs (nodes/pins/connections)
// and produces a canonical FBRExportModel. See TZ_BlueprintReader.md section 5 and 7.
class BLUEPRINTREADERCORE_API FBlueprintExtractor
{
public:
	static FBRExportModel Extract(UBlueprint* Blueprint);

private:
	static void ExtractVariables(UBlueprint* Blueprint, FBRExportModel& Model);
	static void ExtractComponents(UBlueprint* Blueprint, FBRExportModel& Model);
	static void WalkSCSNode(class USCS_Node* Node, const FString& ParentName, FBRExportModel& Model);
	static void ExtractBaseCrossReferences(UBlueprint* Blueprint, FBRExportModel& Model);
	static void ExtractTimelines(UBlueprint* Blueprint, FBRExportModel& Model);

	static void ExtractGraph(UEdGraph* Graph, const FString& GraphType, const FString& GraphNameOverride, FBRExportModel& Model);
	static FBRPinInfo BuildPinInfo(UEdGraphPin* Pin, const TMap<UEdGraphNode*, FString>& NodeIdMap);
	static void ConvertPinType(const FEdGraphPinType& PinType, FString& OutCategory, FString& OutSubCategory, FString& OutContainer, FString& OutValueSubCategory);
	static void AddTypedExtras(UEdGraphNode* Node, FBRNodeInfo& NodeInfo, const FString& GraphName, FBRExportModel& Model);
	static void ScanPinsForLiteralReferences(FBRNodeInfo& NodeInfo, const FString& GraphName, FBRExportModel& Model);
};
