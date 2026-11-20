#include "SDInterior.h"
#include "SDAssets.h"
#include "SDAudio.h"
#include "Components/StaticMeshComponent.h"

FVector ASDFurniture::Size(ESDFurn K)
{
	switch (K)
	{
	case ESDFurn::Bed: return FVector(215, 124, 95);
	case ESDFurn::Nightstand: return FVector(45, 55, 95);
	case ESDFurn::Desk: return FVector(65, 140, 115);
	case ESDFurn::OfficeChair: return FVector(55, 55, 110);
	case ESDFurn::Couch: return FVector(85, 210, 85);
	case ESDFurn::Armchair: return FVector(85, 100, 85);
	case ESDFurn::CoffeeTable: return FVector(60, 110, 45);
	case ESDFurn::Bookshelf: return FVector(36, 100, 192);
	case ESDFurn::FloorLamp: return FVector(40, 40, 180);
	case ESDFurn::Plant: return FVector(50, 50, 130);
	case ESDFurn::Crate: return FVector(36, 46, 32);
	case ESDFurn::Dresser: return FVector(55, 110, 96);
	case ESDFurn::Fridge: return FVector(70, 80, 182);
	case ESDFurn::Counter: return FVector(64, 164, 95);
	case ESDFurn::TVStand: return FVector(45, 140, 125);
	case ESDFurn::FilingCabinet: return FVector(60, 48, 130);
	case ESDFurn::DiningTable: return FVector(150, 150, 95);
	default: return FVector(50, 50, 50);
	}
}

void ASDFurniture::Build(ESDFurn InKind, int32 Seed, const FLinearColor& Accent)
{
	using namespace SDAssets;
	Kind = InKind;
	Type = ESDResource::Container; // only used for harvest feedback
	HP = 100.f;                    // pickaxe swings do 50: two hits
	PerHit = 5;
	Gives = (Kind == ESDFurn::Fridge || Kind == ESDFurn::FilingCabinet) ? ESDMat::Metal : ESDMat::Wood;
	FRandomStream R(Seed);

	UMaterialInterface* WoodL = Surface(ESDSurf::Wood, FLinearColor(0.5f, 0.34f, 0.2f), 0.f, 0.7f);
	UMaterialInterface* WoodD = Surface(ESDSurf::Wood, FLinearColor(0.24f, 0.14f, 0.08f), 0.f, 0.6f);
	UMaterialInterface* Fabric = Solid(Accent, 0.f, 0.95f);
	UMaterialInterface* FabricHi = Solid(Accent * 1.25f, 0.f, 0.95f);
	UMaterialInterface* White = Solid(FLinearColor(0.82f, 0.82f, 0.8f), 0.f, 0.7f);
	UMaterialInterface* Dark = Solid(FLinearColor(0.04f, 0.04f, 0.05f), 0.f, 0.5f);
	UMaterialInterface* Grey = Solid(FLinearColor(0.45f, 0.47f, 0.5f), 0.f, 0.4f, 0.6f);
	UMaterialInterface* Screen = Solid(FLinearColor(0.25f, 0.5f, 0.7f), 1.2f, 0.2f);
	UMaterialInterface* Glow = Solid(FLinearColor(1.f, 0.86f, 0.6f), 2.5f, 0.6f);

	auto Box = [&](const FVector& C, const FVector& S, UMaterialInterface* M, const FRotator& Rot = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* P = AddBox(this, Body, C, S, Rot, M, true);
		P->SetCullDistance(12000.f);
		return P;
	};
	auto Cyl = [&](const FVector& C, const FVector& Scale, UMaterialInterface* M)
	{
		UStaticMeshComponent* P = AddMesh(this, Body, Cylinder(), C, Scale, FRotator::ZeroRotator, M, true);
		P->SetCullDistance(12000.f);
		return P;
	};
	auto Legs = [&](float Dx, float Dy, float Hgt, UMaterialInterface* M)
	{
		for (const float Sx : { -1.f, 1.f })
			for (const float Sy : { -1.f, 1.f })
				Box(FVector(Sx * Dx, Sy * Dy, Hgt * 0.5f), FVector(5, 5, Hgt), M);
	};
	const FLinearColor Stripe[4] = { FLinearColor(0.85f, 0.3f, 0.2f), FLinearColor(0.95f, 0.75f, 0.2f), FLinearColor(0.2f, 0.55f, 0.75f), FLinearColor(0.3f, 0.65f, 0.35f) };

	switch (Kind)
	{
	case ESDFurn::Bed:
		Box(FVector(0, 0, 24), FVector(210, 120, 28), WoodL);
		Box(FVector(0, 0, 47), FVector(198, 110, 20), White);
		for (int32 k = 0; k < 4; ++k) Box(FVector(-5 + k * 30, 0, 59), FVector(30, 114, 7), Solid(Stripe[(k + Seed) & 3], 0.f, 0.95f));
		Box(FVector(-80, 0, 63), FVector(38, 72, 14), White);
		Box(FVector(-106, 0, 50), FVector(10, 126, 98), WoodL);
		Box(FVector(106, 0, 30), FVector(10, 126, 58), WoodL);
		break;
	case ESDFurn::Nightstand:
		Box(FVector(0, 0, 27), FVector(45, 55, 54), WoodL);
		Box(FVector(23, 0, 38), FVector(2, 44, 3), WoodD);
		Box(FVector(23, 0, 20), FVector(2, 44, 3), WoodD);
		Cyl(FVector(-4, 10, 62), FVector(0.1f, 0.1f, 0.16f), Dark);
		Cyl(FVector(-4, 10, 80), FVector(0.3f, 0.3f, 0.2f), Glow);
		break;
	case ESDFurn::Desk:
		Box(FVector(0, 0, 75), FVector(65, 140, 5), WoodL);
		Legs(28, 64, 73, WoodD);
		Box(FVector(-14, 0, 81), FVector(10, 12, 8), Dark);
		Box(FVector(-14, 0, 102), FVector(8, 50, 36), Dark);
		Box(FVector(-9.5f, 0, 102), FVector(1, 44, 30), Screen);
		Box(FVector(12, 0, 78.5f), FVector(16, 42, 2), Grey);
		break;
	case ESDFurn::OfficeChair:
		Box(FVector(0, 0, 47), FVector(50, 50, 8), Fabric);
		Box(FVector(-23, 0, 80), FVector(8, 48, 56), Fabric);
		Cyl(FVector(0, 0, 24), FVector(0.06f, 0.06f, 0.42f), Dark);
		Box(FVector(0, 0, 4), FVector(62, 6, 5), Dark);
		Box(FVector(0, 0, 4), FVector(6, 62, 5), Dark);
		break;
	case ESDFurn::Couch:
	case ESDFurn::Armchair:
	{
		const float Wd = Kind == ESDFurn::Couch ? 210.f : 100.f;
		Box(FVector(0, 0, 20), FVector(85, Wd, 40), Fabric);
		const int32 N = Kind == ESDFurn::Couch ? 2 : 1;
		const float Cw = (Wd - 40.f) / N;
		for (int32 k = 0; k < N; ++k) Box(FVector(8, -Wd * 0.5f + 20.f + Cw * (k + 0.5f), 46), FVector(62, Cw - 4.f, 14), FabricHi);
		Box(FVector(-31, 0, 60), FVector(22, Wd, 46), Fabric);
		Box(FVector(0, -Wd * 0.5f + 10.f, 30), FVector(85, 20, 58), Fabric);
		Box(FVector(0, Wd * 0.5f - 10.f, 30), FVector(85, 20, 58), Fabric);
		break;
	}
	case ESDFurn::CoffeeTable:
		Box(FVector(0, 0, 40), FVector(60, 110, 5), WoodD);
		Legs(25, 50, 38, WoodD);
		break;
	case ESDFurn::Bookshelf:
	{
		Box(FVector(-16, 0, 96), FVector(3, 100, 192), WoodD);
		Box(FVector(0, -48, 96), FVector(35, 4, 192), WoodD);
		Box(FVector(0, 48, 96), FVector(35, 4, 192), WoodD);
		const float Shelves[5] = { 3.f, 48.f, 93.f, 138.f, 189.f };
		for (const float Z : Shelves) Box(FVector(0, 0, Z), FVector(35, 96, 3), WoodD);
		for (int32 s = 0; s < 4; ++s)
		{
			float Y = -45.f;
			while (Y < 40.f)
			{
				const float Bw = R.FRandRange(5.f, 11.f), Bh = R.FRandRange(24.f, 38.f);
				if (R.FRand() < 0.85f)
				{
					const FLinearColor Bc = FLinearColor::MakeFromHSV8((uint8)R.RandRange(0, 255), (uint8)R.RandRange(90, 200), (uint8)R.RandRange(90, 200));
					UStaticMeshComponent* Bk = AddBox(this, Body, FVector(2, Y + Bw * 0.5f, Shelves[s] + 1.5f + Bh * 0.5f), FVector(24, Bw, Bh), FRotator::ZeroRotator, Solid(Bc, 0.f, 0.8f), false);
					Bk->SetCullDistance(6000.f);
				}
				Y += Bw + 0.6f;
			}
		}
		break;
	}
	case ESDFurn::FloorLamp:
		Cyl(FVector(0, 0, 2), FVector(0.3f, 0.3f, 0.04f), Dark);
		Cyl(FVector(0, 0, 82), FVector(0.04f, 0.04f, 1.6f), Dark);
		Cyl(FVector(0, 0, 165), FVector(0.42f, 0.42f, 0.24f), Glow);
		break;
	case ESDFurn::Plant:
	{
		Cyl(FVector(0, 0, 21), FVector(0.42f, 0.42f, 0.42f), Solid(FLinearColor(0.08f, 0.09f, 0.11f), 0.f, 0.5f));
		UMaterialInterface* Leaf = Surface(ESDSurf::Leaf, FLinearColor(0.12f, 0.42f, 0.08f), 0.f, 0.8f);
		for (int32 k = 0; k < 6; ++k)
		{
			const float A = k * 60.f + R.FRandRange(-15.f, 15.f);
			const FVector Dir = FRotator(0, A, 0).Vector();
			UStaticMeshComponent* L = AddMesh(this, Body, Sphere(), FVector(0, 0, 60) + Dir * 22.f + FVector(0, 0, R.FRandRange(10.f, 50.f)),
				FVector(0.22f, 0.5f, 0.12f), FRotator(R.FRandRange(-40.f, -20.f), A + 90.f, 0), Leaf, false);
			L->SetCullDistance(9000.f);
		}
		Cyl(FVector(0, 0, 70), FVector(0.05f, 0.05f, 0.6f), Leaf);
		break;
	}
	case ESDFurn::Crate:
		Box(FVector(0, 0, 16), FVector(36, 46, 32), Solid(Accent.GetLuminance() > 0.25f ? Accent : FLinearColor(0.1f, 0.35f, 0.85f), 0.f, 0.5f));
		Box(FVector(0, 0, 31), FVector(30, 40, 2), Dark);
		break;
	case ESDFurn::Dresser:
		Box(FVector(0, 0, 48), FVector(55, 110, 96), WoodL);
		for (int32 k = 0; k < 3; ++k)
		{
			Box(FVector(28, 0, 22 + k * 28), FVector(2, 100, 2.5f), WoodD);
			Box(FVector(29, -25, 34 + k * 28), FVector(3, 6, 3), Grey);
			Box(FVector(29, 25, 34 + k * 28), FVector(3, 6, 3), Grey);
		}
		break;
	case ESDFurn::Fridge:
		Box(FVector(0, 0, 91), FVector(70, 80, 182), White);
		Box(FVector(35.5f, 0, 125), FVector(1, 78, 2), Grey);
		Box(FVector(37, 30, 150), FVector(4, 4, 40), Grey);
		Box(FVector(37, 30, 85), FVector(4, 4, 50), Grey);
		break;
	case ESDFurn::Counter:
		Box(FVector(0, 0, 44), FVector(60, 160, 88), Solid(FLinearColor(0.75f, 0.72f, 0.64f), 0.f, 0.6f));
		Box(FVector(2, 0, 90.5f), FVector(64, 164, 5), Solid(FLinearColor(0.12f, 0.12f, 0.13f), 0.f, 0.3f));
		Box(FVector(30.5f, -40, 50), FVector(1, 2, 70), WoodD);
		Box(FVector(30.5f, 40, 50), FVector(1, 2, 70), WoodD);
		break;
	case ESDFurn::TVStand:
		Box(FVector(0, 0, 24), FVector(45, 140, 48), WoodD);
		Box(FVector(-8, 0, 52), FVector(10, 30, 8), Dark);
		Box(FVector(-8, 0, 90), FVector(6, 120, 70), Dark);
		Box(FVector(-4.5f, 0, 90), FVector(1, 112, 62), Screen);
		break;
	case ESDFurn::FilingCabinet:
		Box(FVector(0, 0, 65), FVector(60, 48, 130), Grey);
		for (int32 k = 0; k < 4; ++k)
		{
			Box(FVector(30.5f, 0, 8 + k * 31.f), FVector(1, 44, 2), Dark);
			Box(FVector(31.5f, 0, 25 + k * 31.f), FVector(3, 14, 3), Dark);
		}
		break;
	case ESDFurn::DiningTable:
		Box(FVector(0, 0, 75), FVector(90, 150, 5), WoodL);
		Legs(38, 68, 73, WoodD);
		for (const float Sx : { -1.f, 1.f })
		{
			Box(FVector(Sx * 62, 0, 45), FVector(40, 40, 5), WoodL);
			Box(FVector(Sx * 82, 0, 70), FVector(5, 40, 50), WoodL);
		}
		break;
	default: break;
	}
	BaseScale = FVector::OneVector;
}

void ASDFurniture::Smash()
{
	const FVector L = GetActorLocation();
	SDAudio::Play(this, ESDSfx::Build, &L, 0.6f);
	Destroy();
}
