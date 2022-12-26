"use strict";

const binary = require("node:fs").readFileSync("./hshg_3d.wasm");

class HSHG_3D {
    #exports;
    #f32_memory;
    #head = 0;

    #entities;
    #entities_len = 1;
    #free_entity = 0;

    mem = 0;

    #entity_id = 0;

    #update_cb = function(id){};
    #collision_cb = function(a_id, b_id){};
    #query_cb = function(id){};

    /**
     * Creates a new HSHG. Do not worry about the power of 2 requirements. No
     * operation scales with the number of cells. You will likely get higher
     * performance with more cells, so go for it. Any exceptions, like overflows
     * caused by you trying to insert more entities than you declared you would,
     * will result in a crash.
     *
     * @param {number} side Number of cells on the edge of the area, must be a
     * power of 2.
     *
     * @param {number} size Size of the smallest cell, must be a power of 2.
     * All subsequent cells, if any, created by the HSHG, will be powers of 2
     * multiplied by this number. If you go too high, you might have performance
     * issues.
     *
     * @param {number} max_entities The maximum number of entities you ever
     * expect the HSHG to handle. You will not be able to go above that number.
     */
    constructor(side, size, max_entities) {
        return new Promise(async function(resolve) {
            const mem = new WebAssembly.Memory({ initial: 2 });

            const imports = {
                on_update: this.#on_update.bind(this),
                on_collision: this.#on_collision.bind(this),
                on_query: this.#on_query.bind(this),
                memory: mem
            };

            const module =
                await WebAssembly.instantiate(binary, { env: imports });

            this.#exports = module.instance.exports;

            ++max_entities;

            this.mem = this.#exports.mem(side, max_entities);

            imports.memory.grow(Math.ceil(this.mem / 65536) - 1);

            this.#f32_memory = new Float32Array(mem.buffer);

            this.#head = this.#exports.init(side, size, max_entities) >> 2;

            this.#entities = new Array(max_entities);

            resolve(this);
        }.bind(this));
    }

    /**
     * Return the internal C flags for the underlying code to know what to do.
     *
     * @param {number} id Internal ID of the entity.
     *
     * @param {object} entity The entity to update.
     *
     * @returns {number} Internal flags.
     */
    #handle_entity_update(id, entity) {
        const idx = this.#head + id * 9 + 5;

        const x = this.#f32_memory[idx + 0];
        const y = this.#f32_memory[idx + 1];
        const z = this.#f32_memory[idx + 2];
        const r = this.#f32_memory[idx + 3];

        let flags = 0;

        if(entity.x != x) {
            this.#f32_memory[idx + 0] = entity.x;

            flags |= 1;
        }

        if(entity.y != y) {
            this.#f32_memory[idx + 1] = entity.y;

            flags |= 1;
        }

        if(entity.z != z) {
            this.#f32_memory[idx + 2] = entity.z;

            flags |= 1;
        }

        if(entity.r != r) {
            this.#f32_memory[idx + 3] = entity.r;

            flags |= 2;
        }

        return flags;
    }

    /**
     * Get internal index to the next free entity spot.
     */
    #get_idx() {
        if(this.#free_entity != 0) {
            const ret = this.#free_entity;

            this.#free_entity = this.#entities[ret];

            return ret;
        }

        return this.#entities_len++;
    }

    /**
     * Return the internal entity index for later reuse.
     *
     * @param {number} idx The internal index of the entity.
     */
    #ret_idx(idx) {
        this.#entities[idx] = this.#free_entity;

        this.#free_entity = idx;
    }

    /**
     * Inserts the given entity.
     *
     * The entity must have the following fields:
     *
     * - `x` - the X coordinate
     *
     * - `y` - the Y coordinate
     *
     * - `z` - the Z coordinate
     *
     * - `r` - the radius
     *
     * It may also have other, application-defined fields.
     *
     * @param {object} entity The entity.
     */
    insert(entity) {
        const idx = this.#get_idx();

        this.#entities[idx] = entity;

        this.#exports.insert(entity.x, entity.y, entity.z, entity.r, idx);
    }

    /**
     * If invoked in an update call, removes the currently updated entity.
     */
    remove() {
        this.#ret_idx(this.#entity_id);

        this.#entity_id = 0;
    }

    /**
     * Makes sure callbacks aren't something stupid.
     *
     * @param {function():void} callback The callback in question.
     *
     * @param {number} expected_arg_len Required number of arguments.
     */
    static #sanitize_callback(callback, expected_arg_len) {
        if(typeof callback != "function") {
            throw new Error("Expected a function.");
        }
        if(callback.length != expected_arg_len) {
            throw new Error("Expected a callback with " +
                expected_arg_len + " arguments with no default values.");
        }
    }

    /**
     * Sets the update callback for the HSHG.
     *
     * @param {function(object):void} callback
     */
    set update(callback) {
        HSHG_3D.#sanitize_callback(callback, 1);

        this.#update_cb = callback;
    }

    /**
     * Get the callback needed to actually perform an update.
     *
     * @returns {function():void}
     */
    get update() {
        return this.#do_update;
    }

    /**
     * Internal update invoker.
     */
    #do_update() {
        this.#exports.update();
    }

    /**
     * Internal update handler.
     *
     * @param {number} id The updated entity's internal ID.
     *
     * @returns {number} Internal flags.
     */
    #on_update(id) {
        const entity = this.#entities[id];

        this.#entity_id = id;

        this.#update_cb(entity);

        if(this.#entity_id === 0) {
            /* Remove */
            return 4;
        }

        return this.#handle_entity_update(id, entity);
    }

    /**
     * Sets the collision callback for the HSHG.
     *
     * @param {function(object, object):void} callback
     */
    set collide(callback) {
        HSHG_3D.#sanitize_callback(callback, 2);

        this.#collision_cb = callback;
    }

    /**
     * Get the callback needed to actually perform a collision.
     *
     * @returns {function():void}
     */
    get collide() {
        return this.#do_collision;
    }

    /**
     * Internal collision invoker.
     */
    #do_collision() {
        this.#exports.collide();
    }

    /**
     * Internal collision handler.
     */
    #on_collision(a_id, b_id) {
        const entity_a = this.#entities[a_id];
        const entity_b = this.#entities[b_id];

        this.#collision_cb(entity_a, entity_b);
    }

    /**
     * Sets the query callback for the HSHG.
     *
     * @param {function(object):void} callback
     */
    set query(callback) {
        HSHG_3D.#sanitize_callback(callback, 1);

        this.#query_cb = callback;
    }

    /**
     * Get the callback needed to actually perform a query.
     *
     * @returns {function(number, number, number, number, number, number):void}
     */
    get query() {
        return this.#do_query;
    }

    /**
     * Internal query invoker.
     *
     * @param {number} min_x
     * @param {number} min_y
     * @param {number} min_z
     * @param {number} max_x
     * @param {number} max_y
     * @param {number} max_z
     */
    #do_query(min_x, min_y, min_z, max_x, max_y, max_z) {
        this.#exports.query(min_x, min_y, min_z, max_x, max_y, max_z);
    }

    /**
     * Internal query handler.
     */
    #on_query(id) {
        const entity = this.#entities[id];

        this.#query_cb(entity);
    }
}

module.exports = HSHG_3D;
