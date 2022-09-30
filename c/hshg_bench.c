#include "hshg.h"

#include <math.h>
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdatomic.h>

#define AGENTS_NUM 500000

#define CELLS_SIDE 2048
#define AGENT_R 7
#define CELL_SIZE 256
#define ARENA_WIDTH (CELLS_SIDE * CELL_SIZE)
#define ARENA_HEIGHT (CELLS_SIDE * CELL_SIZE)

/* If your hardware is really struggling,
decrease this for more frequent output. */
#define LATENCY_NUM 500

#define SINGLE_LAYER 1

struct ball {
  float vx;
  float vy;
};

struct ball balls[AGENTS_NUM];

void update(struct hshg* hshg, hshg_entity_t x) {
  struct hshg_entity* const a = hshg->entities + x;
  struct ball* const ball = balls + a->ref;

  a->x += ball->vx;
	if(a->x < a->r) {
		++ball->vx;
	} else if(a->x + a->r >= ARENA_WIDTH) {
		--ball->vx;
	}
  
	a->y += ball->vy;
	if(a->y < a->r) {
		++ball->vy;
	} else if(a->y + a->r >= ARENA_HEIGHT) {
		--ball->vy;
	}
  
  hshg_move(hshg, x);
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
  float* rands = malloc(sizeof(float) * AGENTS_NUM * 4);
  assert(rands);
  for(uint32_t i = 0; i < AGENTS_NUM * 4; ++i) {
    rands[i] = randf();
  }

  struct hshg hshg = {0};
  hshg.update = update;
  hshg.collide = collide;
  hshg.entities_size = AGENTS_NUM + 1;
  assert(!hshg_init(&hshg, CELLS_SIDE, CELL_SIZE));

  const uint64_t ins_time = get_time();
  for(hshg_entity_t i = 0; i < AGENTS_NUM; ++i) {
#if SINGLE_LAYER == 0
    float min_r = 999999.0f;
    for(int j = 0; j < 120; ++j) {
      float new = AGENT_R + randf() * 1000.0f;
      if(new < min_r) {
        min_r = new;
      }
    }
#else
    float min_r = AGENT_R;
#endif
    hshg_insert(&hshg, &((struct hshg_entity) {
      .x = rands[i * 4 + 0] * ARENA_WIDTH,
      .y = rands[i * 4 + 1] * ARENA_HEIGHT,
      .r = min_r,
      .ref = i
    }));
    balls[i].vx = rands[i * 4 + 2] * 8 - 4;
    balls[i].vy = rands[i * 4 + 3] * 8 - 4;
  }
  const uint64_t ins_end_time = get_time();
  free(rands);
  printf("took %" PRIu64 "ms to insert %d entities\n%" PRIu8 " grids\n\n", (ins_end_time - ins_time) / UINT64_C(1000000), AGENTS_NUM, hshg.grids_len);
  
  signal(SIGINT, sighandler);

  double upd[LATENCY_NUM];
  double opt[LATENCY_NUM];
  double col[LATENCY_NUM];
  int i = 0;
  while(1) {
    const uint64_t upd_time = get_time();
    hshg_update(&hshg);
    const uint64_t opt_time = get_time();
    assert(!hshg_optimize(&hshg));
    const uint64_t col_time = get_time();
    hshg_collide(&hshg);
    const uint64_t end_time = get_time();

    upd[i] = (double)(opt_time - upd_time) / 1000000.0;
    opt[i] = (double)(col_time - opt_time) / 1000000.0;
    col[i] = (double)(end_time - col_time) / 1000000.0;

    if(i + 1 == LATENCY_NUM) {
      double upd_avg = 0;
      for(int i = 0; i < LATENCY_NUM; ++i) {
        upd_avg += upd[i];
      }
      upd_avg /= LATENCY_NUM;

      double opt_avg = 0;
      for(int i = 0; i < LATENCY_NUM; ++i) {
        opt_avg += opt[i];
      }
      opt_avg /= LATENCY_NUM;
      
      double col_avg = 0;
      for(int i = 0; i < LATENCY_NUM; ++i) {
        col_avg += col[i];
      }
      col_avg /= LATENCY_NUM;
      
      printf("upd %.2lf ms\nopt %.2lf ms\ncol %.2lf ms\nall %.2lf ms\nattempted collisions %" PRIu64 "\nsucceeded collisions %" PRIu64 "\n",
        upd_avg, opt_avg, col_avg, upd_avg + opt_avg + col_avg, maybe_collisions, collisions);

      maybe_collisions = 0;
      collisions = 0;
    }
    i = (i + 1) % LATENCY_NUM;
    if(intr) {
      break;
    }
  }

  hshg_free(&hshg);

  return EXIT_SUCCESS;
}
