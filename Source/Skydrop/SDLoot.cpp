#include "SDLoot.h"
#include "SDAssets.h"
#include "SDCharacter.h"
#include "SDGameMode.h"
#include "SDWorld.h"
#include "SDAudio.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"

// ---------------------------------------------------------------- Resource

ASDResource::ASDResource()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
}

namespace
{
	// Imported Sketchfab nature pieces (see Tools/import_nature.py for credits)
	UStaticMesh* NatureMesh(const TCHAR* Name)
	{
		static TMap<FString, UStaticMesh*> Cache;
		if (UStaticMesh** Found = Cache.Find(Name)) return *Found;
		const FString Path = FString::Printf(TEXT("/Game/Skydrop/Nature/%s/%s/StaticMeshes/%s.%s"), Name, Name, Name, Name);
		UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn);
		if (M) M->AddToRoot();
		Cache.Add(Name, M);
		return M;
	}
}

void ASDResource::Setup(ESDResource InType, float Size, int32 Seed)
{
	Type = InType;
	FRandomStream R(Seed);
	using namespace SDAssets;

	// real models first; the primitive versions below are only a fallback
	{
		UStaticMesh* Mesh = nullptr;
		float Scale = 1.f;
		switch (Type)
		{
		case ESDResource::Tree:
		{
			const float Pick = R.FRand();
			if (Pick < 0.55f)      { Mesh = NatureMesh(TEXT("SM_BigOak")); Scale = R.FRandRange(0.33f, 0.43f) * Size; }
			else if (Pick < 0.8f)  { Mesh = NatureMesh(TEXT("SM_Pine3"));  Scale = R.FRandRange(1.05f, 1.4f) * Size; }
			else                   { Mesh = NatureMesh(TEXT("SM_Birch2")); Scale = R.FRandRange(1.0f, 1.3f) * Size; }
			Gives = ESDMat::Wood; HP = 300.f * Size; PerHit = 7;
			break;
		}
		case ESDResource::Palm:
			Mesh = NatureMesh(TEXT("SM_Palm")); Scale = R.FRandRange(0.010f, 0.014f) * Size;
			Gives = ESDMat::Wood; HP = 220.f * Size; PerHit = 6;
			break;
		case ESDResource::Rock:
			if (Size > 1.55f)      { Mesh = NatureMesh(TEXT("SM_RockBig1"));   Scale = R.FRandRange(0.9f, 1.5f); HP = 700.f; }
			else if (Size > 1.15f) { Mesh = NatureMesh(TEXT("SM_RockMid1"));   Scale = R.FRandRange(0.8f, 1.2f); HP = 450.f; }
			else                   { Mesh = NatureMesh(TEXT("SM_RockSmall1")); Scale = R.FRandRange(0.9f, 1.5f); HP = 250.f; }
			Gives = ESDMat::Stone; PerHit = 6;
			break;
		default:
			break;
		}
		if (Mesh)
		{
			UStaticMeshComponent* C = AddMesh(this, Body, Mesh, FVector::ZeroVector, FVector(Scale), FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), nullptr, true);
			C->SetCullDistance(Type == ESDResource::Rock ? 70000.f : 150000.f);
			WeakSpot = AddMesh(this, Root, Sphere(), FVector::ZeroVector, FVector(0.28f), FRotator::ZeroRotator, Solid(FLinearColor(0.35f, 0.8f, 1.f), 6.f), false);
			WeakSpot->SetVisibility(false);
			WeakSpot->SetCastShadow(false);
			BaseScale = Body->GetRelativeScale3D();
			SetActorTickEnabled(false);
			return;
		}
	}

	switch (Type)
	{
	case ESDResource::Tree:
	{
		Gives = ESDMat::Wood;
		HP = 250.f * Size;
		PerHit = 7;
		const float TrunkH = 380.f * Size;
		const float TrunkR = 26.f * Size;
		UMaterialInterface* Bark = Surface(ESDSurf::Rock, FLinearColor(0.16f, 0.09f, 0.045f), 0.f, 0.9f);
		AddMesh(this, Body, Cylinder(), FVector(0, 0, TrunkH * 0.5f), FVector(TrunkR / 50.f, TrunkR / 50.f, TrunkH / 100.f), FRotator::ZeroRotator, Bark);
		const bool bPine = R.FRand() < 0.5f;
		const FLinearColor Leaf = bPine ? FLinearColor(0.02f, 0.14f + R.FRand() * 0.05f, 0.07f + R.FRand() * 0.03f)
		                                : FLinearColor(0.05f + R.FRand() * 0.07f, 0.24f + R.FRand() * 0.12f, 0.04f + R.FRand() * 0.03f);
		UMaterialInterface* LeafM = Surface(ESDSurf::Leaf, Leaf, 0.f, 0.85f);
		if (bPine)
		{
			for (int32 i = 0; i < 3; ++i)
			{
				const float S = (3.4f - i * 0.8f) * Size;
				AddMesh(this, Body, Cone(), FVector(0, 0, TrunkH * 0.55f + i * 150.f * Size + 120.f * Size), FVector(S, S, 2.4f * Size), FRotator::ZeroRotator, LeafM);
			}
		}
		else
		{
			const int32 N = 3 + R.RandRange(0, 2);
			for (int32 i = 0; i < N; ++i)
			{
				const float S = (2.6f + R.FRand() * 1.4f) * Size;
				const FVector Off(R.FRandRange(-90.f, 90.f) * Size, R.FRandRange(-90.f, 90.f) * Size, TrunkH + R.FRandRange(-40.f, 140.f) * Size);
				AddMesh(this, Body, Sphere(), Off, FVector(S), FRotator::ZeroRotator, LeafM);
			}
		}
		break;
	}
	case ESDResource::Rock:
	{
		Gives = ESDMat::Stone;
		HP = 420.f * Size;
		PerHit = 6;
		const float G = 0.35f + R.FRand() * 0.12f;
		UMaterialInterface* RockM = Surface(ESDSurf::Rock, FLinearColor(G, G, G * 1.04f), 0.f, 0.9f);
		const int32 N = 2 + R.RandRange(0, 2);
		for (int32 i = 0; i < N; ++i)
		{
			const FVector S(R.FRandRange(2.2f, 3.6f) * Size, R.FRandRange(2.0f, 3.2f) * Size, R.FRandRange(1.3f, 2.4f) * Size);
			AddMesh(this, Body, Sphere(), FVector(R.FRandRange(-80.f, 80.f) * Size, R.FRandRange(-80.f, 80.f) * Size, 30.f * Size), S,
				FRotator(R.FRandRange(-15.f, 15.f), R.FRandRange(0.f, 360.f), R.FRandRange(-15.f, 15.f)), RockM);
		}
		break;
	}
	case ESDResource::Car:
	{
		Gives = ESDMat::Metal;
		HP = 600.f;
		PerHit = 6;
		static const FLinearColor Paint[] = { FLinearColor(0.6f,0.08f,0.06f), FLinearColor(0.08f,0.25f,0.55f), FLinearColor(0.85f,0.75f,0.2f), FLinearColor(0.15f,0.4f,0.25f), FLinearColor(0.8f,0.8f,0.82f) };
		UMaterialInterface* PaintM = Solid(Paint[R.RandRange(0, 4)], 0.f, 0.3f, 0.4f);
		UMaterialInterface* Glass = Solid(FLinearColor(0.05f, 0.08f, 0.1f), 0.f, 0.1f, 0.2f);
		UMaterialInterface* Tire = Solid(FLinearColor(0.02f, 0.02f, 0.02f), 0.f, 0.9f);
		AddBox(this, Body, FVector(0, 0, 75), FVector(420, 180, 80), FRotator::ZeroRotator, PaintM);
		AddBox(this, Body, FVector(-20, 0, 150), FVector(220, 165, 70), FRotator::ZeroRotator, Glass);
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector P((i < 2 ? 135.f : -135.f), (i % 2 ? 92.f : -92.f), 38.f);
			AddMesh(this, Body, Cylinder(), P, FVector(0.75f, 0.75f, 0.3f), FRotator(0, 0, 90), Tire);
		}
		break;
	}
	case ESDResource::Container:
	{
		Gives = ESDMat::Metal;
		HP = 800.f;
		PerHit = 7;
		static const FLinearColor Paint[] = { FLinearColor(0.45f,0.06f,0.04f), FLinearColor(0.05f,0.18f,0.45f), FLinearColor(0.08f,0.30f,0.12f), FLinearColor(0.70f,0.32f,0.04f) };
		UMaterialInterface* M = Surface(ESDSurf::Metal, Paint[R.RandRange(0, 3)], 0.f, 0.45f, 0.5f);
		const int32 Stack = R.FRand() < 0.35f ? 2 : 1;
		for (int32 s = 0; s < Stack; ++s)
		{
			AddBox(this, Body, FVector(0, 0, 130.f + s * 262.f), FVector(480, 240, 260), FRotator(0, s * R.FRandRange(-8.f, 8.f), 0), M);
			for (int32 r = -4; r <= 4; ++r) // corrugation ribs
			{
				AddBox(this, Body, FVector(r * 52.f, 0, 130.f + s * 262.f), FVector(10, 248, 250), FRotator::ZeroRotator, M, false);
			}
		}
		break;
	}
	case ESDResource::Cactus:
	{
		Gives = ESDMat::Wood;
		HP = 150.f * Size;
		PerHit = 6;
		UMaterialInterface* M = Surface(ESDSurf::Leaf, FLinearColor(0.10f, 0.30f, 0.08f), 0.f, 0.7f);
		const float Hh = 320.f * Size;
		AddMesh(this, Body, Cylinder(), FVector(0, 0, Hh * 0.5f), FVector(0.55f * Size, 0.55f * Size, Hh / 100.f), FRotator::ZeroRotator, M);
		AddMesh(this, Body, Sphere(), FVector(0, 0, Hh), FVector(0.55f * Size), FRotator::ZeroRotator, M);
		for (int32 a = 0; a < 2; ++a)
		{
			const float S = a ? 1.f : -1.f;
			const float Z = Hh * R.FRandRange(0.35f, 0.6f);
			AddMesh(this, Body, Cylinder(), FVector(S * 55.f * Size, 0, Z), FVector(0.35f * Size, 0.35f * Size, 0.5f * Size), FRotator(0, 0, 90), M);
			AddMesh(this, Body, Cylinder(), FVector(S * 80.f * Size, 0, Z + 60.f * Size), FVector(0.35f * Size, 0.35f * Size, 1.2f * Size), FRotator::ZeroRotator, M);
		}
		break;
	}
	}

	// cull small objects at distance so a big map stays fast
	{
		const float Cull = Type == ESDResource::Tree ? 60000.f : Type == ESDResource::Rock ? 40000.f : 30000.f;
		TArray<UStaticMeshComponent*> Comps;
		GetComponents<UStaticMeshComponent>(Comps);
		for (UStaticMeshComponent* C : Comps) C->SetCullDistance(Cull);
	}

	WeakSpot = AddMesh(this, Root, Sphere(), FVector::ZeroVector, FVector(0.28f), FRotator::ZeroRotator,
		Solid(FLinearColor(0.35f, 0.8f, 1.f), 6.f), false);
	WeakSpot->SetVisibility(false);
	WeakSpot->SetCastShadow(false);
	BaseScale = Body->GetRelativeScale3D();
	SetActorTickEnabled(false);
}

void ASDResource::Harvest(ASDCharacter* By, const FHitResult& Hit)
{
	if (!By) return;
	const bool bWeak = WeakSpot && WeakSpot->IsVisible() && FVector::Dist(WeakSpot->GetComponentLocation(), Hit.ImpactPoint) < 60.f;
	const float Dmg = bWeak ? 100.f : 50.f;
	const int32 Gain = bWeak ? PerHit * 2 : PerHit;
	By->Mats[(int32)Gives] = FMath::Min(999, By->Mats[(int32)Gives] + Gain);
	By->OnHarvest(Hit.ImpactPoint, Gain, bWeak);
	HP -= Dmg;
	Wobble = 0.25f;
	SetActorTickEnabled(true);

	// move the weak point to a new spot near the hit, on the surface
	if (WeakSpot)
	{
		const FVector N = Hit.ImpactNormal.IsNearlyZero() ? -By->GetActorForwardVector() : FVector(Hit.ImpactNormal);
		FVector Tan = FVector::CrossProduct(N, FVector::UpVector).GetSafeNormal();
		if (Tan.IsNearlyZero()) Tan = FVector::RightVector;
		const FVector Up2 = FVector::CrossProduct(Tan, N).GetSafeNormal();
		const float A = FMath::FRandRange(0.f, 2.f * PI);
		const float D = FMath::FRandRange(40.f, 85.f);
		FVector Guess = Hit.ImpactPoint + (Tan * FMath::Cos(A) + Up2 * FMath::Sin(A)) * D;
		FHitResult SH;
		FCollisionQueryParams QP; QP.bTraceComplex = true;
		bool bFound = false;
		TArray<UPrimitiveComponent*> Comps;
		GetComponents<UPrimitiveComponent>(Comps);
		for (UPrimitiveComponent* C : Comps)
		{
			if (C == WeakSpot || !C->IsCollisionEnabled()) continue;
			if (C->LineTraceComponent(SH, Guess + N * 120.f, Guess - N * 120.f, QP))
			{
				Guess = SH.ImpactPoint;
				bFound = true;
				break;
			}
		}
		if (!bFound) Guess = Hit.ImpactPoint;
		WeakSpot->SetWorldLocation(Guess + N * 4.f);
		WeakSpot->SetVisibility(true);
	}

	if (HP <= 0.f)
	{
		Destroy();
	}
}

float ASDResource::TakeDamage(float Amount, FDamageEvent const& Ev, AController* Inst, AActor* Causer)
{
	// gunfire chips objects without giving materials
	HP -= Amount * 0.5f;
	Wobble = 0.15f;
	SetActorTickEnabled(true);
	if (HP <= 0.f) Destroy();
	return Amount;
}

void ASDResource::Tick(float Dt)
{
	Super::Tick(Dt);
	if (Wobble > 0.f)
	{
		Wobble -= Dt;
		const float W = FMath::Max(Wobble, 0.f);
		const float S = 1.f + FMath::Sin(W * 60.f) * W * 0.12f;
		Body->SetRelativeScale3D(BaseScale * FVector(S, S, 1.f / S));
		Body->SetRelativeRotation(FRotator(FMath::Sin(W * 45.f) * W * 6.f, 0, 0));
	}
	else
	{
		Body->SetRelativeScale3D(BaseScale);
		Body->SetRelativeRotation(FRotator::ZeroRotator);
		SetActorTickEnabled(false);
	}
}

// ---------------------------------------------------------------- Pickup

ASDPickup::ASDPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Spin = CreateDefaultSubobject<USceneComponent>(TEXT("Spin"));
	Spin->SetupAttachment(Root);
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Root);
	Glow->SetRelativeLocation(FVector(0, 0, 40));
	Glow->SetCastShadows(false);
	Glow->SetAttenuationRadius(260.f);
	Glow->SetIntensity(1200.f);
	Glow->SetIntensityUnits(ELightUnits::Unitless);
}

void ASDPickup::Setup(const FSDItemStack& InItem)
{
	Item = InItem;
	Phase = FMath::FRand() * 10.f;
	using namespace SDAssets;
	FLinearColor Rar = SDData::RarityColor(Item.Rarity);
	switch (Item.Item)
	{
	case ESDItem::Weapon:
	{
		const FSDWeaponDef& D = SDData::Weapon(Item.Weapon);
		if (UStaticMesh* GunM = GunMesh((uint8)Item.Weapon, Item.Rarity))
		{
			// floor loot: the real model, a bit larger so it reads from a distance, centred on the spin pivot
			const FBox B = GunM->GetBoundingBox();
			const float S = 1.35f;
			AddMesh(this, Spin, GunM, -B.GetCenter() * S, FVector(S), FRotator::ZeroRotator, nullptr, false);
			break;
		}
		AddBox(this, Spin, FVector(0, 0, 0), D.Size * FVector(1, 1.4f, 1.4f), FRotator::ZeroRotator, Solid(D.Body, 0.f, 0.5f, 0.3f), false);
		AddBox(this, Spin, FVector(-D.Size.X * 0.1f, 0, -D.Size.Z * 0.9f), FVector(10, 8, 18), FRotator(0, 0, 0), Solid(D.Body, 0.f, 0.5f), false);
		AddBox(this, Spin, FVector(0, 0, D.Size.Z * 0.75f), FVector(D.Size.X * 0.5f, 4, 3), FRotator::ZeroRotator, Solid(Rar, 2.f), false);
		break;
	}
	case ESDItem::Bandage:
		AddMesh(this, Spin, Cylinder(), FVector::ZeroVector, FVector(0.25f, 0.25f, 0.18f), FRotator(90, 0, 0), Solid(FLinearColor(0.9f, 0.88f, 0.8f)), false);
		Rar = SDData::RarityColor(0);
		break;
	case ESDItem::Medkit:
		AddBox(this, Spin, FVector::ZeroVector, FVector(40, 30, 20), FRotator::ZeroRotator, Solid(FLinearColor(0.9f, 0.9f, 0.9f)), false);
		AddBox(this, Spin, FVector(0, 0, 11), FVector(26, 8, 2), FRotator::ZeroRotator, Solid(FLinearColor(0.9f, 0.05f, 0.05f)), false);
		AddBox(this, Spin, FVector(0, 0, 11), FVector(8, 22, 2), FRotator::ZeroRotator, Solid(FLinearColor(0.9f, 0.05f, 0.05f)), false);
		Rar = SDData::RarityColor(1);
		break;
	case ESDItem::MiniShield:
		AddMesh(this, Spin, Sphere(), FVector::ZeroVector, FVector(0.25f), FRotator::ZeroRotator, Solid(FLinearColor(0.15f, 0.5f, 1.f), 1.5f, 0.1f), false);
		Rar = SDData::RarityColor(1);
		break;
	case ESDItem::BigShield:
		AddMesh(this, Spin, Cylinder(), FVector::ZeroVector, FVector(0.25f, 0.25f, 0.45f), FRotator::ZeroRotator, Solid(FLinearColor(0.15f, 0.5f, 1.f), 1.5f, 0.1f), false);
		Rar = SDData::RarityColor(2);
		break;
	case ESDItem::Ammo:
		AddBox(this, Spin, FVector::ZeroVector, FVector(30, 20, 18), FRotator::ZeroRotator, Solid(FLinearColor(0.25f, 0.3f, 0.15f)), false);
		AddBox(this, Spin, FVector(0, 0, 10), FVector(20, 10, 3), FRotator::ZeroRotator, Solid(FLinearColor(0.9f, 0.7f, 0.2f), 0.5f), false);
		Glow->SetIntensity(300.f);
		Rar = FLinearColor(0.9f, 0.85f, 0.6f);
		break;
	case ESDItem::Mats:
		for (int32 i = 0; i < 3; ++i)
		{
			AddBox(this, Spin, FVector(0, 0, i * 9.f), FVector(40, 18, 8), FRotator(0, i * 20.f, 0), Solid(SDData::MatColor((ESDMat)Item.Sub)), false);
		}
		Glow->SetIntensity(200.f);
		Rar = FLinearColor::White;
		break;
	default: break;
	}
	Glow->SetLightColor(Rar);
	TArray<UStaticMeshComponent*> Comps;
	GetComponents<UStaticMeshComponent>(Comps);
	for (UStaticMeshComponent* C : Comps) C->SetCullDistance(15000.f);
}

void ASDPickup::Tick(float Dt)
{
	Super::Tick(Dt);
	Age += Dt;
	if (!bSettled)
	{
		Velocity.Z -= 2200.f * Dt;
		FVector Next = GetActorLocation() + Velocity * Dt;
		FHitResult Hit;
		FCollisionQueryParams QP(SCENE_QUERY_STAT(PickupFall), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation() + FVector(0, 0, 10), Next - FVector(0, 0, 30), ECC_WorldStatic, QP))
		{
			Next = Hit.ImpactPoint + FVector(0, 0, 30);
			bSettled = Velocity.Z < 0.f;
			Velocity = FVector::ZeroVector;
		}
		SetActorLocation(Next);
	}
	Spin->SetRelativeLocation(FVector(0, 0, FMath::Sin(Age * 2.f + Phase) * 6.f));
	Spin->SetRelativeRotation(FRotator(0, (Age + Phase) * 60.f, 0));

	if (IsAutoPickup() && Age > 0.6f)
	{
		AutoTimer -= Dt;
		if (AutoTimer <= 0.f)
		{
			AutoTimer = 0.15f;
			if (DroppedBy.IsValid() && FVector::DistSquared(DroppedBy->GetActorLocation(), GetActorLocation()) > FMath::Square(300.f)) DroppedBy = nullptr;
			for (TActorIterator<ASDCharacter> It(GetWorld()); It; ++It)
			{
				ASDCharacter* C = *It;
				if (C->bDead || C->Air != ESDAir::Ground || C == DroppedBy.Get()) continue;
				if (FVector::DistSquared(C->GetActorLocation(), GetActorLocation()) < FMath::Square(140.f))
				{
					C->GiveItem(Item, false);
					if (C->IsPlayerControlled()) SDAudio::Play(C, ESDSfx::Pickup);
					Destroy();
					return;
				}
			}
		}
	}
}

// ---------------------------------------------------------------- Chest

ASDChest::ASDChest()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Lid = CreateDefaultSubobject<USceneComponent>(TEXT("Lid"));
	Lid->SetupAttachment(Root);
	Lid->SetRelativeLocation(FVector(-30, 0, 60));
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Root);
	Glow->SetRelativeLocation(FVector(0, 0, 90));
	Glow->SetCastShadows(false);
	Glow->SetLightColor(FLinearColor(1.f, 0.75f, 0.2f));
	Glow->SetIntensityUnits(ELightUnits::Unitless);
	Glow->SetIntensity(2500.f);
	Glow->SetAttenuationRadius(400.f);
}

void ASDChest::BeginPlay()
{
	Super::BeginPlay();
	using namespace SDAssets;
	UMaterialInterface* Gold = Solid(FLinearColor(0.95f, 0.65f, 0.12f), 0.4f, 0.3f, 0.9f);
	UMaterialInterface* Wood = Surface(ESDSurf::Wood, FLinearColor(0.35f, 0.18f, 0.07f), 0.f, 0.8f);
	AddBox(this, Root, FVector(0, 0, 30), FVector(60, 90, 60), FRotator::ZeroRotator, Wood);
	AddBox(this, Root, FVector(0, 0, 30), FVector(64, 94, 10), FRotator::ZeroRotator, Gold);
	AddBox(this, Lid, FVector(30, 0, 10), FVector(62, 92, 22), FRotator::ZeroRotator, Gold, false);
}

void ASDChest::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!bOpened)
	{
		Glow->SetIntensity(2000.f + FMath::Sin(GetWorld()->GetTimeSeconds() * 4.f) * 800.f);
		return;
	}
	LidAnim = FMath::Min(1.f, LidAnim + Dt * 4.f);
	Lid->SetRelativeRotation(FRotator(LidAnim * 75.f, 0, 0));
	Glow->SetIntensity(FMath::Max(0.f, Glow->Intensity - Dt * 4000.f));
	if (LidAnim >= 1.f && Glow->Intensity <= 0.f) SetActorTickEnabled(false);
}

void ASDChest::Open(ASDCharacter* By)
{
	if (bOpened) return;
	bOpened = true;
	{ const FVector L = GetActorLocation(); SDAudio::Play(this, ESDSfx::Chest, &L); }
	if (By && !By->bIsBot)
	{
		if (ASDGameMode* GM = GetWorld()->GetAuthGameMode<ASDGameMode>()) GM->OnPlayerChestOpened();
	}
	const FVector Out = GetActorLocation() + FVector(0, 0, 60);
	UWorld* W = GetWorld();
	const FSDItemStack Wpn = SDLoot::RollWeapon(1);
	SDLoot::Spawn(W, Wpn, Out, true);
	SDLoot::Spawn(W, SDLoot::AmmoFor(Wpn.Weapon), Out, true);
	SDLoot::Spawn(W, FMath::FRand() < 0.6f ? SDLoot::RollConsumable() : SDLoot::RollWeapon(0), Out, true);
	FSDItemStack M; M.Item = ESDItem::Mats; M.Sub = (int32)ESDMat::Wood; M.Count = 30;
	SDLoot::Spawn(W, M, Out, true);
}

// ---------------------------------------------------------------- Tracer

ASDTracer::ASDTracer()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void ASDTracer::Setup(const FVector& From, const FVector& To)
{
	const FVector D = To - From;
	const float Len = D.Size();
	SetActorLocationAndRotation(From, D.Rotation());
	Streak = SDAssets::AddBox(this, Root, FVector(Len * 0.5f, 0, 0), FVector(Len, 1.6f, 1.6f), FRotator::ZeroRotator,
		SDAssets::Solid(FLinearColor(1.f, 0.8f, 0.35f), 25.f), false);
	Streak->SetCastShadow(false);
	SetLifeSpan(0.12f);
}

void ASDTracer::Tick(float Dt)
{
	Super::Tick(Dt);
	Life -= Dt;
	if (Streak)
	{
		const float A = FMath::Clamp(Life / 0.06f, 0.05f, 1.f);
		FVector S = Streak->GetRelativeScale3D();
		S.Y = S.Z = 0.016f * A;
		Streak->SetRelativeScale3D(S);
	}
}

// ---------------------------------------------------------------- Loot tables

FSDItemStack SDLoot::RollWeapon(int32 MinRarity)
{
	FSDItemStack S;
	S.Item = ESDItem::Weapon;
	const float W = FMath::FRand() * 100.f;
	S.Weapon = W < 30 ? ESDWeapon::AR : W < 47 ? ESDWeapon::Pump : W < 57 ? ESDWeapon::Tactical : W < 75 ? ESDWeapon::SMG : W < 90 ? ESDWeapon::Pistol : ESDWeapon::Sniper;
	const float R = FMath::FRand() * 100.f;
	int32 Rar = R < 40 ? 0 : R < 70 ? 1 : R < 88 ? 2 : R < 97 ? 3 : 4;
	if (S.Weapon == ESDWeapon::Sniper) Rar = FMath::Max(Rar, 2);
	S.Rarity = FMath::Max(Rar, MinRarity);
	S.Count = SDData::Weapon(S.Weapon).Mag;
	return S;
}

FSDItemStack SDLoot::RollConsumable()
{
	FSDItemStack S;
	const float R = FMath::FRand() * 100.f;
	if (R < 35) { S.Item = ESDItem::Bandage; S.Count = 5; }
	else if (R < 50) { S.Item = ESDItem::Medkit; S.Count = 1; }
	else if (R < 80) { S.Item = ESDItem::MiniShield; S.Count = 3; }
	else { S.Item = ESDItem::BigShield; S.Count = 1; }
	return S;
}

FSDItemStack SDLoot::AmmoFor(ESDWeapon W)
{
	FSDItemStack S;
	S.Item = ESDItem::Ammo;
	const ESDAmmo A = SDData::Weapon(W).Ammo;
	S.Sub = (int32)A;
	S.Count = A == ESDAmmo::Shells ? 10 : A == ESDAmmo::Heavy ? 6 : A == ESDAmmo::Medium ? 30 : 36;
	return S;
}

ASDPickup* SDLoot::Spawn(UWorld* World, const FSDItemStack& Item, const FVector& At, bool bToss)
{
	if (!World || Item.IsEmpty()) return nullptr;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASDPickup* Pk = World->SpawnActor<ASDPickup>(ASDPickup::StaticClass(), FTransform(At), P);
	if (!Pk) return nullptr;
	Pk->Setup(Item);
	if (bToss)
	{
		const float A = FMath::FRand() * 2.f * PI;
		Pk->Velocity = FVector(FMath::Cos(A) * 220.f, FMath::Sin(A) * 220.f, 520.f);
	}
	return Pk;
}

void SDLoot::SpawnFloorLoot(UWorld* World, const FVector& At)
{
	// some spots hold a second item next to the first
	if (FMath::FRand() < 0.35f) Spawn(World, FMath::FRand() < 0.5f ? RollConsumable() : AmmoFor(RollWeapon(0).Weapon), At + FVector(-45, 35, 0));
	const float R = FMath::FRand();
	if (R < 0.55f)
	{
		const FSDItemStack W = RollWeapon(0);
		Spawn(World, W, At);
		Spawn(World, AmmoFor(W.Weapon), At + FVector(40, 30, 0));
	}
	else if (R < 0.8f)
	{
		Spawn(World, RollConsumable(), At);
	}
	else
	{
		const ESDWeapon Types[] = { ESDWeapon::AR, ESDWeapon::Pump, ESDWeapon::SMG, ESDWeapon::Sniper };	// shells cover both shotguns
		Spawn(World, AmmoFor(Types[FMath::RandRange(0, 3)]), At);
	}
}
