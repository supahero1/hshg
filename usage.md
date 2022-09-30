# Using the HSHG

## Behind the scenes

This section does not document API functions, however you should read this before trying to code with the HSHG.

### Grids

A Hierarchical Spatial Hash Grid, as the name suggests, keeps track of multiple grids that are ordered in a hierarchy, from the "tightest" grid (the most number of cells, the smallest cells) to the "loosest" one (the least number of cells, the biggest cells). In this specific implementation of a HSHG, all grids must have a cell size that's a power of 2, as well as a number of cells on one side being a power of 2. Thus, every grid is a square, with square cells, and subsequent loose grids are created by taking the previous loosest grid, dividing its number of cells on one side by a power of 2 and multiplying its cell size by the same power of 2. The new grid is then of the same size as the old one, however it has fewer cells, with the cells being bigger. The division factor can be manipulated via `hshg.cell_div_log`, with the default value being `1`, corresponding to a division factor of `2`. If it were to have a value of `2`, the division factor would be `4`.

You can also resize entities in the update function (`hshg.update`, called in `hshg_update()` for every entity) via `hshg_resize()`. The function does not accept failure, so it will always expect the new entity, no matter if larger or smaller, to fit in the existing grids. If the entity got bigger and there isn't a grid that can fit it, the program will abort. If you want to use this functionality, you must call `hshg_prealloc(&structure, radius_of_biggest_entity)` after calling `hshg_init()` and before calling `hshg_resize()`. That function will generate enough grids to fit an entity of size `radius_of_biggest_entity * 2`. Subsequent insertions will also benefit from this, because they won't have to create more grids on the fly as new entities come in.

### Entities

A new entity is inserted to a grid that fits the entity entirely in one cell, and so that the previous grid's cell size would be too small for the entity (to not insert entities to oversized cells). Due to this, only one copy of the entity is needed over its whole lifetime, and it only occupies one cell in one grid, unlike other structures like a non-hierarchical grid structure, or a QuadTree, in which case you need to be varied of duplicates in leaf nodes and so. When an entity that's bigger than the top grid's cell size is inserted, the underlying code in response allocates more grids, as many as necessary to fit the new entity, with the method described in the paragraph above.

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

This will result in performance slightly worse than if the HSHG was 4 times smaller in size (one of the cells instead of all 4), because ALL of your arena cells will be mapped to the same exact HSHG cell. If you don't know how this implementation of HSHGs folds the XOY plane to grids, you should just stick to **one** of the four XOY quadrants.

### Tuning

This HSHG has lots of moving parts. To make sure you get the best performance, you need to keep trying various numbers for various things and see what works the best for your setup.

## API

Always begin by zeroing the structure. The only necessary members to set are `hshg.update` (used in `hshg_update()`) and `hshg.collide` (used in `hshg_collide()`). If you plan on using `hshg_query()`, you must also set `hshg.query`.

`hshg_init()` accepts parameters used for creating the first, "tightest" grid:

```c
struct hshg hshg = {0};

hshg.update = (void (*)(struct hshg*, hshg_entity_t id));
hshg.collide = (void (*)(const struct hshg*, const struct hshg_entity* ent_a, const struct hshg_entity* ent_b));
hshg.query = (void (*)(const struct hshg*, const struct hshg_entity* ent));

/* BOTH OF THE BELOW MUST BE POWERS OF 2! */

/* The number of cells on one side of the first,
"tightest" grid (most cells on axis, least cell size).
Square that, and you get the total number of cells. */
const hshg_cell_t cells_on_axis = 64;
/* Side length of one cell in the first, "tightest"
grid. You will want to make this be maybe 2 or 4
times bigger than the smallest entity's size. */
const uint32_t cell_size = 32;

int err = hshg_init(&hshg, cells_on_axis, cell_size);
if(err) {
  /* Out of memory */
}
```

Once you're done using the structure, you can call `hshg_free(&hshg)` to free it. If you plan on initialising the structure again, make sure to zero it first.

All entities have an AABB which is a square. Once collision is detected, it's up to you to provide an algorithm that checks for collision more precisely, if needed. That's why `struct hshg_entity` only has a radius, no width or height.

Insertion:

```c
struct my_obj {
  hshg_pos_t vx;
  hshg_pos_t vy;
  int remove;
};

struct my_obj objs[100] = {0};

int err = hshg_insert(&hshg, &((struct hshg_entity) {
  .x = 0.12,
  .y = 3.45,
  .r = 6.78,
  .ref = 9 /* An index for the "objs" array */
}));
if(err) {
  /* Out of memory */
}
```

No identifier for the entity is returned from `hshg_insert()`, but one is required for removing entities from the HSHG via `hshg_remove()`. In other words, you may only remove entities from the update callback:

```c
void update(struct hshg* hshg, hshg_entity_t id) {
  struct hshg_entity* entity = hshg->entities + id;
  struct my_obj* obj = objs + entity->ref;

  if(obj.remove) {
    hshg_remove(hshg, id);
    return;
  }

  entity->x += obj->vx;
  entity->y += obj->vy;
}

struct hshg hshg = {0};
hshg.update = update;
hshg_init(&hshg, 1, 1);
```

The update callback above actually won't do the trick, because it's updating the entity's position without asking the HSHG to update it too. To fix that, call `hshg_move()`:

```c
void update(struct hshg* hshg, hshg_entity_t id) {
  struct hshg_entity* entity = hshg->entities + id;
  struct my_obj* obj = objs + entity->ref;

  if(obj.remove) {
    hshg_remove(hshg, id);
    return;
  }

  entity->x += obj->vx;
  entity->y += obj->vy;

  hshg_move(hshg, id); /* !!! */
}
```

You don't need to call `hshg_move()` every single time you update the position - you may as well call it once at the end of the update callback.

If you are updating the entity's radius too (`entity->r`), you must also call `hshg_resize(hshg, id)`. It works pretty much like `hshg_move()`.

If you move or update an entity's radius without calling the respective functions at the end of the update callback, collision will not be accurate.

To call `hshg_resize()`, you must call `hshg_prealloc()` first, see above.

If you call `hshg_insert()` from `hshg.update`, the newly inserted entity **might or might not** be updated during the same `hshg_update()` function call. If you need to eliminate this possibility, keep track of a tick number or so, to make sure you don't update entities that were created in the same tick.

`hshg_update(&hshg)` goes through all entities and calls `hshg.update` on them.

`hshg_optimize(&hshg)` reallocates all entities and changes the order they are in so that they appear in the most cache friendly way possible. This process insanely speeds up the next call to `hshg_collide(&hshg)`. It is most effective when entities in the HSHG are clumped. Moreover, for a split second, the memory usage will nearly double, so if you can't have that, don't use the function.

```c
int err = hshg_optimize(&hshg);
if(err) {
  /* Out of memory */
}
```

`hshg_collide(&hshg)` goes through all cells on all grids and detects loose collision between square AABBs of the entities. It is your responsibility to detect the collision with more detail in the `hshg.collide` callback, if necessary. A sample callback for simple circle collision might look like so:

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

`hshg_query(&hshg, min_x, min_y, max_x, max_y)` calls `hshg.query` on every entity that belongs to the rectangular area from `(min_x, min_y)` to `(max_x, max_y)`. It is important that the second and third arguments are smaller or equal to fourth and fifth.

```c
void query(const struct hshg* hshg, const struct hshg_entity* a) {
  circle(a->x, a->y, a->r);
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

Summing up all of the above, a normal update tick would look like so:

```c
hshg_update(&hshg);
assert(!hshg_optimize(&hshg));
hshg_collide(&hshg);
```

For a complete example, see `c/hshg_bench.c` and `js/hshg_wasm.c`.
