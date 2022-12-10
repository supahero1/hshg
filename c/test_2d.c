#define HSHG_D 2
#include "test.c"

#define sqrt_3 1.732

void
test(const hshg_cell_t side, const uint32_t size)
{
    hshg = hshg_create(side, size);

    assert(hshg);

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
