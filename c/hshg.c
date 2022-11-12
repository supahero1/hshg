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

#include <math.h>
#include <stdlib.h>
#include <assert.h>

static uint8_t hshg_create_grid(struct hshg* const hshg) {
  ++hshg->grids_len;
  void* ptr = realloc(hshg->grids, sizeof(*hshg->grids) * hshg->grids_len);
  if(ptr == NULL) {
    goto err;
  }
  hshg->grids = ptr;
  struct hshg_grid* const current = hshg->grids + hshg->grids_len - 1;
  struct hshg_grid* const past = hshg->grids + hshg->grids_len - 2;
  current->cells_side = past->cells_side >> hshg->cell_div_log;
  current->cells_log = past->cells_log - hshg->cell_div_log;
  current->cells_mask = current->cells_side - 1;
  current->cell_size = past->cell_size << hshg->cell_div_log;
  current->inverse_cell_size = past->inverse_cell_size / (UINT32_C(1) << hshg->cell_div_log);
  ptr = calloc((hshg_cell_sq_t) current->cells_side * current->cells_side, sizeof(*current->cells));
  if(ptr == NULL) {
    goto err;
  }
  current->cells = ptr;
  return 0;

  err:
  --hshg->grids_len;
  return UINT8_MAX;
}

int hshg_init(struct hshg* const hshg, const hshg_cell_t side, const uint32_t size) {
  assert(__builtin_popcount(side) == 1 && "Cells on axis must be a power of 2");
  assert(__builtin_popcount(size) == 1 &&     "Cell size must be a power of 2");

  hshg->free_entity = 0;
  hshg->entities_used = 1;
  if(hshg->entities_size == 0) {
    hshg->entities_size = 1;
  } else {
    hshg->entities = malloc(sizeof(*hshg->entities) * hshg->entities_size);
    if(hshg->entities == NULL) {
      return -1;
    }
  }

  hshg->grids = malloc(sizeof(*hshg->grids));
  if(hshg->grids == NULL) {
    goto err_entities;
  }
  hshg->grids->cells = calloc((hshg_cell_sq_t) side * side, sizeof(*hshg->grids->cells));
  if(hshg->grids->cells == NULL) {
    goto err_grids;
  }
  hshg->grids->cells_side = side;
  hshg->grids->cells_mask = side - 1;
  hshg->grids->cells_log = __builtin_ctz(side);
  hshg->grids->cell_size = size;
  hshg->grids->inverse_cell_size = 1.0f / size;

  if(hshg->cell_div_log == 0) {
    hshg->cell_div_log = 1;
  }
  hshg->cell_log = 31 - __builtin_ctz(size);
  hshg->grids_len = 1;

  hshg->grid_size = (hshg_cell_sq_t) side * size;

  return 0;

  err_grids:
  free(hshg->grids);
  err_entities:
  free(hshg->entities);
  return -1;
}

void hshg_free(struct hshg* const hshg) {
  free(hshg->entities);
  for(uint8_t i = 0; i < hshg->grids_len; ++i) {
    free(hshg->grids[i].cells);
  }
  free(hshg->grids);
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
  hshg->entities[idx].cell = hshg_cell_sq_max;
  hshg->entities[idx].next = hshg->free_entity;
  hshg->free_entity = idx;
}

static hshg_cell_t grid_get_cell_(const struct hshg_grid* const grid, const hshg_pos_t x) {
  const hshg_cell_t cell = fabsf(x) * grid->inverse_cell_size;
  if(cell & grid->cells_side) {
    return grid->cells_mask - (cell & grid->cells_mask);
  } else {
    return cell & grid->cells_mask;
  }
}

static hshg_cell_sq_t grid_get_cell(const struct hshg_grid* const grid, const hshg_pos_t x, const hshg_pos_t y) {
  return (hshg_cell_sq_t) grid_get_cell_(grid, x) | ((hshg_cell_sq_t) grid_get_cell_(grid, y) << grid->cells_log);
}

static uint8_t hshg_get_grid(const struct hshg* const hshg, const hshg_pos_t r) {
  const uint32_t rounded = r + r;
  if(rounded < hshg->grids[0].cell_size) {
    return 0;
  }
  return (hshg->cell_log - __builtin_clz(rounded)) / hshg->cell_div_log + 1;
}

static uint8_t hshg_get_grid_resizable(struct hshg* const hshg, const hshg_pos_t r) {
  uint8_t grid = hshg_get_grid(hshg, r);
  if(grid >= hshg->grids_len) {
    for(uint8_t i = hshg->grids_len; i <= grid && hshg->grids[hshg->grids_len - 1].cells_side > 2; ++i) {
      if(hshg_create_grid(hshg) == UINT8_MAX) {
        return UINT8_MAX;
      }
    }
    grid = hshg->grids_len - 1;
  }
  return grid;
}

int hshg_prealloc(struct hshg* const hshg, const hshg_pos_t max_r) {
  assert(hshg->grids_len && "Call hshg_init() before using hshg_prealloc()");
  const uint8_t max_grid = hshg_get_grid(hshg, max_r);
  for(uint8_t i = hshg->grids_len; i <= max_grid && hshg->grids[hshg->grids_len - 1].cells_side > 2; ++i) {
    if(hshg_create_grid(hshg) == UINT8_MAX) {
      return -1;
    }
  }
  return 0;
}

static void hshg_reinsert(const struct hshg* const hshg, const hshg_entity_t idx) {
  struct hshg_entity* const ent = hshg->entities + idx;
  ent->cell = grid_get_cell(hshg->grids + ent->grid, ent->x, ent->y);
  ent->next = hshg->grids[ent->grid].cells[ent->cell];
  if(ent->next != 0) {
    hshg->entities[ent->next].prev = idx;
  }
  ent->prev = 0;
  hshg->grids[ent->grid].cells[ent->cell] = idx;
}

int hshg_insert(struct hshg* const hshg, const hshg_pos_t x, const hshg_pos_t y, const hshg_pos_t r, const hshg_entity_t ref) {
  const uint8_t grid = hshg_get_grid_resizable(hshg, r);
  if(grid == UINT8_MAX) {
    return -1;
  }
  const hshg_entity_t idx = hshg_get_entity(hshg);
  if(idx == 0) {
    return -1;
  }
  struct hshg_entity* const ent = hshg->entities + idx;
  ent->grid = grid;
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
    hshg->grids[entity->grid].cells[entity->cell] = entity->next;
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
  if(entity->cell != cell) {
    hshg_remove_light(hshg);
    entity->cell = cell;
    entity->next = grid->cells[cell];
    if(entity->next != 0) {
      hshg->entities[entity->next].prev = idx;
    }
    entity->prev = 0;
    grid->cells[cell] = idx;
  }
}

void hshg_resize(const struct hshg* const hshg) {
  assert(hshg->entity_id && "hshg_resize() may only be called from within hshg.update()");
  struct hshg_entity* const entity = hshg->entities + hshg->entity_id;
  const uint8_t grid = hshg_get_grid(hshg, entity->r);
  assert(grid < hshg->grids_len && "Call hshg_prealloc() directly after hshg_init()");
  if(entity->grid != grid) {
    hshg_remove_light(hshg);
    entity->grid = grid;
    hshg_reinsert(hshg, hshg->entity_id);
  }
}

void hshg_update(struct hshg* const hshg) {
  assert(hshg->update);
  assert(!hshg->entity_id && "Nested updates are forbidden");
  assert(!hshg->calling && "hshg_update() may not be called from any callback");
  hshg->calling = 1;
  for(hshg->entity_id = 1; hshg->entity_id < hshg->entities_used; ++hshg->entity_id) {
    struct hshg_entity* const entity = hshg->entities + hshg->entity_id;
    if(entity->cell == hshg_cell_sq_max) continue;
    hshg->update(hshg, entity);
  }
  hshg->entity_id = 0;
  hshg->calling = 0;
}

void hshg_collide(struct hshg* const hshg) {
  assert(hshg->collide);
  assert(!hshg->calling && "hshg_collide() may not be called from any callback");
  hshg->calling = 1;
  for(hshg_entity_t i = 1; i < hshg->entities_used; ++i) {
    const struct hshg_entity* const entity = hshg->entities + i;
    if(entity->cell == hshg_cell_sq_max) continue;
    const struct hshg_grid* grid = hshg->grids + entity->grid;
    for(hshg_entity_t j = entity->next; j != 0;) {
      const struct hshg_entity* const ent = hshg->entities + j;
      hshg->collide(hshg, entity, ent);
      j = ent->next;
    }
    hshg_cell_t cell_x = entity->cell & grid->cells_mask;
    hshg_cell_t cell_y = entity->cell >> grid->cells_log;
    if(cell_y != grid->cells_mask) {
      const hshg_entity_t* const cell = grid->cells + (entity->cell + grid->cells_side);
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
      for(hshg_entity_t j = grid->cells[entity->cell + 1]; j != 0;) {
        const struct hshg_entity* const ent = hshg->entities + j;
        hshg->collide(hshg, entity, ent);
        j = ent->next;
      }
    }
    for(uint8_t up_grid = entity->grid + 1; up_grid < hshg->grids_len; ++up_grid) {
      ++grid;
      cell_x >>= hshg->cell_div_log;
      cell_y >>= hshg->cell_div_log;
      const hshg_cell_t min_cell_x = cell_x != 0 ? cell_x - 1 : 0;
      const hshg_cell_t min_cell_y = cell_y != 0 ? cell_y - 1 : 0;
      const hshg_cell_t max_cell_x = cell_x != grid->cells_mask ? cell_x + 1 : cell_x;
      const hshg_cell_t max_cell_y = cell_y != grid->cells_mask ? cell_y + 1 : cell_y;
      for(hshg_cell_t cur_y = min_cell_y; cur_y <= max_cell_y; ++cur_y) {
        for(hshg_cell_t cur_x = min_cell_x; cur_x <= max_cell_x; ++cur_x) {
          for(hshg_entity_t j = grid->cells[(hshg_cell_sq_t) cur_x | (cur_y << grid->cells_log)]; j != 0;) {
            const struct hshg_entity* const ent = hshg->entities + j;
            hshg->collide(hshg, entity, ent);
            j = ent->next;
          }
        }
      }
    }
  }
  hshg->calling = 0;
}

int hshg_optimize(struct hshg* const hshg) {
  assert(!hshg->calling && "hshg_optimize() may not be called from any callback");
  struct hshg_entity* const entities = malloc(sizeof(*hshg->entities) * hshg->entities_size);
  if(entities == NULL) {
    return -1;
  }
  hshg_entity_t idx = 1;
  for(uint8_t i = 0; i < hshg->grids_len; ++i) {
    const struct hshg_grid* const grid = hshg->grids + i;
    const hshg_cell_sq_t sq = grid->cells_side * grid->cells_side;
    for(hshg_cell_sq_t cell = 0; cell < sq; ++cell) {
      hshg_entity_t i = grid->cells[cell];
      if(i == 0) continue;
      grid->cells[cell] = idx;
      while(1) {
        struct hshg_entity* const entity = entities + idx;
        *entity = hshg->entities[i];
        if(entity->prev != 0) {
          entity->prev = idx - 1;
        }
        ++idx;
        if(entity->next != 0) {
          i = entity->next;
          entity->next = idx;
        } else {
          break;
        }
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

void hshg_query(struct hshg* const hshg, const hshg_pos_t _x1, const hshg_pos_t _y1, const hshg_pos_t _x2, const hshg_pos_t _y2) {
  /* ^ +y
     -------------
     |      x2,y2|
     |           |
     |x1,y1      |
     -------------> +x */
  assert(hshg->query);
  assert(_x1 <= _x2);
  assert(_y1 <= _y2);
  
  const uint8_t old_calling = hshg->calling;
  hshg->calling = 1;

  const hshg_pos_t inverse_grid_size = 1.0f / hshg->grid_size;

  hshg_pos_t x1;
  hshg_pos_t x2;
  if(_x1 < 0) {
    const hshg_pos_t shift = (((hshg_cell_t)(-_x1 * inverse_grid_size) << 1) + 2) * hshg->grid_size;
    x1 = _x1 + shift;
    x2 = _x2 + shift;
  } else {
    x1 = _x1;
    x2 = _x2;
  }
  hshg_cell_t start_x;
  hshg_cell_t end_x;
  hshg_cell_t folds = (x2 - (hshg_cell_t)(x1 * inverse_grid_size) * hshg->grid_size) * inverse_grid_size;
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
    const hshg_pos_t shift = (((hshg_cell_t)(-_y1 * inverse_grid_size) << 1) + 2) * hshg->grid_size;
    y1 = _y1 + shift;
    y2 = _y2 + shift;
  } else {
    y1 = _y1;
    y2 = _y2;
  }
  hshg_cell_t start_y;
  hshg_cell_t end_y;
  folds = (y2 - (hshg_cell_t)(y1 * inverse_grid_size) * hshg->grid_size) * inverse_grid_size;
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
        for(hshg_entity_t j = grid->cells[(hshg_cell_sq_t) x | (y << grid->cells_log)]; j != 0;) {
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
    start_x >>= hshg->cell_div_log;
    start_y >>= hshg->cell_div_log;
    end_x >>= hshg->cell_div_log;
    end_y >>= hshg->cell_div_log;
  }

  hshg->calling = old_calling;
}

#undef max
#undef min
