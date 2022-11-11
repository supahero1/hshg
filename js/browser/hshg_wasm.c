#include "hshg.h"

#include <math.h>
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>

#include <emscripten.h>
#include <emscripten/html5.h>

#define AGENTS_NUM 4096

#define CELLS_SIDE 256
#define AGENT_R 7
#define CELL_SIZE 16
#define ARENA_WIDTH (CELLS_SIDE * CELL_SIZE)
#define ARENA_HEIGHT (CELLS_SIDE * CELL_SIZE)

#define LATENCY_NUM 300

#define SINGLE_LAYER 0
#define DELETE_BALLS 1


struct ball {
  float vx;
  float vy;
};

float* init_data = NULL;
struct ball* balls = NULL;
uint32_t ball_idx = 0;
struct ball* balls_new = NULL;

void balls_update(struct hshg* hshg, struct hshg_entity* a) {
  balls_new[ball_idx] = balls[a->ref];
  a->ref = ball_idx;
  ++ball_idx;
}

void balls_optimize(struct hshg* hshg) {
  balls_new = calloc(AGENTS_NUM, sizeof(*balls_new));
  assert(balls_new);
  ball_idx = 0;
  hshg_update_t old = hshg->update;
  hshg->update = balls_update;
  hshg_update(hshg);
  free(balls);
  balls = balls_new;
  hshg->update = old;
}


unsigned int i = 0;
float sin_values[100];
#define SINS (sizeof(sin_values) / sizeof(*sin_values))

float randf(void) {
  return (double) rand() / (double) RAND_MAX;
}

void update(struct hshg* hshg, struct hshg_entity* a) {
  struct ball* const ball = balls + a->ref;

#if DELETE_BALLS == 1
  if(i % AGENTS_NUM == a->ref) {
    const hshg_entity_t ref = a->ref;
    hshg_remove(hshg);
    assert(!hshg_insert(hshg, init_data[ref * 3 + 0], init_data[ref * 3 + 1], init_data[ref * 3 + 2] + AGENT_R * 2, ref));
    return;
  }
#endif

  a->x += ball->vx;
  if(a->x < a->r) {
    ball->vx *= 0.9;
    ++ball->vx;
  } else if(a->x + a->r >= ARENA_WIDTH) {
    ball->vx *= 0.9;
    --ball->vx;
  }
  ball->vx *= 0.98;
  
  a->y += ball->vy;
  if(a->y < a->r) {
    ball->vy *= 0.9;
    ++ball->vy;
  } else if(a->y + a->r >= ARENA_HEIGHT) {
    ball->vy *= 0.9;
    --ball->vy;
  }
  ball->vy *= 0.98;

  hshg_move(hshg);

#if SINGLE_LAYER == 0
  a->r += sin_values[i % SINS] * (50.0f / SINS);
  hshg_resize(hshg);
#endif
}

uint64_t maybe_collisions = 0;
uint64_t collisions = 0;

void collide(const struct hshg* hshg, const struct hshg_entity* a, const struct hshg_entity* b) {
  (void) hshg;
  const float xd = a->x - b->x;
  const float yd = a->y - b->y;
  const float d = xd * xd + yd * yd;
  ++maybe_collisions;
  if(d <= (a->r + b->r) * (a->r + b->r)) {
    ++collisions;
    const float angle = atan2f(yd, xd);
    const float c = cosf(angle);
    const float s = sinf(angle);
    float m_diff = b->r / a->r;
    balls[a->ref].vx += c * m_diff;
    balls[a->ref].vy += s * m_diff;
    m_diff = a->r / b->r;
    balls[b->ref].vx -= c * m_diff;
    balls[b->ref].vy -= s * m_diff;
  }
}

uint32_t width;
uint32_t height;

void query(const struct hshg* hshg, const struct hshg_entity* a) {
  (void) hshg;
  EM_ASM({
    window.ctx.beginPath();
    window.ctx.arc($0, $1, $2, 0, Math.PI * 2);
    window.ctx.strokeStyle = "#ddd";
    window.ctx.lineWidth = 1;
    window.ctx.stroke();
  }, a->x - ((ARENA_WIDTH >> 1) - width), a->y - ((ARENA_HEIGHT >> 1) - height), a->r);
}


uint64_t get_time(void) {
  struct timespec ts;
  (void) timespec_get(&ts, TIME_UTC);
  return UINT64_C(1000000000) * ts.tv_sec + ts.tv_nsec;
}

struct hshg hshg = {0};

double upd[LATENCY_NUM];
double opt[LATENCY_NUM];
double col[LATENCY_NUM];

EM_BOOL tick(double nil1, void* nil2) {
  (void) nil1;
  (void) nil2;

  EM_ASM({
    window.ctx.clearRect(0, 0, window.canvas.width, window.canvas.height);
  });

  width = EM_ASM_INT({
    return window.canvas.width;
  }) >> 1;
  height = EM_ASM_INT({
    return window.canvas.height;
  }) >> 1;

  const uint64_t upd_time = get_time();
  hshg_update(&hshg);
  const uint64_t opt_time = get_time();
  if(i % 64 == 0) {
    assert(!hshg_optimize(&hshg));
    balls_optimize(&hshg);
  }
  const uint64_t col_time = get_time();
  hshg_collide(&hshg);
  const uint64_t end_time = get_time();
  hshg_query(&hshg, (ARENA_WIDTH >> 1) - width, (ARENA_HEIGHT >> 1) - height, (ARENA_WIDTH >> 1) + width, (ARENA_HEIGHT >> 1) + height);

  upd[i] = (double)(opt_time - upd_time) / 1000000.0;
  opt[i] = (double)(col_time - opt_time) / 1000000.0;
  col[i] = (double)(end_time - col_time) / 1000000.0;

  if(i + 1 == LATENCY_NUM) {
    double upd_avg = 0;
    for(i = 0; i < LATENCY_NUM; ++i) {
      upd_avg += upd[i];
    }
    upd_avg /= LATENCY_NUM;

    double upd_sd = 0;
    for(i = 0; i < LATENCY_NUM; ++i) {
      upd_sd += (upd[i] - upd_avg) * (upd[i] - upd_avg);
    }
    upd_sd = sqrtf(upd_sd / LATENCY_NUM);

    double opt_avg = 0;
    for(int i = 0; i < LATENCY_NUM; ++i) {
      opt_avg += opt[i];
    }
    opt_avg /= LATENCY_NUM;

    double opt_sd = 0;
    for(i = 0; i < LATENCY_NUM; ++i) {
      opt_sd += (opt[i] - opt_avg) * (opt[i] - opt_avg);
    }
    opt_sd = sqrtf(opt_sd / LATENCY_NUM);
      
    double col_avg = 0;
    for(int i = 0; i < LATENCY_NUM; ++i) {
      col_avg += col[i];
    }
    col_avg /= LATENCY_NUM;

    double col_sd = 0;
    for(i = 0; i < LATENCY_NUM; ++i) {
      col_sd += (col[i] - col_avg) * (col[i] - col_avg);
    }
    col_sd = sqrtf(col_sd / LATENCY_NUM);
      
    printf(
      "--- | average |  sd  | col stats |\n"
      "upd | %5.2lfms | %4.2lf | attempted |\n"
      "opt | %5.2lfms | %4.2lf | %9." PRIu64 " |\n"
      "col | %5.2lfms | %4.2lf | succeeded |\n"
      "all | %5.2lfms | ---- | %9." PRIu64 " |\n",
      upd_avg,
      upd_sd,
      opt_avg,
      opt_sd,
      maybe_collisions,
      col_avg,
      col_sd,
      upd_avg + opt_avg + col_avg,
      collisions
    );

    maybe_collisions = 0;
    collisions = 0;
    i = LATENCY_NUM - 1;
  }
  i = (i + 1) % LATENCY_NUM;

  return EM_TRUE;
}

int main() {
  EM_ASM({
    const canvas = window.canvas = document.getElementById("canvas");
    const ctx = window.ctx = window.canvas.getContext("2d");
    function resize() {
      canvas.width = window.innerWidth * window.devicePixelRatio;
      canvas.height = window.innerHeight * window.devicePixelRatio;
    }
    resize();
    window.onresize = resize;
    function draw() {
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      requestAnimationFrame(draw);
    }
    draw();
  });

  srand(get_time());

  balls = calloc(AGENTS_NUM, sizeof(*balls));
  assert(balls);

  init_data = malloc(sizeof(float) * AGENTS_NUM * 3);
  assert(init_data);

  for(hshg_entity_t i = 0; i < AGENTS_NUM; ++i) {
#if SINGLE_LAYER == 0
    float min_r = 999999.0f;
    for(int j = 0; j < 120; ++j) {
      float new = AGENT_R + randf() * 1000.0f;
      if(new < min_r) {
        min_r = new;
      }
    }
    if(min_r > 384.0f) {
      min_r = 384.0f;
    }
#else
    float min_r = AGENT_R;
#endif
    init_data[i * 3 + 0] = randf() * ARENA_WIDTH;
    init_data[i * 3 + 1] = randf() * ARENA_HEIGHT;
    init_data[i * 3 + 2] = min_r;
    balls[i].vx = randf() * 1 - 0.5;
    balls[i].vy = randf() * 1 - 0.5;
  }

  hshg.update = update;
  hshg.collide = collide;
  hshg.query = query;
  hshg.entities_size = AGENTS_NUM + 1;
  assert(!hshg_init(&hshg, CELLS_SIDE, CELL_SIZE));
#if SINGLE_LAYER == 0
  assert(!hshg_prealloc(&hshg, 500.0f));
#endif

  const float fragment = M_PI * 2 / SINS;
  for(int s = 0; s < SINS; ++s) {
    sin_values[s] = sinf(fragment * s);
  }

  const uint64_t ins_time = get_time();
  for(hshg_entity_t i = 0; i < AGENTS_NUM; ++i) {
    hshg_insert(&hshg, init_data[i * 3 + 0], init_data[i * 3 + 1], init_data[i * 3 + 2], i);
  }
  const uint64_t ins_end_time = get_time();
  printf("took %" PRIu64 "ms to insert %d entities\n%" PRIu8 " grids\n\n", (ins_end_time - ins_time) / UINT64_C(1000000), AGENTS_NUM, hshg.grids_len);

  emscripten_request_animation_frame_loop(tick, NULL);

  return 0;
}