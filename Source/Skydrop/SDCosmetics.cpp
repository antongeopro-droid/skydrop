#include "SDCosmetics.h"
#include "SDAssets.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/AnimSequence.h"

namespace
{
	FSDCosmetic Outfit(const TCHAR* Id, const TCHAR* Name, const TCHAR* Desc, int32 Rarity, int32 Price, bool bQuinn,
		FLinearColor Torso, FLinearColor Legs, FLinearColor Accent, float Glow, float Metal = 0.f, int32 Tier = 0, bool bPrem = false, bool bDef = false)
	{
		FSDCosmetic C;
		C.Id = Id; C.Type = ESDCos::Outfit; C.Name = Name; C.Desc = Desc; C.Rarity = Rarity; C.Price = Price;
		C.bQuinn = bQuinn; C.Torso = Torso; C.Legs = Legs; C.Accent = Accent; C.Glow = Glow; C.Metal = Metal;
		C.PassTier = Tier; C.bPremium = bPrem; C.bDefault = bDef;
		return C;
	}
	FSDCosmetic Item(ESDCos Type, const TCHAR* Id, const TCHAR* Name, const TCHAR* Desc, int32 Rarity, int32 Price, int32 Style,
		FLinearColor A, FLinearColor B, float Glow = 0.f, int32 Tier = 0, bool bPrem = false, bool bDef = false)
	{
		FSDCosmetic C;
		C.Id = Id; C.Type = Type; C.Name = Name; C.Desc = Desc; C.Rarity = Rarity; C.Price = Price; C.Style = Style;
		C.A = A; C.B = B; C.Glow = Glow; C.PassTier = Tier; C.bPremium = bPrem; C.bDefault = bDef;
		return C;
	}
	FSDCosmetic Emote(const TCHAR* Id, const TCHAR* Name, const TCHAR* Desc, int32 Rarity, int32 Price, const TCHAR* Anim, float Rate,
		int32 Tier = 0, bool bPrem = false, bool bDef = false)
	{
		FSDCosmetic C;
		C.Id = Id; C.Type = ESDCos::Emote; C.Name = Name; C.Desc = Desc; C.Rarity = Rarity; C.Price = Price; C.Anim = Anim; C.Rate = Rate;
		C.PassTier = Tier; C.bPremium = bPrem; C.bDefault = bDef;
		return C;
	}
	FLinearColor H(uint32 Hex) // sRGB hex -> linear
	{
		return FLinearColor(FColor((Hex >> 16) & 255, (Hex >> 8) & 255, Hex & 255));
	}
}

const TArray<FSDCosmetic>& SDCos::All()
{
	static TArray<FSDCosmetic> L;
	if (L.Num() > 0) return L;
	// ---- outfits
	L.Add(Outfit(TEXT("o_recruit"), TEXT("Recruit"), TEXT("Fresh off the airship."), 0, 0, false, H(0xEBEBEB), H(0xBDBDBD), H(0x00FFFF), 1.f, 0.f, 0, false, true));
	L.Add(Outfit(TEXT("o_scout"), TEXT("Scout"), TEXT("Eyes on the horizon."), 0, 0, true, H(0xE0E0E0), H(0xA8A8A8), H(0x00FFFF), 1.f, 0.f, 0, false, true));
	L.Add(Outfit(TEXT("o_nightops"), TEXT("Night Ops"), TEXT("Glows where it counts."), 3, 1500, false, H(0x161616), H(0x101010), H(0x9B30FF), 12.f));
	L.Add(Outfit(TEXT("o_crimson"), TEXT("Crimson Ace"), TEXT("First in, last out."), 2, 1200, true, H(0xC4161C), H(0x222222), H(0xFF8A00), 4.f));
	L.Add(Outfit(TEXT("o_goldrush"), TEXT("Gold Rush"), TEXT("Worth every coin."), 4, 2000, false, H(0xE0B030), H(0xB08020), H(0xFFFFFF), 3.f, 0.95f));
	L.Add(Outfit(TEXT("o_frostbite"), TEXT("Frostbite"), TEXT("Stay frosty."), 3, 1500, true, H(0x9FE4FF), H(0xF2FBFF), H(0x40C0FF), 10.f));
	L.Add(Outfit(TEXT("o_camo"), TEXT("Jungle Camo"), TEXT("Blend into the green."), 1, 800, false, H(0x4A6B2A), H(0x6B5B35), H(0xB8FF40), 1.f));
	L.Add(Outfit(TEXT("o_desert"), TEXT("Desert Ranger"), TEXT("Sand in every pocket."), 1, 800, true, H(0xC9A36A), H(0x7A5232), H(0xFFD27A), 1.f));
	L.Add(Outfit(TEXT("o_neon"), TEXT("Neon Rider"), TEXT("Lights up the night."), 4, 2000, false, H(0x0D0D14), H(0x0D0D14), H(0xFF2BD6), 18.f));
	L.Add(Outfit(TEXT("o_toxic"), TEXT("Toxic"), TEXT("Handle with care."), 2, 1200, true, H(0x9DFF2E), H(0x1A1A1A), H(0x9DFF2E), 8.f));
	L.Add(Outfit(TEXT("o_coral"), TEXT("Coral"), TEXT("Beach-ready, battle-ready."), 2, 1200, true, H(0xFF7A8A), H(0x2EC4B6), H(0xFFFFFF), 2.f));
	L.Add(Outfit(TEXT("o_royal"), TEXT("Royal Guard"), TEXT("Sworn to the crown."), 3, 0, false, H(0x1B2A6B), H(0x0F1838), H(0xFFC83D), 6.f, 0.3f, 1, true));
	L.Add(Outfit(TEXT("o_midnight"), TEXT("Midnight"), TEXT("Strikes at twelve."), 3, 0, true, H(0x3A1060), H(0x1A0830), H(0xFF3DDB), 10.f, 0.f, 10, true));
	L.Add(Outfit(TEXT("o_rust"), TEXT("Rust Bucket"), TEXT("Still runs. Mostly."), 1, 0, false, H(0xA0522D), H(0x5C3317), H(0xFFB070), 1.f, 0.7f, 15, false));
	L.Add(Outfit(TEXT("o_ember"), TEXT("Ember Knight"), TEXT("Forged in the storm."), 3, 0, false, H(0x5A0E0E), H(0x2A0606), H(0xFF6A00), 14.f, 0.4f, 20, true));
	L.Add(Outfit(TEXT("o_skycaptain"), TEXT("Sky Captain"), TEXT("Commands the clouds."), 4, 0, true, H(0xF5F8FF), H(0x5AA9FF), H(0xFFD700), 12.f, 0.2f, 35, true));
	// ---- back blings (style: 0 pack, 1 jet, 2 shield, 3 wings, 4 crate, 5 quiver)
	L.Add(Item(ESDCos::BackBling, TEXT("b_rucksack"), TEXT("Rucksack"), TEXT("Room for one more."), 0, 400, 0, H(0x6B5B3A), H(0x3A2E1D)));
	L.Add(Item(ESDCos::BackBling, TEXT("b_jet"), TEXT("Jet Thrusters"), TEXT("Purely decorative. Probably."), 3, 0, 1, H(0x808890), H(0x40C0FF), 8.f, 5, true));
	L.Add(Item(ESDCos::BackBling, TEXT("b_shield"), TEXT("Knight Shield"), TEXT("Has your back."), 2, 600, 2, H(0x2950A0), H(0xFFC83D)));
	L.Add(Item(ESDCos::BackBling, TEXT("b_wings"), TEXT("Feather Wings"), TEXT("Not for flying."), 4, 0, 3, H(0xFFFFFF), H(0xFFD700), 2.f, 25, true));
	L.Add(Item(ESDCos::BackBling, TEXT("b_crate"), TEXT("Supply Crate"), TEXT("Contents unknown."), 1, 0, 4, H(0x8A5A2B), H(0x3A8AFF), 0.f, 8, false));
	L.Add(Item(ESDCos::BackBling, TEXT("b_quiver"), TEXT("Quiver"), TEXT("Arrows not included."), 2, 500, 5, H(0x5A3A1E), H(0xE04040)));
	// ---- pickaxes (style: 0 pick, 1 axe, 2 sledge, 3 scythe)
	L.Add(Item(ESDCos::Pickaxe, TEXT("p_default"), TEXT("Default Pickaxe"), TEXT("Gets the job done."), 0, 0, 0, H(0x4A2E14), H(0x9AA0A8), 0.f, 0, false, true));
	L.Add(Item(ESDCos::Pickaxe, TEXT("p_axe"), TEXT("Lumber Axe"), TEXT("Trees fear it."), 1, 500, 1, H(0x5A3A1E), H(0xC03020)));
	L.Add(Item(ESDCos::Pickaxe, TEXT("p_sledge"), TEXT("Sledge"), TEXT("Subtle as a brick."), 2, 800, 2, H(0x2A2A2A), H(0x707880)));
	L.Add(Item(ESDCos::Pickaxe, TEXT("p_neon"), TEXT("Neon Pick"), TEXT("Harvests in style."), 3, 0, 0, H(0x151520), H(0xFF2BD6), 10.f, 12, true));
	L.Add(Item(ESDCos::Pickaxe, TEXT("p_reaper"), TEXT("Reaper"), TEXT("A grim harvest."), 4, 1500, 3, H(0x1A1A1A), H(0xB0B8C0), 0.f));
	L.Add(Item(ESDCos::Pickaxe, TEXT("p_ice"), TEXT("Ice Breaker"), TEXT("Cold steel."), 1, 0, 0, H(0xDDEEFF), H(0x60C8FF), 2.f, 4, false));
	// ---- gliders (style: 0 dome, 1 delta, 2 parasol)
	L.Add(Item(ESDCos::Glider, TEXT("g_default"), TEXT("Default Glider"), TEXT("Standard issue."), 0, 0, 0, H(0x2F7DE1), H(0xFFFFFF), 0.f, 0, false, true));
	L.Add(Item(ESDCos::Glider, TEXT("g_sunset"), TEXT("Sunset Wing"), TEXT("Chase the horizon."), 2, 800, 1, H(0xFF7A1A), H(0x7A2BD6)));
	L.Add(Item(ESDCos::Glider, TEXT("g_parasol"), TEXT("Parasol"), TEXT("Arrive in style."), 1, 0, 2, H(0xFFFFFF), H(0xE0405A), 0.f, 18, false));
	L.Add(Item(ESDCos::Glider, TEXT("g_thunder"), TEXT("Thunderbird"), TEXT("Strikes from above."), 3, 0, 1, H(0xFFD21A), H(0x1A1A1A), 3.f, 15, true));
	L.Add(Item(ESDCos::Glider, TEXT("g_candy"), TEXT("Candy Sail"), TEXT("Sweet descent."), 1, 500, 0, H(0xFF8AD0), H(0xFFFFFF)));
	L.Add(Item(ESDCos::Glider, TEXT("g_royal"), TEXT("Royal Kite"), TEXT("Fit for a monarch."), 4, 0, 1, H(0x1B2A6B), H(0xFFC83D), 4.f, 30, true));
	// ---- emotes
	L.Add(Emote(TEXT("e_box"), TEXT("Shadow Boxing"), TEXT("Float like a glider."), 0, 0, TEXT("Unarmed/Attack/MM_Attack_02.MM_Attack_02"), 1.f, 0, false, true));
	L.Add(Emote(TEXT("e_draw"), TEXT("Quick Draw"), TEXT("Fastest hands on the island."), 0, 0, TEXT("Pistol/MM_Pistol_Equip.MM_Pistol_Equip"), 1.f, 0, false, true));
	L.Add(Emote(TEXT("e_flop"), TEXT("The Flop"), TEXT("Dramatic exit."), 1, 300, TEXT("Death/MM_Death_Back_01.MM_Death_Back_01"), 0.9f, 3, false));
	L.Add(Emote(TEXT("e_power"), TEXT("Power Up"), TEXT("Charging..."), 2, 500, TEXT("Unarmed/Attack/MM_ChargedAttack.MM_ChargedAttack"), 1.f));
	L.Add(Emote(TEXT("e_leap"), TEXT("Leap of Faith"), TEXT("No ramp needed."), 1, 300, TEXT("Unarmed/Jump/MM_WallJump.MM_WallJump"), 0.8f));
	L.Add(Emote(TEXT("e_combo"), TEXT("Combo Breaker"), TEXT("C-c-c-combo!"), 2, 500, TEXT("Unarmed/Attack/MM_Attack_03.MM_Attack_03"), 1.f));
	L.Add(Emote(TEXT("e_swagger"), TEXT("Reload Swagger"), TEXT("Always be reloading."), 3, 0, TEXT("Rifle/MM_Rifle_Reload.MM_Rifle_Reload"), 1.f, 7, true));
	return L;
}

const FSDCosmetic* SDCos::Find(FName Id)
{
	for (const FSDCosmetic& C : All()) if (C.Id == Id) return &C;
	return nullptr;
}

const TCHAR* SDCos::TypeName(ESDCos T)
{
	switch (T)
	{
	case ESDCos::Outfit: return TEXT("OUTFIT");
	case ESDCos::BackBling: return TEXT("BACK BLING");
	case ESDCos::Pickaxe: return TEXT("HARVESTING TOOL");
	case ESDCos::Glider: return TEXT("GLIDER");
	case ESDCos::Emote: return TEXT("EMOTE");
	default: return TEXT("");
	}
}

TArray<const FSDCosmetic*> SDCos::OfType(ESDCos T)
{
	TArray<const FSDCosmetic*> R;
	for (const FSDCosmetic& C : All()) if (C.Type == T) R.Add(&C);
	return R;
}

const FSDCosmetic* SDCos::Random(ESDCos T, FRandomStream& R)
{
	TArray<const FSDCosmetic*> L = OfType(T);
	return L.Num() ? L[R.RandRange(0, L.Num() - 1)] : nullptr;
}

FName SDCos::PassReward(int32 Tier, bool bPremium, int32& OutCoins)
{
	OutCoins = 0;
	for (const FSDCosmetic& C : All())
	{
		if (C.PassTier == Tier && C.bPremium == bPremium) return C.Id;
	}
	if (bPremium) OutCoins = Tier % 2 == 0 ? 100 : 50;
	else if (Tier % 3 == 2) OutCoins = 100;
	return NAME_None;
}

// ------------------------------------------------------------------ looks

void SDCos::ApplyOutfit(USkeletalMeshComponent* Mesh, const FSDCosmetic* O)
{
	if (!Mesh || !O) return;
	const TCHAR* MeshPath = O->bQuinn ? TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple")
	                                  : TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
	if (USkeletalMesh* SM = LoadObject<USkeletalMesh>(nullptr, MeshPath, nullptr, LOAD_NoWarn))
	{
		if (Mesh->GetSkeletalMeshAsset() != SM) Mesh->SetSkeletalMeshAsset(SM);
	}
	static TMap<FString, UMaterialInstanceDynamic*> Cache;
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, O->bQuinn
		? TEXT("/Game/Characters/Mannequins/Materials/Quinn/MI_Quinn_01.MI_Quinn_01")
		: TEXT("/Game/Characters/Mannequins/Materials/Manny/MI_Manny_01_New.MI_Manny_01_New"), nullptr, LOAD_NoWarn);
	if (!Base) return;
	for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
	{
		const FString Key = FString::Printf(TEXT("%s_%d"), *O->Id.ToString(), Slot);
		UMaterialInstanceDynamic* MID = nullptr;
		if (UMaterialInstanceDynamic** F = Cache.Find(Key)) MID = IsValid(*F) ? *F : nullptr;
		if (!MID)
		{
			MID = UMaterialInstanceDynamic::Create(Base, GetTransientPackage());
			MID->AddToRoot();
			MID->SetVectorParameterValue(TEXT("Paint Tint"), Slot == 0 ? O->Legs : O->Torso);
			MID->SetVectorParameterValue(TEXT("LogoTint"), O->Accent);
			MID->SetScalarParameterValue(TEXT("EmissivePower"), O->Glow);
			MID->SetScalarParameterValue(TEXT("MetalPaintMetallic"), O->Metal);
			MID->SetScalarParameterValue(TEXT("MetalPaintRoughness"), O->Metal > 0.5f ? 0.25f : 0.5f);
			Cache.Add(Key, MID);
		}
		Mesh->SetMaterial(Slot, MID);
	}
}

namespace
{
	using namespace SDAssets;
	UStaticMeshComponent* Part(TArray<UStaticMeshComponent*>& Out, UStaticMeshComponent* C)
	{
		if (C) { C->SetCastShadow(true); Out.Add(C); }
		return C;
	}
	UMaterialInterface* Mat(const FLinearColor& C, float Glow = 0.f, float Metal = 0.f, float Rough = 0.5f)
	{
		return Solid(C, Glow, Rough, Metal);
	}
}

TArray<UStaticMeshComponent*> SDCos::BuildBackBling(AActor* Ow, USceneComponent* P, const FSDCosmetic* C)
{
	TArray<UStaticMeshComponent*> Out;
	if (!C) return Out;
	// local frame: +X = the character's forward, +Z up; items sit behind (negative X)
	switch (C->Style)
	{
	case 0: // backpack
		Part(Out, AddBox(Ow, P, FVector(-14, 0, 0), FVector(18, 34, 42), FRotator::ZeroRotator, Mat(C->A), false));
		Part(Out, AddBox(Ow, P, FVector(-24, 0, 6), FVector(6, 30, 22), FRotator::ZeroRotator, Mat(C->B), false));
		Part(Out, AddMesh(Ow, P, Cylinder(), FVector(-14, 0, 24), FVector(0.16f, 0.16f, 0.34f), FRotator(0, 0, 90), Mat(C->B), false));
		break;
	case 1: // jet thrusters
		for (int32 s = -1; s <= 1; s += 2)
		{
			Part(Out, AddMesh(Ow, P, Cylinder(), FVector(-16, s * 11.f, 0), FVector(0.18f, 0.18f, 0.48f), FRotator::ZeroRotator, Mat(C->A, 0.f, 0.8f, 0.3f), false));
			Part(Out, AddMesh(Ow, P, Cone(), FVector(-16, s * 11.f, -30), FVector(0.16f, 0.16f, 0.16f), FRotator(180, 0, 0), Mat(C->B, C->Glow), false));
		}
		Part(Out, AddBox(Ow, P, FVector(-10, 0, 6), FVector(8, 30, 20), FRotator::ZeroRotator, Mat(C->A, 0.f, 0.8f, 0.3f), false));
		break;
	case 2: // round shield
		Part(Out, AddMesh(Ow, P, Cylinder(), FVector(-14, 0, -2), FVector(0.9f, 0.9f, 0.05f), FRotator(90, 0, 0), Mat(C->A, 0.f, 0.4f), false));
		Part(Out, AddMesh(Ow, P, Cylinder(), FVector(-16, 0, -2), FVector(0.45f, 0.45f, 0.05f), FRotator(90, 0, 0), Mat(C->B, 0.5f, 0.9f, 0.3f), false));
		break;
	case 3: // wings
		for (int32 s = -1; s <= 1; s += 2)
		{
			Part(Out, AddMesh(Ow, P, Sphere(), FVector(-16, s * 34.f, 10), FVector(0.12f, 0.6f, 0.95f), FRotator(0, s * -25.f, s * 25.f), Mat(C->A, C->Glow), false));
			Part(Out, AddMesh(Ow, P, Sphere(), FVector(-18, s * 22.f, -12), FVector(0.1f, 0.4f, 0.6f), FRotator(0, s * -20.f, s * 40.f), Mat(C->B, C->Glow, 0.8f, 0.3f), false));
		}
		break;
	case 4: // supply crate
		Part(Out, AddBox(Ow, P, FVector(-18, 0, 0), FVector(26, 32, 32), FRotator::ZeroRotator, Mat(C->A), false));
		Part(Out, AddBox(Ow, P, FVector(-18, 0, 0), FVector(27, 33, 6), FRotator::ZeroRotator, Mat(C->B, 1.f), false));
		break;
	default: // quiver
		Part(Out, AddMesh(Ow, P, Cylinder(), FVector(-14, 0, 4), FVector(0.22f, 0.22f, 0.55f), FRotator(0, 0, 25), Mat(C->A), false));
		for (int32 i = 0; i < 4; ++i)
		{
			Part(Out, AddMesh(Ow, P, Cylinder(), FVector(-14, -12.f + i * 4.f, 36), FVector(0.03f, 0.03f, 0.4f), FRotator(0, 0, 25), Mat(FLinearColor(0.6f, 0.45f, 0.25f)), false));
			Part(Out, AddMesh(Ow, P, Cone(), FVector(-14, -20.f + i * 4.f, 54), FVector(0.06f, 0.02f, 0.08f), FRotator(0, 0, 25), Mat(C->B), false));
		}
		break;
	}
	return Out;
}

TArray<UStaticMeshComponent*> SDCos::BuildPickaxe(AActor* Ow, USceneComponent* P, const FSDCosmetic* C)
{
	TArray<UStaticMeshComponent*> Out;
	if (!C) return Out;
	// handle along +Z from the grip, the working end at the top pointing along +X
	const float Len = C->Style == 3 ? 1.35f : 0.95f;
	Part(Out, AddMesh(Ow, P, Cylinder(), FVector(0, 0, Len * 100.f * 0.5f - 22.f), FVector(0.07f, 0.07f, Len), FRotator::ZeroRotator, Mat(C->A, 0.f, 0.f, 0.8f), false));
	const float Top = Len * 100.f - 26.f;
	switch (C->Style)
	{
	case 0: // pick
		Part(Out, AddBox(Ow, P, FVector(6, 0, Top), FVector(56, 11, 11), FRotator::ZeroRotator, Mat(C->B, C->Glow, 0.8f, 0.3f), false));
		Part(Out, AddMesh(Ow, P, Cone(), FVector(40, 0, Top - 4), FVector(0.12f, 0.12f, 0.2f), FRotator(-110, 0, 0), Mat(C->B, C->Glow, 0.8f, 0.3f), false));
		Part(Out, AddMesh(Ow, P, Cone(), FVector(-28, 0, Top - 4), FVector(0.12f, 0.12f, 0.2f), FRotator(110, 0, 0), Mat(C->B, C->Glow, 0.8f, 0.3f), false));
		break;
	case 1: // axe
		Part(Out, AddBox(Ow, P, FVector(16, 0, Top - 6), FVector(30, 6, 34), FRotator::ZeroRotator, Mat(C->B, C->Glow, 0.6f, 0.35f), false));
		Part(Out, AddBox(Ow, P, FVector(2, 0, Top - 6), FVector(10, 12, 14), FRotator::ZeroRotator, Mat(FLinearColor(0.2f, 0.2f, 0.22f), 0.f, 0.8f), false));
		break;
	case 2: // sledge
		Part(Out, AddBox(Ow, P, FVector(0, 0, Top), FVector(40, 26, 26), FRotator::ZeroRotator, Mat(C->B, C->Glow, 0.7f, 0.4f), false));
		break;
	default: // scythe
		for (int32 i = 0; i < 4; ++i)
		{
			const float A = i * 18.f;
			Part(Out, AddBox(Ow, P, FVector(10.f + i * 15.f, 0, Top + 4.f - i * i * 3.f), FVector(20, 4, 9.f - i), FRotator(-A, 0, 0), Mat(C->B, C->Glow, 0.9f, 0.25f), false));
		}
		break;
	}
	return Out;
}

TArray<UStaticMeshComponent*> SDCos::BuildGlider(AActor* Ow, USceneComponent* P, const FSDCosmetic* C)
{
	TArray<UStaticMeshComponent*> Out;
	if (!C) return Out;
	UMaterialInterface* Strut = Mat(FLinearColor(0.08f, 0.08f, 0.09f));
	switch (C->Style)
	{
	case 0: // dome canopy with a stripe
		Part(Out, AddMesh(Ow, P, Sphere(), FVector::ZeroVector, FVector(1.3f, 2.4f, 0.28f), FRotator::ZeroRotator, Mat(C->A, C->Glow), false));
		Part(Out, AddMesh(Ow, P, Sphere(), FVector(0, 0, 1.f), FVector(0.45f, 2.42f, 0.27f), FRotator::ZeroRotator, Mat(C->B, C->Glow), false));
		break;
	case 1: // delta wing
		Part(Out, AddBox(Ow, P, FVector(0, -60, 0), FVector(70, 120, 6), FRotator(0, 25, -6), Mat(C->A, C->Glow, 0.3f, 0.4f), false));
		Part(Out, AddBox(Ow, P, FVector(0, 60, 0), FVector(70, 120, 6), FRotator(0, -25, 6), Mat(C->A, C->Glow, 0.3f, 0.4f), false));
		Part(Out, AddBox(Ow, P, FVector(10, 0, 3), FVector(110, 16, 10), FRotator::ZeroRotator, Mat(C->B, C->Glow, 0.6f, 0.3f), false));
		break;
	default: // parasol
		Part(Out, AddMesh(Ow, P, Sphere(), FVector(0, 0, 10), FVector(2.2f, 2.2f, 0.6f), FRotator::ZeroRotator, Mat(C->A), false));
		for (int32 i = 0; i < 4; ++i)
		{
			Part(Out, AddBox(Ow, P, FVector(0, 0, 20), FVector(222, 14, 4), FRotator(0, i * 45.f, 0), Mat(C->B), false));
		}
		Part(Out, AddMesh(Ow, P, Cylinder(), FVector(0, 0, -50), FVector(0.04f, 0.04f, 1.2f), FRotator::ZeroRotator, Strut, false));
		return Out;
	}
	Part(Out, AddBox(Ow, P, FVector(0, -40, -55), FVector(3, 3, 110), FRotator(0, 0, -20), Strut, false));
	Part(Out, AddBox(Ow, P, FVector(0, 40, -55), FVector(3, 3, 110), FRotator(0, 0, 20), Strut, false));
	return Out;
}

UAnimSequence* SDCos::EmoteAnim(const FSDCosmetic* C)
{
	if (!C || !C->Anim) return nullptr;
	return LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Characters/Mannequins/Anims/%s"), C->Anim), nullptr, LOAD_NoWarn);
}
