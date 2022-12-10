#define HSHG_D 1
#include "test.c"

#define sqrt_3 1.732
#define sqrt_5 2.236
#define sqrt_7 2.646

void
test(const hshg_cell_t side, const uint32_t size)
{
    hshg = hshg_create(side, size);

    assert(hshg);

    hshg->collide = coll;


    err = hshg_set_size(hshg, 1);

    assert(!err);


    insert(15, 1);

    assert_col();

    check_count(((int[]){ 0 }));


    insert(18, sqrt_3);

    assert_col();

    check_count(((int[]){ 0, 0 }));


    insert(1000, 981);

    cols += 1;

    assert_col();

    check_count(((int[]){ 0, 1, 1 }));


    insert(17, sqrt_5);

    cols += 3;

    assert_col();

    check_count(((int[]){ 1, 2, 2, 3 }));


    insert(22, sqrt_7);

    cols += 2;

    assert_col();

    check_count(((int[]){ 1, 3, 3, 3, 2 }));


    insert(0, 1);

    assert_col();

    check_count(((int[]){ 1, 3, 3, 3, 2, 0 }));


    insert(15, 0.125);

    cols += 2;

    assert_col();

    check_count(((int[]){ 2, 3, 3, 4, 2, 0, 2 }));


    assert(!hshg_optimize(hshg));

    assert_col();

    check_count(((int[]){ 2, 3, 3, 4, 2, 0, 2 }));


    insert(-10, 10);

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


    set(((struct dis){ 17, sqrt_5 }), ((struct dis){ 17, 0.5 }));

    cols -= 3;

    assert_col();

    check_count(((int[]){ 1, 3, 2, 1, 2, 1, 1, 1 }));


    set(((struct dis){ 15, 0.125 }), ((struct dis){ 17, 0.125 }));

    cols += 1;

    assert_col();

    check_count(((int[]){ 0, 4, 2, 2, 2, 1, 2, 1 }));


    set(((struct dis){ 17, 0.125 }), ((struct dis){ 17, 1 }));

    cols += 1;

    assert_col();

    check_count(((int[]){ 1, 4, 2, 2, 2, 1, 3, 1 }));


    set(((struct dis){ 17, 1 }), ((struct dis){ .del = 1 }));

    insert(8, 8);

    assert_col();

    check_count(((int[]){ 1, 3, 2, 1, 2, 2, 3, 2 }));

    reset();

    hshg_update(hshg);

    check_count(((int[]){ 1, 1, 1, 1, 1, 1, 1, 1 }));


    assert(!hshg_optimize(hshg));

    assert_col();

    check_count(((int[]){ 1, 3, 2, 1, 2, 2, 3, 2 }));

    reset();

    hshg_update(hshg);

    check_count(((int[]){ 1, 1, 1, 1, 1, 1, 1, 1 }));


    query(0, 0, 3);

    query(1.01, 13.99, 1);

    query(16.01, 16.01, 0);

    query(19.6, 19.6, 3);

    query(-100, -2, 1);

    query(2, 19, 5);


    set(((struct dis){ 15, 1 }), ((struct dis){ .del = 1 }));
    set(((struct dis){ 18, sqrt_3 }), ((struct dis){ .del = 1 }));
    set(((struct dis){ 1000, 981 }), ((struct dis){ .del = 1 }));
    set(((struct dis){ 17, 0.5 }), ((struct dis){ .del = 1 }));
    set(((struct dis){ 22, sqrt_7 }), ((struct dis){ .del = 1 }));
    set(((struct dis){ 0, 1 }), ((struct dis){ .del = 1 }));

    cols = 1;

    consolidate();

    assert_col();

    check_count(((int[]){ 1, 1 }));

    reset();

    hshg_update(hshg);

    check_count(((int[]){ 1, 1 }));


    assert(!hshg_optimize(hshg));

    assert_col();

    check_count(((int[]){ 1, 1 }));

    reset();

    hshg_update(hshg);

    check_count(((int[]){ 1, 1 }));


    query(-100, -20.1, 0);

    query(16.01, 100, 0);

    query(0, 1, 2);

    query(-1, 0, 2);

    query(-2, -1, 1);

    query(1, 2, 1);


    hshg_free(hshg);
}
