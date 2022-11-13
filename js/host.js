const { readFileSync } = require("fs");
const express = require("express");

function read(path) {
  return readFileSync(path, "utf8");
}

const index_html = read("./index.html");
const hshg_js = read("./hshg.js");
const hshg_wasm = readFileSync("./hshg.wasm");

const app = express();

app.get(["/", "/index.html"], function(req, res) {
  res.set("Content-Type", "text/html");
  res.status(200).end(index_html);
});

app.get("/hshg.js", function(req, res) {
  res.set("Content-Type", "text/javascript");
  res.status(200).end(hshg_js);
});

app.get("/hshg.wasm", function(req, res) {
  res.set("Content-Type", "application/wasm");
  res.status(200).end(hshg_wasm);
});

app.listen(9123, function() {
  console.log("Simulation is running at http://localhost:9123");
});
