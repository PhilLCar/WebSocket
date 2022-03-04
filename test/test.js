/* Author: Philippe Caron (philippe-caron@hotmail.com)
 * Date: 03 Mar 2022
 * Description: Very simple client for testing the WebSocket server.
 */

var _socket = null;

function pad(number) {
  if (number < 10) return "0" + number.toString();
  return number.toString();
}

function connect(server, port) {

  try {
    if (_socket === null) {
      _socket = new WebSocket("ws://" + server + ":" + port);
    }
  } catch (exception) {
      alert("Failed to connect to server...");
  }

  _socket.onerror = function(error) { };
  _socket.onopen  = function(event) {
      console.log("Connection established");
      document.getElementById("connect").disabled      =  true;
      document.getElementById("ping").disabled         =  true;
      document.getElementById("disconnect").disabled   = false;
      document.getElementById("send_textbox").disabled = false;
      document.getElementById("send_button").disabled  = false;
      document.getElementById("status_icon").setAttribute("status", "connected");
      document.getElementById("status_text").innerHTML = "Connected (" + port + ")";
      this.binaryType = "arraybuffer";

      this.onmessage = function(event) {
        var date = new Date();
        var format = "[";
        format += date.getFullYear()       + "/";
        format += pad(date.getMonth() + 1) + "/";
        format += pad(date.getDate())      + " ";
        format += pad(date.getHours())     + ":";
        format += pad(date.getMinutes())   + ":";
        format += pad(date.getSeconds())   + "]";
        format += " From server: " + event.data + "<br>";
        document.getElementById("messages").innerHTML += format;
      };
      this.onclose   = function(event) {
        console.log("Connection closed");
        document.getElementById("connect").disabled      = false;
        document.getElementById("ping").disabled         =  true;
        document.getElementById("disconnect").disabled   =  true;
        document.getElementById("send_textbox").disabled =  true;
        document.getElementById("send_button").disabled  =  true;
        document.getElementById("status_icon").setAttribute("status", "disconnected");
        document.getElementById("status_text").innerHTML = "Disconnected";
        _socket = null;
      };
  };
}

function ping() {
  // apparently unavailable
}

function disconnect() {
  _socket.close();
}

function send() {
  var textbox = document.getElementById("send_textbox");
  _socket.send(textbox.value);
  textbox.value = "";
}