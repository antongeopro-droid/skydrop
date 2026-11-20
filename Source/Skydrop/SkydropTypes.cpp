#include "SkydropTypes.h"

namespace
{
	const FSDWeaponDef GWeapons[] = {
		// Name            Dmg   Interval Mag Reload Hip  Ads  Bloom Max  Rec  Pel Head Range   Ammo              Auto  Fov   Body                                  Size
		{ TEXT("None"),     0.f,  1.f,     0,  1.f,   0,   0,   0,    0,   0,   1,  1,   0,      ESDAmmo::Light,   false, 80, FLinearColor::Gray,                   FVector(10,10,10) },
		{ TEXT("Assault Rifle"), 30.f, 0.18f, 30, 2.3f, 2.2f, 0.6f, 0.55f, 5.f, 9.f, 1, 1.5f, 25000, ESDAmmo::Medium, true, 62, FLinearColor(0.08f,0.08f,0.09f), FVector(78,7,12) },
		{ TEXT("Pump Shotgun"),  8.5f, 0.75f, 5, 4.4f, 4.6f, 4.0f, 0.f, 0.f, 0.f, 10, 2.0f, 3200, ESDAmmo::Shells, false, 72, FLinearColor(0.20f,0.12f,0.06f), FVector(80,7,10) },
		{ TEXT("SMG"),           17.f, 0.085f, 30, 2.2f, 2.6f, 1.6f, 0.35f, 5.5f, 10.f, 1, 1.75f, 12000, ESDAmmo::Light, true, 70, FLinearColor(0.10f,0.10f,0.12f), FVector(52,7,12) },
		{ TEXT("Pistol"),        24.f, 0.15f, 16, 1.4f, 1.6f, 0.6f, 0.9f, 5.f, 9.f, 1, 2.0f, 15000, ESDAmmo::Light, false, 70, FLinearColor(0.12f,0.12f,0.12f), FVector(30,5,12) },
		{ TEXT("Bolt Sniper"),   105.f, 1.6f, 1, 2.8f, 8.f, 0.f, 0.f, 0.f, 0.f, 1, 2.5f, 60000, ESDAmmo::Heavy, false, 22, FLinearColor(0.16f,0.18f,0.12f), FVector(110,6,12) },
		{ TEXT("Tactical Shotgun"), 7.0f, 0.66f, 8, 6.0f, 5.2f, 4.6f, 0.f, 0.f, 0.f, 10, 1.75f, 2800, ESDAmmo::Shells, false, 74, FLinearColor(0.35f,0.05f,0.04f), FVector(80,7,10) },
	};
	const float GRarityMult[5] = { 1.0f, 1.05f, 1.10f, 1.16f, 1.22f };
}

const FSDWeaponDef& SDData::Weapon(ESDWeapon W)
{
	const int32 I = FMath::Clamp((int32)W, 0, (int32)ESDWeapon::Count - 1);
	return GWeapons[I];
}

float SDData::WeaponDamage(ESDWeapon W, int32 Rarity)
{
	return FMath::RoundToFloat(Weapon(W).Damage * GRarityMult[FMath::Clamp(Rarity, 0, 4)] * 10.f) / 10.f;
}

FLinearColor SDData::RarityColor(int32 R)
{
	switch (R)
	{
	case 1: return FLinearColor(0.15f, 0.75f, 0.15f);
	case 2: return FLinearColor(0.10f, 0.45f, 1.00f);
	case 3: return FLinearColor(0.65f, 0.20f, 0.95f);
	case 4: return FLinearColor(1.00f, 0.60f, 0.05f);
	default: return FLinearColor(0.65f, 0.65f, 0.65f);
	}
}

const TCHAR* SDData::RarityName(int32 R)
{
	static const TCHAR* N[] = { TEXT("Common"), TEXT("Uncommon"), TEXT("Rare"), TEXT("Epic"), TEXT("Legendary") };
	return N[FMath::Clamp(R, 0, 4)];
}

int32 SDData::MaxStack(ESDItem I)
{
	switch (I)
	{
	case ESDItem::Bandage: return 15;
	case ESDItem::Medkit: return 3;
	case ESDItem::MiniShield: return 6;
	case ESDItem::BigShield: return 3;
	default: return 1;
	}
}

float SDData::UseTime(ESDItem I)
{
	switch (I)
	{
	case ESDItem::Bandage: return 3.5f;
	case ESDItem::Medkit: return 10.f;
	case ESDItem::MiniShield: return 2.f;
	case ESDItem::BigShield: return 5.f;
	default: return 1.f;
	}
}

const TCHAR* SDData::AmmoName(ESDAmmo A)
{
	static const TCHAR* N[] = { TEXT("Light Ammo"), TEXT("Medium Ammo"), TEXT("Heavy Ammo"), TEXT("Shells") };
	return N[FMath::Clamp((int32)A, 0, 3)];
}

const TCHAR* SDData::MatName(ESDMat M)
{
	static const TCHAR* N[] = { TEXT("Wood"), TEXT("Brick"), TEXT("Metal") };
	return N[FMath::Clamp((int32)M, 0, 2)];
}

FLinearColor SDData::MatColor(ESDMat M)
{
	switch (M)
	{
	case ESDMat::Stone: return FLinearColor(0.30f, 0.11f, 0.08f);
	case ESDMat::Metal: return FLinearColor(0.30f, 0.32f, 0.35f);
	default: return FLinearColor(0.27f, 0.14f, 0.055f);
	}
}

FString SDData::ItemName(const FSDItemStack& S)
{
	switch (S.Item)
	{
	case ESDItem::Weapon: return FString::Printf(TEXT("%s %s"), RarityName(S.Rarity), Weapon(S.Weapon).Name);
	case ESDItem::Bandage: return TEXT("Bandages");
	case ESDItem::Medkit: return TEXT("Med Kit");
	case ESDItem::MiniShield: return TEXT("Small Shield Potion");
	case ESDItem::BigShield: return TEXT("Shield Potion");
	case ESDItem::Ammo: return FString::Printf(TEXT("%d %s"), S.Count, AmmoName((ESDAmmo)S.Sub));
	case ESDItem::Mats: return FString::Printf(TEXT("%d %s"), S.Count, MatName((ESDMat)S.Sub));
	default: return TEXT("");
	}
}
