let hshg;
try {
  hshg = require("./build/Release/hshg.node");
} catch(err) {
  hshg = require("./build/Debug/hshg.node");
}

const AGENTS_NUM = 1000;

const CELLS_SIDE = 2048;
const AGENT_R = 7;
const CELL_SIZE = 16;
const ARENA_WIDTH = (CELLS_SIDE * CELL_SIZE);
const ARENA_HEIGHT = (CELLS_SIDE * CELL_SIZE);

const LATENCY_NUM = 1;

const init_data = new Array(AGENTS_NUM);
for(let i = 0; i < AGENTS_NUM; ++i) {
  init_data[i] = [ARENA_WIDTH * Math.random(), ARENA_HEIGHT * Math.random(), AGENT_R, i];
}

const balls = new Array(AGENTS_NUM);
for(let i = 0; i < AGENTS_NUM; ++i) {
  balls[i] = { vx: Math.random() * 2 - 1, vy: Math.random() * 2 - 1 };
}

const engine = new hshg(CELLS_SIDE, CELL_SIZE);
engine.entities_size = AGENTS_NUM + 1;

function update(entity) {
  const ball = balls[entity.ref];

  entity.x += ball.vx;
  if(entity.x < entity.r) {
    ball.vx *= 0.9;
    ++ball.vx;
  } else if(entity.x + entity.r >= ARENA_WIDTH) {
    ball.vx *= 0.9;
    --ball.vx;
  }
  ball.vx *= 0.997;
  
  entity.y += ball.vy;
  if(entity.y < entity.r) {
    ball.vy *= 0.9;
    ++ball.vy;
  } else if(entity.y + entity.r >= ARENA_HEIGHT) {
    ball.vy *= 0.9;
    --ball.vy;
  }
  ball.vy *= 0.997;
  
  engine.move();
}

engine.update(update);

function collide(ent_a, ent_b) {
  const xd = ent_a.x - ent_b.x;
  const yd = ent_a.y - ent_b.y;
  const d = xd * xd + yd * yd;
  if(d <= (ent_a.r + ent_b.r) * (ent_a.r + ent_b.r)) {
    const angle = Math.atan2(yd, xd);
    const c = Math.cos(angle);
    const s = Math.sin(angle);
    const ball_a = balls[ent_a.ref];
    const ball_b = balls[ent_b.ref];
    ball_a.vx += c;
    ball_a.vy += s;
    ball_b.vx -= c;
    ball_b.vy -= s;
  }
}

engine.collide(collide);


console.time("insertion");
for(let i = 0; i < AGENTS_NUM; ++i) {
  engine.insert(...init_data[i]);
}
console.timeEnd("insertion");

function tick() {
  console.time("upd");
  engine.update();
  console.timeEnd("upd");
  console.time("col");
  engine.collide();
  console.timeEnd("col");
  console.log("");
}

setInterval(tick, 100);

module.exports = hshg;
