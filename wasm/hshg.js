"use strict";

const binary = require("node:fs").readFileSync("./hshg_opt.wasm");

class HSHG {
    #exports;
    #f32_memory;
    #u32_memory;
    #head;

    mem;

    #entity_flag;

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
            this.#u32_memory = new Uint32Array(mem.buffer);

            this.#head = this.#exports.init(side, size, max_entities) >> 2;

            resolve(this);
        }.bind(this));
    }

    /**
     * Get entity with given ID.
     *
     * @param {number} id ID of the entity.
     */
    #get_entity(id) {
        const idx = this.#head + (id << 3) + 4;

        return {
            ref: this.#u32_memory[idx + 0],
            x: this.#f32_memory[idx + 1],
            y: this.#f32_memory[idx + 2],
            r: this.#f32_memory[idx + 3]
        };
    }

    /**
     * Set entity with given ID to the new contents.
     *
     * @param {number} id ID of the entity.
     *
     * @param {object} entity The entity in question.
     */
    #set_entity(id, entity) {
        const idx = this.#head + (id << 3) + 4;

        this.#u32_memory[idx + 0] = entity.ref;
        this.#f32_memory[idx + 1] = entity.x;
        this.#f32_memory[idx + 2] = entity.y;
        this.#f32_memory[idx + 3] = entity.r;
    }

    /**
     * Inserts the given entity.
     *
     * @param {number} x The X position.
     *
     * @param {number} y The Y position.
     *
     * @param {number} r The radius.
     *
     * @param {number} ref The reference, used as simply data to something.
     */
    insert(x, y, r, ref) {
        this.#exports.insert(x, y, r, ref);
    }

    /**
     * If invoked in an update call, removes the currently updated entity.
     */
    remove() {
        this.#entity_flag = 1;
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
        HSHG.#sanitize_callback(callback, 1);

        this.#update_cb = callback;
    }

    /**
     * Get the callback needed to actually perform an update.
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
     */
    #on_update(id) {
        const entity = this.#get_entity(id);

        this.#entity_flag = 0;

        this.#update_cb(entity);

        if(this.#entity_flag === 1) {
            return 4;
        }

        let flags = 0;

        const old_entity = this.#get_entity(id);

        if(entity.x != old_entity.x || entity.y != old_entity.y) {
            flags |= 1;
        }

        if(entity.r != old_entity.r) {
            flags |= 2;
        }

        if(flags != 0) {
            this.#set_entity(id, entity);
        }

        return flags;
    }

    /**
     * Sets the collision callback for the HSHG.
     *
     * @param {function(object, object):void} callback
     */
    set collide(callback) {
        HSHG.#sanitize_callback(callback, 2);

        this.#collision_cb = callback;
    }

    /**
     * Get the callback needed to actually perform a collision.
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
        const entity_a = this.#get_entity(a_id);
        const entity_b = this.#get_entity(b_id);

        this.#collision_cb(entity_a, entity_b);
    }

    /**
     * Sets the query callback for the HSHG.
     *
     * @param {function(object):void} callback
     */
    set query(callback) {
        HSHG.#sanitize_callback(callback, 1);

        this.#query_cb = callback;
    }

    /**
     * Get the callback needed to actually perform a query.
     */
    get query() {
        return this.#do_query;
    }

    /**
     * Internal query invoker.
     *
     * @param {number} min_x
     * @param {number} min_y
     * @param {number} max_x
     * @param {number} max_y
     */
    #do_query(min_x, min_y, max_x, max_y) {
        this.#exports.query(min_x, min_y, max_x, max_y);
    }

    /**
     * Internal query handler.
     */
    #on_query(id) {
        const entity = this.#get_entity(id);

        this.#query_cb(entity);
    }
}

module.exports = HSHG;
