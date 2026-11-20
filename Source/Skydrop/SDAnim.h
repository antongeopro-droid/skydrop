#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "SDAnim.generated.h"

class UAnimSequence;

struct FSDAnimSample
{
	const UAnimSequence* Seq = nullptr;
	float Time = 0.f;
	float Weight = 0.f;
};

struct FSDAnimFrame
{
	TArray<FSDAnimSample> Base;  // full-body locomotion blend
	FSDAnimSample Upper;         // layered on spine_01 and above (reload, swing)
	float UpperWeight = 0.f;
	FSDAnimSample AimUp;         // mesh-space rotation additives
	FSDAnimSample AimDown;
	FSDAnimSample Fire;
};

// Evaluates the pose on the animation worker thread from a snapshot taken on the game thread
struct FSDAnimProxy : public FAnimInstanceProxy
{
	FSDAnimProxy() {}
	FSDAnimProxy(UAnimInstance* In) : FAnimInstanceProxy(In) {}
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	FSDAnimFrame Frame;
	TArray<uint8> UpperMask;
	int32 MaskBones = -1;
};

UCLASS(Transient)
class SKYDROP_API USDAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float Dt) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FSDAnimProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete InProxy; }

	FSDAnimFrame Frame;

private:
	struct FSet
	{
		UAnimSequence* Idle = nullptr;
		UAnimSequence* Walk[8] = {};
		UAnimSequence* Jog[8] = {};
		UAnimSequence* Fall = nullptr;
		UAnimSequence* Reload = nullptr;
		UAnimSequence* AimUp = nullptr;
		UAnimSequence* AimDown = nullptr;
		UAnimSequence* Fire = nullptr;
		UAnimSequence* Swing = nullptr;
	};

	UPROPERTY(Transient) TArray<TObjectPtr<UAnimSequence>> Keep; // keeps loaded sequences alive
	FSet Sets[3]; // 0 unarmed, 1 rifle, 2 pistol
	float Phase = 0.f;
	float IdleTime = 0.f;
	float AirBlend = 0.f;
	float FallTime = 0.f;
	float SwingTime = 10.f;
	float FireTime = 10.f;
	float ReloadTime = 0.f;
	int32 LastShots = 0;
	bool bWasSwinging = false;

	UAnimSequence* Load(const TCHAR* Rel);
};
