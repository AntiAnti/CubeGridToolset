// YuriNK, 2026, All Rights Reserved
#include "CubeGridToolset.h"
#include "CubeGridMCPTool.h"

#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "InteractiveToolManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInterface.h"
#include "ModelingToolsEditorMode.h"
#include "Tools/UEdMode.h"
#include "UObject/UObjectIterator.h"

namespace
{
	const FString ToolIdentifier = TEXT("CubeGridMCP");

	void RaiseToolError(const FString& Message) { UKismetSystemLibrary::RaiseScriptError(Message); }

	UInteractiveToolManager* GetManager()
	{
		if (!GEditor) return nullptr;
		UEdMode* Mode = GLevelEditorModeTools().GetActiveScriptableMode(UModelingToolsEditorMode::EM_ModelingToolsEditorModeId);
		return Mode ? Mode->GetToolManager() : nullptr;
	}

	UCubeGridMCPTool* GetTool(bool bRequireReady = true, bool bAllowCorners = true)
	{
		if (!GEditor || GEditor->PlayWorld)
		{
			RaiseToolError(TEXT("CubeGrid requires the Editor outside Play In Editor.")); return nullptr;
		}
		auto* Manager = GetManager();
		auto* Tool = Manager ? Cast<UCubeGridMCPTool>(Manager->GetActiveTool(EToolSide::Left)) : nullptr;
		if (!Tool)
		{
			RaiseToolError(TEXT("No CubeGrid MCP session is active. Call StartCubeGrid first.")); return nullptr;
		}
		if (bRequireReady && Tool->IsBusy())
		{
			RaiseToolError(TEXT("CubeGrid is computing a preview. Await the previous tool call, or inspect GetState.")); return nullptr;
		}
		if (!bAllowCorners && Tool->IsInCornerMode())
		{
			RaiseToolError(TEXT("LeaveCornerMode before changing the grid, selection, UV scale, material, or confirming the mesh.")); return nullptr;
		}
		return Tool;
	}

	UCubeGridAsyncResult* FailedAsync(const FString& Message)
	{
		auto* Result = NewObject<UCubeGridAsyncResult>();
		Result->SetError(Message);
		return Result;
	}

	UCubeGridAsyncResult* Extrude(int32 Blocks, bool bPull)
	{
		if (Blocks < 1 || Blocks > 1000000)
			return FailedAsync(TEXT("Blocks must be between 1 and 1,000,000."));
		auto* Tool = GetTool();
		if (!Tool) return FailedAsync(TEXT("CubeGrid is not ready for extrusion."));
		if (!Tool->ReadState().bHasSelection) return FailedAsync(TEXT("SelectRectangle before pushing or pulling."));
		return Tool->Extrude(bPull ? Blocks : -Blocks);
	}
}

FCubeGridState UCubeGridToolset::StartCubeGrid(double GridSize, FVector Origin, FRotator Rotation, ECubeGridOutput OutputType)
{
	if (!GEditor || GEditor->PlayWorld || !GEditor->GetEditorWorldContext().World())
	{
		RaiseToolError(TEXT("StartCubeGrid requires an Editor world outside Play In Editor.")); return {};
	}
	if (!FMath::IsFinite(GridSize) || GridSize < 0.001 || GridSize > 1000000 ||
		Origin.ContainsNaN() || Rotation.ContainsNaN() || uint8(OutputType) > uint8(ECubeGridOutput::Volume))
	{
		RaiseToolError(TEXT("Provide a finite grid size in [0.001, 1,000,000] centimeters, finite origin/rotation, and valid output type.")); return {};
	}
	// Do not switch modes if another mode has an active tool: mode switching can commit/cancel it.
	for (TObjectIterator<UEdMode> It; It; ++It)
	{
		UEdMode* Mode = *It;
		if (!Mode->IsTemplate() && GLevelEditorModeTools().GetActiveScriptableMode(Mode->GetID()) == Mode &&
			Mode->GetToolManager() && Mode->GetToolManager()->HasAnyActiveTool())
		{
			RaiseToolError(TEXT("An Editor modeling tool is already active. Finish or cancel that tool before starting CubeGrid.")); return {};
		}
	}
	GLevelEditorModeTools().ActivateMode(UModelingToolsEditorMode::EM_ModelingToolsEditorModeId);
	auto* Manager = GetManager();
	if (!Manager || Manager->HasAnyActiveTool())
	{
		RaiseToolError(TEXT("Modeling Mode could not provide an idle tool manager.")); return {};
	}
	Manager->RegisterToolType(ToolIdentifier, NewObject<UCubeGridMCPToolBuilder>(Manager));
	if (!Manager->SelectActiveToolType(EToolSide::Left, ToolIdentifier) || !Manager->ActivateTool(EToolSide::Left))
	{
		RaiseToolError(TEXT("Could not activate the native CubeGrid tool.")); return {};
	}
	auto* Tool = Cast<UCubeGridMCPTool>(Manager->GetActiveTool(EToolSide::Left));
	if (!Tool)
	{
		RaiseToolError(TEXT("The tool manager did not create the CubeGrid MCP tool.")); return {};
	}
	Tool->Configure(GridSize, Origin, Rotation, OutputType);
	return Tool->ReadState();
}

FCubeGridState UCubeGridToolset::GetState()
{
	auto* Manager = GetManager();
	auto* Tool = Manager ? Cast<UCubeGridMCPTool>(Manager->GetActiveTool(EToolSide::Left)) : nullptr;
	return Tool ? Tool->ReadState() : FCubeGridState();
}

FCubeGridState UCubeGridToolset::SetGridSize(double GridSize)
{
	if (!FMath::IsFinite(GridSize) || GridSize < 0.001 || GridSize > 1000000)
	{
		RaiseToolError(TEXT("GridSize must be finite and in [0.001, 1,000,000] centimeters.")); return {};
	}
	auto* Tool = GetTool(true, false);
	if (!Tool) return {};
	Tool->ChangeGridSize(GridSize);
	return Tool->ReadState();
}

FCubeGridState UCubeGridToolset::SelectRectangle(FVector MinCorner, FVector MaxCorner, ECubeGridFace Face)
{
	auto* Tool = GetTool(true, false);
	if (!Tool) return {};
	FString Message;
	if (!Tool->ChooseRectangle(MinCorner, MaxCorner, Face, Message)) { RaiseToolError(Message); return {}; }
	return Tool->ReadState();
}

UCubeGridAsyncResult* UCubeGridToolset::PullRectangle(int32 Blocks) { return Extrude(Blocks, true); }
UCubeGridAsyncResult* UCubeGridToolset::PushRectangle(int32 Blocks) { return Extrude(Blocks, false); }

FCubeGridState UCubeGridToolset::SetMaterialUVScale(float Scale, bool bWorldSpace)
{
	if (!FMath::IsFinite(Scale) || Scale <= 0)
	{
		RaiseToolError(TEXT("UV Scale must be positive and finite.")); return {};
	}
	auto* Tool = GetTool(true, false);
	if (!Tool) return {};
	Tool->ChangeUVScale(Scale, bWorldSpace);
	return Tool->ReadState();
}

FCubeGridState UCubeGridToolset::ApplyMaterialToRectangle(const FString& MaterialPath, bool bPaintExisting)
{
	auto* Tool = GetTool(true, false);
	if (!Tool) return {};
	if (!Tool->ReadState().bHasSelection) { RaiseToolError(TEXT("SelectRectangle before assigning its material.")); return {}; }
	// LoadObject accepts both asset object paths and package paths.
	auto* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
	if (!Material) { RaiseToolError(TEXT("MaterialPath did not resolve to a Material or MaterialInstance asset.")); return {}; }
	int32 Painted = 0;
	FString Message;
	if (!Tool->PaintRectangle(Material, bPaintExisting, Painted, Message)) { RaiseToolError(Message); return {}; }
	auto State = Tool->ReadState();
	State.PaintedTriangles = Painted;
	return State;
}

UCubeGridAsyncResult* UCubeGridToolset::EnterCornerMode(const TArray<int32>& Corners, bool bCrosswiseDiagonal)
{
	for (int32 Index : Corners)
		if (Index < 0 || Index > 3) return FailedAsync(TEXT("Corner indices must be in [0, 3]."));
	auto* Tool = GetTool();
	if (!Tool) return FailedAsync(TEXT("CubeGrid is not ready for corner mode."));
	if (!Tool->ReadState().bHasSelection) return FailedAsync(TEXT("SelectRectangle before entering corner mode."));
	return Tool->BeginCorners(Corners, bCrosswiseDiagonal);
}

UCubeGridAsyncResult* UCubeGridToolset::LeaveCornerMode(bool bApply)
{
	auto* Tool = GetTool();
	if (!Tool) return FailedAsync(TEXT("CubeGrid is not ready to leave corner mode."));
	if (!Tool->IsInCornerMode()) return FailedAsync(TEXT("CubeGrid is not in corner mode."));
	return Tool->EndCorners(bApply);
}

FCubeGridState UCubeGridToolset::ConfirmMesh(const FString& ActorLabel)
{
	auto* Tool = GetTool(true, false);
	if (!Tool) return {};
	auto State = Tool->ReadState();
	if (State.TriangleCount == 0) { RaiseToolError(TEXT("There is no geometry to confirm. Pull a rectangle or CancelCubeGrid.")); return {}; }
	UWorld* World = GEditor->GetEditorWorldContext().World();
	TSet<AActor*> Existing;
	for (TActorIterator<AActor> It(World); It; ++It) Existing.Add(*It);
	GetManager()->DeactivateTool(EToolSide::Left, EToolShutdownType::Accept);
	State.bActive = false;
	State.bBusy = false;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Existing.Contains(Actor) || Actor->HasAnyFlags(RF_Transient)) continue;
		if (!ActorLabel.IsEmpty()) Actor->SetActorLabel(ActorLabel);
		State.CreatedActorPaths.Add(Actor->GetPathName());
		TArray<UStaticMeshComponent*> Components;
		Actor->GetComponents(Components);
		for (auto* Component : Components)
			if (Component->GetStaticMesh()) State.CreatedAssetPaths.AddUnique(Component->GetStaticMesh()->GetPathName());
	}
	if (State.CreatedActorPaths.IsEmpty())
		RaiseToolError(TEXT("CubeGrid closed but no output actor was created. Check the Modeling Mode asset creation settings and Output Log."));
	return State;
}

FCubeGridState UCubeGridToolset::CancelCubeGrid()
{
	auto* Tool = GetTool(false);
	if (!Tool) return {};
	GetManager()->DeactivateTool(EToolSide::Left, EToolShutdownType::Cancel);
	return {};
}
