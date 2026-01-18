#include "file.h"
#include "jsonv.h"
#include <criterion/criterion.h>
#include <criterion/logging.h>
/* ---------- Helpers ---------- */
#define STR_HELPER(x, y) #x #y
#define STR(x, y) STR_HELPER(x, y)
static Arena *a = NULL;
static unsigned char *schema = NULL;
static Jsonv_Context *ctx = NULL;

static void setup(char *file) {
  /*#region*/
  // Default setup: 4KB blocks, 1MB limit, 12KB trim threshold
  a = arena_create(4096, 1024 * 1024, 3 * 4096);
  size_t size;
  char buff[100] = {0};
  snprintf(buff, 50, "./tests/schemas/%s.schema.json", file);
  schema = file_read(buff, &size);
  cr_assert(schema);
  cr_assert(a);
  bool e = jsonv_ctx_prepare_schema(&ctx, schema, a);
  cr_assert(e);
  /*#endregion*/
}

static void global_fini() {
  /*#region*/
  free(schema);
  jsonv_ctx_free(ctx);
  arena_destroy(a);
  /*#endregion*/
}

static bool validate(unsigned char *payload) {
  /*#region*/
  bool e = jsonv_ctx_prepare_data(&ctx, payload, a);
  cr_assert(e);
  if (e)
    e = jsonv_ctx_validate(ctx);
  return e;
  /*#endregion*/
}

#define T1 multipleOf
static void multipleOf_init() { setup(STR(T1, .2)); }
#define T2 maximum
static void maximum_init() { setup(STR(T2, .2)); }
#define T3 minimum
static void minimum_init() { setup(STR(T3, .2)); }
#define T4 exclusiveMinimum
static void exclusiveMinimum_init() { setup(STR(T4, .2)); }
#define T5 exclusiveMaximum
static void exclusiveMaximum_init() { setup(STR(T5, .2)); }

TestSuite(T1, .init = multipleOf_init, .fini = global_fini);

Test(T1, multipleOf_2) {
  /*#region*/
  char c1[] = "{ \"value\": 2 }";
  cr_assert(validate((unsigned char *)c1));
  char c2[] = "{ \"value\": 3 }";
  cr_assert(!validate((unsigned char *)c2));
  /*#endregion*/
}

TestSuite(T2, .init = maximum_init, .fini = global_fini);

Test(T2, maximum_2) {
  /*#region*/
  char c1[] = "{ \"value\": 2 }";
  cr_assert(validate((unsigned char *)c1));
  char c2[] = "{ \"value\": 3 }";
  cr_assert(!validate((unsigned char *)c2));
  /*#endregion*/
}

TestSuite(T3, .init = minimum_init, .fini = global_fini);

Test(T3, minimum_2) {
  /*#region*/
  char c1[] = "{ \"value\": 2 }";
  cr_assert(validate((unsigned char *)c1));
  char c2[] = "{ \"value\": 1 }";
  cr_assert(!validate((unsigned char *)c2));
  /*#endregion*/
}

TestSuite(T4, .init = exclusiveMinimum_init, .fini = global_fini);

Test(T4, exclusiveMinimum_2) {
  /*#region*/
  char c1[] = "{ \"value\": 3 }";
  cr_assert(validate((unsigned char *)c1));
  char c2[] = "{ \"value\": 2 }";
  cr_assert(!validate((unsigned char *)c2));
  /*#endregion*/
}

TestSuite(T5, .init = exclusiveMaximum_init, .fini = global_fini);

Test(T5, exclusiveMaximum_2) {
  /*#region*/
  char c1[] = "{ \"value\": 1 }";
  cr_assert(validate((unsigned char *)c1));
  char c2[] = "{ \"value\": 2 }";
  cr_assert(!validate((unsigned char *)c2));
  /*#endregion*/
}
