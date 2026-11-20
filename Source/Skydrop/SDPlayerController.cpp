#include "SDPlayerController.h"
#include "SDCharacter.h"
#include "SDGameMode.h"
#include "SDHUD.h"
#include "SDSettings.h"
#include "SDLoot.h"
#include "SDAudio.h"
#include "InputCoreTypes.h"
#include "SDWorld.h"
#include "SDAssets.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

ASDPlayerController::ASDPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	bShouldPerformFullTickWhenPaused = true;
}

void ASDPlayerController::BeginPlay()
{
	Super::BeginPlay();
	SetTickableWhenPaused(true);
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = -85.f;
		PlayerCameraManager->ViewPitchMax = 85.f;
	}
}

bool ASDPlayerController::Pressed(ESDAction A) const { return WasInputKeyJustPressed(USDSettings::Get()->Key(A)); }
bool ASDPlayerController::Released(ESDAction A) const { return WasInputKeyJustReleased(USDSettings::Get()->Key(A)); }
bool ASDPlayerController::Down(ESDAction A) const { return IsInputKeyDown(USDSettings::Get()->Key(A)); }

bool ASDPlayerController::CaptureBinding(ASDGameMode* GM)
{
	if (!GM || GM->BindingAction < 0) return false;
	if (WasInputKeyJustPressed(EKeys::Escape)) { GM->BindingAction = -1; return true; }
	// a click on another button (another key box, a section tab, Back...) is a UI click, not a binding
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		float MX = 0.f, MY = 0.f;
		const ASDHUD* Hud = Cast<ASDHUD>(GetHUD());
		if (Hud && GetMousePosition(MX, MY))
		{
			const FName Hit = Hud->LobbyHit(FVector2D(MX, MY));
			if (!Hit.IsNone() && Hit != FName(*FString::Printf(TEXT("bind:%d"), GM->BindingAction)))
			{
				GM->BindingAction = -1;
				return false; // let HandleClick run this frame
			}
		}
	}
	// only real keyboard keys and mouse buttons; skip "Any Key" and virtual/gamepad/axis keys
	static TArray<FKey> All;
	if (All.Num() == 0)
	{
		TArray<FKey> Keys;
		EKeys::GetAllKeys(Keys);
		for (const FKey& K : Keys)
		{
			if (!K.IsValid() || K == EKeys::AnyKey || K == EKeys::Escape) continue;
			if (K.IsAxis1D() || K.IsAxis2D() || K.IsAxis3D() || K.IsTouch() || K.IsGamepadKey() || K.IsDeprecated()) continue;
			if (K == EKeys::MouseScrollUp || K == EKeys::MouseScrollDown || K == EKeys::MouseWheelAxis) continue;
			if (K.GetFName().ToString().StartsWith(TEXT("Virtual_"))) continue;
			if (K.IsVirtual() || K.IsModifierKey() && K.IsMouseButton()) continue;
			All.Add(K);
		}
	}
	for (const FKey& K : All)
	{
		if (WasInputKeyJustPressed(K))
		{
			USDSettings* S = USDSettings::Get();
			S->Bind((ESDAction)GM->BindingAction, K);
			S->Write();
			GM->BindingAction = -1;
			SDAudio::Play(this, ESDSfx::Click);
			return true;
		}
	}
	return true; // swallow input while waiting
}

bool ASDPlayerController::HandleClick(ASDGameMode* GM)
{
	const ASDHUD* Hud = Cast<ASDHUD>(GetHUD());
	if (!Hud || !GM) return false;
	float MX = 0.f, MY = 0.f;
	if (!GetMousePosition(MX, MY)) return false;
	const bool bClick = WasInputKeyJustPressed(EKeys::LeftMouseButton);
	// sliders follow the mouse while the button is held
	if (!bClick)
	{
		if (!IsInputKeyDown(EKeys::LeftMouseButton)) { DragId = NAME_None; return false; }
		if (DragId.IsNone()) return false;
		const FName Hit = Hud->LobbyHit(FVector2D(MX, MY), DragId);
		if (!Hit.IsNone()) GM->MenuAction(Hit);
		return true;
	}
	const FName Hit = Hud->LobbyHit(FVector2D(MX, MY));
	if (Hit.IsNone()) return false;
	const FString S = Hit.ToString();
	if (S.StartsWith(TEXT("slide:")))
	{
		TArray<FString> P;
		S.ParseIntoArray(P, TEXT(":"));
		DragId = P.Num() >= 2 ? FName(*FString::Printf(TEXT("slide:%s"), *P[1])) : NAME_None;
	}
	if (S.StartsWith(TEXT("inv:"))) InventoryAction(S);
	else if (GM->Phase == ESDPhase::Lobby) GM->LobbyAction(Hit);
	else GM->MenuAction(Hit);
	return true;
}

void ASDPlayerController::PlayerTick(float Dt)
{
	Super::PlayerTick(Dt);
	ASDGameMode* GM = GetWorld()->GetAuthGameMode<ASDGameMode>();
	if (Dt > 0.f) SmoothFps = FMath::Lerp(SmoothFps, 1.f / FMath::Max(Dt, 0.0001f), 0.05f);
	USDSettings* Set = USDSettings::Get();

	if (CaptureBinding(GM)) return;

	if (GM && GM->Phase == ESDPhase::Lobby)
	{
		HandleClick(GM);
		if (WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::SpaceBar)) GM->LobbyAction(TEXT("ready"));
		const FKey Num[6] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six };
		for (int32 i = 0; i < 6; ++i) if (WasInputKeyJustPressed(Num[i])) GM->LobbyAction(FName(*FString::Printf(TEXT("tab%d"), i)));
		if (WasInputKeyJustPressed(EKeys::Escape) || WasInputKeyJustPressed(EKeys::BackSpace))
		{
			GM->LobbyAction(GM->PendingBuy.IsNone() ? TEXT("back") : TEXT("cancel"));
		}
		if (GM->LobbyTab == 1 && WasInputKeyJustPressed(EKeys::Left)) GM->LobbyAction(TEXT("bpprev"));
		if (GM->LobbyTab == 1 && WasInputKeyJustPressed(EKeys::Right)) GM->LobbyAction(TEXT("bpnext"));
		if (GM->LobbyTab != 5)
		{
			if (WasInputKeyJustPressed(EKeys::Q)) GM->LobbyAction(TEXT("modeprev"));
			if (WasInputKeyJustPressed(EKeys::E)) GM->LobbyAction(TEXT("modenext"));
		}
		return;
	}
	if (GM && GM->Phase == ESDPhase::Matchmaking) return;

	// pause menu
	if (GM && WasInputKeyJustPressed(EKeys::Escape))
	{
		if (bInventory) SetInventory(false);
		else if (bBigMap) SetBigMap(false);
		else if (GM->bMenuOpen && GM->MenuView == 1) GM->MenuAction(TEXT("mback"));
		else if (GM->Phase == ESDPhase::Bus || GM->Phase == ESDPhase::Playing) GM->SetMenu(!GM->bMenuOpen);
		return;
	}
	if (GM && GM->bMenuOpen)
	{
		HandleClick(GM);
		return;
	}

	if (Pressed(ESDAction::Map)) SetBigMap(!bBigMap);
	if (Pressed(ESDAction::Help)) { Set->bShowHelp = !Set->bShowHelp; Set->Write(); }
	if (WasInputKeyJustPressed(EKeys::LeftBracket)) { Set->Sensitivity = FMath::Max(0.1f, Set->Sensitivity - 0.1f); Set->Write(); }
	if (WasInputKeyJustPressed(EKeys::RightBracket)) { Set->Sensitivity = FMath::Min(4.f, Set->Sensitivity + 0.1f); Set->Write(); }
	if (GM && (GM->Phase == ESDPhase::Over || GM->bPlayerOut) && WasInputKeyJustPressed(EKeys::Enter))
	{
		GM->RestartMatch();
		return;
	}

	ASDCharacter* C = Cast<ASDCharacter>(GetPawn());
	if (bBigMap) TickBigMap();
	if (!C || C->bDead) { if (bInventory) SetInventory(false); return; }
	if (bBigMap)
	{
		// the mouse belongs to the map; you can still run while it is open
		FVector2D In(0.f, 0.f);
		if (Down(ESDAction::Forward)) In.Y += 1.f;
		if (Down(ESDAction::Back)) In.Y -= 1.f;
		if (Down(ESDAction::Right)) In.X += 1.f;
		if (Down(ESDAction::Left)) In.X -= 1.f;
		C->Move(In);
		if (Pressed(ESDAction::Jump)) C->JumpPressed();
		return;
	}

	// inventory: mouse belongs to the screen, movement keeps working
	if (Pressed(ESDAction::Inventory) && C->Air != ESDAir::Bus) SetInventory(!bInventory);
	if (bInventory)
	{
		InventoryMouse();
		if (WasInputKeyJustPressed(EKeys::X)) InventoryAction(TEXT("inv:drop"));
		if (WasInputKeyJustPressed(EKeys::Z)) InventoryAction(TEXT("inv:split"));
		FVector2D In(0.f, 0.f);
		if (Down(ESDAction::Forward)) In.Y += 1.f;
		if (Down(ESDAction::Back)) In.Y -= 1.f;
		if (Down(ESDAction::Right)) In.X += 1.f;
		if (Down(ESDAction::Left)) In.X -= 1.f;
		C->Move(In);
		return;
	}

	// look
	float MX = 0.f, MY = 0.f;
	GetInputMouseDelta(MX, MY);
	const float S = Set->Sensitivity * (C->bAds ? Set->AdsSensitivity : 1.f);
	FRotator R = GetControlRotation();
	R.Yaw += MX * S;
	R.Pitch = FMath::Clamp(FRotator::NormalizeAxis(R.Pitch + MY * S * (Set->bInvertY ? -1.f : 1.f)), -85.f, 85.f);
	SetControlRotation(R);

	// move
	FVector2D In(0.f, 0.f);
	if (Down(ESDAction::Forward)) In.Y += 1.f;
	if (Down(ESDAction::Back)) In.Y -= 1.f;
	if (Down(ESDAction::Right)) In.X += 1.f;
	if (Down(ESDAction::Left)) In.X -= 1.f;
	C->Move(In);

	if (Pressed(ESDAction::Jump)) C->JumpPressed();
	if (Released(ESDAction::Jump)) C->StopJumping();

	// fire / place
	if (Pressed(ESDAction::Fire)) C->PrimaryDown();
	if (Released(ESDAction::Fire)) C->PrimaryUp();

	// aim, change material while building, or reset the piece while editing
	if (C->IsEditing())
	{
		if (Pressed(ESDAction::Aim)) C->ResetEdit();
	}
	else if (C->Mode == ESDMode::Build)
	{
		if (Pressed(ESDAction::Aim)) C->CycleMat();
	}
	else
	{
		C->SetAds(Down(ESDAction::Aim));
	}

	if (Pressed(ESDAction::Reload)) C->ReloadPressed();
	if (Pressed(ESDAction::Build)) C->ToggleBuild();
	if (WasInputKeyJustPressed(EKeys::F1) || Pressed(ESDAction::Wall)) C->SelectBuild(ESDBuildType::Wall);
	if (WasInputKeyJustPressed(EKeys::F2) || Pressed(ESDAction::Floor)) C->SelectBuild(ESDBuildType::Floor);
	if (WasInputKeyJustPressed(EKeys::F3) || Pressed(ESDAction::Ramp)) C->SelectBuild(ESDBuildType::Ramp);
	if (WasInputKeyJustPressed(EKeys::F4) || Pressed(ESDAction::Cone)) C->SelectBuild(ESDBuildType::Cone);
	if (Pressed(ESDAction::Edit)) C->EditPressed();
	// edit on release: holding Edit keeps the grid up, letting go confirms
	else if (Released(ESDAction::Edit) && C->IsEditing() && Set->EditConfirm == 1) C->EndEdit(true);
	if (Pressed(ESDAction::Interact)) C->InteractPressed();
	if (Pressed(ESDAction::Crouch) || WasInputKeyJustPressed(EKeys::C)) C->CrouchToggle();

	// emotes: tap for the first emote, hold and press 1-6 to pick one
	C->bEmoteWheel = Down(ESDAction::Emote);
	if (C->bEmoteWheel)
	{
		const FKey Num[6] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six };
		for (int32 i = 0; i < 6; ++i) if (WasInputKeyJustPressed(Num[i])) { C->PlayEmote(i); bEmotePicked = true; }
		return;
	}
	if (Released(ESDAction::Emote))
	{
		if (!bEmotePicked) C->PlayEmote(0);
		bEmotePicked = false;
	}

	if (Pressed(ESDAction::Pickaxe)) C->SelectSlot(-1);
	if (Pressed(ESDAction::Slot1)) C->SelectSlot(0);
	if (Pressed(ESDAction::Slot2)) C->SelectSlot(1);
	if (Pressed(ESDAction::Slot3)) C->SelectSlot(2);
	if (Pressed(ESDAction::Slot4)) C->SelectSlot(3);
	if (Pressed(ESDAction::Slot5)) C->SelectSlot(4);
	if (WasInputKeyJustPressed(EKeys::MouseScrollDown)) C->ScrollSlot(1);
	if (WasInputKeyJustPressed(EKeys::MouseScrollUp)) C->ScrollSlot(-1);
}

// ------------------------------------------------------------------ inventory

void ASDPlayerController::SetInventory(bool bOpen)
{
	bInventory = bOpen;
	ASDCharacter* C = Cast<ASDCharacter>(GetPawn());
	InvSel = -1; // nothing picked until the player clicks
	DragFrom = -1;
	if (C) { C->PrimaryUp(); C->SetAds(false); }
	SetShowMouseCursor(bOpen);
	if (bOpen)
	{
		FInputModeGameAndUI M;
		M.SetHideCursorDuringCapture(false);
		SetInputMode(M);
	}
	else SetInputMode(FInputModeGameOnly());
	SDAudio::Play(this, ESDSfx::Click);
}

void ASDPlayerController::InventoryAction(const FString& A)
{
	ASDCharacter* C = Cast<ASDCharacter>(GetPawn());
	if (!C) return;
	TArray<FString> P;
	A.ParseIntoArray(P, TEXT(":"));
	if (P.Num() < 2) return;
	const FString& Cmd = P[1];
	if (Cmd == TEXT("close")) { SetInventory(false); return; }

	if (Cmd == TEXT("slot") || Cmd == TEXT("ammo") || Cmd == TEXT("mat"))
	{
		const int32 N = P.Num() >= 3 ? FCString::Atoi(*P[2]) : 0;
		const int32 Id = Cmd == TEXT("slot") ? N : Cmd == TEXT("ammo") ? 10 + N : 20 + N;
		// clicking a second equipment slot swaps the two
		if (Id < 10 && InvSel >= 0 && InvSel < 10 && InvSel != Id)
		{
			Swap(C->Slots[InvSel], C->Slots[Id]);
			if (C->Active == InvSel) C->Active = Id;
			else if (C->Active == Id) C->Active = InvSel;
			C->RefreshHeld();
			SDAudio::Play(this, ESDSfx::Pickup);
			InvSel = Id;
			return;
		}
		InvSel = Id;
		SDAudio::Play(this, ESDSfx::Click);
		return;
	}

	if (Cmd == TEXT("drop") || Cmd == TEXT("split"))
	{
		if (InvSel < 0) return;
		const bool bSplit = Cmd == TEXT("split");
		const FVector At = C->GetActorLocation() + C->GetActorForwardVector() * 90.f;
		FSDItemStack Out;
		if (InvSel < 10)
		{
			FSDItemStack& S = C->Slots[InvSel];
			if (S.IsEmpty()) return;
			if (bSplit)
			{
				if (S.Item == ESDItem::Weapon || S.Count < 2) return;
				Out = S;
				Out.Count = S.Count / 2;
				S.Count -= Out.Count;
			}
			else
			{
				Out = S;
				S = FSDItemStack();
				if (C->Active == InvSel) { C->Active = -1; }
				C->RefreshHeld();
			}
		}
		else if (InvSel < 20)
		{
			const int32 T = InvSel - 10;
			if (C->Ammo[T] <= 0) return;
			Out.Item = ESDItem::Ammo; Out.Sub = T;
			Out.Count = bSplit ? FMath::Max(1, C->Ammo[T] / 2) : C->Ammo[T];
			C->Ammo[T] -= Out.Count;
		}
		else
		{
			const int32 T = InvSel - 20;
			if (C->Mats[T] <= 0) return;
			Out.Item = ESDItem::Mats; Out.Sub = T;
			Out.Count = bSplit ? FMath::Max(1, C->Mats[T] / 2) : C->Mats[T];
			C->Mats[T] -= Out.Count;
		}
		if (ASDPickup* Pk = SDLoot::Spawn(GetWorld(), Out, At, true)) Pk->DroppedBy = C;
		SDAudio::Play(this, ESDSfx::Pickup);
	}
}

int32 ASDPlayerController::InvIdOf(const FString& Hit)
{
	TArray<FString> P;
	Hit.ParseIntoArray(P, TEXT(":"));
	if (P.Num() < 3 || P[0] != TEXT("inv")) return -1;
	const int32 N = FCString::Atoi(*P[2]);
	if (P[1] == TEXT("slot")) return N;
	if (P[1] == TEXT("ammo")) return 10 + N;
	if (P[1] == TEXT("mat")) return 20 + N;
	return -1;
}

void ASDPlayerController::InventoryMouse()
{
	ASDCharacter* C = Cast<ASDCharacter>(GetPawn());
	const ASDHUD* Hud = Cast<ASDHUD>(GetHUD());
	float MX = 0.f, MY = 0.f;
	if (!C || !Hud || !GetMousePosition(MX, MY)) return;
	const FVector2D M(MX, MY);
	auto SwapSlots = [&](int32 A, int32 B)
	{
		Swap(C->Slots[A], C->Slots[B]);
		if (C->Active == A) C->Active = B;
		else if (C->Active == B) C->Active = A;
		C->RefreshHeld();
		SDAudio::Play(this, ESDSfx::Pickup);
	};

	if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		const FName Hit = Hud->LobbyHit(M);
		const FString S = Hit.ToString();
		const int32 Id = InvIdOf(S);
		if (Id < 0)
		{
			if (S.StartsWith(TEXT("inv:")) && S != TEXT("inv:panel")) InventoryAction(S); // drop / split / close buttons
			else InvSel = -1;                                    // empty space: deselect
			DragFrom = -1;
			return;
		}
		// a slot is already picked and this is another slot: swap them
		if (InvSel >= 0 && InvSel < 10 && Id < 10 && InvSel != Id)
		{
			SwapSlots(InvSel, Id);
			InvSel = Id;
			DragFrom = -1;
			return;
		}
		InvSel = Id;
		DragFrom = Id;
		DragStart = M;
		bDragMoved = false;
		SDAudio::Play(this, ESDSfx::Click);
		return;
	}
	if (DragFrom >= 0 && IsInputKeyDown(EKeys::LeftMouseButton))
	{
		if (FVector2D::Distance(M, DragStart) > 24.f) bDragMoved = true;
		return;
	}
	if (DragFrom >= 0 && WasInputKeyJustReleased(EKeys::LeftMouseButton))
	{
		const int32 From = DragFrom;
		DragFrom = -1;
		if (!bDragMoved) return; // plain click: stays selected
		const FName Hit = Hud->LobbyHit(M);
		const int32 To = InvIdOf(Hit.ToString());
		if (To >= 0 && To < 10 && From < 10 && To != From)
		{
			SwapSlots(From, To);
			InvSel = To;
		}
		else if (Hit.IsNone())
		{
			// dragged off the inventory: drop it
			InvSel = From;
			InventoryAction(TEXT("inv:drop"));
			InvSel = -1;
		}
	}
}
// ------------------------------------------------------------------ full map

void ASDPlayerController::SetBigMap(bool bOpen)
{
	bBigMap = bOpen;
	bMapDrag = false;
	if (bOpen)
	{
		// open centred on the player at the current zoom
		if (APawn* P = GetPawn()) MapCenter = FVector2D(P->GetActorLocation());
		if (MapZoom <= 1.01f) MapCenter = FVector2D::ZeroVector;
		if (ASDCharacter* C = Cast<ASDCharacter>(GetPawn())) { C->PrimaryUp(); C->SetAds(false); }
		FInputModeGameAndUI M;
		M.SetHideCursorDuringCapture(false);
		SetInputMode(M);
		SetShowMouseCursor(true);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
}

void ASDPlayerController::SetMarker(const FVector2D& P)
{
	bHasMarker = true;
	MarkerPos = P;
	ASDGameMode* GM = GetWorld()->GetAuthGameMode<ASDGameMode>();
	const float Ground = (GM && GM->Island) ? FMath::Max(GM->Island->HeightAt(P.X, P.Y), 0.f) : 0.f;
	if (!MarkerBeam)
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MarkerBeam = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform::Identity, SP);
		if (MarkerBeam)
		{
			UStaticMeshComponent* M = MarkerBeam->GetStaticMeshComponent();
			M->SetMobility(EComponentMobility::Movable);
			M->SetStaticMesh(SDAssets::Cylinder());
			M->SetMaterial(0, SDAssets::Ghost(FLinearColor(0.15f, 0.55f, 1.f)));
			M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			M->SetCastShadow(false);
			MarkerBeam->SetActorScale3D(FVector(2.25f, 2.25f, 400.f)); // 40,000 units tall
		}
	}
	if (MarkerBeam)
	{
		MarkerBeam->SetActorHiddenInGame(false);
		MarkerBeam->SetActorLocation(FVector(P.X, P.Y, Ground + 20000.f));
	}
	SDAudio::Play(this, ESDSfx::Click);
}

void ASDPlayerController::ClearMarker()
{
	bHasMarker = false;
	if (MarkerBeam) MarkerBeam->SetActorHiddenInGame(true);
	SDAudio::Play(this, ESDSfx::Click);
}

void ASDPlayerController::TickBigMap()
{
	ASDHUD* Hud = Cast<ASDHUD>(GetHUD());
	ASDGameMode* GM = GetWorld()->GetAuthGameMode<ASDGameMode>();
	if (!Hud || !GM || !GM->Island) return;
	float MX = 0.f, MY = 0.f;
	if (!GetMousePosition(MX, MY)) return;
	const FVector2D M(MX, MY);
	const float Ext = GM->Island->MapExtent();
	auto Clamp = [&]()
	{
		const float Half = Ext / MapZoom, Lim = FMath::Max(0.f, Ext - Half);
		MapCenter.X = FMath::Clamp(MapCenter.X, -Lim, Lim);
		MapCenter.Y = FMath::Clamp(MapCenter.Y, -Lim, Lim);
	};
	FVector2D Wp;
	const bool bOver = Hud->MapScreenToWorld(M, Wp);

	// wheel: zoom toward the point under the cursor
	const int32 Wheel = (WasInputKeyJustPressed(EKeys::MouseScrollUp) ? 1 : 0) - (WasInputKeyJustPressed(EKeys::MouseScrollDown) ? 1 : 0)
		+ (WasInputKeyJustPressed(EKeys::Add) || WasInputKeyJustPressed(EKeys::Equals) ? 1 : 0) - (WasInputKeyJustPressed(EKeys::Subtract) || WasInputKeyJustPressed(EKeys::Hyphen) ? 1 : 0);
	if (Wheel != 0)
	{
		const float Old = MapZoom;
		MapZoom = FMath::Clamp(MapZoom * (Wheel > 0 ? 1.35f : 1.f / 1.35f), 1.f, 9.f);
		if (bOver) MapCenter = Wp + (MapCenter - Wp) * (Old / MapZoom);
		Clamp();
	}

	// left drag pans, a left click (no drag) or a right click places / removes the marker
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && bOver)
	{
		bMapDrag = true;
		bMapMoved = false;
		MapDragStart = M;
		MapDragCenter = MapCenter;
	}
	if (bMapDrag && IsInputKeyDown(EKeys::LeftMouseButton))
	{
		const FVector2D D = M - MapDragStart;
		if (D.Size() > 6.f) bMapMoved = true;
		if (bMapMoved)
		{
			const float WorldPerPx = 2.f * (Ext / MapZoom) / FMath::Max(Hud->BigMapSize, 1.f);
			MapCenter = MapDragCenter + FVector2D(D.Y * WorldPerPx, -D.X * WorldPerPx);
			Clamp();
		}
	}
	// left click (without dragging) places the marker, right click removes it
	const bool bPlace = bMapDrag && WasInputKeyJustReleased(EKeys::LeftMouseButton) && !bMapMoved;
	if (WasInputKeyJustReleased(EKeys::LeftMouseButton)) bMapDrag = false;
	if (bPlace && Hud->MapScreenToWorld(M, Wp)) SetMarker(Wp);
	if (WasInputKeyJustPressed(EKeys::RightMouseButton) && bHasMarker) ClearMarker();
	if (WasInputKeyJustPressed(EKeys::Delete) || WasInputKeyJustPressed(EKeys::BackSpace)) { if (bHasMarker) ClearMarker(); }
}