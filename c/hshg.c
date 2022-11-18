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

#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef HSHG_NDEBUG
#define set_calling_to(hshg, to)
#else
#define set_calling_to(hshg, to) ((hshg)->calling = (to))
#endif

#ifdef __GNUC__
#define hshg_attribute(x) __attribute__(x)
#else
#define hshg_attribute(x)
#endif

#define max_t(t) \
  (((UINT64_C(0x1) << ((sizeof(t) << UINT64_C(3)) - UINT64_C(1))) - UINT64_C(1)) \
  | (UINT64_C(0xF) << ((sizeof(t) << UINT64_C(3)) - UINT64_C(4))))

static const hshg_entity_t  hshg_entity_max  = max_t(hshg_entity_t );
static const hshg_cell_t    hshg_cell_max    = max_t(hshg_cell_t   );
static const hshg_cell_sq_t hshg_cell_sq_max = max_t(hshg_cell_sq_t);

struct hshg* hshg_create(const hshg_cell_t side, const uint32_t size) {
  assert(__builtin_popcount(side) == 1 && "Cells on axis must be a power of 2");
  assert(__builtin_popcount(size) == 1 &&     "Cell size must be a power of 2");

  hshg_cell_sq_t cells_len = 0;
  hshg_cell_t _side = side;
  uint8_t grids_len = 0;
  while(_side) {
    const hshg_cell_sq_t new = cells_len + (hshg_cell_sq_t) _side * _side;
    assert(new > cells_len && "hshg_cell_sq_t must be set to a wider data type");
    cells_len = new;
    ++grids_len;
    _side >>= 1;
  }
  if(_side != 1) {
    --grids_len;
    --_side;
  }

  struct hshg* const hshg = calloc(1, sizeof(*hshg) + sizeof(*hshg->cells) * cells_len);
  if(hshg == NULL) {
    goto err;
  }

  struct hshg_grid* const grids = malloc(sizeof(*hshg->grids) * grids_len);
  if(hshg->grids == NULL) {
    goto err_hshg;
  }

  hshg_entity_t idx = 0;
  _side = side;
  uint32_t _size = size;
  for(uint8_t i = 0; i < grids_len; ++i) {
    (void) memcpy(grids + i, &((struct hshg_grid) {
      .cells = hshg->cells + idx,
      .idx = idx,

      .cells_side = _side,
      .cells_mask = _side - 1,
      .cells_log = __builtin_ctz(_side),
      .inverse_cell_size = (hshg_pos_t) 1.0 / _size,

      .entities = 0
    }), sizeof(*grids));

    idx += (hshg_cell_sq_t) _side * _side;
    _side >>= 1;
    _size <<= 1;
  }

  const hshg_cell_sq_t grid_size = (hshg_cell_sq_t) side * size;

  (void) memcpy(hshg, &((struct hshg) {
    .entities = NULL,
    .grids = grids,

    .update = NULL,
    .collide = NULL,
    .query = NULL,

    .cell_log = 31 - __builtin_ctz(size),
    .grids_len = grids_len,
    .calling = 0,

    .grid_size = grid_size,
    .inverse_grid_size = (hshg_pos_t) 1.0 / grid_size,
    .cells_len = cells_len,
    .cell_size = size,

    .free_entity = 0,
    .entities_used = 1,
    .entities_size = 1,
    .entity_id = 0
  }), sizeof(*hshg));

  return hshg;

  err_hshg:
  free(hshg);
  err:
  return NULL;
}

void hshg_free(struct hshg* const hshg) {
  free(hshg->entities);
  free(hshg->grids);
  free(hshg);
}

int hshg_set_size(struct hshg* const hshg, const hshg_entity_t size) {
  assert(size >= hshg->entities_used);
  void* const ptr = realloc(hshg->entities, sizeof(*hshg->entities) * size);
  if(ptr == NULL) {
    return -1;
  }
  hshg->entities = ptr;
  hshg->entities_size = size;
  return 0;
}

static hshg_entity_t hshg_get_entity(struct hshg* const hshg) {
  if(hshg->free_entity != 0) {
    const hshg_entity_t ret = hshg->free_entity;
    hshg->free_entity = hshg->entities[ret].next;
    return ret;
  }
  if(hshg->entities_used == hshg->entities_size) {
    const hshg_entity_t size = hshg->entities_size << 1;
    const hshg_entity_t entities_size = hshg->entities_size > size ? hshg_entity_max : size;
    if(hshg_set_size(hshg, entities_size) == -1) {
      return 0;
    }
  }
  return hshg->entities_used++;
}

static void hshg_return_entity(struct hshg* const hshg) {
  const hshg_entity_t idx = hshg->entity_id;
  hshg->entities[idx].rel_cell = hshg_cell_sq_max;
  hshg->entities[idx].next = hshg->free_entity;
  hshg->free_entity = idx;
}

hshg_attribute((const))
static hshg_cell_t grid_get_cell_(const struct hshg_grid* const grid, const hshg_pos_t x) {
  const hshg_cell_t cell = fabsf(x) * grid->inverse_cell_size;
  if(cell & grid->cells_side) {
    return grid->cells_mask - (cell & grid->cells_mask);
  } else {
    return cell & grid->cells_mask;
  }
}

hshg_attribute((const))
static hshg_cell_sq_t grid_get_idx(const struct hshg_grid* const grid, const hshg_cell_sq_t x, const hshg_cell_sq_t y) {
  return x | (y << grid->cells_log);
}

hshg_attribute((const))
static hshg_cell_sq_t grid_get_cell(const struct hshg_grid* const grid, const hshg_pos_t x, const hshg_pos_t y) {
  return grid_get_idx(grid, grid_get_cell_(grid, x), grid_get_cell_(grid, y));
}

hshg_attribute((const))
static uint8_t hshg_get_grid(const struct hshg* const hshg, const hshg_pos_t r) {
  const uint32_t rounded = r + r;
  if(rounded < hshg->cell_size) {
    return 0;
  }
  return hshg->cell_log - __builtin_clz(rounded) + 1;
}

static void hshg_reinsert(const struct hshg* const hshg, const hshg_entity_t idx) {
  struct hshg_entity* const entity = hshg->entities + idx;
  struct hshg_grid* const grid = hshg->grids + entity->grid;
  entity->rel_cell = grid_get_cell(grid, entity->x, entity->y);
  entity->abs_cell = grid->idx + entity->rel_cell;
  hshg_entity_t* const cell = hshg->cells + entity->abs_cell;
  entity->next = *cell;
  if(entity->next != 0) {
    hshg->entities[entity->next].prev = idx;
  }
  entity->prev = 0;
  *cell = idx;
}

int hshg_insert(struct hshg* const hshg, const hshg_pos_t x, const hshg_pos_t y, const hshg_pos_t r, const hshg_entity_t ref) {
  const hshg_entity_t idx = hshg_get_entity(hshg);
  if(idx == 0) {
    return -1;
  }
  struct hshg_entity* const ent = hshg->entities + idx;
  ent->grid = hshg_get_grid(hshg, r);
  ent->ref = ref;
  ent->x = x;
  ent->y = y;
  ent->r = r;
  hshg_reinsert(hshg, idx);
  return 0;
}

static void hshg_remove_light(const struct hshg* const hshg) {
  struct hshg_entity* const entity = hshg->entities + hshg->entity_id;
  if(entity->prev == 0) {
    hshg->cells[entity->abs_cell] = entity->next;
  } else {
    hshg->entities[entity->prev].next = entity->next;
  }
  hshg->entities[entity->next].prev = entity->prev;
}

void hshg_remove(struct hshg* const hshg) {
  assert(hshg->entity_id && "hshg_remove() may only be called from within hshg.update()");
  hshg_remove_light(hshg);
  hshg_return_entity(hshg);
}

void hshg_move(const struct hshg* const hshg) {
  assert(hshg->entity_id && "hshg_move() may only be called from within hshg.update()");
  const hshg_entity_t idx = hshg->entity_id;
  struct hshg_entity* const entity = hshg->entities + idx;
  const struct hshg_grid* const grid = hshg->grids + entity->grid;
  const hshg_cell_sq_t cell = grid_get_cell(grid, entity->x, entity->y);
  if(entity->rel_cell != cell) {
    hshg_remove_light(hshg);
    hshg_reinsert(hshg, idx);
  }
}

void hshg_resize(const struct hshg* const hshg) {
  assert(hshg->entity_id && "hshg_resize() may only be called from within hshg.update()");
  const hshg_entity_t idx = hshg->entity_id;
  struct hshg_entity* const entity = hshg->entities + idx;
  const uint8_t grid = hshg_get_grid(hshg, entity->r);
  assert(grid < hshg->grids_len && "Call hshg_prealloc() directly after hshg_init()");
  if(entity->grid != grid) {
    hshg_remove_light(hshg);
    entity->grid = grid;
    hshg_reinsert(hshg, idx);
  }
}

void hshg_update(struct hshg* const hshg) {
  assert(hshg->update);
  assert(!hshg->entity_id && "Nested updates are forbidden");
  assert(!hshg->calling && "hshg_update() may not be called from any callback");
  set_calling_to(hshg, 1);
  for(hshg->entity_id = 1; hshg->entity_id < hshg->entities_used; ++hshg->entity_id) {
    struct hshg_entity* const entity = hshg->entities + hshg->entity_id;
    if(entity->rel_cell == hshg_cell_sq_max) continue;
    hshg->update(hshg, entity);
  }
  hshg->entity_id = 0;
  set_calling_to(hshg, 0);
}

void hshg_collide(MAYBE_CONST struct hshg* const hshg) {
  assert(hshg->collide);
  assert(!hshg->calling && "hshg_collide() may not be called from any callback");
  set_calling_to(hshg, 1);
  for(hshg_entity_t i = 1; i < hshg->entities_used; ++i) {
    const struct hshg_entity* const entity = hshg->entities + i;
    if(entity->rel_cell == hshg_cell_sq_max) continue;
    const struct hshg_grid* grid = hshg->grids + entity->grid;
    for(hshg_entity_t j = entity->next; j != 0;) {
      const struct hshg_entity* const ent = hshg->entities + j;
      hshg->collide(hshg, entity, ent);
      j = ent->next;
    }
    hshg_cell_t cell_x = entity->rel_cell & grid->cells_mask;
    hshg_cell_t cell_y = entity->rel_cell >> grid->cells_log;
    if(cell_y != grid->cells_mask) {
      const hshg_entity_t* const cell = hshg->cells + (entity->abs_cell + grid->cells_side);
      if(cell_x != 0) {
        for(hshg_entity_t j = *(cell - 1); j != 0;) {
          const struct hshg_entity* const ent = hshg->entities + j;
          hshg->collide(hshg, entity, ent);
          j = ent->next;
        }
      }
      for(hshg_entity_t j = *cell; j != 0;) {
        const struct hshg_entity* const ent = hshg->entities + j;
        hshg->collide(hshg, entity, ent);
        j = ent->next;
      }
      if(cell_x != grid->cells_mask) {
        for(hshg_entity_t j = *(cell + 1); j != 0;) {
          const struct hshg_entity* const ent = hshg->entities + j;
          hshg->collide(hshg, entity, ent);
          j = ent->next;
        }
      }
    }
    if(cell_x != grid->cells_mask) {
      for(hshg_entity_t j = hshg->cells[entity->abs_cell + 1]; j != 0;) {
        const struct hshg_entity* const ent = hshg->entities + j;
        hshg->collide(hshg, entity, ent);
        j = ent->next;
      }
    }
    for(uint8_t up_grid = entity->grid + 1; up_grid < hshg->grids_len; ++up_grid) {
      ++grid;
      cell_x >>= 1;
      cell_y >>= 1;
      const hshg_cell_t min_cell_x = cell_x != 0 ? cell_x - 1 : 0;
      const hshg_cell_t min_cell_y = cell_y != 0 ? cell_y - 1 : 0;
      const hshg_cell_t max_cell_x = cell_x != grid->cells_mask ? cell_x + 1 : cell_x;
      const hshg_cell_t max_cell_y = cell_y != grid->cells_mask ? cell_y + 1 : cell_y;
      for(hshg_cell_t cur_y = min_cell_y; cur_y <= max_cell_y; ++cur_y) {
        for(hshg_cell_t cur_x = min_cell_x; cur_x <= max_cell_x; ++cur_x) {
          for(hshg_entity_t j = grid->cells[grid_get_idx(grid, cur_x, cur_y)]; j != 0;) {
            const struct hshg_entity* const ent = hshg->entities + j;
            hshg->collide(hshg, entity, ent);
            j = ent->next;
          }
        }
      }
    }
  }
  set_calling_to(hshg, 0);
}

int hshg_optimize(struct hshg* const hshg) {
  assert(!hshg->calling && "hshg_optimize() may not be called from any callback");
  struct hshg_entity* const entities = malloc(sizeof(*hshg->entities) * hshg->entities_size);
  if(entities == NULL) {
    return -1;
  }
  hshg_entity_t idx = 1;
  hshg_entity_t* cell = hshg->cells;
  for(hshg_cell_sq_t i = 0; i < hshg->cells_len; ++i) {
    hshg_entity_t entity_idx = *cell;
    if(entity_idx == 0) {
      ++cell;
      continue;
    }
    *cell = idx;
    ++cell;
    while(1) {
      struct hshg_entity* const entity = entities + idx;
      *entity = hshg->entities[entity_idx];
      if(entity->prev != 0) {
        entity->prev = idx - 1;
      }
      ++idx;
      if(entity->next != 0) {
        entity_idx = entity->next;
        entity->next = idx;
      } else {
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

#define min(a, b) ({ \
  __typeof__ (a) _a = (a); \
  __typeof__ (b) _b = (b); \
  _a > _b ? _b : _a; \
})

#define max(a, b) ({ \
  __typeof__ (a) _a = (a); \
  __typeof__ (b) _b = (b); \
  _a > _b ? _a : _b; \
})

void hshg_query(MAYBE_CONST struct hshg* const hshg, const hshg_pos_t _x1, const hshg_pos_t _y1, const hshg_pos_t _x2, const hshg_pos_t _y2) {
  /* ^ +y
     -------------
     |      x2,y2|
     |           |
     |x1,y1      |
     -------------> +x */
  assert(hshg->query);
  assert(_x1 <= _x2);
  assert(_y1 <= _y2);
  
#ifndef HSHG_NDEBUG
  const uint8_t old_calling = hshg->calling;
#endif
  set_calling_to(hshg, 1);

  hshg_pos_t x1;
  hshg_pos_t x2;
  if(_x1 < 0) {
    const hshg_pos_t shift = (((hshg_cell_t)(-_x1 * hshg->inverse_grid_size) << 1) + 2) * hshg->grid_size;
    x1 = _x1 + shift;
    x2 = _x2 + shift;
  } else {
    x1 = _x1;
    x2 = _x2;
  }
  hshg_cell_t start_x;
  hshg_cell_t end_x;
  hshg_cell_t folds = (x2 - (hshg_cell_t)(x1 * hshg->inverse_grid_size) * hshg->grid_size) * hshg->inverse_grid_size;
  switch(folds) {
    case 0: {
      const hshg_cell_t cell = grid_get_cell_(hshg->grids, x1);
      end_x = grid_get_cell_(hshg->grids, x2);
      start_x = min(cell, end_x);
      end_x = max(cell, end_x);
      break;
    }
    case 1: {
      const hshg_cell_t cell = fabsf(x1) * hshg->grids->inverse_cell_size;
      end_x = grid_get_cell_(hshg->grids, x2);
      if(cell & hshg->grids->cells_side) {
        start_x = 0;
        end_x = max(hshg->grids->cells_mask - (cell & hshg->grids->cells_mask), end_x);
      } else {
        start_x = min(cell & hshg->grids->cells_mask, end_x);
        end_x = hshg->grids->cells_mask;
      }
      break;
    }
    default: {
      start_x = 0;
      end_x = hshg->grids->cells_mask;
      break;
    }
  }

  hshg_pos_t y1;
  hshg_pos_t y2;
  if(_y1 < 0) {
    const hshg_pos_t shift = (((hshg_cell_t)(-_y1 * hshg->inverse_grid_size) << 1) + 2) * hshg->grid_size;
    y1 = _y1 + shift;
    y2 = _y2 + shift;
  } else {
    y1 = _y1;
    y2 = _y2;
  }
  hshg_cell_t start_y;
  hshg_cell_t end_y;
  folds = (y2 - (hshg_cell_t)(y1 * hshg->inverse_grid_size) * hshg->grid_size) * hshg->inverse_grid_size;
  switch(folds) {
    case 0: {
      const hshg_cell_t cell = grid_get_cell_(hshg->grids, y1);
      end_y = grid_get_cell_(hshg->grids, y2);
      start_y = min(cell, end_y);
      end_y = max(cell, end_y);
      break;
    }
    case 1: {
      const hshg_cell_t cell = fabsf(y1) * hshg->grids->inverse_cell_size;
      end_y = grid_get_cell_(hshg->grids, y2);
      if(cell & hshg->grids->cells_side) {
        start_y = 0;
        end_y = max(hshg->grids->cells_mask - (cell & hshg->grids->cells_mask), end_y);
      } else {
        start_y = min(cell & hshg->grids->cells_mask, end_y);
        end_y = hshg->grids->cells_mask;
      }
      break;
    }
    default: {
      start_y = 0;
      end_y = hshg->grids->cells_mask;
      break;
    }
  }

  const struct hshg_grid* grid = hshg->grids;
  uint8_t i = 0;
  while(1) {
    const hshg_cell_t s_x = start_x != 0 ? start_x - 1 : 0;
    const hshg_cell_t s_y = start_y != 0 ? start_y - 1 : 0;
    const hshg_cell_t e_x = end_x != grid->cells_mask ? end_x + 1 : end_x;
    const hshg_cell_t e_y = end_y != grid->cells_mask ? end_y + 1 : end_y;
    for(hshg_cell_t y = s_y; y <= e_y; ++y) {
      for(hshg_cell_t x = s_x; x <= e_x; ++x) {
        for(hshg_entity_t j = grid->cells[grid_get_idx(grid, x, y)]; j != 0;) {
          const struct hshg_entity* const entity = hshg->entities + j;
          if(entity->x + entity->r >= _x1 && entity->x - entity->r <= _x2 && entity->y + entity->r >= _y1 && entity->y - entity->r <= _y2) {
            hshg->query(hshg, entity);
          }
          j = entity->next;
        }
      }
    }
    if(++i == hshg->grids_len) break;
    ++grid;
    start_x >>= 1;
    start_y >>= 1;
    end_x >>= 1;
    end_y >>= 1;
  }

  set_calling_to(hshg, old_calling);
}
