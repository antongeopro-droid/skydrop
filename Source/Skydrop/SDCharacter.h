#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SkydropTypes.h"
#include "SDCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class ASDBuildPiece;

enum class ESDAir : uint8 { Bus, Skydive, Glide, Ground };
enum class ESDMode : uint8 { Combat, Build };

struct FSDDamageNumber
{
	FVector Pos;
	FString Text;
	float Time;
	FLinearColor Color;
};

UCLASS()
class SKYDROP_API ASDCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	ASDCharacter();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	virtual float TakeDamage(float Amount, struct FDamageEvent const& Ev, AController* Inst, AActor* Causer) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> Boom;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY() TObjectPtr<USceneComponent> HeldRoot;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> HeldParts;
	UPROPERTY() TObjectPtr<USceneComponent> GliderRoot;
	UPROPERTY() TObjectPtr<ASDBuildPiece> Ghost;
	UPROPERTY() TObjectPtr<USceneComponent> BlingRoot;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BlingParts;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> GliderParts;
	UPROPERTY() TObjectPtr<class UAnimSequence> EmoteSeq;

	// --- state
	float Health = 100.f;
	float Shield = 0.f;
	bool bDead = false;
	bool bIsBot = false;
	FString DisplayName = TEXT("Player");
	int32 Kills = 0;
	ESDAir Air = ESDAir::Ground;
	ESDMode Mode = ESDMode::Combat;
	FLinearColor Tint = FLinearColor::White;

	// --- inventory
	FSDItemStack Slots[5];
	int32 Active = -1; // -1 = pickaxe
	int32 Ammo[(int32)ESDAmmo::Count] = { 0, 0, 0, 0 };
	int32 Mats[3] = { 0, 0, 0 };

	// --- build
	ESDBuildType BuildType = ESDBuildType::Wall;
	ESDMat BuildMat = ESDMat::Wood;
	bool bGhostValid = false;
	FSDBuildSlot GhostSlot;

	// --- combat
	bool bTrigger = false;
	bool bAds = false;
	float Cooldown = 0.f;
	float Bloom = 0.f;
	float ReloadLeft = 0.f;
	float UseLeft = 0.f;
	float SwingAnim = 0.f;
	float LastDamageTime = -100.f;
	TWeakObjectPtr<ASDCharacter> LastAttacker;
	float HitMarkerTime = -10.f;
	bool bHitMarkerHead = false;
	float DamagedFlash = 0.f;
	TArray<FSDDamageNumber> DamageNumbers;
	float FallStartZ = 0.f;
	float MeshLean = 0.f;
	int32 ShotCount = 0;
	FName CosOutfit, CosBling, CosPickaxe = TEXT("p_default"), CosGlider = TEXT("g_default");
	TArray<FName> CosEmotes;
	float EmoteTime = 0.f;
	float EmoteRate = 1.f;
	bool bEmoteWheel = false;
	void ApplyCosmetics(FName Outfit, FName Bling, FName Pickaxe, FName Glider, const TArray<FName>& Emotes);
	void PlayEmote(int32 Slot);
	void StopEmote();
	float MuzzleDist = 60.f;
	float ScopeT = 0.f;          // seconds looking through the sniper scope (0 = not scoped)
	bool bScopeHidden = false;

	// --- edit mode (Fortnite-style tile editing)
	TWeakObjectPtr<ASDBuildPiece> EditTarget;
	int32 EditSel = 0;          // selected (removed) tiles
	TArray<int32> EditPath;     // ramp tiles in the order they were drawn
	int32 EditPaint = -1;       // -1 not dragging, 1 selecting, 0 deselecting
	int32 EditHover = -1;
	bool IsEditing() const { return EditTarget.IsValid(); }
	void BeginEdit(ASDBuildPiece* P);
	void EndEdit(bool bConfirm);
	void ResetEdit();
	void TickEdit();
	FRotator PickaxeGrip = FRotator(0.f, 0.f, 0.f);

	// --- input API (used by player controller and bots)
	void Move(const FVector2D& In);
	void JumpPressed();
	void PrimaryDown();
	void PrimaryUp();
	void SetAds(bool bOn);
	void ReloadPressed();
	void SelectSlot(int32 Idx);
	void ScrollSlot(int32 Delta);
	void ToggleBuild();
	void SelectBuild(ESDBuildType T);
	void CycleMat();
	void EditPressed();
	void InteractPressed();
	void InteractWith(AActor* Target);
	void CrouchToggle();

	// --- queries
	bool GetAimRay(FVector& Start, FVector& Dir) const;
	AActor* FindInteractable() const;
	FString InteractPrompt() const;
	bool ComputeBuildSlot(ESDBuildType T, FSDBuildSlot& Out) const;
	bool PlaceBuild(ESDBuildType T);
	const FSDItemStack* ActiveItem() const;
	bool HasWeapon() const;
	int32 BestWeaponSlot(float Distance) const;
	float CurrentSpread() const;
	ASDBuildPiece* AimedPiece(float MaxDist) const;
	float GroundDistance() const;

	// --- items
	bool GiveItem(const FSDItemStack& S, bool bSwapIfFull);
	void DropAll();
	void OnHarvest(const FVector& At, int32 Gain, bool bWeak);
	void AddDmgNumber(const FVector& P, const FString& Text, const FLinearColor& C);

	// --- match
	void EnterBus();
	void LeaveBus(const FVector& At);
	void DeployGlider();
	void Die(ASDCharacter* Killer);
	void RefreshHeld();

private:
	void DoFire();
	void DoSwing();
	void UpdateGhost();
	void TickConsumable(float Dt);
	void ApplyHit(const FHitResult& Hit, float Damage, const FVector& Dir, float HeadMult, bool bShotgun);
	void SpawnTracer(const FVector& From, const FVector& To);
	void UpdateMeshPose(float Dt);
};
