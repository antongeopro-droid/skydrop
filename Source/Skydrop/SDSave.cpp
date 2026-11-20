#include "SDSave.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/DateTime.h"

namespace
{
	const TCHAR* ProfileSlot = TEXT("SkydropProfile");
}

USDSave* USDSave::Load()
{
	USDSave* S = Cast<USDSave>(UGameplayStatics::LoadGameFromSlot(ProfileSlot, 0));
	if (!S) S = Cast<USDSave>(UGameplayStatics::CreateSaveGameObject(USDSave::StaticClass()));
	S->Challenge.SetNum(SDProfile::NumChallenges);
	S->ChallengeDone.SetNum(SDProfile::NumChallenges);
	S->Mode = FMath::Clamp(S->Mode, 0, SDProfile::NumModes - 1);
	for (const FSDCosmetic& C : SDCos::All())
	{
		if (C.bDefault) S->Owned.AddUnique(C.Id);
	}
	if (S->Emotes.Num() != 6)
	{
		S->Emotes.SetNum(6);
		S->Emotes[0] = TEXT("e_box");
		S->Emotes[1] = TEXT("e_draw");
	}
	if (!S->Owns(S->Outfit)) S->Outfit = TEXT("o_recruit");
	if (!S->Owns(S->Pickaxe)) S->Pickaxe = TEXT("p_default");
	if (!S->Owns(S->Glider)) S->Glider = TEXT("g_default");
	S->ProcessPass();
	return S;
}

void USDSave::Write()
{
	UGameplayStatics::SaveGameToSlot(this, ProfileSlot, 0);
}

bool USDSave::Owns(FName Id) const
{
	return Id == NAME_None || Owned.Contains(Id);
}

FName USDSave::Equipped(ESDCos Type, int32 EmoteSlot) const
{
	switch (Type)
	{
	case ESDCos::Outfit: return Outfit;
	case ESDCos::BackBling: return BackBling;
	case ESDCos::Pickaxe: return Pickaxe;
	case ESDCos::Glider: return Glider;
	case ESDCos::Emote: return Emotes.IsValidIndex(EmoteSlot) ? Emotes[EmoteSlot] : NAME_None;
	default: return NAME_None;
	}
}

void USDSave::Equip(ESDCos Type, FName Id, int32 EmoteSlot)
{
	if (!Owns(Id)) return;
	switch (Type)
	{
	case ESDCos::Outfit: if (Id != NAME_None) Outfit = Id; break;
	case ESDCos::BackBling: BackBling = Id; break;
	case ESDCos::Pickaxe: if (Id != NAME_None) Pickaxe = Id; break;
	case ESDCos::Glider: if (Id != NAME_None) Glider = Id; break;
	case ESDCos::Emote: if (Emotes.IsValidIndex(EmoteSlot)) Emotes[EmoteSlot] = Id; break;
	default: break;
	}
}

bool USDSave::Buy(FName Id)
{
	const FSDCosmetic* C = SDCos::Find(Id);
	if (!C || C->Price <= 0 || Owns(Id) || Coins < C->Price) return false;
	Coins -= C->Price;
	Owned.Add(Id);
	return true;
}

bool USDSave::BuyPass()
{
	if (bPassOwned || Coins < SDCos::PassPrice) return false;
	Coins -= SDCos::PassPrice;
	bPassOwned = true;
	ProcessPass();
	return true;
}

TArray<FString> USDSave::ProcessPass()
{
	TArray<FString> Lines;
	const int32 T = Tier();
	auto Grant = [&](int32 Tr, bool bPrem)
	{
		int32 C = 0;
		const FName Id = SDCos::PassReward(Tr, bPrem, C);
		if (Id != NAME_None && !Owned.Contains(Id))
		{
			Owned.Add(Id);
			if (const FSDCosmetic* Cos = SDCos::Find(Id)) Lines.Add(FString::Printf(TEXT("Unlocked %s: %s"), SDCos::TypeName(Cos->Type), Cos->Name));
		}
		if (C > 0)
		{
			Coins += C;
			Lines.Add(FString::Printf(TEXT("Battle Pass tier %d: +%d coins"), Tr, C));
		}
	};
	for (int32 i = PassClaimedFree + 1; i <= T; ++i) Grant(i, false);
	PassClaimedFree = FMath::Max(PassClaimedFree, T);
	if (bPassOwned)
	{
		for (int32 i = PassClaimedPremium + 1; i <= T; ++i) Grant(i, true);
		PassClaimedPremium = FMath::Max(PassClaimedPremium, T);
	}
	return Lines;
}

const SDProfile::FChallenge& SDProfile::Challenge(int32 I)
{
	static const FChallenge C[NumChallenges] = {
		{ TEXT("Play matches"), 3, 500 },
		{ TEXT("Eliminate opponents"), 5, 500 },
		{ TEXT("Search chests"), 7, 500 },
		{ TEXT("Place top 10"), 2, 500 },
		{ TEXT("Harvest materials"), 500, 500 },
	};
	return C[FMath::Clamp(I, 0, NumChallenges - 1)];
}

const SDProfile::FMode& SDProfile::Mode(int32 I)
{
	static const FMode M[NumModes] = {
		{ TEXT("SKIRMISH"), TEXT("25 players"), 24 },
		{ TEXT("SOLO"), TEXT("50 players"), 49 },
		{ TEXT("MEGA DROP"), TEXT("80 players"), 79 },
	};
	return M[FMath::Clamp(I, 0, NumModes - 1)];
}

void SDProfile::ShopToday(TArray<FName>& Featured, TArray<FName>& Daily)
{
	Featured.Reset();
	Daily.Reset();
	const FDateTime Now = FDateTime::UtcNow();
	FRandomStream R(Now.GetYear() * 1000 + Now.GetDayOfYear());
	TArray<const FSDCosmetic*> Big, Small;
	for (const FSDCosmetic& C : SDCos::All())
	{
		if (C.Price <= 0) continue;
		if (C.Type == ESDCos::Outfit && C.Rarity >= 2) Big.Add(&C);
		else Small.Add(&C);
	}
	auto Pick = [&R](TArray<const FSDCosmetic*>& From, int32 N, TArray<FName>& To)
	{
		for (int32 i = 0; i < N && From.Num() > 0; ++i)
		{
			const int32 K = R.RandRange(0, From.Num() - 1);
			To.Add(From[K]->Id);
			From.RemoveAt(K);
		}
	};
	Pick(Big, 2, Featured);
	// third featured: a top-rarity item of any kind
	Small.Sort([](const FSDCosmetic& A, const FSDCosmetic& B) { return A.Rarity > B.Rarity; });
	if (Small.Num()) { Featured.Add(Small[R.RandRange(0, FMath::Min(2, Small.Num() - 1))]->Id); Small.Remove(SDCos::Find(Featured.Last())); }
	Small.Append(Big);
	Pick(Small, 6, Daily);
}

FTimespan SDProfile::TimeToReset()
{
	const FDateTime Now = FDateTime::UtcNow();
	return (Now.GetDate() + FTimespan::FromDays(1)) - Now;
}

int32 SDProfile::SeasonDaysLeft()
{
	const FDateTime Now = FDateTime::UtcNow();
	const int32 Days = FDateTime::DaysInMonth(Now.GetYear(), Now.GetMonth());
	return Days - Now.GetDay() + 1 + (Now.GetMonth() % 2 == 1 ? Days : 0);
}

void SDProfile::ApplySkin(USkeletalMeshComponent* Mesh, int32 Skin)
{
	const TArray<const FSDCosmetic*> O = SDCos::OfType(ESDCos::Outfit);
	if (O.Num()) SDCos::ApplyOutfit(Mesh, O[FMath::Abs(Skin) % O.Num()]);
}
