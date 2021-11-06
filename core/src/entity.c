#include <stdlib.h>
#include <string.h>

#include "entity.h"

API const entity null_entity = (UINT64_MAX);
API const entity_id null_entity_id = (UINT32_MAX);

entity_version get_entity_version(entity e) {
	return e >> 32;
}

entity_id get_entity_id(entity e) {
	return (entity_id)e;
}

entity make_handle(entity_id id, entity_version v) {
	return ((u64)v << 32) | id;
}

struct pool {
	i32* sparse;
	u32 sparse_capacity;

	entity* dense;
	u32 dense_count;
	u32 dense_capacity;

	u32 element_size;
	void* data;
	u32 count;
	u32 capacity;

	u32 type_id;

	struct world* world;

	component_create_func on_create;
	component_destroy_func on_destroy;
};

static void init_pool(struct pool* pool, struct world* world, struct type_info t) {
	*pool = (struct pool) { 0 };

	pool->type_id = t.id;
	pool->element_size = t.size;

	pool->world = world;
}

static void* pool_get(struct pool* pool, entity e);

static void deinit_pool(struct pool* pool) {	
	if (pool->on_destroy) {
		for (u32 i = 0; i < pool->dense_count; i++) {
			const entity e = pool->dense[i];

			void* ptr = pool_get(pool, e);
			pool->on_destroy(pool->world, e, ptr);
		}
	}

	if (pool->sparse) {
		free(pool->sparse);
	}
	if (pool->dense) {
		free(pool->dense);
	}
	if (pool->data) {
		free(pool->data);
	}
}

static bool pool_has(struct pool* pool, entity e) {
	const entity_id id = get_entity_id(e);
	return (id < pool->sparse_capacity) && (pool->sparse[id] != -1);
}

static i32 pool_sparse_idx(struct pool* pool, entity e) {
	return pool->sparse[get_entity_id(e)];
}

static void* pool_add(struct pool* pool, entity e, void* init) {
	if (pool->count >= pool->capacity) {
		pool->capacity = pool->capacity < 8 ? 8 : pool->capacity * 2;
		pool->data = realloc(pool->data, pool->capacity * pool->element_size);
	}

	void* ptr = &((u8*)pool->data)[(pool->count++) * pool->element_size];

	const entity eid = get_entity_id(e);
	if (eid >= pool->sparse_capacity) {
		const u32 new_cap = pool->sparse_capacity < 8 ? 8 : pool->capacity * 2;
		pool->sparse = realloc(pool->sparse, new_cap * sizeof(i32));
		for (u32 i = pool->sparse_capacity; i < new_cap; i++) {
			pool->sparse[i] = -1;
		}
		pool->sparse_capacity = new_cap;
	}

	pool->sparse[eid] = (i32)pool->dense_count;

	if (pool->dense_count >= pool->dense_capacity) {
		pool->dense_capacity = pool->dense_capacity < 8 ? 8 : pool->dense_capacity * 2;
		pool->dense = realloc(pool->dense, pool->dense_capacity * sizeof(entity));
	}

	pool->dense[pool->dense_count++] = e;

	memcpy(ptr, init, pool->element_size);

	if (pool->on_create) {
		pool->on_create(pool->world, e, ptr);
	}

	return ptr;
}

static void pool_remove(struct pool* pool, entity e) {
	const i32 pos = pool->sparse[get_entity_id(e)];

	if (pool->on_destroy) {
		void* ptr = &((char*)pool->data)[pos * pool->element_size];
		pool->on_destroy(pool->world, e, ptr);
	}

	const entity other = pool->dense[pool->dense_count - 1];

	pool->sparse[get_entity_id(other)] = pos;
	pool->dense[pos] = other;
	pool->sparse[pos] = -1;

	pool->dense_count--;

	memmove(
		&((char*)pool->data)[pos * pool->element_size],
		&((char*)pool->data)[(pool->count - 1) * pool->element_size],
		pool->element_size);

	pool->count--;
}

static void* pool_get_by_idx(struct pool* pool, i32 idx) {
	return &((char*)pool->data)[idx * pool->element_size];
}

static void* pool_get(struct pool* pool, entity e) {
	return pool_get_by_idx(pool, pool_sparse_idx(pool, e));
}

struct world {
	struct pool* pools;
	u32 pool_count;
	u32 pool_capacity;

	entity* entities;
	u32 entity_count;
	u32 entity_capacity;

	entity_id avail_id;
};

static entity generate_entity(struct world* world) {
	if (world->entity_count >= world->entity_capacity) {
		world->entity_capacity = world->entity_capacity < 8 ? 8 : world->entity_capacity * 2;
		world->entities = realloc(world->entities, world->entity_capacity * sizeof(entity));
	}

	const entity e = make_handle(world->entity_count, 0);
	world->entities[world->entity_count++] = e;

	return e;
}

static entity recycle_entity(struct world* world) {
	const entity_id cur_id = world->avail_id;
	const entity_version cur_ver = get_entity_version(world->entities[cur_id]);
	world->avail_id = get_entity_id(world->entities[cur_id]);
	const entity recycled = make_handle(cur_id, cur_ver);
	world->entities[cur_id] = recycled;
	return recycled;
}

static void release_entity(struct world* world, entity e, entity_version desired) {
	const entity_id id = get_entity_id(e);
	world->entities[id] = make_handle(world->avail_id, desired);
	world->avail_id = id;
}

static struct pool* get_pool(struct world* world, struct type_info type) {
	struct pool* r = null;

	for (u32 i = 0; i < world->pool_count; i++) {
		if (world->pools[i].type_id == type.id) {
			r = &world->pools[i];
		}
	}

	if (r) {
		return r;
	} else {
		if (world->pool_count >= world->pool_capacity) {
			world->pool_capacity = world->pool_capacity < 8 ? 8 : world->pool_capacity * 2;
			world->pools = realloc(world->pools, world->pool_capacity * sizeof(struct pool));
		}

		struct pool* new = &world->pools[world->pool_count++];
		init_pool(new, world, type);
		return new;
	}
}

struct world* new_world() {
	struct world* w = calloc(1, sizeof(struct world));

	w->avail_id = null_entity_id;

	return w;
}

void free_world(struct world* world) {
	if (world->pools) {
		for (u32 i = 0; i < world->pool_count; i++) {
			deinit_pool(&world->pools[i]);
		}

		free(world->pools);
	}

	if (world->entities) {
		free(world->entities);
	}

	free(world);
}

entity new_entity(struct world* world) {
	if (world->avail_id == null_entity_id) {
		return generate_entity(world);
	} else {
		return recycle_entity(world);
	}
}

void destroy_entity(struct world* world, entity e) {
	for (u32 i = 0; i < world->pool_count; i++) {
		if (pool_has(&world->pools[i], e)) {
			pool_remove(&world->pools[i], e);
		}
	}

	const entity nv = get_entity_version(e) + 1;
	release_entity(world, e, nv);
}

bool entity_valid(struct world* world, entity e) {
	const entity_id id = get_entity_id(e);
	return id < world->entity_count && world->entities[id] == e;
}

void _set_component_create_func(struct world* world, struct type_info type, component_create_func f) {
	get_pool(world, type)->on_create = f;
}

void _set_component_destroy_func(struct world* world, struct type_info type, component_create_func f) {
	get_pool(world, type)->on_destroy = f;
}

void* _add_component(struct world* world, entity e, struct type_info type, void* init) {
	struct pool* pool = get_pool(world, type);

	return pool_add(pool, e, init);
}

void _remove_component(struct world* world, entity e, struct type_info type) {
	struct pool* p = get_pool(world, type);
	
	pool_remove(p, e);
}

bool _has_component(struct world* world, entity e, struct type_info type) {
	return pool_has(get_pool(world, type), e);
}

void* _get_component(struct world* world, entity e, struct type_info type) {
	return pool_get(get_pool(world, type), e);
}

struct single_view _new_single_view(struct world* world, struct type_info type) {
	struct single_view v = { 0 };
	v.pool = get_pool(world, type);

	struct pool* pool = (struct pool*)v.pool;
	if (pool->count != 0) {
		v.idx = pool->count - 1;
		v.e = pool->dense[v.idx];
	} else {
		v.idx = 0;
		v.e = null_entity;
	}

	return v;
}

bool single_view_valid(struct single_view* view) {
	return (view->e != null_entity);
}

void* single_view_get(struct single_view* view) {
	return pool_get_by_idx(view->pool, view->idx);
}

void single_view_next(struct single_view* view) {
	if (view->idx) {
		view->idx--;
		view->e = ((struct pool*)view->pool)->dense[view->idx];
	} else {
		view->e = null_entity;
	}
}

static bool view_contains(struct view* view, entity e) {
	for (u32 i = 0; i < view->pool_count; i++) {
		if (!pool_has(view->pools[i], e)) {
			return false;
		}
	}

	return true;
}

static u32 view_get_idx(struct view* view, struct type_info type) {
	for (u32 i = 0; i < view->pool_count; i++) {
		if (view->to_pool[i] == type.id) {
			return i;
		}
	}

	return 0;
}

struct view new_view(struct world* world, u32 type_count, struct type_info* types) {
	struct view v = { 0 };
	v.pool_count = type_count;

	for (u32 i = 0; i < type_count; i++) {
		v.pools[i] = get_pool(world, types[i]);
		if (!v.pool) {
			v.pool = v.pools[i];
		} else {
			if (((struct pool*)v.pools[i])->count < ((struct pool*)v.pool)->count) {
				v.pool = v.pools[i];
			}
		}
		v.to_pool[i] = types[i].id;
	}

	if (v.pool && ((struct pool*)v.pool)->count != 0) {
		v.idx = ((struct pool*)v.pool)->count - 1;
		v.e = ((struct pool*)v.pool)->dense[v.idx];
		if (!view_contains(&v, v.e)) {
			view_next(&v);
		}
	} else {
		v.idx = 0;
		v.e = null_entity;
	}

	return v;
}

bool view_valid(struct view* view) {
	return view->e != null_entity;
}

void* _view_get(struct view* view, struct type_info type) {
	return pool_get(view->pools[view_get_idx(view, type)], view->e);
}

void view_next(struct view* view) {
	do {
		if (view->idx) {
			view->idx--;
			view->e = ((struct pool*)view->pool)->dense[view->idx];
		} else {
			view->e = null_entity;
		}
	} while ((view->e != null_entity) && !view_contains(view, view->e));
}
