# Hierarchical spatial hash grid

Based on [this paper](https://www10.cs.fau.de/publications/theses/2009/Schornbaum_SA_2009.pdf), this repository is my own 2D C implementation of a HSHG. The publication was initially used as a skeleton to then improve upon.

This HSHG implementation features:

### Ridiculously low memory requirements
For a max of 65,535 objects on a 128x128 grid with 128 cell size, one cell will use 2 bytes and one object will take 24 bytes (includes `x`, `y`, and `r`), so in the worst case scenario, you will only need 1.6MB.

### No hidden costs[^1]
When you insert an object, a few allocations might occur, but no further memory will be requested or shrank after the insertion.

### Incredible versatility
The HSHG is suited for large areas thanks to mapping the infinite plane to a finite number of cells, but you may as well use it in setups in which objects are very dense and often clumped together in cells thanks to `O(1)` insertion, `O(1)` removal, and `hshg_optimize()`.

Not only that, but this is a **hierarchical** grid, so you may insert any sized objects to it. You only need to specify parameters for the first "tightest" grid - the underlying code will do the rest for you.

## Codebase

There's no `cmake` or `Makefile` files to build this project as a library. It's probably best if you simply include `hshg.c` and `hshg.h` in your project and build them there. However, you can build and run the test suite `hshg_test.c` with `./run.sh` and the benchmark `hshg_bench.c` with `./bench.sh`.

If you do decide to use this project, you can read about how to use the underlying C functions in `usage.md`.

## JavaScript

You can preview the HSHG running live with the help of [emscripten](https://emscripten.org/).

If you are satisfied with the precompiled WASM file under the `js` directory, go ahead and simply run `cd js; npm i && ./run.sh`. Otherwise, you can do `cd js; npm i && ./build.sh && ./run.sh`.

## C Benchmarks

Only not overclocked CPUs, one core, with `glibc`. See `hshg_bench.c` to see the specifics of how this is carried out.

In a nutshell, the benchmark runs a simulation consisting of circles. Initially, they are moving in a random direction and are spaced out randomly. They collide with each other in real time.

The benchmark logs various timings in the console, specifically `upd` (`hshg_update()`), `opt` (`hshg_optimize()`), and `col` (`hshg_collide()`). All of that is summed in `all`, and that is what is shown below.

|          CPU          |  Entities  | Entity radius | Cells[^2] | Cell size | Ins 1[^3] | Ins 2[^4] | Avg 1[^5] | Avg 2[^6] |
| --------------------- | ---------- | ------------- | --------- | --------- | --------- | --------- | --------- | --------- |
|     `i5 9600K`[^7]    |  500,000   |       7       |   2048    |    256    |   31ms    |   24ms    |  21.1ms   |  18.3ms   |
|   `Intel Broadwell`   |  300,000   |       7       |   1024    |    256    |   40ms    |   30ms    |  27.9ms   |  24.0ms   |
|    `Intel Skylake`    |  500,000   |       7       |   2048    |    256    |   41ms    |   33ms    |  19.6ms   |  16.9ms   |
|     `Intel Xeon`      |  500,000   |       7       |   2048    |    256    |   53ms    |   48ms    |  28.7ms   |  25.2ms   |
|    `AMD EPYC-Rome`    |  500,000   |       7       |   2048    |    256    |   38ms    |   33ms    |  22.5ms   |  18.5ms   |
|    `i5 1135G7`[^8]    |  500,000   |       7       |   2048    |    256    |   31ms    |   24ms    |  18.1ms   |  16.3ms   |

---

[^1]: Except `hshg_optimize()`, which makes an allocation to speed up other parts of the code. The function is optional though.

[^2]: The number of cells on an axis, not the total number of cells in the smallest grid.

[^3]: Insertion, compiled with `-O3 -march=native`.

[^4]: Insertion, compiled with `-O3 -march=native -fprofile-use`.

[^5]: Update + optimize + collide (one tick), compiled with `-O3 -march=native`.

[^6]: Update + optimize + collide (one tick), compiled with `-O3 -march=native -fprofile-use`.

[^7]: With this setup of the simulation, in 500 ticks, on average, there are 133 million collision checks, 73,500 of which result in a collision.

[^8]: A laptop with performance mode on.
