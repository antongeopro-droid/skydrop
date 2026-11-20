#include "SDAnim.h"
#include "SDCharacter.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "Animation/AnimationPoseData.h"
#include "GameFramework/CharacterMovementComponent.h"

// ------------------------------------------------------------------ game thread

UAnimSequence* USDAnimInstance::Load(const TCHAR* Rel)
{
	const FString Path = FString::Printf(TEXT("/Game/Characters/Mannequins/Anims/%s"), Rel);
	UAnimSequence* S = LoadObject<UAnimSequence>(nullptr, *Path, nullptr, LOAD_NoWarn);
	if (S) Keep.Add(S);
	else UE_LOG(LogTemp, Warning, TEXT("[sdanim] missing %s"), *Path);
	return S;
}

void USDAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	static const TCHAR* Dirs[8] = { TEXT("Fwd"), TEXT("Fwd_Right"), TEXT("Right"), TEXT("Bwd_Right"), TEXT("Bwd"), TEXT("Bwd_Left"), TEXT("Left"), TEXT("Fwd_Left") };
	struct FDef { const TCHAR* Folder; const TCHAR* Tag; };
	const FDef Defs[3] = { { TEXT("Unarmed"), TEXT("Unarmed") }, { TEXT("Rifle"), TEXT("Rifle") }, { TEXT("Pistol"), TEXT("Pistol") } };
	for (int32 s = 0; s < 3; ++s)
	{
		FSet& S = Sets[s];
		for (int32 d = 0; d < 8; ++d)
		{
			S.Walk[d] = Load(*FString::Printf(TEXT("%s/Walk/MF_%s_Walk_%s.MF_%s_Walk_%s"), Defs[s].Folder, Defs[s].Tag, Dirs[d], Defs[s].Tag, Dirs[d]));
			S.Jog[d] = Load(*FString::Printf(TEXT("%s/Jog/MF_%s_Jog_%s.MF_%s_Jog_%s"), Defs[s].Folder, Defs[s].Tag, Dirs[d], Defs[s].Tag, Dirs[d]));
		}
	}
	Sets[0].Idle = Load(TEXT("Unarmed/MM_Idle.MM_Idle"));
	Sets[0].Fall = Load(TEXT("Unarmed/Jump/MM_Fall_Loop.MM_Fall_Loop"));
	Sets[0].Swing = Load(TEXT("Unarmed/Attack/MM_Attack_01.MM_Attack_01"));

	Sets[1].Idle = Load(TEXT("Rifle/MF_Rifle_Idle_ADS.MF_Rifle_Idle_ADS"));
	Sets[1].Fall = Load(TEXT("Rifle/Jump/MM_Rifle_Jump_Fall_Loop.MM_Rifle_Jump_Fall_Loop"));
	Sets[1].Reload = Load(TEXT("Rifle/MM_Rifle_Reload.MM_Rifle_Reload"));
	Sets[1].AimUp = Load(TEXT("Rifle/AIM/MM_Rifle_Idle_ADS_AO_CU.MM_Rifle_Idle_ADS_AO_CU"));
	Sets[1].AimDown = Load(TEXT("Rifle/AIM/MM_Rifle_Idle_ADS_AO_CD.MM_Rifle_Idle_ADS_AO_CD"));
	Sets[1].Fire = Load(TEXT("Rifle/MM_Rifle_Fire.MM_Rifle_Fire"));

	Sets[2].Idle = Load(TEXT("Pistol/MF_Pistol_Idle_ADS.MF_Pistol_Idle_ADS"));
	Sets[2].Fall = Load(TEXT("Pistol/Jump/MM_Pistol_Jump_Fall_Loop.MM_Pistol_Jump_Fall_Loop"));
	Sets[2].Reload = Load(TEXT("Pistol/MM_Pistol_Reload.MM_Pistol_Reload"));
	Sets[2].AimUp = Load(TEXT("Pistol/Aim/MF_Pistol_Idle_ADS_AO_CU.MF_Pistol_Idle_ADS_AO_CU"));
	Sets[2].AimDown = Load(TEXT("Pistol/Aim/MF_Pistol_Idle_ADS_AO_CD.MF_Pistol_Idle_ADS_AO_CD"));
	Sets[2].Fire = Load(TEXT("Pistol/MM_Pistol_Fire.MM_Pistol_Fire"));
}

void USDAnimInstance::NativeUpdateAnimation(float Dt)
{
	Super::NativeUpdateAnimation(Dt);
	Frame = FSDAnimFrame();
	ASDCharacter* C = Cast<ASDCharacter>(TryGetPawnOwner());
	if (!C) return;

	// which animation set
	int32 SetIdx = 0;
	const FSDItemStack* It = C->ActiveItem();
	const bool bWeapon = C->Mode == ESDMode::Combat && It && It->Item == ESDItem::Weapon && C->Air == ESDAir::Ground;
	if (bWeapon) SetIdx = It->Weapon == ESDWeapon::Pistol ? 2 : 1;
	const FSet& S = Sets[SetIdx];

	// locomotion
	const FVector V = C->GetVelocity();
	const FVector Local = C->GetActorRotation().UnrotateVector(FVector(V.X, V.Y, 0.f));
	const float Speed = Local.Size();
	const bool bFalling = C->GetCharacterMovement() && C->GetCharacterMovement()->IsFalling();
	AirBlend = FMath::FInterpTo(AirBlend, bFalling ? 1.f : 0.f, Dt, 10.f);
	FallTime = bFalling ? FallTime + Dt : 0.f;
	IdleTime += Dt;

	const float IdleW = 1.f - FMath::Clamp(Speed / 140.f, 0.f, 1.f);
	const float JogF = FMath::Clamp((Speed - 230.f) / 220.f, 0.f, 1.f);
	const float WalkRate = Speed / 190.f;
	const float JogRate = Speed / 480.f;
	const float CycleLen = FMath::Lerp(1.45f, 1.6f, JogF);
	Phase = FMath::Fmod(Phase + Dt * FMath::Lerp(WalkRate, JogRate, JogF) / CycleLen, 1.f);

	float Ang = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
	if (Ang < 0.f) Ang += 360.f;
	const int32 D0 = FMath::FloorToInt(Ang / 45.f) % 8;
	const int32 D1 = (D0 + 1) % 8;
	const float DA = (Ang - D0 * 45.f) / 45.f;

	const float Ground = 1.f - AirBlend;
	auto Add = [&](const UAnimSequence* Seq, float Time, float W)
	{
		if (!Seq || W <= 0.001f) return;
		FSDAnimSample Smp;
		Smp.Seq = Seq;
		Smp.Time = FMath::Fmod(Time, FMath::Max(Seq->GetPlayLength(), 0.01f));
		Smp.Weight = W;
		Frame.Base.Add(Smp);
	};
	auto AddCycle = [&](UAnimSequence* const* Arr, float W)
	{
		if (W <= 0.001f) return;
		if (Arr[D0]) Add(Arr[D0], Phase * Arr[D0]->GetPlayLength(), W * (1.f - DA));
		if (Arr[D1]) Add(Arr[D1], Phase * Arr[D1]->GetPlayLength(), W * DA);
	};
	Add(S.Idle, IdleTime, Ground * IdleW);
	AddCycle(S.Walk, Ground * (1.f - IdleW) * (1.f - JogF));
	AddCycle(S.Jog, Ground * (1.f - IdleW) * JogF);
	Add(S.Fall ? S.Fall : Sets[0].Fall, FallTime, AirBlend);

	// upper-body layers
	const bool bSwinging = C->SwingAnim > 0.f && C->Mode == ESDMode::Combat && C->Active < 0;
	if (bSwinging && !bWasSwinging) SwingTime = 0.f;
	bWasSwinging = bSwinging;
	SwingTime += Dt;
	if (Sets[0].Swing && SwingTime < 0.5f)
	{
		Frame.Upper.Seq = Sets[0].Swing;
		Frame.Upper.Time = FMath::Min(SwingTime * 2.2f + 0.05f, Sets[0].Swing->GetPlayLength());
		Frame.UpperWeight = FMath::Clamp(FMath::Min(SwingTime, 0.5f - SwingTime) * 10.f, 0.f, 1.f);
	}
	if (bWeapon && C->ReloadLeft > 0.f && S.Reload)
	{
		const float Total = SDData::Weapon(It->Weapon).Reload;
		const float Elapsed = Total - C->ReloadLeft;
		Frame.Upper.Seq = S.Reload;
		Frame.Upper.Time = FMath::Clamp(Elapsed / Total, 0.f, 1.f) * S.Reload->GetPlayLength();
		Frame.UpperWeight = FMath::Clamp(FMath::Min(Elapsed, C->ReloadLeft) * 6.f, 0.f, 1.f);
	}

	// aim pitch and recoil
	if (bWeapon)
	{
		const float Pitch = FRotator::NormalizeAxis(C->Controller ? C->Controller->GetControlRotation().Pitch : C->GetBaseAimRotation().Pitch);
		if (S.AimUp && Pitch > 0.f) { Frame.AimUp.Seq = S.AimUp; Frame.AimUp.Weight = FMath::Clamp(Pitch / 80.f, 0.f, 1.f); }
		if (S.AimDown && Pitch < 0.f) { Frame.AimDown.Seq = S.AimDown; Frame.AimDown.Weight = FMath::Clamp(-Pitch / 80.f, 0.f, 1.f); }
		if (C->ShotCount != LastShots) { LastShots = C->ShotCount; FireTime = 0.f; }
		FireTime += Dt;
		if (S.Fire && FireTime < S.Fire->GetPlayLength())
		{
			Frame.Fire.Seq = S.Fire;
			Frame.Fire.Time = FireTime;
			Frame.Fire.Weight = It->Weapon == ESDWeapon::SMG ? 0.6f : 1.f;
		}
	}
	else
	{
		LastShots = C->ShotCount;
	}

	// an emote takes over the whole body
	if (C->EmoteSeq)
	{
		Frame = FSDAnimFrame();
		FSDAnimSample E;
		E.Seq = C->EmoteSeq;
		E.Time = FMath::Clamp(C->EmoteTime, 0.f, C->EmoteSeq->GetPlayLength());
		E.Weight = 1.f;
		Frame.Base.Add(E);
		return;
	}

	// normalise base weights
	float Sum = 0.f;
	for (const FSDAnimSample& B : Frame.Base) Sum += B.Weight;
	if (Sum > 0.f) for (FSDAnimSample& B : Frame.Base) B.Weight /= Sum;
}

// ------------------------------------------------------------------ worker thread

void FSDAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	if (const USDAnimInstance* I = Cast<USDAnimInstance>(InAnimInstance))
	{
		Frame = I->Frame;
	}
}

namespace
{
	void Extract(const FSDAnimSample& S, FCompactPose& Pose, FBlendedCurve& Curve, UE::Anim::FStackAttributeContainer& Attr)
	{
		FAnimationPoseData Data(Pose, Curve, Attr);
		S.Seq->GetAnimationPose(Data, FAnimExtractContext((double)S.Time, false, FDeltaTimeRecord(), true));
	}
}

bool FSDAnimProxy::Evaluate(FPoseContext& Output)
{
	const FBoneContainer& BC = Output.Pose.GetBoneContainer();
	TArray<const FSDAnimSample*, TInlineAllocator<8>> Valid;
	for (const FSDAnimSample& S : Frame.Base)
	{
		if (S.Seq && S.Weight > 0.f) Valid.Add(&S);
	}
	if (Valid.Num() == 0)
	{
		Output.ResetToRefPose();
		return true;
	}

	// 1) full-body locomotion blend
	{
		const int32 N = Valid.Num();
		TArray<FCompactPose, TInlineAllocator<8>> Poses;
		TArray<FBlendedCurve, TInlineAllocator<8>> Curves;
		TArray<UE::Anim::FStackAttributeContainer, TInlineAllocator<8>> Attrs;
		TArray<float, TInlineAllocator<8>> W;
		Poses.SetNum(N);
		Curves.SetNum(N);
		Attrs.SetNum(N);
		W.SetNum(N);
		for (int32 i = 0; i < N; ++i)
		{
			Poses[i].SetBoneContainer(&BC);
			Curves[i].InitFrom(Output.Curve);
			Extract(*Valid[i], Poses[i], Curves[i], Attrs[i]);
			W[i] = Valid[i]->Weight;
		}
		FAnimationPoseData Out(Output);
		FAnimationRuntime::BlendPosesTogether(Poses, Curves, Attrs, W, Out);
	}

	// 2) layered upper body (spine_01 and children)
	if (Frame.Upper.Seq && Frame.UpperWeight > 0.f)
	{
		const int32 NumBones = Output.Pose.GetNumBones();
		if (MaskBones != NumBones)
		{
			UpperMask.Init(0, NumBones);
			const FReferenceSkeleton& Ref = BC.GetReferenceSkeleton();
			const int32 Spine = Ref.FindBoneIndex(TEXT("spine_01"));
			for (FCompactPoseBoneIndex I : Output.Pose.ForEachBoneIndex())
			{
				int32 B = BC.MakeMeshPoseIndex(I).GetInt();
				while (B != INDEX_NONE)
				{
					if (B == Spine) { UpperMask[I.GetInt()] = 1; break; }
					B = Ref.GetParentIndex(B);
				}
			}
			MaskBones = NumBones;
		}
		FCompactPose UpperPose;
		UpperPose.SetBoneContainer(&BC);
		FBlendedCurve UC;
		UC.InitFrom(Output.Curve);
		UE::Anim::FStackAttributeContainer UA;
		Extract(Frame.Upper, UpperPose, UC, UA);
		const float A = FMath::Clamp(Frame.UpperWeight, 0.f, 1.f);
		for (FCompactPoseBoneIndex I : Output.Pose.ForEachBoneIndex())
		{
			if (UpperMask.IsValidIndex(I.GetInt()) && UpperMask[I.GetInt()])
			{
				FTransform Blended;
				Blended.Blend(Output.Pose[I], UpperPose[I], A);
				Output.Pose[I] = Blended;
			}
		}
		Output.Pose.NormalizeRotations();
	}

	// 3) mesh-space additives: aim up/down, fire kick
	auto ApplyAdditive = [&](const FSDAnimSample& S)
	{
		if (!S.Seq || S.Weight <= 0.001f) return;
		FCompactPose Add;
		Add.SetBoneContainer(&BC);
		FBlendedCurve AC;
		AC.InitFrom(Output.Curve);
		UE::Anim::FStackAttributeContainer AA;
		Extract(S, Add, AC, AA);
		FAnimationPoseData Base(Output);
		const FAnimationPoseData AddData(Add, AC, AA);
		FAnimationRuntime::AccumulateMeshSpaceRotationAdditiveToLocalPose(Base, AddData, S.Weight);
	};
	ApplyAdditive(Frame.AimUp);
	ApplyAdditive(Frame.AimDown);
	ApplyAdditive(Frame.Fire);
	Output.Pose.NormalizeRotations();
	return true;
}
