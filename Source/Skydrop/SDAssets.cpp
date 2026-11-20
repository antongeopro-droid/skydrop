#include "SDAssets.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

namespace
{
	template <typename T>
	T* LoadRooted(const TCHAR* Path, T*& Cache)
	{
		if (!Cache)
		{
			Cache = LoadObject<T>(nullptr, Path, nullptr, LOAD_NoWarn);
			if (Cache) Cache->AddToRoot();
		}
		return Cache;
	}

	UMaterialInterface* BaseSolid()
	{
		static UMaterialInterface* M = nullptr;
		if (!M) LoadRooted<UMaterialInterface>(TEXT("/Game/Skydrop/Materials/M_Solid.M_Solid"), M);
		if (!M) LoadRooted<UMaterialInterface>(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"), M);
		return M;
	}

	UMaterialInterface* BaseGhost()
	{
		static UMaterialInterface* M = nullptr;
		if (!M) LoadRooted<UMaterialInterface>(TEXT("/Game/Skydrop/Materials/M_Ghost.M_Ghost"), M);
		return M ? M : BaseSolid();
	}

	UMaterialInstanceDynamic* MakeMID(UMaterialInterface* Base)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, GetTransientPackage());
		MID->AddToRoot();
		return MID;
	}
}

UStaticMesh* SDAssets::Cube()     { static UStaticMesh* M = nullptr; return LoadRooted<UStaticMesh>(TEXT("/Engine/BasicShapes/Cube.Cube"), M); }
UStaticMesh* SDAssets::Sphere()   { static UStaticMesh* M = nullptr; return LoadRooted<UStaticMesh>(TEXT("/Engine/BasicShapes/Sphere.Sphere"), M); }
UStaticMesh* SDAssets::Cylinder() { static UStaticMesh* M = nullptr; return LoadRooted<UStaticMesh>(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), M); }
UStaticMesh* SDAssets::Cone()     { static UStaticMesh* M = nullptr; return LoadRooted<UStaticMesh>(TEXT("/Engine/BasicShapes/Cone.Cone"), M); }
UStaticMesh* SDAssets::Plane()    { static UStaticMesh* M = nullptr; return LoadRooted<UStaticMesh>(TEXT("/Engine/BasicShapes/Plane.Plane"), M); }

UStaticMesh* SDAssets::GunMesh(uint8 Weapon, int32 Rarity)
{
	// matches ESDWeapon: None, AR, Pump, SMG, Pistol, Sniper, Tactical
	const TCHAR* Name = nullptr;
	switch (Weapon)
	{
	case 1: Name = Rarity >= 2 ? TEXT("SM_Gun_SCAR") : TEXT("SM_Gun_AR"); break;
	case 2: Name = TEXT("SM_Gun_Pump"); break;
	case 3: Name = TEXT("SM_Gun_SMG"); break;
	case 4: Name = TEXT("SM_Gun_Pistol"); break;
	case 5: Name = TEXT("SM_Gun_Sniper"); break;
	case 6: Name = TEXT("SM_Gun_Tactical"); break;
	default: return nullptr;
	}
	static TMap<FString, UStaticMesh*> Cache;
	if (UStaticMesh** Found = Cache.Find(Name)) return *Found;
	const FString Path = FString::Printf(TEXT("/Game/Skydrop/Guns/%s/%s/StaticMeshes/%s.%s"), Name, Name, Name, Name);
	UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn);
	if (M) M->AddToRoot();
	Cache.Add(Name, M);
	return M;
}

UMaterialInterface* SDAssets::Solid(const FLinearColor& C, float Emissive, float Roughness, float Metallic)
{
	return Surface(ESDSurf::Plain, C, Emissive, Roughness, Metallic);
}

UMaterialInterface* SDAssets::Surface(ESDSurf Surf, const FLinearColor& C, float Emissive, float Roughness, float Metallic)
{
	static UMaterialInterface* Bases[(int32)ESDSurf::Count] = {};
	static const TCHAR* Paths[(int32)ESDSurf::Count] = {
		nullptr,
		TEXT("/Game/Skydrop/Materials/M_Wood.M_Wood"),
		TEXT("/Game/Skydrop/Materials/M_Brick.M_Brick"),
		TEXT("/Game/Skydrop/Materials/M_Metal.M_Metal"),
		TEXT("/Game/Skydrop/Materials/M_Rock.M_Rock"),
		TEXT("/Game/Skydrop/Materials/M_Leaf.M_Leaf"),
		TEXT("/Game/Skydrop/Materials/M_Ground.M_Ground"),
		TEXT("/Game/Skydrop/Materials/M_Siding.M_Siding"),
		TEXT("/Game/Skydrop/Materials/M_Shingle.M_Shingle"),
		TEXT("/Game/Skydrop/Materials/M_Plaster.M_Plaster"),
		TEXT("/Game/Skydrop/Materials/M_Asphalt.M_Asphalt"),
		TEXT("/Game/Skydrop/Materials/M_Concrete.M_Concrete"),
		TEXT("/Game/Skydrop/Materials/M_Stripes.M_Stripes"),
		TEXT("/Game/Skydrop/Materials/M_Damask.M_Damask"),
	};
	const int32 SI = FMath::Clamp((int32)Surf, 0, (int32)ESDSurf::Count - 1);
	if (SI > 0 && !Bases[SI]) LoadRooted<UMaterialInterface>(Paths[SI], Bases[SI]);
	UMaterialInterface* Base = (SI > 0 && Bases[SI]) ? Bases[SI] : BaseSolid();

	static TMap<uint64, UMaterialInstanceDynamic*> Cache;
	const uint64 Key =
		  (uint64)FMath::Clamp(FMath::RoundToInt(C.R * 255.f), 0, 255)
		| ((uint64)FMath::Clamp(FMath::RoundToInt(C.G * 255.f), 0, 255) << 8)
		| ((uint64)FMath::Clamp(FMath::RoundToInt(C.B * 255.f), 0, 255) << 16)
		| ((uint64)FMath::Clamp(FMath::RoundToInt(Emissive * 10.f), 0, 4095) << 24)
		| ((uint64)FMath::Clamp(FMath::RoundToInt(Roughness * 20.f), 0, 31) << 36)
		| ((uint64)FMath::Clamp(FMath::RoundToInt(Metallic * 10.f), 0, 15) << 41)
		| ((uint64)SI << 46);
	if (UMaterialInstanceDynamic** Found = Cache.Find(Key))
	{
		if (IsValid(*Found)) return *Found;
	}
	if (!Base) return nullptr;
	UMaterialInstanceDynamic* MID = MakeMID(Base);
	MID->SetVectorParameterValue(TEXT("Color"), C);
	MID->SetScalarParameterValue(TEXT("Emissive"), Emissive);
	MID->SetScalarParameterValue(TEXT("Roughness"), Roughness);
	MID->SetScalarParameterValue(TEXT("Metallic"), Metallic);
	Cache.Add(Key, MID);
	return MID;
}

UMaterialInterface* SDAssets::Ghost(const FLinearColor& C)
{
	static TMap<uint32, UMaterialInstanceDynamic*> Cache;
	const uint32 Key = C.ToFColor(false).ToPackedARGB();
	if (UMaterialInstanceDynamic** Found = Cache.Find(Key))
	{
		if (IsValid(*Found)) return *Found;
	}
	UMaterialInstanceDynamic* MID = MakeMID(BaseGhost());
	MID->SetVectorParameterValue(TEXT("Color"), C);
	Cache.Add(Key, MID);
	return MID;
}

UMaterialInterface* SDAssets::Storm()
{
	static UMaterialInterface* M = nullptr;
	LoadRooted<UMaterialInterface>(TEXT("/Game/Skydrop/Materials/M_Storm.M_Storm"), M);
	return M ? M : Ghost(FLinearColor(0.5f, 0.1f, 0.9f));
}

UMaterialInterface* SDAssets::VertexColor()
{
	static UMaterialInterface* M = nullptr;
	LoadRooted<UMaterialInterface>(TEXT("/Game/Skydrop/Materials/M_Terrain.M_Terrain"), M);
	if (!M) LoadRooted<UMaterialInterface>(TEXT("/Game/Skydrop/Materials/M_VertexColor.M_VertexColor"), M);
	return M ? M : Solid(FLinearColor(0.25f, 0.5f, 0.18f));
}

UMaterialInterface* SDAssets::Water()
{
	static UMaterialInterface* M = nullptr;
	LoadRooted<UMaterialInterface>(TEXT("/Game/Skydrop/Materials/M_Water.M_Water"), M);
	return M ? M : Solid(FLinearColor(0.05f, 0.35f, 0.55f), 0.f, 0.05f);
}

UStaticMeshComponent* SDAssets::AddMesh(AActor* Owner, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Loc,
	const FVector& Scale, const FRotator& Rot, UMaterialInterface* Mat, bool bCollide)
{
	if (!Owner) return nullptr;
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(Parent ? Parent : Owner->GetRootComponent());
	C->SetRelativeLocation(Loc);
	C->SetRelativeRotation(Rot);
	C->SetRelativeScale3D(Scale);
	if (Mat) C->SetMaterial(0, Mat);
	if (bCollide)
	{
		C->SetCollisionProfileName(TEXT("BlockAll"));
	}
	else
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	C->RegisterComponent();
	Owner->AddInstanceComponent(C);
	return C;
}

UStaticMeshComponent* SDAssets::AddBox(AActor* Owner, USceneComponent* Parent, const FVector& Center, const FVector& Size,
	const FRotator& Rot, UMaterialInterface* Mat, bool bCollide)
{
	return AddMesh(Owner, Parent, Cube(), Center, Size / 100.f, Rot, Mat, bCollide);
}

void SDAssets::SetFloatProp(UObject* Obj, FName Name, float Value)
{
	if (!Obj) return;
	if (FFloatProperty* P = FindFProperty<FFloatProperty>(Obj->GetClass(), Name))
	{
		P->SetPropertyValue_InContainer(Obj, Value);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[skydrop] no float property %s on %s"), *Name.ToString(), *Obj->GetClass()->GetName());
	}
}

void SDAssets::SetColorProp(UObject* Obj, FName Name, const FLinearColor& Value)
{
	if (!Obj) return;
	if (FStructProperty* P = FindFProperty<FStructProperty>(Obj->GetClass(), Name))
	{
		if (P->Struct == TBaseStructure<FLinearColor>::Get())
		{
			*P->ContainerPtrToValuePtr<FLinearColor>(Obj) = Value;
			return;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[skydrop] no color property %s on %s"), *Name.ToString(), *Obj->GetClass()->GetName());
}

void SDAssets::SetBoolProp(UObject* Obj, FName Name, bool bValue)
{
	if (!Obj) return;
	if (FBoolProperty* P = FindFProperty<FBoolProperty>(Obj->GetClass(), Name))
	{
		P->SetPropertyValue_InContainer(Obj, bValue);
	}
}
