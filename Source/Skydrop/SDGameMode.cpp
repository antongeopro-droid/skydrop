#include "SDGameMode.h"
#include "SDWorld.h"
#include "SDMatchActors.h"
#include "SDCharacter.h"
#include "SDBuildPiece.h"
#include "SDBotController.h"
#include "SDPlayerController.h"
#include "SDHUD.h"
#include "SDGrass.h"
#include "SDLobby.h"
#include "SDSave.h"
#include "SDCosmetics.h"
#include "SDSettings.h"
#include "SDInterior.h"
#include "EngineUtils.h"
#include "SDAudio.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformMisc.h"

namespace
{
	const TCHAR* BotNames[] = {
		TEXT("Ashby"), TEXT("Bramble"), TEXT("Cinder"), TEXT("Dax"), TEXT("Ember"), TEXT("Flint"), TEXT("Gale"), TEXT("Hollow"),
		TEXT("Ivy"), TEXT("Jinx"), TEXT("Kestrel"), TEXT("Lumen"), TEXT("Moss"), TEXT("Nova"), TEXT("Onyx"), TEXT("Pike"),
		TEXT("Quill"), TEXT("Rook"), TEXT("Sable"), TEXT("Thistle"), TEXT("Umber"), TEXT("Vale"), TEXT("Wren"), TEXT("Yarrow"), TEXT("Zephyr")
	};
}

ASDGameMode::ASDGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ASDPlayerController::StaticClass();
	HUDClass = ASDHUD::StaticClass();
}

void ASDGameMode::BeginPlay()
{
	Super::BeginPlay();
	Save = USDSave::Load();
	USDSettings::Get()->Apply(true);
	if (FParse::Param(FCommandLine::Get(), TEXT("sdautotest")))
	{
		Phase = ESDPhase::Loading;
		GetWorldTimerManager().SetTimer(StartHandle, this, &ASDGameMode::StartMatch, 0.2f, false);
	}
	else
	{
		GetWorldTimerManager().SetTimer(StartHandle, this, &ASDGameMode::EnterLobby, 0.1f, false);
	}
}

void ASDGameMode::EnterLobby()
{
	Phase = ESDPhase::Lobby;
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Lobby = GetWorld()->SpawnActor<ASDLobby>(ASDLobby::StaticClass(), FTransform(FVector(0, 0, 0)), SP);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetViewTarget(Lobby);
		PC->SetShowMouseCursor(true);
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
	}
	SDAudio::StartMusic(this);
}

void ASDGameMode::LobbyAction(FName A)
{
	if (Phase != ESDPhase::Lobby || !Save) return;
	const FString S = A.ToString();
	if (S.StartsWith(TEXT("set:")) || S.StartsWith(TEXT("slide:")) || S.StartsWith(TEXT("bind:")) || S.StartsWith(TEXT("stab:")) || S == TEXT("sreset"))
	{
		MenuAction(A);
		return;
	}
	if (S == TEXT("ready")) SDAudio::Play(this, ESDSfx::Ready);
	else if (S == TEXT("confirm")) SDAudio::Play(this, ESDSfx::Buy);
	else SDAudio::Play(this, ESDSfx::Click);
	auto Say = [this](const FString& Msg) { Toast = Msg; ToastTime = 2.5f; };
	// purchase popup swallows everything else
	if (!PendingBuy.IsNone())
	{
		if (S == TEXT("confirm"))
		{
			const bool bPass = PendingBuy == TEXT("pass");
			const bool bOk = bPass ? Save->BuyPass() : Save->Buy(PendingBuy);
			if (bOk)
			{
				if (bPass) Say(TEXT("Battle Pass unlocked!"));
				else if (const FSDCosmetic* C = SDCos::Find(PendingBuy)) Say(FString::Printf(TEXT("%s added to your Locker"), C->Name));
				Save->Write();
			}
			PendingBuy = NAME_None;
		}
		else if (S == TEXT("cancel")) PendingBuy = NAME_None;
		return;
	}
	if (S.StartsWith(TEXT("tab")))
	{
		LobbyTab = FMath::Clamp(FCString::Atoi(*S.Mid(3)), 0, 5);
		BindingAction = -1;
		bSelecting = false;
		if (Lobby)
		{
			Lobby->SetFocus(LobbyTab == 0 ? 0 : 1);
			Lobby->ClearPreview(Save->Outfit, Save->BackBling);
		}
		ShopView = NAME_None;
	}
	else if (S == TEXT("modeprev")) Save->Mode = (Save->Mode + SDProfile::NumModes - 1) % SDProfile::NumModes;
	else if (S == TEXT("modenext")) Save->Mode = (Save->Mode + 1) % SDProfile::NumModes;
	else if (S == TEXT("back") || S == TEXT("shopback"))
	{
		if (!ShopView.IsNone()) { ShopView = NAME_None; if (Lobby) Lobby->ClearPreview(Save->Outfit, Save->BackBling); }
		else if (bSelecting) { bSelecting = false; if (Lobby) Lobby->ClearPreview(Save->Outfit, Save->BackBling); }
		else if (LobbyTab != 0) LobbyAction(TEXT("tab0"));
	}
	else if (S.StartsWith(TEXT("slot")))
	{
		LockerSlot = FMath::Clamp(FCString::Atoi(*S.Mid(4)), 0, 9);
		bSelecting = true;
		const ESDCos Type = LockerSlot < 4 ? (ESDCos)LockerSlot : ESDCos::Emote;
		const FName Cur = Save->Equipped(Type, FMath::Max(0, LockerSlot - 4));
		if (Lobby && !Cur.IsNone()) Lobby->Preview(Cur, Save->Outfit, Save->BackBling);
	}
	else if (S.StartsWith(TEXT("pick:")))
	{
		const FName Id(*S.Mid(5));
		const ESDCos Type = LockerSlot < 4 ? (ESDCos)LockerSlot : ESDCos::Emote;
		Save->Equip(Type, Id, LockerSlot - 4);
		if (Lobby)
		{
			if (!Id.IsNone()) Lobby->Preview(Id, Save->Outfit, Save->BackBling);
			else Lobby->ClearPreview(Save->Outfit, Save->BackBling);
		}
	}
	else if (S == TEXT("saveexit")) { bSelecting = false; if (Lobby) Lobby->ClearPreview(Save->Outfit, Save->BackBling); }
	else if (S == TEXT("bpprev")) PassPage = FMath::Max(0, PassPage - 1);
	else if (S == TEXT("bpnext")) PassPage = FMath::Min(FMath::DivideAndRoundUp(SDCos::PassTiers, 7) - 1, PassPage + 1);
	else if (S.StartsWith(TEXT("bpcell:")))
	{
		TArray<FString> P;
		S.ParseIntoArray(P, TEXT(":"));
		if (P.Num() == 3)
		{
			PassSelTier = FCString::Atoi(*P[1]);
			PassSelRow = FCString::Atoi(*P[2]);
			int32 Coins = 0;
			const FSDCosmetic* C = SDCos::Find(SDCos::PassReward(PassSelTier, PassSelRow == 1, Coins));
			if (Lobby)
			{
				if (C) Lobby->Preview(C->Id, Save->Outfit, Save->BackBling);
				else Lobby->ClearPreview(Save->Outfit, Save->BackBling);
			}
		}
	}
	else if (S == TEXT("buypass")) PendingBuy = TEXT("pass");
	else if (S.StartsWith(TEXT("shop:")))
	{
		// open the item page: preview it on the stage, buy from there
		ShopView = FName(*S.Mid(5));
		if (Lobby) Lobby->Preview(ShopView, Save->Outfit, Save->BackBling);
	}
	else if (S == TEXT("shopbuy") && !ShopView.IsNone())
	{
		if (Save->Owns(ShopView)) Say(TEXT("Already in your Locker"));
		else PendingBuy = ShopView;
	}
	else if (S == TEXT("ready"))
	{
		Save->Write();
		LobbyTab = 0;
		ShopView = NAME_None;
		if (Lobby) { Lobby->SetFocus(0); Lobby->ClearPreview(Save->Outfit, Save->BackBling); }
		Phase = ESDPhase::Matchmaking;
		MatchmakingT = 0.f;
		FakeQueue = 1;
		return;
	}
	Save->Write();
}
void ASDGameMode::MenuAction(FName A)
{
	const FString S = A.ToString();
	USDSettings* St = USDSettings::Get();
	const bool bSlide = S.StartsWith(TEXT("slide:"));
	if (!bSlide) SDAudio::Play(this, ESDSfx::Click);
	if (S.StartsWith(TEXT("stab:"))) { SettingsTab = FMath::Clamp(FCString::Atoi(*S.Mid(5)), 0, 3); BindingAction = -1; return; }
	if (S.StartsWith(TEXT("bind:"))) { BindingAction = FCString::Atoi(*S.Mid(5)); return; }
	if (S == TEXT("resume")) { SetMenu(false); return; }
	if (S == TEXT("msettings")) { MenuView = 1; return; }
	if (S == TEXT("mback")) { MenuView = 0; BindingAction = -1; return; }
	if (S == TEXT("leave")) { SetMenu(false); RestartMatch(); return; }
	bool bDisplay = false;
	if (S == TEXT("sreset"))
	{
		St->ResetSection(SettingsTab);
		bDisplay = SettingsTab == 0;
	}
	else if (S.StartsWith(TEXT("set:")) || bSlide)
	{
		// set:<id>:<+1|-1>   slide:<id>:<0..1>
		TArray<FString> P;
		S.ParseIntoArray(P, TEXT(":"));
		const FSDOption* O = P.Num() >= 3 ? USDSettings::FindOption(FName(*P[1])) : nullptr;
		if (!O) return;
		const float Arg = FCString::Atof(*P[2]);
		float V = O->Get(St);
		if (bSlide)
		{
			V = FMath::Lerp(O->Min, O->Max, FMath::Clamp(Arg, 0.f, 1.f));
			V = FMath::GridSnap(V - O->Min, O->Step) + O->Min;
		}
		else if (O->bSlider) V = FMath::Clamp(V + Arg * O->Step, O->Min, O->Max);
		else V = (float)((FMath::RoundToInt(V) + (Arg < 0 ? -1 : 1) + O->Choices.Num()) % O->Choices.Num());
		if (FMath::IsNearlyEqual(V, O->Get(St))) return;
		O->Set(St, V);
		bDisplay = O->bDisplay;
		if (bDisplay) St->bDisplaySet = true;
	}
	else return;
	St->Apply(bDisplay);
	St->Write();
	SDAudio::RefreshVolumes();
}

void ASDGameMode::SetMenu(bool bOpen)
{
	bMenuOpen = bOpen;
	MenuView = 0;
	BindingAction = -1;
	UGameplayStatics::SetGamePaused(this, bOpen);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetShowMouseCursor(bOpen);
		if (bOpen) { FInputModeGameAndUI M; M.SetHideCursorDuringCapture(false); PC->SetInputMode(M); }
		else PC->SetInputMode(FInputModeGameOnly());
	}
}

void ASDGameMode::StartMatch()
{
	UWorld* W = GetWorld();
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (Save) NumBots = SDProfile::Mode(Save->Mode).Bots;
	if (Lobby) { Lobby->Destroy(); Lobby = nullptr; }
	SDAudio::StopMusic();
	if (APlayerController* PC = W->GetFirstPlayerController())
	{
		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeGameOnly());
	}

	Island = W->SpawnActor<ASDWorld>(ASDWorld::StaticClass(), FTransform::Identity, SP);
	Island->Generate(FMath::Rand(), this);

	if (ASDGrass* Grass = W->SpawnActor<ASDGrass>(ASDGrass::StaticClass(), FTransform::Identity, SP)) Grass->Island = Island;

	Storm = W->SpawnActor<ASDStorm>(ASDStorm::StaticClass(), FTransform::Identity, SP);
	Storm->Init(Island->IslandRadius);

	Bus = W->SpawnActor<ASDBus>(ASDBus::StaticClass(), FTransform::Identity, SP);
	Bus->Init(Island->IslandRadius);

	const FTransform BusT(Bus->GetActorLocation());
	if (APlayerController* PC = W->GetFirstPlayerController())
	{
		PlayerChar = W->SpawnActorDeferred<ASDCharacter>(ASDCharacter::StaticClass(), BusT, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		PlayerChar->DisplayName = TEXT("You");
		PlayerChar->Tint = FLinearColor(0.1f, 0.55f, 1.f);
		PlayerChar->FinishSpawning(BusT);
		if (Save) PlayerChar->ApplyCosmetics(Save->Outfit, Save->BackBling, Save->Pickaxe, Save->Glider, Save->Emotes);
		PC->Possess(PlayerChar);
		PC->SetControlRotation(Bus->GetActorRotation());
		PlayerChar->EnterBus();
		Alive.Add(PlayerChar);
		InBus.Add(PlayerChar);
	}
	for (int32 i = 0; i < NumBots; ++i)
	{
		ASDCharacter* B = W->SpawnActorDeferred<ASDCharacter>(ASDCharacter::StaticClass(), BusT, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		B->bIsBot = true;
		B->DisplayName = BotNames[i % UE_ARRAY_COUNT(BotNames)];
		B->Tint = FLinearColor::MakeRandomColor();
		B->FinishSpawning(BusT);
		{
			FRandomStream CR(FMath::Rand());
			const FSDCosmetic* O = SDCos::Random(ESDCos::Outfit, CR);
			const FSDCosmetic* Bb = CR.FRand() < 0.6f ? SDCos::Random(ESDCos::BackBling, CR) : nullptr;
			const FSDCosmetic* Px = SDCos::Random(ESDCos::Pickaxe, CR);
			const FSDCosmetic* Gl = SDCos::Random(ESDCos::Glider, CR);
			B->ApplyCosmetics(O ? O->Id : NAME_None, Bb ? Bb->Id : NAME_None, Px ? Px->Id : NAME_None, Gl ? Gl->Id : NAME_None, TArray<FName>());
		}
		B->SpawnDefaultController();
		B->EnterBus();
		Alive.Add(B);
		InBus.Add(B);
	}
	TotalPlayers = Alive.Num();
	Phase = ESDPhase::Bus;
}

void ASDGameMode::Tick(float Dt)
{
	Super::Tick(Dt);
	ToastTime -= Dt;
	static const bool bLobbyShots = FParse::Param(FCommandLine::Get(), TEXT("sdlobbyshot"));
	if (bLobbyShots && (Phase == ESDPhase::Lobby || Phase == ESDPhase::Matchmaking))
	{
		AutoT += FMath::Min(Dt, 0.05f);
		auto LShot = [this](float At, const TCHAR* Name) { if (AutoT >= At && AutoStep < At) FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("lobby_%s.png"), Name), true, false); };
		auto Do = [this](float At, const TCHAR* Act) { if (AutoT >= At && AutoStep < At) LobbyAction(FName(Act)); };
		LShot(3.f, TEXT("1_lobby"));
		Do(3.5f, TEXT("tab1"));
		Do(3.7f, TEXT("bpcell:1:1"));
		LShot(5.f, TEXT("2_battlepass"));
		Do(5.5f, TEXT("tab2"));
		LShot(6.5f, TEXT("3_challenges"));
		Do(7.f, TEXT("tab3"));
		LShot(8.5f, TEXT("4_locker"));
		Do(9.f, TEXT("slot0"));
		LShot(10.f, TEXT("5_selecting"));
		Do(10.1f, TEXT("saveexit"));
		Do(10.15f, TEXT("slot3"));
		LShot(10.45f, TEXT("5b_glider"));
		Do(10.5f, TEXT("saveexit"));
		Do(10.6f, TEXT("tab4"));
		LShot(12.f, TEXT("6_shop"));
		if (AutoT >= 12.5f && AutoStep < 12.5f)
		{
			TArray<FName> Fe, Da;
			SDProfile::ShopToday(Fe, Da);
			FName Pick = Da.Num() ? Da[0] : NAME_None;
			for (FName D : Da) if (const FSDCosmetic* Cc = SDCos::Find(D)) if (Cc->Type == ESDCos::Pickaxe || Cc->Type == ESDCos::Glider) Pick = D;
			if (!Pick.IsNone()) LobbyAction(FName(*(TEXT("shop:") + Pick.ToString())));
		}
		LShot(13.5f, TEXT("7_item"));
		Do(13.6f, TEXT("shopbuy"));
		LShot(13.9f, TEXT("7b_buy"));
		Do(14.f, TEXT("cancel"));
		Do(14.05f, TEXT("shopback"));
		Do(14.2f, TEXT("tab5"));
		LShot(15.2f, TEXT("9_settings"));
		Do(15.3f, TEXT("stab:1"));
		LShot(15.45f, TEXT("9b_game"));
		Do(15.5f, TEXT("stab:2"));
		LShot(16.2f, TEXT("10_audio"));
		Do(16.5f, TEXT("stab:3"));
		// rebinding check: Move forward -> F, then Move right -> F (forward should swap to D)
		auto Key = [this](float At, const FKey& K, EInputEvent E)
		{
			if (AutoT >= At && AutoStep < At)
				if (APlayerController* P = GetWorld()->GetFirstPlayerController()) P->InputKey(FInputKeyEventArgs::CreateSimulated(K, E, E == IE_Pressed ? 1.f : 0.f));
		};
		static TArray<FName> SavedKeys;
		if (AutoT >= 16.55f && AutoStep < 16.55f) SavedKeys = USDSettings::Get()->Keys; // keep the player's own bindings
		Do(16.6f, TEXT("bind:0"));
		Key(16.7f, EKeys::F, IE_Pressed);
		Key(16.75f, EKeys::F, IE_Released);
		Do(16.85f, TEXT("bind:3"));
		Key(16.95f, EKeys::F, IE_Pressed);
		Key(17.f, EKeys::F, IE_Released);
		LShot(17.2f, TEXT("11_controls"));
		if (AutoT >= 17.3f && AutoStep < 17.3f)
		{
			const USDSettings* St = USDSettings::Get();
			UE_LOG(LogTemp, Display, TEXT("[sdbind] forward=%s right=%s"), *St->Key(ESDAction::Forward).ToString(), *St->Key(ESDAction::Right).ToString());
		}
		if (AutoT >= 17.4f && AutoStep < 17.4f && SavedKeys.Num()) { USDSettings::Get()->Keys = SavedKeys; USDSettings::Get()->Write(); }
		Do(17.5f, TEXT("stab:0"));
		Do(17.6f, TEXT("tab0"));
		Do(17.7f, TEXT("ready"));
		LShot(19.f, TEXT("8_search"));
		AutoStep = AutoT;
		if (AutoT > 20.f) { FPlatformMisc::RequestExit(false); return; }
	}
	if (Phase == ESDPhase::Matchmaking)
	{
		// short fake queue so the transition feels like matchmaking
		MatchmakingT += Dt;
		const int32 Target = Save ? SDProfile::Mode(Save->Mode).Bots + 1 : 50;
		FakeQueue = FMath::Clamp(FMath::RoundToInt(FMath::Pow(FMath::Clamp(MatchmakingT / 2.6f, 0.f, 1.f), 0.6f) * Target), 1, Target);
		if (MatchmakingT > 3.0f) { Phase = ESDPhase::Loading; LoadingFrames = 0; }
		return;
	}
	if (Phase == ESDPhase::Loading && !Island && !GetWorldTimerManager().IsTimerActive(StartHandle))
	{
		// let the loading screen draw for a couple of frames, then build the island
		if (++LoadingFrames == 3) StartMatch();
		return;
	}
	MatchClock += Dt;
	Feed.RemoveAll([this](const FSDFeed& F) { return MatchClock - F.Time > 7.f; });

	if (Phase == ESDPhase::Bus && Bus)
	{
		// passengers ride behind the airship so the camera frames it from outside
		const FVector Ride = Bus->GetActorLocation() - Bus->GetActorForwardVector() * 1800.f + FVector(0, 0, 900.f);
		for (const TWeakObjectPtr<ASDCharacter>& WC : InBus)
		{
			if (ASDCharacter* C = WC.Get()) C->SetActorLocation(Ride);
		}
		const float P = Bus->Progress();
		TArray<TWeakObjectPtr<ASDCharacter>> Copy = InBus;
		for (const TWeakObjectPtr<ASDCharacter>& WC : Copy)
		{
			ASDCharacter* C = WC.Get();
			if (!C) continue;
			if (ASDBotController* BC = Cast<ASDBotController>(C->GetController()))
			{
				if (Bus->DoorsOpen() && P >= BC->DropFraction) DropFromBus(C);
			}
		}
		if (Bus->DoorsOpen() && P >= 1.f)
		{
			Copy = InBus;
			for (const TWeakObjectPtr<ASDCharacter>& WC : Copy)
			{
				if (ASDCharacter* C = WC.Get()) DropFromBus(C);
			}
			Phase = ESDPhase::Playing;
			Storm->Start();
		}
	}

	if (Phase != ESDPhase::Loading) TickAutoTest(Dt);

	if (bIntegrityDirty)
	{
		IntegrityTimer -= Dt;
		if (IntegrityTimer <= 0.f)
		{
			bIntegrityDirty = false;
			CheckIntegrity();
		}
	}
}

void ASDGameMode::DropFromBus(ASDCharacter* C)
{
	if (!C || !Bus || !Bus->DoorsOpen() || !InBus.Contains(C)) return;
	InBus.Remove(C);
	const FVector Side = Bus->GetActorRightVector() * FMath::FRandRange(-250.f, 250.f);
	C->LeaveBus(Bus->GetActorLocation() - FVector(0, 0, 800) + Side);
}

bool ASDGameMode::IsOccupied(const FSDBuildSlot& S) const
{
	const TWeakObjectPtr<ASDBuildPiece>* P = Pieces.Find(ASDBuildPiece::MakeKey(S));
	return P && P->IsValid();
}

ASDBuildPiece* ASDGameMode::TryPlace(const FSDBuildSlot& S, ESDMat M, ASDCharacter* By, bool bWorldPiece, bool bInstant)
{
	if (IsOccupied(S)) return nullptr;
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASDBuildPiece* P = GetWorld()->SpawnActor<ASDBuildPiece>(ASDBuildPiece::StaticClass(), ASDBuildPiece::SlotTransform(S), SP);
	if (!P) return nullptr;
	P->Setup(S, M, false, bWorldPiece, bInstant);
	if (!bWorldPiece && !P->IsGrounded())
	{
		// must touch the ground or another structure
		bool bSupported = false;
		const FBox Box = P->GetComponentsBoundingBox(true).ExpandBy(14.f);
		for (const TPair<FString, TWeakObjectPtr<ASDBuildPiece>>& KV : Pieces)
		{
			ASDBuildPiece* O = KV.Value.Get();
			if (!O || FMath::Abs(O->Slot.I - S.I) > 1 || FMath::Abs(O->Slot.J - S.J) > 1 || FMath::Abs(O->Slot.L - S.L) > 1) continue;
			if (Box.Intersect(O->GetComponentsBoundingBox(true)))
			{
				bSupported = true;
				break;
			}
		}
		if (!bSupported)
		{
			P->bGhost = true; // skip removal bookkeeping
			P->Destroy();
			return nullptr;
		}
	}
	Pieces.Add(P->Key, P);
	// ramps and cones built by players go straight through furniture, smashing it
	if (!bWorldPiece && (S.Type == ESDBuildType::Ramp || S.Type == ESDBuildType::Cone))
	{
		const FBox B = P->GetComponentsBoundingBox(true).ExpandBy(-6.f);
		TArray<ASDFurniture*> Hit;
		for (TActorIterator<ASDFurniture> It(GetWorld()); It; ++It)
		{
			if (FVector::DistSquared(It->GetActorLocation(), B.GetCenter()) > FMath::Square(800.f)) continue;
			if (B.Intersect(It->GetComponentsBoundingBox(true))) Hit.Add(*It);
		}
		for (ASDFurniture* F : Hit) F->Smash();
	}
	return P;
}

void ASDGameMode::OnPieceRemoved(ASDBuildPiece* P)
{
	if (const TWeakObjectPtr<ASDBuildPiece>* Found = Pieces.Find(P->Key))
	{
		if (Found->Get() == P || !Found->IsValid()) Pieces.Remove(P->Key);
	}
	bIntegrityDirty = true;
	IntegrityTimer = 0.12f;
}

void ASDGameMode::CheckIntegrity()
{
	// flood fill from grounded pieces; anything not reached collapses
	TArray<ASDBuildPiece*> All;
	TMap<FIntPoint, TArray<int32>> Buckets;
	TArray<FBox> Boxes;
	for (const TPair<FString, TWeakObjectPtr<ASDBuildPiece>>& KV : Pieces)
	{
		if (ASDBuildPiece* P = KV.Value.Get())
		{
			if (P->bCollapsing) continue;
			const int32 Idx = All.Add(P);
			Boxes.Add(P->GetComponentsBoundingBox(true).ExpandBy(14.f));
			Buckets.FindOrAdd(FIntPoint(P->Slot.I, P->Slot.J)).Add(Idx);
		}
	}
	TArray<bool> Reached;
	Reached.Init(false, All.Num());
	TArray<int32> Queue;
	for (int32 i = 0; i < All.Num(); ++i)
	{
		if (All[i]->IsGrounded()) { Reached[i] = true; Queue.Add(i); }
	}
	for (int32 q = 0; q < Queue.Num(); ++q)
	{
		const int32 Cur = Queue[q];
		const ASDBuildPiece* P = All[Cur];
		for (int32 di = -1; di <= 1; ++di)
		{
			for (int32 dj = -1; dj <= 1; ++dj)
			{
				const TArray<int32>* B = Buckets.Find(FIntPoint(P->Slot.I + di, P->Slot.J + dj));
				if (!B) continue;
				for (int32 Other : *B)
				{
					if (Reached[Other]) continue;
					if (Boxes[Cur].Intersect(Boxes[Other]))
					{
						Reached[Other] = true;
						Queue.Add(Other);
					}
				}
			}
		}
	}
	for (int32 i = 0; i < All.Num(); ++i)
	{
		if (!Reached[i]) All[i]->Collapse();
	}
}

void ASDGameMode::OnEliminated(ASDCharacter* Victim, ASDCharacter* Killer)
{
	Alive.Remove(Victim);
	InBus.Remove(Victim);
	FSDFeed F;
	F.Time = MatchClock;
	F.bPlayer = Victim == PlayerChar || Killer == PlayerChar;
	if (Killer && Killer != Victim)
	{
		Killer->Kills++;
		const FSDItemStack* It = Killer->ActiveItem();
		const FString With = Killer->Active < 0 ? TEXT("Pickaxe") : It && It->Item == ESDItem::Weapon ? SDData::Weapon(It->Weapon).Name : TEXT("???");
		F.Text = FString::Printf(TEXT("%s eliminated %s (%s)"), *Killer->DisplayName, *Victim->DisplayName, *With);
	}
	else if (Storm && !Storm->IsInside(Victim->GetActorLocation()))
	{
		F.Text = FString::Printf(TEXT("%s was lost in the storm"), *Victim->DisplayName);
	}
	else
	{
		F.Text = FString::Printf(TEXT("%s took a bad fall"), *Victim->DisplayName);
	}
	Feed.Add(F);

	if (Victim == PlayerChar)
	{
		bPlayerOut = true;
		PlayerPlace = Alive.Num() + 1;
		AwardRewards();
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (Killer && Killer != Victim) PC->SetViewTargetWithBlend(Killer, 1.5f);
		}
	}
	if (Alive.Num() <= 1)
	{
		ASDCharacter* Winner = Alive.Num() == 1 ? Alive[0].Get() : nullptr;
		WinnerName = Winner ? Winner->DisplayName : TEXT("Nobody");
		if (Winner && Winner == PlayerChar)
		{
			bPlayerWon = true;
			PlayerPlace = 1;
			AwardRewards();
		}
		Phase = ESDPhase::Over;
	}
}

void ASDGameMode::TickAutoTest(float Dt)
{
	static const bool bAuto = FParse::Param(FCommandLine::Get(), TEXT("sdautotest"));
	if (!bAuto || !PlayerChar) return;
	AutoT += Dt;
	auto Shot = [this](float At, const TCHAR* Name)
	{
		if (AutoT >= At && AutoStep < At)
		{
			FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("sd_%s.png"), Name), true, false);
			UE_LOG(LogTemp, Display, TEXT("[sdtest] shot %s alive=%d air=%d pos=%s hp=%.0f"), Name, Alive.Num(), (int32)PlayerChar->Air,
				*PlayerChar->GetActorLocation().ToCompactString(), PlayerChar->Health);
		}
	};
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PlayerChar->bDead) { PlayerChar->Health = 100.f; PlayerChar->FallStartZ = FMath::Min(PlayerChar->FallStartZ, PlayerChar->GetActorLocation().Z); } // test character can't die
	if (AutoT >= 7.f && AutoStep < 7.f) DropFromBus(PlayerChar);
	if (PC && PlayerChar->Air == ESDAir::Skydive) PC->SetControlRotation(FRotator(-40.f, PC->GetControlRotation().Yaw, 0.f));
	Shot(5.f, TEXT("1_bus"));
	Shot(11.f, TEXT("2_skydive"));
	Shot(17.f, TEXT("3_glide"));
	if (AutoT >= 24.f && AutoStep < 24.f && Island && Island->Pois.Num() > 0 && PC)
	{
		const FSDPoi& P = Island->Pois[0];
		const FVector At(P.Pos.X - 2600.f, P.Pos.Y - 2600.f, P.Z + 400.f);
		PlayerChar->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
		PC->SetControlRotation(FRotator(-12.f, 45.f, 0.f));
		PlayerChar->Air = ESDAir::Ground;
		PlayerChar->Health = 100.f;
	}
	Shot(27.f, TEXT("4_town"));
	if (AutoT >= 29.f && AutoStep < 29.f && PC)
	{
		PlayerChar->Mats[0] = 300;
		PlayerChar->SelectBuild(ESDBuildType::Wall);
		PlayerChar->PlaceBuild(ESDBuildType::Wall);
		PlayerChar->PlaceBuild(ESDBuildType::Ramp);
		PC->SetControlRotation(FRotator(-60.f, 45.f, 0.f));
		PlayerChar->PlaceBuild(ESDBuildType::Floor);
		PC->SetControlRotation(FRotator(-8.f, 225.f, 0.f));
		PlayerChar->BuildType = ESDBuildType::Ramp;
	}
	Shot(31.f, TEXT("5_build"));
	if (AutoT >= 33.f && AutoStep < 33.f && PC)
	{
		FSDItemStack W; W.Item = ESDItem::Weapon; W.Weapon = ESDWeapon::AR; W.Rarity = 3; W.Count = 30;
		PlayerChar->GiveItem(W, true);
		PlayerChar->Ammo[(int32)ESDAmmo::Medium] = 120;
		PlayerChar->SelectSlot(0);
		PC->SetControlRotation(FRotator(-5.f, 225.f, 0.f));
		PlayerChar->PrimaryDown();
	}
	Shot(34.5f, TEXT("6_shoot"));
	if (AutoT >= 36.f && AutoStep < 36.f) PlayerChar->PrimaryUp();
	if (AutoT >= 38.f && AutoStep < 38.f)
	{
		if (ASDPlayerController* SPC = Cast<ASDPlayerController>(PC)) SPC->SetBigMap(true);
	}
	if (ASDPlayerController* SPC = Cast<ASDPlayerController>(PC))
	{
		const ASDHUD* Hud = Cast<ASDHUD>(SPC->GetHUD());
		const FSDPoi* Tgt = Island && Island->Pois.Num() ? &Island->Pois[Island->Pois.Num() > 1 ? 1 : 0] : nullptr;
		FVector2D TS;
		if (AutoT >= 38.2f && AutoStep < 38.2f && Hud && Tgt && Hud->MapWorldToScreen(Tgt->Pos, TS)) SPC->SetMouseLocation(FMath::RoundToInt(TS.X), FMath::RoundToInt(TS.Y));
		if (AutoT >= 38.3f && AutoStep < 38.3f) SPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseScrollUp, IE_Pressed, 1.f));
		if (AutoT >= 38.4f && AutoStep < 38.4f) SPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseScrollUp, IE_Pressed, 1.f));
		if (AutoT >= 38.5f && AutoStep < 38.5f && Hud && Tgt && Hud->MapWorldToScreen(Tgt->Pos + FVector2D(1500.f, 0.f), TS)) SPC->SetMouseLocation(FMath::RoundToInt(TS.X), FMath::RoundToInt(TS.Y));
		if (AutoT >= 38.6f && AutoStep < 38.6f) SPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::RightMouseButton, IE_Pressed, 1.f));
		if (AutoT >= 38.65f && AutoStep < 38.65f) SPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::RightMouseButton, IE_Released, 0.f));
		if (AutoT >= 38.8f && AutoStep < 38.8f)
			UE_LOG(LogTemp, Display, TEXT("[sdmap] zoom=%.2f marker=%d at %s target=%s"), SPC->MapZoom, SPC->bHasMarker ? 1 : 0,
				*SPC->MarkerPos.ToString(), Tgt ? *Tgt->Pos.ToString() : TEXT("-"));
		if (AutoT >= 38.9f && AutoStep < 38.9f && Tgt) SPC->SetMarker(Tgt->Pos + FVector2D(1500.f, 0.f)); // beam check in the aerial shots
	}
	Shot(39.f, TEXT("7_map"));
	if (AutoT >= 40.f && AutoStep < 40.f && PC && Island)
	{
		if (ASDPlayerController* SPC = Cast<ASDPlayerController>(PC)) SPC->SetBigMap(false);
		const FSDPoi& P = Island->Pois[Island->Pois.Num() > 1 ? 1 : 0];
		PlayerChar->EnterBus();
		PlayerChar->SetActorLocation(FVector(P.Pos.X - 14000.f, P.Pos.Y - 14000.f, P.Z + 7000.f));
		PC->SetControlRotation(FRotator(-22.f, 45.f, 0.f));
	}
	Shot(43.f, TEXT("8_aerial"));
	if (AutoT >= 44.f && AutoStep < 44.f)
	{
		PC->SetControlRotation(FRotator(-30.f, 200.f, 0.f));
	}
	Shot(46.f, TEXT("9_aerial2"));
	if (AutoT >= 47.f && AutoStep < 47.f && PC && Island)
	{
		// stand on the lake shore looking across the water
		const FVector2D L = Island->GetLakePos();
		const float Rr = Island->GetLakeRadius();
		FVector2D Spot = L + FVector2D(1.f, 0.f) * Rr * 0.95f;
		for (int32 k = 0; k < 16; ++k)
		{
			const FVector2D Q = L + FVector2D(FMath::Cos(k * PI / 8.f), FMath::Sin(k * PI / 8.f)) * Rr * 0.95f;
			if (Island->HeightAt(Q.X, Q.Y) > 20.f) { Spot = Q; break; }
		}
		PlayerChar->Air = ESDAir::Ground;
		PlayerChar->FallStartZ = -1e6f;
		PlayerChar->SetActorHiddenInGame(false);
		PlayerChar->SetActorEnableCollision(true);
		PlayerChar->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		PlayerChar->SetActorLocation(FVector(Spot.X, Spot.Y, FMath::Max(Island->HeightAt(Spot.X, Spot.Y), 0.f) + 150.f));
		PC->SetControlRotation(FRotator(-18.f, FMath::RadiansToDegrees(FMath::Atan2((L - Spot).Y, (L - Spot).X)), 0.f));
	}
	Shot(50.f, TEXT("10_water"));
	if (AutoT >= 51.f && AutoStep < 51.f && PC && Island)
	{
		FRandomStream RS(7);
		for (int32 k = 0; k < 200; ++k)
		{
			const FVector P = Island->RandomLandPoint(RS, 300.f, 0.7f);
			if (Island->NearPoi(FVector2D(P), 1500.f) || !Island->IsGrassy(P.X, P.Y)) continue;
			PlayerChar->FallStartZ = -1e6f;
			PlayerChar->SetActorLocation(P + FVector(0, 0, 150.f));
			PC->SetControlRotation(FRotator(-6.f, 30.f, 0.f));
			break;
		}
	}
	Shot(56.f, TEXT("11_grass"));
	// pose check: a camera in front of the character
	if (AutoT >= 57.f && AutoStep < 57.f && PC)
	{
		// clean stage: a small platform high in the sky
		for (int32 i = -1; i <= 1; ++i)
			for (int32 j = -1; j <= 1; ++j)
			{
				FSDBuildSlot S; S.Type = ESDBuildType::Floor; S.I = i; S.J = j; S.L = 70;
				TryPlace(S, ESDMat::Stone, nullptr, true, true);
			}
		PlayerChar->FallStartZ = -1e6f;
		PlayerChar->SetActorLocation(FVector(256.f, 256.f, 70 * SDGrid::Level + 100.f));
		PlayerChar->PrimaryUp();
		PlayerChar->SelectSlot(0);
		if (!PlayerChar->Slots[0].IsEmpty()) PlayerChar->Slots[0].Count = 10;
		PC->SetControlRotation(FRotator(0.f, 30.f, 0.f));
		const FVector Fwd = FRotator(0.f, 30.f, 0.f).Vector();
		const FVector Right = FRotator(0.f, 120.f, 0.f).Vector();
		const FVector CamLoc = FVector(256.f, 256.f, 70 * SDGrid::Level + 100.f) + Fwd * 280.f + Right * 150.f + FVector(0, 0, 30.f);
		ACameraActor* Cam = GetWorld()->SpawnActor<ACameraActor>(CamLoc, (FVector(256.f, 256.f, 70 * SDGrid::Level + 120.f) - CamLoc).Rotation());
		if (Cam) PC->SetViewTarget(Cam);
	}
	Shot(58.5f, TEXT("12_pose_aim"));
	if (AutoT >= 59.f && AutoStep < 59.f) PlayerChar->ReloadPressed();
	Shot(60.f, TEXT("13_pose_reload"));
	if (AutoT >= 61.5f && AutoStep < 61.5f && PC) PC->SetControlRotation(FRotator(35.f, 30.f, 0.f));
	Shot(62.5f, TEXT("14_pose_aimup"));
	if (AutoT >= 63.f && AutoStep < 63.f && PC)
	{
		PC->SetControlRotation(FRotator(0.f, 30.f, 0.f));
		PlayerChar->SelectSlot(-1);
		PlayerChar->Cooldown = 0.f;
		PlayerChar->PrimaryDown();
	}
	Shot(63.6f, TEXT("15_pose_swing"));
	if (AutoT >= 64.5f && AutoStep < 64.5f && PC)
	{
		PlayerChar->PrimaryUp();
		FSDItemStack Pump; Pump.Item = ESDItem::Weapon; Pump.Weapon = ESDWeapon::Pump; Pump.Rarity = 2; Pump.Count = 5;
		PlayerChar->Slots[1] = Pump;
		PlayerChar->SelectSlot(1);
		PC->SetControlRotation(FRotator(0.f, 120.f, 0.f)); // side-on to the camera
	}
	Shot(66.f, TEXT("16_pose_side"));
	if (AutoT >= 66.2f && AutoStep < 66.2f && PC) { PC->SetViewTarget(PlayerChar); PC->SetControlRotation(FRotator(-8.f, 120.f, 0.f)); }
	if (AutoT >= 66.2f && AutoT < 67.6f) PlayerChar->Move(FVector2D(0.6f, 1.f));
	Shot(67.4f, TEXT("17_pose_run"));
	// inventory and the pause menu (menu shown without pausing so the test keeps ticking)
	if (ASDPlayerController* SPC = Cast<ASDPlayerController>(PC))
	{
		if (AutoT >= 67.8f && AutoStep < 67.8f) { SPC->SetViewTarget(PlayerChar); SPC->SetInventory(true); }
		// real mouse input: click slot 1, then click slot 3 (should swap)
		auto ClickAt = [&](float At, const TCHAR* Id)
		{
			FVector2D P;
			const ASDHUD* Hud = Cast<ASDHUD>(SPC->GetHUD());
			if (AutoT >= At && AutoStep < At && Hud && Hud->ButtonCenter(FName(Id), P)) { SPC->SetMouseLocation(FMath::RoundToInt(P.X), FMath::RoundToInt(P.Y)); }
			if (AutoT >= At + 0.05f && AutoStep < At + 0.05f) SPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Pressed, 1.f));
			if (AutoT >= At + 0.1f && AutoStep < At + 0.1f) SPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Released, 0.f));
		};
		ClickAt(67.9f, TEXT("inv:slot:0"));
		ClickAt(68.1f, TEXT("inv:slot:2"));
		if (AutoT >= 68.3f && AutoStep < 68.3f)
		{
			float MX = -1, MY = -1; SPC->GetMousePosition(MX, MY);
			UE_LOG(LogTemp, Display, TEXT("[sdinv] sel=%d slot0=%d slot1=%d slot2=%d mouse=%.0f,%.0f"), SPC->InvSel,
				(int32)PlayerChar->Slots[0].Weapon, (int32)PlayerChar->Slots[1].Weapon, (int32)PlayerChar->Slots[2].Weapon, MX, MY);
		}
		if (AutoT >= 68.6f && AutoStep < 68.6f) SPC->SetInventory(false);
		if (AutoT >= 68.8f && AutoStep < 68.8f) { bMenuOpen = true; MenuView = 0; SPC->SetShowMouseCursor(true); }
		if (AutoT >= 69.6f && AutoStep < 69.6f) { MenuView = 1; SettingsTab = 0; }
		if (AutoT >= 70.4f && AutoStep < 70.4f) { SettingsTab = 3; }
	}
	Shot(68.4f, TEXT("18_inventory"));
	Shot(69.4f, TEXT("19_pause"));
	Shot(70.2f, TEXT("20_settings_video"));
	Shot(71.f, TEXT("21_settings_controls"));
	// edits: a small floating build with each kind of edit, then the edit overlay on a wall and a ramp
	static TWeakObjectPtr<ASDBuildPiece> TDoorWall, TRamp;
	if (AutoT >= 71.3f && AutoStep < 71.3f && PC)
	{
		bMenuOpen = false; MenuView = 0; PC->SetShowMouseCursor(false);
		const FVector Pp = PlayerChar->GetActorLocation();
		const int32 I0 = FMath::FloorToInt(Pp.X / SDGrid::Tile), J0 = FMath::FloorToInt(Pp.Y / SDGrid::Tile);
		const int32 Lp = FMath::CeilToInt(Pp.Z / SDGrid::Level) + 3;
		auto Put = [&](ESDBuildType Ty, int32 I, int32 J, int32 Axis, int32 Dir, bool bWorldP) -> ASDBuildPiece*
		{
			FSDBuildSlot S; S.Type = Ty; S.I = I; S.J = J; S.L = Lp; S.Axis = Axis; S.Dir = Dir;
			return TryPlace(S, ESDMat::Wood, PlayerChar, bWorldP, true);
		};
		for (int32 j = -1; j <= 1; ++j) Put(ESDBuildType::Floor, I0, J0 + j, 0, 0, true);
		ASDBuildPiece* FrontFloor = Put(ESDBuildType::Floor, I0 + 1, J0, 0, 0, false);
		Put(ESDBuildType::Floor, I0 + 1, J0 - 1, 0, 0, false);
		Put(ESDBuildType::Floor, I0 + 1, J0 + 1, 0, 0, false);
		ASDBuildPiece* Arch = Put(ESDBuildType::Wall, I0 + 2, J0 - 1, 0, 0, false);
		ASDBuildPiece* Door = Put(ESDBuildType::Wall, I0 + 2, J0, 0, 0, false);
		ASDBuildPiece* Tri = Put(ESDBuildType::Wall, I0 + 2, J0 + 1, 0, 0, false);
		ASDBuildPiece* Ramp = Put(ESDBuildType::Ramp, I0 + 1, J0 - 1, 0, 0, false);
		ASDBuildPiece* Cone = Put(ESDBuildType::Cone, I0 + 1, J0 + 1, 0, 0, false);
		const TArray<int32> NoPath;
		if (Arch) Arch->ApplyEdit(1 | 2 | 4 | 16, NoPath);            // arch
		if (Tri) Tri->ApplyEdit(32 | 128 | 256, NoPath);              // triangle
		if (Cone) Cone->ApplyEdit(1, NoPath);                          // one corner pulled up
		if (FrontFloor) FrontFloor->ApplyEdit(1 << 1, NoPath);         // one quarter cut out
		if (Ramp) { const TArray<int32> L = { 6, 7, 8, 5, 2 }; Ramp->ApplyEdit(64 | 128 | 256 | 32 | 4, L); } // L stairs
		TDoorWall = Door; TRamp = Ramp;
		UE_LOG(LogTemp, Display, TEXT("[sdedit] arch=%d tri=%d cone=%d floor=%d ramp=%d(dir %d) door=%d"),
			Arch ? Arch->Edit : -1, Tri ? Tri->Edit : -1, Cone ? Cone->Edit : -1, FrontFloor ? FrontFloor->Edit : -1,
			Ramp ? Ramp->Edit : -1, Ramp ? Ramp->Slot.Dir : -1, Door ? 1 : 0);
		PlayerChar->SetActorLocation(FVector((I0 + 0.3f) * SDGrid::Tile, (J0 + 0.5f) * SDGrid::Tile, Lp * SDGrid::Level + 100.f));
		PlayerChar->SelectSlot(-1);
		PC->SetViewTarget(PlayerChar);
		PC->SetControlRotation(FRotator(-8.f, 0.f, 0.f));
	}
	Shot(72.6f, TEXT("22_edits"));
	if (AutoT >= 73.f && AutoStep < 73.f && PC && TDoorWall.IsValid())
	{
		PC->SetControlRotation((TDoorWall->GetActorLocation() + FVector(0, 0, 150.f) - PlayerChar->Camera->GetComponentLocation()).Rotation());
		PlayerChar->BeginEdit(TDoorWall.Get());
		PlayerChar->EditSel = SDEditMask::Door;
	}
	Shot(73.8f, TEXT("23_edit_wall"));
	if (AutoT >= 74.2f && AutoStep < 74.2f && PC)
	{
		PlayerChar->EndEdit(true);
		if (TRamp.IsValid())
		{
			PC->SetControlRotation((TRamp->GetActorLocation() - PlayerChar->Camera->GetComponentLocation()).Rotation());
			PlayerChar->BeginEdit(TRamp.Get());
			PlayerChar->EditPath = { 0, 1, 2, 5, 8, 7 };
			for (int32 Tl : PlayerChar->EditPath) PlayerChar->EditSel |= 1 << Tl;
		}
	}
	Shot(75.f, TEXT("24_edit_ramp"));
	if (AutoT >= 75.4f && AutoStep < 75.4f)
	{
		PlayerChar->EndEdit(false);
		PC->SetControlRotation(FRotator(-8.f, 0.f, 0.f));
	}
	Shot(76.2f, TEXT("25_after_edit"));
	// sniper scope: equip a sniper and hold aim (real right-mouse input)
	if (AutoT >= 76.4f && AutoStep < 76.4f && PC)
	{
		FSDItemStack Sn; Sn.Item = ESDItem::Weapon; Sn.Weapon = ESDWeapon::Sniper; Sn.Rarity = 3; Sn.Count = 1;
		PlayerChar->Slots[4] = Sn;
		PlayerChar->SelectSlot(4);
		PC->SetControlRotation(FRotator(-4.f, 200.f, 0.f));
	}
	if (AutoT >= 76.9f && AutoStep < 76.9f && PC) PC->InputKey(FInputKeyEventArgs::CreateSimulated(USDSettings::Get()->Key(ESDAction::Aim), IE_Pressed, 1.f));
	Shot(77.0f, TEXT("26_scope_enter"));
	Shot(77.6f, TEXT("27_scope"));
	if (AutoT >= 77.9f && AutoStep < 77.9f && PC) PC->InputKey(FInputKeyEventArgs::CreateSimulated(USDSettings::Get()->Key(ESDAction::Aim), IE_Released, 0.f));
	// towns from a fixed camera: a city, a suburb and a farm/industrial town
	{
		static TWeakObjectPtr<ACameraActor> TownCam;
		auto TownShot = [&](float At, ESDPoiType Want, ESDPoiType Alt, const TCHAR* Name, float Yaw)
		{
			if (AutoT >= At && AutoStep < At && Island && PC)
			{
				const FSDPoi* Found = nullptr;
				for (const FSDPoi& P : Island->Pois) if (P.Type == Want) { Found = &P; break; }
				if (!Found) for (const FSDPoi& P : Island->Pois) if (P.Type == Alt) { Found = &P; break; }
				if (!Found) return;
				const FVector Ctr(Found->Pos.X, Found->Pos.Y, Found->Z + 200.f);
				const FVector Dir = FRotator(0.f, Yaw, 0.f).Vector();
				const FVector Eye = Ctr - Dir * (Found->Radius * 1.05f) + FVector(0, 0, Want == ESDPoiType::City ? 2600.f : 900.f);
				if (!TownCam.IsValid()) TownCam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
				TownCam->SetActorLocationAndRotation(Eye, (Ctr - Eye).Rotation());
				TownCam->GetCameraComponent()->SetFieldOfView(70.f);
				PC->SetViewTarget(TownCam.Get());
				PC->InputKey(FInputKeyEventArgs::CreateSimulated(USDSettings::Get()->Key(ESDAction::Aim), IE_Released, 0.f));
				PlayerChar->SetAds(false);
				if (AHUD* Hd = PC->GetHUD()) Hd->bShowHUD = false; // clean shots of the town
			}
			Shot(At + 1.2f, Name);
		};
		TownShot(78.4f, ESDPoiType::City, ESDPoiType::Industrial, TEXT("28_town_city"), 35.f);
		TownShot(80.0f, ESDPoiType::Suburb, ESDPoiType::Harbor, TEXT("29_town_suburb"), 120.f);
		TownShot(81.6f, ESDPoiType::Farm, ESDPoiType::Industrial, TEXT("30_town_farm"), 210.f);
	}
	// interiors: look into a bedroom, a living room and an office
	{
		static TWeakObjectPtr<ACameraActor> RoomCam;
		auto RoomShot = [&](float At, ESDFurn Want, const TCHAR* Name)
		{
			if (AutoT >= At && AutoStep < At && PC)
			{
				ASDFurniture* Best = nullptr;
				for (TActorIterator<ASDFurniture> It(GetWorld()); It; ++It) if (It->Kind == Want) { Best = *It; break; }
				if (!Best) return;
				const FVector Fwd = Best->GetActorForwardVector();
				const FVector Side = FVector::CrossProduct(FVector::UpVector, Fwd);
				const FVector Eye = Best->GetActorLocation() + Fwd * 330.f + Side * 120.f + FVector(0, 0, 175.f);
				if (!RoomCam.IsValid()) RoomCam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
				RoomCam->SetActorLocationAndRotation(Eye, (Best->GetActorLocation() + FVector(0, 0, 60.f) - Eye).Rotation());
				RoomCam->GetCameraComponent()->SetFieldOfView(90.f);
				PC->SetViewTarget(RoomCam.Get());
			}
			Shot(At + 1.2f, Name);
		};
		RoomShot(83.4f, ESDFurn::Bed, TEXT("31_room_bedroom"));
		RoomShot(85.0f, ESDFurn::Couch, TEXT("32_room_living"));
		RoomShot(86.6f, ESDFurn::FilingCabinet, TEXT("33_room_office"));
		// two pickaxe hits break furniture; a ramp built through furniture smashes it
		if (AutoT >= 88.2f && AutoStep < 88.2f)
		{
			ASDFurniture* A = nullptr; ASDFurniture* B = nullptr;
			for (TActorIterator<ASDFurniture> It(GetWorld()); It; ++It) { if (!A) A = *It; else if (FVector::Dist(It->GetActorLocation(), A->GetActorLocation()) > 2000.f) { B = *It; break; } }
			if (A)
			{
				TWeakObjectPtr<ASDFurniture> WA = A;
				FHitResult Hh; Hh.ImpactPoint = A->GetActorLocation(); Hh.ImpactNormal = FVector::UpVector;
				A->Harvest(PlayerChar, Hh);
				const bool bAfter1 = WA.IsValid();
				if (WA.IsValid()) WA->Harvest(PlayerChar, Hh);
				UE_LOG(LogTemp, Display, TEXT("[sdfurn] pickaxe: alive after 1 hit=%d, after 2 hits=%d"), bAfter1 ? 1 : 0, WA.IsValid() ? 1 : 0);
			}
			if (B)
			{
				TWeakObjectPtr<ASDFurniture> WB = B;
				const FVector L = B->GetActorLocation();
				FSDBuildSlot S; S.Type = ESDBuildType::Ramp; S.I = FMath::FloorToInt(L.X / SDGrid::Tile); S.J = FMath::FloorToInt(L.Y / SDGrid::Tile); S.L = FMath::RoundToInt(L.Z / SDGrid::Level);
				ASDBuildPiece* Rp = TryPlace(S, ESDMat::Wood, PlayerChar, false, true);
				UE_LOG(LogTemp, Display, TEXT("[sdfurn] ramp placed=%d furniture still there=%d"), Rp ? 1 : 0, WB.IsValid() ? 1 : 0);
			}
			int32 Count = 0;
			for (TActorIterator<ASDFurniture> It(GetWorld()); It; ++It) ++Count;
			UE_LOG(LogTemp, Display, TEXT("[sdfurn] furniture in the world: %d"), Count);
		}
	}
	if (AutoT >= 88.6f && AutoStep < 88.6f)
	{
		UE_LOG(LogTemp, Display, TEXT("[sdtest] done alive=%d pieces=%d phase=%d"), Alive.Num(), Pieces.Num(), (int32)Phase);
		FPlatformMisc::RequestExit(false);
	}
	AutoStep = AutoT;
}

void ASDGameMode::AwardRewards()
{
	if (bRewarded || !Save || !PlayerChar) return;
	bRewarded = true;
	RewardLines.Reset();
	const int32 Kills = PlayerChar->Kills;
	int32 XP = 100;
	RewardLines.Add(TEXT("Match played  +100 XP"));
	if (Kills > 0) { XP += Kills * 150; RewardLines.Add(FString::Printf(TEXT("%d elimination%s  +%d XP"), Kills, Kills > 1 ? TEXT("s") : TEXT(""), Kills * 150)); }
	const int32 Placement = FMath::Max(0, TotalPlayers - PlayerPlace) * 12;
	if (Placement > 0) { XP += Placement; RewardLines.Add(FString::Printf(TEXT("Placement #%d  +%d XP"), PlayerPlace, Placement)); }
	if (bPlayerWon) { XP += 1000; RewardLines.Add(TEXT("Victory  +1000 XP")); }

	Save->Matches++;
	Save->Kills += Kills;
	if (bPlayerWon) Save->Wins++;
	if (PlayerPlace <= 10) Save->Top10++;
	const int32 Adds[SDProfile::NumChallenges] = { 1, Kills, PlayerChests, PlayerPlace <= 10 ? 1 : 0, PlayerHarvest };
	for (int32 i = 0; i < SDProfile::NumChallenges; ++i)
	{
		const SDProfile::FChallenge& C = SDProfile::Challenge(i);
		if (Save->ChallengeDone[i]) continue;
		Save->Challenge[i] = FMath::Min(C.Goal, Save->Challenge[i] + Adds[i]);
		if (Save->Challenge[i] >= C.Goal)
		{
			Save->ChallengeDone[i] = true;
			XP += C.Reward;
			RewardLines.Add(FString::Printf(TEXT("Challenge: %s  +%d XP"), C.Text, C.Reward));
		}
	}
	// a fresh set once all are complete
	bool bAll = true;
	for (bool D : Save->ChallengeDone) bAll &= D;
	if (bAll) { for (int32 i = 0; i < SDProfile::NumChallenges; ++i) { Save->Challenge[i] = 0; Save->ChallengeDone[i] = false; } }

	const int32 OldLevel = Save->Level();
	Save->XP += XP;
	if (Save->Level() > OldLevel) RewardLines.Add(FString::Printf(TEXT("LEVEL UP!  Level %d  (Battle Pass tier %d)"), Save->Level(), Save->Tier()));
	const int32 CoinGain = 50 + Kills * 20 + (PlayerPlace <= 10 ? 50 : 0) + (bPlayerWon ? 250 : 0);
	Save->Coins += CoinGain;
	RewardLines.Add(FString::Printf(TEXT("+%d coins"), CoinGain));
	RewardLines.Append(Save->ProcessPass());
	Save->Write();
}

void ASDGameMode::RestartMatch()
{
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}
