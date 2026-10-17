// YuriNK, 2026, All Rights Reserved
#include "CubeGridMCPTool.h"

#include "CompositionOps/CubeGridBooleanOp.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "InteractiveToolChange.h"
#include "InteractiveToolManager.h"
#include "Materials/MaterialInterface.h"
#include "MeshOpPreviewHelpers.h"
#include "Operations/MeshPlaneCut.h"
#include "Properties/MeshMaterialProperties.h"
#include "PropertySets/CreateMeshObjectTypeProperties.h"

using namespace UE::Geometry;

namespace
{
	// Material painting modifies topology as well as attributes. Keep both mesh and material table
	// in one undo record, so material IDs remain meaningful after undo/redo.
	class FCubeGridPaintChange : public FToolCommandChange
	{
	public:
		TSharedPtr<FDynamicMesh3> Before, After;
		TArray<UMaterialInterface*> BeforeMaterials, AfterMaterials;
		virtual void Apply(UObject* Object) override
		{
			CastChecked<UCubeGridMCPTool>(Object)->ApplyMeshSnapshot(*After, AfterMaterials);
		}
		virtual void Revert(UObject* Object) override
		{
			CastChecked<UCubeGridMCPTool>(Object)->ApplyMeshSnapshot(*Before, BeforeMaterials);
		}
		virtual bool HasExpired(UObject* Object) const override
		{
			const auto* Tool = Cast<UCubeGridMCPTool>(Object);
			return !Tool || Tool->IsSessionClosed();
		}
		virtual FString ToString() const override { return TEXT("CubeGrid rectangle material"); }
	};

	ECubeGridFace FromNativeFace(FCubeGrid::EFaceDirection Face)
	{
		switch (Face)
		{
		case FCubeGrid::EFaceDirection::PositiveX: return ECubeGridFace::PositiveX;
		case FCubeGrid::EFaceDirection::NegativeX: return ECubeGridFace::NegativeX;
		case FCubeGrid::EFaceDirection::PositiveY: return ECubeGridFace::PositiveY;
		case FCubeGrid::EFaceDirection::NegativeY: return ECubeGridFace::NegativeY;
		case FCubeGrid::EFaceDirection::NegativeZ: return ECubeGridFace::NegativeZ;
		default: return ECubeGridFace::PositiveZ;
		}
	}
}

FCubeGrid::EFaceDirection UCubeGridMCPTool::ToNativeFace(ECubeGridFace Face)
{
	switch (Face)
	{
	case ECubeGridFace::PositiveX: return FCubeGrid::EFaceDirection::PositiveX;
	case ECubeGridFace::NegativeX: return FCubeGrid::EFaceDirection::NegativeX;
	case ECubeGridFace::PositiveY: return FCubeGrid::EFaceDirection::PositiveY;
	case ECubeGridFace::NegativeY: return FCubeGrid::EFaceDirection::NegativeY;
	case ECubeGridFace::NegativeZ: return FCubeGrid::EFaceDirection::NegativeZ;
	default: return FCubeGrid::EFaceDirection::PositiveZ;
	}
}

bool UCubeGridMCPTool::ValidateRectangle(FVector Min, FVector Max, ECubeGridFace Face, FString& Error)
{
	if (Min.ContainsNaN() || Max.ContainsNaN() || uint8(Face) > uint8(ECubeGridFace::NegativeZ))
	{
		Error = TEXT("Rectangle corners must be finite and Face must be a valid axis direction.");
		return false;
	}
	const int32 Flat = FCubeGrid::DirToFlatDim(ToNativeFace(Face));
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (FMath::Abs(Min[Axis]) > 1000000 || FMath::Abs(Max[Axis]) > 1000000 ||
			!FMath::IsNearlyEqual(Min[Axis], FMath::RoundToDouble(Min[Axis]), 1.e-6) ||
			!FMath::IsNearlyEqual(Max[Axis], FMath::RoundToDouble(Max[Axis]), 1.e-6))
		{
			Error = TEXT("Use integer grid-cell boundary coordinates within +/-1,000,000.");
			return false;
		}
		if ((Axis == Flat && Min[Axis] != Max[Axis]) || (Axis != Flat && Min[Axis] >= Max[Axis]))
		{
			Error = TEXT("The face-normal axis must be flat; both spanning axes must have MinCorner < MaxCorner.");
			return false;
		}
	}
	return true;
}

FCubeGridState UCubeGridMCPTool::ReadState() const
{
	FCubeGridState State;
	State.bActive = !bSessionClosed;
	State.bBusy = IsBusy();
	State.bCornerMode = IsInCornerMode();
	State.bHasSelection = bHaveSelection;
	if (Settings)
	{
		State.GridSize = Settings->CurrentBlockSize;
		State.GridOrigin = Settings->GridFrameOrigin;
		State.GridRotation = Settings->GridFrameOrientation;
	}
	if (bHaveSelection && CubeGrid)
	{
		State.SelectionMinCells = Selection.Box.Min / CubeGrid->GetCurrentGridCellSize();
		State.SelectionMaxCells = Selection.Box.Max / CubeGrid->GetCurrentGridCellSize();
		State.Face = FromNativeFace(Selection.Direction);
	}
	if (State.bCornerMode && CubeGrid)
	{
		for (int32 Index = 0; Index < 4; ++Index)
		{
			if (CornerSelectedFlags[Index]) State.SelectedCorners.Add(Index);
		}
		// The operator uses the same oriented box and corner numbering as the stock tool.
		auto Operator = const_cast<UCubeGridMCPTool*>(this)->MakeNewOperator();
		const auto* CubeOperator = static_cast<const FCubeGridBooleanOp*>(Operator.Get());
		if (CubeOperator)
		{
			for (int32 Index = 0; Index < 4; ++Index)
				State.CornerPositionsCells.Add(CubeGrid->ToGridPoint(CubeOperator->WorldBox.GetCorner(Index)));
		}
	}
	const FDynamicMesh3* Mesh = CurrentMesh.Get();
	FTransformSRT3d Transform = CurrentMeshTransform;
	if (State.bCornerMode && Preview && Preview->HaveValidResult())
	{
		Mesh = Preview->PreviewMesh->GetMesh();
		Transform = FTransformSRT3d(Preview->PreviewMesh->GetTransform());
	}
	if (Mesh && Mesh->TriangleCount() > 0)
	{
		State.TriangleCount = Mesh->TriangleCount();
		FAxisAlignedBox3d Bounds = FAxisAlignedBox3d::Empty();
		for (int32 Vertex : Mesh->VertexIndicesItr()) Bounds.Contain(Transform.TransformPosition(Mesh->GetVertex(Vertex)));
		State.MeshBoundsMin = Bounds.Min;
		State.MeshBoundsMax = Bounds.Max;
	}
	if (MaterialProperties)
	{
		State.UVScale = MaterialProperties->UVScale;
		State.bWorldSpaceUVScale = MaterialProperties->bWorldSpaceUVScale;
		if (MaterialProperties->Material.IsValid()) State.MaterialPath = MaterialProperties->Material->GetPathName();
	}
	return State;
}

bool UCubeGridMCPTool::IsBusy() const
{
	return PendingResult || bWaitingToApplyPreview || bBlockUntilPreviewUpdate ||
		PendingAction != ECubeGridToolAction::NoAction ||
		(bPreviewMayDiffer && (!IsInCornerMode() || !Preview || !Preview->HaveValidResult()));
}

void UCubeGridMCPTool::ChangeGridSize(double Size)
{
	ClearSelection(true);
	CubeGrid->SetCurrentGridCellSize(Size);
	Settings->CurrentBlockSize = CubeGrid->GetCurrentGridCellSize();
	Settings->BlockBaseSize = CubeGrid->GetBaseGridCellSize();
	Settings->SilentUpdateWatched();
	ResetMultiStepConsistencyData();
	UpdateGridLineSet();
	NotifyOfPropertyChangeByTool(Settings);
}

void UCubeGridMCPTool::Configure(double Size, FVector Origin, FRotator Rotation, ECubeGridOutput Output)
{
	UpdateGridTransform(FTransform(Rotation, Origin));
	Settings->bShowGrid = true;
	ChangeGridSize(Size);
	MaterialProperties->bWorldSpaceUVScale = true;
	MaterialProperties->UVScale = 1;
	MaterialProperties->SilentUpdateWatched();
	switch (Output)
	{
	case ECubeGridOutput::DynamicMesh: OutputTypeProperties->OutputType = UCreateMeshObjectTypeProperties::DynamicMeshActorIdentifier; break;
	case ECubeGridOutput::Volume: OutputTypeProperties->OutputType = UCreateMeshObjectTypeProperties::VolumeIdentifier; break;
	default: OutputTypeProperties->OutputType = UCreateMeshObjectTypeProperties::StaticMeshIdentifier; break;
	}
	OutputTypeProperties->UpdatePropertyVisibility();
	NotifyOfPropertyChangeByTool(OutputTypeProperties);
}

bool UCubeGridMCPTool::ChooseRectangle(FVector Min, FVector Max, ECubeGridFace Face, FString& Error)
{
	if (!ValidateRectangle(Min, Max, Face, Error)) return false;
	FSelection NewSelection;
	const double Size = CubeGrid->GetCurrentGridCellSize();
	NewSelection.Box = FAxisAlignedBox3d(Min * Size, Max * Size);
	NewSelection.StartBox = NewSelection.Box;
	NewSelection.Direction = ToNativeFace(Face);
	SetSelection(NewSelection, true);
	return true;
}

UCubeGridAsyncResult* UCubeGridMCPTool::BeginPendingResult()
{
	PendingResult = NewObject<UCubeGridAsyncResult>(this);
	OperationStartedAt = FPlatformTime::Seconds();
	return PendingResult;
}

UCubeGridAsyncResult* UCubeGridMCPTool::Extrude(int32 Blocks)
{
	auto* Result = BeginPendingResult();
	ApplyPushPull(Blocks);
	return Result;
}

void UCubeGridMCPTool::OnTick(float DeltaTime)
{
	Super::OnTick(DeltaTime);
	if (!PendingResult) return;
	const bool bSettled = PendingAction == ECubeGridToolAction::NoAction &&
		!bWaitingToApplyPreview && !bBlockUntilPreviewUpdate &&
		(IsInCornerMode() ? (Preview && Preview->HaveValidResult()) : !bPreviewMayDiffer);
	if (bSettled)
	{
		auto* Result = PendingResult.Get();
		PendingResult = nullptr;
		Result->SetValue(ReadState());
	}
	else if (FPlatformTime::Seconds() - OperationStartedAt > 60)
	{
		auto* Result = PendingResult.Get();
		PendingResult = nullptr;
		Result->SetError(TEXT("CubeGrid preview did not settle within 60 seconds. Inspect GetState; wait or CancelCubeGrid before retrying."));
	}
}

void UCubeGridMCPTool::Shutdown(EToolShutdownType ShutdownType)
{
	bSessionClosed = true;
	if (PendingResult)
	{
		auto* Result = PendingResult.Get();
		PendingResult = nullptr;
		Result->SetError(TEXT("CubeGrid was closed before its operation completed."));
	}
	Super::Shutdown(ShutdownType);
}

void UCubeGridMCPTool::ChangeUVScale(float Scale, bool bWorldSpace)
{
	MaterialProperties->UVScale = Scale;
	MaterialProperties->bWorldSpaceUVScale = bWorldSpace;
	MaterialProperties->SilentUpdateWatched();
	ResetMultiStepConsistencyData();
	NotifyOfPropertyChangeByTool(MaterialProperties);
}

UCubeGridAsyncResult* UCubeGridMCPTool::BeginCorners(const TArray<int32>& Corners, bool bCrosswise)
{
	auto* Result = BeginPendingResult();
	if (!IsInCornerMode()) StartCornerMode();
	Settings->bCrosswiseDiagonal = bCrosswise;
	Settings->SilentUpdateWatched();
	bool Flags[4] = {false, false, false, false};
	for (int32 Index : Corners) Flags[Index] = true;
	SetCornerSelection(Flags);
	InvalidatePreview();
	NotifyOfPropertyChangeByTool(Settings);
	return Result;
}

UCubeGridAsyncResult* UCubeGridMCPTool::EndCorners(bool bApply)
{
	auto* Result = BeginPendingResult();
	if (bApply) ApplyCornerMode();
	else CancelCornerMode();
	return Result;
}

void UCubeGridMCPTool::ApplyMeshSnapshot(const FDynamicMesh3& Mesh, const TArray<UMaterialInterface*>& Materials)
{
	Preview->CancelCompute();
	*CurrentMesh = Mesh;
	CurrentMeshMaterials = Materials;
	MeshSpatial->SetMesh(CurrentMesh.Get(), true);
	UpdateComputeInputs();
	ResetMultiStepConsistencyData();
	Preview->ConfigureMaterials(CurrentMeshMaterials, Preview->WorkingMaterial);
	Preview->PreviewMesh->UpdatePreview(CurrentMesh.Get());
	Preview->PreviewMesh->SetTransform(FTransform(CurrentMeshTransform));
	bPreviewMayDiffer = false;
	bWaitingToApplyPreview = false;
	bBlockUntilPreviewUpdate = false;
	SetChangesMade(true);
}

bool UCubeGridMCPTool::PaintRectangle(UMaterialInterface* Material, bool bPaintExisting, int32& Painted, FString& Error)
{
	Painted = 0;
	if (bPaintExisting)
	{
		if (!CurrentMesh || !CurrentMesh->HasAttributes() || !CurrentMesh->Attributes()->HasMaterialID())
		{
			Error = TEXT("No paintable mesh exists. Use bPaintExisting=false to set the material before extrusion.");
			return false;
		}
		auto After = MakeShared<FDynamicMesh3>(*CurrentMesh);
		const int32 Flat = FCubeGrid::DirToFlatDim(Selection.Direction);
		const int32 U = (Flat + 1) % 3, V = (Flat + 2) % 3;
		const double Tolerance = FMath::Max(1.e-5, CubeGrid->GetCurrentGridCellSize() * 1.e-6);
		const FVector3d GridNormal = FCubeGrid::DirToNormal(Selection.Direction);
		auto ToFrame = [this](FVector3d Position)
		{
			return CubeGrid->GetFrame().ToFramePoint(CurrentMeshTransform.TransformPosition(Position));
		};
		auto IsFace = [&](int32 Triangle)
		{
			FVector3d A, B, C;
			After->GetTriVertices(Triangle, A, B, C);
			A = ToFrame(A); B = ToFrame(B); C = ToFrame(C);
			return FMath::Abs(A[Flat] - Selection.Box.Min[Flat]) <= Tolerance &&
				FMath::Abs(B[Flat] - Selection.Box.Min[Flat]) <= Tolerance &&
				FMath::Abs(C[Flat] - Selection.Box.Min[Flat]) <= Tolerance &&
				((B-A).Cross(C-A)).Dot(GridNormal) > 0;
		};
		// Split only edges adjacent to coplanar face triangles. The geometry cutter interpolates
		// UVs/normals and preserves shared connectivity, including neighbors across a face seam.
		for (int32 Axis : {U, V})
		{
			for (double Boundary : {Selection.Box.Min[Axis], Selection.Box.Max[Axis]})
			{
				FVector3d PlanePoint = Selection.Box.Min;
				PlanePoint[Axis] = Boundary;
				FVector3d GridAxis = FVector3d::Zero(); GridAxis[Axis] = 1;
				const FVector3d WorldPoint = CubeGrid->GetFrame().FromFramePoint(PlanePoint);
				const FVector3d WorldNormal = CubeGrid->GetFrame().FromFrameVector(GridAxis);
				FVector3d MeshNormal = CurrentMeshTransform.GetRotation().Inverse() * WorldNormal;
				MeshNormal *= CurrentMeshTransform.GetScale();
				MeshNormal.Normalize();
				FMeshPlaneCut Cut(&After.Get(), CurrentMeshTransform.InverseTransformPosition(WorldPoint), MeshNormal);
				Cut.bCollapseDegenerateEdgesOnCut = false;
				Cut.EdgeFilterFunc = [&](int32 Edge)
				{
					const FIndex2i Tris = After->GetEdgeT(Edge);
					return (Tris.A >= 0 && IsFace(Tris.A)) || (Tris.B >= 0 && IsFace(Tris.B));
				};
				if (!Cut.SplitEdgesOnly(false, nullptr))
				{
					Error = TEXT("Could not split the surface at the rectangle boundary; the mesh was left unchanged.");
					return false;
				}
			}
		}
		TArray<UMaterialInterface*> Materials = CurrentMeshMaterials;
		int32 MaterialID = Materials.IndexOfByKey(Material);
		if (MaterialID == INDEX_NONE) MaterialID = Materials.Add(Material);
		for (int32 Triangle : After->TriangleIndicesItr())
		{
			if (!IsFace(Triangle)) continue;
			const FVector3d Center = ToFrame(After->GetTriCentroid(Triangle));
			if (Center[U] > Selection.Box.Min[U] - Tolerance && Center[U] < Selection.Box.Max[U] + Tolerance &&
				Center[V] > Selection.Box.Min[V] - Tolerance && Center[V] < Selection.Box.Max[V] + Tolerance)
			{
				After->Attributes()->GetMaterialID()->SetValue(Triangle, MaterialID);
				++Painted;
			}
		}
		if (Painted == 0)
		{
			Error = TEXT("No outward-facing coplanar triangles overlap this rectangle. Select an existing surface, or use bPaintExisting=false.");
			return false;
		}
		auto Change = MakeUnique<FCubeGridPaintChange>();
		Change->Before = MakeShared<FDynamicMesh3>(*CurrentMesh);
		Change->BeforeMaterials = CurrentMeshMaterials;
		Change->After = After;
		Change->AfterMaterials = Materials;
		ApplyMeshSnapshot(*After, Materials);
		GetToolManager()->EmitObjectChange(this, MoveTemp(Change), NSLOCTEXT("CubeGridToolset", "Paint", "Paint CubeGrid rectangle"));
	}
	MaterialProperties->Material = Material;
	MaterialProperties->SilentUpdateWatched();
	UpdateOpMaterials();
	ResetMultiStepConsistencyData();
	NotifyOfPropertyChangeByTool(MaterialProperties);
	return true;
}

bool UCubeGridMCPToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	return SceneState.World != nullptr;
}

UInteractiveTool* UCubeGridMCPToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	auto* Tool = NewObject<UCubeGridMCPTool>(SceneState.ToolManager);
	Tool->SetWorld(SceneState.World);
	// A null Target deliberately creates a fresh mesh, regardless of Editor selection.
	return Tool;
}
