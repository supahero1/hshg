"use strict";

const { readFileSync } = require("node:fs");

const binary = readFileSync("./hshg_opt.wasm");

const imports = {
    on_update: function(id) {
        console.log("update", id);
    },
    on_collision: function(a_id, b_id) {
        console.log("collision", a_id, b_id);
    },
    on_query: function(id) {
        console.log("query", id);
    }
};

imports.memory = new WebAssembly.Memory({ initial: 10 });

WebAssembly.instantiate(binary, { env: imports }).then(run);

function run(module) {
    const exports = module.instance.exports;
    console.log(exports);

    exports.init(256, 32, 32767);

    console.log("done");

    exports.insert(0, 0, 10, 0);

    exports.insert(2, 2, 2, 1);

    exports.update();

    exports.collide();

    exports.query(-3, -3, -3, -3);
}
