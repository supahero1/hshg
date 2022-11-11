# Hierarchical spatial hash grid

Based on [this paper](https://www10.cs.fau.de/publications/theses/2009/Schornbaum_SA_2009.pdf), this repository is my own 2D C implementation of a HSHG. The publication was initially used as a skeleton to then improve upon.

This HSHG implementation features:

### Ridiculously low memory requirements
For a max of 65,535 entities on a 128x128 grid with 128 cell size, one cell will use 2 bytes and one entity will take 24 bytes (includes `x`, `y`, and `r`), so in the worst case scenario, you will only need 1.6MB.

### No hidden costs[^1]
When you insert an object, a few allocations might occur, but no further memory will be requested or shrank after the insertion.

### Incredible versatility
The HSHG is suited for large areas thanks to mapping the infinite plane to a finite number of cells, but you may as well use it in setups in which entities are very dense and often clumped together in cells thanks to `O(1)` insertion, `O(1)` removal, and `hshg_optimize()`.

Not only that, but this is a **hierarchical** grid, so you may insert any sized entities to it. You only need to specify parameters for the first "tightest" grid - the underlying code will do the rest for you.

# Usage

## C/C++

`cd c`

To install the library, run `make && sudo make install`.

You can also run a benchmark with `./run.sh`. You don't need to build the library for that.

If you decide to use the library in your project, read `usage.md` first. It goes into more detail about how the HSHG actually works and how to use it from a developer's perspective.

## JavaScript

`cd js`

### Browser

`cd browser; npm i`

You can preview the HSHG running live with the help of [emscripten](https://emscripten.org/).

If you are satisfied with the precompiled WASM file, go ahead and simply do `./run.sh`. Otherwise, you will need to manually install emscripten and then do `./build.sh && ./run.sh`.

The file used, `hshg_wasm.c`, is almost an exact copy of `c/hshg_bench.c`, with some emscripten-specific additions.

WASM runs with about half the speed of the C equivalent.

### Node.js

`cd node; npm run install`

Read `js_usage.md` for more information.

# C Benchmarks

Only not overclocked CPUs, one core, with `glibc`. See `hshg_bench.c` to see the specifics of how this is carried out.

In a nutshell, the benchmark runs a simulation consisting of circles. Initially, they are moving in a random direction and are spaced out randomly. They collide with each other in real time.

The benchmark logs various pieces of information in the console, specifically:

- time spent inserting all of the entities, not counted towards any of the below timings,
- `upd` - the average of `hshg_update()` along with the standard deviation,
- `opt` - the average of `hshg_optimize()` and `balls_optimize()` (defined in the benchmark's file) along with the standard deviation (which will usually be really high - that's normal)
- `col` - the average of `hshg_collide()` along with the standard deviation,
- `all` - summed up averages from the 3 above, and what is shown in the results' table below.

All of the below results feature the following simulation settings:

|  Entities  | Entity size |   Cells   | Cell size |
| ---------- | ----------- | --------- | --------- |
|  500,000   |    14x14    | 2048x2048 |   16x16   |

These settings are also in `c/hshg_bench.c`.

The results:

|          CPU          | Insertion | Average |
| --------------------- | --------- | ------- |
| `i5 9600K`            |   27ms    |  14.4ms |
| `Intel Broadwell`     |   27ms    |  24.0ms |
| `Intel Skylake`       |   19ms    |  16.9ms |
| `Intel Xeon`          |   28ms    |  25.2ms |
| `AMD EPYC-Rome`       |   22ms    |  18.5ms |
| `i5 1135G7`           |   18ms    |  16.3ms |

---

[^1]: Except `hshg_optimize()`, which makes an allocation to speed up other parts of the code. The function is optional though.
