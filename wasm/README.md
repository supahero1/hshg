```js
const { HSHG_2D } = require("./hshg");
```

To create a HSHG, you need to know the number of cells you want to use, size of a cell, and the maximum number of entities you are planning on inserting.

The number of cells and the cell size must both be powers of 2. You needn't be concerned however, as no operation scales with the number of cells. If you require 50x50 cells, just go for 64x64.

You won't be able to change any of these initialization values afterwards.

```js
/* in 2D, the total number of cells is then 32 * 32 */
const cells_on_edge = 32;
const cell_size = 32;
const max_entities = 1000;

const engine = await new HSHG_2D(cells_on_edge, cell_size, max_entities);

/* or */

new HSHG_2D(cells_on_edge, cell_size, max_entities).then(function(engine) {
    /* ... */
});
```

Now that you have that, your next goal should be to decide what you will want to do upon updating, colliding, or querying. You can do that like so:

```js
engine.update = function(entity) {
    /* reported once per entity */
};

engine.collide = function(ent_a, ent_b) {
    /* reported once per collision */
};

engine.query = function(entity) {
    /* reported once per entity in searched area */
};
```

At the same time, you can begin inserting some entities. Note that since we are in 2D, you will need `x` and `y` coordinates. In 1D, you will only need `x`, and in 3D, you will also need `z`.

The entities you insert are stored internally. You do not have access to them in any other way than via the engine calls. For instance, if you want to go through all of them, you can just call `engine.update()`. You can modify the callbacks you set before as many times as you like.

The entities you insert must meet certain requirements. Namely, they must have fields called `x`, `y`, and `r` (for 2D).

```js
class Ball {
    x = 2;
    y = 3;
    r = 1;

    constructor(){}
}

engine.insert(new Ball());
```

Now this ball that was just inserted will be reported in `engine.update()` once it's called.

If you wish to remove an entity, you need to do it explicitly from the `engine.update` callback like so:

```js
engine.update = function(entity) {
    if(entity.designated_to_be_deleted) {
        return this.remove(); /* or engine.remove() */
    }

    /* else, do other fun stuff */

    entity.x += entity.vx;
    entity.y += entity.vy;

    if(entity.x - entity.r < 0) {
        entity.designated_to_be_deleted = true;
    }
};
```

Any changes you make to `entity.x`, `entity.y`, or `entity.r`, will be propagated to the underlying WASM module.

`engine.collide()` simply detects collision. No collision will ever be detected twice. If `A` and `B` collide, this fact will only be reported via one call to `engine.collide`, with `A` and `B` being in any order in the arguments, so it could be either `A` `B` or `B` `A`, it doesn't matter.

Note that this provides **broad** collision detection - the items might not be colliding. In fact, their bounding boxes might not even be in contact. It's up to you to check.

`engine.query(min_x, min_y, max_x, max_y)` searches the specified area and calls `engine.query` on every found entity. It will only be called once per entity. Any entities intersecting the queried area will be included, not just those which are entirely inside.

You can access `engine.mem` to see roughly how many bytes of memory it has allocated for itself.

If you want to learn more, check out [the source repository](https://github.com/supahero1/hshg).
