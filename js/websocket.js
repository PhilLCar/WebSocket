function connect(server, port) {
  var socket    = null;

  try {
    if (socket === null) {
      socket = new WebSocket("ws://" + server + ":" + port);
    }
  } catch (exception) {
      alert("Failed to connect to server...");
  }

  socket.onerror = function(error) {
  };

  socket.onopen = function(event) {
      console.log("Connection established");
      var interface = new Interface(socket);

      // Lorsque la connexion se termine.
      this.onclose = function(event) {
          console.log("Connection closed");
          DM.interface = null;
          socket       = null;
      };

      // Lorsque le serveur envoi un message.
      this.onmessage = function(event) {
        var response = new WebSocketResponse(new Uint8Array(event.data));
        DM.interface.update(response);
      };
      
      var init = new Uint8Array(1);
      init[0]  = PHILIPONT_MAGIC_ID;
      this.binaryType = "arraybuffer";
      this.send(init);
  };
}

const PHILIPONT_MAGIC_ID = 0x02;

const CMD_IDENT        = 0x0F
const CMD_NEW_LEVEL    = 0x00;
const CMD_LOAD_LEVEL   = 0x01;
const CMD_SAVE_LEVEL   = 0x02;
const CMD_UPDATE_LEVEL = 0x03;
const CMD_SIMULATE     = 0x10;
const CMD_PLAY         = 0x11;
const CMD_PAUSE        = 0x12;
const CMD_SPEED        = 0x13;

const ACK_IDENT        = 0xFF;
const ACK_NEW_LEVEL    = 0xF0;

const SKIN_EARTH = 0;
const SKIN_MARS  = 1;
const SKIN_MOON  = 2;
const SKIN_VENUS = 3;

const NODE_MAX_LINK = 32;
const LINK_MAX_NODE =  4;

class Interface {
  constructor(socket) {
    this.socket = socket;
    this.level  = null;
  }

  load() {
    this.identify(12, "01234567890123456789012345678901");
    //newLevel("WOOHOO!", "Phil za best");
  }
  
  update(response) {
    switch (response.get(WebSocketResponse.UBYTE)) {
      case ACK_IDENT:
        this.newLevel("WOOHOO!", "Phil za best", 10.0, 10.0, 0.5);
        break;
      case ACK_NEW_LEVEL:
        DM.scene.destroyBuffers();
        this.level = this.parseLevel(response);
        DM.scene.initBuffers(this.level);
        break;
    }
  }

  identify(userid, passhash) {
    if (passhash.length != 32) return;
    var request = new WebSocketRequest(this.socket);
    request.append(CMD_IDENT);
    request.append(userid, 32);
    request.append(passhash);
    request.send();
  }

  newLevel(name, designer, terrainX, terrainZ, terrainRes) {
    if (name.length > 255 || designer.length > 255) return;
    if ((Math.floor(terrainX / terrainRes) + 1) * (Math.floor(terrainZ / terrainRes)) > 256 * 256) return;
    var request = new WebSocketRequest(this.socket);
    request.append(CMD_NEW_LEVEL);
    request.append(63, 32);
    request.append(name.length);
    request.append(name);
    request.append(designer.length);
    request.append(designer);
    request.append(terrainX,   64, false);
    request.append(terrainZ,   64, false);
    request.append(terrainRes, 64, false);
    request.send();
  }

  parseLevel(response) {
    var level = {};
    var length;
    // Metadata
    level.lid      = response.get(WebSocketResponse.INT);
    level.uid      = response.get(WebSocketResponse.INT);
    length         = response.get(WebSocketResponse.UBYTE);
    level.name     = response.get(WebSocketResponse.STRING, length);
    length         = response.get(WebSocketResponse.UBYTE);
    level.designer = response.get(WebSocketResponse.STRING, length);
    level.auth     = response.get(WebSocketResponse.STRING, 32);
    // Terrain
    level.waterLevel   = response.get(WebSocketResponse.DOUBLE);
    level.terrainSizeX = response.get(WebSocketResponse.DOUBLE);
    level.terrainSizeZ = response.get(WebSocketResponse.DOUBLE);
    level.terrainRes   = response.get(WebSocketResponse.DOUBLE);
    level.terrain      = response.get(WebSocketResponse.CUSTOM,
                                      (Math.floor(level.terrainSizeX / level.terrainRes) + 1) *
                                      (Math.floor(level.terrainSizeZ / level.terrainRes) + 1),
                                      new vec3Converter());
    // Road
    level.roadSegments = response.get(WebSocketResponse.INT);
    level.road         = response.get(WebSocketResponse.CUSTOM,
                                      4 * level.roadSegments, new vec3Converter());
    // Environment
    level.skin        = response.get(WebSocketResponse.INT);
    level.atmoDensity = response.get(WebSocketResponse.DOUBLE);
    level.humidity    = response.get(WebSocketResponse.DOUBLE);
    level.windSpeed   = response.get(WebSocketResponse.CUSTOM, null, new vec3Converter());
    level.gravity     = response.get(WebSocketResponse.CUSTOM, null, new vec3Converter());
    // Bridge nodes
    level.nodes = [];
    length      = response.get(WebSocketResponse.SHORT);
    for (var i = 0; i < length; i++) {
      var node = {};
      node.id           = response.get(WebSocketResponse.INT);
      node.type         = response.get(WebSocketResponse.INT);
      node.nlinks       = response.get(WebSocketResponse.INT);
      node.position     = response.get(WebSocketResponse.CUSTOM, null, new vec3Converter());
      node.speed        = response.get(WebSocketResponse.CUSTOM, null, new vec3Converter());
      node.acceleration = response.get(WebSocketResponse.CUSTOM, null, new vec3Converter());
      node.links = [];
      for (var j = 0; j < node.nlinks && j < NODE_MAX_LINK; j++) {
        node.links.push(response.get(WebSocketResponse.SHORT));
      }
      level.nodes.push(node);
    }
    // Bridge links
    level.links = [];
    length      = response.get(WebSocketResponse.SHORT);
    for (var i = 0; i < length; i++) {
      var link = {};
      link.id           = response.get(WebSocketResponse.INT);
      link.material     = response.get(WebSocketResponse.INT);
      link.length       = response.get(WebSocketResponse.DOUBLE);
      link.longStress   = response.get(WebSocketResponse.DOUBLE);
      link.rotStress    = response.get(WebSocketResponse.DOUBLE);
      link.nodes = [];
      for (var j = 0; j < LINK_MAX_NODE; j++) {
        link.nodes.push(response.get(WebSocketResponse.SHORT));
      }
      level.links.push(link);
    }
    level.gridRes    = 0.1;
    level.gridZ      = 1;
    return level;
  }
}

function vec3Converter() {
  this.typeSize = 3 * 8; // 3 doubles
  this.convert = function (array) {
    var f64 = new Float64Array(array.buffer);
    return glMatrix.vec3.fromValues(f64[0], f64[1], f64[2]);
  }
}