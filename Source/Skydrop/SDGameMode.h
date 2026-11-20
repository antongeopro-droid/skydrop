#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SkydropTypes.h"
#include "SDGameMode.generated.h"

class ASDWorld;
class ASDStorm;
class ASDBus;
class ASDCharacter;
class ASDBuildPiece;
class ASDLobby;
class USDSave;

enum class ESDPhase : uint8 { Lobby, Matchmaking, Loading, Bus, Playing, Over };

struct FSDFeed
{
	FString Text;
	float Time;
	bool bPlayer;
};

UCLASS()
class SKYDROP_API ASDGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ASDGameMode();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	virtual void RestartPlayer(AController* NewPlayer) override {}

	bool IsOccupied(const FSDBuildSlot& S) const;
	ASDBuildPiece* TryPlace(const FSDBuildSlot& S, ESDMat M, ASDCharacter* By, bool bWorldPiece, bool bInstant);
	void OnPieceRemoved(ASDBuildPiece* P);
	void DropFromBus(ASDCharacter* C);
	void OnEliminated(ASDCharacter* Victim, ASDCharacter* Killer);
	void RestartMatch();

	// --- lobby / profile
	void LobbyAction(FName Action);
	// settings screen + in-match pause menu (works in any phase)
	void MenuAction(FName Action);
	void SetMenu(bool bOpen);
	int32 SettingsTab = 0;      // 0 video, 1 game, 2 audio, 3 controls
	int32 BindingAction = -1;   // waiting for a key for this ESDAction
	bool bMenuOpen = false;     // in-match pause menu
	int32 MenuView = 0;         // 0 pause buttons, 1 settings
	void OnPlayerChestOpened() { ++PlayerChests; }
	UPROPERTY() TObjectPtr<ASDLobby> Lobby;
	UPROPERTY() TObjectPtr<USDSave> Save;
	int32 LobbyTab = 0;
	int32 PassPage = 0;
	int32 PassSelTier = 0;
	int32 PassSelRow = 0;
	bool bSelecting = false;
	int32 LockerSlot = 0;
	FName PendingBuy = NAME_None;
	FName ShopNewId = NAME_None;
	FName ShopView = NAME_None;   // item page open in the shop
	FString Toast;
	float ToastTime = 0.f;
	int32 PlayerHarvest = 0;
	void OnPlayerHarvest(int32 N) { PlayerHarvest += N; }
	float MatchmakingT = 0.f;
	int32 FakeQueue = 1;
	int32 PlayerChests = 0;
	bool bRewarded = false;
	TArray<FString> RewardLines;

	UPROPERTY() TObjectPtr<ASDWorld> Island;
	UPROPERTY() TObjectPtr<ASDStorm> Storm;
	UPROPERTY() TObjectPtr<ASDBus> Bus;
	UPROPERTY() TObjectPtr<ASDCharacter> PlayerChar;

	TArray<TWeakObjectPtr<ASDCharacter>> Alive;
	TArray<TWeakObjectPtr<ASDCharacter>> InBus;
	TMap<FString, TWeakObjectPtr<ASDBuildPiece>> Pieces;
	TArray<FSDFeed> Feed;
	ESDPhase Phase = ESDPhase::Lobby;
	int32 PlayerPlace = 0;
	bool bPlayerOut = false;
	bool bPlayerWon = false;
	FString WinnerName;
	int32 TotalPlayers = 0;
	int32 NumBots = 49;
	float MatchClock = 0.f;

private:
	void StartMatch();
	void EnterLobby();
	void AwardRewards();
	int32 LoadingFrames = 0;
	void CheckIntegrity();
	void TickAutoTest(float Dt);
	float AutoT = 0.f;
	float AutoStep = 0.f;
	bool bIntegrityDirty = false;
	float IntegrityTimer = 0.f;
	FTimerHandle StartHandle;
};
