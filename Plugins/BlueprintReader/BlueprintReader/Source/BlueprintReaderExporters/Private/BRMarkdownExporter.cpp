#include "BRMarkdownExporter.h"

#include "Misc/FileHelper.h"

namespace
{
	constexpr int32 MaxExprDepth = 3;
	constexpr int32 MaxPropertyOverrideValueLength = 200;

	FString Indent(int32 Depth)
	{
		return FString::ChrN(Depth * 2, TEXT(' '));
	}

	// Struct properties (PostProcessSettings, BodyInstance, ...) export their ENTIRE value as
	// text the moment a single nested field differs from default, which can run to thousands of
	// characters. Truncate for the human/LLM-readable Markdown; the full value stays in the JSON.
	FString TruncateForDisplay(const FString& Value)
	{
		if (Value.Len() <= MaxPropertyOverrideValueLength)
		{
			return Value;
		}
		return Value.Left(MaxPropertyOverrideValueLength) + TEXT("... (обрезано, полное значение см. в .bp.json)");
	}

	FString SplitNodeId(const FString& LinkedTo)
	{
		FString NodeId, PinName;
		LinkedTo.Split(TEXT("."), &NodeId, &PinName);
		return NodeId;
	}

	FString SplitPinName(const FString& LinkedTo)
	{
		FString NodeId, PinName;
		LinkedTo.Split(TEXT("."), &NodeId, &PinName);
		return PinName;
	}

	// Groups cross-references by (Kind, TargetClass) so repeated identical occurrences
	// (e.g. the same cast/spawn/library call appearing dozens of times) collapse into one
	// line with a count, instead of drowning the reader in duplicates. Full per-occurrence
	// detail (node id, graph, note) remains available in the JSON export.
	void AppendGroupedCrossReferences(TArray<FString>& Lines, const TArray<FBRCrossReference>& XRefs)
	{
		TMap<FString, int32> Counts;
		TMap<FString, FBRCrossReference> FirstSeen;
		TArray<FString> Order;
		for (const FBRCrossReference& XRef : XRefs)
		{
			const FString Key = XRef.Kind + TEXT("|") + XRef.TargetClass;
			if (!Counts.Contains(Key))
			{
				Order.Add(Key);
				FirstSeen.Add(Key, XRef);
			}
			Counts.FindOrAdd(Key)++;
		}

		for (const FString& Key : Order)
		{
			const FBRCrossReference& XRef = FirstSeen[Key];
			const int32 Count = Counts[Key];
			FString Line = FString::Printf(TEXT("- [%s] -> `%s`"), *XRef.Kind, *XRef.TargetClass);
			if (Count > 1) { Line += FString::Printf(TEXT(" (x%d)"), Count); }
			if (!XRef.bResolved) { Line += TEXT(" (динамическая/нерезолвленная)"); }
			Lines.Add(Line);
		}
	}

	// Renders logic for a single graph (EventGraph, a function, a macro, a collapsed sub-graph, ...).
	class FGraphRenderer
	{
	public:
		explicit FGraphRenderer(const FBRGraphInfo& InGraph)
			: Graph(InGraph)
		{
			for (const FBRNodeInfo& Node : Graph.Nodes)
			{
				NodeById.Add(Node.Id, &Node);
			}
		}

		FString Render()
		{
			TArray<FString> Lines;

			for (const FBRNodeInfo& Node : Graph.Nodes)
			{
				if (!IsEntryNode(Node))
				{
					continue;
				}
				Lines.Add(FString::Printf(TEXT("### %s"), *Node.Title));
				RenderedNodes.Add(Node.Id);

				bool bHasAnyTarget = false;
				for (const FBRPinInfo& Pin : Node.Pins)
				{
					if (Pin.Direction != TEXT("output") || Pin.Category != TEXT("exec"))
					{
						continue;
					}
					for (const FString& Target : Pin.LinkedTo)
					{
						bHasAnyTarget = true;
						RenderNode(SplitNodeId(Target), 1, Lines);
					}
				}
				if (!bHasAnyTarget)
				{
					Lines.Add(Indent(1) + TEXT("(empty)"));
				}
				Lines.Add(FString());
			}

			TArray<FString> DeadLines;
			for (const FBRNodeInfo& Node : Graph.Nodes)
			{
				if (RenderedNodes.Contains(Node.Id) || IsEntryNode(Node))
				{
					continue;
				}
				if (HasAnyExecPin(Node))
				{
					DeadLines.Add(FString::Printf(TEXT("- `%s` (%s): %s"), *Node.Id, *Node.NodeType, *Node.Title));
				}
			}
			if (DeadLines.Num() > 0)
			{
				Lines.Add(TEXT("### Недостижимый / несвязанный exec-код"));
				Lines.Append(DeadLines);
				Lines.Add(FString());
			}

			return FString::Join(Lines, TEXT("\n"));
		}

	private:
		const FBRGraphInfo& Graph;
		TMap<FString, const FBRNodeInfo*> NodeById;
		TSet<FString> RenderedNodes;

		static bool IsEntryNode(const FBRNodeInfo& Node)
		{
			if (Node.NodeType == TEXT("K2Node_Event")
				|| Node.NodeType == TEXT("K2Node_CustomEvent")
				|| Node.NodeType == TEXT("K2Node_FunctionEntry"))
			{
				return true;
			}

			// Collapsed graphs (Collapse Nodes / Collapse to Function) don't have an Event node -
			// execution enters through a K2Node_Tunnel instead. A tunnel that only has exec OUTPUTS
			// (and no exec input) is the "Inputs" entry tunnel; the "Outputs" exit tunnel is the
			// mirror image (exec input, no exec output) and must not be treated as an entry point.
			if (Node.NodeType == TEXT("K2Node_Tunnel"))
			{
				bool bHasExecOut = false;
				bool bHasExecIn = false;
				for (const FBRPinInfo& Pin : Node.Pins)
				{
					if (Pin.Category != TEXT("exec"))
					{
						continue;
					}
					if (Pin.Direction == TEXT("output")) { bHasExecOut = true; }
					else { bHasExecIn = true; }
				}
				return bHasExecOut && !bHasExecIn;
			}

			return false;
		}

		static bool HasAnyExecPin(const FBRNodeInfo& Node)
		{
			for (const FBRPinInfo& Pin : Node.Pins)
			{
				if (Pin.Category == TEXT("exec"))
				{
					return true;
				}
			}
			return false;
		}

		static FString PrimaryExecOutPinName(const FBRNodeInfo& Node)
		{
			for (const FBRPinInfo& Pin : Node.Pins)
			{
				if (Pin.Direction == TEXT("output") && Pin.Category == TEXT("exec"))
				{
					return Pin.Name;
				}
			}
			return FString();
		}

		static const FBRPinInfo* FindPin(const FBRNodeInfo& Node, const FString& PinName)
		{
			for (const FBRPinInfo& Pin : Node.Pins)
			{
				if (Pin.Name == PinName)
				{
					return &Pin;
				}
			}
			return nullptr;
		}

		static TArray<FString> GetExecTargets(const FBRNodeInfo& Node, const FString& PinName)
		{
			if (const FBRPinInfo* Pin = FindPin(Node, PinName))
			{
				return Pin->LinkedTo;
			}
			return {};
		}

		const FBRNodeInfo* Lookup(const FString& NodeId) const
		{
			const FBRNodeInfo* const* Found = NodeById.Find(NodeId);
			return Found ? *Found : nullptr;
		}

		FString DescribeValue(const FString& NodeId, const FString& PinName, int32 Depth) const
		{
			const FBRNodeInfo* Node = Lookup(NodeId);
			if (!Node)
			{
				return TEXT("<?>");
			}
			const FBRPinInfo* Pin = FindPin(*Node, PinName);
			if (!Pin)
			{
				return TEXT("<?>");
			}
			if (Pin->LinkedTo.Num() == 0)
			{
				if (!Pin->DefaultObjectPath.IsEmpty())
				{
					FString Left, Right;
					return Pin->DefaultObjectPath.Split(TEXT("."), &Left, &Right, ESearchCase::IgnoreCase, ESearchDir::FromEnd) ? Right : Pin->DefaultObjectPath;
				}
				return Pin->DefaultValue.IsEmpty() ? TEXT("<unset>") : Pin->DefaultValue;
			}

			const FString SourceNodeId = SplitNodeId(Pin->LinkedTo[0]);
			const FString SourcePinName = SplitPinName(Pin->LinkedTo[0]);
			const FBRNodeInfo* Source = Lookup(SourceNodeId);
			if (!Source)
			{
				return TEXT("<?>");
			}

			if (Source->NodeType == TEXT("K2Node_Knot"))
			{
				// reroute node: transparently follow its single input pin
				for (const FBRPinInfo& InPin : Source->Pins)
				{
					if (InPin.Direction == TEXT("input"))
					{
						return DescribeValue(SourceNodeId, InPin.Name, Depth);
					}
				}
				return TEXT("<?>");
			}

			if (Source->NodeType == TEXT("K2Node_VariableGet"))
			{
				const FString* VarName = Source->Extras.Find(TEXT("VariableName"));
				return VarName ? *VarName : Source->Title;
			}
			if (Source->NodeType == TEXT("K2Node_Self"))
			{
				return TEXT("self");
			}
			if (Depth <= 0)
			{
				return FString::Printf(TEXT("<%s>"), *Source->Title);
			}
			if (Source->NodeType == TEXT("K2Node_CallFunction"))
			{
				const FString* FnName = Source->Extras.Find(TEXT("FunctionName"));
				return FString::Printf(TEXT("%s(%s)"), FnName ? **FnName : *Source->Title, *BuildArgsString(*Source, Depth - 1));
			}
			return FString::Printf(TEXT("<%s>"), *Source->Title);
		}

		FString BuildArgsString(const FBRNodeInfo& Node, int32 Depth) const
		{
			TArray<FString> Args;
			for (const FBRPinInfo& Pin : Node.Pins)
			{
				if (Pin.Direction != TEXT("input") || Pin.Category == TEXT("exec"))
				{
					continue;
				}
				Args.Add(FString::Printf(TEXT("%s=%s"), *Pin.Name, *DescribeValue(Node.Id, Pin.Name, Depth)));
			}
			return FString::Join(Args, TEXT(", "));
		}

		FString DescribeStatement(const FBRNodeInfo& Node) const
		{
			if (Node.NodeType == TEXT("K2Node_CallFunction"))
			{
				const FString* FnName = Node.Extras.Find(TEXT("FunctionName"));
				return FString::Printf(TEXT("Call %s(%s)"), FnName ? **FnName : *Node.Title, *BuildArgsString(Node, MaxExprDepth));
			}
			if (Node.NodeType == TEXT("K2Node_VariableSet"))
			{
				const FString* VarName = Node.Extras.Find(TEXT("VariableName"));
				FString ValueExpr = TEXT("<unset>");
				for (const FBRPinInfo& Pin : Node.Pins)
				{
					if (Pin.Direction == TEXT("input") && Pin.Category != TEXT("exec"))
					{
						ValueExpr = DescribeValue(Node.Id, Pin.Name, MaxExprDepth);
						break;
					}
				}
				return FString::Printf(TEXT("Set %s = %s"), VarName ? **VarName : *Node.Title, *ValueExpr);
			}
			if (Node.NodeType == TEXT("K2Node_DynamicCast") || Node.NodeType == TEXT("K2Node_ClassDynamicCast"))
			{
				const FString* TargetClass = Node.Extras.Find(TEXT("CastTargetClass"));
				FString ObjectExpr = TEXT("<?>");
				for (const FBRPinInfo& Pin : Node.Pins)
				{
					if (Pin.Direction == TEXT("input") && Pin.Category != TEXT("exec"))
					{
						ObjectExpr = DescribeValue(Node.Id, Pin.Name, MaxExprDepth);
						break;
					}
				}
				return FString::Printf(TEXT("Cast %s to %s"), *ObjectExpr, TargetClass ? **TargetClass : TEXT("?"));
			}
			return FString::Printf(TEXT("%s(%s)"), *Node.Title, *BuildArgsString(Node, MaxExprDepth));
		}

		void RenderNode(const FString& NodeId, int32 Depth, TArray<FString>& Lines)
		{
			if (RenderedNodes.Contains(NodeId))
			{
				const FBRNodeInfo* Node = Lookup(NodeId);
				Lines.Add(Indent(Depth) + FString::Printf(TEXT("-> (продолжение см. выше: %s)"), Node ? *Node->Title : *NodeId));
				return;
			}
			const FBRNodeInfo* Node = Lookup(NodeId);
			if (!Node)
			{
				Lines.Add(Indent(Depth) + TEXT("-> (неизвестный узел)"));
				return;
			}
			RenderedNodes.Add(NodeId);

			if (Node->NodeType == TEXT("K2Node_Knot"))
			{
				for (const FBRPinInfo& Pin : Node->Pins)
				{
					if (Pin.Direction == TEXT("output"))
					{
						for (const FString& Target : Pin.LinkedTo)
						{
							RenderNode(SplitNodeId(Target), Depth, Lines);
						}
					}
				}
				return;
			}

			if (Node->NodeType == TEXT("K2Node_IfThenElse"))
			{
				Lines.Add(Indent(Depth) + FString::Printf(TEXT("IF %s:"), *DescribeValue(NodeId, TEXT("Condition"), MaxExprDepth)));
				const TArray<FString> ThenTargets = GetExecTargets(*Node, TEXT("then"));
				if (ThenTargets.Num() == 0) { Lines.Add(Indent(Depth + 1) + TEXT("(nothing)")); }
				for (const FString& T : ThenTargets) { RenderNode(SplitNodeId(T), Depth + 1, Lines); }

				Lines.Add(Indent(Depth) + TEXT("ELSE:"));
				const TArray<FString> ElseTargets = GetExecTargets(*Node, TEXT("else"));
				if (ElseTargets.Num() == 0) { Lines.Add(Indent(Depth + 1) + TEXT("(nothing)")); }
				for (const FString& T : ElseTargets) { RenderNode(SplitNodeId(T), Depth + 1, Lines); }
				return;
			}

			if (Node->NodeType == TEXT("K2Node_ExecutionSequence"))
			{
				Lines.Add(Indent(Depth) + TEXT("SEQUENCE:"));
				int32 BranchIndex = 0;
				for (const FBRPinInfo& Pin : Node->Pins)
				{
					if (Pin.Direction != TEXT("output") || Pin.Category != TEXT("exec"))
					{
						continue;
					}
					++BranchIndex;
					Lines.Add(Indent(Depth + 1) + FString::Printf(TEXT("%d."), BranchIndex));
					if (Pin.LinkedTo.Num() == 0) { Lines.Add(Indent(Depth + 2) + TEXT("(nothing)")); }
					for (const FString& T : Pin.LinkedTo) { RenderNode(SplitNodeId(T), Depth + 2, Lines); }
				}
				return;
			}

			if (Node->NodeType.StartsWith(TEXT("K2Node_Switch")))
			{
				Lines.Add(Indent(Depth) + TEXT("SWITCH:"));
				for (const FBRPinInfo& Pin : Node->Pins)
				{
					if (Pin.Direction != TEXT("output") || Pin.Category != TEXT("exec"))
					{
						continue;
					}
					Lines.Add(Indent(Depth + 1) + FString::Printf(TEXT("CASE %s:"), *Pin.Name));
					if (Pin.LinkedTo.Num() == 0) { Lines.Add(Indent(Depth + 2) + TEXT("(nothing)")); }
					for (const FString& T : Pin.LinkedTo) { RenderNode(SplitNodeId(T), Depth + 2, Lines); }
				}
				return;
			}

			// Generic single statement, then continue sequentially.
			Lines.Add(Indent(Depth) + DescribeStatement(*Node));
			const TArray<FString> NextTargets = GetExecTargets(*Node, PrimaryExecOutPinName(*Node));
			for (const FString& T : NextTargets)
			{
				RenderNode(SplitNodeId(T), Depth, Lines);
			}
		}
	};
}

FString FBRMarkdownExporter::ToMarkdownString(const FBRExportModel& Model)
{
	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("# %s"), *Model.ClassName));
	Lines.Add(FString::Printf(TEXT("Путь: `%s`"), *Model.AssetPath));
	if (!Model.ParentClass.IsEmpty())
	{
		Lines.Add(FString::Printf(TEXT("Родитель: `%s`"), *Model.ParentClass));
	}
	if (Model.Interfaces.Num() > 0)
	{
		Lines.Add(FString::Printf(TEXT("Интерфейсы: %s"), *FString::Join(Model.Interfaces, TEXT(", "))));
	}
	Lines.Add(FString());

	if (Model.Variables.Num() > 0)
	{
		Lines.Add(TEXT("## Переменные"));
		for (const FBRVariableInfo& Var : Model.Variables)
		{
			FString TypeStr = Var.TypeSubCategory.IsEmpty() ? Var.TypeCategory : Var.TypeSubCategory;
			if (Var.ContainerType != TEXT("None"))
			{
				TypeStr = FString::Printf(TEXT("%s<%s>"), *Var.ContainerType, *TypeStr);
			}
			FString Flags;
			if (Var.bEditable) { Flags += TEXT("Instance Editable, "); }
			if (Var.bExposeOnSpawn) { Flags += TEXT("Expose on Spawn, "); }
			if (Var.Replication != TEXT("None")) { Flags += FString::Printf(TEXT("Replicated: %s, "), *Var.Replication); }
			Flags.RemoveFromEnd(TEXT(", "));

			FString Line = FString::Printf(TEXT("- **%s**: %s"), *Var.Name, *TypeStr);
			if (!Var.DefaultValue.IsEmpty()) { Line += FString::Printf(TEXT(" = %s"), *Var.DefaultValue); }
			if (!Flags.IsEmpty()) { Line += FString::Printf(TEXT(" [%s]"), *Flags); }
			Lines.Add(Line);
		}
		Lines.Add(FString());
	}

	if (Model.Components.Num() > 0)
	{
		Lines.Add(TEXT("## Компоненты"));
		for (const FBRComponentInfo& Comp : Model.Components)
		{
			FString Line = FString::Printf(TEXT("- %s: `%s`"), *Comp.Name, *Comp.ComponentClass);
			if (!Comp.ParentComponentName.IsEmpty()) { Line += FString::Printf(TEXT(" (parent: %s)"), *Comp.ParentComponentName); }
			Lines.Add(Line);
			for (const FBRPropertyOverride& Override : Comp.PropertyOverrides)
			{
				Lines.Add(FString::Printf(TEXT("  - %s = %s"), *Override.Name, *TruncateForDisplay(Override.Value)));
			}
		}
		Lines.Add(FString());
	}

	if (Model.Timelines.Num() > 0)
	{
		Lines.Add(TEXT("## Таймлайны"));
		for (const FBRTimelineInfo& Timeline : Model.Timelines)
		{
			FString Flags;
			if (Timeline.bAutoPlay) { Flags += TEXT("AutoPlay, "); }
			if (Timeline.bLoop) { Flags += TEXT("Loop, "); }
			if (Timeline.bReplicated) { Flags += TEXT("Replicated, "); }
			Flags.RemoveFromEnd(TEXT(", "));

			FString Line = FString::Printf(TEXT("- **%s**: length=%.2f (%s)"), *Timeline.Name, Timeline.Length, *Timeline.LengthMode);
			if (!Flags.IsEmpty()) { Line += FString::Printf(TEXT(" [%s]"), *Flags); }
			Lines.Add(Line);

			for (const FBRTimelineTrackInfo& Track : Timeline.Tracks)
			{
				FString TrackLine = FString::Printf(TEXT("  - [%s] %s"), *Track.Kind, *Track.Name);
				if (!Track.BoundFunctionOrProperty.IsEmpty()) { TrackLine += FString::Printf(TEXT(" -> %s"), *Track.BoundFunctionOrProperty); }
				if (!Track.CurveAsset.IsEmpty()) { TrackLine += FString::Printf(TEXT(" (curve: %s)"), *Track.CurveAsset); }
				Lines.Add(TrackLine);
			}
		}
		Lines.Add(FString());
	}

	for (const FBRGraphInfo& Graph : Model.Graphs)
	{
		if (Graph.Type == TEXT("Collapsed"))
		{
			continue; // rendered inline where referenced would be nicer; kept as its own section for MVP simplicity
		}
		Lines.Add(FString::Printf(TEXT("## %s (%s)"), *Graph.Name, *Graph.Type));
		FGraphRenderer Renderer(Graph);
		Lines.Add(Renderer.Render());
	}

	for (const FBRGraphInfo& Graph : Model.Graphs)
	{
		if (Graph.Type != TEXT("Collapsed"))
		{
			continue;
		}
		Lines.Add(FString::Printf(TEXT("## %s (свёрнутый граф)"), *Graph.Name));
		FGraphRenderer Renderer(Graph);
		Lines.Add(Renderer.Render());
	}

	if (Model.CrossReferences.Num() > 0)
	{
		Lines.Add(TEXT("## Связи с другими классами"));
		AppendGroupedCrossReferences(Lines, Model.CrossReferences);
		Lines.Add(FString());
	}

	return FString::Join(Lines, TEXT("\n"));
}

bool FBRMarkdownExporter::ExportToFile(const FBRExportModel& Model, const FString& FilePath)
{
	const FString Markdown = ToMarkdownString(Model);
	return FFileHelper::SaveStringToFile(Markdown, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

FString FBRMarkdownExporter::ProjectSummaryToMarkdownString(const TArray<FBRExportModel>& Models)
{
	TArray<FString> Lines;
	Lines.Add(TEXT("# Карта связей Blueprint-проекта"));
	Lines.Add(FString::Printf(TEXT("Экспортировано классов: %d"), Models.Num()));
	Lines.Add(FString());

	for (const FBRExportModel& Model : Models)
	{
		if (Model.CrossReferences.Num() == 0)
		{
			continue;
		}
		Lines.Add(FString::Printf(TEXT("## %s"), *Model.ClassName));
		AppendGroupedCrossReferences(Lines, Model.CrossReferences);
		Lines.Add(FString());
	}

	return FString::Join(Lines, TEXT("\n"));
}

bool FBRMarkdownExporter::ExportProjectSummaryToFile(const TArray<FBRExportModel>& Models, const FString& FilePath)
{
	const FString Markdown = ProjectSummaryToMarkdownString(Models);
	return FFileHelper::SaveStringToFile(Markdown, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
