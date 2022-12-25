"use strict";

const HSHG = require("./hshg");

(async function() {
    const engine = await new HSHG(16, 4, 100);

    console.log(engine);

    engine.insert(10, 20, 5, 0);

    engine.insert(20, 0, 100, 1);

    engine.update = function(entity) {
        console.log("updating", entity);
    };

    engine.update();

    engine.collide = function(ent_a, ent_b) {
        console.log("colliding", ent_a, ent_b);
    };

    engine.collide();

    engine.query = function(entity) {
        console.log("querying", entity);
    };

    engine.query(20, 0, 20, 0);
})();
