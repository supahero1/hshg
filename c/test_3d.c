#define HSHG_D 3
#include "test.c"

void
test(const hshg_cell_t side, const uint32_t size)
{
    hshg = hshg_create(side, size);

    assert(hshg);

    hshg->collide = coll;


    err = hshg_set_size(hshg, 1);

    assert(!err);


    insert(0, 0, 0, 1);

    assert_col();

    check_count(((int[]){ 0 }));


    insert(0, 5, 0, 3);

    assert_col();

    check_count(((int[]){ 0, 0 }));


    insert(2, 1, 2, 2);

    cols += 2;

    assert_col();

    check_count(((int[]){ 1, 1, 2 }));


    insert(-38, 43, -33, 61);

    cols += 1;

    assert_col();

    check_count(((int[]){ 1, 2, 2, 1 }));


    insert(-2, 2, 3, 1.5);

    assert_col();

    check_count(((int[]){ 1, 2, 2, 1, 0 }));


    assert(!hshg_optimize(hshg));

    assert_col();

    check_count(((int[]){ 1, 2, 2, 1, 0 }));


    insert(-0.5, 1, 1, 0.5);

    cols += 1;

    assert_col();

    check_count(((int[]){ 2, 2, 2, 1, 0, 1 }));


    insert(0, 2, 2, 1);

    cols += 4;

    assert_col();

    check_count(((int[]){ 2, 3, 3, 1, 1, 2, 4 }));


    assert(!hshg_optimize(hshg));

    assert_col();

    check_count(((int[]){ 2, 3, 3, 1, 1, 2, 4 }));


    insert(-2, 1.5, -1, 2);

    cols += 2;

    assert_col();

    check_count(((int[]){ 3, 4, 3, 1, 1, 2, 4, 2 }));


    hshg->update = upd;

    reset();

    hshg_update(hshg);

    check_count(((int[]){ 1, 1, 1, 1, 1, 1, 1, 1 }));


    insert(0, 1.5, 0, 2);

    cols += 6;

    assert_col();

    check_count(((int[]){ 4, 5, 4, 1, 1, 3, 5, 3, 6 }));


    assert(!hshg_optimize(hshg));

    assert_col();

    check_count(((int[]){ 4, 5, 4, 1, 1, 3, 5, 3, 6 }));


    /* make sure to change cells a couple times */
    for(int i = 0; i < 100; ++i)
    {
        hshg_update_t u = hshg->update;

        hshg->update = change_pos;

        hshg_update(hshg);

        hshg->update = u;


        reset();

        hshg_update(hshg);

        check_count(((int[]){ 1, 1, 1, 1, 1, 1, 1, 1, 1 }));

        assert_col();

        check_count(((int[]){ 4, 5, 4, 1, 1, 3, 5, 3, 6 }));


        assert(!hshg_optimize(hshg));

        reset();

        hshg_update(hshg);

        check_count(((int[]){ 1, 1, 1, 1, 1, 1, 1, 1, 1 }));

        assert_col();

        check_count(((int[]){ 4, 5, 4, 1, 1, 3, 5, 3, 6 }));
    }


    {
        hshg_update_t u = hshg->update;

        hshg->update = revert_pos;

        hshg_update(hshg);

        hshg->update = u;
    }


    set(((struct dis){ 0, 1.5, 0, 2 }), ((struct dis){ 0, 1.5, 0, 0.5 }));

    cols -= 3;

    assert_col();

    check_count(((int[]){ 4, 5, 3, 1, 1, 2, 4, 3, 3 }));


    set(((struct dis){ 0, 1.5, 0, 0.5 }), ((struct dis){ 0.5, 1.5, 0, 0.5 }));

    cols -= 3;

    assert_col();

    check_count(((int[]){ 3, 4, 3, 1, 1, 2, 4, 2, 0 }));


    set(((struct dis){ 0.5, 1.5, 0, 0.5 }), ((struct dis){ .del = 1 }));

    assert_col();

    check_count(((int[]){ 3, 4, 3, 1, 1, 2, 4, 2 }));


    set(((struct dis){ -0.5, 1, 1, 0.5 }), ((struct dis){ -0.5, 1, 1, 10 }));

    cols += 5;

    assert_col();

    check_count(((int[]){ 3, 5, 4, 2, 2, 7, 4, 3 }));


    set(((struct dis){ -0.5, 1, 1, 10 }), ((struct dis){ -10, 10, -10, 10 }));

    cols -= 6;

    assert_col();

    check_count(((int[]){ 2, 4, 3, 2, 1, 1, 3, 2 }));


    set(((struct dis){ 0, 0, 0, 1 }), ((struct dis){ 0, 0, 0, 0.99 }));

    cols -= 1;

    assert_col();

    check_count(((int[]){ 1, 4, 2, 2, 1, 1, 3, 2 }));

    reset();

    hshg_update(hshg);

    check_count(((int[]){ 1, 1, 1, 1, 1, 1, 1, 1 }));


    assert(!hshg_optimize(hshg));

    assert_col();

    check_count(((int[]){ 1, 4, 2, 2, 1, 1, 3, 2 }));

    reset();

    hshg_update(hshg);

    check_count(((int[]){ 1, 1, 1, 1, 1, 1, 1, 1 }));


    query(-10, 10, -10, -10, 10, -10, 2);

    query(0, 0, -4, 0, 0, -4, 2);


    set(((struct dis){ -10, 10, -10, 10 }), ((struct dis){ .del = 1 }));

    set(((struct dis){ -38, 43, -33, 61 }), ((struct dis){ .del = 1 }));

    cols -= 2;

    consolidate();

    assert_col();

    check_count(((int[]){ 1, 3, 2, 1, 3, 2 }));

    reset();

    hshg_update(hshg);

    check_count(((int[]){ 1, 1, 1, 1, 1, 1 }));


    assert(!hshg_optimize(hshg));

    assert_col();

    check_count(((int[]){ 1, 3, 2, 1, 3, 2 }));

    reset();

    hshg_update(hshg);

    check_count(((int[]){ 1, 1, 1, 1, 1, 1 }));


    query(-2, 0, -1, 2, 3, 3, 6);

    query(0, 1.5, -100, 0, 1.5, 0.99, 2); /* 2 barely touching */

    query(0, 1.5, -100, 0, 1.5, 1, 3); /* 3 barely touching */

    query(-0.6, 2.9, 2.5, 0, 2.9, 2.5, 4); /* none are touching */


    set(((struct dis){ 0, 5, 0, 3 }), ((struct dis){ .del = 1 }));
    set(((struct dis){ 2, 1, 2, 2 }), ((struct dis){ .del = 1 }));
    set(((struct dis){ -2, 2, 3, 1.5 }), ((struct dis){ .del = 1 }));

    consolidate();

    cols = 1;

    assert_col();

    check_count(((int[]){ 1, 0, 1 }));

    reset();

    hshg_update(hshg);

    check_count(((int[]){ 1, 1, 1 }));


    assert(!hshg_optimize(hshg));

    assert_col();

    check_count(((int[]){ 1, 0, 1 }));

    reset();

    hshg_update(hshg);

    check_count(((int[]){ 1, 1, 1 }));


    set(((struct dis){ 0, 0, 0, 0.99 }), ((struct dis){ .del = 1 }));


    query(0, 1.5, 1, 0, 1.5, 1, 2);

    query(0, 0, 1, 0, 0, 1, 1);

    query(0, 0, 0, 0, 0, 0, 1);


    hshg_free(hshg);
}
