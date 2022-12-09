#define HSHG_D 2
#define HSHG_UNIFORM
#include "hshg.c"

#include <stdio.h>
#include <assert.h>

struct hshg* hshg;

int err;

int obj_count = 0;
int _obj_count = 0;

struct obj
{
    int count;
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

    return ret;
}


void
ret_obj(int idx)
{
    objs[idx].count = free_obj;
    free_obj = idx;
    --obj_count;
}


void
insert(hshg_pos_t x, hshg_pos_t y, hshg_pos_t r)
{
    assert(!hshg_insert(hshg, x, y, r, get_obj()));
}


int col_num;

#define unused __attribute__((unused))


void
coll(unused const struct hshg* _,
    const struct hshg_entity* a, const struct hshg_entity* b)
{
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    float sr = a->r + b->r;

    if(dx * dx + dy * dy <= sr * sr)
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
    --ent->y;
}


void
revert_pos(unused struct hshg* _, struct hshg_entity* ent)
{
    ent->x += 100;
    ent->y += 100;
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
    hshg_pos_t y;
    hshg_pos_t r;
    int del;
};

struct dis old_data;
struct dis new_data;


void
upd_set(unused struct hshg* _, struct hshg_entity* ent)
{
    if(ent->x == old_data.x && ent->y == old_data.y && ent->r == old_data.r)
    {
        did_it = 1;

        if(new_data.del)
        {
            ret_obj(ent->ref);

            return hshg_remove(hshg);
        }

        ent->x = new_data.x;
        ent->y = new_data.y;
        ent->r = new_data.r;

        if(old_data.x != new_data.x || old_data.y != new_data.y)
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


#define query(min_x, min_y, max_x, max_y, expected) \
do                                                  \
{                                                   \
    hshg_query_t old = hshg->query;                 \
                                                    \
    hshg->query = qury;                             \
    queries = 0;                                    \
                                                    \
    hshg_query(hshg, min_x, min_y, max_x, max_y);   \
                                                    \
    assert_eq(queries, expected);                   \
                                                    \
    hshg->query = old;                              \
}                                                   \
while(0)


#define sqrt_3 1.732


void
test(const hshg_cell_t side, const uint32_t size)
{
    obj_count = 0;
    _obj_count = 0;
    free_obj = NUM_OBJ;
    cols = 0;


    hshg = hshg_create(8, 4);

    assert(hshg);

    assert_eq(hshg->grids_len, 3);

    hshg->collide = coll;


    err = hshg_set_size(hshg, 1);

    assert(!err);


    insert(15, 15, 1);

    assert_col();

    check_count(((int[]){ 0 }));


    insert(17, 17, sqrt_3);

    assert_col();

    check_count(((int[]){ 0, 0 }));


    insert(1000, -1000, 1413);

    cols += 1;

    assert_col();

    check_count(((int[]){ 0, 1, 1 }));


    insert(13, 17, 5);

    cols += 3;

    assert_col();

    check_count(((int[]){ 1, 2, 2, 3 }));


    insert(24, 12, 7);

    cols += 2;

    assert_col();

    check_count(((int[]){ 1, 3, 3, 3, 2 }));


    insert(0, 0, 1);

    assert_col();

    check_count(((int[]){ 1, 3, 3, 3, 2, 0 }));


    insert(15, 15, 0.125);

    cols += 2;

    assert_col();

    check_count(((int[]){ 2, 3, 3, 4, 2, 0, 2 }));


    assert(!hshg_optimize(hshg));

    assert_col();

    check_count(((int[]){ 2, 3, 3, 4, 2, 0, 2 }));


    insert(-20, 20, 28);

    cols += 1;

    assert_col();

    check_count(((int[]){ 2, 3, 3, 4, 2, 1, 2, 1 }));


    hshg->update = upd;

    reset();

    hshg_update(hshg);

    check_count(((int[]){ 1, 1, 1, 1, 1, 1, 1, 1 }));


    /* make sure to change cells a couple times */
    for(int i = 0; i < 100; ++i)
    {
        hshg_update_t u = hshg->update;

        hshg->update = change_pos;

        hshg_update(hshg);

        hshg->update = u;


        reset();

        hshg_update(hshg);

        check_count(((int[]){ 1, 1, 1, 1, 1, 1, 1, 1 }));

        assert_col();

        check_count(((int[]){ 2, 3, 3, 4, 2, 1, 2, 1 }));


        assert(!hshg_optimize(hshg));

        reset();

        hshg_update(hshg);

        check_count(((int[]){ 1, 1, 1, 1, 1, 1, 1, 1 }));

        assert_col();

        check_count(((int[]){ 2, 3, 3, 4, 2, 1, 2, 1 }));
    }


    {
        hshg_update_t u = hshg->update;

        hshg->update = revert_pos;

        hshg_update(hshg);

        hshg->update = u;
    }


    set(((struct dis){ 13, 17, 5 }), ((struct dis){ 13, 17, 2 }));

    cols -= 3;

    assert_col();

    check_count(((int[]){ 2, 2, 2, 1, 2, 1, 1, 1 }));


    set(((struct dis){ 15, 15, 0.125 }), ((struct dis){ 18, 16, 0.125 }));

    cols += 1;

    assert_col();

    check_count(((int[]){ 1, 3, 3, 1, 2, 1, 2, 1 }));


    set(((struct dis){ 18, 16, 0.125 }), ((struct dis){ 18, 16, 0.375 }));

    cols += 1;

    assert_col();

    check_count(((int[]){ 1, 3, 3, 1, 3, 1, 3, 1 }));


    set(((struct dis){ 18, 16, 0.375 }), ((struct dis){ .del = 1 }));

    insert(0, 16, 3);

    cols -= 2;

    assert_col();

    check_count(((int[]){ 1, 2, 2, 1, 2, 1, 1, 2 }));

    reset();

    hshg_update(hshg);

    check_count(((int[]){ 1, 1, 1, 1, 1, 1, 1, 1 }));


    assert(!hshg_optimize(hshg));

    assert_col();

    check_count(((int[]){ 1, 2, 2, 1, 2, 1, 1, 2 }));

    reset();

    hshg_update(hshg);

    check_count(((int[]){ 1, 1, 1, 1, 1, 1, 1, 1 }));


    query(10, 16, 16, 20, 4); /* 3 are in, but 4th's hitbox matches too */

    query(24, 12, 24, 12, 2);

    query(8, 12, 8, 12, 2); /* 0 are in */

    query(15.1, 17, 15.1, 17, 1); /* 0 are in */

    query(3, 16, 3, 16, 3); /* 2 are in */


    set(((struct dis){ 1000, -1000, 1413 }), ((struct dis){ .del = 1 }));

    query(10, 16, 16, 20, 3);

    query(24, 12, 24, 12, 1);

    query(8, 12, 8, 12, 1); /* 0 are in */

    query(15.1, 17, 15.1, 17, 0);

    query(3, 16, 3, 16, 2);


    set(((struct dis){ 17, 17, sqrt_3 }), ((struct dis){ .del = 1 }));
    set(((struct dis){ 24, 12, 7 }), ((struct dis){ .del = 1 }));
    set(((struct dis){ 0, 0, 1 }), ((struct dis){ .del = 1 }));
    set(((struct dis){ -20, 20, 28 }), ((struct dis){ .del = 1 }));
    set(((struct dis){ 0, 16, 3 }), ((struct dis){ .del = 1 }));

    cols = 1;

    assert_col();

    check_count(((int[]){ 1, 0 }));

    assert_eq(objs[3].count, 1);

    reset();

    hshg_update(hshg);

    check_count(((int[]){ 1, 0 }));

    assert_eq(objs[3].count, 1);


    assert(!hshg_optimize(hshg));

    assert_col();

    check_count(((int[]){ 1, 0 }));

    assert_eq(objs[3].count, 1);

    reset();

    hshg_update(hshg);

    check_count(((int[]){ 1, 0 }));

    assert_eq(objs[3].count, 1);


    insert(15, 17, 1);

    set(((struct dis){ 13, 17, 2 }), ((struct dis){ .del = 1 }));

    assert_col();


    query(16.01, 0, 32, 32, 0);

    query(0, 0, 13.99, 32, 0);


    hshg_free(hshg);
}


int
main()
{
    for(int q = 0; q < 4; ++q)
    {
        for(int w = 0; w < 6; ++w)
        {
            printf("iter %dx%d\n", q, w);
            fflush(NULL);

            test(1 << q, 2 << w);
        }
    }

    return 0;
}
