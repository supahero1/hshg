# Hierarchical spatial hash grid

Based on [this paper](https://www10.cs.fau.de/publications/theses/2009/Schornbaum_SA_2009.pdf), this repository is my own implementation of a HSHG for 1D, 2D, and 3D, easily expandable to higher dimensions. The paper was initially used as a skeleton to then improve upon. As of now, the implementation highly differs.

Most of the documentation only refers to the 2D version. Other dimensions don't differ highly. To learn how to use other dimensions, read the usage file in the respective language section below.

This HSHG implementation features:

### Ridiculously low memory requirements
For a max of 65,535 entities on a 128x128 grid, one cell will use 2 bytes and one entity will take 24 bytes (includes `x`, `y`, and `r`), so in the worst case scenario, you will only need 1.6MB.

### No hidden costs[^1]
When you insert an object, a few allocations might occur, but no further memory will be requested or shrank after the insertion.

### Incredible versatility
The HSHG is suited for large areas thanks to mapping the infinite plane to a finite number of cells, but you may as well use it in setups in which entities are very dense and often clumped together in cells thanks to `O(1)` insertion, `O(1)` removal, and `hshg_optimize()`.

Not only that, but this is a **hierarchical** grid, so you may insert any sized entities to it. You only need to specify parameters for the first "tightest" grid - the underlying code will do the rest for you.

# Usage

## C/C++

`cd c`

You can test the code via `make test`. You can also benchmark it via `make bench`. By default, that will benchmark the 2D version, but you can change that by supplying a `DIMENSIONS=` argument like so: `DIMENSIONS=3 make bench`.

There's no libraries. Include the code in your project and build it there.

If you do decide to use it, read [the usage](c/README.md).

## JavaScript/WASM

`cd wasm`

If you want easy to use JavaScript bindings (via WASM), that's where you will get them.

Run `npm i; ./run.sh` for a graphical preview and some timings in the console. You can easily modify the constants used in the simulation by editing the top few lines of `test.js`.

WASM slows down about twice compared to native code, and the JavaScript bindings subsequently decrease performance by about 2.5. If you want high performance, you should use C directly for backend, or [emscripten](https://emscripten.org/) for browser (for "only" ~2 times slower code).

Read [the usage](wasm/README.md) to learn how to use the JavaScript bindings.

# C Benchmarks

Only not overclocked CPUs, one core, with `glibc`. See [the bench file](c/bench.c) to see the specifics of how this is carried out.

In a nutshell, the benchmark runs a simulation consisting of circles. Initially, they are moving in a random direction and are spaced out randomly. They collide with each other in real time.

The benchmark logs various pieces of information in the console, specifically:

- time spent inserting all of the entities, not counted towards any of the below timings,
- `opt` - the average of `hshg_optimize()` and `balls_optimize()` (defined in the benchmark's file),
- `col` - the average of `hshg_collide()` along with the standard deviation,
- `qry` - the average of 63 calls to `hshg_query()` with a 1920x1080 area of coverage on random entities each tick,
- `upd` - the average of `hshg_update()` along with the standard deviation,
- `all` - summed up averages from the 3 above, and what is shown in the results' table below,
- `attempted` fine collision checks (which equals broad collision checks),
- `succeeded` fine collision checks (resolved by applying forces).

Additionally, collision and update timings include percentage of samples being below certain deviation to denote how stable the computation is.

All of the below results feature the following simulation settings:

|  Entities  | Entity size |   Cells   | Cell size |
| ---------- | ----------- | --------- | --------- |
|  500,000   |    14x14    | 2048x2048 |   16x16   |

The simulation looks like so (a tiny part of it):

![Simulation preview](img/preview.png)

The results:

|          CPU          | Insertion | Average |
| --------------------- | --------- | ------- |
| `i5 9600K`            |   23ms    |  21.0ms |
| `Intel Xeon`          |   57ms    |  28.5ms |
| `Intel Skylake`       |   31ms    |  19.9ms |
| `Intel Broadwell`     |   27ms    |  24.0ms |
| `AMD EPYC-Rome`       |   29ms    |  23.6ms |
| `i5 1135G7`           |   21ms    |  18.6ms |

You can speed up the code via profiling. For instance, on `AMD EPYC-Rome`, code compiled with `-fprofile-arcs` and then `-fprofile-use` performs 11% better, with both runs having totally different simulation, since the starting conditions are random.

# The future

In a previous commit this section was filled with comments on things to do. However, this project will no longer be maintained, and I won't create an offspring as I said I would.

Truth be told, while HSHG's performance is really amazing and stable, it weakens the more grids there are (the more versatile the object sizes are) more or less twice per one grid, and also queries are kind of slow (some 30 times slower than a Quadtree).

While HSHG certainly has its uses, and even more so for uniform entity sizes, for fast queries and good support for pretty much any entity size a Quadtree is better. Maybe slower than baseline HSHG, but quickly becomes faster in aforementioned situations.

I've written an optimized Quadtree in a separate project. It does not have good documentation and support like this project, but if you were interested, you could take a look. If at all, I would prefer to update that than HSHG.

[^1]: Except `hshg_optimize()`, which makes an allocation to speed up other parts of the code. The function is optional though.
