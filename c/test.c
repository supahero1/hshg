#define HSHG_UNIFORM
#include "hshg.c"

#include <stdio.h>
#include <assert.h>

#define unused __attribute__((unused))

struct hshg* hshg;

int err;

int obj_count = 0;
int _obj_count = 0;

struct obj
{
    int count;
    int dead;
};

#define NUM_OBJ 16

struct obj objs[NUM_OBJ];
int free_obj = NUM_OBJ;


int
get_obj(void)
{
    ++obj_count;

    if(free_obj == NUM_OBJ)
    {
        return _obj_count++;
    }

    int ret = free_obj;

    free_obj = objs[ret].count;
    objs[ret].count = 0;
    objs[ret].dead = 0;

    return ret;
}


void
ret_obj(int idx)
{
    objs[idx].count = free_obj;
    objs[idx].dead = 1;
    free_obj = idx;
    --obj_count;
}


void
insert(hshg_pos_t x _2D(, hshg_pos_t y) _3D(, hshg_pos_t z), hshg_pos_t r)
{
    assert(!hshg_insert(hshg, x _2D(, y) _3D(, z), r, get_obj()));
}


int col_num;


void
coll(unused const struct hshg* _,
    const struct hshg_entity* a, const struct hshg_entity* b)
{
    float dx = a->x - b->x;
_2D(float dy = a->y - b->y;)
_3D(float dz = a->z - b->z;)
    float sr = a->r + b->r;

    if(dx * dx _2D(+ dy * dy) _3D(+ dz * dz) <= sr * sr)
    {
        ++objs[a->ref].count;
        ++objs[b->ref].count;

        ++col_num;
    }
}


void
reset(void)
{
    for(int i = 0; i < NUM_OBJ; ++i)
    {
        objs[i].count = 0;
    }

    col_num = 0;
}


int opt_idx = 0;
int refs[NUM_OBJ];


void
obj_optimize(unused struct hshg* _, struct hshg_entity* ent)
{
    if(ent->ref == refs[opt_idx])
    {
        ent->ref = opt_idx++;
    }
}


void
consolidate(void)
{
    free_obj = NUM_OBJ;
    opt_idx = 0;

    for(int i = 0; i < _obj_count; ++i)
    {
        if(objs[i].dead)
        {
            continue;
        }

        refs[opt_idx] = i;

        if(opt_idx != i)
        {
            objs[opt_idx] = objs[i];
        }

        ++opt_idx;
    }

    refs[opt_idx] = 255;
    opt_idx = 0;
    _obj_count = obj_count;

    hshg_update_t old = hshg->update;

    hshg->update = obj_optimize;

    for(int i = 0; i < obj_count; ++i)
    {
        hshg_update(hshg);
    }

    hshg->update = old;
}


void
col(void)
{
    reset();

    hshg_collide(hshg);
}


int cols = 0;


#define assert_col()                        \
do                                          \
{                                           \
    col();                                  \
                                            \
    if(col_num != cols)                     \
    {                                       \
        printf("expected %d, but got %d\n", \
            cols, col_num);                 \
        assert(0);                          \
    }                                       \
}                                           \
while(0)


#define assert_eq(n1, n2)                   \
do                                          \
{                                           \
    if(n1 != n2)                            \
    {                                       \
        printf("expected %d, but got %d\n", \
            n2, n1);                        \
        assert(0);                          \
    }                                       \
}                                           \
while(0)


#define assert_neq(n1, n2)                  \
do                                          \
{                                           \
    if(n1 == n2)                            \
    {                                       \
        printf("expected %d, but got %d\n", \
            n2, n1);                        \
        assert(0);                          \
    }                                       \
}                                           \
while(0)


void
upd(unused struct hshg* _, struct hshg_entity* ent)
{
    ++objs[ent->ref].count;
}


void
change_pos(unused struct hshg* _, struct hshg_entity* ent)
{
    --ent->x;
_2D(++ent->y;)
_3D(--ent->z;)
}


void
revert_pos(unused struct hshg* _, struct hshg_entity* ent)
{
    ent->x += 100;
_2D(ent->y -= 100;)
_3D(ent->z += 100;)
}


#define _check_count(checks, _len)           \
do                                           \
{                                            \
    int len = _len;                          \
                                             \
    assert_eq(obj_count, len);               \
                                             \
    for(int i = 0; i < len; ++i)             \
    {                                        \
        assert_eq(objs[i].count, checks[i]); \
    }                                        \
}                                            \
while(0)


#define check_count(checks) \
_check_count(checks, sizeof(checks) / sizeof(checks[0]))


int did_it;

struct dis
{
    hshg_pos_t x;
_2D(hshg_pos_t y;)
_3D(hshg_pos_t z;)
    hshg_pos_t r;
    int del;
};

struct dis old_data;
struct dis new_data;


void
upd_set(unused struct hshg* _, struct hshg_entity* ent)
{
    if(ent->x == old_data.x _2D( && ent->y == old_data.y)
        _3D( && ent->z == old_data.z) && ent->r == old_data.r)
    {
        did_it = 1;

        if(new_data.del)
        {
            ret_obj(ent->ref);

            return hshg_remove(hshg);
        }

        ent->x = new_data.x;
    _2D(ent->y = new_data.y;)
    _3D(ent->z = new_data.z;)
        ent->r = new_data.r;

        if(old_data.x != new_data.x _2D( || old_data.y != new_data.y)
            _3D( || old_data.z != new_data.z))
        {
            hshg_move(hshg);
        }

        if(old_data.r != new_data.r)
        {
            hshg_resize(hshg);
        }
    }
}


#define set(_old_data, _new_data)     \
do                                    \
{                                     \
    hshg_update_t old = hshg->update; \
                                      \
    hshg->update = upd_set;           \
    old_data = _old_data;             \
    new_data = _new_data;             \
    did_it = 0;                       \
                                      \
    hshg_update(hshg);                \
                                      \
    assert(did_it);                   \
                                      \
    hshg->update = old;               \
}                                     \
while(0)


int queries;


void
qury(unused const struct hshg* _, const struct hshg_entity* ent)
{
    ++queries;
}


EXCL_1D(

#define query_1d(min_x, max_x, expected)    \
do                                          \
{                                           \
    hshg_query_t old = hshg->query;         \
                                            \
    hshg->query = qury;                     \
    queries = 0;                            \
                                            \
    hshg_query(hshg, min_x, max_x);         \
                                            \
    assert_eq(queries, expected);           \
                                            \
    hshg->query = old;                      \
}                                           \
while(0)

)


EXCL_2D(

#define query_2d(min_x, min_y, max_x, max_y, expected)  \
do                                                      \
{                                                       \
    hshg_query_t old = hshg->query;                     \
                                                        \
    hshg->query = qury;                                 \
    queries = 0;                                        \
                                                        \
    hshg_query(hshg, min_x, min_y, max_x, max_y);       \
                                                        \
    assert_eq(queries, expected);                       \
                                                        \
    hshg->query = old;                                  \
}                                                       \
while(0)

)


EXCL_3D(

#define query_3d(min_x, min_y, min_z, max_x, max_y, max_z, expected)    \
do                                                                      \
{                                                                       \
    hshg_query_t old = hshg->query;                                     \
                                                                        \
    hshg->query = qury;                                                 \
    queries = 0;                                                        \
                                                                        \
    hshg_query(hshg, min_x, min_y, min_z, max_x, max_y, max_z);         \
                                                                        \
    assert_eq(queries, expected);                                       \
                                                                        \
    hshg->query = old;                                                  \
}                                                                       \
while(0)

)


#define query(...)              \
EXCL_1D(query_1d(__VA_ARGS__))  \
EXCL_2D(query_2d(__VA_ARGS__))  \
EXCL_3D(query_3d(__VA_ARGS__))


void
test(const hshg_cell_t, const uint32_t);


void
_test(const hshg_cell_t side, const uint32_t size)
{
    obj_count = 0;
    _obj_count = 0;
    free_obj = NUM_OBJ;
    cols = 0;

    for(int i = 0; i < NUM_OBJ; ++i)
    {
        objs[i].count = 0;
        objs[i].dead = 0;
    }

    test(side, size);
}


int
main() {
    for(int q = 0; q < 8; ++q)
    {
        for(int w = 0; w < 8; ++w)
        {
            printf("\riter %dx%d ", q, w);
            fflush(NULL);

            _test(1 << q, 1 << w);
        }
    }

    puts("\rpass");

    return 0;
}
