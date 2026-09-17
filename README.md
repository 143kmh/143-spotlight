# 143 Spotlight

Gaming-first replay capture and clip workflow for OBS Studio.

143 Spotlight turns OBS Replay Buffer into a focused local clipper: quick buffer controls, a simplified Replay Mode, game-aware clip organization, fast trim, and size-targeted Discord export without requiring a separate capture stack.

## Current v0.1 scaffold

- Native OBS dock via `obs_frontend_add_dock_by_id`
- Start / stop Replay Buffer
- Save replay
- Last saved clip display
- Replay Mode that temporarily hides other OBS docks and restores them later
- Resolution presets: Native / 1440p / 1080p / 720p
- FPS presets: 30 / 60 / 120
- Replay length: 30–180 seconds
- Performance / Balanced / Quality UI preset stored per OBS profile
- Optional automatic Replay Buffer start after OBS finishes loading
- Settings persist per OBS profile

## Target workflow

`Start game -> Spotlight detects it -> Replay Buffer runs -> Save clip -> Trim -> Export <=50 MB / <=200 MB -> Discord`

## Roadmap

1. Map Performance / Balanced / Quality safely to NVENC, AMF and QSV.
2. Game process detection and a friendly game picker.
3. Clip index + local library grouped by game.
4. Fast timeline trim.
5. Discord export targets (`<= 50 MB` / `<= 200 MB`) through FFmpeg and hardware encoding.
6. Windows tray/startup/shutdown helper.
7. Optional OBS 31.x launcher integration for `--disable-shutdown-check`.

## Baseline

The first development target is OBS Studio 31.1.x on Windows x64. The template dependencies currently build against OBS 31.1.1; compatibility with 31.1.2 is a target for local testing. Newer OBS versions remain a compatibility target as the project evolves.
