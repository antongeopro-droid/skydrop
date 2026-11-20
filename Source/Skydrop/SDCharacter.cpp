#include "SDCharacter.h"
#include "SDAssets.h"
#include "SDBuildPiece.h"
#include "SDLoot.h"
#include "SDGameMode.h"
#include "SDBotController.h"
#include "SDMatchActors.h"
#include "SDAnim.h"
#include "SDCosmetics.h"
#include "SDAudio.h"
#include "SDSettings.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/AnimInstance.h"

namespace
{
	constexpr float BaseSpeed = 560.f;

	float RecoilFor(ESDWeapon W)
	{
		switch (W)
		{
		case ESDWeapon::AR: return 0.35f;
		case ESDWeapon::SMG: return 0.22f;
		case ESDWeapon::Pistol: return 0.7f;
		case ESDWeapon::Pump: return 2.4f;
		case ESDWeapon::Tactical: return 1.8f;
		case ESDWeapon::Sniper: return 3.5f;
		default: return 0.f;
		}
	}
}

ASDCharacter::ASDCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(36.f, 90.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* M = GetCharacterMovement();
	M->bOrientRotationToMovement = false;
	M->MaxWalkSpeed = BaseSpeed;
	M->MaxWalkSpeedCrouched = 330.f;
	M->JumpZVelocity = 720.f;
	M->GravityScale = 1.75f;
	M->AirControl = 0.7f;
	M->MaxAcceleration = 4096.f;
	M->BrakingDecelerationWalking = 3500.f;
	M->GroundFriction = 8.f;
	M->BrakingDecelerationFalling = 400.f;
	M->NavAgentProps.bCanCrouch = true;
	M->SetWalkableFloorAngle(52.f);
	M->MaxStepHeight = 55.f;
	M->bCanWalkOffLedgesWhenCrouching = true;

	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(RootComponent);
	Boom->TargetArmLength = 290.f;
	Boom->SocketOffset = FVector(0.f, 62.f, 70.f);
	Boom->bUsePawnControlRotation = true;
	Boom->ProbeSize = 10.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Boom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
	Camera->SetFieldOfView(80.f);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshF(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (MeshF.Succeeded()) GetMesh()->SetSkeletalMeshAsset(MeshF.Object);
	// native C++ animation: rifle / pistol / unarmed locomotion, aim offsets, reload and swing layers
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	GetMesh()->SetAnimInstanceClass(USDAnimInstance::StaticClass());
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	HeldRoot = CreateDefaultSubobject<USceneComponent>(TEXT("HeldRoot"));
	HeldRoot->SetupAttachment(GetMesh(), TEXT("hand_r"));

	BlingRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BlingRoot"));
	BlingRoot->SetupAttachment(GetMesh());
	BlingRoot->SetRelativeLocationAndRotation(FVector(0.f, -20.f, 128.f), FRotator(0.f, 90.f, 0.f));

	GliderRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GliderRoot"));
	GliderRoot->SetupAttachment(RootComponent);
	GliderRoot->SetRelativeLocation(FVector(0.f, 0.f, 170.f));

	AIControllerClass = ASDBotController::StaticClass();
	AutoPossessAI = EAutoPossessAI::Disabled;
}

void ASDCharacter::BeginPlay()
{
	Super::BeginPlay();
	using namespace SDAssets;

	if (bIsBot && FMath::FRand() < 0.5f)
	{
		if (USkeletalMesh* Quinn = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple")))
		{
			GetMesh()->SetSkeletalMeshAsset(Quinn);
		}
	}

	// glider from the equipped cosmetic
	for (UStaticMeshComponent* P : SDCos::BuildGlider(this, GliderRoot, SDCos::Find(CosGlider))) GliderParts.Add(P);
	GliderRoot->SetVisibility(false, true);

	RefreshHeld();
}

void ASDCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Ghost)
	{
		Ghost->Destroy();
		Ghost = nullptr;
	}
	Super::EndPlay(Reason);
}

// ------------------------------------------------------------------ helpers

const FSDItemStack* ASDCharacter::ActiveItem() const
{
	return Active >= 0 && Active < 5 && !Slots[Active].IsEmpty() ? &Slots[Active] : nullptr;
}

bool ASDCharacter::HasWeapon() const
{
	for (const FSDItemStack& S : Slots)
	{
		if (S.Item == ESDItem::Weapon) return true;
	}
	return false;
}

int32 ASDCharacter::BestWeaponSlot(float Distance) const
{
	int32 Best = -1;
	float BestScore = -1.f;
	for (int32 i = 0; i < 5; ++i)
	{
		const FSDItemStack& S = Slots[i];
		if (S.Item != ESDItem::Weapon) continue;
		const bool bHasAmmo = S.Count > 0 || Ammo[(int32)SDData::Weapon(S.Weapon).Ammo] > 0;
		if (!bHasAmmo) continue;
		float Score = 1.f + S.Rarity * 0.1f;
		switch (S.Weapon)
		{
		case ESDWeapon::Pump:   Score += Distance < 700.f ? 3.f : Distance < 1400.f ? 0.5f : -1.f; break;
		case ESDWeapon::Tactical: Score += Distance < 700.f ? 2.7f : Distance < 1400.f ? 0.6f : -1.f; break;
		case ESDWeapon::SMG:    Score += Distance < 1500.f ? 2.f : 0.3f; break;
		case ESDWeapon::AR:     Score += Distance < 700.f ? 1.f : 2.2f; break;
		case ESDWeapon::Sniper: Score += Distance > 3500.f ? 3.f : -0.5f; break;
		case ESDWeapon::Pistol: Score += 0.8f; break;
		default: break;
		}
		if (Score > BestScore) { BestScore = Score; Best = i; }
	}
	return Best;
}

bool ASDCharacter::GetAimRay(FVector& Start, FVector& Dir) const
{
	if (IsPlayerControlled() && Camera)
	{
		Start = Camera->GetComponentLocation();
		Dir = Camera->GetForwardVector();
		const float ToChar = FVector::DotProduct(GetActorLocation() - Start, Dir);
		Start += Dir * FMath::Max(ToChar, 0.f);
		return true;
	}
	if (Controller)
	{
		Start = GetPawnViewLocation();
		Dir = Controller->GetControlRotation().Vector();
		return true;
	}
	Start = GetActorLocation();
	Dir = GetActorForwardVector();
	return false;
}

float ASDCharacter::CurrentSpread() const
{
	const FSDItemStack* It = ActiveItem();
	if (!It || It->Item != ESDItem::Weapon) return 0.f;
	const FSDWeaponDef& D = SDData::Weapon(It->Weapon);
	float S = (bAds ? D.AdsSpread : D.HipSpread) + Bloom;
	if (GetVelocity().Size2D() > 120.f) S += bAds ? 0.4f : 1.0f;
	if (GetCharacterMovement()->IsFalling()) S *= 1.8f;
	if (bIsCrouched) S *= 0.8f;
	return S;
}

float ASDCharacter::GroundDistance() const
{
	FHitResult Hit;
	FCollisionQueryParams QP(SCENE_QUERY_STAT(GroundDist), false, this);
	const FVector S = GetActorLocation();
	if (GetWorld()->LineTraceSingleByChannel(Hit, S, S - FVector(0, 0, 60000.f), ECC_Visibility, QP))
	{
		return Hit.Distance;
	}
	return 60000.f;
}

ASDBuildPiece* ASDCharacter::AimedPiece(float MaxDist) const
{
	FVector S, D;
	GetAimRay(S, D);
	FHitResult Hit;
	FCollisionQueryParams QP(SCENE_QUERY_STAT(AimPiece), true, this);
	if (Ghost) QP.AddIgnoredActor(Ghost);
	if (GetWorld()->LineTraceSingleByChannel(Hit, S, S + D * MaxDist, ECC_Visibility, QP))
	{
		return Cast<ASDBuildPiece>(Hit.GetActor());
	}
	return nullptr;
}

// ------------------------------------------------------------------ input

void ASDCharacter::Move(const FVector2D& In)
{
	if (bDead || Air == ESDAir::Bus || In.IsNearlyZero()) return;
	const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
	const FRotationMatrix RM(Yaw);
	AddMovementInput(RM.GetUnitAxis(EAxis::X), In.Y);
	AddMovementInput(RM.GetUnitAxis(EAxis::Y), In.X);
}

void ASDCharacter::JumpPressed()
{
	if (bDead) return;
	switch (Air)
	{
	case ESDAir::Bus:
		if (ASDGameMode* GM = GetWorld()->GetAuthGameMode<ASDGameMode>()) GM->DropFromBus(this);
		break;
	case ESDAir::Skydive:
		DeployGlider();
		break;
	case ESDAir::Glide:
		break;
	case ESDAir::Ground:
		if (bIsCrouched) UnCrouch();
		Jump();
		break;
	}
}

void ASDCharacter::PrimaryDown()
{
	if (bDead || Air != ESDAir::Ground) return;
	bTrigger = true;
}

void ASDCharacter::PrimaryUp()
{
	bTrigger = false;
	UseLeft = 0.f;
	if (IsEditing())
	{
		EditPaint = -1;
		if (IsPlayerControlled() && USDSettings::Get()->EditConfirm == 2) EndEdit(true);
	}
}

void ASDCharacter::SetAds(bool bOn)
{
	const FSDItemStack* It = ActiveItem();
	bAds = bOn && Mode == ESDMode::Combat && It && It->Item == ESDItem::Weapon && Air == ESDAir::Ground;
}

void ASDCharacter::ReloadPressed()
{
	if (Mode == ESDMode::Build)
	{
		// R in build mode rotates the aimed ramp, like an edit shortcut
		if (ASDBuildPiece* P = AimedPiece(700.f))
		{
			if (P->Slot.Type == ESDBuildType::Ramp) P->CycleEdit();
		}
		return;
	}
	const FSDItemStack* It = ActiveItem();
	if (!It || It->Item != ESDItem::Weapon || ReloadLeft > 0.f) return;
	const FSDWeaponDef& D = SDData::Weapon(It->Weapon);
	if (It->Count >= D.Mag || Ammo[(int32)D.Ammo] <= 0) return;
	ReloadLeft = D.Reload;
	if (IsPlayerControlled()) SDAudio::Play(this, ESDSfx::Reload);
}

void ASDCharacter::SelectSlot(int32 Idx)
{
	if (IsEditing()) EndEdit(true);
	if (bDead) return;
	const bool bChanged = Idx != Active || Mode != ESDMode::Combat;
	Mode = ESDMode::Combat;
	if (Ghost) Ghost->SetActorHiddenInGame(true);
	if (!bChanged) return;
	Active = FMath::Clamp(Idx, -1, 4);
	bAds = false;
	ReloadLeft = 0.f;
	UseLeft = 0.f;
	Cooldown = FMath::Max(Cooldown, 0.25f);
	RefreshHeld();
}

void ASDCharacter::ScrollSlot(int32 Delta)
{
	int32 N = Active + Delta;
	if (N > 4) N = -1;
	if (N < -1) N = 4;
	SelectSlot(N);
}

void ASDCharacter::ToggleBuild()
{
	if (IsEditing()) EndEdit(true);
	if (bDead || Air != ESDAir::Ground) return;
	if (Mode == ESDMode::Build)
	{
		SelectSlot(Active);
		Mode = ESDMode::Combat;
		if (Ghost) Ghost->SetActorHiddenInGame(true);
		RefreshHeld();
	}
	else
	{
		Mode = ESDMode::Build;
		bAds = false;
		ReloadLeft = 0.f;
		RefreshHeld();
	}
}

void ASDCharacter::SelectBuild(ESDBuildType T)
{
	if (bDead || Air != ESDAir::Ground) return;
	BuildType = T;
	if (Mode != ESDMode::Build)
	{
		Mode = ESDMode::Build;
		bAds = false;
		ReloadLeft = 0.f;
		RefreshHeld();
	}
}

void ASDCharacter::CycleMat()
{
	BuildMat = (ESDMat)(((int32)BuildMat + 1) % 3);
	if (Ghost) Ghost->LastVisualStep = -1;
}

void ASDCharacter::EditPressed()
{
	if (bDead) return;
	if (IsEditing()) { EndEdit(true); return; }
	ASDBuildPiece* P = AimedPiece(700.f);
	if (P && !P->bWorld && !P->bGhost) BeginEdit(P);
}

void ASDCharacter::BeginEdit(ASDBuildPiece* P)
{
	if (Mode == ESDMode::Build) ToggleBuild();
	StopEmote();
	bTrigger = false;
	bAds = false;
	EditTarget = P;
	EditSel = P->Slot.Type == ESDBuildType::Ramp ? 0 : P->Edit;
	EditPath.Reset();
	EditPaint = -1;
	EditHover = -1;
	if (IsPlayerControlled()) SDAudio::Play(this, ESDSfx::Click);
}

void ASDCharacter::EndEdit(bool bConfirm)
{
	ASDBuildPiece* P = EditTarget.Get();
	EditTarget = nullptr;
	EditPaint = -1;
	bTrigger = false;
	if (!P || !bConfirm) return;
	const bool bChanged = P->ApplyEdit(EditSel, EditPath);
	if (bChanged)
	{
		const FVector L = P->GetActorLocation();
		SDAudio::Play(this, ESDSfx::Build, &L, 0.7f);
	}
	else if (EditSel != (P->Slot.Type == ESDBuildType::Ramp ? 0 : P->Edit) && !P->IsValidEdit(EditSel, EditPath))
	{
		SDAudio::Play(this, ESDSfx::Hit); // not an allowed shape
	}
}

void ASDCharacter::ResetEdit()
{
	ASDBuildPiece* P = EditTarget.Get();
	EditTarget = nullptr;
	EditPaint = -1;
	bTrigger = false;
	if (P && P->Edit != 0)
	{
		P->SetEdit(0);
		const FVector L = P->GetActorLocation();
		SDAudio::Play(this, ESDSfx::Build, &L, 0.7f);
	}
}

void ASDCharacter::TickEdit()
{
	ASDBuildPiece* P = EditTarget.Get();
	if (!P || bDead || FVector::Dist(P->GetActorLocation(), GetActorLocation()) > 1100.f) { EndEdit(false); return; }
	const FVector View = Camera ? Camera->GetComponentLocation() : GetActorLocation();
	TArray<FSDEditTile> Tiles;
	P->GetEditTiles(View, Tiles);
	FVector Start, Dir;
	GetAimRay(Start, Dir);
	// the tile under the crosshair: ray against each tile quad (all tiles of a piece share one plane)
	EditHover = -1;
	float Best = 1e9f;
	for (const FSDEditTile& Tl : Tiles)
	{
		const FVector E1 = Tl.C[1] - Tl.C[0], E2 = Tl.C[3] - Tl.C[0];
		const FVector N = FVector::CrossProduct(E1, E2).GetSafeNormal();
		const float Den = FVector::DotProduct(Dir, N);
		if (FMath::Abs(Den) < 1e-4f) continue;
		const float Tt = FVector::DotProduct(Tl.C[0] - Start, N) / Den;
		if (Tt <= 0.f || Tt >= Best) continue;
		const FVector Hit = Start + Dir * Tt - Tl.C[0];
		const float A = FVector::DotProduct(Hit, E1) / E1.SizeSquared();
		const float B = FVector::DotProduct(Hit, E2) / E2.SizeSquared();
		if (A < 0.f || A > 1.f || B < 0.f || B > 1.f) continue;
		Best = Tt;
		EditHover = Tl.Idx;
	}
	if (bTrigger && EditHover >= 0)
	{
		const int32 M = 1 << EditHover;
		if (EditPaint < 0) EditPaint = (EditSel & M) ? 0 : 1;
		const int32 Before = EditSel;
		EditSel = EditPaint ? (EditSel | M) : (EditSel & ~M);
		if (EditPaint && !(Before & M)) EditPath.Add(EditHover);
		else if (!EditPaint && (Before & M)) EditPath.Remove(EditHover);
		if (Before != EditSel && IsPlayerControlled()) SDAudio::Play(this, ESDSfx::Click, nullptr, 0.6f);
	}
}

void ASDCharacter::CrouchToggle()
{
	if (Air != ESDAir::Ground) return;
	if (bIsCrouched) UnCrouch(); else Crouch();
}

AActor* ASDCharacter::FindInteractable() const
{
	if (bDead || Air != ESDAir::Ground) return nullptr;
	FVector S, D;
	GetAimRay(S, D);
	const FVector D2 = FVector(D.X, D.Y, 0.f).GetSafeNormal();
	const FVector Me = GetActorLocation();
	AActor* Best = nullptr;
	float BestScore = -10.f;
	auto Consider = [&](AActor* A)
	{
		const FVector To = A->GetActorLocation() - Me;
		const float Dist = To.Size2D();
		if (Dist > 320.f || FMath::Abs(To.Z) > 220.f) return;
		const float Dot = FVector::DotProduct(D2, FVector(To.X, To.Y, 0.f).GetSafeNormal());
		if (Dot < 0.25f && Dist > 110.f) return;
		const float Score = Dot - Dist / 640.f;
		if (Score > BestScore) { BestScore = Score; Best = A; }
	};
	for (TActorIterator<ASDPickup> It(GetWorld()); It; ++It)
	{
		if (!It->IsAutoPickup()) Consider(*It);
	}
	for (TActorIterator<ASDChest> It(GetWorld()); It; ++It)
	{
		if (!It->bOpened) Consider(*It);
	}
	return Best;
}

FString ASDCharacter::InteractPrompt() const
{
	AActor* A = FindInteractable();
	if (const ASDPickup* P = Cast<ASDPickup>(A)) return FString::Printf(TEXT("[E] Pick up %s"), *SDData::ItemName(P->Item));
	if (Cast<ASDChest>(A)) return TEXT("[E] Search chest");
	return FString();
}

void ASDCharacter::InteractPressed()
{
	InteractWith(FindInteractable());
}

void ASDCharacter::InteractWith(AActor* Target)
{
	if (!Target || bDead) return;
	if (ASDPickup* P = Cast<ASDPickup>(Target))
	{
		if (GiveItem(P->Item, true)) { P->Destroy(); if (IsPlayerControlled()) SDAudio::Play(this, ESDSfx::Pickup); }
	}
	else if (ASDChest* C = Cast<ASDChest>(Target))
	{
		C->Open(this);
	}
}

// ------------------------------------------------------------------ items

bool ASDCharacter::GiveItem(const FSDItemStack& S, bool bSwapIfFull)
{
	switch (S.Item)
	{
	case ESDItem::Ammo:
		Ammo[FMath::Clamp(S.Sub, 0, 3)] = FMath::Min(999, Ammo[FMath::Clamp(S.Sub, 0, 3)] + S.Count);
		return true;
	case ESDItem::Mats:
		Mats[FMath::Clamp(S.Sub, 0, 2)] = FMath::Min(999, Mats[FMath::Clamp(S.Sub, 0, 2)] + S.Count);
		return true;
	case ESDItem::None:
		return false;
	default:
		break;
	}

	FSDItemStack Left = S;
	if (S.Item != ESDItem::Weapon)
	{
		const int32 Max = SDData::MaxStack(S.Item);
		for (FSDItemStack& Slot : Slots)
		{
			if (Slot.Item == S.Item && Slot.Count < Max)
			{
				const int32 Add = FMath::Min(Max - Slot.Count, Left.Count);
				Slot.Count += Add;
				Left.Count -= Add;
				if (Left.Count <= 0) return true;
			}
		}
	}
	for (int32 i = 0; i < 5; ++i)
	{
		if (Slots[i].IsEmpty())
		{
			Slots[i] = Left;
			if (i == Active) RefreshHeld();
			return true;
		}
	}
	if (bSwapIfFull && Active >= 0)
	{
		SDLoot::Spawn(GetWorld(), Slots[Active], GetActorLocation() - FVector(0, 0, 60), true);
		Slots[Active] = Left;
		ReloadLeft = 0.f;
		RefreshHeld();
		return true;
	}
	return false;
}

void ASDCharacter::DropAll()
{
	const FVector At = GetActorLocation();
	for (FSDItemStack& S : Slots)
	{
		if (!S.IsEmpty()) SDLoot::Spawn(GetWorld(), S, At, true);
		S = FSDItemStack();
	}
	for (int32 i = 0; i < (int32)ESDAmmo::Count; ++i)
	{
		if (Ammo[i] > 0)
		{
			FSDItemStack A; A.Item = ESDItem::Ammo; A.Sub = i; A.Count = Ammo[i];
			SDLoot::Spawn(GetWorld(), A, At, true);
			Ammo[i] = 0;
		}
	}
	for (int32 i = 0; i < 3; ++i)
	{
		if (Mats[i] > 0)
		{
			FSDItemStack M; M.Item = ESDItem::Mats; M.Sub = i; M.Count = Mats[i];
			SDLoot::Spawn(GetWorld(), M, At, true);
			Mats[i] = 0;
		}
	}
}

void ASDCharacter::OnHarvest(const FVector& At, int32 Gain, bool bWeak)
{
	if (!IsPlayerControlled()) return;
	if (ASDGameMode* GM = GetWorld()->GetAuthGameMode<ASDGameMode>()) GM->OnPlayerHarvest(Gain);
	AddDmgNumber(At, FString::Printf(TEXT("+%d"), Gain), bWeak ? FLinearColor(0.4f, 0.85f, 1.f) : FLinearColor::White);
}

void ASDCharacter::AddDmgNumber(const FVector& P, const FString& Text, const FLinearColor& C)
{
	if (!USDSettings::Get()->bDamageNumbers) return;
	FSDDamageNumber N;
	N.Pos = P + FVector(FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(-20.f, 20.f), 30.f);
	N.Text = Text;
	N.Time = GetWorld()->GetTimeSeconds();
	N.Color = C;
	DamageNumbers.Add(N);
}

// ------------------------------------------------------------------ held visuals

void ASDCharacter::RefreshHeld()
{
	for (UStaticMeshComponent* C : HeldParts)
	{
		if (C) C->DestroyComponent();
	}
	HeldParts.Reset();
	if (!HeldRoot || bDead) return;
	using namespace SDAssets;

	auto Add = [&](UStaticMeshComponent* C) { if (C) { C->SetCastShadow(true); HeldParts.Add(C); } };

	if (Mode == ESDMode::Build)
	{
		return; // empty hands while building
	}
	const FSDItemStack* It = ActiveItem();
	if (Active < 0)
	{
		// pickaxe from the equipped cosmetic
		for (UStaticMeshComponent* P : SDCos::BuildPickaxe(this, HeldRoot, SDCos::Find(CosPickaxe))) Add(P);
		return;

	}
	if (!It) return;
	if (It->Item == ESDItem::Weapon)
	{
		const FSDWeaponDef& D = SDData::Weapon(It->Weapon);
		MuzzleDist = D.Size.X * 0.85f;
		if (UStaticMesh* GunM = GunMesh((uint8)It->Weapon, It->Rarity))
		{
			UStaticMeshComponent* G = AddMesh(this, HeldRoot, GunM, FVector::ZeroVector, FVector(1.f), FRotator::ZeroRotator, nullptr, false);
			Add(G);
			MuzzleDist = GunM->GetBoundingBox().Max.X;
			return;
		}
		UMaterialInterface* Body = Solid(D.Body, 0.f, 0.45f, 0.4f);
		Add(AddBox(this, HeldRoot, FVector(D.Size.X * 0.35f, 0, 0), D.Size, FRotator::ZeroRotator, Body, false));
		Add(AddBox(this, HeldRoot, FVector(0, 0, -D.Size.Z * 0.9f), FVector(8, 6, 16), FRotator(-15, 0, 0), Body, false));
		Add(AddBox(this, HeldRoot, FVector(D.Size.X * 0.35f, 0, D.Size.Z * 0.62f), FVector(D.Size.X * 0.6f, 3, 2.5f), FRotator::ZeroRotator,
			Solid(SDData::RarityColor(It->Rarity), 2.5f), false));
		if (It->Weapon == ESDWeapon::Sniper)
		{
			Add(AddMesh(this, HeldRoot, Cylinder(), FVector(D.Size.X * 0.3f, 0, D.Size.Z + 4.f), FVector(0.08f, 0.08f, 0.32f), FRotator(90, 0, 0),
				Solid(FLinearColor(0.05f, 0.05f, 0.05f)), false));
		}
	}
	else
	{
		FLinearColor C = It->Item == ESDItem::Bandage ? FLinearColor(0.9f, 0.88f, 0.8f)
			: It->Item == ESDItem::Medkit ? FLinearColor(0.9f, 0.9f, 0.9f) : FLinearColor(0.15f, 0.5f, 1.f);
		Add(AddMesh(this, HeldRoot, Sphere(), FVector(8, 0, 0), FVector(0.2f), FRotator::ZeroRotator, Solid(C, It->Item >= ESDItem::MiniShield ? 1.5f : 0.f), false));
	}
}

// ------------------------------------------------------------------ building

bool ASDCharacter::ComputeBuildSlot(ESDBuildType T, FSDBuildSlot& Out) const
{
	constexpr float Ti = SDGrid::Tile;
	constexpr float H = SDGrid::Level;
	const FVector P = GetActorLocation();
	const float Feet = P.Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FRotator CR = Controller ? Controller->GetControlRotation() : GetActorRotation();
	const float Pitch = FRotator::NormalizeAxis(CR.Pitch);
	const float YawRad = FMath::DegreesToRadians(CR.Yaw);
	const FVector F2(FMath::Cos(YawRad), FMath::Sin(YawRad), 0.f);
	const int32 Dir = ((FMath::RoundToInt(FRotator::NormalizeAxis(CR.Yaw) / 90.f) % 4) + 4) % 4;

	bool bOnRamp = false;
	const UCharacterMovementComponent* M = GetCharacterMovement();
	if (M->CurrentFloor.IsWalkableFloor())
	{
		if (const ASDBuildPiece* Under = Cast<ASDBuildPiece>(M->CurrentFloor.HitResult.GetActor()))
		{
			bOnRamp = Under->Slot.Type == ESDBuildType::Ramp;
		}
	}
	const bool bAirborne = M->IsFalling();
	int32 L = FMath::FloorToInt((Feet + (bOnRamp ? H * 0.75f : bAirborne ? H * 0.45f : 60.f)) / H);

	auto TileOf = [](const FVector& V) { return FIntPoint(FMath::FloorToInt(V.X / Ti), FMath::FloorToInt(V.Y / Ti)); };
	const FIntPoint Here = TileOf(P);
	const FIntPoint Ahead = TileOf(P + F2 * Ti * 0.75f);

	Out = FSDBuildSlot();
	Out.Type = T;
	Out.Dir = Dir;
	switch (T)
	{
	case ESDBuildType::Wall:
	{
		if (Pitch > 40.f) L += 1;
		Out.L = L;
		switch (Dir)
		{
		case 0: Out.I = Here.X + 1; Out.J = Here.Y; Out.Axis = 0; break;
		case 2: Out.I = Here.X;     Out.J = Here.Y; Out.Axis = 0; break;
		case 1: Out.I = Here.X; Out.J = Here.Y + 1; Out.Axis = 1; break;
		default: Out.I = Here.X; Out.J = Here.Y;    Out.Axis = 1; break;
		}
		break;
	}
	case ESDBuildType::Floor:
		if (Pitch < -50.f)
		{
			Out.I = Here.X; Out.J = Here.Y;
			Out.L = FMath::FloorToInt((Feet + H * 0.5f) / H);
		}
		else
		{
			Out.I = Ahead.X; Out.J = Ahead.Y;
			Out.L = L + (Pitch > 35.f ? 1 : 0);
		}
		break;
	case ESDBuildType::Ramp:
		Out.I = Ahead.X; Out.J = Ahead.Y;
		Out.L = L + (Pitch > 50.f ? 1 : 0);
		break;
	case ESDBuildType::Cone:
		if (Pitch > 20.f) { Out.I = Here.X; Out.J = Here.Y; Out.L = L + 1; }
		else { Out.I = Ahead.X; Out.J = Ahead.Y; Out.L = L; }
		break;
	}
	return true;
}

bool ASDCharacter::PlaceBuild(ESDBuildType T)
{
	if (bDead || Air != ESDAir::Ground) return false;
	ASDGameMode* GM = GetWorld()->GetAuthGameMode<ASDGameMode>();
	if (!GM) return false;
	ESDMat M = BuildMat;
	if (bIsBot)
	{
		M = Mats[0] >= Mats[1] && Mats[0] >= Mats[2] ? ESDMat::Wood : Mats[1] >= Mats[2] ? ESDMat::Stone : ESDMat::Metal;
	}
	if (Mats[(int32)M] < SDGrid::Cost) return false;
	FSDBuildSlot S;
	if (!ComputeBuildSlot(T, S)) return false;
	if (GM->TryPlace(S, M, this, false, false))
	{
		Mats[(int32)M] -= SDGrid::Cost;
		{ const FVector L = GetActorLocation(); SDAudio::Play(this, ESDSfx::Build, &L, 0.8f); }
		return true;
	}
	return false;
}

void ASDCharacter::UpdateGhost()
{
	ASDGameMode* GM = GetWorld()->GetAuthGameMode<ASDGameMode>();
	if (!GM) return;
	FSDBuildSlot S;
	ComputeBuildSlot(BuildType, S);
	if (!Ghost)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Ghost = GetWorld()->SpawnActor<ASDBuildPiece>(ASDBuildPiece::StaticClass(), FTransform::Identity, P);
		Ghost->Setup(S, BuildMat, true, false, true);
	}
	const bool bSame = ASDBuildPiece::MakeKey(S) == Ghost->Key && S.Dir == Ghost->Slot.Dir;
	if (!bSame)
	{
		Ghost->Setup(S, BuildMat, true, false, true);
	}
	GhostSlot = S;
	bGhostValid = !GM->IsOccupied(S) && Mats[(int32)BuildMat] >= SDGrid::Cost;
	Ghost->SetGhostValid(bGhostValid);
	Ghost->SetActorHiddenInGame(false);
}

// ------------------------------------------------------------------ combat

void ASDCharacter::SpawnTracer(const FVector& From, const FVector& To)
{
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ASDTracer* Tr = GetWorld()->SpawnActor<ASDTracer>(ASDTracer::StaticClass(), FTransform(From), P))
	{
		Tr->Setup(From, To);
	}
}

void ASDCharacter::ApplyHit(const FHitResult& Hit, float Damage, const FVector& Dir, float HeadMult, bool bShotgun)
{
	AActor* A = Hit.GetActor();
	if (!A) return;
	const bool bMine = IsPlayerControlled();
	if (ASDCharacter* V = Cast<ASDCharacter>(A))
	{
		if (V->bDead) return;
		const float HalfH = V->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const bool bHead = Hit.ImpactPoint.Z > V->GetActorLocation().Z + HalfH * 0.55f;
		const float D = FMath::RoundToFloat(Damage * (bHead ? HeadMult : 1.f));
		const bool bShield = V->Shield > 0.f;
		UGameplayStatics::ApplyPointDamage(V, D, Dir, Hit, GetController(), this, UDamageType::StaticClass());
		if (bMine)
		{
			HitMarkerTime = GetWorld()->GetTimeSeconds();
			bHitMarkerHead = bHead;
			SDAudio::Play(this, bHead ? ESDSfx::HeadHit : ESDSfx::Hit);
			AddDmgNumber(Hit.ImpactPoint, FString::FromInt(FMath::RoundToInt(D)),
				bHead ? FLinearColor(1.f, 0.85f, 0.1f) : bShield ? FLinearColor(0.45f, 0.75f, 1.f) : FLinearColor::White);
		}
	}
	else if (Cast<ASDBuildPiece>(A))
	{
		UGameplayStatics::ApplyPointDamage(A, Damage, Dir, Hit, GetController(), this, UDamageType::StaticClass());
		if (bMine && !bShotgun) AddDmgNumber(Hit.ImpactPoint, FString::FromInt(FMath::RoundToInt(Damage)), FLinearColor(0.85f, 0.85f, 0.85f));
	}
	else if (Cast<ASDResource>(A))
	{
		UGameplayStatics::ApplyPointDamage(A, Damage, Dir, Hit, GetController(), this, UDamageType::StaticClass());
	}
}

void ASDCharacter::DoFire()
{
	FSDItemStack& S = Slots[Active];
	const FSDWeaponDef& D = SDData::Weapon(S.Weapon);
	S.Count--;
	++ShotCount;
	Cooldown = D.FireInterval;

	FVector Start, Dir;
	GetAimRay(Start, Dir);
	const float Spread = CurrentSpread();
	const float Dmg = SDData::WeaponDamage(S.Weapon, S.Rarity);
	const FVector Muzzle = HeldRoot->GetComponentLocation() + HeldRoot->GetForwardVector() * MuzzleDist;
	const bool bShotgun = D.Pellets > 1;
	{
		const ESDSfx Snd = S.Weapon == ESDWeapon::SMG ? ESDSfx::SMG : S.Weapon == ESDWeapon::Pistol ? ESDSfx::Pistol
			: S.Weapon == ESDWeapon::Sniper ? ESDSfx::Sniper : bShotgun ? ESDSfx::Shotgun : ESDSfx::AR;
		SDAudio::Play(this, Snd, &Muzzle, IsPlayerControlled() ? 0.8f : 1.f);
	}

	FCollisionQueryParams QP(SCENE_QUERY_STAT(Fire), true, this);
	if (Ghost) QP.AddIgnoredActor(Ghost);
	TMap<AActor*, float> PelletTotals;
	for (int32 p = 0; p < D.Pellets; ++p)
	{
		const FVector ShotDir = FMath::VRandCone(Dir, FMath::DegreesToRadians(FMath::Max(Spread, 0.01f)));
		const FVector End = Start + ShotDir * D.Range;
		FHitResult Hit;
		FVector TracerEnd = End;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QP))
		{
			TracerEnd = Hit.ImpactPoint;
			float PD = Dmg;
			if (bShotgun)
			{
				const float Dist = Hit.Distance;
				if (Dist > 700.f) PD *= FMath::Lerp(1.f, 0.3f, FMath::Clamp((Dist - 700.f) / 2500.f, 0.f, 1.f));
			}
			ApplyHit(Hit, PD, ShotDir, D.HeadMult, bShotgun);
		}
		if (p < 3) SpawnTracer(Muzzle, TracerEnd);
	}
	Bloom = FMath::Min(D.BloomMax, Bloom + D.Bloom);

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		FRotator R = PC->GetControlRotation();
		R.Pitch += RecoilFor(S.Weapon) * (bAds ? 0.6f : 1.f);
		R.Yaw += FMath::FRandRange(-0.25f, 0.25f) * RecoilFor(S.Weapon);
		PC->SetControlRotation(R);
	}
}

void ASDCharacter::DoSwing()
{
	Cooldown = 0.5f;
	SwingAnim = 0.35f;
	FVector Start, Dir;
	GetAimRay(Start, Dir);
	FHitResult Hit;
	FCollisionQueryParams QP(SCENE_QUERY_STAT(Swing), true, this);
	if (Ghost) QP.AddIgnoredActor(Ghost);
	if (!GetWorld()->SweepSingleByChannel(Hit, Start, Start + Dir * 240.f, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(16.f), QP))
	{
		return;
	}
	AActor* A = Hit.GetActor();
	{ const FVector L = Hit.ImpactPoint; SDAudio::Play(this, ESDSfx::Pickaxe, &L, 0.7f); }
	if (ASDResource* R = Cast<ASDResource>(A))
	{
		R->Harvest(this, Hit);
	}
	else if (ASDBuildPiece* B = Cast<ASDBuildPiece>(A))
	{
		if (B->bWorld)
		{
			const int32 Gain = 5;
			Mats[(int32)B->Mat] = FMath::Min(999, Mats[(int32)B->Mat] + Gain);
			OnHarvest(Hit.ImpactPoint, Gain, false);
		}
		UGameplayStatics::ApplyPointDamage(B, 50.f, Dir, Hit, GetController(), this, UDamageType::StaticClass());
	}
	else if (Cast<ASDCharacter>(A))
	{
		ApplyHit(Hit, 20.f, Dir, 1.f, false);
	}
}

void ASDCharacter::TickConsumable(float Dt)
{
	FSDItemStack& S = Slots[Active];
	bool bCan = false;
	switch (S.Item)
	{
	case ESDItem::Bandage: bCan = Health < 75.f; break;
	case ESDItem::Medkit: bCan = Health < 100.f; break;
	case ESDItem::MiniShield: bCan = Shield < 50.f; break;
	case ESDItem::BigShield: bCan = Shield < 100.f; break;
	default: break;
	}
	if (!bCan)
	{
		bTrigger = false;
		UseLeft = 0.f;
		return;
	}
	UseLeft += Dt;
	if (UseLeft < SDData::UseTime(S.Item)) return;
	switch (S.Item)
	{
	case ESDItem::Bandage: Health = FMath::Min(75.f, Health + 15.f); break;
	case ESDItem::Medkit: Health = 100.f; break;
	case ESDItem::MiniShield: Shield = FMath::Min(50.f, Shield + 25.f); break;
	case ESDItem::BigShield: Shield = FMath::Min(100.f, Shield + 50.f); break;
	default: break;
	}
	S.Count--;
	if (S.Count <= 0)
	{
		S = FSDItemStack();
		RefreshHeld();
	}
	UseLeft = 0.f;
	bTrigger = false;
}

// ------------------------------------------------------------------ tick

void ASDCharacter::UpdateMeshPose(float Dt)
{
	float Target = 0.f;
	if (Air == ESDAir::Skydive)
	{
		const float Pitch = Controller ? FRotator::NormalizeAxis(Controller->GetControlRotation().Pitch) : 0.f;
		Target = FMath::Lerp(55.f, 85.f, FMath::Clamp(-Pitch / 70.f, 0.f, 1.f));
	}
	else if (Air == ESDAir::Glide)
	{
		Target = 12.f;
	}
	if (FMath::IsNearlyEqual(MeshLean, Target, 0.05f) && Target == 0.f) return;
	MeshLean = FMath::FInterpTo(MeshLean, Target, Dt, 6.f);
	const FQuat Pitch = FQuat(FVector(0.f, 1.f, 0.f), FMath::DegreesToRadians(MeshLean));
	GetMesh()->SetRelativeRotation(Pitch * FQuat(FRotator(0.f, -90.f, 0.f)));
	GetMesh()->SetRelativeLocation(-Pitch.RotateVector(FVector(0.f, 0.f, 90.f)));
}

void ASDCharacter::Tick(float Dt)
{
	Super::Tick(Dt);
	const float Now = GetWorld()->GetTimeSeconds();
	DamageNumbers.RemoveAll([Now](const FSDDamageNumber& N) { return Now - N.Time > 1.1f; });
	DamagedFlash = FMath::Max(0.f, DamagedFlash - Dt);
	if (bDead) return;

	UCharacterMovementComponent* M = GetCharacterMovement();
	if (EmoteSeq)
	{
		EmoteTime += Dt * EmoteRate;
		if (EmoteTime >= EmoteSeq->GetPlayLength() || GetVelocity().Size2D() > 60.f || bTrigger || Air != ESDAir::Ground || Mode == ESDMode::Build || M->IsFalling())
		{
			StopEmote();
		}
	}
	Cooldown -= Dt;
	SwingAnim = FMath::Max(0.f, SwingAnim - Dt);

	// --- air states
	switch (Air)
	{
	case ESDAir::Skydive:
	case ESDAir::Glide:
	{
		M->GravityScale = 0.f;
		const FVector In = GetLastMovementInputVector().GetClampedToMaxSize(1.f);
		FVector Want;
		if (Air == ESDAir::Skydive)
		{
			const float Pitch = Controller ? FRotator::NormalizeAxis(Controller->GetControlRotation().Pitch) : 0.f;
			const float Dive = FMath::Clamp(-Pitch / 70.f, 0.f, 1.f);
			Want = FVector(In.X, In.Y, 0.f) * FMath::Lerp(2600.f, 900.f, Dive);
			Want.Z = -FMath::Lerp(1700.f, 4800.f, Dive);
			if (GroundDistance() < 3000.f) DeployGlider();
		}
		else
		{
			Want = FVector(In.X, In.Y, 0.f) * 1500.f;
			Want.Z = -600.f;
		}
		M->Velocity = FMath::VInterpTo(M->Velocity, Want, Dt, 2.5f);
		if (!M->IsFalling())
		{
			Air = ESDAir::Ground;
		}
		break;
	}
	case ESDAir::Ground:
		M->GravityScale = 1.75f;
		break;
	default:
		break;
	}
	if (Air == ESDAir::Ground && GliderRoot->IsVisible())
	{
		GliderRoot->SetVisibility(false, true);
	}
	UpdateMeshPose(Dt);

	// --- fall tracking
	if (Air == ESDAir::Ground)
	{
		if (M->IsFalling()) FallStartZ = FMath::Max(FallStartZ, GetActorLocation().Z);
		else FallStartZ = GetActorLocation().Z;
	}

	// --- camera
	const FSDItemStack* It = ActiveItem();
	const bool bWeapon = It && It->Item == ESDItem::Weapon;
	if (bAds && (!bWeapon || Mode != ESDMode::Combat)) bAds = false;
	const float BaseFov = IsPlayerControlled() ? USDSettings::Get()->FieldOfView : 80.f;
	// sniper: look through the scope (first person, strong zoom); the HUD draws the scope over it
	const bool bScoped = bAds && bWeapon && It->Weapon == ESDWeapon::Sniper && IsPlayerControlled();
	ScopeT = bScoped ? ScopeT + Dt : 0.f;
	const float WantFov = bScoped ? 16.f : bAds ? SDData::Weapon(It->Weapon).AdsFov * BaseFov / 80.f : BaseFov;
	Camera->SetFieldOfView(bScoped ? FMath::FInterpTo(Camera->FieldOfView, WantFov, Dt, 9.f) : FMath::FInterpTo(Camera->FieldOfView, WantFov, Dt, 14.f));
	const float WantArm = Air == ESDAir::Bus ? 1800.f : Air != ESDAir::Ground ? 520.f : bScoped ? 0.f : bAds ? 170.f : 290.f;
	Boom->TargetArmLength = bScoped && ScopeT > 0.12f ? 0.f : FMath::FInterpTo(Boom->TargetArmLength, WantArm, Dt, bScoped ? 18.f : 10.f);
	const bool bHideSelf = bScoped && ScopeT > 0.12f;
	if (bHideSelf != bScopeHidden)
	{
		bScopeHidden = bHideSelf;
		SetActorHiddenInGame(bHideSelf); // we are looking out of our own eyes
	}
	Boom->bDoCollisionTest = Air != ESDAir::Bus;

	// --- speed
	float Speed = BaseSpeed;
	if (bAds) Speed *= 0.65f;
	if (UseLeft > 0.f) Speed *= 0.45f;
	// wading: water surface is at Z = 0
	const float Feet = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (Air == ESDAir::Ground && Feet < -25.f) Speed *= FMath::Lerp(0.85f, 0.6f, FMath::Clamp(-Feet / 120.f, 0.f, 1.f));
	M->MaxWalkSpeed = Speed;

	// --- reload
	if (ReloadLeft > 0.f)
	{
		ReloadLeft -= Dt;
		if (ReloadLeft <= 0.f && bWeapon)
		{
			FSDItemStack& S = Slots[Active];
			const FSDWeaponDef& D = SDData::Weapon(S.Weapon);
			const int32 Need = D.Mag - S.Count;
			const int32 Take = FMath::Min(Need, Ammo[(int32)D.Ammo]);
			S.Count += Take;
			Ammo[(int32)D.Ammo] -= Take;
		}
	}

	// --- actions
	if (IsEditing())
	{
		TickEdit();
	}
	else if (Mode == ESDMode::Combat)
	{
		if (bWeapon) Bloom = FMath::Max(0.f, Bloom - SDData::Weapon(It->Weapon).BloomRecover * Dt);
		if (bTrigger && Air == ESDAir::Ground)
		{
			if (Active < 0)
			{
				if (Cooldown <= 0.f) DoSwing();
			}
			else if (bWeapon)
			{
				if (Cooldown <= 0.f && ReloadLeft <= 0.f)
				{
					if (It->Count > 0)
					{
						DoFire();
						if (!SDData::Weapon(Slots[Active].Weapon).bAuto) bTrigger = false;
					}
					else
					{
						ReloadPressed();
					}
				}
			}
			else if (It)
			{
				TickConsumable(Dt);
			}
		}
		else
		{
			UseLeft = 0.f;
		}
		// auto reload empty weapon
		if (bWeapon && It->Count == 0 && ReloadLeft <= 0.f && !bTrigger) ReloadPressed();
	}
	else if (Mode == ESDMode::Build)
	{
		if (IsPlayerControlled()) UpdateGhost();
		if (bTrigger && Cooldown <= 0.f)
		{
			if (PlaceBuild(BuildType)) Cooldown = 0.05f;
		}
	}

	// --- held item: guns point exactly along the aim, the pickaxe follows the hand bone
	if (HeldRoot)
	{
		if (bWeapon && Mode == ESDMode::Combat)
		{
			HeldRoot->SetWorldRotation(Controller ? Controller->GetControlRotation() : GetBaseAimRotation());
		}
		else
		{
			// pickaxe held upright in the hand, tilted forward; a swing arcs it from over the shoulder down in front
			float Tilt = -18.f;
			if (SwingAnim > 0.f)
			{
				const float T = 1.f - SwingAnim / 0.35f;
				const float E = T * T * (3.f - 2.f * T);
				Tilt = FMath::Lerp(55.f, -105.f, E);
			}
			HeldRoot->SetWorldRotation(FRotator(Tilt, GetActorRotation().Yaw, 0.f));
		}
	}
}

// ------------------------------------------------------------------ damage & death

float ASDCharacter::TakeDamage(float Amount, FDamageEvent const& Ev, AController* Inst, AActor* Causer)
{
	if (bDead || Air == ESDAir::Bus || Amount <= 0.f) return 0.f;
	ASDGameMode* GM = GetWorld()->GetAuthGameMode<ASDGameMode>();
	const bool bStorm = Causer && GM && Causer == GM->Storm;
	float A = Amount;
	if (!bStorm)
	{
		const float S = FMath::Min(Shield, A);
		Shield -= S;
		A -= S;
	}
	Health -= A;
	LastDamageTime = GetWorld()->GetTimeSeconds();
	DamagedFlash = 0.35f;
	ASDCharacter* By = Cast<ASDCharacter>(Causer);
	if (!By && Inst) By = Cast<ASDCharacter>(Inst->GetPawn());
	if (By && By != this) LastAttacker = By;
	if (ASDBotController* Bot = Cast<ASDBotController>(Controller))
	{
		Bot->OnDamaged(By);
	}
	if (Health <= 0.f)
	{
		Health = 0.f;
		Die(By ? By : LastAttacker.Get());
	}
	return Amount;
}

void ASDCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (Air == ESDAir::Skydive || Air == ESDAir::Glide)
	{
		Air = ESDAir::Ground;
		GetCharacterMovement()->GravityScale = 1.75f;
		GliderRoot->SetVisibility(false, true);
		FallStartZ = GetActorLocation().Z;
		return;
	}
	const float Drop = FallStartZ - GetActorLocation().Z;
	const float Safe = SDGrid::Level * 3.2f;
	if (Drop > Safe)
	{
		const float Dmg = FMath::RoundToFloat((Drop - Safe) * 0.12f);
		TakeDamage(Dmg, FDamageEvent(), nullptr, nullptr);
	}
	FallStartZ = GetActorLocation().Z;
}

void ASDCharacter::Die(ASDCharacter* Killer)
{
	if (bDead) return;
	if (bScopeHidden) { bScopeHidden = false; SetActorHiddenInGame(false); }
	ScopeT = 0.f;
	bDead = true;
	{ const FVector L = GetActorLocation(); SDAudio::Play(this, ESDSfx::Elim, (Killer && Killer->IsPlayerControlled()) ? nullptr : &L, 0.8f); }
	bTrigger = false;
	bAds = false;
	Mode = ESDMode::Combat;
	if (Ghost) { Ghost->Destroy(); Ghost = nullptr; }
	DropAll();
	RefreshHeld();
	GliderRoot->SetVisibility(false, true);
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->SetSimulatePhysics(true);
	if (ASDGameMode* GM = GetWorld()->GetAuthGameMode<ASDGameMode>())
	{
		GM->OnEliminated(this, Killer);
	}
	SetLifeSpan(30.f);
}

// ------------------------------------------------------------------ bus / glider

void ASDCharacter::EnterBus()
{
	Air = ESDAir::Bus;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	GetCharacterMovement()->DisableMovement();
}

void ASDCharacter::LeaveBus(const FVector& At)
{
	SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	GetCharacterMovement()->Velocity = FVector(0, 0, -1500.f);
	Air = ESDAir::Skydive;
}

void ASDCharacter::DeployGlider()
{
	if (Air != ESDAir::Skydive) return;
	Air = ESDAir::Glide;
	GliderRoot->SetVisibility(true, true);
	{ const FVector L = GetActorLocation(); SDAudio::Play(this, ESDSfx::Glider, &L, 0.7f); }
}

// ------------------------------------------------------------------ cosmetics

void ASDCharacter::ApplyCosmetics(FName Outfit, FName Bling, FName Pickaxe, FName Glider, const TArray<FName>& Emotes)
{
	CosOutfit = Outfit;
	CosBling = Bling;
	if (!Pickaxe.IsNone()) CosPickaxe = Pickaxe;
	if (!Glider.IsNone()) CosGlider = Glider;
	CosEmotes = Emotes;
	if (const FSDCosmetic* O = SDCos::Find(Outfit)) SDCos::ApplyOutfit(GetMesh(), O);
	for (UStaticMeshComponent* P : BlingParts) if (P) P->DestroyComponent();
	BlingParts.Reset();
	for (UStaticMeshComponent* P : SDCos::BuildBackBling(this, BlingRoot, SDCos::Find(Bling))) BlingParts.Add(P);
	for (UStaticMeshComponent* P : GliderParts) if (P) P->DestroyComponent();
	GliderParts.Reset();
	for (UStaticMeshComponent* P : SDCos::BuildGlider(this, GliderRoot, SDCos::Find(CosGlider))) GliderParts.Add(P);
	GliderRoot->SetVisibility(Air == ESDAir::Glide, true);
	RefreshHeld();
}

void ASDCharacter::PlayEmote(int32 Slot)
{
	if (bDead || Air != ESDAir::Ground || !CosEmotes.IsValidIndex(Slot)) return;
	const FSDCosmetic* C = SDCos::Find(CosEmotes[Slot]);
	UAnimSequence* A = SDCos::EmoteAnim(C);
	if (!A) return;
	if (Mode == ESDMode::Build) ToggleBuild();
	EmoteSeq = A;
	EmoteTime = 0.f;
	EmoteRate = C->Rate;
	bAds = false;
	HeldRoot->SetVisibility(false, true);
}

void ASDCharacter::StopEmote()
{
	EmoteSeq = nullptr;
	EmoteTime = 0.f;
	if (!bDead) HeldRoot->SetVisibility(true, true);
}