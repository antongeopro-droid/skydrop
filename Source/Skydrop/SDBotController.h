#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "SDBotController.generated.h"

class ASDCharacter;

UCLASS()
class SKYDROP_API ASDBotController : public AAIController
{
	GENERATED_BODY()
public:
	ASDBotController();
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float Dt) override;
	void OnDamaged(ASDCharacter* By);

	float DropFraction = 0.5f;
	float Skill = 0.5f;

private:
	ASDCharacter* Me() const;
	void Think();
	void TickAir(ASDCharacter* C, float Dt);
	void TickGround(ASDCharacter* C, float Dt);
	void SteerTo(ASDCharacter* C, const FVector& Goal, float Dt, bool bFace);
	bool CanSee(ASDCharacter* C, ASDCharacter* Other) const;
	void PickRoamGoal(ASDCharacter* C);

	FVector LandTarget = FVector::ZeroVector;
	FVector MoveGoal = FVector::ZeroVector;
	bool bHasGoal = false;
	TWeakObjectPtr<AActor> LootGoal;
	TWeakObjectPtr<ASDCharacter> Target;
	float ThinkTimer = 0.f;
	float SeeTime = 0.f;
	float StuckTime = 0.f;
	float StrafeTimer = 0.f;
	float StrafeDir = 1.f;
	float BurstTimer = 0.f;
	float BuildCooldown = 0.f;
	float ReactionDelay = 0.5f;
	FRotator AimNoise = FRotator::ZeroRotator;
	float LastAimNoiseTime = 0.f;
	bool bHealing = false;
	int32 HealSlot = -1;
	float BreakTimer = 0.f;
	bool bVisible = false;
	float LostTime = 0.f;
	float LootRetarget = 0.f;
};
