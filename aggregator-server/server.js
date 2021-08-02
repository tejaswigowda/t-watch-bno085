var express = require("express");
var app = express();
var bodyParser = require('body-parser');
var errorHandler = require('errorhandler');
var methodOverride = require('method-override');
var hostname = process.env.HOSTNAME || 'localhost';
var port = 1234;
var qte= require('quaternion-to-euler');

app.get("/", function (req, res) {
    res.redirect("index.html")
});

var values = {}
app.get("/setValue", function (req, res) {
  res.send("1");
  var euler = qte([parseFloat(req.query.w), parseFloat(req.query.x), parseFloat(req.query.y), parseFloat(req.query.z)]);
  req.query.w = parseFloat(req.query.w);
  req.query.x = parseFloat(req.query.x);
  req.query.y = parseFloat(req.query.y);
  req.query.z = parseFloat(req.query.z);
  req.query.xE = euler[0]
  req.query.yE = euler[1]
  req.query.zE = euler[2]
  values[req.query.id] = req.query
});

app.get("/getValue", function (req, res) {
  res.send(values[req.query.id]);
});

app.get("/getAllValues", function (req, res) {
  res.send(JSON.stringify(values));
});


app.use(methodOverride());
app.use(bodyParser());
app.use(express.static(__dirname + '/public'));
app.use(errorHandler());

console.log("Simple static server listening at http://" + hostname + ":" + port);
app.listen(port);



const WebSocket = require('ws')

const wss = new WebSocket.Server({ port: 3000})

wss.on('connection', ws => {
  ws.on('message', message => {
    // console.log(`Received message => ${message}`)
    wss.clients.forEach(function each(client) {
      if (client !== ws && client.readyState === WebSocket.OPEN) {
        client.send(message, { binary: false});
      }
    });
  })
  ws.send('start');
})

