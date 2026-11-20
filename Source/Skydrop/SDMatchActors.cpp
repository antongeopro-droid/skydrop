#include "SDMatchActors.h"
#include "SDAssets.h"
#include "SDGameMode.h"
#include "SDCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"

// ---------------------------------------------------------------- Storm

ASDStorm::ASDStorm()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void ASDStorm::Init(float InIslandR)
{
	IslandR = InIslandR;
	// { wait, shrink seconds, radius fraction, damage per second } - slow, OG-style pacing (~22 min total)
	Phases = {
		{ 150.f, 150.f, 0.55f, 1.f },
		{ 120.f, 120.f, 0.32f, 1.f },
		{ 100.f, 100.f, 0.18f, 2.f },
		{ 80.f, 80.f, 0.09f, 5.f },
		{ 60.f, 70.f, 0.045f, 8.f },
		{ 45.f, 60.f, 0.02f, 10.f },
		{ 30.f, 90.f, 0.00f, 10.f },
	};
	Center = FVector2D::ZeroVector;
	Radius = IslandR * 1.4f;
	NextCenter = Center;
	NextRadius = Radius;
	Wall = SDAssets::AddMesh(this, Root, SDAssets::Cylinder(), FVector::ZeroVector, FVector::OneVector, FRotator::ZeroRotator, SDAssets::Storm(), false);
	Wall->SetCastShadow(false);
	Wall->SetVisibility(false);
	UpdateVisual();
}

void ASDStorm::Start()
{
	bActive = true;
	if (Wall) Wall->SetVisibility(true);
	Index = -1;
	NextPhase();
}

void ASDStorm::NextPhase()
{
	++Index;
	bShrinking = false;
	if (!Phases.IsValidIndex(Index))
	{
		Timer = 1e9f;
		return;
	}
	const FPhase& P = Phases[Index];
	Timer = P.Wait;
	NextRadius = FMath::Max(P.Frac * IslandR, 1.f);
	const float Slack = FMath::Max(0.f, Radius - NextRadius) * 0.8f;
	FVector2D C = Center + FVector2D(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)).GetSafeNormal() * FMath::FRand() * Slack;
	// keep circles over land
	const float MaxFromOrigin = IslandR * 0.6f;
	if (C.Size() > MaxFromOrigin) C = C.GetSafeNormal() * MaxFromOrigin;
	NextCenter = C;
}

void ASDStorm::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!bActive) return;

	Timer -= Dt;
	if (Phases.IsValidIndex(Index))
	{
		const FPhase& P = Phases[Index];
		if (!bShrinking && Timer <= 0.f)
		{
			bShrinking = true;
			Timer = P.Shrink;
			StartRadius = Radius;
			StartCenter = Center;
			Damage = P.Damage;
		}
		if (bShrinking)
		{
			const float A = 1.f - FMath::Clamp(Timer / P.Shrink, 0.f, 1.f);
			Radius = FMath::Lerp(StartRadius, NextRadius, A);
			Center = FMath::Lerp(StartCenter, NextCenter, A);
			if (Timer <= 0.f) NextPhase();
		}
	}
	UpdateVisual();

	DamageTimer -= Dt;
	if (DamageTimer <= 0.f)
	{
		DamageTimer = 1.f;
		if (ASDGameMode* GM = GetWorld()->GetAuthGameMode<ASDGameMode>())
		{
			TArray<TWeakObjectPtr<ASDCharacter>> List = GM->Alive;
			for (const TWeakObjectPtr<ASDCharacter>& W : List)
			{
				ASDCharacter* C = W.Get();
				if (C && !C->bDead && C->Air != ESDAir::Bus && !IsInside(C->GetActorLocation()))
				{
					C->TakeDamage(Damage, FDamageEvent(), nullptr, this);
				}
			}
		}
	}
}

void ASDStorm::UpdateVisual()
{
	if (!Wall) return;
	const float R = FMath::Max(Radius, 10.f);
	Wall->SetRelativeLocation(FVector(Center.X, Center.Y, 10000.f));
	Wall->SetRelativeScale3D(FVector(R / 50.f, R / 50.f, 600.f));
}

// ---------------------------------------------------------------- Airship ("bus")

ASDBus::ASDBus()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
}

void ASDBus::Init(float IslandR)
{
	using namespace SDAssets;
	const float A = FMath::FRand() * 2.f * PI;
	const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
	const FVector Side(-Dir.Y, Dir.X, 0.f);
	const FVector Mid = Side * FMath::FRandRange(-0.35f, 0.35f) * IslandR;
	StartPos = Mid - Dir * IslandR * 1.15f + FVector(0, 0, 24000.f);
	EndPos = Mid + Dir * IslandR * 1.15f + FVector(0, 0, 24000.f);
	Duration = FVector::Dist(StartPos, EndPos) / 4200.f;
	SetActorLocationAndRotation(StartPos, Dir.Rotation());

	// original airship: striped envelope, gondola, fins, propellers
	UMaterialInterface* Env = Solid(FLinearColor(0.85f, 0.88f, 0.92f), 0.f, 0.5f);
	UMaterialInterface* Stripe = Solid(FLinearColor(0.95f, 0.45f, 0.1f), 0.1f, 0.5f);
	UMaterialInterface* Dark = Solid(FLinearColor(0.12f, 0.14f, 0.18f), 0.f, 0.4f, 0.5f);
	AddMesh(this, Body, Sphere(), FVector(0, 0, 0), FVector(26.f, 9.f, 9.f), FRotator::ZeroRotator, Env, false);
	AddMesh(this, Body, Sphere(), FVector(0, 0, 0), FVector(18.f, 9.15f, 9.15f), FRotator::ZeroRotator, Stripe, false);
	AddBox(this, Body, FVector(0, 0, -560), FVector(700, 220, 200), FRotator::ZeroRotator, Dark, false);
	AddBox(this, Body, FVector(-1150, 0, 0), FVector(300, 20, 700), FRotator::ZeroRotator, Stripe, false);
	AddBox(this, Body, FVector(-1150, 0, 0), FVector(300, 700, 20), FRotator::ZeroRotator, Stripe, false);
	for (int32 s = -1; s <= 1; s += 2)
	{
		AddMesh(this, Body, Cylinder(), FVector(-300, s * 300.f, -500), FVector(0.6f, 0.6f, 1.2f), FRotator(90, 0, 0), Dark, false);
	}
}

void ASDBus::Tick(float Dt)
{
	Super::Tick(Dt);
	Time += Dt;
	SetActorLocation(FMath::Lerp(StartPos, EndPos, Progress()));
	Body->SetRelativeRotation(FRotator(FMath::Sin(Time * 0.7f) * 1.5f, 0, FMath::Sin(Time * 0.5f) * 2.f));
	if (Time > Duration + 25.f) Destroy();
}
