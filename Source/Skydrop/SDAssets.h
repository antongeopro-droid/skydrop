#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UMaterialInterface;
class UStaticMeshComponent;
class USceneComponent;
class AActor;

enum class ESDSurf : uint8 { Plain, Wood, Brick, Metal, Rock, Leaf, Ground, Siding, Shingle, Plaster, Asphalt, Concrete, Stripes, Damask, Count };

namespace SDAssets
{
	// Patterned surface (wood planks, bricks, metal panels, rock strata, leaves, paving)
	UMaterialInterface* Surface(ESDSurf S, const FLinearColor& C, float Emissive = 0.f, float Roughness = 0.7f, float Metallic = 0.f);
	UStaticMesh* Cube();
	UStaticMesh* Sphere();
	UStaticMesh* Cylinder();
	UStaticMesh* Cone();
	UStaticMesh* Plane();
	// imported gun model for a weapon (rarity picks the AR variant); origin at the grip, muzzle along +X
	UStaticMesh* GunMesh(uint8 Weapon, int32 Rarity);

	UMaterialInterface* Solid(const FLinearColor& C, float Emissive = 0.f, float Roughness = 0.65f, float Metallic = 0.f);
	UMaterialInterface* Ghost(const FLinearColor& C);
	UMaterialInterface* Storm();
	UMaterialInterface* VertexColor();
	UMaterialInterface* Water();

	UStaticMeshComponent* AddMesh(AActor* Owner, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Loc,
		const FVector& Scale, const FRotator& Rot, UMaterialInterface* Mat, bool bCollide = true);

	// Size is in world units (cube mesh is 100 units)
	UStaticMeshComponent* AddBox(AActor* Owner, USceneComponent* Parent, const FVector& Center, const FVector& Size,
		const FRotator& Rot, UMaterialInterface* Mat, bool bCollide = true);

	void SetBoolProp(UObject* Obj, FName Name, bool bValue);
	void SetFloatProp(UObject* Obj, FName Name, float Value);
	void SetColorProp(UObject* Obj, FName Name, const FLinearColor& Value);
}
