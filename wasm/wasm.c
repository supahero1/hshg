#include <stddef.h>
#include <stdint.h>


extern uint8_t __heap_base;

static uint8_t* malloc_base = &__heap_base;


static void*
malloc(size_t len)
{
    void* ret = malloc_base;

    len = (len + 31) & ~31;

    malloc_base += len;

    return ret;
}


static void
free(void* ptr)
{
    __builtin_unreachable();
}


static void*
calloc(size_t num, size_t size)
{
    return malloc(num * size);
}


static void*
realloc(void* ptr, size_t new_size)
{
    if(ptr == NULL)
    {
        return malloc(new_size);
    }

    __builtin_unreachable();
}


static float
fabsf(float num)
{
    return __builtin_fabsf(num);
}


static void*
memcpy(void* dest, const void* src, size_t n)
{
    uint8_t* _dest = dest;
    const uint8_t* _src = src;

    for(size_t i = 0; i < n; ++i)
    {
        _dest[i] = _src[i];
    }

    return dest;
}


#define HSHG_NDEBUG
#define HSHG_UNIFORM

#define hshg_entity_t uint32_t
#define hshg_cell_t uint32_t
#define hshg_cell_sq_t uint32_t
#define hshg_pos_t float

#include "hshg.c"


#define export __attribute__((visibility("default")))
#define unused __attribute__((unused))


static struct hshg* hshg;


int on_update(hshg_entity_t);

void on_collision(hshg_entity_t, hshg_entity_t);

void on_query(hshg_entity_t);


static void
_update(unused struct hshg* _, struct hshg_entity* a)
{
    int flags = on_update(a->ref);

    if(flags & 4)
    {
        return hshg_remove(hshg);
    }

    if(flags & 1)
    {
        hshg_move(hshg);
    }

    if(flags & 2)
    {
        hshg_resize(hshg);
    }
}


static void
_collide(unused const struct hshg* _, const struct hshg_entity* a,
    const struct hshg_entity* b)
{
    on_collision(a->ref, b->ref);
}


static void
_query(unused const struct hshg* _, const struct hshg_entity* a)
{
    on_query(a->ref);
}


export size_t
mem(_hshg_cell_t side, _hshg_entity_t max_entities)
{
    return hshg_memory_usage(side, max_entities);
}


export uintptr_t
init(hshg_cell_t side, uint32_t size, hshg_entity_t max_entities)
{
    hshg = hshg_create(side, size);

    hshg_set_size(hshg, max_entities + 1);

    hshg->update = _update;
    hshg->collide = _collide;
    hshg->query = _query;

    return (uintptr_t) hshg->entities;
}


export void
insert(hshg_pos_t x _2D(, hshg_pos_t y)
    _3D(, hshg_pos_t z), hshg_pos_t r, hshg_entity_t ref)
{
    hshg_insert(hshg, x _2D(, y) _3D(, z), r, ref);
}


export void
update(void)
{
    hshg_update(hshg);
}


export void
collide(void)
{
    hshg_collide(hshg);
}


export void
query(hshg_pos_t x1 _2D(, hshg_pos_t y1) _3D(, hshg_pos_t z1),
    hshg_pos_t x2 _2D(, hshg_pos_t y2) _3D(, hshg_pos_t z2))
{
    hshg_query(hshg, x1 _2D(, y1) _3D(, z1), x2 _2D(, y2) _3D(, z2));
}
