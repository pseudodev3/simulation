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

## Build

```bash
cmake -S . -B build
cmake --build build
./build/simulation
```

Optional arguments:

```bash
./build/simulation <seed> <population> <days>
./build/simulation 20260916 50 14
```

Using the same seed and settings should reproduce the same run.

## Direction

Next milestones:

1. visual top-down neighborhood renderer
2. richer routines: groceries, food at home, commuting, days off
3. employment performance, lateness, layoffs and job searching
4. households, friendships, dating, arguments and grudges
5. bills, debt and meaningful material pressure
6. event director that automatically follows emerging stories
7. only then: crime, witnesses, police and consequences

The renderer and future YouTube camera system should consume the same structured event stream rather than controlling the simulation.
