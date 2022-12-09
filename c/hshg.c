/*
 *   Copyright 2022 Franciszek Balcerak
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "hshg.h"

#ifdef HSHG_NDEBUG
#define NDEBUG
#endif

extern void* malloc(size_t);
extern void  free(void*);
extern void* calloc(size_t, size_t);
extern void* realloc(void*, size_t);

extern float fabsf(float);
extern void* memcpy(void*, const void*, size_t);

#ifdef HSHG_NDEBUG
#define hshg_set(prop, to)
#else
#define hshg_set(prop, to) (hshg-> prop = (to))
#endif

#define min(a, b)               \
({                              \
    __typeof__ (a) _a = (a);    \
    __typeof__ (b) _b = (b);    \
    _a > _b ? _b : _a;          \
})

#define max(a, b)               \
({                              \
    __typeof__ (a) _a = (a);    \
    __typeof__ (b) _b = (b);    \
    _a > _b ? _a : _b;          \
})

#define max_t(t)                                \
(                                               \
    (((t)0x1 << ((sizeof(t) << 3) - 1)) - 1)    \
    |((t)0xF << ((sizeof(t) << 3) - 4))         \
)

hshg_attrib_unused
static const _hshg_entity_t  _hshg_entity_max  = max_t(_hshg_entity_t );

hshg_attrib_unused
static const _hshg_cell_t    _hshg_cell_max    = max_t(_hshg_cell_t   );

hshg_attrib_unused
static const _hshg_cell_sq_t _hshg_cell_sq_max = max_t(_hshg_cell_sq_t);

#undef max_t


hshg_attrib_const
static uint8_t
hshg_max_grids(_hshg_cell_t side)
{
    uint8_t grids_len = 0;

    do
    {
        ++grids_len;
        side >>= 1;
    }
    while(side >= 2);

    return grids_len;
}


hshg_attrib_const
static hshg_cell_sq_t
hshg_max_cells(_hshg_cell_t side)
{
    _hshg_cell_sq_t cells_len = 0;

    do
    {
        const _hshg_cell_sq_t new = cells_len +
            (_hshg_cell_sq_t) side
                    HSHG_2D(* side)
                    HSHG_3D(* side)
        ;

        assert(new > cells_len &&
            "_hshg_cell_sq_t must be set to a wider data type");

        cells_len = new;
        side >>= 1;
    }
    while(side >= 2);

    return cells_len;
}


_hshg*
_hshg_create(const _hshg_cell_t side, const uint32_t size)
{
    assert(__builtin_popcount(side) == 1 &&
        "Both arguments must be powers of 2");
    assert(__builtin_popcount(size) == 1 &&
        "Both arguments must be powers of 2");

    const _hshg_cell_sq_t cells_len = hshg_max_cells(side);
    const uint8_t grids_len = hshg_max_grids(side);

    _hshg* const hshg =
        malloc(sizeof(_hshg) + sizeof(_hshg_grid) * grids_len);

    if(hshg == NULL)
    {
        goto err;
    }

    _hshg_entity_t* const cells = calloc(cells_len, sizeof(_hshg_entity_t));

    if(cells == NULL)
    {
        goto err_hshg;
    }

    const _hshg_cell_sq_t grid_size = (_hshg_cell_sq_t) side * size;

    (void) memcpy(hshg, &(
    (_hshg)
    {
        .entities = NULL,
        .cells = cells,

        .update = NULL,
        .collide = NULL,
        .query = NULL,

        .cell_log = 31 - __builtin_ctz(size),
        .grids_len = grids_len,

        .calling = 0,
        .removed = 0,

        .old_cache = 0,
        .new_cache = 0,

        .grid_size = grid_size,
        .inverse_grid_size = (_hshg_pos_t) 1.0 / grid_size,
        .cells_len = cells_len,
        .cell_size = size,

        .free_entity = 0,
        .entities_used = 1,
        .entities_size = 1,
        .entity_id = 0
    }
    ), sizeof(_hshg));

    _hshg_entity_t idx = 0;
    uint32_t _size = size;

    _hshg_cell_t _side = side;

    for(uint8_t i = 0; i < grids_len; ++i)
    {
        (void) memcpy(hshg->grids + i, &(
        (_hshg_grid)
        {
            .cells = hshg->cells + idx,

            .cells_side = _side,
            .cells_mask = _side - 1,

            HSHG_2D(.cells2d_log = __builtin_ctz(_side) << 0,)
            HSHG_3D(.cells3d_log = __builtin_ctz(_side) << 1,)

            .shift = 0,

            .inverse_cell_size = (_hshg_pos_t) 1.0 / _size,

            .entities_len = 0
        }
        ), sizeof(_hshg_grid));

        idx +=
        (_hshg_cell_sq_t) _side
                HSHG_2D(* _side)
                HSHG_3D(* _side)
        ;
        _side >>= 1;
        _size <<= 1;
    }

    return hshg;

    err_hshg:
    free(hshg);

    err:
    return NULL;
}


void
_hshg_free(_hshg* const hshg)
{
    free(hshg->entities);
    free(hshg->cells);
    free(hshg);
}


hshg_attrib_const
size_t
_hshg_memory_usage(const _hshg_cell_t side,
    const _hshg_entity_t max_entities)
{
    const size_t entities = sizeof(_hshg_entity) * max_entities;
    const size_t cells = sizeof(_hshg_entity_t) * hshg_max_cells(side);
    const size_t grids = sizeof(_hshg_grid) * hshg_max_grids(side);
    const size_t hshg = sizeof(_hshg);
    return entities + cells + grids + hshg;
}


int
_hshg_set_size(_hshg* const hshg, const _hshg_entity_t size)
{
    assert(!hshg->calling &&
        "hshg_set_size() may not be called from any callback");
    assert(size >= hshg->entities_used);

    void* const ptr = realloc(hshg->entities, sizeof(_hshg_entity) * size);

    if(ptr == NULL)
    {
        return -1;
    }

    hshg->entities = ptr;
    hshg->entities_size = size;

    return 0;
}


static void
invalidate_entity(_hshg_entity* const entity)
{
    entity->cell = _hshg_cell_sq_max;
}


static int
invalid_entity(const _hshg_entity* const entity)
{
    return entity->cell == _hshg_cell_sq_max;
}


static _hshg_entity_t
hshg_get_entity(_hshg* const hshg)
{
    if(hshg->free_entity != 0)
    {
        const _hshg_entity_t ret = hshg->free_entity;

        hshg->free_entity = hshg->entities[ret].next;

        return ret;
    }

    if(hshg->entities_used == hshg->entities_size)
    {
        const _hshg_entity_t size = hshg->entities_size << 1;
        const _hshg_entity_t entities_size =
            hshg->entities_size > size ? _hshg_entity_max : size;

        if(_hshg_set_size(hshg, entities_size) == -1)
        {
            return 0;
        }
    }

    return hshg->entities_used++;
}


static void
hshg_return_entity(_hshg* const hshg)
{
    const _hshg_entity_t idx = hshg->entity_id;
    _hshg_entity* const ent = hshg->entities + idx;

    invalidate_entity(ent);

    ent->next = hshg->free_entity;
    hshg->free_entity = idx;
}


hshg_attrib_const
static _hshg_cell_t
grid_get_cell_1d(const _hshg_grid* const grid, const _hshg_pos_t x)
{
    const _hshg_cell_t cell = fabsf(x) * grid->inverse_cell_size;

    if(cell & grid->cells_side)
    {
        return grid->cells_mask - (cell & grid->cells_mask);
    }
    else
    {
        return cell & grid->cells_mask;
    }
}


hshg_attrib_const
static _hshg_cell_sq_t
grid_get_idx(const _hshg_grid* const grid
            , const _hshg_cell_sq_t x
    HSHG_2D(, const _hshg_cell_sq_t y)
    HSHG_3D(, const _hshg_cell_sq_t z)
)
{
    return x
        HSHG_2D( | (y << grid->cells2d_log))
        HSHG_3D( | (z << grid->cells3d_log))
        ;
}


hshg_attrib_const
static _hshg_cell_t
idx_get_x(const _hshg_grid* const grid, const _hshg_cell_sq_t cell)
{
    return cell & grid->cells_mask;
}


HSHG_2D(

hshg_attrib_const
static _hshg_cell_t
idx_get_y(const _hshg_grid* const grid, const _hshg_cell_sq_t cell)
{
    return cell >> grid->cells2d_log;
}

)


HSHG_3D(

hshg_attrib_const
static _hshg_cell_t
idx_get_z(const _hshg_grid* const grid, const _hshg_cell_sq_t cell)
{
    return cell >> grid->cells3d_log;
}

)


hshg_attrib_const
static _hshg_cell_sq_t
grid_get_cell(const _hshg_grid* const grid
            , const _hshg_pos_t x
    HSHG_2D(, const _hshg_pos_t y)
    HSHG_3D(, const _hshg_pos_t z)
)
{
            const _hshg_cell_t cell_x = grid_get_cell_1d(grid, x);
    HSHG_2D(const _hshg_cell_t cell_y = grid_get_cell_1d(grid, y);)
    HSHG_3D(const _hshg_cell_t cell_z = grid_get_cell_1d(grid, z);)

    return grid_get_idx(grid,
                  cell_x
        HSHG_2D(, cell_y)
        HSHG_3D(, cell_z)
    );
}


hshg_attrib_const
static uint8_t
hshg_get_grid(const _hshg* const hshg, const _hshg_pos_t r)
{
    const uint32_t rounded = r + r;

    if(rounded < hshg->cell_size)
    {
        return 0;
    }

    const uint8_t grid = hshg->cell_log - __builtin_clz(rounded) + 1;

    return min(grid, hshg->grids_len - 1);
}


static void
hshg_reinsert(_hshg* const hshg, const _hshg_entity_t idx)
{
    _hshg_entity* const entity = hshg->entities + idx;
    _hshg_grid* const grid = hshg->grids + entity->grid;

    entity->cell = grid_get_cell(grid
                , entity->x
        HSHG_2D(, entity->y)
        HSHG_3D(, entity->z)
    );

    _hshg_entity_t* const cell = grid->cells + entity->cell;

    entity->next = *cell;

    if(entity->next != 0)
    {
        hshg->entities[entity->next].prev = idx;
    }

    entity->prev = 0;
    *cell = idx;

    if(grid->entities_len == 0)
    {
        hshg->new_cache |= UINT32_C(1) << entity->grid;
    }

    ++grid->entities_len;
}


int
_hshg_insert(_hshg* const hshg
            , const _hshg_pos_t x
    HSHG_2D(, const _hshg_pos_t y)
    HSHG_3D(, const _hshg_pos_t z)
            , const _hshg_pos_t r
            , const _hshg_entity_t ref
)
{
    assert(!hshg->calling &&
        "hshg_insert() may not be called from any callback");

    const _hshg_entity_t idx = hshg_get_entity(hshg);

    if(idx == 0)
    {
        return -1;
    }

    _hshg_entity* const ent = hshg->entities + idx;

    ent->grid = hshg_get_grid(hshg, r);
    ent->ref = ref;
            ent->x = x;
    HSHG_2D(ent->y = y;)
    HSHG_3D(ent->z = z;)
    ent->r = r;

    hshg_reinsert(hshg, idx);

    return 0;
}


static void
hshg_remove_light(_hshg* const hshg)
{
    _hshg_entity* const entity = hshg->entities + hshg->entity_id;
    _hshg_grid* const grid = hshg->grids + entity->grid;

    if(entity->prev == 0)
    {
        grid->cells[entity->cell] = entity->next;
    }
    else
    {
        hshg->entities[entity->prev].next = entity->next;
    }

    hshg->entities[entity->next].prev = entity->prev;

    --grid->entities_len;

    if(grid->entities_len == 0)
    {
        hshg->new_cache &= ~(UINT32_C(1) << entity->grid);
    }
}


void
_hshg_remove(_hshg* const hshg)
{
    assert(hshg->updating &&
        "hshg_remove() may only be called from within hshg.update()");

    hshg_set(removed, 1);

    hshg_remove_light(hshg);
    hshg_return_entity(hshg);
}


void
_hshg_move(_hshg* const hshg)
{
    assert(hshg->updating &&
        "hshg_move() may only be called from within hshg.update()");

    const _hshg_entity_t idx = hshg->entity_id;
    _hshg_entity* const entity = hshg->entities + idx;
    const _hshg_grid* const grid = hshg->grids + entity->grid;

    if(entity->cell !=
        grid_get_cell(grid
                    , entity->x
            HSHG_2D(, entity->y)
            HSHG_3D(, entity->z)
        )
    )
    {
        hshg_remove_light(hshg);
        hshg_reinsert(hshg, idx);
    }
}


void
_hshg_resize(_hshg* const hshg)
{
    assert(hshg->updating &&
        "hshg_resize() may only be called from within hshg.update()");

    const _hshg_entity_t idx = hshg->entity_id;
    _hshg_entity* const entity = hshg->entities + idx;
    const uint8_t new_grid = hshg_get_grid(hshg, entity->r);

    if(entity->grid != new_grid)
    {
        hshg_remove_light(hshg);

        entity->grid = new_grid;

        hshg_reinsert(hshg, idx);
    }
}


void
_hshg_update(_hshg* const hshg)
{
    assert(hshg->update);
    assert(!hshg->calling &&
        "hshg_update() may not be called from any callback");

    hshg_set(updating, 1);

    _hshg_entity* entity = hshg->entities;

#define i hshg->entity_id

    for(i = 1; i < hshg->entities_used; ++i)
    {
        ++entity;

        if(invalid_entity(entity))
        {
            continue;
        }

        hshg->update(hshg, entity);
    }

#undef i

    hshg_set(removed, 0);
    hshg_set(updating, 0);
}


void
_hshg_update_multithread(const _hshg* const hshg,
    const uint8_t threads, const uint8_t idx)
{
    assert(hshg->const_update);

    const _hshg_entity_t used = hshg->entities_used - 1;
    const _hshg_entity_t div = used / threads;
    const _hshg_entity_t start = div * idx + 1;
    const _hshg_entity_t end =
        div + (idx + 1 == threads ? (used % threads) : 0) + 1;

    _hshg_entity* entity;
    _hshg_entity* const entity_max = hshg->entities + end;

    for(entity = hshg->entities + start; entity != entity_max; ++entity)
    {
        if(invalid_entity(entity))
        {
            continue;
        }

        hshg->const_update(hshg, entity);
    }
}


void
_hshg_update_cache(_hshg* const hshg)
{
    if(hshg->old_cache == hshg->new_cache)
    {
        return;
    }

    hshg->old_cache = hshg->new_cache;

    _hshg_grid* old_grid;
    const _hshg_grid* const grid_max = hshg->grids + hshg->grids_len;

    for(old_grid = hshg->grids; old_grid != grid_max; ++old_grid)
    {
        old_grid->shift = 0;
    }

    old_grid = hshg->grids;

    while(1)
    {
        if(old_grid == grid_max)
        {
            return;
        }

        if(old_grid->entities_len != 0)
        {
            break;
        }

        ++old_grid;
    }

    _hshg_grid* new_grid;

    uint8_t shift = 1;

    for(new_grid = old_grid + 1; new_grid != grid_max; ++new_grid)
    {
        if(new_grid->entities_len == 0)
        {
            ++shift;

            continue;
        }

        old_grid->shift = shift;
        old_grid = new_grid;
        shift = 1;
    }
}


void
_hshg_collide(_hshg* const hshg)
{
    assert(hshg->collide);
    assert(!hshg->calling &&
        "hshg_collide() may not be called from any callback");

    hshg_set(colliding, 1);

    _hshg_update_cache(hshg);

    const _hshg_entity* entity;
    const _hshg_entity* const entity_max =
        hshg->entities + hshg->entities_used;

    _hshg_entity_t i;
    const _hshg_entity* ent;

#define loop_over(from)                     \
                                            \
do                                          \
{                                           \
    i = (from);                             \
                                            \
    while(i != 0)                           \
    {                                       \
        ent = hshg->entities + i;           \
                                            \
        hshg->collide(hshg, entity, ent);   \
                                            \
        i = ent->next;                      \
    }                                       \
}                                           \
while(0)

    for(entity = hshg->entities + 1; entity != entity_max; ++entity)
    {
        if(invalid_entity(entity))
        {
            continue;
        }

        const _hshg_grid* grid = hshg->grids + entity->grid;

                _hshg_cell_t cell_x = idx_get_x(grid, entity->cell);
        HSHG_2D(_hshg_cell_t cell_y = idx_get_y(grid, entity->cell);)
        HSHG_3D(_hshg_cell_t cell_z = idx_get_z(grid, entity->cell);)

        HSHG_2D(

        if(cell_y != grid->cells_mask)
        {
            const _hshg_entity_t* const cell =
                grid->cells + (entity->cell + grid->cells_side);

            if(cell_x != 0)
            {
                loop_over(*(cell - 1));
            }

            loop_over(*cell);

            if(cell_x != grid->cells_mask)
            {
                loop_over(*(cell + 1));
            }
        }

        )

        loop_over(entity->next);

        if(cell_x != grid->cells_mask)
        {
            loop_over(grid->cells[entity->cell + 1]);
        }

        while(grid->shift)
        {
                    cell_x >>= grid->shift;
            HSHG_2D(cell_y >>= grid->shift;)
            HSHG_3D(cell_z >>= grid->shift;)


            grid += grid->shift;


                    const _hshg_cell_t min_cell_x =
                        cell_x != 0 ? cell_x - 1 : 0;

            HSHG_2D(const _hshg_cell_t min_cell_y =
                        cell_y != 0 ? cell_y - 1 : 0;)

            HSHG_3D(const _hshg_cell_t min_cell_z =
                        cell_z != 0 ? cell_z - 1 : 0;)


                    const _hshg_cell_t max_cell_x =
                        cell_x != grid->cells_mask ? cell_x + 1 : cell_x;

            HSHG_2D(const _hshg_cell_t max_cell_y =
                        cell_y != grid->cells_mask ? cell_y + 1 : cell_y;)

            HSHG_3D(const _hshg_cell_t max_cell_z =
                        cell_z != grid->cells_mask ? cell_z + 1 : cell_z;)


                    _hshg_cell_t cur_x;
            HSHG_2D(_hshg_cell_t cur_y;)
            HSHG_3D(_hshg_cell_t cur_z;)

            HSHG_3D(for(cur_z = min_cell_z; cur_z <= max_cell_z; ++cur_z))
            {

            HSHG_2D(for(cur_y = min_cell_y; cur_y <= max_cell_y; ++cur_y))
            {

            for(cur_x = min_cell_x; cur_x <= max_cell_x; ++cur_x)
            {
                const _hshg_cell_t cell =
                    grid_get_idx(grid
                                , cur_x
                        HSHG_2D(, cur_y)
                        HSHG_3D(, cur_z)
                    );

                loop_over(grid->cells[cell]);
            }

            }

            }
        }
    }

#undef loop_over

    hshg_set(colliding, 0);
}


int
_hshg_optimize(_hshg* const hshg)
{
    assert(!hshg->calling &&
        "hshg_optimize() may not be called from any callback");

    _hshg_entity* const entities =
        malloc(sizeof(_hshg_entity) * hshg->entities_size);

    if(entities == NULL)
    {
        return -1;
    }

    _hshg_entity_t idx = 1;
    _hshg_entity_t* cell = hshg->cells;

    for(_hshg_cell_sq_t i = 0; i < hshg->cells_len; ++i)
    {
        _hshg_entity_t entity_idx = *cell;

        if(entity_idx == 0)
        {
            ++cell;
            continue;
        }

        *cell = idx;
        ++cell;

        while(1)
        {
            _hshg_entity* const entity = entities + idx;

            *entity = hshg->entities[entity_idx];

            if(entity->prev != 0)
            {
                entity->prev = idx - 1;
            }

            ++idx;

            if(entity->next != 0)
            {
                entity_idx = entity->next;
                entity->next = idx;
            }
            else
            {
                break;
            }
        }
    }

    free(hshg->entities);

    hshg->entities = entities;
    hshg->entities_used = idx;
    hshg->free_entity = 0;

    return 0;
}


static void
hshg_map_pos(const _hshg* const hshg, _hshg_cell_t* const ret,
    const _hshg_pos_t _x1, const _hshg_pos_t _x2)
{
    _hshg_pos_t x1;
    _hshg_pos_t x2;

    if(_x1 < 0)
    {
        const _hshg_pos_t shift =
            ((
                (_hshg_cell_t)(-_x1 * hshg->inverse_grid_size) << 1
            ) + 2) * hshg->grid_size;

        x1 = _x1 + shift;
        x2 = _x2 + shift;
    }
    else
    {
        x1 = _x1;
        x2 = _x2;
    }

    _hshg_cell_t start;
    _hshg_cell_t end;
    _hshg_cell_t folds =
        (x2 - (_hshg_cell_t)(x1 * hshg->inverse_grid_size) * hshg->grid_size)
        * hshg->inverse_grid_size;

    const _hshg_grid* const grid = hshg->grids;

    switch(folds) {
    case 0:
    {
        const _hshg_cell_t cell = grid_get_cell_1d(grid, x1);

        end = grid_get_cell_1d(grid, x2);
        start = min(cell, end);
        end = max(cell, end);

        break;
    }
    case 1:
    {
        const _hshg_cell_t cell = fabsf(x1) * grid->inverse_cell_size;

        end = grid_get_cell_1d(grid, x2);

        if(cell & grid->cells_side)
        {
            start = 0;
            end = max(grid->cells_mask - (cell & grid->cells_mask), end);
        }
        else
        {
            start = min(cell & grid->cells_mask, end);
            end = grid->cells_mask;
        }

        break;
    }
    default:
    {
        start = 0;
        end = grid->cells_mask;

        break;
    }
    }

    *(ret + 0) = start;
    *(ret + 1) = end;
}


static void
hshg_query_common(const _hshg* const hshg
            , const _hshg_pos_t x1
    HSHG_2D(, const _hshg_pos_t y1)
    HSHG_3D(, const _hshg_pos_t z1)
            , const _hshg_pos_t x2
    HSHG_2D(, const _hshg_pos_t y2)
    HSHG_3D(, const _hshg_pos_t z2)
)
{
    assert(hshg->query);

            assert(x1 <= x2);
    HSHG_2D(assert(y1 <= y2);)
    HSHG_3D(assert(z1 <= z2);)

    struct
    {
        _hshg_cell_t start;
        _hshg_cell_t end;
    } x HSHG_2D(, y) HSHG_3D(, z);

            hshg_map_pos(hshg, &x.start, x1, x2);
    HSHG_2D(hshg_map_pos(hshg, &y.start, y1, y2);)
    HSHG_3D(hshg_map_pos(hshg, &z.start, z1, z2);)

    const _hshg_grid* grid = hshg->grids;
    const _hshg_grid* const grid_max = hshg->grids + hshg->grids_len;

    uint8_t shift = 0;

    while(1)
    {
        if(grid == grid_max)
        {
            return;
        }

        if(grid->entities_len != 0)
        {
            break;
        }

        ++grid;
        ++shift;
    }

            x.start >>= shift;
    HSHG_2D(y.start >>= shift;)
    HSHG_3D(z.start >>= shift;)

            x.end >>= shift;
    HSHG_2D(y.end >>= shift;)
    HSHG_3D(z.end >>= shift;)

    while(1)
    {
                const _hshg_cell_t s_x = x.start != 0 ? x.start - 1 : 0;
        HSHG_2D(const _hshg_cell_t s_y = y.start != 0 ? y.start - 1 : 0;)
        HSHG_3D(const _hshg_cell_t s_z = z.start != 0 ? z.start - 1 : 0;)

                const _hshg_cell_t e_x =
                    x.end != grid->cells_mask ? x.end + 1 : x.end;
        HSHG_2D(const _hshg_cell_t e_y =
                    y.end != grid->cells_mask ? y.end + 1 : y.end;)
        HSHG_3D(const _hshg_cell_t e_z =
                    z.end != grid->cells_mask ? z.end + 1 : z.end;)

        HSHG_3D(for(_hshg_cell_t z = s_z; z <= e_z; ++z))
        {

        HSHG_2D(for(_hshg_cell_t y = s_y; y <= e_y; ++y))
        {

        for(_hshg_cell_t x = s_x; x <= e_x; ++x)
        {
            _hshg_entity_t j;

            const _hshg_cell_sq_t cell =
                grid_get_idx(grid
                            , x
                    HSHG_2D(, y)
                    HSHG_3D(, z)
                );

            for(j = grid->cells[cell]; j != 0;)
            {
                const _hshg_entity* const entity =
                    hshg->entities + j;

                if(
                    entity->x + entity->r >= x1 &&
                    entity->x - entity->r <= x2
                    HSHG_2D(&&
                    entity->y + entity->r >= y1 &&
                    entity->y - entity->r <= y2)
                    HSHG_3D(&&
                    entity->z + entity->r >= z1 &&
                    entity->z - entity->r <= z2)
                )
                {
                    hshg->query(hshg, entity);
                }

                j = entity->next;
            }
        }

        }

        }

        if(grid->shift)
        {
                    x.start >>= grid->shift;
            HSHG_2D(y.start >>= grid->shift;)
            HSHG_3D(z.start >>= grid->shift;)

                    x.end >>= grid->shift;
            HSHG_2D(y.end >>= grid->shift;)
            HSHG_3D(z.end >>= grid->shift;)

            grid += grid->shift;
        }
        else
        {
            break;
        }
    }
}


void
_hshg_query(_hshg* const hshg
            , const _hshg_pos_t x1
    HSHG_2D(, const _hshg_pos_t y1)
    HSHG_3D(, const _hshg_pos_t z1)
            , const _hshg_pos_t x2
    HSHG_2D(, const _hshg_pos_t y2)
    HSHG_3D(, const _hshg_pos_t z2)
)
{
    assert((!hshg->updating || (hshg->updating && !hshg->removed)) &&
      "hshg_remove() and hshg_query() can't be mixed in the same "
      "hshg_update() tick, consider calling hshg_update() twice"
    );

#ifndef HSHG_NDEBUG

    const uint8_t old_querying = hshg->querying;

#endif

    hshg_set(querying, 1);

    _hshg_update_cache(hshg);

    hshg_query_common(hshg
                , x1
        HSHG_2D(, y1)
        HSHG_3D(, z1)
                , x2
        HSHG_2D(, y2)
        HSHG_3D(, z2)
    );

    hshg_set(querying, old_querying);
}


void
_hshg_query_multithread(const _hshg* const hshg
            , const _hshg_pos_t x1
    HSHG_2D(, const _hshg_pos_t y1)
    HSHG_3D(, const _hshg_pos_t z1)
            , const _hshg_pos_t x2
    HSHG_2D(, const _hshg_pos_t y2)
    HSHG_3D(, const _hshg_pos_t z2)
)
{
    hshg_query_common(hshg
                , x1
        HSHG_2D(, y1)
        HSHG_3D(, z1)
                , x2
        HSHG_2D(, y2)
        HSHG_3D(, z2)
    );
}
