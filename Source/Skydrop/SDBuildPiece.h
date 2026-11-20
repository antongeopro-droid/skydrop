#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkydropTypes.h"
#include "SDAssets.h"
#include "SDBuildPiece.generated.h"

class UPrimitiveComponent;

// Edit = bit mask of removed tiles. Walls: 3x3, tile = row*3 + col (row 0 at the bottom, col 0 at local -Y).
// Floors and cones: 2x2, tile = b*2 + a (a along local X). Ramps: a stair code from a path drawn on a 3x3 ring.
namespace SDEditMask
{
	constexpr int32 Window = 1 << 4;                     // centre tile
	constexpr int32 Door = (1 << 4) | (1 << 1);          // centre column, bottom two
	constexpr int32 HalfWall = (1 << 6) | (1 << 7) | (1 << 8); // top row
}

struct FSDEditTile
{
	int32 Idx = 0;
	FVector C[4];
};

UCLASS()
class SKYDROP_API ASDBuildPiece : public AActor
{
	GENERATED_BODY()
public:
	ASDBuildPiece();

	void Setup(const FSDBuildSlot& InSlot, ESDMat InMat, bool bInGhost, bool bInWorld, bool bInstant);
	void Rebuild();
	void SetGhostValid(bool bValid);
	bool CycleEdit();           // returns true if the piece changed (quick presets: R on ramps, legacy)
	// Fortnite-style tile editing
	int32 NumTiles() const;
	void GetEditTiles(const FVector& ViewPos, TArray<FSDEditTile>& Out) const;
	bool IsValidEdit(int32 Sel, const TArray<int32>& Path) const;
	bool ApplyEdit(int32 Sel, const TArray<int32>& Path); // true if the piece changed
	static bool StairFromPath(const TArray<int32>& Path, int32& OutEdit, int32& OutRot);
	static TArray<int32> CleanStairPath(const TArray<int32>& Path);
	void SetEdit(int32 NewEdit);
	// town buildings: their own surface (siding, brick, plaster, shingles...) plus trim, window frames and glass
	void SetWorldStyle(ESDSurf Surf, const FLinearColor& Color, const FLinearColor& Trim);
	bool bStyled = false;
	ESDSurf StyleSurf = ESDSurf::Plain;
	FLinearColor StyleColor = FLinearColor::White;
	FLinearColor TrimColor = FLinearColor::White;
	UPROPERTY() TArray<TObjectPtr<UPrimitiveComponent>> DecoParts;
	// interior finish: walls get paint/wallpaper + baseboard + crown on the room side (+1/-1 = local X side),
	// floors with a room below get a plaster ceiling underneath
	void SetInterior(int32 Side, ESDSurf Surf, const FLinearColor& Color, bool bCeilingBelow);
	int32 InteriorSide = 0;
	ESDSurf InteriorSurf = ESDSurf::Plaster;
	FLinearColor InteriorColor = FLinearColor::White;
	bool bCeiling = false;
	void Collapse();
	float HealthFraction() const { return MaxHP > 0 ? HP / MaxHP : 0.f; }
	bool IsGrounded() const;

	virtual void Tick(float Dt) override;
	virtual float TakeDamage(float Amount, struct FDamageEvent const& Ev, AController* Inst, AActor* Causer) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	static FString MakeKey(const FSDBuildSlot& S);
	static FTransform SlotTransform(const FSDBuildSlot& S);

	FSDBuildSlot Slot;
	FString Key;
	ESDMat Mat = ESDMat::Wood;
	int32 Edit = 0;
	bool bGhost = false;
	bool bWorld = false;
	bool bGhostValid = true;
	bool bCollapsing = false;
	float HP = 100.f;
	float MaxHP = 150.f;
	float BuildLeft = 0.f;
	float BuildTotal = 1.f;
	float HitFlash = 0.f;
	int32 LastVisualStep = -1;

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TArray<TObjectPtr<UPrimitiveComponent>> Parts;

private:
	void ApplyMaterial();
	void AddPart(const FVector& Center, const FVector& Size, const FRotator& Rot = FRotator::ZeroRotator);
	void AddCone();
	void AddProfileWall(TFunctionRef<float(float)> Bottom, TFunctionRef<float(float)> Top);
	void AddRoof(int32 E);
	void AddStairs(int32 E);
	void AddMeshPart(class UProceduralMeshComponent* P);
	void AddWallDecor();
};
