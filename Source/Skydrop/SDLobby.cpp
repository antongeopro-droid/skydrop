#include "SDLobby.h"
#include "SDAssets.h"
#include "SDSave.h"
#include "SDCosmetics.h"
#include "SkydropTypes.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Animation/AnimSequence.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const FVector StudioOffset(0.f, 8000.f, 0.f);
	UAnimSequence* IdleAnim()
	{
		static UAnimSequence* A = nullptr;
		if (!A)
		{
			A = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"), nullptr, LOAD_NoWarn);
			if (A) A->AddToRoot();
		}
		return A;
	}
}

ASDLobby::ASDLobby()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Hero = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Hero"));
	Hero->SetupAttachment(Root);
	Hero->SetRelativeLocationAndRotation(FVector(0, 0, 12.f), FRotator(0, 98.f, 0));
	Hero->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ItemStage = CreateDefaultSubobject<USceneComponent>(TEXT("ItemStage"));
	ItemStage->SetupAttachment(Root);
	ItemStage->SetRelativeLocation(FVector(-40.f, -175.f, 125.f));

	HeroBling = CreateDefaultSubobject<USceneComponent>(TEXT("HeroBling"));
	HeroBling->SetupAttachment(Hero);
	HeroBling->SetRelativeLocationAndRotation(FVector(0, -20.f, 128.f), FRotator(0, 90.f, 0));

	Cam = CreateDefaultSubobject<UCameraComponent>(TEXT("Cam"));
	Cam->SetupAttachment(Root);
	Cam->SetRelativeLocationAndRotation(FVector(-640.f, 0.f, 150.f), FRotator(-6.f, 0.f, 0.f));
	Cam->SetFieldOfView(62.f);

	auto MakeLight = [this](const TCHAR* Name, const FVector& Loc, float Cd, float Rad, const FLinearColor& C, USceneComponent* Parent)
	{
		UPointLightComponent* L = CreateDefaultSubobject<UPointLightComponent>(Name);
		L->SetupAttachment(Parent);
		L->SetRelativeLocation(Loc);
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(Cd);
		L->SetAttenuationRadius(Rad);
		L->SetLightColor(C);
		return L;
	};
	Key = MakeLight(TEXT("Key"), FVector(-260.f, -180.f, 300.f), 60.f, 1500.f, FLinearColor(1.f, 0.95f, 0.9f), Root);
	Rim = MakeLight(TEXT("Rim"), FVector(220.f, 160.f, 260.f), 45.f, 1200.f, FLinearColor(0.35f, 0.6f, 1.f), Root);
	PadGlow = MakeLight(TEXT("PadGlow"), FVector(0.f, 0.f, 25.f), 10.f, 400.f, FLinearColor(0.3f, 0.85f, 1.f), Root);
	PadGlow->SetCastShadows(false);

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);
	Fog->SetFogDensity(0.012f);
	Fog->SetFogHeightFalloff(0.001f);
	Fog->SetStartDistance(800.f);
	Fog->SetFogMaxOpacity(1.f);
	Fog->SetFogInscatteringColor(FLinearColor(0.02f, 0.05f, 0.13f));

	Post = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Post"));
	Post->SetupAttachment(Root);
	Post->Settings.bOverride_AutoExposureMinBrightness = true;
	Post->Settings.AutoExposureMinBrightness = 1.f;
	Post->Settings.bOverride_AutoExposureMaxBrightness = true;
	Post->Settings.AutoExposureMaxBrightness = 1.f;
	Post->Settings.bOverride_AutoExposureBias = true;
	Post->Settings.AutoExposureBias = 0.f;
	Post->Settings.bOverride_BloomIntensity = true;
	Post->Settings.BloomIntensity = 1.0f;
	Post->Settings.bOverride_VignetteIntensity = true;
	Post->Settings.VignetteIntensity = 0.6f;

	// --- photo studio, far from the stage
	Studio = CreateDefaultSubobject<USceneComponent>(TEXT("Studio"));
	Studio->SetupAttachment(Root);
	Studio->SetRelativeLocation(StudioOffset);
	StudioHero = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("StudioHero"));
	StudioHero->SetupAttachment(Studio);
	StudioHero->SetRelativeRotation(FRotator(0, 98.f, 0));
	StudioHero->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StudioItem = CreateDefaultSubobject<USceneComponent>(TEXT("StudioItem"));
	StudioItem->SetupAttachment(Studio);
	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(Studio);
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->FOVAngle = 40.f;
	Capture->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
	Capture->PostProcessSettings.AutoExposureMinBrightness = 1.f;
	Capture->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
	Capture->PostProcessSettings.AutoExposureMaxBrightness = 1.f;
	Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
	Capture->PostProcessSettings.AutoExposureBias = 0.f;
	Capture->PostProcessBlendWeight = 1.f;
	StudioKey = MakeLight(TEXT("StudioKey"), FVector(-220.f, -140.f, 260.f), 90.f, 1200.f, FLinearColor(1.f, 0.97f, 0.92f), Studio);
	StudioFill = MakeLight(TEXT("StudioFill"), FVector(-160.f, 200.f, 80.f), 35.f, 1000.f, FLinearColor(0.7f, 0.85f, 1.f), Studio);
}

FVector ASDLobby::PadLocation(int32 Index) const
{
	const float Side = Index == 1 ? -260.f : Index == 2 ? 260.f : 0.f;
	const float Back = Index == 0 ? 0.f : 90.f;
	return GetActorTransform().TransformPosition(FVector(Back, Side, 0.f));
}

void ASDLobby::PlayIdle(USkeletalMeshComponent* M)
{
	if (UAnimSequence* Idle = IdleAnim())
	{
		M->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		M->PlayAnimation(Idle, true);
	}
}

void ASDLobby::BeginPlay()
{
	Super::BeginPlay();
	using namespace SDAssets;
	Post->bUnbound = true;

	UMaterialInterface* Navy = Solid(FLinearColor(0.03f, 0.075f, 0.18f), 0.05f, 0.9f);
	UMaterialInterface* Floor = Solid(FLinearColor(0.05f, 0.09f, 0.17f), 0.f, 0.28f);
	UMaterialInterface* PadTop = Solid(FLinearColor(0.22f, 0.27f, 0.36f), 0.f, 0.3f, 0.5f);
	UMaterialInterface* Glow = Solid(FLinearColor(0.55f, 0.92f, 1.f), 3.f);

	// room: floor and a back wall close behind the pads, fading into the fog
	AddBox(this, Root, FVector(0.f, 0, -10.f), FVector(1800.f, 3200.f, 20.f), FRotator::ZeroRotator, Floor, false);
	AddBox(this, Root, FVector(720.f, 0, 900.f), FVector(20.f, 3200.f, 2000.f), FRotator::ZeroRotator, Navy, false);

	// thin honeycomb on the back wall and the floor. Every hex has its own phase and speed
	// (per-instance custom data read by M_Hex), so each one lights up and fades on its own.
	{
		UMaterialInterface* HexMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Skydrop/Materials/M_Hex.M_Hex"));
		FRandomStream R(1337);
		auto MakeISM = [&](const FLinearColor& Col, float Base, float Peak)
		{
			UInstancedStaticMeshComponent* I = NewObject<UInstancedStaticMeshComponent>(this);
			I->SetupAttachment(Root);
			I->SetStaticMesh(Cube());
			I->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			I->SetCastShadow(false);
			I->NumCustomDataFloats = 3;
			I->RegisterComponent();
			if (HexMat)
			{
				UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(HexMat, this);
				M->SetVectorParameterValue(TEXT("Color"), Col);
				M->SetScalarParameterValue(TEXT("Base"), Base);
				M->SetScalarParameterValue(TEXT("Peak"), Peak);
				I->SetMaterial(0, M);
				HexMats.Add(M);
			}
			return I;
		};
		UInstancedStaticMeshComponent* WallHex = MakeISM(FLinearColor(0.25f, 0.55f, 1.f), 0.22f, 9.f);
		UInstancedStaticMeshComponent* FloorHex = MakeISM(FLinearColor(0.2f, 0.45f, 0.95f), 0.12f, 3.5f);
		auto AddHex = [&](UInstancedStaticMeshComponent* I, const FVector& C, const FVector& AxisA, const FVector& AxisB, const FVector& Normal, float Rad, float Thick, float Fade)
		{
			const float Phase = R.FRandRange(0.f, 2.f * PI);
			const float Speed = R.FRandRange(0.25f, 0.55f);
			for (int32 k = 0; k < 6; ++k)
			{
				const float A0 = FMath::DegreesToRadians(60.f * k), A1 = FMath::DegreesToRadians(60.f * (k + 1));
				const FVector P0 = C + (AxisA * FMath::Cos(A0) + AxisB * FMath::Sin(A0)) * Rad;
				const FVector P1 = C + (AxisA * FMath::Cos(A1) + AxisB * FMath::Sin(A1)) * Rad;
				const FVector Dir = (P1 - P0).GetSafeNormal();
				const FRotator Rot = FRotationMatrix::MakeFromYZ(Dir, Normal).Rotator();
				const int32 Idx = I->AddInstance(FTransform(Rot, (P0 + P1) * 0.5f, FVector(Thick / 100.f, Rad * 0.86f / 100.f, Thick / 100.f)));
				I->SetCustomDataValue(Idx, 0, Phase, false);
				I->SetCustomDataValue(Idx, 1, Speed, false);
				I->SetCustomDataValue(Idx, 2, Fade, false);
			}
		};
		// back wall (YZ plane, facing the camera), brightest behind the hero and fading outward
		const float WR = 22.f, WX = 708.f;
		for (int32 c = -46; c <= 46; ++c)
			for (int32 r = 0; r <= 37; ++r)
			{
				const FVector C(WX, c * 1.5f * WR, r * FMath::Sqrt(3.f) * WR + ((c & 1) ? FMath::Sqrt(3.f) * 0.5f * WR : 0.f));
				const float D = FMath::Sqrt(FMath::Square(C.Y / 1100.f) + FMath::Square((C.Z - 250.f) / 650.f));
				const float Fade = FMath::Lerp(1.f, 0.12f, FMath::SmoothStep(0.25f, 1.1f, D));
				AddHex(WallHex, C, FVector(0, 1, 0), FVector(0, 0, 1), FVector(-1, 0, 0), WR, 1.f, Fade);
			}
		// floor (XY plane, facing up), fading toward the camera and the sides
		const float FR = 45.f;
		for (int32 c = -24; c <= 24; ++c)
			for (int32 r = -11; r <= 9; ++r)
			{
				const FVector C(r * FMath::Sqrt(3.f) * FR + ((c & 1) ? FMath::Sqrt(3.f) * 0.5f * FR : 0.f), c * 1.5f * FR, 1.f);
				const float D = FMath::Sqrt(FMath::Square(C.Y / 900.f) + FMath::Square((C.X - 250.f) / 650.f));
				const float Fade = FMath::Lerp(1.f, 0.1f, FMath::SmoothStep(0.3f, 1.1f, D));
				AddHex(FloorHex, C, FVector(0, 1, 0), FVector(1, 0, 0), FVector(0, 0, 1), FR, 0.9f, Fade);
			}
		WallHex->MarkRenderStateDirty();
		FloorHex->MarkRenderStateDirty();
	}
	for (int32 p = 0; p < 3; ++p)
	{
		const FVector L = GetActorTransform().InverseTransformPosition(PadLocation(p));
		AddMesh(this, Root, Cylinder(), L + FVector(0, 0, 5.f), FVector(2.2f, 2.2f, 0.12f), FRotator::ZeroRotator, PadTop, false);
		AddMesh(this, Root, Cylinder(), L + FVector(0, 0, 11.f), FVector(1.95f, 1.95f, 0.02f), FRotator::ZeroRotator,
			p == 0 ? Glow : Solid(FLinearColor(0.3f, 0.42f, 0.6f), 0.25f), false);
		AddMesh(this, Root, Cylinder(), L + FVector(0, 0, 12.f), FVector(1.75f, 1.75f, 0.02f), FRotator::ZeroRotator, PadTop, false);
	}

	USDSave* Sv = USDSave::Load();
	ShowLoadout(Sv->Outfit, Sv->BackBling);

	// thumbnails for every cosmetic, rendered a couple per frame
	for (const FSDCosmetic& C : SDCos::All()) Queue.Add(C.Id);
	SDCos::ApplyOutfit(StudioHero, SDCos::Find(TEXT("o_recruit")));
	PlayIdle(StudioHero);
}

void ASDLobby::ShowLoadout(FName Outfit, FName BackBling)
{
	if (Outfit != ShownOutfit)
	{
		SDCos::ApplyOutfit(Hero, SDCos::Find(Outfit));
		PlayIdle(Hero);
		ShownOutfit = Outfit;
	}
	if (BackBling != ShownBling || BlingParts.Num() == 0)
	{
		for (UStaticMeshComponent* P : BlingParts) if (P) P->DestroyComponent();
		BlingParts.Reset();
		for (UStaticMeshComponent* P : SDCos::BuildBackBling(this, HeroBling, SDCos::Find(BackBling))) BlingParts.Add(P);
		ShownBling = BackBling;
	}
}

void ASDLobby::Preview(FName Id, FName BaseOutfit, FName BaseBling)
{
	ClearPreview(BaseOutfit, BaseBling);
	const FSDCosmetic* C = SDCos::Find(Id);
	if (!C) return;
	PreviewId = Id;
	switch (C->Type)
	{
	case ESDCos::Outfit:
		ShowLoadout(Id, BaseBling);
		break;
	case ESDCos::BackBling:
		ShowLoadout(BaseOutfit, Id);
		HeroYawTarget = 98.f + 150.f; // turn round to show the back
		break;
	case ESDCos::Pickaxe:
	{
		for (UStaticMeshComponent* P : SDCos::BuildPickaxe(this, ItemStage, C)) { P->SetCastShadow(true); ItemParts.Add(P); }
		ItemStage->SetRelativeScale3D(FVector(0.95f));
		ItemStage->SetRelativeLocation(FVector(-40.f, -170.f, 75.f));
		break;
	}
	case ESDCos::Glider:
	{
		for (UStaticMeshComponent* P : SDCos::BuildGlider(this, ItemStage, C)) ItemParts.Add(P);
		ItemStage->SetRelativeScale3D(FVector(0.62f));
		ItemStage->SetRelativeLocation(FVector(-40.f, -185.f, 150.f));
		break;
	}
	case ESDCos::Emote:
		PlayEmote(Id);
		break;
	default: break;
	}
}

void ASDLobby::ClearPreview(FName BaseOutfit, FName BaseBling)
{
	for (UStaticMeshComponent* P : ItemParts) if (P) P->DestroyComponent();
	ItemParts.Reset();
	PreviewId = NAME_None;
	HeroYawTarget = 98.f;
	ShowLoadout(BaseOutfit, BaseBling);
}

void ASDLobby::PlayEmote(FName Emote)
{
	const FSDCosmetic* C = SDCos::Find(Emote);
	if (UAnimSequence* A = SDCos::EmoteAnim(C))
	{
		Hero->PlayAnimation(A, false);
		Hero->SetPlayRate(C->Rate);
		EmoteLeft = A->GetPlayLength() / FMath::Max(C->Rate, 0.1f);
	}
}

UTextureRenderTarget2D* ASDLobby::Thumb(FName Id) const
{
	const TObjectPtr<UTextureRenderTarget2D>* F = Thumbs.Find(Id);
	return F ? F->Get() : nullptr;
}

void ASDLobby::StageItem(const FSDCosmetic* C)
{
	using namespace SDAssets;
	for (UStaticMeshComponent* P : StudioParts) if (P) P->DestroyComponent();
	StudioParts.Reset();
	Capture->ClearShowOnlyComponents();
	StudioItem->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);

	// rarity backdrop: bright top, deeper bottom
	const FLinearColor Rc = SDData::RarityColor(C->Rarity);
	StudioParts.Add(AddBox(this, Studio, FVector(160.f, 0, 260.f), FVector(4.f, 1400.f, 500.f), FRotator::ZeroRotator, Solid(Rc * 0.55f, 0.55f, 0.9f), false));
	StudioParts.Add(AddBox(this, Studio, FVector(150.f, 0, -100.f), FVector(4.f, 1400.f, 420.f), FRotator::ZeroRotator, Solid(Rc * 0.25f, 0.25f, 0.9f), false));

	FVector CamLoc(-170.f, 0.f, 145.f), Look(0.f, 0.f, 135.f);
	float Fov = 40.f;
	StudioHero->SetVisibility(false);
	switch (C->Type)
	{
	case ESDCos::Outfit:
		SDCos::ApplyOutfit(StudioHero, C);
		PlayIdle(StudioHero);
		StudioHero->SetVisibility(true);
		Capture->ShowOnlyComponent(StudioHero);
		CamLoc = FVector(-200.f, -30.f, 130.f); Look = FVector(0.f, 0.f, 118.f); Fov = 42.f;
		break;
	case ESDCos::Emote:
		SDCos::ApplyOutfit(StudioHero, SDCos::Find(TEXT("o_recruit")));
		if (UAnimSequence* A = SDCos::EmoteAnim(C))
		{
			StudioHero->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			StudioHero->PlayAnimation(A, false);
			StudioHero->SetPosition(A->GetPlayLength() * 0.45f, false);
			StudioHero->SetPlayRate(0.f);
		}
		StudioHero->SetVisibility(true);
		Capture->ShowOnlyComponent(StudioHero);
		CamLoc = FVector(-340.f, -60.f, 100.f); Look = FVector(0.f, 0.f, 90.f); Fov = 42.f;
		break;
	case ESDCos::BackBling:
		StudioItem->SetRelativeLocationAndRotation(FVector(0, 0, 120.f), FRotator(0, 200.f, 0));
		for (UStaticMeshComponent* P : SDCos::BuildBackBling(this, StudioItem, C)) StudioParts.Add(P);
		CamLoc = FVector(-170.f, 0.f, 128.f); Look = FVector(0.f, 0.f, 122.f); Fov = 36.f;
		break;
	case ESDCos::Pickaxe:
		StudioItem->SetRelativeLocationAndRotation(FVector(0, 0, 70.f), FRotator(0, 90.f, -35.f));
		for (UStaticMeshComponent* P : SDCos::BuildPickaxe(this, StudioItem, C)) StudioParts.Add(P);
		CamLoc = FVector(-260.f, 0.f, 105.f); Look = FVector(0.f, 0.f, 100.f); Fov = 40.f;
		break;
	case ESDCos::Glider:
		StudioItem->SetRelativeLocationAndRotation(FVector(0, 0, 120.f), FRotator(0, 0, 0));
		for (UStaticMeshComponent* P : SDCos::BuildGlider(this, StudioItem, C)) StudioParts.Add(P);
		CamLoc = FVector(-430.f, 0.f, 20.f); Look = FVector(0.f, 0.f, 95.f); Fov = 44.f;
		break;
	default: break;
	}
	for (UStaticMeshComponent* P : StudioParts) Capture->ShowOnlyComponent(P);
	Capture->SetRelativeLocationAndRotation(CamLoc, (Look - CamLoc).Rotation());
	Capture->FOVAngle = Fov;
}

void ASDLobby::Tick(float Dt)
{
	Super::Tick(Dt);
	T += Dt;
	PadGlow->SetIntensity(9.f + FMath::Sin(T * 2.2f) * 2.5f);
	// hero turns smoothly (back bling preview), inspected items spin and bob
	HeroYaw = FMath::FInterpTo(HeroYaw, HeroYawTarget, Dt, 4.f);
	Hero->SetRelativeRotation(FRotator(0.f, HeroYaw, 0.f));
	if (ItemParts.Num())
	{
		ItemStage->SetRelativeRotation(FRotator(0.f, T * 55.f, 0.f));
		ItemStage->AddLocalOffset(FVector(0.f, 0.f, FMath::Sin(T * 1.6f) * 0.15f));
	}

	// camera framing per menu
	const FVector Want = Focus == 0 ? FVector(-640.f, 0.f, 150.f) : FVector(-600.f, -250.f, 150.f);
	Cam->SetRelativeLocation(FMath::VInterpTo(Cam->GetRelativeLocation(), Want, Dt, 6.f));

	if (EmoteLeft > 0.f)
	{
		EmoteLeft -= Dt;
		if (EmoteLeft <= 0.f) { Hero->SetPlayRate(1.f); PlayIdle(Hero); }
	}

	// thumbnails
	if (!bStarted)
	{
		if (T < 0.3f) return;
		bStarted = true;
	}
	if (Queue.Num() == 0) return;
	if (Staged.IsNone())
	{
		Staged = Queue[0];
		StageFrames = 0;
		if (const FSDCosmetic* C = SDCos::Find(Staged)) StageItem(C);
		return;
	}
	if (++StageFrames < 2) return;
	UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(this);
	RT->RenderTargetFormat = RTF_RGBA8_SRGB;
	RT->ClearColor = FLinearColor::Black;
	RT->InitAutoFormat(256, 256);
	RT->UpdateResourceImmediate(true);
	Capture->TextureTarget = RT;
	Capture->CaptureScene();
	Thumbs.Add(Staged, RT);
	Queue.RemoveAt(0);
	Staged = NAME_None;
}
