#define HSHG_UNIFORM
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

#define AGENTS_NUM EXCL_1D(500000) EXCL_2D(500000) EXCL_3D(300000)
#define CELLS_SIDE EXCL_1D(2048 * 2048) EXCL_2D(2048) EXCL_3D(128)

#else

#define AGENTS_NUM EXCL_1D(100000) EXCL_2D(100000) EXCL_3D(100000)
#define CELLS_SIDE EXCL_1D(512 * 512) EXCL_2D(1024) EXCL_3D(128)

#endif

#define AGENT_R 7
#define CELL_SIZE 16
#define ARENA_WIDTH (CELLS_SIDE * CELL_SIZE)
#define ARENA_HEIGHT (CELLS_SIDE * CELL_SIZE)
#define ARENA_DEPTH (CELLS_SIDE * CELL_SIZE)

/* If your hardware is really struggling,
decrease this for more frequent output. */
#define LATENCY_NUM 1000

#define SINGLE_LAYER 1


struct ball
{
    float vx;
_2D(float vy;)
_3D(float vz;)
};

struct ball* balls = NULL;
struct ball* ball = 0;
struct ball* balls_new = NULL;
hshg_entity_t ball_idx = 0;


void
balls_update(struct hshg* hshg, struct hshg_entity* a)
{
    *ball = balls[a->ref];
    a->ref = ball_idx;
    ++ball;
    ++ball_idx;
}


void
balls_optimize(struct hshg* hshg)
{
    balls_new = calloc(AGENTS_NUM, sizeof(*balls_new));

    assert(balls_new);

    ball_idx = 0;
    ball = balls_new;

    hshg_update_t old = hshg->update;

    hshg->update = balls_update;

    hshg_update(hshg);

    free(balls);

    balls = balls_new;
    hshg->update = old;
}


void
update(struct hshg* hshg, struct hshg_entity* a)
{
    struct ball* const ball = balls + a->ref;

    a->x += ball->vx;

    if(a->x < a->r)
    {
      ball->vx *= 0.9;
      ++ball->vx;
    }
    else if(a->x + a->r >= ARENA_WIDTH)
    {
      ball->vx *= 0.9;
      --ball->vx;
    }

_2D(
    a->y += ball->vy;

    if(a->y < a->r)
    {
      ball->vy *= 0.9;
      ++ball->vy;
    }
    else if(a->y + a->r >= ARENA_HEIGHT)
    {
      ball->vy *= 0.9;
      --ball->vy;
    }
)

_3D(
    a->z += ball->vz;

    if(a->z < a->r)
    {
      ball->vz *= 0.9;
      ++ball->vz;
    }
    else if(a->z + a->r >= ARENA_HEIGHT)
    {
      ball->vz *= 0.9;
      --ball->vz;
    }
)

    hshg_move(hshg);
}


uint64_t maybe_collisions = 0;
uint64_t collisions = 0;


float
inv_sqrt(float x)
{
  union
  {
    float f;
    uint32_t i;
  } y;

  y.f = x;
  y.i = 0x5f3759df - (y.i >> 1);

  return y.f * (1.5f - 0.5f * x * y.f * y.f);
}

void
collide(const struct hshg* hshg,
  const struct hshg_entity* restrict a, const struct hshg_entity* restrict b)
{
    (void) hshg;
    ++maybe_collisions;

    const float dr = a->r + b->r;
    float dx = a->x - b->x;
_2D(float dy = a->y - b->y;)
_3D(float dz = a->z - b->z;)
    const float dist = dx * dx _2D(+ dy * dy) _3D(+ dz * dz);

    if(dist <= dr * dr)
    {
        ++collisions;
        const float mag = inv_sqrt(dist);

        struct ball* const ball_a = balls + a->ref;
        struct ball* const ball_b = balls + b->ref;

        dx *= mag;
    _2D(dy *= mag;)
    _3D(dz *= mag;)

        ball_a->vx *= 0.75;
        ball_a->vx += dx;

    _2D(ball_a->vy *= 0.75;)
    _2D(ball_a->vy += dy;)

    _3D(ball_a->vz *= 0.75;)
    _3D(ball_a->vz += dz;)

        ball_b->vx *= 0.75;
        ball_b->vx -= dx;

    _2D(ball_b->vy *= 0.75;)
    _2D(ball_b->vy -= dy;)

    _3D(ball_b->vz *= 0.75;)
    _3D(ball_b->vz -= dz;)
    }
}


uint32_t queried_refs[AGENTS_NUM];
uint32_t queried_len = 0;


void
query(const struct hshg* hshg, const struct hshg_entity* const a)
{
    queried_refs[queried_len++] = a->ref;
    queried_len = (queried_len + 1) % AGENTS_NUM;
}


uint64_t
get_time(void)
{
  struct timespec ts;

  (void) timespec_get(&ts, TIME_UTC);

  return UINT64_C(1000000000) * ts.tv_sec + ts.tv_nsec;
}


sig_atomic_t intr = 0;


void
sighandler(int nil)
{
  (void) nil;
  intr = 1;
}


float
randf(void)
{
  return (double) rand() / (double) RAND_MAX;
}


int
main()
{
    srand(get_time());
    signal(SIGINT, sighandler);

    balls = calloc(AGENTS_NUM, sizeof(*balls));

    assert(balls);

    const int mul = 2 _2D(+ 1) _3D(+ 1);

    float* init_data = malloc(sizeof(float) * AGENTS_NUM * mul);

    assert(init_data);

    for(hshg_entity_t i = 0; i < AGENTS_NUM; ++i)
    {

  #if SINGLE_LAYER == 0

        float min_r = 999999.0f;

        for(int j = 0; j < 120; ++j)
        {
            float new = AGENT_R + randf() * 1000.0f;

            if(new < min_r)
            {
                min_r = new;
            }
        }

        if(min_r > 384.0f)
        {
            min_r = 384.0f;
        }

  #else

        float min_r = AGENT_R;

  #endif

        init_data[i * mul + 0] = randf() * ARENA_WIDTH;
    _2D(init_data[i * mul + 2] = randf() * ARENA_HEIGHT;)
    _3D(init_data[i * mul + 3] = randf() * ARENA_DEPTH;)
        init_data[i * mul + 1] = min_r;
        balls[i].vx = randf() * 1 - 0.5;
    _2D(balls[i].vy = randf() * 1 - 0.5;)
    _3D(balls[i].vz = randf() * 1 - 0.5;)
    }

    struct hshg* hshg = hshg_create(CELLS_SIDE, CELL_SIZE);

    assert(hshg);

    hshg->update = update;
    hshg->collide = collide;
    hshg->query = query;

    assert(!hshg_set_size(hshg, AGENTS_NUM + 1));

    const uint64_t ins_time = get_time();

    for(hshg_entity_t i = 0; i < AGENTS_NUM; ++i)
    {
        assert(
            !hshg_insert(hshg,
                init_data[i * mul + 0] _2D(, init_data[i * mul + 2])
                _3D(, init_data[i * mul + 3]), init_data[i * mul + 1], i)
        );
    }

    const uint64_t ins_end_time = get_time();

    printf("took %" PRIu64 "ms to insert %d entities\n%" PRIu8 " grids\n\n",
    (ins_end_time - ins_time) / UINT64_C(1000000), AGENTS_NUM, hshg->grids_len);

    double upd[LATENCY_NUM];
    double opt[LATENCY_NUM];
    double col[LATENCY_NUM];
    double qry[LATENCY_NUM];
    int i = 0;

    puts("--- | average |  sd  | +- 0.1ms | +- 0.4ms | +- 0.7ms | +- 1.0ms | col stats |");

    while(1)
    {
        const uint64_t opt_time = get_time();

        assert(!hshg_optimize(hshg));

        balls_optimize(hshg);

        const uint64_t col_time = get_time();

        hshg_collide(hshg);

        const uint64_t qry_time = get_time();

        const float* ptr = init_data + queried_len * mul;

        for(uint8_t i = 0; i < 255 >> (1 << (HSHG_D - 1)); ++i)
        {
            hshg_query(hshg
                , *(ptr + 0) - (1920 / 2)
            _2D(, *(ptr + 2) - (1080 / 2))
            _3D(, *(ptr + 3) - (1080 / 2))
                , *(ptr + 0) + (1920 / 2)
            _2D(, *(ptr + 2) + (1080 / 2))
            _3D(, *(ptr + 3) + (1080 / 2))
            );

            ++ptr;
        }

        const uint64_t upd_time = get_time();

        hshg_update(hshg);

        const uint64_t end_time = get_time();

        opt[i] = (double)(col_time - opt_time) / 1000000.0;
        col[i] = (double)(qry_time - col_time) / 1000000.0;
        qry[i] = (double)(upd_time - qry_time) / 1000000.0;
        upd[i] = (double)(end_time - upd_time) / 1000000.0;

        if(i + 1 == LATENCY_NUM)
        {
            double opt_avg = 0;

            for(i = 0; i < LATENCY_NUM; ++i)
            {
                opt_avg += opt[i];
            }

            opt_avg /= LATENCY_NUM;


            double col_avg = 0;

            for(i = 0; i < LATENCY_NUM; ++i)
            {
                col_avg += col[i];
            }

            col_avg /= LATENCY_NUM;


            double col_sd = 0;

            for(i = 0; i < LATENCY_NUM; ++i)
            {
                col_sd += (col[i] - col_avg) * (col[i] - col_avg);
            }

            col_sd = sqrtf(col_sd / LATENCY_NUM);


            double col_01 = 0;

            for(i = 0; i < LATENCY_NUM; ++i)
            {
                if(col[i] >= col_avg - 0.1 && col[i] <= col_avg + 0.1)
                {
                    ++col_01;
                }
            }

            col_01 = col_01 / LATENCY_NUM * 100;


            double col_04 = 0;

            for(i = 0; i < LATENCY_NUM; ++i)
            {
                if(col[i] >= col_avg - 0.4 && col[i] <= col_avg + 0.4)
                {
                    ++col_04;
                }
            }

            col_04 = col_04 / LATENCY_NUM * 100;


            double col_07 = 0;

            for(i = 0; i < LATENCY_NUM; ++i)
            {
                if(col[i] >= col_avg - 0.7 && col[i] <= col_avg + 0.7)
                {
                    ++col_07;
                }
            }

            col_07 = col_07 / LATENCY_NUM * 100;


            double col_10 = 0;

            for(i = 0; i < LATENCY_NUM; ++i)
            {
                if(col[i] >= col_avg - 1.0 && col[i] <= col_avg + 1.0)
                {
                    ++col_10;
                }
            }

            col_10 = col_10 / LATENCY_NUM * 100;


            double qry_avg = 0;

            for(i = 0; i < LATENCY_NUM; ++i)
            {
                qry_avg += qry[i];
            }

            qry_avg /= LATENCY_NUM;


            double upd_avg = 0;

            for(i = 0; i < LATENCY_NUM; ++i)
            {
                upd_avg += upd[i];
            }

            upd_avg /= LATENCY_NUM;


            double upd_sd = 0;

            for(i = 0; i < LATENCY_NUM; ++i)
            {
                upd_sd += (upd[i] - upd_avg) * (upd[i] - upd_avg);
            }

            upd_sd = sqrtf(upd_sd / LATENCY_NUM);


            double upd_01 = 0;

            for(i = 0; i < LATENCY_NUM; ++i)
            {
                if(upd[i] >= upd_avg - 0.1 && upd[i] <= upd_avg + 0.1)
                {
                    ++upd_01;
                }
            }

            upd_01 = upd_01 / LATENCY_NUM * 100;


            double upd_04 = 0;

            for(i = 0; i < LATENCY_NUM; ++i)
            {
                if(upd[i] >= upd_avg - 0.4 && upd[i] <= upd_avg + 0.4)
                {
                    ++upd_04;
                }
            }

            upd_04 = upd_04 / LATENCY_NUM * 100;


            double upd_07 = 0;

            for(i = 0; i < LATENCY_NUM; ++i)
            {
                if(upd[i] >= upd_avg - 0.7 && upd[i] <= upd_avg + 0.7)
                {
                    ++upd_07;
                }
            }

            upd_07 = upd_07 / LATENCY_NUM * 100;


            double upd_10 = 0;

            for(i = 0; i < LATENCY_NUM; ++i)
            {
                if(upd[i] >= upd_avg - 1.0 && upd[i] <= upd_avg + 1.0)
                {
                    ++upd_10;
                }
            }

            upd_10 = upd_10 / LATENCY_NUM * 100;


            printf(
"------------------------------------------------------------------------------\n"
"opt | %5.2lfms | ---- | -------- | -------- | -------- | -------- | attempted |\n"
"col | %5.2lfms | %4.2lf |  %5.1lf%%  |  %5.1lf%%  |  %5.1lf%%  |  %5.1lf%%  | %9." PRIu64 " |\n"
"qry | %5.2lfms | ---- | -------- | -------- | -------- | -------- |           |\n"
"upd | %5.2lfms | %4.2lf |  %5.1lf%%  |  %5.1lf%%  |  %5.1lf%%  |  %5.1lf%%  | succeeded |\n"
"all | %5.2lfms | ---- | -------- | -------- | -------- | -------- | %9." PRIu64 " |\n",
                opt_avg,
                col_avg,
                col_sd,
                col_01,
                col_04,
                col_07,
                col_10,
                maybe_collisions,
                qry_avg,
                upd_avg,
                upd_sd,
                upd_01,
                upd_04,
                upd_07,
                upd_10,
                upd_avg + opt_avg + col_avg + qry_avg,
                collisions
            );

            maybe_collisions = 0;
            collisions = 0;
            i = LATENCY_NUM - 1;
        }

        i = (i + 1) % LATENCY_NUM;

        if(intr)
        {
            break;
        }
    }

    hshg_free(hshg);

    free(init_data);

    return EXIT_SUCCESS;
}
