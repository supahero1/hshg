#include "../../c/hshg.h"

#include <stdlib.h>

#include <node_api.h>

#define NAPI_CALL(call)                                         \
do {                                                            \
  napi_status status = (call);                                  \
  if(status != napi_ok) {                                       \
    const napi_extended_error_info* error_info = NULL;          \
    napi_get_last_error_info(env, &error_info);                 \
    bool is_pending;                                            \
    napi_is_exception_pending(env, &is_pending);                \
    if(!is_pending) {                                           \
      const char* message = (error_info->error_message == NULL) \
        ? "empty error message"                                 \
        : error_info->error_message;                            \
      napi_throw_error(env, NULL, message);                     \
      return NULL;                                              \
    }                                                           \
  }                                                             \
} while(0)

static napi_value hshg_new(napi_env env, napi_callback_info info) {
  napi_value instance;

  return instance;
}

NAPI_MODULE_INIT() {
  napi_value exports;
  napi_property_descriptor properties[] = {

  };
  NAPI_CALL(napi_define_class(env, "exports", NAPI_AUTO_LENGTH, hshg_new, NULL, sizeof(properties) / sizeof(properties[0]), properties, &exports));
  return exports;
}
