#include "../../c/hshg.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>

#include <node_api.h>

#define NAPI_CALL(call)                                \
do {                                                   \
  napi_status status = (call);                         \
  if(__builtin_expect(status != napi_ok, 0)) {         \
    const napi_extended_error_info* error_info = NULL; \
    napi_get_last_error_info(env, &error_info);        \
    bool is_pending;                                   \
    napi_is_exception_pending(env, &is_pending);       \
    if(!is_pending) {                                  \
      const char* message;                             \
      char info[256];                                  \
      if(error_info->error_message == NULL) {          \
        snprintf(info, 256, "addon.c:%d", __LINE__);   \
        message = info;                                \
      } else {                                         \
        message = error_info->error_message;           \
      }                                                \
      napi_throw_error(env, NULL, message);            \
      return NULL;                                     \
    }                                                  \
  }                                                    \
} while(0)

#define OOM() \
do { \
  napi_throw_error(env, NULL, "Out of memory."); \
  return NULL; \
} while(0)

static const char* th(const uint8_t num) {
  switch(num % 10) {
    case 1 : return "st";
    case 2 : return "nd";
    case 3 : return "rd";
    default: return "th";
  }
}

#define CHECK_NUMBER(id) \
do { \
  napi_valuetype type; \
  NAPI_CALL(napi_typeof(env, argv[id], &type)); \
  if(type != napi_number) { \
    char err[64]; \
    (void) sprintf(err, "The %u%s argument must be a number.", (id + 1), th(id + 1)); \
    napi_throw_error(env, NULL, err); \
    return NULL; \
  } \
} while(0)

#define CHECK_TYPE() \
do { \
  bool correct_type; \
  NAPI_CALL(napi_check_object_type_tag(env, this, &hshg_type_tag, &correct_type)); \
  if(!correct_type) { \
    napi_throw_error(env, NULL, "Illegal invocation."); \
    return NULL; \
  } \
} while(0)

#define UNWRAP() \
struct class_data* data; \
do { \
  void* res; \
  NAPI_CALL(napi_unwrap(env, this, &res)); \
  data = res; \
} while(0)

#define CHECK_ARGC(min, max) \
do { \
  if(argc < min || argc > max) { \
    char err[128]; \
    if(min != max) { \
      (void) sprintf(err, "The number of arguments must be between %d and %d inclusive, but got %zu.", min, max, argc); \
    } else { \
      (void) sprintf(err, "Expected %d argument%s, but got %zu.", min, min == 1 ? "" : "s", argc); \
    } \
    napi_throw_error(env, NULL, err); \
    return NULL; \
  } \
} while(0)

static const napi_type_tag hshg_type_tag = {
  0xea2573fc2b12fd69, 0xf94a614e617f231f
};

#define _class_data           \
  napi_env env;               \
  napi_value this;            \
  napi_value js_entity;       \
  struct hshg_entity* entity; \
  napi_ref update_ref;        \
  napi_ref collide_ref;       \
  napi_ref query_ref;         \
  uint8_t has_update:1;       \
  uint8_t has_collide:1;      \
  uint8_t has_query:1;        \
  uint8_t in_update:1;        \
  uint8_t calling:1;          \
  uint8_t called_prealloc:1

struct class_data {
  struct hshg hshg;
  _class_data;
};

struct const_class_data {
  const struct hshg hshg;
  _class_data;
};

static napi_value __hshg_update(struct class_data* data, struct hshg_entity* entity) {
  napi_env env = data->env;
  data->entity = entity;
  NAPI_CALL(napi_create_object(data->env, &data->js_entity));
  napi_value temp;

  NAPI_CALL(napi_create_double(data->env, entity->x, &temp));
  NAPI_CALL(napi_set_named_property(data->env, data->js_entity, "x", temp));
  NAPI_CALL(napi_create_double(data->env, entity->y, &temp));
  NAPI_CALL(napi_set_named_property(data->env, data->js_entity, "y", temp));
  NAPI_CALL(napi_create_double(data->env, entity->r, &temp));
  NAPI_CALL(napi_set_named_property(data->env, data->js_entity, "r", temp));
  NAPI_CALL(napi_create_uint32(data->env, entity->ref, &temp));
  NAPI_CALL(napi_set_named_property(data->env, data->js_entity, "ref", temp));
  
  napi_value cb = NULL;
  NAPI_CALL(napi_get_reference_value(data->env, data->update_ref, &cb));
  NAPI_CALL(napi_call_function(data->env, data->this, cb, 1, &data->js_entity, NULL));

  uint32_t ref;
  NAPI_CALL(napi_get_named_property(data->env, data->js_entity, "ref", &temp));
  NAPI_CALL(napi_get_value_uint32(data->env, temp, &ref));
  entity->ref = ref;

  return NULL;
}

static void _hshg_update(struct hshg* hshg, struct hshg_entity* entity) {
  (void) __hshg_update((struct class_data*) hshg, entity);
}

static napi_value __hshg_collide(struct const_class_data* data, const struct hshg_entity* ent_a, const struct hshg_entity* ent_b) {
  napi_env env = data->env;
  napi_value obj[2];
  NAPI_CALL(napi_create_object(data->env, obj + 0));
  NAPI_CALL(napi_create_object(data->env, obj + 1));
  napi_value temp;

  NAPI_CALL(napi_create_double(data->env, ent_a->x, &temp));
  NAPI_CALL(napi_set_named_property(data->env, obj[0], "x", temp));
  NAPI_CALL(napi_create_double(data->env, ent_a->y, &temp));
  NAPI_CALL(napi_set_named_property(data->env, obj[0], "y", temp));
  NAPI_CALL(napi_create_double(data->env, ent_a->r, &temp));
  NAPI_CALL(napi_set_named_property(data->env, obj[0], "r", temp));
  NAPI_CALL(napi_create_uint32(data->env, ent_a->ref, &temp));
  NAPI_CALL(napi_set_named_property(data->env, obj[0], "ref", temp));

  NAPI_CALL(napi_create_double(data->env, ent_b->x, &temp));
  NAPI_CALL(napi_set_named_property(data->env, obj[1], "x", temp));
  NAPI_CALL(napi_create_double(data->env, ent_b->y, &temp));
  NAPI_CALL(napi_set_named_property(data->env, obj[1], "y", temp));
  NAPI_CALL(napi_create_double(data->env, ent_b->r, &temp));
  NAPI_CALL(napi_set_named_property(data->env, obj[1], "r", temp));
  NAPI_CALL(napi_create_uint32(data->env, ent_b->ref, &temp));
  NAPI_CALL(napi_set_named_property(data->env, obj[1], "ref", temp));

  napi_value cb = NULL;
  NAPI_CALL(napi_get_reference_value(data->env, data->collide_ref, &cb));
  NAPI_CALL(napi_call_function(data->env, data->this, cb, 2, obj, NULL));

  return NULL;
}

static void _hshg_collide(const struct hshg* hshg, const struct hshg_entity* ent_a, const struct hshg_entity* ent_b) {
  (void) __hshg_collide((struct const_class_data*) hshg, ent_a, ent_b);
}

static napi_value __hshg_query(struct const_class_data* data, const struct hshg_entity* entity) {
  napi_env env = data->env;
  NAPI_CALL(napi_create_object(data->env, &data->js_entity));
  napi_value temp;

  NAPI_CALL(napi_create_double(data->env, entity->x, &temp));
  NAPI_CALL(napi_set_named_property(data->env, data->js_entity, "x", temp));
  NAPI_CALL(napi_create_double(data->env, entity->y, &temp));
  NAPI_CALL(napi_set_named_property(data->env, data->js_entity, "y", temp));
  NAPI_CALL(napi_create_double(data->env, entity->r, &temp));
  NAPI_CALL(napi_set_named_property(data->env, data->js_entity, "r", temp));
  NAPI_CALL(napi_create_uint32(data->env, entity->ref, &temp));
  NAPI_CALL(napi_set_named_property(data->env, data->js_entity, "ref", temp));

  napi_value cb = NULL;
  NAPI_CALL(napi_get_reference_value(data->env, data->query_ref, &cb));
  NAPI_CALL(napi_call_function(data->env, data->this, cb, 1, &data->js_entity, NULL));

  return NULL;
}

static void _hshg_query(const struct hshg* hshg, const struct hshg_entity* entity) {
  (void) __hshg_query((struct const_class_data*) hshg, entity);
}

static napi_value __hshg_free(napi_env env, void* _data) {
  struct class_data* const data = _data;
  hshg_free(&data->hshg);
  if(data->has_update) {
    NAPI_CALL(napi_delete_reference(env, data->update_ref));
  }
  if(data->has_collide) {
    NAPI_CALL(napi_delete_reference(env, data->collide_ref));
  }
  if(data->has_query) {
    NAPI_CALL(napi_delete_reference(env, data->query_ref));
  }
  free(data);
  return NULL;
}

static void _hshg_free(napi_env env, void* data, void* hint) {
  (void) __hshg_free(env, data);
}

static napi_value class_new(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value argv[3];
  napi_value this;
  NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, &this, NULL));

  CHECK_ARGC(2, 3);
  for(uint8_t i = 0; i < argc; ++i) {
    CHECK_NUMBER(i);
  }

  uint32_t cells;
  NAPI_CALL(napi_get_value_uint32(env, argv[0], &cells));
  if(cells == 0 || cells > 65535) {
    char err[100];
    (void) sprintf(err, "First argument expected to be in range (0, 65536), but got %" PRIu32 ".", cells);
    napi_throw_error(env, NULL, err);
    return NULL;
  }
  if(__builtin_popcount(cells) != 1) {
    napi_throw_error(env, NULL, "First 2 arguments must be powers of 2 (first isn't).");
    return NULL;
  }

  uint32_t cell_size;
  NAPI_CALL(napi_get_value_uint32(env, argv[1], &cell_size));
  if(cell_size == 0) {
    char err[100];
    (void) sprintf(err, "Second argument expected to be greater than 0, but got %" PRIu32 ".", cell_size);
    napi_throw_error(env, NULL, err);
    return NULL;
  }
  if(__builtin_popcount(cell_size) != 1) {
    napi_throw_error(env, NULL, "First 2 arguments must be powers of 2 (second isn't).");
    return NULL;
  }

  uint32_t cell_div_log = 0;
  if(argc == 3) {
    NAPI_CALL(napi_get_value_uint32(env, argv[2], &cell_div_log));
    if(cell_div_log == 0 || cell_div_log > 15) {
      char err[100];
      (void) sprintf(err, "Third argument expected to be in range (0, 16), but got %" PRIu32 ".", cell_div_log);
      napi_throw_error(env, NULL, err);
      return NULL;
    }
  }

  struct class_data* const data = calloc(1, sizeof(*data));
  if(data == NULL) OOM();
  data->hshg.cell_div_log = cell_div_log;
  data->hshg.update = _hshg_update;
  data->hshg.collide = _hshg_collide;
  data->hshg.query = _hshg_query;
  if(hshg_init(&data->hshg, cells, cell_size)) {
    free(data);
    OOM();
  }

  NAPI_CALL(napi_type_tag_object(env, this, &hshg_type_tag));
  NAPI_CALL(napi_wrap(env, this, data, _hshg_free, NULL, NULL));

  return this;
}

static napi_value class_get_grids_len(napi_env env, napi_callback_info info) {
  napi_value this;
  NAPI_CALL(napi_get_cb_info(env, info, NULL, NULL, &this, NULL));

  CHECK_TYPE();
  UNWRAP();

  napi_value ret;
  NAPI_CALL(napi_create_uint32(env, data->hshg.grids_len, &ret));
  return ret;
}

static napi_value class_get_entities_used(napi_env env, napi_callback_info info) {
  napi_value this;
  NAPI_CALL(napi_get_cb_info(env, info, NULL, NULL, &this, NULL));

  CHECK_TYPE();
  UNWRAP();

  napi_value ret;
  NAPI_CALL(napi_create_uint32(env, data->hshg.entities_used, &ret));
  return ret;
}

static napi_value class_get_entities_size(napi_env env, napi_callback_info info) {
  napi_value this;
  NAPI_CALL(napi_get_cb_info(env, info, NULL, NULL, &this, NULL));

  CHECK_TYPE();
  UNWRAP();

  napi_value ret;
  NAPI_CALL(napi_create_uint32(env, data->hshg.entities_size, &ret));
  return ret;
}

static napi_value class_set_entities_size(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1];
  napi_value this;
  NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, &this, NULL));

  CHECK_TYPE();
  CHECK_ARGC(1, 1);
  CHECK_NUMBER(0);
  UNWRAP();

  uint32_t val;
  NAPI_CALL(napi_get_value_uint32(env, argv[0], &val));
  if(val < data->hshg.entities_used) {
    napi_throw_range_error(env, NULL, "You may not set entities_size to a value lower than entities_used.");
    return NULL;
  }

  if(hshg_set_size(&data->hshg, val)) OOM();

  return NULL;
}

static napi_value class_prealloc(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1];
  napi_value this;
  NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, &this, NULL));

  CHECK_TYPE();
  CHECK_ARGC(1, 1);
  CHECK_NUMBER(0);
  UNWRAP();

  double val;
  NAPI_CALL(napi_get_value_double(env, argv[0], &val));
  if(val <= 0) {
    napi_throw_range_error(env, NULL, "Provided radius (first argument) is not positive.");
    return NULL;
  }

  if(hshg_prealloc(&data->hshg, val)) OOM();
  data->called_prealloc = 1;

  return NULL;
}

static napi_value class_insert(napi_env env, napi_callback_info info) {
  size_t argc = 4;
  napi_value argv[4];
  napi_value this;
  NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, &this, NULL));

  CHECK_TYPE();
  CHECK_ARGC(3, 4);
  for(uint8_t i = 0; i < argc; ++i) {
    CHECK_NUMBER(i);
  }
  UNWRAP();

  double x;
  NAPI_CALL(napi_get_value_double(env, argv[0], &x));

  double y;
  NAPI_CALL(napi_get_value_double(env, argv[1], &y));

  double r;
  NAPI_CALL(napi_get_value_double(env, argv[2], &r));
  if(r <= 0) {
    napi_throw_range_error(env, NULL, "Provided radius (third argument) is not positive.");
    return NULL;
  }

  uint32_t ref = 0;
  if(argc == 4) {
    NAPI_CALL(napi_get_value_uint32(env, argv[3], &ref));
  }

  if(hshg_insert(&data->hshg, x, y, r, ref)) OOM();

  return NULL;
}

static napi_value class_remove(napi_env env, napi_callback_info info) {
  napi_value this;
  NAPI_CALL(napi_get_cb_info(env, info, NULL, NULL, &this, NULL));

  CHECK_TYPE();
  UNWRAP();

  if(!data->in_update) {
    napi_throw_error(env, NULL, "This function is only callable from within hshg.update()'s callback.");
    return NULL;
  }

  hshg_remove(&data->hshg);

  return NULL;
}

static napi_value class_move(napi_env env, napi_callback_info info) {
  napi_value this;
  NAPI_CALL(napi_get_cb_info(env, info, NULL, NULL, &this, NULL));

  CHECK_TYPE();
  UNWRAP();

  if(!data->in_update) {
    napi_throw_error(env, NULL, "This function is only callable from within hshg.update()'s callback.");
    return NULL;
  }

  napi_value temp;
  double _temp;

  NAPI_CALL(napi_get_named_property(data->env, data->js_entity, "x", &temp));
  NAPI_CALL(napi_get_value_double(data->env, temp, &_temp));
  data->entity->x = _temp;

  NAPI_CALL(napi_get_named_property(data->env, data->js_entity, "y", &temp));
  NAPI_CALL(napi_get_value_double(data->env, temp, &_temp));
  data->entity->y = _temp;

  hshg_move(&data->hshg);

  return NULL;
}

static napi_value class_resize(napi_env env, napi_callback_info info) {
  napi_value this;
  NAPI_CALL(napi_get_cb_info(env, info, NULL, NULL, &this, NULL));

  CHECK_TYPE();
  UNWRAP();

  if(!data->in_update) {
    napi_throw_error(env, NULL, "This function is only callable from within hshg.update()'s callback.");
    return NULL;
  }

  if(!data->called_prealloc) {
    napi_throw_error(env, NULL, "hshg.prealloc() was not called yet. Refer to the docs for more info.");
    return NULL;
  }

  napi_value temp;
  double r;

  NAPI_CALL(napi_get_named_property(data->env, data->js_entity, "r", &temp));
  NAPI_CALL(napi_get_value_double(data->env, temp, &r));
  data->entity->r = r;

  hshg_resize(&data->hshg);

  return NULL;
}

static napi_value class_update(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1];
  napi_value this;
  NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, &this, NULL));

  CHECK_TYPE();
  UNWRAP();

  if(argc == 0 && !data->has_update) {
    napi_throw_error(env, NULL, "No known callback for updates. Pass it as the first argument at least once.");
    return NULL;
  }

  if(data->in_update) {
    napi_throw_error(env, NULL, "hshg.update() calls may not be nested.");
    return NULL;
  }

  if(data->calling) {
    napi_throw_error(env, NULL, "hshg.update() may not be called from any other callback.");
    return NULL;
  }

  if(argc == 1) {
    if(data->has_update) {
      NAPI_CALL(napi_delete_reference(env, data->update_ref));
      data->has_update = 0;
    }
    napi_valuetype type;
    NAPI_CALL(napi_typeof(env, argv[0], &type));
    if(type != napi_function) {
      napi_throw_error(env, NULL, "The callback (first argument) must be a function.");
      return NULL;
    }
    NAPI_CALL(napi_create_reference(env, argv[0], 1, &data->update_ref));
    data->has_update = 1;
  }

  data->env = env;
  data->this = this;

  data->in_update = 1;
  data->calling = 1;
  hshg_update(&data->hshg);
  data->calling = 0;
  data->in_update = 0;

  return NULL;
}

static napi_value class_collide(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1];
  napi_value this;
  NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, &this, NULL));

  CHECK_TYPE();
  UNWRAP();

  if(argc == 0 && !data->has_collide) {
    napi_throw_error(env, NULL, "No known callback for collisions. Pass it as the first argument at least once.");
    return NULL;
  }

  if(data->calling) {
    napi_throw_error(env, NULL, "hshg.collide() may not be called from any other callback.");
    return NULL;
  }

  if(argc == 1) {
    if(data->has_collide) {
      NAPI_CALL(napi_delete_reference(env, data->collide_ref));
      data->has_collide = 0;
    }
    napi_valuetype type;
    NAPI_CALL(napi_typeof(env, argv[0], &type));
    if(type != napi_function) {
      napi_throw_error(env, NULL, "The callback (first argument) must be a function.");
      return NULL;
    }
    NAPI_CALL(napi_create_reference(env, argv[0], 1, &data->collide_ref));
    data->has_collide = 1;
  }

  data->env = env;
  data->this = this;

  data->calling = 1;
  hshg_collide(&data->hshg);
  data->calling = 0;

  return NULL;
}

static napi_value class_optimize(napi_env env, napi_callback_info info) {
  napi_value this;
  NAPI_CALL(napi_get_cb_info(env, info, NULL, NULL, &this, NULL));

  CHECK_TYPE();
  UNWRAP();

  if(data->calling) {
    napi_throw_error(env, NULL, "hshg.optimize() may not be called from any other callback.");
    return NULL;
  }

  if(hshg_optimize(&data->hshg)) OOM();

  return NULL;
}

static napi_value class_query(napi_env env, napi_callback_info info) {
  size_t argc = 5;
  napi_value argv[5];
  napi_value this;
  NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, &this, NULL));

  CHECK_TYPE();
  CHECK_ARGC(4, 5);
  for(uint8_t i = 0; i < 4; ++i) {
    CHECK_NUMBER(i);
  }
  UNWRAP();

  if(argc == 4 && !data->has_query) {
    napi_throw_error(env, NULL, "No known callback for queries. Pass it as the fifth argument at least once.");
    return NULL;
  }

  if(argc == 5) {
    if(data->has_query) {
      NAPI_CALL(napi_delete_reference(env, data->query_ref));
      data->has_query = 0;
    }
    napi_valuetype type;
    NAPI_CALL(napi_typeof(env, argv[4], &type));
    if(type != napi_function) {
      napi_throw_error(env, NULL, "The callback (fifth argument) must be a function.");
      return NULL;
    }
    NAPI_CALL(napi_create_reference(env, argv[4], 1, &data->query_ref));
    data->has_query = 1;
  }

  double min_x;
  NAPI_CALL(napi_get_value_double(env, argv[0], &min_x));

  double min_y;
  NAPI_CALL(napi_get_value_double(env, argv[1], &min_y));

  double max_x;
  NAPI_CALL(napi_get_value_double(env, argv[2], &max_x));

  if(max_x < min_x) {
    napi_throw_error(env, NULL, "Third argument must be greater or equal to the first argument.");
    return NULL;
  }

  double max_y;
  NAPI_CALL(napi_get_value_double(env, argv[3], &max_y));

  if(max_y < min_y) {
    napi_throw_error(env, NULL, "Fourth argument must be greater or equal to the second argument.");
    return NULL;
  }

  napi_env old_env = data->env;
  napi_value old_this = data->this;
  data->env = env;
  data->this = this;

  data->calling = 1;
  hshg_query(&data->hshg, min_x, min_y, max_x, max_y);
  data->calling = 0;

  data->this = old_this;
  data->env = old_env;

  return NULL;
}

static napi_value module_init(napi_env env, napi_value exports) {
  napi_property_descriptor properties[] = {
    { "grids_len"    , NULL, NULL          , class_get_grids_len    , NULL                   , NULL, napi_default_jsproperty, NULL },
    { "entities_used", NULL, NULL          , class_get_entities_used, NULL                   , NULL, napi_default_jsproperty, NULL },
    { "entities_size", NULL, NULL          , class_get_entities_size, class_set_entities_size, NULL, napi_default_jsproperty, NULL },
    { "prealloc"     , NULL, class_prealloc, NULL                   , NULL                   , NULL, napi_default_method    , NULL },
    { "insert"       , NULL, class_insert  , NULL                   , NULL                   , NULL, napi_default_method    , NULL },
    { "remove"       , NULL, class_remove  , NULL                   , NULL                   , NULL, napi_default_method    , NULL },
    { "move"         , NULL, class_move    , NULL                   , NULL                   , NULL, napi_default_method    , NULL },
    { "resize"       , NULL, class_resize  , NULL                   , NULL                   , NULL, napi_default_method    , NULL },
    { "update"       , NULL, class_update  , NULL                   , NULL                   , NULL, napi_default_method    , NULL },
    { "collide"      , NULL, class_collide , NULL                   , NULL                   , NULL, napi_default_method    , NULL },
    { "optimize"     , NULL, class_optimize, NULL                   , NULL                   , NULL, napi_default_method    , NULL },
    { "query"        , NULL, class_query   , NULL                   , NULL                   , NULL, napi_default_method    , NULL }
  };
  NAPI_CALL(napi_define_class(env, "HSHG", NAPI_AUTO_LENGTH, class_new, NULL, sizeof(properties) / sizeof(properties[0]), properties, &exports));
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, module_init)
