#pragma once

#include "CoreMinimal.h"

enum class ESDBuildType : uint8 { Wall, Floor, Ramp, Cone };
enum class ESDMat : uint8 { Wood, Stone, Metal };
enum class ESDWeapon : uint8 { None, AR, Pump, SMG, Pistol, Sniper, Tactical, Count };
enum class ESDItem : uint8 { None, Weapon, Bandage, Medkit, MiniShield, BigShield, Ammo, Mats };
enum class ESDAmmo : uint8 { Light, Medium, Heavy, Shells, Count };

namespace SDGrid
{
	constexpr float Tile = 512.f;
	constexpr float Level = 384.f;
	constexpr int32 Cost = 10;
}

struct FSDBuildSlot
{
	ESDBuildType Type = ESDBuildType::Wall;
	int32 I = 0, J = 0, L = 0;
	int32 Axis = 0; // walls: 0 = lies on an X grid line, 1 = on a Y grid line
	int32 Dir = 0;  // ramps: 0 +X, 1 +Y, 2 -X, 3 -Y (direction it rises toward)
};

struct FSDItemStack
{
	ESDItem Item = ESDItem::None;
	ESDWeapon Weapon = ESDWeapon::None;
	int32 Rarity = 0;
	int32 Count = 0; // weapon: rounds in mag, consumable: stack, ammo/mats: amount
	int32 Sub = 0;   // ammo type or material type
	bool IsEmpty() const { return Item == ESDItem::None; }
};

struct FSDWeaponDef
{
	const TCHAR* Name;
	float Damage;       // per pellet for shotguns, common rarity
	float FireInterval;
	int32 Mag;
	float Reload;
	float HipSpread;    // degrees (cone half angle)
	float AdsSpread;
	float Bloom;        // added per shot
	float BloomMax;
	float BloomRecover; // degrees per second
	int32 Pellets;
	float HeadMult;
	float Range;
	ESDAmmo Ammo;
	bool bAuto;
	float AdsFov;
	FLinearColor Body;
	FVector Size;       // visual size of the held model (length, width, height)
};

namespace SDData
{
	const FSDWeaponDef& Weapon(ESDWeapon W);
	float WeaponDamage(ESDWeapon W, int32 Rarity);
	FLinearColor RarityColor(int32 R);
	const TCHAR* RarityName(int32 R);
	FString ItemName(const FSDItemStack& S);
	int32 MaxStack(ESDItem I);
	const TCHAR* AmmoName(ESDAmmo A);
	const TCHAR* MatName(ESDMat M);
	FLinearColor MatColor(ESDMat M);
	float UseTime(ESDItem I);
}
