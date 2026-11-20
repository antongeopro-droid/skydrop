# Skydrop

A fan-made battle royale built in **Unreal Engine 5.8** (C++). Jump from the airship, loot towns, build and edit, and be the last one standing against up to 79 bots.

**Play it:** grab the Windows zip from the [latest release](../../releases/latest) — unzip, run `Skydrop.exe`. The project page with screenshots and controls lives in [`docs/`](docs/index.html) (published with GitHub Pages).

## What's in here

| Folder | What it is |
|---|---|
| `Source/Skydrop/` | All the game code: world generator, building/editing, weapons, bots, storm, lobby UI, settings, audio |
| `Content/Skydrop/` | Materials, imported tree/rock/gun models, the island map |
| `Config/` | Engine and packaging settings |
| `Tools/` | Python scripts that create the materials and import models (run with the editor's `-run=pythonscript`) |
| `docs/` | The GitHub Pages site |

Most of the game is generated in code at runtime: the island, towns, interiors, furniture, sounds and music.

## Building from source

1. Install Unreal Engine 5.8 and Visual Studio 2022+ with the "Game development with C++" workload.
2. The UE5 mannequin isn't included here (Epic's template content). In the editor: **Add → Add Feature or Content Pack → Third Person**, so `Content/Characters/Mannequins` exists.
3. Right-click `Skydrop.uproject` → *Generate Visual Studio project files*, then build `SkydropEditor` (Development, Win64) and open the project.

## Credits

Third-party 3D models are from Sketchfab under CC-BY 4.0 — see [CREDITS.md](CREDITS.md). Character mannequin and animations: Unreal Engine template content (Epic Games).

Skydrop is a non-commercial fan project and is not affiliated with or endorsed by Epic Games.
