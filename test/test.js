function connect(server, port) {
  var socket = null;

  try {
    if (socket === null) {
      socket = new WebSocket("ws://" + server + ":" + port);
    }
  } catch (exception) {
      alert("Failed to connect to server...");
  }

  socket.onerror = function(error) { };
  socket.onopen = function(event) {
      console.log("Connection established");
      this.binaryType = "arraybuffer";

      this.onmessage = function(event) { };
      this.onclose = function(event) {
          console.log("Connection closed");
          socket = null;
      };
  };
}