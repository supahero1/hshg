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

#ifndef _hshg_h_
#define _hshg_h_ 1

#ifndef HSHG_NDEBUG

#include <assert.h>

static_assert(__STDC_VERSION__ >= 201112L);

#else

#define assert(...)

#endif


#define hshg_attrib_const __attribute__((const))
#define hshg_attrib_unused __attribute__((unused))


#include <stdint.h>


#ifndef HSHG_D
#define HSHG_D 2
#endif

#define _HSHG_CAT(A, B) A ## B
#define HSHG_CAT(A, B) _HSHG_CAT(A, B)

#if   HSHG_D == 1
#define HSHG_2D(...)
#define HSHG_3D(...)
#elif HSHG_D == 2
#define HSHG_2D(...) __VA_ARGS__
#define HSHG_3D(...)
#elif HSHG_D == 3
#define HSHG_2D(...) __VA_ARGS__
#define HSHG_3D(...) __VA_ARGS__
#endif

#ifdef HSHG_UNIFORM
#undef HSHG_D
#define HSHG_D
#endif

#define HSHG_NAME(name) HSHG_CAT(HSHG_CAT(hshg, HSHG_D), HSHG_CAT(_, name))



/**
 * A type that will be able to hold the maximum number of entities that the
 * application expects to ever hold at once + 1.
 */
#define __hshg_entity_t HSHG_NAME(entity_t)

#ifndef hshg_entity_t
#define hshg_entity_t uint32_t
#endif

typedef hshg_entity_t
#undef hshg_entity_t
    __hshg_entity_t;

typedef __hshg_entity_t _hshg_entity_t;

#undef __hshg_entity_t



/**
 * A type that will be able to hold the maximum number of cells on an edge of
 * a HSHG (the first parameter to hshg_create()).
 */
#define __hshg_cell_t HSHG_NAME(cell_t)

#ifndef hshg_cell_t
#define hshg_cell_t uint16_t
#endif

typedef hshg_cell_t
#undef hshg_cell_t
    __hshg_cell_t;

typedef __hshg_cell_t _hshg_cell_t;

#undef __hshg_cell_t



/**
 * A type that will be able to hold the total number of cells in a HSHG. To get
 * an upper bound of that number, calculate:
 *
 * ( side ^ dimensions ) * [ 2, 1.333, 1.143 ][ dimensions ]
 *
 * For 2D, you would do: side * side * 1.333.
 *
 */
#define __hshg_cell_sq_t HSHG_NAME(cell_sq_t)

#ifndef hshg_cell_sq_t
#define hshg_cell_sq_t uint32_t
#endif

typedef hshg_cell_sq_t
#undef hshg_cell_sq_t
    __hshg_cell_sq_t;

typedef __hshg_cell_sq_t _hshg_cell_sq_t;

#undef __hshg_cell_sq_t



/**
 * A type for x, y, z, and radius values used throughout a HSHG.
 */
#define __hshg_pos_t HSHG_NAME(pos_t)

#ifndef hshg_pos_t
#define hshg_pos_t float
#endif

typedef hshg_pos_t
#undef hshg_pos_t
    __hshg_pos_t;

typedef __hshg_pos_t _hshg_pos_t;

#undef __hshg_pos_t



#define __hshg_entity_t     \
{                           \
    _hshg_cell_sq_t cell;   \
    uint8_t grid;           \
    _hshg_entity_t next;    \
    _hshg_entity_t prev;    \
    _hshg_entity_t ref;     \
    _hshg_pos_t x;          \
    HSHG_2D(_hshg_pos_t y;) \
    HSHG_3D(_hshg_pos_t z;) \
    _hshg_pos_t r;          \
}

#define __hshg_entity HSHG_NAME(entity)

typedef struct __hshg_entity __hshg_entity_t _hshg_entity;

#undef __hshg_entity



#define __hshg_grid_t               \
{                                   \
    _hshg_entity_t* cells;          \
                                    \
    _hshg_cell_t cells_side;        \
    _hshg_cell_t cells_mask;        \
                                    \
    HSHG_2D(uint8_t cells2d_log;)   \
    HSHG_3D(uint8_t cells3d_log;)   \
                                    \
    uint8_t cache_idx;              \
    uint8_t shift;                  \
                                    \
    _hshg_pos_t inverse_cell_size;  \
                                    \
    _hshg_entity_t entities_len;    \
}

#define __hshg_grid HSHG_NAME(grid)

typedef struct __hshg_grid __hshg_grid_t _hshg_grid;

#undef __hshg_grid



#define __hshg HSHG_CAT(hshg, HSHG_D)

typedef struct __hshg _hshg;



#define __hshg_update_t HSHG_NAME(update_t)

typedef void (*__hshg_update_t)(_hshg*, _hshg_entity*);

typedef __hshg_update_t _hshg_update_t;

#undef __hshg_update_t



/**
 * Same as hshg_update_t, but suitable for multithreading.
 */
#define __hshg_const_update_t HSHG_NAME(const_update_t)

typedef void (*__hshg_const_update_t)(
    const _hshg*, _hshg_entity*);

typedef __hshg_const_update_t _hshg_const_update_t;

#undef __hshg_const_update_t



#define __hshg_collide_t HSHG_NAME(collide_t)

typedef void (*__hshg_collide_t)(const _hshg*,
    const _hshg_entity*, const _hshg_entity*);

typedef __hshg_collide_t _hshg_collide_t;

#undef __hshg_collide_t



#define __hshg_query_t HSHG_NAME(query_t)

typedef void (*__hshg_query_t)(const _hshg*, const _hshg_entity*);

typedef __hshg_query_t _hshg_query_t;

#undef __hshg_query_t



#define __hshg_t                            \
{                                           \
    _hshg_entity* entities;                 \
    _hshg_entity_t* const cells;            \
    _hshg_grid* const grid_cache;           \
                                            \
    _hshg_update_t update;                  \
    _hshg_const_update_t const_update;      \
    _hshg_collide_t collide;                \
    _hshg_query_t query;                    \
                                            \
    const uint8_t cell_log;                 \
    const uint8_t grids_len;                \
    uint8_t grid_cache_len;                 \
                                            \
    union                                   \
    {                                       \
        struct                              \
        {                                   \
            uint8_t updating:1;             \
            uint8_t colliding:1;            \
            uint8_t querying:1;             \
        };                                  \
        uint8_t calling;                    \
    };                                      \
    uint8_t removed:1;                      \
                                            \
    uint32_t old_cache;                     \
    uint32_t new_cache;                     \
                                            \
    const _hshg_cell_sq_t grid_size;        \
    const _hshg_pos_t inverse_grid_size;    \
    const _hshg_cell_sq_t cells_len;        \
    const uint32_t cell_size;               \
                                            \
    _hshg_entity_t free_entity;             \
    _hshg_entity_t entities_used;           \
    _hshg_entity_t entities_size;           \
    _hshg_entity_t entity_id;               \
                                            \
    _hshg_grid grids[];                     \
}

struct __hshg __hshg_t;

#undef __hshg



/**
 * Creates a new HSHG.
 *
 * \param side the number of cells on the smallest grid's edge
 * \param size smallest cell size
 */
#define _hshg_create HSHG_NAME(create)

extern _hshg*
_hshg_create(const _hshg_cell_t side, const uint32_t size);



#define _hshg_free HSHG_NAME(free)

extern void
_hshg_free(_hshg* const);



#define _hshg_set_size HSHG_NAME(set_size)

extern int
_hshg_set_size(_hshg* const, const _hshg_entity_t size);



#define _hshg_insert HSHG_NAME(insert)

extern int
_hshg_insert(_hshg* const
            , const _hshg_pos_t x
    HSHG_2D(, const _hshg_pos_t y)
    HSHG_3D(, const _hshg_pos_t z)
            , const _hshg_pos_t r
            , const _hshg_entity_t ref
);



#define _hshg_remove HSHG_NAME(remove)

extern void
_hshg_remove(_hshg* const);



#define _hshg_move HSHG_NAME(move)

extern void
_hshg_move(_hshg* const);



#define _hshg_resize HSHG_NAME(resize)

extern void
_hshg_resize(_hshg* const);



#define _hshg_update HSHG_NAME(update)

extern void
_hshg_update(_hshg* const);



/**
 * Multithreaded update.
 *
 * \param threads total number of threads used
 * \param idx index of the thread calling the function at the moment, counting
 * from 0
 */
#define _hshg_update_multithread HSHG_NAME(update_multithread)

extern void
_hshg_update_multithread(const _hshg* const,
    const uint8_t threads, const uint8_t idx);



#define _hshg_update_cache HSHG_NAME(update_cache)

extern void
_hshg_update_cache(_hshg* const);



#define _hshg_collide HSHG_NAME(collide)

extern void
_hshg_collide(_hshg* const);



#define _hshg_optimize HSHG_NAME(optimize)

extern int
_hshg_optimize(_hshg* const);



#define _hshg_query HSHG_NAME(query)

extern void
_hshg_query(_hshg* const
            , const _hshg_pos_t min_x
    HSHG_2D(, const _hshg_pos_t min_y)
    HSHG_3D(, const _hshg_pos_t min_z)
            , const _hshg_pos_t max_x
    HSHG_2D(, const _hshg_pos_t max_y)
    HSHG_3D(, const _hshg_pos_t max_z)
);



#define _hshg_query_multithread HSHG_NAME(query_multithread)

extern void
_hshg_query_multithread(const _hshg* const
            , const _hshg_pos_t min_x
    HSHG_2D(, const _hshg_pos_t min_y)
    HSHG_3D(, const _hshg_pos_t min_z)
            , const _hshg_pos_t max_x
    HSHG_2D(, const _hshg_pos_t max_y)
    HSHG_3D(, const _hshg_pos_t max_z)
);



#endif /* _hshg_h_ */
