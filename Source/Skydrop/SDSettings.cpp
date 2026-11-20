#include "SDSettings.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

namespace
{
	const TCHAR* SettingsSlot = TEXT("SkydropSettings");
	USDSettings* GSettings = nullptr;
}

USDSettings* USDSettings::Get()
{
	if (!GSettings)
	{
		GSettings = Cast<USDSettings>(UGameplayStatics::LoadGameFromSlot(SettingsSlot, 0));
		if (!GSettings) GSettings = Cast<USDSettings>(UGameplayStatics::CreateSaveGameObject(USDSettings::StaticClass()));
		GSettings->AddToRoot();
		if (GSettings->Keys.Num() != (int32)ESDAction::Count)
		{
			const TArray<FName> Old = GSettings->Keys;
			GSettings->Keys.SetNum((int32)ESDAction::Count);
			for (int32 i = 0; i < (int32)ESDAction::Count; ++i)
			{
				GSettings->Keys[i] = Old.IsValidIndex(i) && !Old[i].IsNone() ? Old[i] : DefaultKey((ESDAction)i).GetFName();
			}
		}
		// repair bindings saved by the old capture bug ("Any Key", virtual or unknown keys)
		for (int32 i = 0; i < GSettings->Keys.Num(); ++i)
		{
			const FKey K(GSettings->Keys[i]);
			if (!K.IsValid() || K == EKeys::AnyKey || K.IsVirtual() || K.IsGamepadKey())
			{
				GSettings->Keys[i] = DefaultKey((ESDAction)i).GetFName();
			}
		}
	}
	return GSettings;
}

void USDSettings::Write()
{
	UGameplayStatics::SaveGameToSlot(this, SettingsSlot, 0);
}

const TArray<FIntPoint>& USDSettings::Resolutions()
{
	static const TArray<FIntPoint> R = { {1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160} };
	return R;
}

const TArray<int32>& USDSettings::FrameCaps()
{
	static const TArray<int32> F = { 30, 60, 120, 144, 240, 0 };
	return F;
}

void USDSettings::Apply(bool bDisplay)
{
	if (!GEngine) return;
	if (UGameUserSettings* G = GEngine->GetGameUserSettings())
	{
		G->SetViewDistanceQuality(ViewDistance);
		G->SetShadowQuality(Shadows);
		G->SetAntiAliasingQuality(AntiAliasing);
		G->SetVisualEffectQuality(Effects);
		G->SetPostProcessingQuality(FMath::Max(Quality, 1));
		G->SetTextureQuality(Quality);
		G->SetShadingQuality(Quality);
		G->SetFoliageQuality(Quality);
		G->SetGlobalIlluminationQuality(Quality);
		G->SetReflectionQuality(Quality);
		G->SetVSyncEnabled(bVSync);
		G->SetFrameRateLimit((float)FrameCaps()[FMath::Clamp(FrameCap, 0, FrameCaps().Num() - 1)]);
		if (bDisplay && bDisplaySet)
		{
			const EWindowMode::Type Modes[3] = { EWindowMode::Windowed, EWindowMode::WindowedFullscreen, EWindowMode::Fullscreen };
			G->SetFullscreenMode(Modes[FMath::Clamp(WindowMode, 0, 2)]);
			G->SetScreenResolution(Resolutions()[FMath::Clamp(Resolution, 0, Resolutions().Num() - 1)]);
			G->ApplySettings(false);
		}
		else
		{
			// leave the window exactly as launched until the player picks a mode/resolution
			G->ApplyNonResolutionSettings();
		}
	}
	GEngine->DisplayGamma = FMath::Clamp(Brightness, 1.4f, 3.2f);
	if (IConsoleVariable* MB = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality"))) MB->Set(bMotionBlur ? 4 : 0, ECVF_SetByGameSetting);
}

void USDSettings::ResetSection(int32 Section)
{
	const USDSettings* D = GetDefault<USDSettings>();
	switch (Section)
	{
	case 0:
		Quality = D->Quality; ViewDistance = D->ViewDistance; Shadows = D->Shadows; Effects = D->Effects;
		AntiAliasing = D->AntiAliasing; Grass = D->Grass; FrameCap = D->FrameCap; bVSync = D->bVSync;
		bMotionBlur = D->bMotionBlur; bShowFPS = D->bShowFPS; Brightness = D->Brightness;
		break;
	case 1:
		Sensitivity = D->Sensitivity; AdsSensitivity = D->AdsSensitivity; bInvertY = D->bInvertY; FieldOfView = D->FieldOfView;
		bShowHelp = D->bShowHelp; bDamageNumbers = D->bDamageNumbers; bShowMinimap = D->bShowMinimap; EditConfirm = D->EditConfirm;
		break;
	case 2:
		MasterVolume = D->MasterVolume; MusicVolume = D->MusicVolume; EffectsVolume = D->EffectsVolume; UIVolume = D->UIVolume;
		break;
	default:
		for (int32 i = 0; i < (int32)ESDAction::Count; ++i) Keys[i] = DefaultKey((ESDAction)i).GetFName();
		break;
	}
}

FKey USDSettings::Key(ESDAction A) const
{
	const int32 I = (int32)A;
	return Keys.IsValidIndex(I) ? FKey(Keys[I]) : DefaultKey(A);
}

void USDSettings::Bind(ESDAction A, const FKey& K)
{
	// a key can only do one thing: whatever had it before gets this action's old key
	const FName Old = Keys[(int32)A];
	for (int32 i = 0; i < Keys.Num(); ++i)
	{
		if (i != (int32)A && Keys[i] == K.GetFName()) Keys[i] = Old;
	}
	Keys[(int32)A] = K.GetFName();
}

FKey USDSettings::DefaultKey(ESDAction A)
{
	switch (A)
	{
	case ESDAction::Forward: return EKeys::W;
	case ESDAction::Back: return EKeys::S;
	case ESDAction::Left: return EKeys::A;
	case ESDAction::Right: return EKeys::D;
	case ESDAction::Jump: return EKeys::SpaceBar;
	case ESDAction::Crouch: return EKeys::LeftControl;
	case ESDAction::Fire: return EKeys::LeftMouseButton;
	case ESDAction::Aim: return EKeys::RightMouseButton;
	case ESDAction::Reload: return EKeys::R;
	case ESDAction::Interact: return EKeys::E;
	case ESDAction::Build: return EKeys::Q;
	case ESDAction::Wall: return EKeys::Z;
	case ESDAction::Floor: return EKeys::X;
	case ESDAction::Ramp: return EKeys::V;
	case ESDAction::Cone: return EKeys::T;
	case ESDAction::Edit: return EKeys::G;
	case ESDAction::Pickaxe: return EKeys::One;
	case ESDAction::Slot1: return EKeys::Two;
	case ESDAction::Slot2: return EKeys::Three;
	case ESDAction::Slot3: return EKeys::Four;
	case ESDAction::Slot4: return EKeys::Five;
	case ESDAction::Slot5: return EKeys::Six;
	case ESDAction::Map: return EKeys::M;
	case ESDAction::Emote: return EKeys::B;
	case ESDAction::Help: return EKeys::H;
	case ESDAction::Inventory: return EKeys::Tab;
	default: return EKeys::Invalid;
	}
}

const TCHAR* USDSettings::ActionName(ESDAction A)
{
	static const TCHAR* N[] = {
		TEXT("Move forward"), TEXT("Move back"), TEXT("Move left"), TEXT("Move right"), TEXT("Jump"), TEXT("Crouch"),
		TEXT("Fire / place"), TEXT("Aim down sights"), TEXT("Reload"), TEXT("Use / pick up"),
		TEXT("Build mode"), TEXT("Wall"), TEXT("Floor"), TEXT("Ramp"), TEXT("Cone"), TEXT("Edit"),
		TEXT("Pickaxe"), TEXT("Slot 1"), TEXT("Slot 2"), TEXT("Slot 3"), TEXT("Slot 4"), TEXT("Slot 5"),
		TEXT("Map"), TEXT("Emote"), TEXT("Show / hide help"), TEXT("Inventory") };
	static_assert(UE_ARRAY_COUNT(N) == (int32)ESDAction::Count, "action names");
	return N[(int32)A];
}

// ------------------------------------------------------------------ options table

FString FSDOption::ValueText(const USDSettings* S) const
{
	const float V = Get(S);
	if (!bSlider) return Choices.IsValidIndex(FMath::RoundToInt(V)) ? Choices[FMath::RoundToInt(V)] : FString();
	if (bPercent) return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(V * 100.f));
	return FString::Printf(TEXT("%.*f"), Decimals, V);
}

const TArray<FSDOption>& USDSettings::Options()
{
	static TArray<FSDOption> O;
	if (O.Num()) return O;
	const TArray<FString> Q4 = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic") };
	const TArray<FString> OnOff = { TEXT("Off"), TEXT("On") };
	auto Choice = [&](const TCHAR* Id, const TCHAR* Label, int32 Sec, const TArray<FString>& Ch,
		TFunction<float(const USDSettings*)> G, TFunction<void(USDSettings*, float)> S, bool bDisplay = false)
	{
		FSDOption X; X.Id = Id; X.Label = Label; X.Section = Sec; X.Choices = Ch; X.Get = G; X.Set = S; X.bDisplay = bDisplay;
		O.Add(X);
	};
	auto Slider = [&](const TCHAR* Id, const TCHAR* Label, int32 Sec, float Mn, float Mx, float Step, int32 Dec, bool bPct,
		TFunction<float(const USDSettings*)> G, TFunction<void(USDSettings*, float)> S)
	{
		FSDOption X; X.Id = Id; X.Label = Label; X.Section = Sec; X.bSlider = true; X.Min = Mn; X.Max = Mx; X.Step = Step;
		X.Decimals = Dec; X.bPercent = bPct; X.Get = G; X.Set = S;
		O.Add(X);
	};
#define SD_INT(F) [](const USDSettings* s) { return (float)s->F; }, [](USDSettings* s, float v) { s->F = FMath::RoundToInt(v); }
#define SD_BOOL(F) [](const USDSettings* s) { return s->F ? 1.f : 0.f; }, [](USDSettings* s, float v) { s->F = v > 0.5f; }
#define SD_FLT(F) [](const USDSettings* s) { return s->F; }, [](USDSettings* s, float v) { s->F = v; }

	TArray<FString> Res;
	for (const FIntPoint& R : Resolutions()) Res.Add(FString::Printf(TEXT("%d x %d"), R.X, R.Y));
	TArray<FString> Caps;
	for (int32 F : FrameCaps()) Caps.Add(F > 0 ? FString::Printf(TEXT("%d FPS"), F) : FString(TEXT("Unlimited")));

	// video
	Choice(TEXT("window"), TEXT("Window Mode"), 0, { TEXT("Windowed"), TEXT("Borderless"), TEXT("Fullscreen") }, SD_INT(WindowMode), true);
	Choice(TEXT("res"), TEXT("Resolution"), 0, Res, SD_INT(Resolution), true);
	Choice(TEXT("quality"), TEXT("Quality Preset"), 0, Q4,
		[](const USDSettings* s) { return (float)s->Quality; },
		[](USDSettings* s, float v) { const int32 q = FMath::RoundToInt(v); s->Quality = q; s->ViewDistance = q; s->Shadows = q; s->Effects = q; s->AntiAliasing = q; s->Grass = q == 0 ? 0 : q == 1 ? 1 : 2; });
	Choice(TEXT("view"), TEXT("View Distance"), 0, { TEXT("Near"), TEXT("Medium"), TEXT("Far"), TEXT("Epic") }, SD_INT(ViewDistance));
	Choice(TEXT("shadows"), TEXT("Shadows"), 0, { TEXT("Off"), TEXT("Medium"), TEXT("High"), TEXT("Epic") }, SD_INT(Shadows));
	Choice(TEXT("aa"), TEXT("Anti-Aliasing"), 0, { TEXT("Off"), TEXT("Medium"), TEXT("High"), TEXT("Epic") }, SD_INT(AntiAliasing));
	Choice(TEXT("effects"), TEXT("Effects"), 0, Q4, SD_INT(Effects));
	Choice(TEXT("grass"), TEXT("Grass"), 0, { TEXT("Off"), TEXT("Low"), TEXT("High") }, SD_INT(Grass));
	Choice(TEXT("fps"), TEXT("Frame Rate Limit"), 0, Caps, SD_INT(FrameCap));
	Choice(TEXT("vsync"), TEXT("VSync"), 0, OnOff, SD_BOOL(bVSync));
	Choice(TEXT("blur"), TEXT("Motion Blur"), 0, OnOff, SD_BOOL(bMotionBlur));
	Choice(TEXT("showfps"), TEXT("Show FPS"), 0, OnOff, SD_BOOL(bShowFPS));
	Slider(TEXT("bright"), TEXT("Brightness"), 0, 1.4f, 3.2f, 0.1f, 1, false, SD_FLT(Brightness));
	// game
	Slider(TEXT("sens"), TEXT("Mouse Sensitivity"), 1, 0.1f, 4.f, 0.05f, 2, false, SD_FLT(Sensitivity));
	Slider(TEXT("adssens"), TEXT("Aim (ADS) Sensitivity"), 1, 0.2f, 1.f, 0.05f, 2, false, SD_FLT(AdsSensitivity));
	Choice(TEXT("invert"), TEXT("Invert Look"), 1, OnOff, SD_BOOL(bInvertY));
	Slider(TEXT("fov"), TEXT("Field of View"), 1, 70.f, 110.f, 1.f, 0, false, SD_FLT(FieldOfView));
	Choice(TEXT("help"), TEXT("Show Controls Help"), 1, OnOff, SD_BOOL(bShowHelp));
	Choice(TEXT("dmgnum"), TEXT("Damage Numbers"), 1, OnOff, SD_BOOL(bDamageNumbers));
	Choice(TEXT("minimap"), TEXT("Minimap"), 1, OnOff, SD_BOOL(bShowMinimap));
	Choice(TEXT("editrelease"), TEXT("Edit on Release"), 1, { TEXT("Off (press Edit again)"), TEXT("Release Edit key"), TEXT("Release mouse") }, SD_INT(EditConfirm));
	// audio
	Slider(TEXT("master"), TEXT("Master Volume"), 2, 0.f, 1.f, 0.05f, 0, true, SD_FLT(MasterVolume));
	Slider(TEXT("music"), TEXT("Music Volume"), 2, 0.f, 1.f, 0.05f, 0, true, SD_FLT(MusicVolume));
	Slider(TEXT("sfx"), TEXT("Sound Effects Volume"), 2, 0.f, 1.f, 0.05f, 0, true, SD_FLT(EffectsVolume));
	Slider(TEXT("ui"), TEXT("Menu Sounds Volume"), 2, 0.f, 1.f, 0.05f, 0, true, SD_FLT(UIVolume));
#undef SD_INT
#undef SD_BOOL
#undef SD_FLT
	return O;
}

const FSDOption* USDSettings::FindOption(FName Id)
{
	for (const FSDOption& O : Options()) if (O.Id == Id) return &O;
	return nullptr;
}