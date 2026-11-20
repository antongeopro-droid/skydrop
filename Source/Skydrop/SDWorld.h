#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkydropTypes.h"
#include "SDAssets.h"
#include "SDWorld.generated.h"

class UProceduralMeshComponent;
class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UVolumetricCloudComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class UStaticMeshComponent;
class UTexture2D;
class ASDGameMode;

enum class ESDPoiType : uint8 { Suburb, City, Farm, Lodge, Industrial, Desert, Harbor };

struct FSDPoi
{
	FString Name;
	FVector2D Pos;
	float Radius = 4200.f;
	float Z = 0.f;
	ESDPoiType Type = ESDPoiType::Suburb;
};

UCLASS()
class SKYDROP_API ASDWorld : public AActor
{
	GENERATED_BODY()
public:
	ASDWorld();
	virtual void BeginPlay() override;

	void Generate(int32 Seed, ASDGameMode* GM);
	float HeightAt(float X, float Y) const;
	FVector RandomLandPoint(FRandomStream& R, float MinH = 80.f, float MaxFrac = 0.85f) const;
	bool NearPoi(const FVector2D& P, float Extra = 0.f) const;
	float MapExtent() const { return IslandRadius * 1.2f; }
	FVector2D GetLakePos() const { return LakePos; }
	FLinearColor GroundColorAt(float X, float Y, float Z, float Nz) const { return GroundColor(X, Y, Z, Nz); }
	bool IsGrassy(float X, float Y) const { FLinearColor Tc; float Paved = 0.f; TownGround(X, Y, Tc, Paved); return Paved < 0.35f && DesertW(X, Y) < 0.35f && RoadDist(X, Y) > 450.f && !Used.Contains(FIntPoint(FMath::FloorToInt(X / SDGrid::Tile), FMath::FloorToInt(Y / SDGrid::Tile))); }
	// how much a town repaints the ground here (0..1), its colour, and how paved it is (no grass)
	float TownGround(float X, float Y, FLinearColor& OutColor, float& OutPaved) const;
	float GetLakeRadius() const { return LakeRadius; }

	float IslandRadius = 100000.f;
	TArray<FSDPoi> Pois;
	int32 SkyPreset = 0;

	UPROPERTY() TObjectPtr<UTexture2D> MapTexture;
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Terrain;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> WaterMesh;
	UPROPERTY() TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY() TObjectPtr<USkyAtmosphereComponent> Sky;
	UPROPERTY() TObjectPtr<USkyLightComponent> SkyLight;
	UPROPERTY() TObjectPtr<UVolumetricCloudComponent> Clouds;
	UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY() TObjectPtr<UPostProcessComponent> Post;

private:
	// terrain fields
	float Fbm(float X, float Y, int32 Oct) const;
	float RawHeight(float X, float Y) const;
	float DesertW(float X, float Y) const;
	float ForestW(float X, float Y) const;
	float RoadDist(float X, float Y) const;
	float RiverDist(float X, float Y) const;
	FLinearColor GroundColor(float X, float Y, float Z, float Nz) const;
	void BuildTerrain();
	void BuildMapTexture();
	void ApplySkyPreset(int32 Preset);

	// structures
	struct FTown { const FSDPoi* Poi; int32 CI, CJ, L0; };
	bool Reserve(int32 I0, int32 J0, int32 W, int32 D, int32 Margin);
	void BuildTown(FRandomStream& R, ASDGameMode* GM, const FSDPoi& Poi);
	void BuildHouse(FRandomStream& R, ASDGameMode* GM, int32 I0, int32 J0, int32 W, int32 D, int32 Floors, int32 L0, ESDMat Mat, bool bFlatRoof);
	void BuildTower(FRandomStream& R, ASDGameMode* GM, int32 I0, int32 J0, int32 W, int32 D, int32 Floors, int32 L0, ESDMat Mat);
	void BuildBarn(FRandomStream& R, ASDGameMode* GM, int32 I0, int32 J0, int32 L0);
	struct FSDTownStyle { ESDSurf Wall = ESDSurf::Siding; FLinearColor WallC = FLinearColor::White; FLinearColor Trim = FLinearColor::White; ESDSurf Roof = ESDSurf::Shingle; FLinearColor RoofC = FLinearColor::Gray;
		ESDSurf InSurf = ESDSurf::Plaster; FLinearColor InC = FLinearColor::White; ESDSurf FloorSurf = ESDSurf::Wood; FLinearColor FloorC = FLinearColor(0.45f, 0.3f, 0.17f); };
	FSDTownStyle PickStyle(FRandomStream& R, ESDMat Mat, int32 Kind) const; // Kind 0 house, 1 tower, 2 barn
	void StylePiece(class ASDBuildPiece* P, const FSDTownStyle& S, int32 L0, int32 Top, const FVector2D& Center, bool bInterior) const;
	// furniture along the walls of one floor of a building; RoomKind 0 living, 1 bedroom, 2 kitchen, 3 office
	void FurnishRoom(FRandomStream& R, ASDGameMode* GM, int32 I0, int32 J0, int32 W, int32 D, int32 L, int32 RoomKind, const TArray<FIntPoint>& Avoid);
	void Foundation(int32 I0, int32 J0, int32 W, int32 D, int32 L0);
	void GableRoof(int32 I0, int32 J0, int32 W, int32 D, int32 Top, ESDMat Mat, TFunctionRef<class ASDBuildPiece*(ESDBuildType, int32, int32, int32, int32, int32, ESDMat)> Place);
	void StreetLamp(const FVector& Base, float Yaw);
	int32 CurrentPoiType = 0;
	void Slab(const FVector& Center, const FVector& Size, const FLinearColor& Color, bool bCollide = true, ESDSurf Surf = ESDSurf::Ground);
	void SpawnResource(FRandomStream& R, int32 Type, const FVector& At, float Size);

	FVector2D NoiseOffset = FVector2D::ZeroVector;
	FVector2D DesertDir = FVector2D(1, 0);
	FVector2D ForestDir = FVector2D(-1, 0);
	FVector2D LakePos = FVector2D::ZeroVector;
	float LakeRadius = 0.f;
	FVector2D MountainPos = FVector2D::ZeroVector;
	struct FButte { FVector2D Pos; float Rad; float Hgt; float Rad2; float Hgt2; float BaseZ; float Seed; FVector2D Off2; };
	TArray<FButte> Buttes;
	TArray<FVector2D> River;
	TArray<TPair<FVector2D, FVector2D>> Roads;
	TSet<FIntPoint> Used;
	int32 Spawned = 0;
};
