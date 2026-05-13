# Naraku Prototype Design

## Goal
First-layer prototype for testing the core loop: enter, explore, mine, decide whether to carry relics, return, sell, and re-enter.

## Project Policy
- Base project is copied from `C:\HAL\4-2after\DX22\Program\DX22_Project`.
- Keep DirectX, ImGui, Input, Texture, Model, Assimp, and DirectXTex initialization from the base project.
- Add the prototype as an isolated scene: `SceneNarakuProto`.
- Start directly from `SCENE_NARAKU_PROTO` while prototyping.
- Keyboard and mouse only for now. Controller support is deferred.

## Implementation Stages
1. Project setup and baseline build.
2. Add `SceneNarakuProto` and verify it boots.
3. Implement playable debug loop in ImGui top-down view.
4. Replace debug drawing with 2.5D/3D presentation after rules feel good.
5. Add controller mapping after keyboard behavior is stable.

## Prototype Systems
- Player movement: walk, run, step, jump, rope depth movement.
- Stamina: max 100, immediate recovery when not spending, cost scaling at high weight.
- Weight: pickaxe 10, relic 15, soft penalties at 70/90/100 percent, pickup cap at 150 percent.
- Upper-load: first-layer 10m cumulative ascent causes mental -10, gauge decays at 1m/sec when not ascending.
- Mining: 10 fixed points, 4 visual variants, same behavior.
- Relics: fourth-grade sale-only relics, fixed value 5, pickup prompt shows name and weight.
- Inventory: open, inspect, drop relics.
- Enemy: weak creature, telegraphed body charge, damage only during charge collision.
- Return/sell: return point interaction, result/sell screen, money update, re-enter.
- Death: HP 0 or lethal fall, carried relics and exploration pins lost, result screen.

## Current Debug Representation
The first implementation uses ImGui draw lists for a fast top-down debug field. This keeps asset work out of the way and lets the rules be tested before committing to 2.5D art and model placement.
