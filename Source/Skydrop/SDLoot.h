#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkydropTypes.h"
#include "SDLoot.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class ASDCharacter;

enum class ESDResource : uint8 { Tree, Rock, Car, Container, Cactus, Palm };

// Harvestable world object (trees, rocks, cars) with a moving weak point
UCLASS()
class SKYDROP_API ASDResource : public AActor
{
	GENERATED_BODY()
public:
	ASDResource();
	void Setup(ESDResource InType, float Size, int32 Seed);
	void Harvest(ASDCharacter* By, const FHitResult& Hit);
	virtual void Tick(float Dt) override;
	virtual float TakeDamage(float Amount, struct FDamageEvent const& Ev, AController* Inst, AActor* Causer) override;

	ESDResource Type = ESDResource::Tree;
	ESDMat Gives = ESDMat::Wood;
	float HP = 300.f;
	int32 PerHit = 6;
	float Wobble = 0.f;
	FVector BaseScale = FVector::OneVector;

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<USceneComponent> Body;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> WeakSpot;
};

// Item lying on the ground
UCLASS()
class SKYDROP_API ASDPickup : public AActor
{
	GENERATED_BODY()
public:
	// whoever dropped this is ignored by auto-pickup until they walk away from it
	TWeakObjectPtr<class ASDCharacter> DroppedBy;
	ASDPickup();
	void Setup(const FSDItemStack& InItem);
	virtual void Tick(float Dt) override;
	bool IsAutoPickup() const { return Item.Item == ESDItem::Ammo || Item.Item == ESDItem::Mats; }

	FSDItemStack Item;
	float Age = 0.f;
	float Phase = 0.f;
	FVector Velocity = FVector::ZeroVector;
	bool bSettled = false;
	float AutoTimer = 0.f;

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<USceneComponent> Spin;
	UPROPERTY() TObjectPtr<UPointLightComponent> Glow;
};

UCLASS()
class SKYDROP_API ASDChest : public AActor
{
	GENERATED_BODY()
public:
	ASDChest();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	void Open(ASDCharacter* By);

	bool bOpened = false;
	float LidAnim = 0.f;

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<USceneComponent> Lid;
	UPROPERTY() TObjectPtr<UPointLightComponent> Glow;
};

// Short-lived bullet tracer streak
UCLASS()
class SKYDROP_API ASDTracer : public AActor
{
	GENERATED_BODY()
public:
	ASDTracer();
	void Setup(const FVector& From, const FVector& To);
	virtual void Tick(float Dt) override;
	float Life = 0.06f;
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Streak;
};

namespace SDLoot
{
	FSDItemStack RollWeapon(int32 MinRarity = 0);
	FSDItemStack RollConsumable();
	FSDItemStack AmmoFor(ESDWeapon W);
	ASDPickup* Spawn(UWorld* World, const FSDItemStack& Item, const FVector& At, bool bToss = false);
	void SpawnFloorLoot(UWorld* World, const FVector& At);
}
