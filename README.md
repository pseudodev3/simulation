# Crime City Simulation

A procedural C++ city simulation built around one rule: **normal lives first, consequences later**.

Citizens begin with homes, jobs, money, needs, routines, personalities, friendships, stress and memories. Crime is intentionally absent for now. Later conflict should emerge from those systems rather than from a hard-coded plot.

## Core rule

We script **systems**, not outcomes.

The simulation may eventually produce a chain such as:

`late for work -> poor performance -> loses job -> money falls -> rent pressure -> stress rises -> relationship problems -> risky decisions`

No master script is allowed to say "make this person a criminal on day 20." Each subsystem only changes state and the next subsystem reacts to it.

## Autonomous episode loop

The production path is now `simulation_episode`.

Once started, it accepts **no runtime input**. A seed creates the world, C++ advances the entire neighborhood in deterministic 10-minute steps, the director scores events and decides what the audience should see, and the renderer turns that decision stream into a finished MP4.

```text
seed
  -> world simulation
  -> needs / jobs / money / relationships / routines
  -> structured events
  -> autonomous director
  -> camera target + shot duration + caption
  -> raylib frame renderer
  -> ffmpeg
  -> mason-block.mp4
```

The same seed + population + day count reproduces the same simulation and shot plan.

### What the director currently does

- stays wide during ordinary life
- creates establishing beats around morning, lunch, evening and night
- follows a resident when a meaningful event belongs to them
- holds longer on higher-importance events
- generates captions from the actual simulation event stream
- never changes the simulation state

The director observes the world. It does not cause the story.

## Build

Raylib is fetched automatically by CMake.

```bash
cmake -S . -B build
cmake --build build
```

FFmpeg must be available in `PATH` when rendering video.

## Generate an episode automatically

Default run: seed `20260916`, 30 residents, 7 simulated days.

```bash
./build/simulation_episode
```

Custom run:

```bash
./build/simulation_episode <seed> <population> <days> <output_dir>
./build/simulation_episode 48192 30 7 episode-001
```

Outputs:

```text
episode-001/
  episode.json      # autonomous shot plan + event stream
  timeline.txt      # readable story timeline
  mason-block.mp4   # finished video
```

Useful development flags:

```bash
./build/simulation_episode 48192 30 7 episode-001 --plan-only
./build/simulation_episode 48192 30 1 smoke --smoke
./build/simulation_episode 48192 30 7 episode-001 --keep-frames
```

`--plan-only` is the fastest way to inspect whether the autonomous simulation and director are behaving correctly before spending time rendering video.

## GitHub Actions episode render

The **Render autonomous episode** workflow can generate an MP4 entirely in GitHub Actions. Supply a seed, population and day count, then download the produced artifact containing the video, JSON plan and timeline.

## Current life systems

- assigned homes and workplaces
- staggered work shifts
- wages and daily living costs
- hunger, energy, stress and loneliness
- café, shop and park behavior
- evening plans derived from needs and personality
- repeated social contact and friendship formation
- persistent memories
- deterministic seeded runs
- structured event importance scoring
- autonomous episode direction

There is intentionally **no crime system yet**.

## Visual direction

Mason Block is a quiet miniature residential neighborhood rendered on a crisp 640x360 pixel canvas and scaled to 1280x720 for the final video. Residents move between simulated locations; day/night tinting, lit windows, streetlights, roads, homes, work buildings, the café, shop and park are all visualizations of the current world state.

`simulation_viewer` still exists, but it is debug tooling only. The YouTube content path is `simulation_episode`, which runs without human control once started.

## Next simulation milestones

1. physical movement: sidewalks, doors, routes and actual commuting time
2. groceries, food at home, possessions and household routines
3. lateness, work performance, warnings, layoffs and job searching
4. households, friendships, dating, arguments and grudges
5. bills, debt and meaningful material pressure
6. stronger director logic for recurring characters and multi-day story arcs
7. only then: crime, witnesses, police and consequences
