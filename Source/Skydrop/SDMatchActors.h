#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SDMatchActors.generated.h"

class UStaticMeshComponent;

UCLASS()
class SKYDROP_API ASDStorm : public AActor
{
	GENERATED_BODY()
public:
	ASDStorm();
	void Init(float InIslandR);
	void Start();
	virtual void Tick(float Dt) override;
	bool IsInside(const FVector& P) const { return FVector2D::Distance(FVector2D(P), Center) <= Radius; }
	bool IsInsideNext(const FVector& P) const { return FVector2D::Distance(FVector2D(P), NextCenter) <= NextRadius; }

	struct FPhase { float Wait, Shrink, Frac, Damage; };
	TArray<FPhase> Phases;
	int32 Index = -1;
	float Timer = 0.f;
	bool bShrinking = false;
	bool bActive = false;
	FVector2D Center = FVector2D::ZeroVector, NextCenter = FVector2D::ZeroVector, StartCenter = FVector2D::ZeroVector;
	float Radius = 1.f, NextRadius = 1.f, StartRadius = 1.f;
	float Damage = 1.f;
	float IslandR = 30000.f;
	float DamageTimer = 1.f;

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Wall;

private:
	void NextPhase();
	void UpdateVisual();
};

UCLASS()
class SKYDROP_API ASDBus : public AActor
{
	GENERATED_BODY()
public:
	ASDBus();
	void Init(float IslandR);
	virtual void Tick(float Dt) override;
	float Progress() const { return Duration > 0 ? FMath::Clamp(Time / Duration, 0.f, 1.f) : 1.f; }
	bool DoorsOpen() const { return Time > 0.f; }

	FVector StartPos, EndPos;
	float Duration = 30.f;
	float Time = -6.f; // countdown before doors open

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<USceneComponent> Body;
};
