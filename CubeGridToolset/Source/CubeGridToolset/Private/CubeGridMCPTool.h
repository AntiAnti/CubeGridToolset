// YuriNK, 2026, All Rights Reserved
#pragma once

#include "CubeGridTool.h"
#include "CubeGridToolset.h"
#include "CubeGridMCPTool.generated.h"

class UMaterialInterface;

/// The stock CubeGrid implementation with a narrow automation bridge to its protected state.
UCLASS(Transient)
class UCubeGridMCPTool : public UCubeGridTool
{
	GENERATED_BODY()
public:
	virtual void OnTick(float DeltaTime) override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	FCubeGridState ReadState() const;
	bool IsBusy() const;
	void Configure(double Size, FVector Origin, FRotator Rotation, ECubeGridOutput Output);
	void ChangeGridSize(double Size);
	bool ChooseRectangle(FVector Min, FVector Max, ECubeGridFace Face, FString& Error);
	UCubeGridAsyncResult* Extrude(int32 Blocks);
	void ChangeUVScale(float Scale, bool bWorldSpace);
	bool PaintRectangle(UMaterialInterface* Material, bool bPaintExisting, int32& Painted, FString& Error);
	UCubeGridAsyncResult* BeginCorners(const TArray<int32>& Corners, bool bCrosswise);
	UCubeGridAsyncResult* EndCorners(bool bApply);
	void ApplyMeshSnapshot(const UE::Geometry::FDynamicMesh3& Mesh, const TArray<UMaterialInterface*>& Materials);
	bool IsSessionClosed() const { return bSessionClosed; }
	static bool ValidateRectangle(FVector Min, FVector Max, ECubeGridFace Face, FString& Error);
	static UE::Geometry::FCubeGrid::EFaceDirection ToNativeFace(ECubeGridFace Face);
	friend class FCubeGridGeometryCommand;
private:
	UCubeGridAsyncResult* BeginPendingResult();
	UPROPERTY(Transient) TObjectPtr<UCubeGridAsyncResult> PendingResult;
	double OperationStartedAt = 0;
	bool bSessionClosed = false;
};

UCLASS(Transient)
class UCubeGridMCPToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
