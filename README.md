# Crime City Simulation

A procedural C++ city simulation built around one rule: **normal lives first, consequences later**.

The goal is not to spawn crime randomly. Citizens begin with ordinary routines, jobs, homes, money, needs, friendships, stress and habits. Later conflict should emerge from those systems so the story feels earned.

## Phase 0 — life before crime

The current foundation models:

- assigned homes and workplaces
- staggered work shifts
- wages and daily living costs
- hunger, energy, stress and loneliness
- cafés, shops and a park
- evening plans chosen from current needs and personality
- repeated social contact and friendship formation
- persistent memories
- structured events with importance scores
- deterministic seeded runs

There is intentionally **no crime system yet**.

The simulation should eventually produce chains like:

`late for work -> poor performance -> loses job -> money falls -> rent pressure -> stress rises -> relationship problems -> risky decisions`

No single system should know that entire story. Each system only changes state, and the next system reacts to it.

## Mason Block visual prototype

`simulation_viewer` is the first graphical layer. It uses raylib and renders the existing simulation as a quiet, miniature residential block rather than changing the simulation rules.

The viewer currently includes:

- 640x360 internal pixel canvas scaled crisply to 1280x720
- muted lawns, houses, workplaces, café, shop, park and roads
- tiny residents that visibly travel when their simulation location changes
- dawn, daylight, dusk and night tinting
- house/business windows and streetlights at night
- a small bus stop and neighborhood props
- resident selection with cash, needs and current activity
- recent structured-event feed
- pause and simulation speed controls

The renderer is deliberately separate from `simulation_core`: the simulation can still run headless at high speed later, while the visual layer can replay or follow interesting periods for YouTube.

### Viewer controls

- `Space` — pause/resume
- `1` / `2` / `3` — 1x / 3x / 8x simulation speed
- `N` — advance one 10-minute simulation step
- `Tab` — toggle place labels
- click a resident — inspect their current life state

## Build

Raylib is fetched automatically by CMake for the viewer.

```bash
cmake -S . -B build
cmake --build build
```

Run the quiet-neighborhood viewer:

```bash
./build/simulation_viewer
```

Optional seed and population:

```bash
./build/simulation_viewer 20260916 30
```

Run the headless simulation:

```bash
./build/simulation
./build/simulation <seed> <population> <days>
./build/simulation 20260916 50 14
```

Using the same seed and settings should reproduce the same simulation run.

To build only the headless core without raylib:

```bash
cmake -S . -B build -DSIMULATION_BUILD_VIEWER=OFF
cmake --build build
```

## Direction

Next milestones:

1. make the neighborhood itself more physical: sidewalks, door/room entry, groceries and objects carried home
2. commuting, lateness and days off
3. employment performance, warnings, layoffs and job searching
4. households, friendships, dating, arguments and grudges
5. bills, debt and meaningful material pressure
6. event director that automatically follows emerging stories
7. only then: crime, witnesses, police and consequences

The renderer and future YouTube camera system should consume the same world state and structured event stream rather than controlling the simulation.
