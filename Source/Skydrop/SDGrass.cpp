#include "SDGrass.h"
#include "SDSettings.h"
#include "SDWorld.h"
#include "SDAssets.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"

ASDGrass::ASDGrass()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void ASDGrass::BuildCell(const FIntPoint& Key)
{
	if (!Mat)
	{
		Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Skydrop/Materials/M_GrassBlades.M_GrassBlades"), nullptr, LOAD_NoWarn);
	}
	UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
	H->SetStaticMesh(SDAssets::Plane());
	if (Mat) H->SetMaterial(0, Mat);
	H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	H->SetCastShadow(false);
	H->SetCullDistances(FMath::RoundToInt(Radius * 0.85f), FMath::RoundToInt(Radius));
	H->NumCustomDataFloats = 3; // ground colour under the tuft
	H->SetupAttachment(Root);
	H->RegisterComponent();
	Cells.Add(Key, H);

	FRandomStream R(Key.X * 73856093 ^ Key.Y * 19349663);
	TArray<FTransform> Ts;
	TArray<FLinearColor> Cols;
	Ts.Reserve(TuftsPerCell * 3);
	Cols.Reserve(TuftsPerCell * 3);
	FCollisionQueryParams QP(SCENE_QUERY_STAT(GrassPlace), true);
	for (int32 i = 0; i < TuftsPerCell; ++i)
	{
		const float X = (Key.X + R.FRand()) * CellSize;
		const float Y = (Key.Y + R.FRand()) * CellSize;
		const float Hh = R.FRandRange(38.f, 78.f);
		const float Wd = R.FRandRange(45.f, 80.f);
		const float Yaw = R.FRand() * 120.f;
		if (!Island->IsGrassy(X, Y)) continue;
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, FVector(X, Y, 20000.f), FVector(X, Y, -1000.f), ECC_WorldStatic, QP)) continue;
		if (Hit.GetComponent() != Island->Terrain || Hit.ImpactNormal.Z < 0.8f || Hit.ImpactPoint.Z < 15.f) continue;
		const FLinearColor G = Island->GroundColorAt(X, Y, Hit.ImpactPoint.Z, Hit.ImpactNormal.Z);
		for (int32 c = 0; c < 3; ++c)
		{
			// plane stood upright (local X = width, local Y = height), three cards per tuft with a slight random tilt
			const FRotator Rot(R.FRandRange(-9.f, 9.f), Yaw + c * 60.f + R.FRandRange(-12.f, 12.f), 90.f);
			const float S = R.FRandRange(0.8f, 1.15f);
			Ts.Add(FTransform(Rot, FVector(X + R.FRandRange(-12.f, 12.f), Y + R.FRandRange(-12.f, 12.f), Hit.ImpactPoint.Z + Hh * S * 0.5f - 8.f),
				FVector(Wd * S / 100.f, Hh * S / 100.f, 1.f)));
			Cols.Add(G);
		}
	}
	if (Ts.Num() > 0)
	{
		const TArray<int32> Idx = H->AddInstances(Ts, true, true);
		for (int32 k = 0; k < Idx.Num() && k < Cols.Num(); ++k)
		{
			H->SetCustomData(Idx[k], { Cols[k].R, Cols[k].G, Cols[k].B }, false);
		}
		H->MarkRenderStateDirty();
	}
}

void ASDGrass::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Island) return;
	// grass quality from settings: rebuild the ring when it changes
	const int32 Lv = USDSettings::Get()->Grass;
	if (Lv != AppliedLevel)
	{
		for (auto& Cell : Cells) if (Cell.Value) Cell.Value->DestroyComponent();
		Cells.Reset();
		AppliedLevel = Lv;
		Radius = Lv == 1 ? 3200.f : 5000.f;
		TuftsPerCell = Lv == 1 ? 140 : 230;
	}
	if (Lv == 0) return;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager) return;
	const FVector View = PC->PlayerCameraManager->GetCameraLocation();
	const FIntPoint C(FMath::FloorToInt(View.X / CellSize), FMath::FloorToInt(View.Y / CellSize));
	const int32 Rc = FMath::CeilToInt(Radius / CellSize);

	for (auto It = Cells.CreateIterator(); It; ++It)
	{
		if (FMath::Abs(It.Key().X - C.X) > Rc + 2 || FMath::Abs(It.Key().Y - C.Y) > Rc + 2)
		{
			if (It.Value()) It.Value()->DestroyComponent();
			It.RemoveCurrent();
		}
	}

	// nothing to see from high up
	if (View.Z - FMath::Max(Island->HeightAt(View.X, View.Y), 0.f) > Radius) return;

	int32 Budget = 4;
	for (int32 d = 0; d <= Rc; ++d)
	{
		for (int32 dx = -d; dx <= d; ++dx)
		{
			for (int32 dy = -d; dy <= d; ++dy)
			{
				if (FMath::Max(FMath::Abs(dx), FMath::Abs(dy)) != d) continue;
				if (dx * dx + dy * dy > Rc * Rc) continue;
				const FIntPoint K(C.X + dx, C.Y + dy);
				if (Cells.Contains(K)) continue;
				BuildCell(K);
				if (--Budget <= 0) return;
			}
		}
	}
}
