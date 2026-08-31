#pragma once

#include "CoreMinimal.h"

// Plain (non-UObject) in-memory representation of a single Blueprint's exported
// content. This is the canonical "source of truth" produced by FBlueprintExtractor
// and consumed by the JSON / Markdown exporters. See TZ_BlueprintReader.md section 6.

struct FBRPinInfo
{
	FString Name;
	FString Direction;          // "input" | "output"
	FString Category;           // exec, bool, int, float, object, struct, class, interface, wildcard, delegate ...
	FString SubCategory;        // class/struct/enum name, if applicable
	FString ContainerType;      // "None" | "Array" | "Set" | "Map"
	FString ValueSubCategory;   // for Map: value type name
	FString DefaultValue;
	FString DefaultObjectPath;  // set if DefaultObject is a literal reference
	bool bIsReference = false;
	TArray<FString> LinkedTo;   // "NodeId.PinName" of every connected pin
};

struct FBRNodeInfo
{
	FString Id;
	FString NodeType;   // e.g. K2Node_CallFunction
	FString Title;
	FString Comment;
	float PosX = 0.f;
	float PosY = 0.f;
	TArray<FBRPinInfo> Pins;
	TMap<FString, FString> Extras; // typed semantic extras, see BlueprintExtractor.cpp
};

struct FBRConnectionInfo
{
	FString From; // "NodeId.PinName"
	FString To;   // "NodeId.PinName"
	FString Kind; // "exec" | "data"
};

struct FBRCommentBoxInfo
{
	FString Text;
	TArray<FString> ContainedNodeIds;
};

struct FBRGraphInfo
{
	FString Name;
	FString Type; // "Ubergraph" | "Function" | "Macro" | "Delegate" | "Collapsed"
	TArray<FBRNodeInfo> Nodes;
	TArray<FBRConnectionInfo> Connections;
	TArray<FBRCommentBoxInfo> CommentBoxes;
};

struct FBRVariableInfo
{
	FString Name;
	FString TypeCategory;
	FString TypeSubCategory;
	FString ContainerType;
	FString DefaultValue;
	FString Category;
	FString Tooltip;
	bool bEditable = false;
	bool bBlueprintReadOnly = false;
	bool bExposeOnSpawn = false;
	bool bPrivate = false;
	FString Replication; // "None" | "Replicated" | "RepNotify"
	FString RepNotifyFunctionName;
};

struct FBRPropertyOverride
{
	FString Name;
	FString Value; // full ExportText value - may be very large for struct properties (PostProcessSettings, BodyInstance, ...)
};

struct FBRComponentInfo
{
	FString Name;
	FString ComponentClass;
	FString ParentComponentName;
	FString AttachSocket;
	TArray<FBRPropertyOverride> PropertyOverrides; // properties changed from class defaults
};

struct FBRTimelineTrackInfo
{
	FString Kind; // "Event" | "Float" | "Vector" | "LinearColor"
	FString Name;
	FString BoundFunctionOrProperty; // event track: bound function name; property track: driven property name
	FString CurveAsset;              // curve asset name, if any
};

struct FBRTimelineInfo
{
	FString Name;
	float Length = 0.f;
	FString LengthMode; // "TL_TimelineLength" | "TL_LastKeyFrame"
	bool bAutoPlay = false;
	bool bLoop = false;
	bool bReplicated = false;
	TArray<FBRTimelineTrackInfo> Tracks;
};

struct FBRCrossReference
{
	FString Kind;        // inherits, implements_interface, variable_type, cast, spawn_actor, component_class, literal_object_reference, structural
	FString TargetClass;
	FString NodeId;
	FString GraphName;
	bool bResolved = true; // false => dynamic/unresolved reference
	FString Note;
};

struct FBRExportModel
{
	// Asset metadata
	FString AssetPath;
	FString ClassName;
	FString ParentClass;
	FString BlueprintType;
	TArray<FString> Interfaces;

	TArray<FBRVariableInfo> Variables;
	TArray<FBRComponentInfo> Components;
	TArray<FBRTimelineInfo> Timelines;
	TArray<FBRGraphInfo> Graphs;
	TArray<FBRCrossReference> CrossReferences;

	TArray<FString> UnsupportedNodeTypes; // distinct node classes hit only by generic fallback
};
