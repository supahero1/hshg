#include "../../c/hshg.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <node_api.h>

#define NAPI_CALL(call)                                \
do {                                                   \
  napi_status status = (call);                         \
  if(status != napi_ok) {                              \
    const napi_extended_error_info* error_info = NULL; \
    napi_get_last_error_info(env, &error_info);        \
    bool is_pending;                                   \
    napi_is_exception_pending(env, &is_pending);       \
    if(!is_pending) {                                  \
      const char* message =                            \
        (error_info->error_message == NULL)            \
        ? "empty error message"                        \
        : error_info->error_message;                   \
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

struct class_data {
  struct hshg hshg;
  napi_value update;
  napi_value collide;
  napi_value query;
  napi_ref update_ref;
  napi_ref collide_ref;
  napi_ref query_ref;
  uint32_t entity_id;
  uint8_t has_update:1;
  uint8_t has_collide:1;
  uint8_t has_query:1;
};

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

  hshg_set_size(&data->hshg, val);

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
  if(hshg_prealloc(&data->hshg, val)) OOM();

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

  uint32_t ref = 0;
  if(argc == 4) {
    NAPI_CALL(napi_get_value_uint32(env, argv[3], &ref));
  }

  if(hshg_insert(&data->hshg, &(struct hshg_entity) {
    .x = x,
    .y = y,
    .r = r,
    .ref = ref
  })) OOM();

  return NULL;
}

static napi_value class_remove(napi_env env, napi_callback_info info) {
  napi_value this;
  NAPI_CALL(napi_get_cb_info(env, info, NULL, NULL, &this, NULL));

  CHECK_TYPE();
  UNWRAP();

  uint32_t val;
  NAPI_CALL(napi_get_value_uint32(env, argv[0], &val));
  if(val == 0 || val >= data->hshg.entities_used) {
    napi_throw_error(env, NULL, "Invalid entity ID.");
    return NULL;
  }

  if(!data->in_update) {
    napi_throw_error(env, NULL, "This function is only callable from within hshg.update()'s callback.");
    return NULL;
  }

  hshg_remove(&data->hshg, val);

  return NULL;
}

static napi_value class_move(napi_env env, napi_callback_info info) {
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
  if(val == 0 || val >= data->hshg.entities_used) {
    napi_throw_error(env, NULL, "Invalid entity ID.");
    return NULL;
  }

  if(!data->in_update) {
    napi_throw_error(env, NULL, "This function is only callable from within hshg.update()'s callback.");
    return NULL;
  }

  hshg_move(&data->hshg, val);

  return NULL;
}

static napi_value class_resize(napi_env env, napi_callback_info info) {
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
  if(val == 0 || val >= data->hshg.entities_used) {
    napi_throw_error(env, NULL, "Invalid entity ID.");
    return NULL;
  }

  if(!data->in_update) {
    napi_throw_error(env, NULL, "This function is only callable from within hshg.update()'s callback.");
    return NULL;
  }

  hshg_resize(&data->hshg, val);

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
  };
  NAPI_CALL(napi_define_class(env, "HSHG", NAPI_AUTO_LENGTH, class_new, NULL, sizeof(properties) / sizeof(properties[0]), properties, &exports));
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, module_init)
