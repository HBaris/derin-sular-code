# Derin Sular — gameplay code (Unreal Engine 5 / C++)

**Derin Sular** is a co-op survival game I'm building in Unreal Engine 5. This repository contains the **C++ gameplay source and config**. Art, audio and level assets are not included: they are binary, large, and some are third-party licensed.

## Systems

- **Data-driven wave system** (`DSWaveManager`). Waves, enemy mixes and timings come from data, so balancing doesn't need code changes.
- **Enemy types** (`DSZombiTypes`). Each type has its own stats and behaviour, driven by **StateTree AI**.
- **World state subsystem** (`DSWorldStateSubsystem`). Game-wide state lives in a subsystem instead of the level.
- **Melee weapons** (`DSMeleeWeapon`) and a custom character body rig.
- **Delegate-based events** decouple gameplay systems from UI and audio.

## Layout

```
Source/DerinSular/   C++ gameplay code (game mode, character, player controller, waves, enemies, weapons)
Config/              project and input config
*.md                 design notes (Turkish): ROADMAP, LEVEL_TASARIMI (level design), ZOMBI_TIPLERI (enemy types)
```

Assets are tracked with Git LFS in the private working repository.
