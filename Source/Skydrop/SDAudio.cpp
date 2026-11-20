#include "SDAudio.h"
#include "SDSettings.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundAttenuation.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

namespace
{
	constexpr int32 SR = 22050;
	using FBuf = TArray<int16>;

	float Noise(uint32& Seed)
	{
		Seed = Seed * 1664525u + 1013904223u;
		return ((Seed >> 9) & 0x7FFF) / 16383.5f - 1.f;
	}

	FBuf ToPCM(const TArray<float>& F, float Gain = 1.f)
	{
		FBuf B;
		B.SetNumUninitialized(F.Num());
		float Peak = 0.001f;
		for (float V : F) Peak = FMath::Max(Peak, FMath::Abs(V));
		const float K = Gain * 0.9f / Peak;
		for (int32 i = 0; i < F.Num(); ++i)
		{
			// soft clip keeps loud transients round instead of harsh
			const float V = FMath::Clamp(F[i] * K, -1.5f, 1.5f);
			const float S = V - V * V * V / 6.75f;
			B[i] = (int16)FMath::Clamp(FMath::RoundToInt(S * 32000.f), -32767, 32767);
		}
		return B;
	}

	// gunshot: noise crack + low thump + optional tail
	TArray<float> Gun(float Len, float Decay, float Thump, float ThumpHz, float Bright, float Tail, uint32 Seed)
	{
		const int32 N = FMath::RoundToInt(Len * SR);
		TArray<float> F; F.SetNumZeroed(N);
		float Lp = 0.f, Lp2 = 0.f;
		for (int32 i = 0; i < N; ++i)
		{
			const float T = i / float(SR);
			const float Env = FMath::Exp(-T * Decay);
			const float Nz = Noise(Seed);
			Lp += Bright * (Nz - Lp);
			Lp2 += 0.05f * (Nz - Lp2);
			const float Crack = (Lp * 0.8f + (Nz - Lp) * 0.35f) * Env;
			const float Body = FMath::Sin(2.f * PI * ThumpHz * T * (1.f - T * 0.8f)) * FMath::Exp(-T * 22.f) * Thump;
			const float Rumble = Lp2 * FMath::Exp(-T * (Decay * 0.25f)) * Tail;
			F[i] = Crack + Body + Rumble;
		}
		return F;
	}

	TArray<float> Tones(float Len, const TArray<FVector3f>& Parts /* hz, amp, decay */, float NoiseAmt = 0.f, float Attack = 0.002f)
	{
		const int32 N = FMath::RoundToInt(Len * SR);
		TArray<float> F; F.SetNumZeroed(N);
		uint32 Seed = 77;
		for (int32 i = 0; i < N; ++i)
		{
			const float T = i / float(SR);
			const float A = FMath::Clamp(T / Attack, 0.f, 1.f);
			float V = 0.f;
			for (const FVector3f& P : Parts) V += FMath::Sin(2.f * PI * P.X * T) * P.Y * FMath::Exp(-T * P.Z);
			V += Noise(Seed) * NoiseAmt * FMath::Exp(-T * 60.f);
			F[i] = V * A;
		}
		return F;
	}

	// notes played one after another (hz, start, amp)
	TArray<float> Arp(float Len, const TArray<FVector3f>& Notes, float Decay)
	{
		const int32 N = FMath::RoundToInt(Len * SR);
		TArray<float> F; F.SetNumZeroed(N);
		for (const FVector3f& No : Notes)
		{
			const int32 S0 = FMath::RoundToInt(No.Y * SR);
			for (int32 i = S0; i < N; ++i)
			{
				const float T = (i - S0) / float(SR);
				const float A = FMath::Clamp(T / 0.004f, 0.f, 1.f) * FMath::Exp(-T * Decay) * No.Z;
				F[i] += (FMath::Sin(2.f * PI * No.X * T) + 0.3f * FMath::Sin(4.f * PI * No.X * T)) * A;
			}
		}
		return F;
	}

	FBuf Build(ESDSfx S)
	{
		switch (S)
		{
		case ESDSfx::Click: return ToPCM(Tones(0.06f, { {1400.f, 1.f, 70.f}, {2800.f, 0.3f, 90.f} }), 0.5f);
		case ESDSfx::Ready: return ToPCM(Arp(0.6f, { {523.f, 0.f, 1.f}, {659.f, 0.08f, 1.f}, {784.f, 0.16f, 1.f}, {1046.f, 0.24f, 1.2f} }, 7.f), 0.6f);
		case ESDSfx::Buy: return ToPCM(Arp(0.7f, { {1318.f, 0.f, 1.f}, {1568.f, 0.07f, 1.f}, {2093.f, 0.14f, 1.3f} }, 6.f), 0.6f);
		case ESDSfx::AR: return ToPCM(Gun(0.45f, 26.f, 0.9f, 95.f, 0.35f, 0.5f, 11));
		case ESDSfx::SMG: return ToPCM(Gun(0.3f, 38.f, 0.6f, 120.f, 0.45f, 0.3f, 12), 0.85f);
		case ESDSfx::Pistol: return ToPCM(Gun(0.35f, 32.f, 0.7f, 130.f, 0.4f, 0.3f, 13), 0.9f);
		case ESDSfx::Shotgun: return ToPCM(Gun(0.8f, 14.f, 1.3f, 70.f, 0.22f, 0.9f, 14));
		case ESDSfx::Sniper: return ToPCM(Gun(1.4f, 12.f, 1.4f, 60.f, 0.3f, 1.2f, 15));
		case ESDSfx::Pickaxe: return ToPCM(Tones(0.35f, { {1180.f, 0.8f, 18.f}, {2650.f, 0.5f, 26.f}, {4150.f, 0.3f, 35.f}, {320.f, 0.6f, 30.f} }, 1.2f), 0.8f);
		case ESDSfx::Build: return ToPCM(Tones(0.3f, { {170.f, 1.f, 16.f}, {340.f, 0.4f, 22.f}, {95.f, 0.6f, 12.f} }, 1.6f), 0.8f);
		case ESDSfx::Pickup: return ToPCM(Arp(0.3f, { {880.f, 0.f, 1.f}, {1320.f, 0.06f, 1.f} }, 14.f), 0.55f);
		case ESDSfx::Chest: return ToPCM(Arp(1.2f, { {1046.f, 0.f, 0.8f}, {1318.f, 0.09f, 0.8f}, {1568.f, 0.18f, 0.8f}, {2093.f, 0.27f, 1.f}, {2637.f, 0.36f, 0.7f} }, 4.f), 0.7f);
		case ESDSfx::Hit: return ToPCM(Tones(0.08f, { {2100.f, 1.f, 60.f} }, 0.4f), 0.45f);
		case ESDSfx::HeadHit: return ToPCM(Tones(0.35f, { {2600.f, 1.f, 12.f}, {3900.f, 0.4f, 16.f} }), 0.55f);
		case ESDSfx::Elim: return ToPCM(Arp(0.9f, { {392.f, 0.f, 1.f}, {587.f, 0.1f, 1.f}, {784.f, 0.2f, 1.2f} }, 5.f), 0.7f);
		case ESDSfx::Glider:
		{
			const int32 N = FMath::RoundToInt(0.9f * SR);
			TArray<float> F; F.SetNumZeroed(N);
			uint32 Seed = 5; float Lp = 0.f;
			for (int32 i = 0; i < N; ++i)
			{
				const float T = i / float(SR);
				const float Env = FMath::Sin(PI * FMath::Clamp(T / 0.9f, 0.f, 1.f));
				Lp += (0.02f + 0.08f * Env) * (Noise(Seed) - Lp);
				F[i] = Lp * Env;
			}
			return ToPCM(F, 0.7f);
		}
		case ESDSfx::Reload:
		{
			TArray<float> A = Tones(0.5f, { {900.f, 0.5f, 60.f}, {2400.f, 0.3f, 80.f} }, 0.8f);
			const TArray<float> B = Tones(0.25f, { {700.f, 0.6f, 50.f}, {1900.f, 0.3f, 70.f} }, 0.8f);
			const int32 Off = FMath::RoundToInt(0.25f * SR);
			for (int32 i = 0; i < B.Num() && Off + i < A.Num(); ++i) A[Off + i] += B[i];
			return ToPCM(A, 0.6f);
		}
		default: return FBuf();
		}
	}

	// ~19 s lobby loop: soft pads, arpeggio, light kick and hats at 100 bpm
	FBuf BuildMusic()
	{
		const float Bpm = 100.f, Beat = 60.f / Bpm;
		const int32 Bars = 8;
		const int32 N = FMath::RoundToInt(Bars * 4 * Beat * SR);
		TArray<float> F; F.SetNumZeroed(N);
		// chords (root midi) : Am F C G, twice
		const int32 Roots[8] = { 57, 53, 48, 55, 57, 53, 48, 55 };
		const bool bMinor[8] = { true, false, false, false, true, false, false, false };
		auto Hz = [](float Midi) { return 440.f * FMath::Pow(2.f, (Midi - 69.f) / 12.f); };
		float PadLp = 0.f;
		uint32 Seed = 99;
		for (int32 i = 0; i < N; ++i)
		{
			const float T = i / float(SR);
			const int32 Bar = FMath::Min(int32(T / (4 * Beat)), Bars - 1);
			const float InBar = T - Bar * 4 * Beat;
			const int32 R = Roots[Bar];
			const int32 Third = bMinor[Bar] ? 3 : 4;
			// pad: detuned saws through a lowpass
			float Pad = 0.f;
			const float Notes[4] = { (float)R, (float)(R + Third), (float)(R + 7), (float)(R + 12) };
			for (float M : Notes)
			{
				const float H = Hz(M - 12.f);
				Pad += FMath::Fmod(T * H, 1.f) - 0.5f;
				Pad += FMath::Fmod(T * H * 1.004f, 1.f) - 0.5f;
			}
			const float BarFade = FMath::Min(1.f, InBar / 0.4f) * FMath::Min(1.f, (4 * Beat - InBar) / 0.3f + 0.6f);
			PadLp += 0.035f * (Pad * BarFade - PadLp);
			// arpeggio: 8th notes up the chord, two octaves
			const float Step = Beat * 0.5f;
			const int32 Si = int32(InBar / Step);
			const float St = InBar - Si * Step;
			const int32 Pattern[8] = { 0, 1, 2, 3, 2, 3, 1, 2 };
			const float ArpM = Notes[Pattern[Si % 8]] + 12.f;
			const float Arp = FMath::Sin(2.f * PI * Hz(ArpM) * St) * FMath::Exp(-St * 9.f) * 0.35f;
			// bass on beats 1 and 3
			const float Bt = FMath::Fmod(InBar, 2.f * Beat);
			const float Bass = FMath::Sin(2.f * PI * Hz(R - 24.f) * Bt) * FMath::Exp(-Bt * 2.5f) * 0.6f;
			// kick on every beat, hat on off-beats
			const float Kt = FMath::Fmod(T, Beat);
			const float Kick = FMath::Sin(2.f * PI * (50.f + 90.f * FMath::Exp(-Kt * 30.f)) * Kt) * FMath::Exp(-Kt * 9.f) * 0.7f;
			const float Ht = FMath::Fmod(T + Beat * 0.5f, Beat);
			const float Hat = Noise(Seed) * FMath::Exp(-Ht * 70.f) * 0.12f;
			F[i] = PadLp * 0.22f + Arp + Bass + Kick + Hat;
		}
		// crossfade the loop seam
		const int32 X = SR / 20;
		for (int32 i = 0; i < X; ++i) { const float A = i / float(X); F[i] *= A; F[N - 1 - i] *= A; }
		return ToPCM(F, 0.8f);
	}

	TArray<FBuf>& Bank()
	{
		static TArray<FBuf> B;
		if (B.Num() == 0)
		{
			B.SetNum((int32)ESDSfx::Count);
			for (int32 i = 0; i < (int32)ESDSfx::Count; ++i) B[i] = Build((ESDSfx)i);
		}
		return B;
	}

	USoundAttenuation* Atten()
	{
		static USoundAttenuation* A = nullptr;
		if (!A)
		{
			A = NewObject<USoundAttenuation>();
			A->AddToRoot();
			A->Attenuation.bAttenuate = true;
			A->Attenuation.bSpatialize = true;
			A->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
			A->Attenuation.AttenuationShapeExtents = FVector(400.f, 0.f, 0.f);
			A->Attenuation.FalloffDistance = 9000.f;
			A->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		}
		return A;
	}

	USoundWaveProcedural* MakeWave(const FBuf& B, bool bLoop)
	{
		USoundWaveProcedural* W = NewObject<USoundWaveProcedural>();
		W->SetSampleRate(SR);
		W->NumChannels = 1;
		W->Duration = bLoop ? INDEFINITELY_LOOPING_DURATION : B.Num() / float(SR);
		W->SoundGroup = SOUNDGROUP_Default;
		W->bLooping = false;
		W->QueueAudio(reinterpret_cast<const uint8*>(B.GetData()), B.Num() * sizeof(int16));
		return W;
	}

	float Category(ESDSfx S)
	{
		const USDSettings* St = USDSettings::Get();
		const bool bUI = S == ESDSfx::Click || S == ESDSfx::Ready || S == ESDSfx::Buy;
		return St->MasterVolume * (bUI ? St->UIVolume : St->EffectsVolume);
	}

	TWeakObjectPtr<UAudioComponent> Music;
	TSharedPtr<FBuf, ESPMode::ThreadSafe> MusicBuf;
}

void SDAudio::Play(UObject* Ctx, ESDSfx S, const FVector* At, float Volume)
{
	UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	if (!W || W->IsNetMode(NM_DedicatedServer)) return;
	const float V = Category(S) * Volume;
	if (V <= 0.01f) return;
	if (At)
	{
		// skip sounds far beyond hearing range
		if (APlayerController* PC = W->GetFirstPlayerController())
		{
			if (PC->PlayerCameraManager && FVector::DistSquared(PC->PlayerCameraManager->GetCameraLocation(), *At) > FMath::Square(9500.f)) return;
		}
	}
	const FBuf& B = Bank()[(int32)S];
	if (B.Num() == 0) return;
	USoundWaveProcedural* Wave = MakeWave(B, false);
	const float Pitch = FMath::FRandRange(0.96f, 1.04f);
	if (At) UGameplayStatics::PlaySoundAtLocation(W, Wave, *At, V, Pitch, 0.f, Atten());
	else UGameplayStatics::PlaySound2D(W, Wave, V, Pitch);
}

void SDAudio::StartMusic(UObject* Ctx)
{
	UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	if (!W) return;
	if (Music.IsValid() && Music->IsPlaying()) return;
	if (!MusicBuf.IsValid()) MusicBuf = MakeShared<FBuf, ESPMode::ThreadSafe>(BuildMusic());
	USoundWaveProcedural* Wave = MakeWave(*MusicBuf, true);
	TSharedPtr<FBuf, ESPMode::ThreadSafe> Buf = MusicBuf;
	// keep the loop going: whenever the queue runs low, queue the whole track again
	Wave->OnSoundWaveProceduralUnderflow = FOnSoundWaveProceduralUnderflow::CreateLambda([Buf](USoundWaveProcedural* InWave, int32)
	{
		InWave->QueueAudio(reinterpret_cast<const uint8*>(Buf->GetData()), Buf->Num() * sizeof(int16));
	});
	const USDSettings* St = USDSettings::Get();
	UAudioComponent* AC = UGameplayStatics::CreateSound2D(W, Wave, FMath::Max(St->MasterVolume * St->MusicVolume, 0.001f), 1.f, 0.f, nullptr, false, false);
	if (AC)
	{
		AC->Play();
		Music = AC;
	}
}

void SDAudio::StopMusic()
{
	if (Music.IsValid()) { Music->FadeOut(0.6f, 0.f); }
	Music = nullptr;
}

void SDAudio::RefreshVolumes()
{
	if (Music.IsValid())
	{
		const USDSettings* St = USDSettings::Get();
		Music->SetVolumeMultiplier(FMath::Max(St->MasterVolume * St->MusicVolume, 0.001f));
	}
}
