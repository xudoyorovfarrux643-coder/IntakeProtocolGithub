#include "BlueprintExtractor.h"

#include "Engine/Blueprint.h"
#include "Engine/MemberReference.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/TimelineTemplate.h"
#include "Components/ActorComponent.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveLinearColor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"

#include "K2Node_CallFunction.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableSet.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Composite.h"

namespace
{
	FString ContainerTypeToString(EPinContainerType InType)
	{
		switch (InType)
		{
		case EPinContainerType::Array: return TEXT("Array");
		case EPinContainerType::Set:   return TEXT("Set");
		case EPinContainerType::Map:   return TEXT("Map");
		default:                       return TEXT("None");
		}
	}

	FString LastPathComponent(const FString& Path)
	{
		FString Left, Right;
		if (Path.Split(TEXT("."), &Left, &Right, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
		{
			return Right;
		}
		return Path;
	}
}

void FBlueprintExtractor::ConvertPinType(const FEdGraphPinType& PinType, FString& OutCategory, FString& OutSubCategory, FString& OutContainer, FString& OutValueSubCategory)
{
	OutCategory = PinType.PinCategory.ToString();
	OutSubCategory = PinType.PinSubCategoryObject.IsValid() ? PinType.PinSubCategoryObject->GetName() : PinType.PinSubCategory.ToString();
	OutContainer = ContainerTypeToString(PinType.ContainerType);
	OutValueSubCategory.Reset();
	if (PinType.ContainerType == EPinContainerType::Map)
	{
		OutValueSubCategory = PinType.PinValueType.TerminalSubCategoryObject.IsValid()
			? PinType.PinValueType.TerminalSubCategoryObject->GetName()
			: PinType.PinValueType.TerminalSubCategory.ToString();
	}
}

FBRPinInfo FBlueprintExtractor::BuildPinInfo(UEdGraphPin* Pin, const TMap<UEdGraphNode*, FString>& NodeIdMap)
{
	FBRPinInfo Info;
	Info.Name = Pin->PinName.ToString();
	Info.Direction = (Pin->Direction == EGPD_Input) ? TEXT("input") : TEXT("output");
	ConvertPinType(Pin->PinType, Info.Category, Info.SubCategory, Info.ContainerType, Info.ValueSubCategory);
	Info.DefaultValue = Pin->DefaultValue;
	Info.DefaultObjectPath = Pin->DefaultObject ? Pin->DefaultObject->GetPathName() : FString();
	Info.bIsReference = Pin->PinType.bIsReference;

	for (UEdGraphPin* Linked : Pin->LinkedTo)
	{
		if (!Linked)
		{
			continue;
		}
		if (const FString* LinkedId = NodeIdMap.Find(Linked->GetOwningNodeUnchecked()))
		{
			Info.LinkedTo.Add(*LinkedId + TEXT(".") + Linked->PinName.ToString());
		}
	}
	return Info;
}

void FBlueprintExtractor::AddTypedExtras(UEdGraphNode* Node, FBRNodeInfo& NodeInfo, const FString& GraphName, FBRExportModel& Model)
{
	if (UK2Node_CallFunction* CallFn = Cast<UK2Node_CallFunction>(Node))
	{
		NodeInfo.Extras.Add(TEXT("FunctionName"), CallFn->FunctionReference.GetMemberName().ToString());
		UClass* OwnerClass = CallFn->FunctionReference.GetMemberParentClass();
		NodeInfo.Extras.Add(TEXT("FunctionOwnerClass"), OwnerClass ? OwnerClass->GetName() : TEXT("Self"));
		NodeInfo.Extras.Add(TEXT("IsPure"), CallFn->IsNodePure() ? TEXT("true") : TEXT("false"));

		static const TSet<FString> StructuralFunctions = { TEXT("GetOwner"), TEXT("GetAttachParentActor"), TEXT("GetParentComponent"), TEXT("GetInstigator") };
		const FString FnName = CallFn->FunctionReference.GetMemberName().ToString();
		if (StructuralFunctions.Contains(FnName))
		{
			FBRCrossReference XRef;
			XRef.Kind = TEXT("structural");
			XRef.TargetClass = TEXT("Unknown");
			XRef.NodeId = NodeInfo.Id;
			XRef.GraphName = GraphName;
			XRef.bResolved = false;
			XRef.Note = FString::Printf(TEXT("call to %s - target class not statically known"), *FnName);
			Model.CrossReferences.Add(MoveTemp(XRef));
		}
	}
	else if (UK2Node_Variable* VarNode = Cast<UK2Node_Variable>(Node))
	{
		NodeInfo.Extras.Add(TEXT("VariableName"), VarNode->VariableReference.GetMemberName().ToString());
		UClass* OwnerClass = VarNode->VariableReference.GetMemberParentClass();
		NodeInfo.Extras.Add(TEXT("VariableOwnerClass"), OwnerClass ? OwnerClass->GetName() : TEXT("Self"));
		NodeInfo.Extras.Add(TEXT("AccessKind"), Node->IsA<UK2Node_VariableSet>() ? TEXT("Set") : TEXT("Get"));
	}
	else if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
	{
		const FName EventMemberName = EventNode->EventReference.GetMemberName();
		NodeInfo.Extras.Add(TEXT("EventName"), EventMemberName != NAME_None ? EventMemberName.ToString() : EventNode->CustomFunctionName.ToString());
		NodeInfo.Extras.Add(TEXT("IsOverride"), EventNode->bOverrideFunction ? TEXT("true") : TEXT("false"));

		const uint32 Flags = EventNode->FunctionFlags;
		TArray<FString> NetFlags;
		if (Flags & FUNC_NetMulticast) { NetFlags.Add(TEXT("Multicast")); }
		if (Flags & FUNC_NetServer)    { NetFlags.Add(TEXT("Server")); }
		if (Flags & FUNC_NetClient)    { NetFlags.Add(TEXT("Client")); }
		if (Flags & FUNC_NetReliable)  { NetFlags.Add(TEXT("Reliable")); }
		if (NetFlags.Num() > 0)
		{
			NodeInfo.Extras.Add(TEXT("Replication"), FString::Join(NetFlags, TEXT("|")));
		}
	}
	else if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
	{
		UClass* Target = CastNode->TargetType;
		NodeInfo.Extras.Add(TEXT("CastTargetClass"), Target ? Target->GetName() : TEXT("Unknown"));
		if (Target)
		{
			FBRCrossReference XRef;
			XRef.Kind = TEXT("cast");
			XRef.TargetClass = Target->GetName();
			XRef.NodeId = NodeInfo.Id;
			XRef.GraphName = GraphName;
			XRef.bResolved = true;
			Model.CrossReferences.Add(MoveTemp(XRef));
		}
	}
	else if (UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(Node))
	{
		if (UEdGraph* MacroGraph = MacroNode->GetMacroGraph())
		{
			NodeInfo.Extras.Add(TEXT("MacroName"), MacroGraph->GetName());
			if (const UObject* MacroOuter = MacroGraph->GetOuter())
			{
				NodeInfo.Extras.Add(TEXT("MacroSource"), MacroOuter->GetName());
			}
		}
	}
	else if (UK2Node_Composite* CompositeNode = Cast<UK2Node_Composite>(Node))
	{
		if (UEdGraph* Bound = CompositeNode->BoundGraph)
		{
			const FString SubGraphName = GraphName + TEXT("::") + Bound->GetName();
			NodeInfo.Extras.Add(TEXT("SubGraphName"), SubGraphName);
			ExtractGraph(Bound, TEXT("Collapsed"), SubGraphName, Model);
		}
	}
	else
	{
		Model.UnsupportedNodeTypes.AddUnique(Node->GetClass()->GetName());
	}
}

void FBlueprintExtractor::ScanPinsForLiteralReferences(FBRNodeInfo& NodeInfo, const FString& GraphName, FBRExportModel& Model)
{
	for (const FBRPinInfo& Pin : NodeInfo.Pins)
	{
		if (Pin.Direction != TEXT("input") || Pin.DefaultObjectPath.IsEmpty())
		{
			continue;
		}
		const bool bIsClassLike = (Pin.Category == TEXT("class") || Pin.Category == TEXT("softclass"));
		const bool bIsObjectLike = (Pin.Category == TEXT("object") || Pin.Category == TEXT("softobject") || Pin.Category == TEXT("interface"));
		if (!bIsClassLike && !bIsObjectLike)
		{
			continue;
		}

		const FString TargetName = LastPathComponent(Pin.DefaultObjectPath);
		if (TargetName.StartsWith(TEXT("Default__")))
		{
			// Implicit "self" target of a static/library function call (Class Default Object) -
			// not a meaningful project cross-reference. The calling class is already captured
			// via the node's "FunctionOwnerClass" extra.
			continue;
		}

		FString Kind = bIsClassLike ? TEXT("literal_class_reference") : TEXT("literal_object_reference");
		if (bIsClassLike)
		{
			if (NodeInfo.NodeType == TEXT("K2Node_SpawnActorFromClass"))
			{
				Kind = TEXT("spawn_actor");
			}
			else if (const FString* FnName = NodeInfo.Extras.Find(TEXT("FunctionName")))
			{
				if (*FnName == TEXT("GetAllActorsOfClass") || *FnName == TEXT("GetActorOfClass"))
				{
					Kind = TEXT("query_actors_of_class");
				}
			}
		}

		FBRCrossReference XRef;
		XRef.Kind = Kind;
		XRef.TargetClass = TargetName;
		XRef.NodeId = NodeInfo.Id;
		XRef.GraphName = GraphName;
		XRef.bResolved = true;
		XRef.Note = FString::Printf(TEXT("via pin '%s'"), *Pin.Name);
		Model.CrossReferences.Add(MoveTemp(XRef));
	}
}

void FBlueprintExtractor::ExtractGraph(UEdGraph* Graph, const FString& GraphType, const FString& GraphNameOverride, FBRExportModel& Model)
{
	if (!Graph)
	{
		return;
	}

	FBRGraphInfo GInfo;
	GInfo.Name = GraphNameOverride.IsEmpty() ? Graph->GetName() : GraphNameOverride;
	GInfo.Type = GraphType;

	TMap<UEdGraphNode*, FString> NodeIdMap;
	NodeIdMap.Reserve(Graph->Nodes.Num());
	for (int32 Index = 0; Index < Graph->Nodes.Num(); ++Index)
	{
		if (UEdGraphNode* N = Graph->Nodes[Index])
		{
			NodeIdMap.Add(N, FString::Printf(TEXT("N%d"), Index));
		}
	}

	for (int32 Index = 0; Index < Graph->Nodes.Num(); ++Index)
	{
		UEdGraphNode* Node = Graph->Nodes[Index];
		if (!Node)
		{
			continue;
		}

		if (UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(Node))
		{
			FBRCommentBoxInfo Box;
			Box.Text = CommentNode->NodeComment;
			for (UObject* Obj : CommentNode->GetNodesUnderComment())
			{
				if (UEdGraphNode* Contained = Cast<UEdGraphNode>(Obj))
				{
					if (const FString* Id = NodeIdMap.Find(Contained))
					{
						Box.ContainedNodeIds.Add(*Id);
					}
				}
			}
			GInfo.CommentBoxes.Add(MoveTemp(Box));
			continue;
		}

		FBRNodeInfo NodeInfo;
		NodeInfo.Id = NodeIdMap[Node];
		NodeInfo.NodeType = Node->GetClass()->GetName();
		NodeInfo.Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
		NodeInfo.Comment = Node->NodeComment;
		NodeInfo.PosX = static_cast<float>(Node->NodePosX);
		NodeInfo.PosY = static_cast<float>(Node->NodePosY);

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin)
			{
				continue;
			}
			NodeInfo.Pins.Add(BuildPinInfo(Pin, NodeIdMap));
		}

		AddTypedExtras(Node, NodeInfo, GInfo.Name, Model);
		ScanPinsForLiteralReferences(NodeInfo, GInfo.Name, Model);

		GInfo.Nodes.Add(MoveTemp(NodeInfo));
	}

	for (const FBRNodeInfo& N : GInfo.Nodes)
	{
		for (const FBRPinInfo& P : N.Pins)
		{
			if (P.Direction != TEXT("output"))
			{
				continue;
			}
			for (const FString& Target : P.LinkedTo)
			{
				FBRConnectionInfo Conn;
				Conn.From = N.Id + TEXT(".") + P.Name;
				Conn.To = Target;
				Conn.Kind = (P.Category == TEXT("exec")) ? TEXT("exec") : TEXT("data");
				GInfo.Connections.Add(MoveTemp(Conn));
			}
		}
	}

	Model.Graphs.Add(MoveTemp(GInfo));
}

void FBlueprintExtractor::ExtractVariables(UBlueprint* Blueprint, FBRExportModel& Model)
{
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		FBRVariableInfo Info;
		Info.Name = Var.VarName.ToString();
		FString UnusedValueSubCategory;
		ConvertPinType(Var.VarType, Info.TypeCategory, Info.TypeSubCategory, Info.ContainerType, UnusedValueSubCategory);
		Info.DefaultValue = Var.DefaultValue;
		Info.Category = Var.Category.ToString();
		if (Var.HasMetaData(TEXT("tooltip")))
		{
			Info.Tooltip = Var.GetMetaData(TEXT("tooltip"));
		}

		Info.bEditable = (Var.PropertyFlags & CPF_Edit) != 0 && (Var.PropertyFlags & CPF_DisableEditOnInstance) == 0;
		Info.bBlueprintReadOnly = (Var.PropertyFlags & CPF_BlueprintReadOnly) != 0;
		Info.bExposeOnSpawn = (Var.PropertyFlags & CPF_ExposeOnSpawn) != 0;
		Info.bPrivate = (Var.PropertyFlags & CPF_DisableEditOnInstance) != 0 && (Var.PropertyFlags & CPF_Edit) == 0;

		if ((Var.PropertyFlags & CPF_Net) != 0)
		{
			if (Var.RepNotifyFunc != NAME_None)
			{
				Info.Replication = TEXT("RepNotify");
				Info.RepNotifyFunctionName = Var.RepNotifyFunc.ToString();
			}
			else
			{
				Info.Replication = TEXT("Replicated");
			}
		}
		else
		{
			Info.Replication = TEXT("None");
		}

		if (!Info.TypeSubCategory.IsEmpty() &&
			(Info.TypeCategory == TEXT("object") || Info.TypeCategory == TEXT("class") ||
			 Info.TypeCategory == TEXT("softobject") || Info.TypeCategory == TEXT("softclass") ||
			 Info.TypeCategory == TEXT("interface")))
		{
			FBRCrossReference XRef;
			XRef.Kind = TEXT("variable_type");
			XRef.TargetClass = Info.TypeSubCategory;
			XRef.NodeId.Reset();
			XRef.GraphName.Reset();
			XRef.bResolved = true;
			XRef.Note = FString::Printf(TEXT("variable '%s'"), *Info.Name);
			Model.CrossReferences.Add(MoveTemp(XRef));
		}

		Model.Variables.Add(MoveTemp(Info));
	}
}

void FBlueprintExtractor::WalkSCSNode(USCS_Node* Node, const FString& ParentName, FBRExportModel& Model)
{
	if (!Node)
	{
		return;
	}

	FBRComponentInfo Info;
	Info.Name = Node->GetVariableName().ToString();
	Info.ComponentClass = Node->ComponentClass ? Node->ComponentClass->GetName() : TEXT("Unknown");
	Info.ParentComponentName = ParentName;
	Info.AttachSocket = Node->AttachToName.ToString();

	if (UActorComponent* Template = Node->ComponentTemplate)
	{
		UClass* CompClass = Template->GetClass();
		const UObject* Cdo = CompClass->GetDefaultObject();
		for (TFieldIterator<FProperty> PropIt(CompClass); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (!Property->HasAnyPropertyFlags(CPF_Edit))
			{
				continue;
			}
			if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_Deprecated))
			{
				continue;
			}
			if (Property->Identical_InContainer(Template, Cdo))
			{
				continue;
			}

			FString ValueStr;
			Property->ExportText_InContainer(0, ValueStr, Template, Template, nullptr, PPF_None);
			FBRPropertyOverride Override;
			Override.Name = Property->GetName();
			Override.Value = ValueStr;
			Info.PropertyOverrides.Add(MoveTemp(Override));
		}
	}

	Model.Components.Add(Info);

	if (Node->ComponentClass)
	{
		FBRCrossReference XRef;
		XRef.Kind = TEXT("component_class");
		XRef.TargetClass = Node->ComponentClass->GetName();
		XRef.NodeId.Reset();
		XRef.GraphName.Reset();
		XRef.bResolved = true;
		XRef.Note = FString::Printf(TEXT("component '%s'"), *Info.Name);
		Model.CrossReferences.Add(MoveTemp(XRef));
	}

	for (USCS_Node* Child : Node->GetChildNodes())
	{
		WalkSCSNode(Child, Info.Name, Model);
	}
}

void FBlueprintExtractor::ExtractComponents(UBlueprint* Blueprint, FBRExportModel& Model)
{
	if (!Blueprint->SimpleConstructionScript)
	{
		return;
	}
	for (USCS_Node* Root : Blueprint->SimpleConstructionScript->GetRootNodes())
	{
		WalkSCSNode(Root, FString(), Model);
	}
}

void FBlueprintExtractor::ExtractBaseCrossReferences(UBlueprint* Blueprint, FBRExportModel& Model)
{
	if (Blueprint->ParentClass)
	{
		FBRCrossReference XRef;
		XRef.Kind = TEXT("inherits");
		XRef.TargetClass = Blueprint->ParentClass->GetName();
		XRef.bResolved = true;
		Model.CrossReferences.Add(MoveTemp(XRef));
	}

	for (const FBPInterfaceDescription& Iface : Blueprint->ImplementedInterfaces)
	{
		if (Iface.Interface)
		{
			FBRCrossReference XRef;
			XRef.Kind = TEXT("implements_interface");
			XRef.TargetClass = Iface.Interface->GetName();
			XRef.bResolved = true;
			Model.CrossReferences.Add(MoveTemp(XRef));
		}
	}
}

void FBlueprintExtractor::ExtractTimelines(UBlueprint* Blueprint, FBRExportModel& Model)
{
	static const UEnum* LengthModeEnum = StaticEnum<ETimelineLengthMode>();

	for (const UTimelineTemplate* Timeline : Blueprint->Timelines)
	{
		if (!Timeline)
		{
			continue;
		}

		FBRTimelineInfo Info;
		Info.Name = Timeline->GetVariableName().ToString();
		Info.Length = Timeline->TimelineLength;
		Info.LengthMode = LengthModeEnum ? LengthModeEnum->GetNameStringByValue(static_cast<int64>(Timeline->LengthMode.GetValue())) : FString();
		Info.bAutoPlay = Timeline->bAutoPlay != 0;
		Info.bLoop = Timeline->bLoop != 0;
		Info.bReplicated = Timeline->bReplicated != 0;

		for (const FTTEventTrack& Track : Timeline->EventTracks)
		{
			FBRTimelineTrackInfo T;
			T.Kind = TEXT("Event");
			T.Name = Track.GetTrackName().ToString();
			T.BoundFunctionOrProperty = Track.GetFunctionName().ToString();
			T.CurveAsset = Track.CurveKeys ? Track.CurveKeys->GetName() : FString();
			Info.Tracks.Add(MoveTemp(T));
		}
		for (const FTTFloatTrack& Track : Timeline->FloatTracks)
		{
			FBRTimelineTrackInfo T;
			T.Kind = TEXT("Float");
			T.Name = Track.GetTrackName().ToString();
			T.BoundFunctionOrProperty = Track.GetPropertyName().ToString();
			T.CurveAsset = Track.CurveFloat ? Track.CurveFloat->GetName() : FString();
			Info.Tracks.Add(MoveTemp(T));
		}
		for (const FTTVectorTrack& Track : Timeline->VectorTracks)
		{
			FBRTimelineTrackInfo T;
			T.Kind = TEXT("Vector");
			T.Name = Track.GetTrackName().ToString();
			T.BoundFunctionOrProperty = Track.GetPropertyName().ToString();
			T.CurveAsset = Track.CurveVector ? Track.CurveVector->GetName() : FString();
			Info.Tracks.Add(MoveTemp(T));
		}
		for (const FTTLinearColorTrack& Track : Timeline->LinearColorTracks)
		{
			FBRTimelineTrackInfo T;
			T.Kind = TEXT("LinearColor");
			T.Name = Track.GetTrackName().ToString();
			T.BoundFunctionOrProperty = Track.GetPropertyName().ToString();
			T.CurveAsset = Track.CurveLinearColor ? Track.CurveLinearColor->GetName() : FString();
			Info.Tracks.Add(MoveTemp(T));
		}

		Model.Timelines.Add(MoveTemp(Info));
	}
}

FBRExportModel FBlueprintExtractor::Extract(UBlueprint* Blueprint)
{
	FBRExportModel Model;
	if (!Blueprint)
	{
		return Model;
	}

	Model.AssetPath = Blueprint->GetPathName();
	Model.ClassName = Blueprint->GetName();
	Model.ParentClass = Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : FString();

	static const UEnum* BPTypeEnum = StaticEnum<EBlueprintType>();
	Model.BlueprintType = BPTypeEnum ? BPTypeEnum->GetNameStringByValue(static_cast<int64>(Blueprint->BlueprintType.GetValue())) : FString();

	for (const FBPInterfaceDescription& Iface : Blueprint->ImplementedInterfaces)
	{
		if (Iface.Interface)
		{
			Model.Interfaces.Add(Iface.Interface->GetName());
		}
	}

	ExtractBaseCrossReferences(Blueprint, Model);
	ExtractVariables(Blueprint, Model);
	ExtractComponents(Blueprint, Model);
	ExtractTimelines(Blueprint, Model);

	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		ExtractGraph(Graph, TEXT("Ubergraph"), FString(), Model);
	}
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		ExtractGraph(Graph, TEXT("Function"), FString(), Model);
	}
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		ExtractGraph(Graph, TEXT("Macro"), FString(), Model);
	}
	for (const FBPInterfaceDescription& Iface : Blueprint->ImplementedInterfaces)
	{
		for (UEdGraph* Graph : Iface.Graphs)
		{
			ExtractGraph(Graph, TEXT("Function"), FString(), Model);
		}
	}

	return Model;
}
