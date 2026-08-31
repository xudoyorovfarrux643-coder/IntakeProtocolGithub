#include "BRJsonExporter.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"

namespace
{
	TSharedRef<FJsonValueString> Str(const FString& S)
	{
		return MakeShared<FJsonValueString>(S);
	}

	TSharedRef<FJsonObject> PinToJson(const FBRPinInfo& Pin)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Pin.Name);
		Obj->SetStringField(TEXT("direction"), Pin.Direction);
		Obj->SetStringField(TEXT("category"), Pin.Category);
		if (!Pin.SubCategory.IsEmpty()) { Obj->SetStringField(TEXT("subCategory"), Pin.SubCategory); }
		if (Pin.ContainerType != TEXT("None")) { Obj->SetStringField(TEXT("containerType"), Pin.ContainerType); }
		if (!Pin.ValueSubCategory.IsEmpty()) { Obj->SetStringField(TEXT("valueSubCategory"), Pin.ValueSubCategory); }
		if (!Pin.DefaultValue.IsEmpty()) { Obj->SetStringField(TEXT("defaultValue"), Pin.DefaultValue); }
		if (!Pin.DefaultObjectPath.IsEmpty()) { Obj->SetStringField(TEXT("defaultObject"), Pin.DefaultObjectPath); }
		if (Pin.bIsReference) { Obj->SetBoolField(TEXT("isReference"), true); }
		if (Pin.LinkedTo.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FString& L : Pin.LinkedTo) { Arr.Add(Str(L)); }
			Obj->SetArrayField(TEXT("linkedTo"), Arr);
		}
		return Obj;
	}

	TSharedRef<FJsonObject> NodeToJson(const FBRNodeInfo& Node)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("id"), Node.Id);
		Obj->SetStringField(TEXT("nodeType"), Node.NodeType);
		Obj->SetStringField(TEXT("title"), Node.Title);
		if (!Node.Comment.IsEmpty()) { Obj->SetStringField(TEXT("comment"), Node.Comment); }

		TSharedRef<FJsonObject> Pos = MakeShared<FJsonObject>();
		Pos->SetNumberField(TEXT("x"), Node.PosX);
		Pos->SetNumberField(TEXT("y"), Node.PosY);
		Obj->SetObjectField(TEXT("position"), Pos);

		TArray<TSharedPtr<FJsonValue>> PinsArr;
		for (const FBRPinInfo& Pin : Node.Pins) { PinsArr.Add(MakeShared<FJsonValueObject>(PinToJson(Pin))); }
		Obj->SetArrayField(TEXT("pins"), PinsArr);

		if (Node.Extras.Num() > 0)
		{
			TSharedRef<FJsonObject> Extras = MakeShared<FJsonObject>();
			for (const TPair<FString, FString>& Pair : Node.Extras) { Extras->SetStringField(Pair.Key, Pair.Value); }
			Obj->SetObjectField(TEXT("extras"), Extras);
		}
		return Obj;
	}

	TSharedRef<FJsonObject> ConnectionToJson(const FBRConnectionInfo& Conn)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("from"), Conn.From);
		Obj->SetStringField(TEXT("to"), Conn.To);
		Obj->SetStringField(TEXT("kind"), Conn.Kind);
		return Obj;
	}

	TSharedRef<FJsonObject> CommentBoxToJson(const FBRCommentBoxInfo& Box)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("text"), Box.Text);
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FString& Id : Box.ContainedNodeIds) { Arr.Add(Str(Id)); }
		Obj->SetArrayField(TEXT("containedNodeIds"), Arr);
		return Obj;
	}

	TSharedRef<FJsonObject> GraphToJson(const FBRGraphInfo& Graph)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Graph.Name);
		Obj->SetStringField(TEXT("type"), Graph.Type);

		TArray<TSharedPtr<FJsonValue>> NodesArr;
		for (const FBRNodeInfo& Node : Graph.Nodes) { NodesArr.Add(MakeShared<FJsonValueObject>(NodeToJson(Node))); }
		Obj->SetArrayField(TEXT("nodes"), NodesArr);

		TArray<TSharedPtr<FJsonValue>> ConnArr;
		for (const FBRConnectionInfo& Conn : Graph.Connections) { ConnArr.Add(MakeShared<FJsonValueObject>(ConnectionToJson(Conn))); }
		Obj->SetArrayField(TEXT("connections"), ConnArr);

		if (Graph.CommentBoxes.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> CommentsArr;
			for (const FBRCommentBoxInfo& Box : Graph.CommentBoxes) { CommentsArr.Add(MakeShared<FJsonValueObject>(CommentBoxToJson(Box))); }
			Obj->SetArrayField(TEXT("commentBoxes"), CommentsArr);
		}
		return Obj;
	}

	TSharedRef<FJsonObject> VariableToJson(const FBRVariableInfo& Var)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Var.Name);

		TSharedRef<FJsonObject> Type = MakeShared<FJsonObject>();
		Type->SetStringField(TEXT("category"), Var.TypeCategory);
		if (!Var.TypeSubCategory.IsEmpty()) { Type->SetStringField(TEXT("subCategory"), Var.TypeSubCategory); }
		if (Var.ContainerType != TEXT("None")) { Type->SetStringField(TEXT("containerType"), Var.ContainerType); }
		Obj->SetObjectField(TEXT("type"), Type);

		if (!Var.DefaultValue.IsEmpty()) { Obj->SetStringField(TEXT("defaultValue"), Var.DefaultValue); }
		if (!Var.Category.IsEmpty()) { Obj->SetStringField(TEXT("category"), Var.Category); }
		if (!Var.Tooltip.IsEmpty()) { Obj->SetStringField(TEXT("tooltip"), Var.Tooltip); }
		Obj->SetBoolField(TEXT("editable"), Var.bEditable);
		Obj->SetBoolField(TEXT("blueprintReadOnly"), Var.bBlueprintReadOnly);
		Obj->SetBoolField(TEXT("exposeOnSpawn"), Var.bExposeOnSpawn);
		Obj->SetBoolField(TEXT("private"), Var.bPrivate);
		Obj->SetStringField(TEXT("replication"), Var.Replication);
		if (!Var.RepNotifyFunctionName.IsEmpty()) { Obj->SetStringField(TEXT("repNotifyFunction"), Var.RepNotifyFunctionName); }
		return Obj;
	}

	TSharedRef<FJsonObject> ComponentToJson(const FBRComponentInfo& Comp)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Comp.Name);
		Obj->SetStringField(TEXT("class"), Comp.ComponentClass);
		if (!Comp.ParentComponentName.IsEmpty()) { Obj->SetStringField(TEXT("parent"), Comp.ParentComponentName); }
		if (!Comp.AttachSocket.IsEmpty()) { Obj->SetStringField(TEXT("attachSocket"), Comp.AttachSocket); }
		if (Comp.PropertyOverrides.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FBRPropertyOverride& Override : Comp.PropertyOverrides)
			{
				TSharedRef<FJsonObject> OverrideObj = MakeShared<FJsonObject>();
				OverrideObj->SetStringField(TEXT("name"), Override.Name);
				OverrideObj->SetStringField(TEXT("value"), Override.Value);
				Arr.Add(MakeShared<FJsonValueObject>(OverrideObj));
			}
			Obj->SetArrayField(TEXT("propertyOverrides"), Arr);
		}
		return Obj;
	}

	TSharedRef<FJsonObject> TimelineTrackToJson(const FBRTimelineTrackInfo& Track)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("kind"), Track.Kind);
		Obj->SetStringField(TEXT("name"), Track.Name);
		if (!Track.BoundFunctionOrProperty.IsEmpty()) { Obj->SetStringField(TEXT("boundFunctionOrProperty"), Track.BoundFunctionOrProperty); }
		if (!Track.CurveAsset.IsEmpty()) { Obj->SetStringField(TEXT("curveAsset"), Track.CurveAsset); }
		return Obj;
	}

	TSharedRef<FJsonObject> TimelineToJson(const FBRTimelineInfo& Timeline)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Timeline.Name);
		Obj->SetNumberField(TEXT("length"), Timeline.Length);
		Obj->SetStringField(TEXT("lengthMode"), Timeline.LengthMode);
		Obj->SetBoolField(TEXT("autoPlay"), Timeline.bAutoPlay);
		Obj->SetBoolField(TEXT("loop"), Timeline.bLoop);
		Obj->SetBoolField(TEXT("replicated"), Timeline.bReplicated);

		TArray<TSharedPtr<FJsonValue>> TracksArr;
		for (const FBRTimelineTrackInfo& Track : Timeline.Tracks) { TracksArr.Add(MakeShared<FJsonValueObject>(TimelineTrackToJson(Track))); }
		Obj->SetArrayField(TEXT("tracks"), TracksArr);
		return Obj;
	}

	TSharedRef<FJsonObject> CrossReferenceToJson(const FBRCrossReference& XRef)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("kind"), XRef.Kind);
		Obj->SetStringField(TEXT("targetClass"), XRef.TargetClass);
		if (!XRef.NodeId.IsEmpty()) { Obj->SetStringField(TEXT("node"), XRef.NodeId); }
		if (!XRef.GraphName.IsEmpty()) { Obj->SetStringField(TEXT("graph"), XRef.GraphName); }
		Obj->SetBoolField(TEXT("resolved"), XRef.bResolved);
		if (!XRef.Note.IsEmpty()) { Obj->SetStringField(TEXT("note"), XRef.Note); }
		return Obj;
	}
}

FString FBRJsonExporter::ToJsonString(const FBRExportModel& Model)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schemaVersion"), TEXT("1.0"));

	TSharedRef<FJsonObject> Asset = MakeShared<FJsonObject>();
	Asset->SetStringField(TEXT("path"), Model.AssetPath);
	Asset->SetStringField(TEXT("class"), Model.ClassName);
	Asset->SetStringField(TEXT("parentClass"), Model.ParentClass);
	Asset->SetStringField(TEXT("blueprintType"), Model.BlueprintType);
	TArray<TSharedPtr<FJsonValue>> IfaceArr;
	for (const FString& Iface : Model.Interfaces) { IfaceArr.Add(Str(Iface)); }
	Asset->SetArrayField(TEXT("interfaces"), IfaceArr);
	Root->SetObjectField(TEXT("asset"), Asset);

	TArray<TSharedPtr<FJsonValue>> VarsArr;
	for (const FBRVariableInfo& Var : Model.Variables) { VarsArr.Add(MakeShared<FJsonValueObject>(VariableToJson(Var))); }
	Root->SetArrayField(TEXT("variables"), VarsArr);

	TArray<TSharedPtr<FJsonValue>> CompsArr;
	for (const FBRComponentInfo& Comp : Model.Components) { CompsArr.Add(MakeShared<FJsonValueObject>(ComponentToJson(Comp))); }
	Root->SetArrayField(TEXT("components"), CompsArr);

	TArray<TSharedPtr<FJsonValue>> TimelinesArr;
	for (const FBRTimelineInfo& Timeline : Model.Timelines) { TimelinesArr.Add(MakeShared<FJsonValueObject>(TimelineToJson(Timeline))); }
	Root->SetArrayField(TEXT("timelines"), TimelinesArr);

	TArray<TSharedPtr<FJsonValue>> GraphsArr;
	for (const FBRGraphInfo& Graph : Model.Graphs) { GraphsArr.Add(MakeShared<FJsonValueObject>(GraphToJson(Graph))); }
	Root->SetArrayField(TEXT("graphs"), GraphsArr);

	TArray<TSharedPtr<FJsonValue>> XRefsArr;
	for (const FBRCrossReference& XRef : Model.CrossReferences) { XRefsArr.Add(MakeShared<FJsonValueObject>(CrossReferenceToJson(XRef))); }
	Root->SetArrayField(TEXT("crossReferences"), XRefsArr);

	if (Model.UnsupportedNodeTypes.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> UnsupportedArr;
		for (const FString& Type : Model.UnsupportedNodeTypes) { UnsupportedArr.Add(Str(Type)); }
		Root->SetArrayField(TEXT("genericallyHandledNodeTypes"), UnsupportedArr);
	}

	FString Output;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Output);
	FJsonSerializer::Serialize(Root, Writer);
	return Output;
}

bool FBRJsonExporter::ExportToFile(const FBRExportModel& Model, const FString& FilePath)
{
	const FString Json = ToJsonString(Model);
	return FFileHelper::SaveStringToFile(Json, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

FString FBRJsonExporter::ProjectSummaryToJsonString(const TArray<FBRExportModel>& Models)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schemaVersion"), TEXT("1.0"));

	TArray<TSharedPtr<FJsonValue>> BlueprintsArr;
	for (const FBRExportModel& Model : Models)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("path"), Model.AssetPath);
		Obj->SetStringField(TEXT("class"), Model.ClassName);
		BlueprintsArr.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Root->SetArrayField(TEXT("blueprints"), BlueprintsArr);

	TArray<TSharedPtr<FJsonValue>> EdgesArr;
	for (const FBRExportModel& Model : Models)
	{
		for (const FBRCrossReference& XRef : Model.CrossReferences)
		{
			TSharedRef<FJsonObject> Edge = MakeShared<FJsonObject>();
			Edge->SetStringField(TEXT("from"), Model.ClassName);
			Edge->SetStringField(TEXT("kind"), XRef.Kind);
			Edge->SetStringField(TEXT("to"), XRef.TargetClass);
			Edge->SetBoolField(TEXT("resolved"), XRef.bResolved);
			if (!XRef.GraphName.IsEmpty()) { Edge->SetStringField(TEXT("graph"), XRef.GraphName); }
			if (!XRef.Note.IsEmpty()) { Edge->SetStringField(TEXT("note"), XRef.Note); }
			EdgesArr.Add(MakeShared<FJsonValueObject>(Edge));
		}
	}
	Root->SetArrayField(TEXT("edges"), EdgesArr);

	FString Output;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Output);
	FJsonSerializer::Serialize(Root, Writer);
	return Output;
}

bool FBRJsonExporter::ExportProjectSummaryToFile(const TArray<FBRExportModel>& Models, const FString& FilePath)
{
	const FString Json = ProjectSummaryToJsonString(Models);
	return FFileHelper::SaveStringToFile(Json, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
