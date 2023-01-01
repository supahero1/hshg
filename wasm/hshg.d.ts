declare module "hshg" {
    type HSHG_Callback_1 = (entity: Object) => void;
    type HSHG_Callback_2 = (ent_a: Object, ent_b: Object) => void;

    class HSHG {
        /**
         * The rough amount of memory allocated for the HSHG.
         */
        mem: number;

        /**
         * Creates a new HSHG. Do not worry about the power of 2 requirements.
         * No operation scales with the number of cells. You will likely get
         * higher performance with more cells, so go for it. Any exceptions,
         * like overflows caused by you trying to insert more entities than you
         * declared you would, will result in a crash.
         *
         * @param side Number of cells on the edge of the area, must be
         * a power of 2.
         *
         * @param size Size of the smallest cell, must be a power of 2.
         * All subsequent cells, if any, created by the HSHG, will be powers of
         * 2 multiplied by this number. If you go too high, you might have
         * performance issues.
         *
         * @param max_entities The maximum number of entities you ever
         * expect the HSHG to handle. You will not be able to go above that
         * number.
         *
         * @returns a Promise that then returns the HSHG.
         */
        constructor(side: number, size: number, max_entities: number);

        /**
         * If invoked in an update call, removes the currently updated entity.
         */
        remove(): void;

        /**
         * Sets the update callback for the HSHG.
         */
        set update(callback: HSHG_Callback_1);

        /**
         * Get the callback needed to actually perform an update.
         */
        get update(): () => void;

        /**
         * Sets the collision callback for the HSHG.
         */
        set collide(callback: HSHG_Callback_2);

        /**
         * Get the callback needed to actually perform a collision.
         */
        get collide(): () => void;

        /**
         * Sets the query callback for the HSHG.
         */
        set query(callback: (entity: any, ..._: any[]) => void);
    }

    export class HSHG_1D extends HSHG {
        /**
         * Inserts the given entity.
         *
         * The entity must have the following fields:
         *
         * - `x` - the X coordinate
         *
         * - `r` - the radius
         *
         * It may also have other, application-defined fields.
         */
        insert(entity: Object): void;

        /**
         * Get the callback needed to actually perform a query.
         */
        get query(): (min_x: Number, max_x: Number) => void;
    }

    export class HSHG_2D extends HSHG {
        /**
         * Inserts the given entity.
         *
         * The entity must have the following fields:
         *
         * - `x` - the X coordinate
         *
         * - `y` - the Y coordinate
         *
         * - `r` - the radius
         *
         * It may also have other, application-defined fields.
         */
        insert(entity: Object): void;

        /**
         * Get the callback needed to actually perform a query.
         */
        get query(): (min_x: Number, min_y: Number,
            max_x: Number, max_y: Number) => void;
    }

    export class HSHG_3D extends HSHG {
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
         */
        insert(entity: Object): void;

        /**
         * Get the callback needed to actually perform a query.
         */
        get query(): (min_x: Number, min_y: Number, min_z: Number,
            max_x: Number, max_y: Number, max_z: Number) => void;
    }
}
