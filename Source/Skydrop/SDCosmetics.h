#pragma once

#include "CoreMinimal.h"

class USkeletalMeshComponent;
class USceneComponent;
class UStaticMeshComponent;
class AActor;

enum class ESDCos : uint8 { Outfit, BackBling, Pickaxe, Glider, Emote, Count };

struct FSDCosmetic
{
	FName Id;
	ESDCos Type = ESDCos::Outfit;
	const TCHAR* Name = TEXT("");
	const TCHAR* Desc = TEXT("");
	int32 Rarity = 0;     // 0 common .. 4 legendary
	int32 Price = 0;      // coins; 0 = not sold in the shop
	int32 PassTier = 0;   // >0: battle pass reward at this tier
	bool bPremium = false;
	bool bDefault = false;
	// outfit look
	bool bQuinn = false;
	FLinearColor Torso = FLinearColor::White;
	FLinearColor Legs = FLinearColor::White;
	FLinearColor Accent = FLinearColor(0, 1, 1);
	float Glow = 1.f;
	float Metal = 0.f;
	// item look (back bling / pickaxe / glider)
	int32 Style = 0;
	FLinearColor A = FLinearColor::Gray;
	FLinearColor B = FLinearColor::White;
	// emote
	const TCHAR* Anim = nullptr;
	float Rate = 1.f;
};

namespace SDCos
{
	const TArray<FSDCosmetic>& All();
	const FSDCosmetic* Find(FName Id);
	const TCHAR* TypeName(ESDCos T);
	TArray<const FSDCosmetic*> OfType(ESDCos T);
	const FSDCosmetic* Random(ESDCos T, FRandomStream& R);

	void ApplyOutfit(USkeletalMeshComponent* Mesh, const FSDCosmetic* Outfit);
	// builds the item's meshes under Parent; returns the created components
	TArray<UStaticMeshComponent*> BuildBackBling(AActor* Owner, USceneComponent* Parent, const FSDCosmetic* C);
	TArray<UStaticMeshComponent*> BuildPickaxe(AActor* Owner, USceneComponent* Parent, const FSDCosmetic* C);
	TArray<UStaticMeshComponent*> BuildGlider(AActor* Owner, USceneComponent* Parent, const FSDCosmetic* C);
	class UAnimSequence* EmoteAnim(const FSDCosmetic* C);

	// battle pass: rewards for a tier (free and premium rows); coins rewards are encoded as negative ints in the coin amount
	constexpr int32 PassTiers = 35;
	FName PassReward(int32 Tier, bool bPremium, int32& OutCoins);
	constexpr int32 PassPrice = 950;
}
