#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SDCosmetics.h"
#include "SDSave.generated.h"

// Persistent player profile: XP / level, coins, owned cosmetics, loadout, battle pass, challenges, stats
UCLASS()
class SKYDROP_API USDSave : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() int32 XP = 0;
	UPROPERTY() int32 Coins = 300;
	UPROPERTY() int32 Matches = 0;
	UPROPERTY() int32 Wins = 0;
	UPROPERTY() int32 Kills = 0;
	UPROPERTY() int32 Top10 = 0;
	UPROPERTY() int32 Skin = 0;      // legacy
	UPROPERTY() int32 Mode = 1;
	UPROPERTY() TArray<int32> Challenge;
	UPROPERTY() TArray<bool> ChallengeDone;

	UPROPERTY() TArray<FName> Owned;
	UPROPERTY() FName Outfit = TEXT("o_recruit");
	UPROPERTY() FName BackBling = NAME_None;
	UPROPERTY() FName Pickaxe = TEXT("p_default");
	UPROPERTY() FName Glider = TEXT("g_default");
	UPROPERTY() TArray<FName> Emotes;      // 6 slots
	UPROPERTY() bool bPassOwned = false;
	UPROPERTY() int32 PassClaimedFree = 0;    // coin rewards paid up to this tier
	UPROPERTY() int32 PassClaimedPremium = 0;

	static constexpr int32 XPPerLevel = 1000;
	int32 Level() const { return XP / XPPerLevel + 1; }
	float LevelFrac() const { return (XP % XPPerLevel) / float(XPPerLevel); }
	int32 Tier() const { return FMath::Min(Level(), SDCos::PassTiers); }

	bool Owns(FName Id) const;
	FName Equipped(ESDCos Type, int32 EmoteSlot = 0) const;
	void Equip(ESDCos Type, FName Id, int32 EmoteSlot = 0);
	bool Buy(FName Id);
	bool BuyPass();
	// grants battle pass items/coins for every tier reached; returns lines describing new rewards
	TArray<FString> ProcessPass();

	static USDSave* Load();
	void Write();
};

namespace SDProfile
{
	struct FChallenge { const TCHAR* Text; int32 Goal; int32 Reward; };
	const FChallenge& Challenge(int32 I);
	constexpr int32 NumChallenges = 5;
	struct FMode { const TCHAR* Name; const TCHAR* Desc; int32 Bots; };
	const FMode& Mode(int32 I);
	constexpr int32 NumModes = 3;

	// shop: rotates daily (UTC); featured = 3 big cards, daily = 6
	void ShopToday(TArray<FName>& Featured, TArray<FName>& Daily);
	FTimespan TimeToReset();
	int32 SeasonDaysLeft();

	// legacy entry used by the old lobby code
	void ApplySkin(class USkeletalMeshComponent* Mesh, int32 Skin);
}
