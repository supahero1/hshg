#define HSHG_D 2
#define HSHG_UNIFORM

#include "hshg.h"
#include "hshg.c"

#include <math.h>
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdatomic.h>

#ifndef BENCH_LITE

#define AGENTS_NUM 500000
#define CELLS_SIDE 2048

#else

#define AGENTS_NUM 100000
#define CELLS_SIDE 1024

#endif

#define AGENT_R 7
#define CELL_SIZE 16
#define ARENA_WIDTH (CELLS_SIDE * CELL_SIZE)
#define ARENA_HEIGHT (CELLS_SIDE * CELL_SIZE)

/* If your hardware is really struggling,
decrease this for more frequent output. */
#define LATENCY_NUM 100

#define SINGLE_LAYER 1


struct ball {
  float vx;
  float vy;
};

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


void update(struct hshg* hshg, struct hshg_entity* a) {
  struct ball* const ball = balls + a->ref;

  a->x += ball->vx;
  if(a->x < a->r) {
    ball->vx *= 0.9;
    ++ball->vx;
  } else if(a->x + a->r >= ARENA_WIDTH) {
    ball->vx *= 0.9;
    --ball->vx;
  }
  ball->vx *= 0.997;

  a->y += ball->vy;
  if(a->y < a->r) {
    ball->vy *= 0.9;
    ++ball->vy;
  } else if(a->y + a->r >= ARENA_HEIGHT) {
    ball->vy *= 0.9;
    --ball->vy;
  }
  ball->vy *= 0.997;

  hshg_move(hshg);
}

uint64_t maybe_collisions = 0;
uint64_t collisions = 0;

void collide(const struct hshg* hshg, const struct hshg_entity* a, const struct hshg_entity* b) {
  (void) hshg;
  const float r = a->r + b->r;
  ++maybe_collisions;
  const float xd = a->x - b->x;
  const float yd = a->y - b->y;
  const float d = xd * xd + yd * yd;
  if(d <= r * r) {
    ++collisions;
    const float angle = atan2f(yd, xd);
    const float c = cosf(angle);
    const float s = sinf(angle);
    balls[a->ref].vx += c;
    balls[a->ref].vy += s;
    balls[b->ref].vx -= c;
    balls[b->ref].vy -= s;
  }
}


uint64_t get_time(void) {
  struct timespec ts;
  (void) timespec_get(&ts, TIME_UTC);
  return UINT64_C(1000000000) * ts.tv_sec + ts.tv_nsec;
}

sig_atomic_t intr = 0;

void sighandler(int nil) {
  (void) nil;
  intr = 1;
}

float randf(void) {
  return (double) rand() / (double) RAND_MAX;
}


int main() {
  srand(get_time());
  signal(SIGINT, sighandler);

  balls = calloc(AGENTS_NUM, sizeof(*balls));
  assert(balls);

  float* init_data = malloc(sizeof(float) * AGENTS_NUM * 3);
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

  struct hshg* hshg = hshg_create(CELLS_SIDE, CELL_SIZE);
  assert(hshg);
  hshg->update = update;
  hshg->collide = collide;
  assert(!hshg_set_size(hshg, AGENTS_NUM + 1));

  const uint64_t ins_time = get_time();
  for(hshg_entity_t i = 0; i < AGENTS_NUM; ++i) {
    assert(!hshg_insert(hshg, init_data[i * 3 + 0], init_data[i * 3 + 1], init_data[i * 3 + 2], i));
  }
  const uint64_t ins_end_time = get_time();
  free(init_data);
  printf("took %" PRIu64 "ms to insert %d entities\n%" PRIu8 " grids\n\n", (ins_end_time - ins_time) / UINT64_C(1000000), AGENTS_NUM, hshg->grids_len);

  double upd[LATENCY_NUM];
  double opt[LATENCY_NUM];
  double col[LATENCY_NUM];
  int i = 0;
  puts("--- | average |  sd  | +- 0.1ms | +- 0.4ms | +- 0.7ms | +- 1.0ms | col stats |");
  while(1) {
    const uint64_t upd_time = get_time();
    hshg_update(hshg);
    const uint64_t opt_time = get_time();
    if(i % 32 == 0) {
      assert(!hshg_optimize(hshg));
      balls_optimize(hshg);
    }
    const uint64_t col_time = get_time();
    hshg_collide(hshg);
    const uint64_t end_time = get_time();

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

      double upd_01 = 0;
      for(i = 0; i < LATENCY_NUM; ++i) {
        if(upd[i] >= upd_avg - 0.1 && upd[i] <= upd_avg + 0.1) {
          ++upd_01;
        }
      }
      upd_01 = upd_01 / LATENCY_NUM * 100;

      double upd_04 = 0;
      for(i = 0; i < LATENCY_NUM; ++i) {
        if(upd[i] >= upd_avg - 0.4 && upd[i] <= upd_avg + 0.4) {
          ++upd_04;
        }
      }
      upd_04 = upd_04 / LATENCY_NUM * 100;

      double upd_07 = 0;
      for(i = 0; i < LATENCY_NUM; ++i) {
        if(upd[i] >= upd_avg - 0.7 && upd[i] <= upd_avg + 0.7) {
          ++upd_07;
        }
      }
      upd_07 = upd_07 / LATENCY_NUM * 100;

      double upd_10 = 0;
      for(i = 0; i < LATENCY_NUM; ++i) {
        if(upd[i] >= upd_avg - 1.0 && upd[i] <= upd_avg + 1.0) {
          ++upd_10;
        }
      }
      upd_10 = upd_10 / LATENCY_NUM * 100;

      double opt_avg = 0;
      for(i = 0; i < LATENCY_NUM; ++i) {
        opt_avg += opt[i];
      }
      opt_avg /= LATENCY_NUM;

      double col_avg = 0;
      for(i = 0; i < LATENCY_NUM; ++i) {
        col_avg += col[i];
      }
      col_avg /= LATENCY_NUM;

      double col_sd = 0;
      for(i = 0; i < LATENCY_NUM; ++i) {
        col_sd += (col[i] - col_avg) * (col[i] - col_avg);
      }
      col_sd = sqrtf(col_sd / LATENCY_NUM);

      double col_01 = 0;
      for(i = 0; i < LATENCY_NUM; ++i) {
        if(col[i] >= col_avg - 0.1 && col[i] <= col_avg + 0.1) {
          ++col_01;
        }
      }
      col_01 = col_01 / LATENCY_NUM * 100;

      double col_04 = 0;
      for(i = 0; i < LATENCY_NUM; ++i) {
        if(col[i] >= col_avg - 0.4 && col[i] <= col_avg + 0.4) {
          ++col_04;
        }
      }
      col_04 = col_04 / LATENCY_NUM * 100;

      double col_07 = 0;
      for(i = 0; i < LATENCY_NUM; ++i) {
        if(col[i] >= col_avg - 0.7 && col[i] <= col_avg + 0.7) {
          ++col_07;
        }
      }
      col_07 = col_07 / LATENCY_NUM * 100;

      double col_10 = 0;
      for(i = 0; i < LATENCY_NUM; ++i) {
        if(col[i] >= col_avg - 1.0 && col[i] <= col_avg + 1.0) {
          ++col_10;
        }
      }
      col_10 = col_10 / LATENCY_NUM * 100;

      printf(
        "------------------------------------------------------------------------------\n"
        "upd | %5.2lfms | %4.2lf |  %5.1lf%%  |  %5.1lf%%  |  %5.1lf%%  |  %5.1lf%%  | attempted |\n"
        "opt | %5.2lfms | ---- | -------- | -------- | -------- | -------- | %9." PRIu64 " |\n"
        "col | %5.2lfms | %4.2lf |  %5.1lf%%  |  %5.1lf%%  |  %5.1lf%%  |  %5.1lf%%  | succeeded |\n"
        "all | %5.2lfms | ---- | -------- | -------- | -------- | -------- | %9." PRIu64 " |\n",
        upd_avg,
        upd_sd,
        upd_01,
        upd_04,
        upd_07,
        upd_10,
        opt_avg,
        maybe_collisions,
        col_avg,
        col_sd,
        col_01,
        col_04,
        col_07,
        col_10,
        upd_avg + opt_avg + col_avg,
        collisions
      );

      maybe_collisions = 0;
      collisions = 0;
      i = LATENCY_NUM - 1;
    }
    i = (i + 1) % LATENCY_NUM;
    if(intr) {
      break;
    }
  }

  hshg_free(hshg);

  return EXIT_SUCCESS;
}
