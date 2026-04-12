Liquid War 5
============

![Liquid War 5 icon](https://raw.githubusercontent.com/ufoot/liquidwar5/main/misc/liquidwar.png)

Liquid War is a unique multiplayer wargame. Its rules are
truely original and have been invented by Thomas Colcombet.
You control an army of liquid and have to try and eat your
opponents. A single player mode is available, but the game is
definitely designed to be multiplayer, and has network support.

* Liquid War homepage : https://ufoot.org/liquidwar/v5
* Contact author      : ufoot@ufoot.org

If you have any questions or remarks about Liquid War, you can
get help and informations on the Liquid War user mailing list:

* http://mail.nongnu.org/mailman/listinfo/liquidwar-user

Have a good day,

U-Foot

AI Fork
-------

This is an AI-enhanced fork of Liquid War 5. Changes from upstream:

- **Smarter AI opponents** — scored target selection based on enemy density, proximity, and health instead of random targeting
- **Periodic replanning** — AI adapts to battlefield changes every N ticks
- **Defensive retreat** — AI consolidates forces when losing fighters rapidly
- **Headless mode** (`-headless`) — runs games at max speed with no display for batch simulation
- **Reproducible seeds** (`-seed N`) — deterministic games for training
- **Configurable AI parameters** — tune AI behavior via command line
- **Battle data logging** — CSV logs of game state and AI decisions

Training pipeline: [liquidwar5-ai-training](https://github.com/pandora-wolf-meow/liquidwar5-ai-training)

### Headless Mode

Run a game with no display at max speed, outputting results as CSV:

```bash
./src/liquidwar -dat ./data/liquidwar.dat -headless -seed 42
```

Output:
```
result,winner,ticks,team0_fighters,...,ai_candidates,ai_density_weight,...
result,1,24000,3099,3221,48,284,202,496,10,5,50,100,50,20
```

Run 8 games in parallel (~7 seconds):
```bash
for i in $(seq 8); do
    ./src/liquidwar -dat ./data/liquidwar.dat -headless -seed $i &
done
wait
```

### AI Parameters

| Flag | Default | Description |
|------|---------|-------------|
| `-ai-candidates N` | 10 | Target candidates to evaluate per decision |
| `-ai-density-radius N` | 5 | Enemy density search radius (grid cells) |
| `-ai-density-weight N` | 50 | Weight for enemy concentration in scoring |
| `-ai-health-weight N` | 100 | Divisor for health factor in scoring |
| `-ai-replan N` | 50 | Ticks between forced path replanning |
| `-ai-retreat N` | 20 | Retreat if lost more than 1/N fighters |

### Build

```bash
sudo apt-get install -y build-essential autoconf automake liballegro4-dev
autoconf && ./configure && gmake
```

Status
------

[![Build](https://github.com/ufoot/liquidwar5/actions/workflows/build-full.yml/badge.svg)](https://github.com/ufoot/liquidwar5/actions)

Liquid War 5 is now (at least) 20 years old. Some files probably
remained unchanged through all those years but yet, apparently,
still compile and run. Don't trust the git log for activity,
this thing used to be on [Source Forge](https://sourceforge.net/projects/liquidwar/)
and even before that it was developped without any source control system, back in 1998.

Since 2005, [Liquid War 6](https://www.gnu.org/software/liquidwar6)
has been released, trying to get a better interfaces and technology
to that program.

It happens Liquid War 5 is still the de facto best implementation
around, usually available for major Linux distros, and Windows or
Mac binaries usually work. Your mileage may vary, I'm definitely
not actively developping this, however I'm still experimenting on new
Liquid War ideas, I even have a `liquidwar7` folder somewhere on
my laptop where I'm hacking random things.

My overall advice would be to use this game and have fun with it,
but if you want to hack around, there might be better things to
do than touching this old and dusty code base. Please
[contact me](mailto:ufoot@ufoot.org) if you want to know more.

Install
-------

Get files [here](https://ufoot.org/liquidwar/v5/download) for Windows or Mac users.
For GNU/Linux users, most of the time, something like this is enough:

```
sudo apt-get install liquidwar   # .deb distro (Debian, Ubuntu, ...)
sudo yum install liquidwar       # .rpm distro (Fedora, ...)
```

Documentation
-------------

* [user documentation](https://ufoot.org/liquidwar/v5/doc)
* [code algorithm](https://ufoot.org/liquidwar/v5/techinfo/algorithm)
* [source code](https://ufoot.org/liquidwar/v5/techinfo/source)

License
-------

Liquid War is a multiplayer wargame.
Copyright (C) 1998-2025 Christian Mauduit (ufoot@ufoot.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
