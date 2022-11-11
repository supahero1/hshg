# Using the HSHG

## Behind the scenes

This section does not document API functions, however you should read this before trying to code with the HSHG.

### Grids

A Hierarchical Spatial Hash Grid, as the name suggests, keeps track of multiple grids that are ordered in a hierarchy, from the "tightest" grid (the most number of cells, the smallest cells) to the "loosest" one (the least number of cells, the biggest cells). In this specific implementation of a HSHG, all grids must have a cell size that's a power of 2, as well as a number of cells on one side being a power of 2. Thus, every grid is a square, with square cells, and subsequent loose grids are created by taking the previous loosest grid, dividing its number of cells on one side by a power of 2 and multiplying its cell size by the same power of 2. The new grid is then of the same size as the old one, however it has fewer cells, with the cells being bigger. The division factor can be manipulated via `hshg.cell_div_log`, with the default value being `1`, corresponding to a division factor of `2`. If it were to have a value of `2`, the division factor would be `4`, and so on (a `2 ** n` relation).

You can also resize entities in the update function (`hshg.update`, called in `hshg_update()` for every entity) via `hshg_resize()`. The function does not accept failure, so it will always expect the new entity, no matter if larger or smaller, to fit in the existing grids. If the entity got bigger and there isn't a grid that can fit it, the program will abort. If you want to use this functionality, you must call `hshg_prealloc(&structure, radius_of_biggest_entity)` after calling `hshg_init()` and before calling `hshg_resize()`. That function will generate enough grids to fit an entity of size `radius_of_biggest_entity * 2`. Subsequent insertions will also benefit from this, because they won't have to create more grids on the fly as new entities come in.

### Entities

A new entity is inserted to a grid that fits the entity entirely in one cell, and so that the previous grid's cell size would be too small for the entity (to not insert entities to oversized cells). Due to this, only one copy of the entity is needed over its whole lifetime, and it only occupies one cell in one grid, unlike other structures like a non-hierarchical grid structure, or a QuadTree, in which case you need to be varied of duplicates in leaf nodes and so. When an entity that's bigger than the top grid's cell size is inserted, the underlying code in response allocates more grids, as many as necessary to fit the new entity, with the method described in the paragraphs above.

All entities are stored in one big array that's dynamically resized if needed. By default, if it needs resizing in order to fit a new entity, the underlying code will double the current array size. This might be a pretty universal solution, however it might not be feasible for some use cases. If you need precise control of the array's size, consider using `hshg_set_size()` before using `hshg_insert()`. Additionally, if entities are removed, the array is not shrank to a smaller size. If you need to do that, you can use `hshg_set_size(&structure, structure.entities_used)`, which will eliminate most of the unused space in the array.

Entities may only be altered in `hshg_update()` (`hshg.update`). In all other callbacks, they are read-only. To apply velocity changes that happen during collision or any other changes, you must offload them to some external object that's linked to the HSHG's internal representation of an entity. To do that, insert an entity with its `ref` member set to something that lets you point to the object that the entity owns (see `c/hshg_bench.c` for an example). This can be done with a simple array holding objects and `ref` being an integer index. You can then access `ref` from within the `hshg.update` callback. Trying to change HSHG's entities while in `hshg.collide` or `hshg.query` will lead to unwanted results (like inaccurate collision).

This implementation of a HSHG maps the infinite plane to a finite number of cells - entities don't need to always stay on top of the HSHG's area (from `(0, 0)` to whatever `(cells * cell_size, cells * cell_size)` is), they can be somewhere entirely else. However, refrain from doing something like this:

```
                    0                1                2
                  2 ----------------------------------- 2
                    |                |                |
                    |                |                |
                    |                |                |
                    |                |                |
                    |   HSHGs area   |   HSHGs area   |
  -1                |                |                |
 1 ---------------------------------------------------- 1
   |                |                |                |
   |   Your arena   |   Your arena   |                |
   |                |                |                |
   |                |                |                |
   |                |   HSHGs area   |   HSHGs area   |
   |                |                |                |
 0 ----------------0,0--------------------------------- 0
   |                |                |                2
   |   Your arena   |   Your arena   |
   |                |                |
   |                |                |
   |                |                |
   |                |                |
-1 ----------------------------------- -1
  -1                0                1
```

This will result in performance slightly worse than if the HSHG was 4 times smaller in size (one of the cells instead of all 4), because ALL of your arena cells will be mapped to the same exact HSHG cell. If you don't know how this implementation of HSHGs folds the XOY plane to grids, you should just stick to **one** of the four XOY quadrants (as in, maybe make all positions positive instead of having any negatives).

### Tuning

This HSHG has lots of moving parts. To make sure you get the best performance, you need to keep trying various numbers for various things and see what works the best for your setup.

## API

Always begin by zeroing the structure. The only necessary members to set are `hshg.update` (used in `hshg_update()`) and `hshg.collide` (used in `hshg_collide()`). If you plan on using `hshg_query()`, you must also set `hshg.query`.

`hshg_init()` accepts parameters used for creating the first, "tightest" grid:

```c
struct hshg hshg = {0};

hshg.update = (hshg_update_t) some_update_func;
hshg.collide = (hshg_collide_t) some_collide_func;
hshg.query = (hshg_query_t) some_query_func;

/* BOTH OF THE BELOW MUST BE POWERS OF 2! */

/* The number of cells on one side of the first,
"tightest" grid (most cells on axis, least cell size).
Square that, and you get the total number of cells. */
const hshg_cell_t cells_on_axis = 64;
/* Side length of one cell in the first, "tightest"
grid. Generally, you will want to make this be as
close to the smallest entity's radius as possible. */
const uint32_t cell_size = 16;

int err = hshg_init(&hshg, cells_on_axis, cell_size);
if(err) {
  /* Out of memory */
}
```

Once you're done using the structure, you can call `hshg_free(&hshg)` to free it. If you plan on initialising the structure again, make sure to zero it first.

All entities have an AABB which is a square. Once collision is detected, it's up to you to provide an algorithm that checks for collision more precisely, if needed. That's why `struct hshg_entity` only has a radius, no width or height.

Insertion:

```c
const hshg_pos_t x = 0.12;
const hshg_pos_t y = 3.45;
const hshg_pos_t r = 6.78;
const hshg_entity_t ref = 0; /* Not needed for now */
int err = hshg_insert(&hshg, x, y, r, ref);
if(err) {
  /* Out of memory */
}
```

No identifier for the entity is returned from `hshg_insert()`, but one is required for removing entities from the HSHG via `hshg_remove()`. In other words, you may only remove entities from the update callback. However, you may ask how you are supposed to do that, without attaching any metadata to the entity when it's inserted.

That's what the `ref` variable mentioned above achieves - it lets you attach a piece of data (generally an index to a larger array containing lots of data that an entity needs) to the entities you insert. To begin with, you can create an array of data per entity you will want to use:

```c
struct my_entity {
  hshg_pos_t vx;
  hshg_pos_t vy;
  int remove_me;
};

#define ABC /* some number */

struct my_entity entities[ABC];
```

Now, really, you can do a load of stuff with this to suit it to your needs, like an `hshg_insert()` wrapper that also populates the chosen `entities[]` spot, and a method for actually choosing a spot in the array. I'd like to keep it simple, so I will go for a rather dumb solution:

```c
hshg_entity_t entities_len = 0;

int my_insert(struct hshg_entity* ent, struct my_entity* my_ent) {
  entities[entities_len] = *my_ent;
  const int ret = hshg_insert(&hshg, ent->x, ent->y, ent->r, entities_len);
  ++entities_len;
  return ret;
}
```

Now, from any callback that involves `struct hshg_entity`, you will also be given the number kept in `ref`, that will then allow you to access that specific spot in the array `entities`. Then, you can implement the process of deleting an entity like so:


```c
void update(struct hshg* hshg, struct hshg_entity* entity) {
  struct my_entity* my_ent = entities + entity->ref;

  if(my_ent->remove_me) {
    hshg_remove(hshg);
    return;
  }

  entity->x += my_ent->vx;
  entity->y += my_ent->vy;
}

struct hshg hshg = {0};
hshg.update = update;
assert(!hshg_init(&hshg, 1, 1));

/* sometime, set entities[idx].remove_me to 1 */
```

The update callback above can delete entities, and seems to update their position, however the underlying code doesn't actually know the position was updated after you return from the callback. To fix that, call `hshg_move()`, generally at the end of the callback:

```c
void update(struct hshg* hshg, struct hshg_entity* entity) {
  struct my_entity* my_ent = entities + entity->ref;

  if(my_ent->remove_me) {
    hshg_remove(hshg);
    return;
  }

  entity->x += my_ent->vx;
  entity->y += my_ent->vy;

  hshg_move(hshg); /* !!! */
}
```

You don't need to call `hshg_move()` every single time you change `x` or `y` - you may as well call it once at the end of the update callback.

If you are updating the entity's radius too (`entity->r`), you must also call `hshg_resize(hshg)`. It works pretty much like `hshg_move()`. If you need to call both, it does not matter in what order you do so.

If you move or update an entity's radius without calling the respective functions at the end of the update callback, collision will not be accurate, query will not return the right entities, stuff will break, and the world is going to end.

To call `hshg_resize()`, you must call `hshg_prealloc()` first, see above.

If you call `hshg_insert()` from `hshg.update`, the newly inserted entity **might or might not** be updated during the same `hshg_update()` function call. If you need to eliminate this possibility, keep track of a tick number or so, to make sure you don't update entities that were created in the same tick, and then spare one bit for knowing whether or not an entity was just created or not.

Note that `hshg_remove()` may **only** be called from `hshg.update()`. Same goes for `hshg_move()` and `hshg_resize()`. These functions do not accept any arguments on purpose, because they remove the currently examined entity that `hshg.update()` is called on.

`hshg_update(&hshg)` goes through all entities and calls `hshg.update` on them. You may not call this function recursively from its callback, nor can you call `hshg_optimize()` and `hshg_collide()`. You are allowed to call `hshg_query()`, however note that you must do so **after** you update positions of entities, which generally will require you to do two `hshg_update()`'s:

```c
void update(struct hshg* hshg, struct hshg_entity* entity) {
  struct my_entity* my_ent = entities + entity->ref;

  if(my_ent->remove_me) {
    hshg_remove(hshg);
    return;
  }

  entity->x += my_ent->vx;
  entity->y += my_ent->vy;

  hshg_move(hshg);
}

const struct hshg_entity* queried_entities[ABC];
hshg_entity_t query_len = 0;

void query(const struct hshg* hshg, const struct hshg_entity* entity) {
  queried_entities[query_len++] = entity;
}

void update_with_query(struct hshg* hshg, struct hshg_entity* entity) {
  struct my_entity* my_ent = entities + entity->ref;
  
  query_len = 0;
  hshg_query(hshg, minx, miny, maxx, maxy);

  /* do something with the query data stored in queried_entities[] */
}

struct hshg hshg = {0};
hshg.query = query;
assert(!hshg_init(&hshg, 1, 1));

void tick(void) {
  hshg.update = update;
  hshg_update(&hshg);
  hshg.update = update_with_query;
  hshg_update(&hshg);
  /* maybe also collide, etc */
}
```

This sequentiality is required for most projects that want accurate *things*. If you mix updating an entity with viewing an entity's state, all weird sorts of things can happen. Mostly it will be harmless, perhaps minimal visual bugs on the edges of the screen due to incorrect data fetched by `hshg_query()`, but if you don't want that minimal incorrectness (and you probably don't), then separate the concept of modifying `struct hshg_entity` from viewing it. For instance, **NEVER** update an entity in `hshg_collide()`'s callback. Because the next entity the function goes to will see something else than what the previous entity saw.

`hshg_optimize(&hshg)` reallocates all entities and changes the order they are in so that they appear in the most cache friendly way possible. This process insanely speeds up the next call to `hshg_collide(&hshg)` and a little bit `hshg_query(&hshg, ...)`. It is most effective when entities in the HSHG are clumped. Moreover, for the duration of the function, the memory usage will nearly double, so if you can't have that, don't use the function. It generally helps most when there are really a lot of entities, counting in hundreds of thousands.

```c
int err = hshg_optimize(&hshg);
if(err) {
  /* Out of memory */
}
```

Don't ever call this function *every single tick*. Instead, have a tick counting variable, say, `int tick = 0`, and call `hshg_optimize()` every 64 ticks or so, with `if(tick % 64 == 0) do_it()`. This is one of the places you will need to experiment with for a few minutes, because while 64 is a good "starting" value, your environment might require a different one. Check what works best for you via benchmarking. Generally, the faster the entities in your simulation move, the lower the number above will need to be to be effective.

`hshg_collide(&hshg)` goes through all entities and detects loose collision between square AABBs of the entities. It is your responsibility to detect the collision with more detail in the `hshg.collide` callback, if necessary. A sample callback for simple circle collision might look like so:

```c
void collide(const struct hshg* hshg, const struct hshg_entity* a, const struct hshg_entity* b) {
  (void) hshg;
  const float xd = a->x - b->x;
  const float yd = a->y - b->y;
  const float d = xd * xd + yd * yd;
  if(d <= (a->r + b->r) * (a->r + b->r)) { /* if distance less than sum of radiuses */
    const float angle = atan2f(yd, xd);
    const float c = cosf(angle);
    const float s = sinf(angle);
    objs[a->ref].vx += c;
    objs[a->ref].vy += s;
    objs[b->ref].vx -= c;
    objs[b->ref].vy -= s;
  }
}

hshg.collide = collide;
```

From this callback, you may not call `hshg_update()` or `hshg_optimize()`.

`hshg_query(&hshg, min_x, min_y, max_x, max_y)` calls `hshg.query` on every entity that belongs to the rectangular area from `(min_x, min_y)` to `(max_x, max_y)`. It is important that the second and third arguments are smaller or equal to fourth and fifth.

```c
void query(const struct hshg* hshg, const struct hshg_entity* a) {
  draw_a_circle(a->x, a->y, a->r);
}

hshg.query = query;
const hshg_cell_t cells = 128;
const uint32_t cell_size = 128;
assert(!hshg_init(&hshg, cells, cell_size));

/* This is not equal to querying every entity, because
entities don't need to lay within the area of the HSHG
to be properly mapped to cells - they can exist millions
of units away. If you want to limit them to this range,
impose limits on their position in the update callback. */
const uint32_t total_size = cells * cell_size;
hshg_query(&hshg, 0, 0, total_size, total_size);
```

You may not call any of `hshg_update()`, `hshg_optimize()`, or `hshg_collide()` from this callback.

Summing up all of the above, a normal update tick would look like so:

```c
hshg_update(&hshg);
if(tick_counter % 64 == 0) {
  assert(!hshg_optimize(&hshg)); /* no "no mem" */
}
/* hshg_query(&hshg, ...); (can be either here or below) */
hshg_collide(&hshg);
/* hshg_query(&hshg, ...); (can be either here or above) */
++tick_counter;
```

For a complete example, see `c/hshg_bench.c` and `js/browser/hshg_wasm.c`.
