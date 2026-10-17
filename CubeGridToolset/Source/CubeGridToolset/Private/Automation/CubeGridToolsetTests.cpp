// YuriNK, 2026, All Rights Reserved
#if WITH_DEV_AUTOMATION_TESTS

#include "CubeGridMCPTool.h"
#include "CubeGridToolset.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "InteractiveToolManager.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "ModelingToolsEditorMode.h"
#include "Tests/AutomationEditorCommon.h"
#include "ToolsetRegistry/ToolCallExceptionHandler.h"
#include "Tools/UEdMode.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCubeGridRectangleValidation, "CubeGridToolset.RectangleValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCubeGridRectangleValidation::RunTest(const FString& Parameters)
{
	FString Error;
	for (ECubeGridFace Face : {ECubeGridFace::PositiveX, ECubeGridFace::NegativeX,
		ECubeGridFace::PositiveY, ECubeGridFace::NegativeY, ECubeGridFace::PositiveZ, ECubeGridFace::NegativeZ})
	{
		FVector Min(-3, -2, -1), Max(4, 5, 6);
		int32 Flat = UE::Geometry::FCubeGrid::DirToFlatDim(UCubeGridMCPTool::ToNativeFace(Face));
		Max[Flat] = Min[Flat];
		TestTrue(TEXT("Valid rectangle on every signed axis"), UCubeGridMCPTool::ValidateRectangle(Min, Max, Face, Error));
	}
	TestFalse(TEXT("Nonplanar rectangle rejected"), UCubeGridMCPTool::ValidateRectangle(FVector::ZeroVector, FVector(1,1,1), ECubeGridFace::PositiveZ, Error));
	TestFalse(TEXT("Degenerate rectangle rejected"), UCubeGridMCPTool::ValidateRectangle(FVector::ZeroVector, FVector(0,1,0), ECubeGridFace::PositiveZ, Error));
	TestFalse(TEXT("Reversed rectangle rejected"), UCubeGridMCPTool::ValidateRectangle(FVector(1,0,0), FVector(0,1,0), ECubeGridFace::PositiveZ, Error));
	TestFalse(TEXT("Fractional cells rejected"), UCubeGridMCPTool::ValidateRectangle(FVector::ZeroVector, FVector(1.5,1,0), ECubeGridFace::PositiveZ, Error));
	return true;
}

// Runs in a disposable Editor map. Uses the real tool manager, boolean worker, material cutter,
// corner preview, and dynamic output actor. No project assets or working maps are saved.
class FCubeGridGeometryCommand : public IAutomationLatentCommand
{
public:
	explicit FCubeGridGeometryCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		if (Pending.IsValid())
		{
			if (!Pending->bIsComplete) return false;
			if (!Pending->Error.IsEmpty()) return Fail(Pending->Error);
			Pending.Reset();
		}
		if (Step > 0 && Step < 14 && !Tool.IsValid()) return Fail(TEXT("CubeGrid tool unexpectedly disappeared."));
		switch (Step++)
		{
		case 0:
		{
			const auto State = UCubeGridToolset::StartCubeGrid(100, FVector(1000,2000,3000), FRotator::ZeroRotator, ECubeGridOutput::DynamicMesh);
			if (!State.bActive) return Fail(TEXT("Could not start CubeGrid."));
			auto* Mode = GLevelEditorModeTools().GetActiveScriptableMode(UModelingToolsEditorMode::EM_ModelingToolsEditorModeId);
			Tool = Cast<UCubeGridMCPTool>(Mode->GetToolManager()->GetActiveTool(EToolSide::Left));
			UCubeGridToolset::SelectRectangle(FVector(0,0,0), FVector(4,4,0), ECubeGridFace::PositiveZ);
			UCubeGridToolset::ApplyMaterialToRectangle(TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"), false);
			Pending.Reset(UCubeGridToolset::PullRectangle(2));
			break;
		}
		case 1:
		{
			Test->TestTrue(TEXT("Pull creates the expected volume"), FMath::IsNearlyEqual(Volume(), 32000000.0, 1.0));
			auto State = UCubeGridToolset::GetState();
			Test->TestTrue(TEXT("World-space minimum bounds"), State.MeshBoundsMin.Equals(FVector(1000,2000,3000), 0.001));
			Test->TestTrue(TEXT("World-space maximum bounds"), State.MeshBoundsMax.Equals(FVector(1400,2400,3200), 0.001));
			UCubeGridToolset::SetMaterialUVScale(2, true);
			UCubeGridToolset::SelectRectangle(FVector(1,1,2), FVector(3,3,2), ECubeGridFace::PositiveZ);
			State = UCubeGridToolset::ApplyMaterialToRectangle(TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
			if (State.PaintedTriangles == 0) return Fail(TEXT("Painting did not find the top surface."));
			Test->TestTrue(TEXT("Painting preserves volume"), FMath::IsNearlyEqual(Volume(), 32000000.0, 1.0));
			CheckPaintBounds();
			Pending.Reset(UCubeGridToolset::PushRectangle(1));
			break;
		}
		case 2:
			Test->TestTrue(TEXT("Push removes only the selected prism"), FMath::IsNearlyEqual(Volume(), 28000000.0, 1.0));
			UCubeGridToolset::SelectRectangle(FVector(4,0,0), FVector(6,2,0), ECubeGridFace::PositiveZ);
			Pending.Reset(UCubeGridToolset::EnterCornerMode({}));
			break;
		case 3:
		{
			const auto State = UCubeGridToolset::GetState();
			Test->TestEqual(TEXT("Four corner positions returned"), State.CornerPositionsCells.Num(), 4);
			TArray<int32> Corners;
			for (int32 Index = 0; Index < State.CornerPositionsCells.Num(); ++Index)
				if (FMath::IsNearlyEqual(State.CornerPositionsCells[Index].X, 6.0)) Corners.Add(Index);
			if (Corners.Num() != 2) return Fail(TEXT("Could not map the high edge of the ramp."));
			Pending.Reset(UCubeGridToolset::EnterCornerMode(Corners));
			break;
		}
		case 4: Pending.Reset(UCubeGridToolset::PullRectangle(2)); break;
		case 5: Pending.Reset(UCubeGridToolset::LeaveCornerMode(true)); break;
		case 6:
			Test->TestFalse(TEXT("Corner mode committed and exited"), UCubeGridToolset::GetState().bCornerMode);
			Test->TestTrue(TEXT("Corner mode produces a half-prism ramp"), FMath::IsNearlyEqual(Volume(), 32000000.0, 1.0));
			UCubeGridToolset::SelectRectangle(FVector(6,0,0), FVector(8,2,0), ECubeGridFace::PositiveZ);
			Pending.Reset(UCubeGridToolset::EnterCornerMode({0,1}));
			break;
		case 7: Pending.Reset(UCubeGridToolset::PullRectangle(2)); break;
		case 8: Pending.Reset(UCubeGridToolset::LeaveCornerMode(false)); break;
		case 9:
			Test->TestTrue(TEXT("Cancelling corners preserves committed volume"), FMath::IsNearlyEqual(Volume(), 32000000.0, 1.0));
			UCubeGridToolset::SetGridSize(50);
			Test->TestFalse(TEXT("Changing grid size clears selection"), UCubeGridToolset::GetState().bHasSelection);
			Test->TestTrue(TEXT("Changing grid size preserves mesh"), FMath::IsNearlyEqual(Volume(), 32000000.0, 1.0));
			UCubeGridToolset::SelectRectangle(FVector(0,0,0), FVector(0,2,2), ECubeGridFace::NegativeX);
			Pending.Reset(UCubeGridToolset::PullRectangle(1));
			break;
		case 10:
			Test->TestTrue(TEXT("Negative-axis pull extends minimum X"), FMath::IsNearlyEqual(UCubeGridToolset::GetState().MeshBoundsMin.X, 950.0, 0.001));
			Test->TestTrue(TEXT("Negative-axis extrusion adds expected volume"), FMath::IsNearlyEqual(Volume(), 32500000.0, 1.0));
			Test->TestTrue(TEXT("Boolean mesh remains valid"), Tool->CurrentMesh->CheckValidity());
			Pending.Reset(UCubeGridToolset::PushRectangle(0));
			// Expected async error is a normal result, not an Editor log error.
			Test->TestTrue(TEXT("Invalid depth returns an MCP error"), Pending->bIsComplete && !Pending->Error.IsEmpty());
			Pending.Reset();
			break;
		case 11:
		{
			const auto State = UCubeGridToolset::ConfirmMesh(TEXT("CubeGridAutomation"));
			Test->TestFalse(TEXT("Confirmation closes the tool"), UCubeGridToolset::GetState().bActive);
			Test->TestEqual(TEXT("One output actor created"), State.CreatedActorPaths.Num(), 1);
			Tool.Reset();
			Step = 14;
			break;
		}
		case 14:
		{
			const auto State = UCubeGridToolset::StartCubeGrid(50, FVector(10,20,30), FRotator(0,90,0), ECubeGridOutput::DynamicMesh);
			Test->TestTrue(TEXT("New session starts after confirmation"), State.bActive);
			UCubeGridToolset::SelectRectangle(FVector::ZeroVector, FVector(2,1,0), ECubeGridFace::PositiveZ);
			Pending.Reset(UCubeGridToolset::PullRectangle(1));
			break;
		}
		case 15:
		{
			const auto State = UCubeGridToolset::GetState();
			Test->TestTrue(TEXT("Rotated grid minimum bounds"), State.MeshBoundsMin.Equals(FVector(-40,20,30), 0.001));
			Test->TestTrue(TEXT("Rotated grid maximum bounds"), State.MeshBoundsMax.Equals(FVector(10,120,80), 0.001));
			UCubeGridToolset::CancelCubeGrid();
			Test->TestFalse(TEXT("Cancel closes the tool"), UCubeGridToolset::GetState().bActive);
			return true;
		}
		}
		return false;
	}
private:
	bool Fail(const FString& Message)
	{
		Test->AddError(Message);
		if (UCubeGridToolset::GetState().bActive) UCubeGridToolset::CancelCubeGrid();
		return true;
	}
	double Volume() const
	{
		double Sum = 0;
		for (int32 Triangle : Tool->CurrentMesh->TriangleIndicesItr())
		{
			FVector3d A, B, C; Tool->CurrentMesh->GetTriVertices(Triangle, A, B, C);
			Sum += A.Dot(B.Cross(C)) / 6.0;
		}
		return FMath::Abs(Sum);
	}
	void CheckPaintBounds()
	{
		const int32 Material = Tool->CurrentMeshMaterials.IndexOfByKey(LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial")));
		double Area = 0;
		for (int32 Triangle : Tool->CurrentMesh->TriangleIndicesItr())
		{
			if (Tool->CurrentMesh->Attributes()->GetMaterialID()->GetValue(Triangle) != Material) continue;
			FVector3d A, B, C; Tool->CurrentMesh->GetTriVertices(Triangle, A, B, C);
			// Only top-facing triangles are part of the painted rectangle.
			if ((B-A).Cross(C-A).Z <= 0) continue;
			for (FVector3d Vertex : {A,B,C})
			{
				const FVector3d Frame = Tool->CubeGrid->GetFrame().ToFramePoint(Tool->CurrentMeshTransform.TransformPosition(Vertex));
				Test->TestTrue(TEXT("Paint does not spill outside rectangle"), Frame.X >= 100-0.001 && Frame.X <= 300+0.001 && Frame.Y >= 100-0.001 && Frame.Y <= 300+0.001);
			}
			Area += (B-A).Cross(C-A).Length() * 0.5;
		}
		Test->TestTrue(TEXT("Painted area matches rectangle"), FMath::IsNearlyEqual(Area, 40000.0, 0.001));
		Test->TestTrue(TEXT("Painting preserves mesh validity"), Tool->CurrentMesh->CheckValidity());
	}
	FAutomationTestBase* Test;
	int32 Step = 0;
	TWeakObjectPtr<UCubeGridMCPTool> Tool;
	TStrongObjectPtr<UCubeGridAsyncResult> Pending;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCubeGridGeometryTest, "CubeGridToolset.GeometryWorkflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCubeGridGeometryTest::RunTest(const FString& Parameters)
{
	FAutomationEditorCommonUtils::CreateNewMap();
	ADD_LATENT_AUTOMATION_COMMAND(FCubeGridGeometryCommand(this));
	return true;
}

#endif
