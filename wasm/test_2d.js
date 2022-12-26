"use strict";

const ENTITIES_NUM = 10000;

const SIDE = 1024;
const SIZE = 16;
const MAP_SIZE = SIDE * SIZE;

let ctx;
let min_x, min_y;

const R_TICKS = 100;

const Rs = new Array(R_TICKS);

for(let i = 0; i < R_TICKS; ++i) {
    Rs[i] = Math.sin(Math.PI * 2 * (i / R_TICKS));
}

/* https://stackoverflow.com/a/49434653/17113867 */
function gaussianRandom(mean=0, stdev=1) {
    let u = 1 - Math.random(); //Converting [0,1) to (0,1)
    let v = Math.random();
    let z = Math.sqrt( -2.0 * Math.log( u ) ) * Math.cos( 2.0 * Math.PI * v );
    // Transform to the desired mean and standard deviation:
    return z * stdev + mean;
}

class Entity {
    x;
    y;
    r;

    vx;
    vy;

    r_tick = 0;

    constructor() {
        this.r = 7 + Math.random() * gaussianRandom(0, 0.2) * 100;
        this.x = this.r + Math.random() * (MAP_SIZE - this.r * 2);
        this.y = this.r + Math.random() * (MAP_SIZE - this.r * 2);

        this.vx = (Math.random() - 1) * 4;
        this.vy = (Math.random() - 1) * 4;
    }

    update() {
        this.x += this.vx;

        if(this.x - this.r < 0) {
            this.vx *= 0.9;
            ++this.vx;
        } else if(this.x + this.r > SIDE * SIZE) {
            this.vx *= 0.9;
            --this.vx;
        }


        this.y += this.vy;

        if(this.y - this.r < 0) {
            this.vy *= 0.9;
            ++this.vy;
        } else if(this.y + this.r > SIDE * SIZE) {
            this.vy *= 0.9;
            --this.vy;
        }


        this.r_tick = (this.r_tick + 1) % R_TICKS;
        this.r += Rs[this.r_tick];
    }

    draw() {
        ctx.beginPath();
        ctx.strokeStyle = "#DDD";
        ctx.arc(this.x - min_x, this.y - min_y, this.r, 0, Math.PI * 2);
        ctx.stroke();
        ctx.closePath();
    }

    collide(other) {
        const dr = this.r + other.r;

        let dx = this.x - other.x;
        let dy = this.y - other.y;

        const dist = dx * dx + dy * dy;

        if(dist > dr * dr) return;

        const mag = 1 / Math.sqrt(dist);

        dx *= mag;
        dy *= mag;

        this.vx *= 0.75;
        this.vx += dx;

        this.vy *= 0.75;
        this.vy += dy;

        other.vx *= 0.75;
        other.vx -= dx;

        other.vy *= 0.75;
        other.vy -= dy;
    }
}

const requestAnimationFrame = function(cb) {
    setTimeout(cb, 1000 / 60);
}

const { HSHG_2D } = require("./hshg");

(new HSHG_2D(SIDE, SIZE, ENTITIES_NUM)).then(function(engine) {
    engine.update = function(entity) {
        entity.update();
    };

    engine.collide = function(entity, other) {
        entity.collide(other);
    };

    engine.query = function(entity) {
        entity.draw();
    };

    for(let i = 0; i < ENTITIES_NUM; ++i) {
        engine.insert(new Entity());
    }

    const { Window } = require("skia-canvas");

    const window = new Window(1600, 900);

    window.title = "ezeza";

    const canvas = window.canvas;

    ctx = window.ctx;

    window.on("draw", function() {
        ctx.fillStyle = "#333";
        ctx.fillRect(0, 0, canvas.width, canvas.height);

        console.time("tick");

        engine.update();
        engine.collide();

        min_x = (MAP_SIZE - canvas.width) / 2;
        min_y = (MAP_SIZE - canvas.height) / 2;

        engine.query(
            min_x, min_y,
            (MAP_SIZE + canvas.width) / 2,
            (MAP_SIZE + canvas.height) / 2
        );

        console.timeEnd("tick");
    });
});
