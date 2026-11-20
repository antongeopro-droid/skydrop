#include "SDBotController.h"
#include "SDCharacter.h"
#include "SDGameMode.h"
#include "SDWorld.h"
#include "SDMatchActors.h"
#include "SDLoot.h"
#include "SDBuildPiece.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ASDBotController::ASDBotController()
{
	PrimaryActorTick.bCanEverTick = true;
	bSetControlRotationFromPawnOrientation = false;
}

ASDCharacter* ASDBotController::Me() const
{
	return Cast<ASDCharacter>(GetPawn());
}

void ASDBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Skill = FMath::FRandRange(0.2f, 0.85f);
	DropFraction = FMath::FRandRange(0.08f, 0.92f);
	ReactionDelay = FMath::Lerp(0.9f, 0.3f, Skill);
	if (ASDCharacter* C = Me())
	{
		C->Mats[0] = FMath::RandRange(80, 200);
	}
	if (ASDGameMode* GM = GetWorld()->GetAuthGameMode<ASDGameMode>())
	{
		if (GM->Island)
		{
			FRandomStream R(FMath::Rand());
			if (GM->Island->Pois.Num() > 0 && FMath::FRand() < 0.7f)
			{
				const FSDPoi& P = GM->Island->Pois[FMath::RandRange(0, GM->Island->Pois.Num() - 1)];
				const FVector2D Off(FMath::FRandRange(-2500.f, 2500.f), FMath::FRandRange(-2500.f, 2500.f));
				LandTarget = FVector(P.Pos + Off, 0.f);
			}
			else
			{
				LandTarget = GM->Island->RandomLandPoint(R, 200.f, 0.8f);
			}
		}
	}
}

bool ASDBotController::CanSee(ASDCharacter* C, ASDCharacter* Other) const
{
	if (!C || !Other) return false;
	FHitResult Hit;
	FCollisionQueryParams QP(SCENE_QUERY_STAT(BotSight), true, C);
	const FVector From = C->GetPawnViewLocation();
	const FVector To = Other->GetActorLocation() + FVector(0, 0, 50);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, QP)) return true;
	return Hit.GetActor() == Other;
}

void ASDBotController::PickRoamGoal(ASDCharacter* C)
{
	ASDGameMode* GM = GetWorld()->GetAuthGameMode<ASDGameMode>();
	if (!GM || !GM->Island) return;
	FVector2D G;
	if (GM->Storm && GM->Storm->bActive)
	{
		const float A = FMath::FRand() * 2.f * PI;
		G = GM->Storm->NextCenter + FVector2D(FMath::Cos(A), FMath::Sin(A)) * FMath::FRand() * GM->Storm->NextRadius * 0.6f;
	}
	else if (GM->Island->Pois.Num() > 0 && FMath::FRand() < 0.5f)
	{
		G = GM->Island->Pois[FMath::RandRange(0, GM->Island->Pois.Num() - 1)].Pos + FVector2D(FMath::FRandRange(-2000.f, 2000.f), FMath::FRandRange(-2000.f, 2000.f));
	}
	else
	{
		G = FVector2D(C->GetActorLocation()) + FVector2D(FMath::FRandRange(-6000.f, 6000.f), FMath::FRandRange(-6000.f, 6000.f));
	}
	MoveGoal = FVector(G, GM->Island->HeightAt(G.X, G.Y));
	bHasGoal = true;
}

void ASDBotController::Think()
{
	ASDCharacter* C = Me();
	ASDGameMode* GM = GetWorld()->GetAuthGameMode<ASDGameMode>();
	if (!C || !GM) return;
	const FVector MyLoc = C->GetActorLocation();

	// --- targets
	const float Sight = FMath::Lerp(4500.f, 8000.f, Skill);
	ASDCharacter* Best = nullptr;
	float BestD = Sight;
	for (const TWeakObjectPtr<ASDCharacter>& W : GM->Alive)
	{
		ASDCharacter* O = W.Get();
		if (!O || O == C || O->bDead || O->Air == ESDAir::Bus) continue;
		const float D = FVector::Dist(MyLoc, O->GetActorLocation());
		if (D < BestD && CanSee(C, O))
		{
			BestD = D;
			Best = O;
		}
	}
	if (Best)
	{
		if (Target.Get() != Best) { Target = Best; SeeTime = 0.f; }
		bVisible = true;
		LostTime = 0.f;
	}
	else if (ASDCharacter* T = Target.Get())
	{
		bVisible = !T->bDead && CanSee(C, T) && FVector::Dist(MyLoc, T->GetActorLocation()) < Sight * 1.3f;
	}
	else
	{
		bVisible = false;
	}

	// --- loot
	LootRetarget -= 0.3f;
	if (!LootGoal.IsValid() || LootRetarget <= 0.f)
	{
		LootRetarget = 3.f;
		LootGoal = nullptr;
		int32 Weapons = 0, Free = 0;
		for (const FSDItemStack& S : C->Slots)
		{
			if (S.Item == ESDItem::Weapon) ++Weapons;
			if (S.IsEmpty()) ++Free;
		}
		AActor* BestLoot = nullptr;
		float BestScore = 0.f;
		for (TActorIterator<ASDPickup> It(GetWorld()); It; ++It)
		{
			if (It->IsAutoPickup() || !It->bSettled) continue;
			const float D = FVector::Dist(MyLoc, It->GetActorLocation());
			if (D > 3500.f) continue;
			float Want = 0.f;
			if (It->Item.Item == ESDItem::Weapon) Want = Weapons < 2 && Free > 0 ? 3.f : 0.f;
			else if (Free > 0) Want = 1.f;
			if (Want <= 0.f) continue;
			const float Score = Want / (D + 300.f);
			if (Score > BestScore) { BestScore = Score; BestLoot = *It; }
		}
		for (TActorIterator<ASDChest> It(GetWorld()); It; ++It)
		{
			if (It->bOpened) continue;
			const float D = FVector::Dist(MyLoc, It->GetActorLocation());
			if (D > 4000.f) continue;
			const float Score = 2.5f / (D + 300.f);
			if (Score > BestScore) { BestScore = Score; BestLoot = *It; }
		}
		if (BestLoot && GM->Storm && GM->Storm->bActive && !GM->Storm->IsInside(BestLoot->GetActorLocation())) BestLoot = nullptr;
		LootGoal = BestLoot;
	}

	// --- storm
	if (GM->Storm && GM->Storm->bActive && !GM->Storm->IsInsideNext(MyLoc))
	{
		if (!bHasGoal || !GM->Storm->IsInsideNext(MoveGoal)) PickRoamGoal(C);
	}
	else if (!bHasGoal)
	{
		PickRoamGoal(C);
	}
}

void ASDBotController::SteerTo(ASDCharacter* C, const FVector& Goal, float Dt, bool bFace)
{
	const FVector To = Goal - C->GetActorLocation();
	const FVector Dir = FVector(To.X, To.Y, 0.f).GetSafeNormal();
	if (Dir.IsNearlyZero()) return;
	// don't walk off cliffs: probe the ground a little ahead
	{
		FHitResult GH;
		FCollisionQueryParams QP(SCENE_QUERY_STAT(BotLedge), false, C);
		const FVector Ahead = C->GetActorLocation() + Dir * 220.f;
		const float Feet = C->GetActorLocation().Z - 90.f;
		const bool bGround = GetWorld()->LineTraceSingleByChannel(GH, Ahead + FVector(0, 0, 100.f), Ahead - FVector(0, 0, 3000.f), ECC_Visibility, QP);
		if (!bGround || GH.ImpactPoint.Z < Feet - 900.f)
		{
			bHasGoal = false;
			LootGoal = nullptr;
			C->AddMovementInput(-Dir, 1.f);
			return;
		}
	}
	C->AddMovementInput(Dir, 1.f);
	if (bFace)
	{
		const FRotator Want(0.f, Dir.Rotation().Yaw, 0.f);
		SetControlRotation(FMath::RInterpTo(GetControlRotation(), Want, Dt, 6.f));
	}
	// stuck handling
	if (C->GetVelocity().Size2D() < 80.f && !C->GetCharacterMovement()->IsFalling())
	{
		StuckTime += Dt;
		if (StuckTime > 0.5f && StuckTime < 0.6f) C->Jump();
		if (StuckTime > 1.2f)
		{
			FHitResult Hit;
			FCollisionQueryParams QP(SCENE_QUERY_STAT(BotBlock), false, C);
			const FVector S = C->GetActorLocation();
			if (GetWorld()->LineTraceSingleByChannel(Hit, S, S + Dir * 200.f, ECC_Visibility, QP) && Cast<ASDBuildPiece>(Hit.GetActor()))
			{
				BreakTimer = 1.6f;
				StuckTime = 0.f;
			}
			else if (StuckTime > 3.f)
			{
				StuckTime = 0.f;
				bHasGoal = false;
				LootGoal = nullptr;
			}
		}
	}
	else
	{
		StuckTime = 0.f;
	}
}

void ASDBotController::TickAir(ASDCharacter* C, float Dt)
{
	const FVector To = LandTarget - C->GetActorLocation();
	const float D = To.Size2D();
	const FVector Dir = FVector(To.X, To.Y, 0.f).GetSafeNormal();
	C->AddMovementInput(Dir, FMath::Clamp(D / 800.f, 0.f, 1.f));
	const float Pitch = D > 3000.f ? 0.f : -70.f;
	SetControlRotation(FMath::RInterpTo(GetControlRotation(), FRotator(Pitch, Dir.IsNearlyZero() ? GetControlRotation().Yaw : Dir.Rotation().Yaw, 0.f), Dt, 3.f));
}

void ASDBotController::TickGround(ASDCharacter* C, float Dt)
{
	ThinkTimer -= Dt;
	if (ThinkTimer <= 0.f)
	{
		ThinkTimer = 0.3f;
		Think();
	}
	BuildCooldown -= Dt;
	const float Now = GetWorld()->GetTimeSeconds();

	// breaking through a wall that blocks the path
	if (BreakTimer > 0.f)
	{
		BreakTimer -= Dt;
		if (C->Active != -1) C->SelectSlot(-1);
		C->PrimaryDown();
		if (BreakTimer <= 0.f) C->PrimaryUp();
		return;
	}

	ASDCharacter* T = Target.Get();
	if (T && T->bDead) { Target = nullptr; T = nullptr; }

	if (T && (bVisible || LostTime < 2.f))
	{
		if (!bVisible) LostTime += Dt; else SeeTime += Dt;
		bHealing = false;
		const FVector MyLoc = C->GetActorLocation();
		const float Dist = FVector::Dist(MyLoc, T->GetActorLocation());

		const int32 Want = C->BestWeaponSlot(Dist);
		if (Want != C->Active && C->ReloadLeft <= 0.f && C->Cooldown <= 0.f) C->SelectSlot(Want);

		// aim with human-like error
		if (Now - LastAimNoiseTime > 0.45f)
		{
			LastAimNoiseTime = Now;
			const float E = (1.f - Skill) * 5.f * (Dist > 2500.f ? 1.4f : 1.f) + 0.4f;
			AimNoise = FRotator(FMath::FRandRange(-E, E) * 0.6f, FMath::FRandRange(-E, E), 0.f);
		}
		const FVector AimAt = T->GetActorLocation() + FVector(0, 0, FMath::FRand() < Skill * 0.3f ? 55.f : 15.f) + T->GetVelocity() * 0.06f;
		const FRotator Desired = (AimAt - C->GetPawnViewLocation()).Rotation() + AimNoise;
		const FRotator NewRot = FMath::RInterpTo(GetControlRotation(), Desired, Dt, 4.f + 8.f * Skill);
		SetControlRotation(NewRot);
		const float Err = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(NewRot.Vector(), Desired.Vector()), -1.f, 1.f)));

		// fire
		bool bFire = false;
		const FSDItemStack* It = C->ActiveItem();
		if (bVisible && SeeTime > ReactionDelay && Err < 6.f)
		{
			if (C->Active < 0) bFire = Dist < 230.f;
			else if (It && It->Item == ESDItem::Weapon)
			{
				const FSDWeaponDef& D = SDData::Weapon(It->Weapon);
				bFire = Dist < D.Range;
				if (D.bAuto)
				{
					BurstTimer += Dt;
					const float Cycle = FMath::Fmod(BurstTimer, 1.05f);
					bFire = bFire && Cycle < 0.7f;
				}
			}
		}
		if (bFire) C->PrimaryDown(); else C->PrimaryUp();

		// movement: keep a preferred distance and strafe
		float Pref = 1400.f;
		if (C->Active < 0) Pref = 120.f;
		else if (It && It->Item == ESDItem::Weapon)
		{
			switch (It->Weapon)
			{
			case ESDWeapon::Pump: case ESDWeapon::Tactical: Pref = 350.f; break;
			case ESDWeapon::SMG: Pref = 700.f; break;
			case ESDWeapon::Sniper: Pref = 4000.f; break;
			default: break;
			}
		}
		const FVector To = (T->GetActorLocation() - MyLoc).GetSafeNormal2D();
		const FVector Side(-To.Y, To.X, 0.f);
		StrafeTimer -= Dt;
		if (StrafeTimer <= 0.f)
		{
			StrafeTimer = FMath::FRandRange(0.5f, 1.4f);
			StrafeDir = FMath::FRand() < 0.5f ? -1.f : 1.f;
		}
		FVector Move = Side * StrafeDir * 0.8f;
		if (Dist > Pref * 1.3f) Move += To;
		else if (Dist < Pref * 0.6f) Move -= To;
		C->AddMovementInput(Move.GetClampedToMaxSize(1.f), 1.f);
		if (FMath::FRand() < Dt * 0.5f) C->Jump();
		if (It && It->Item == ESDItem::Weapon && It->Count == 0) C->ReloadPressed();
		return;
	}

	// out of combat
	Target = nullptr;
	SeeTime = 0.f;

	// healing
	const float SinceHit = Now - C->LastDamageTime;
	if (SinceHit > 4.f)
	{
		int32 Heal = -1;
		for (int32 i = 0; i < 5; ++i)
		{
			const FSDItemStack& S = C->Slots[i];
			if ((S.Item == ESDItem::Bandage && C->Health < 75.f) || (S.Item == ESDItem::Medkit && C->Health < 70.f) ||
				(S.Item == ESDItem::MiniShield && C->Shield < 50.f) || (S.Item == ESDItem::BigShield && C->Shield <= 50.f))
			{
				Heal = i;
				break;
			}
		}
		if (Heal >= 0)
		{
			if (C->Active != Heal) C->SelectSlot(Heal);
			C->PrimaryDown();
			bHealing = true;
			return;
		}
	}
	if (bHealing)
	{
		bHealing = false;
		C->PrimaryUp();
	}
	C->PrimaryUp();

	// carry the best weapon while roaming
	const int32 Ready = C->BestWeaponSlot(1200.f);
	if (Ready != C->Active && C->Cooldown <= 0.f) C->SelectSlot(Ready);

	if (AActor* L = LootGoal.Get())
	{
		SteerTo(C, L->GetActorLocation(), Dt, true);
		if (FVector::Dist2D(C->GetActorLocation(), L->GetActorLocation()) < 170.f)
		{
			C->InteractWith(L);
			LootGoal = nullptr;
			LootRetarget = 0.f;
		}
		return;
	}
	if (bHasGoal)
	{
		SteerTo(C, MoveGoal, Dt, true);
		if (FVector::Dist2D(C->GetActorLocation(), MoveGoal) < 300.f) bHasGoal = false;
	}
}

void ASDBotController::Tick(float Dt)
{
	Super::Tick(Dt);
	ASDCharacter* C = Me();
	if (!C || C->bDead) return;
	switch (C->Air)
	{
	case ESDAir::Bus: break;
	case ESDAir::Skydive:
	case ESDAir::Glide: TickAir(C, Dt); break;
	case ESDAir::Ground: TickGround(C, Dt); break;
	}
}

void ASDBotController::OnDamaged(ASDCharacter* By)
{
	ASDCharacter* C = Me();
	if (!C || C->bDead || !By || By == C) return;
	if (!Target.IsValid())
	{
		Target = By;
		SeeTime = ReactionDelay * 0.5f;
	}
	// classic panic build: wall up toward the attacker, sometimes a ramp
	const int32 MaxMats = FMath::Max3(C->Mats[0], C->Mats[1], C->Mats[2]);
	if (BuildCooldown <= 0.f && MaxMats >= SDGrid::Cost && C->Air == ESDAir::Ground && FMath::FRand() < 0.3f + 0.45f * Skill)
	{
		BuildCooldown = FMath::Lerp(3.5f, 1.5f, Skill);
		const FRotator Saved = GetControlRotation();
		const FVector To = By->GetActorLocation() - C->GetActorLocation();
		SetControlRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
		C->PlaceBuild(ESDBuildType::Wall);
		if (FMath::FRand() < 0.35f + 0.3f * Skill) C->PlaceBuild(ESDBuildType::Ramp);
		SetControlRotation(Saved);
	}
}
