#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "InputCoreTypes.h"
#include "SDSettings.generated.h"

// Rebindable match actions
enum class ESDAction : uint8
{
	Forward, Back, Left, Right, Jump, Crouch,
	Fire, Aim, Reload, Interact,
	Build, Wall, Floor, Ramp, Cone, Edit,
	Pickaxe, Slot1, Slot2, Slot3, Slot4, Slot5,
	Map, Emote, Help, Inventory,
	Count
};

// One row on the settings screen. Choice rows step through Choices, slider rows run Min..Max.
struct FSDOption
{
	FName Id;
	const TCHAR* Label = TEXT("");
	int32 Section = 0;           // 0 video, 1 game, 2 audio
	bool bSlider = false;
	TArray<FString> Choices;
	float Min = 0.f, Max = 1.f, Step = 0.05f;
	int32 Decimals = 2;
	bool bPercent = false;
	bool bDisplay = false;       // needs a window/resolution apply
	TFunction<float(const class USDSettings*)> Get;
	TFunction<void(class USDSettings*, float)> Set;
	FString ValueText(const class USDSettings* S) const;
};

// Player options: video, game, audio and key bindings. Saved to the "SkydropSettings" slot.
UCLASS()
class SKYDROP_API USDSettings : public USaveGame
{
	GENERATED_BODY()
public:
	static USDSettings* Get();
	void Write();
	// push everything to the engine (scalability, window, gamma, cvars); bDisplay also applies window mode + resolution
	void Apply(bool bDisplay);
	void ResetSection(int32 Section);

	// --- video
	UPROPERTY() int32 WindowMode = 0;      // 0 windowed, 1 borderless, 2 fullscreen
	UPROPERTY() int32 Resolution = 2;      // index into Resolutions()
	UPROPERTY() bool bDisplaySet = false;  // only touch the window once the player changed it
	UPROPERTY() int32 Quality = 3;         // preset 0 low .. 3 epic
	UPROPERTY() int32 ViewDistance = 3;
	UPROPERTY() int32 Shadows = 3;
	UPROPERTY() int32 Effects = 3;
	UPROPERTY() int32 AntiAliasing = 3;
	UPROPERTY() int32 Grass = 2;           // 0 off, 1 low, 2 high
	UPROPERTY() int32 FrameCap = 5;        // index into FrameCaps()
	UPROPERTY() bool bVSync = false;
	UPROPERTY() bool bMotionBlur = false;
	UPROPERTY() bool bShowFPS = false;
	UPROPERTY() float Brightness = 2.2f;   // display gamma

	// --- game
	UPROPERTY() float Sensitivity = 1.f;
	UPROPERTY() float AdsSensitivity = 0.55f;
	UPROPERTY() bool bInvertY = false;
	UPROPERTY() float FieldOfView = 80.f;
	UPROPERTY() bool bShowHelp = true;
	UPROPERTY() bool bDamageNumbers = true;
	UPROPERTY() bool bShowMinimap = true;
	UPROPERTY() int32 EditConfirm = 0;  // 0 press Edit again, 1 release the Edit key (edit on release), 2 release the fire button

	// --- audio (0..1)
	UPROPERTY() float MasterVolume = 0.8f;
	UPROPERTY() float MusicVolume = 0.5f;
	UPROPERTY() float EffectsVolume = 0.9f;
	UPROPERTY() float UIVolume = 0.8f;

	// --- controls (FKey names, one per ESDAction)
	UPROPERTY() TArray<FName> Keys;

	FKey Key(ESDAction A) const;
	void Bind(ESDAction A, const FKey& K);
	static FKey DefaultKey(ESDAction A);
	static const TCHAR* ActionName(ESDAction A);

	static const TArray<FSDOption>& Options();
	static const FSDOption* FindOption(FName Id);
	static const TArray<FIntPoint>& Resolutions();
	static const TArray<int32>& FrameCaps(); // 0 = unlimited
};
