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

#include <stdint.h>

#ifndef hshg_entity_t
#define hshg_entity_t  uint32_t
#endif

#ifndef hshg_cell_t
#define hshg_cell_t    uint16_t
#endif

#ifndef hshg_cell_sq_t
#define hshg_cell_sq_t uint32_t
#endif

#ifndef hshg_pos_t
#define hshg_pos_t     float
#endif

#ifdef HSHG_NDEBUG
#define MAYBE_CONST const
#else
#define MAYBE_CONST
#endif

struct hshg_entity {
  hshg_cell_sq_t rel_cell;
  hshg_cell_sq_t abs_cell;
  hshg_entity_t next;
  hshg_entity_t prev;
  hshg_entity_t ref;
  hshg_pos_t x;
  hshg_pos_t y;
  hshg_pos_t r;
};

struct hshg_grid {
  hshg_entity_t* const cells;
  hshg_entity_t idx;

  const hshg_cell_t cells_side;
  const hshg_cell_t cells_mask;
  const uint8_t cells_log;
  const hshg_pos_t inverse_cell_size;

  hshg_entity_t entities;
};

struct hshg;
typedef void (*hshg_update_t)(struct hshg*, struct hshg_entity*);
typedef void (*hshg_collide_t)(const struct hshg*, const struct hshg_entity*, const struct hshg_entity*);
typedef void (*hshg_query_t)(const struct hshg*, const struct hshg_entity*);

struct hshg {
  struct hshg_entity* entities;
  struct hshg_grid* const grids;

  hshg_update_t update;
  hshg_collide_t collide;
  hshg_query_t query;
  
  const uint8_t cell_log;
  const uint8_t grids_len;
  uint8_t calling;
  
  const hshg_cell_sq_t grid_size;
  const hshg_pos_t inverse_grid_size;
  const hshg_cell_sq_t cells_len;
  const uint32_t cell_size;

  hshg_entity_t free_entity;
  hshg_entity_t entities_used;
  hshg_entity_t entities_size;
  hshg_entity_t entity_id;

  hshg_entity_t cells[];
};

extern struct hshg* hshg_create(const hshg_cell_t, const uint32_t);

extern void hshg_free(struct hshg* const);

extern int  hshg_set_size(struct hshg* const, const hshg_entity_t);

extern int  hshg_insert(struct hshg* const, const hshg_pos_t, const hshg_pos_t, const hshg_pos_t, const hshg_entity_t);

extern void hshg_remove(struct hshg* const);

extern void hshg_move(const struct hshg* const);

extern void hshg_resize(const struct hshg* const);

extern void hshg_update(struct hshg* const);

extern void hshg_collide(MAYBE_CONST struct hshg* const);

extern int  hshg_optimize(struct hshg* const);

extern void hshg_query(MAYBE_CONST struct hshg* const, const hshg_pos_t, const hshg_pos_t, const hshg_pos_t, const hshg_pos_t);

#endif /* _hshg_h_ */
