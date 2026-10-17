// YuriNK, 2026, All Right Reserved

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "ToolsetRegistry/ToolCallAsyncResult.h"

#include "CubeGridToolset.generated.h"

UENUM()
enum class ECubeGridFace : uint8
{
	PositiveX, NegativeX, PositiveY, NegativeY, PositiveZ, NegativeZ
};

UENUM()
enum class ECubeGridOutput : uint8
{
	StaticMesh, DynamicMesh, Volume
};

/// Snapshot of the live CubeGrid session. Distances are centimeters unless labeled cells.
USTRUCT(BlueprintType)
struct FCubeGridState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") bool bActive = false;
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") bool bBusy = false;
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") bool bCornerMode = false;
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") bool bHasSelection = false;
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") double GridSize = 0;
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") FVector GridOrigin = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") FRotator GridRotation = FRotator::ZeroRotator;
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") FVector SelectionMinCells = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") FVector SelectionMaxCells = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") ECubeGridFace Face = ECubeGridFace::PositiveZ;
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") TArray<int32> SelectedCorners;
	/// Native corner indices 0..3 mapped to grid-cell positions while in corner mode.
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") TArray<FVector> CornerPositionsCells;
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") int32 TriangleCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") FVector MeshBoundsMin = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") FVector MeshBoundsMax = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") float UVScale = 1;
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") bool bWorldSpaceUVScale = true;
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") FString MaterialPath;
	/// Filled by ConfirmMesh; packages are left dirty for the normal Editor save workflow.
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") TArray<FString> CreatedActorPaths;
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") TArray<FString> CreatedAssetPaths;
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") int32 PaintedTriangles = 0;
};

/// Native asynchronous result: MCP waits for CubeGrid's preview to settle, without blocking the game thread.
UCLASS()
class UCubeGridAsyncResult : public UToolCallAsyncResult
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") FCubeGridState Value;
	void SetValue(FCubeGridState InValue) { MaybeBroadcastSuccessfulCompletion(MoveTemp(InValue), Value); }
};

USTRUCT(BlueprintType)
struct FCubeGridSkillInfo
{
	GENERATED_BODY()
	/// Portable skill identifier accepted by ReadSkill.
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") FString Name;
	/// When this skill should be used.
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") FString Description;
	/// Path relative to this plugin's Content directory; no project-specific absolute paths.
	UPROPERTY(BlueprintReadOnly, Category="CubeGrid") FString ContentPath;
};

/** Direct CubeGrid control for grid-aligned level geometry. Coordinates use grid cells, not screen pixels.
 * Before generating geometry, call GetSkillCatalog and ReadSkill to load the relevant bundled instructions.
 * Use cubegrid-plan-build for floor plans and dimensioned blockouts; cubegrid-concept-build for concept images;
 * ue-selection-describe only when inspecting or cataloging selected architecture. Read each skill's required
 * references through ReadSkill. Skills live in this plugin's Content/Skills and travel with it to new projects.
 * Discover the live schemas and await each geometry operation before the next mutation.
 */
UCLASS(BlueprintType, Hidden)
class UCubeGridToolset : public UToolsetDefinition
{
	GENERATED_BODY()
public:
	/** List bundled skills under this plugin's Content/Skills directory.
	 * Before building, choose the relevant skill and load its SKILL.md and required references with ReadSkill.
	 */
	UFUNCTION(meta=(AICallable), Category="CubeGrid Skills")
	static TArray<FCubeGridSkillInfo> GetSkillCatalog();

	/** Read a bundled skill or its supporting reference as text, independently of the current project path.
	 * @param SkillName Identifier returned by GetSkillCatalog, e.g. cubegrid-plan-build.
	 * @param RelativePath File within that skill directory, e.g. SKILL.md or references/CUBEGRID-MCP.md.
	 * Read the relevant skill before starting geometry. This reads files only; it does not execute scripts.
	 */
	UFUNCTION(meta=(AICallable), Category="CubeGrid Skills")
	static FString ReadSkill(const FString& SkillName, const FString& RelativePath = TEXT("SKILL.md"));

	/** Start a new, empty CubeGrid session in Modeling Mode. Does not modify selected meshes.
	 * First read the relevant Content/Skills instructions using GetSkillCatalog and ReadSkill.
	 * @param GridSize Cell edge length in centimeters; at least 0.001.
	 * @param Origin World-space grid origin in centimeters.
	 * @param Rotation World-space grid orientation in degrees.
	 * @param OutputType Object created on confirmation.
	 */
	UFUNCTION(meta=(AICallable), Category="CubeGrid")
	static FCubeGridState StartCubeGrid(double GridSize = 100, FVector Origin = FVector::ZeroVector,
		FRotator Rotation = FRotator::ZeroRotator, ECubeGridOutput OutputType = ECubeGridOutput::StaticMesh);

	/** Read selection, pending computation, material, and world-space geometry bounds. */
	UFUNCTION(meta=(AICallable), Category="CubeGrid")
	static FCubeGridState GetState();

	/** Change cell size in centimeters. Clears the rectangle; existing geometry stays in place.
	 * @param GridSize Cell edge length, at least 0.001. Unavailable during corner mode.
	 */
	UFUNCTION(meta=(AICallable), Category="CubeGrid")
	static FCubeGridState SetGridSize(double GridSize);

	/** Select a planar rectangle using two grid-cell boundary coordinates.
	 * @param MinCorner Integer grid-cell coordinates of the minimum boundary corner, inclusive.
	 * @param MaxCorner Integer coordinates of the maximum boundary corner, exclusive in the two spanning axes.
	 * @param Face Outward direction normal to the rectangle. The normal-axis coordinate must be equal in both corners.
	 */
	UFUNCTION(meta=(AICallable), Category="CubeGrid")
	static FCubeGridState SelectRectangle(FVector MinCorner, FVector MaxCorner, ECubeGridFace Face);

	/** Add solid geometry outward from the rectangle. Waits for the boolean result.
	 * @param Blocks Positive extrusion depth in grid cells. In corner mode, adjusts the corner preview.
	 */
	UFUNCTION(meta=(AICallable), Category="CubeGrid")
	static UCubeGridAsyncResult* PullRectangle(int32 Blocks = 1);

	/** Subtract solid geometry inward from the rectangle. Waits for the boolean result.
	 * @param Blocks Positive subtraction depth in grid cells. In corner mode, adjusts the corner preview.
	 */
	UFUNCTION(meta=(AICallable), Category="CubeGrid")
	static UCubeGridAsyncResult* PushRectangle(int32 Blocks = 1);

	/** Set CubeGrid's UV generation scale for subsequent push/pull operations; existing UVs are unchanged.
	 * @param Scale Positive finite multiplier.
	 * @param bWorldSpace Relative to world-space dimensions, so different sized blocks have consistent texel density.
	 */
	UFUNCTION(meta=(AICallable), Category="CubeGrid")
	static FCubeGridState SetMaterialUVScale(float Scale = 1, bool bWorldSpace = true);

	/** Assign a material to outward-facing, coplanar mesh surfaces inside the rectangle.
	 * Splits crossing triangles at rectangle boundaries, preserving geometry and interpolating attributes.
	 * @param MaterialPath Material or material-instance object path. Also sets the material for subsequent extrusion.
	 * @param bPaintExisting If false, only set the next extrusion's material (works on an empty selection plane).
	 */
	UFUNCTION(meta=(AICallable), Category="CubeGrid")
	static FCubeGridState ApplyMaterialToRectangle(const FString& MaterialPath, bool bPaintExisting = true);

	/** Enter corner mode, or replace its selected corner mask. Requires a rectangle.
	 * @param Corners Corner indices 0..3 to extrude. Indices follow Unreal's oriented-box convention; see SKILL.md.
	 * @param bCrosswiseDiagonal Prefer the alternate triangulation across the selected face.
	 */
	UFUNCTION(meta=(AICallable), Category="CubeGrid")
	static UCubeGridAsyncResult* EnterCornerMode(const TArray<int32>& Corners, bool bCrosswiseDiagonal = false);

	/** Leave corner mode and wait for the result.
	 * @param bApply True commits the corner preview; false discards it.
	 */
	UFUNCTION(meta=(AICallable), Category="CubeGrid")
	static UCubeGridAsyncResult* LeaveCornerMode(bool bApply = true);

	/** Confirm geometry and close CubeGrid. Refuses an unfinished corner preview or an empty mesh.
	 * @param ActorLabel Optional label for the created actor. Created asset/actor paths are returned; save separately.
	 */
	UFUNCTION(meta=(AICallable), Category="CubeGrid")
	static FCubeGridState ConfirmMesh(const FString& ActorLabel = TEXT("CubeGrid"));

	/** Discard this plugin's active CubeGrid session and close the tool. Does not delete previously confirmed meshes. */
	UFUNCTION(meta=(AICallable), Category="CubeGrid")
	static FCubeGridState CancelCubeGrid();
};

