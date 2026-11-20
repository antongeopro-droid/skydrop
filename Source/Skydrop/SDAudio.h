#pragma once

#include "CoreMinimal.h"

class UObject;

// Synthesised sound effects and lobby music (no audio assets needed).
// Volumes come from USDSettings: Master x Effects / UI / Music.
enum class ESDSfx : uint8
{
	Click, Ready, Buy,
	AR, SMG, Pistol, Shotgun, Sniper,
	Pickaxe, Build, Pickup, Chest, Hit, HeadHit, Elim, Glider, Reload,
	Count
};

namespace SDAudio
{
	// At == nullptr plays 2D
	void Play(UObject* Ctx, ESDSfx S, const FVector* At = nullptr, float Volume = 1.f);
	void StartMusic(UObject* Ctx);
	void StopMusic();
	void RefreshVolumes();
}
