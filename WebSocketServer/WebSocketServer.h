#ifndef WEBSOCKETSERVER_H
#define WEBSOCKETSERVER_H

// TODO: Seems to get into loop if I send a char that is ignored (like { ) then
// try to send more good data.

// TODO: Make a websocket message that's dynamic on the heap.

// Bug: When connected but browser/page not focused it sometimes resets the CC3000.  Also noticed when socket is
// forcefully disconnected.

// Other thoughts:
// - Make a simple read only REST API server that exposes all the digital and analog
// - pins for reading.  Also writing?

#include <Adafruit_CC3000_Server.h>
#include <sha1.h>

#include "WebSocketScanner.h"

class WebSocketServer {
public:
  WebSocketServer(char* serverPath, Adafruit_CC3000_ClientRef& client)
    : _client(client)
    , _scanner(client)
    , _state(&WebSocketServer::parseRequestLine)
    , _serverPath(serverPath)
    , _index(0)
    , _upgradeHeader(-1)
    , _connectionHeader(-1)
    , _webSocVersionHeader(-1)
    , _webSocKeyHeader(-1)
    , _connected(false)
  { }
  
  void update();
  bool connected();
  Adafruit_CC3000_ClientRef& client();

  int receive(uint8_t* buffer, size_t bufferSize);

private:
  typedef void (WebSocketServer::* StateFunction)();
  StateFunction _state = NULL;
  Adafruit_CC3000_ClientRef _client;
  WebSocketScanner _scanner;
  char* _serverPath;
  int _index;
  int _upgradeHeader;
  int _connectionHeader;
  int _webSocVersionHeader;
  int _webSocKeyHeader;
  bool _connected;
  char _keyResponse[HASH_LENGTH];

  bool expectTokens(int* tokens, char* identifiers[], int numTokens);

  void parseRequestLine();
  void parseHeaderLine();

  bool tokenCompare(const PROGMEM char* identifier);

  // Load the contents of a received websocket frame into buffer.  Handle unmasking the data if necessary.
  // Note that this expects the first byte of the frame to already be processed.
  // Can be called with a NULL buffer value to ignore the data.
  int receiveFrame(uint8_t* buffer, size_t bufferSize);

  int handleClose();
  int handlePing();
};


// TODO: This class can only send up to 127 byte messages.  Consider support for more.
// TODO: Making the size a template is nice in theory, but will blow up the amount of code
// generated in practice (each separate size is a separate class instance).  Make a dynamic
// memory version instead of this templatized version.
template <int size>
class WebSocketMessage {

public:
  WebSocketMessage() {
    memset(_data, 0, size+2);
  }

  int write(Adafruit_CC3000_ClientRef& client) {
    if (buildFrameHeader(true, (uint8_t)0x2, false) < 0) {
      return -1;
    }
    client.write((void*)_data, size+2);
    //  TODO: Check for errors in previous send calls.
    return size;
  }

  uint8_t& operator[] (size_t n) {
    return _data[2+n];
  }

private:
  // Reserve 2 bytes at the front for the data header.
  uint8_t _data[size+2];

  int buildFrameHeader(bool fin, uint8_t opcode, bool mask) {
    // Validate parameters
    if (opcode > 15) {
      // Opcode must be between 0 and 15
      return -1;
    }
    // if (payload_len > 125) {
    //   // Payload length must be less than 126
    //   return -1;
    // }
    // Build first byte with fin, reserved bits, and opcode
    uint8_t byte = (fin << 7);
    byte = byte | (opcode & 0x0F);
    _data[0] = byte;
    // Build second byte with mask bit and size
    byte = (mask << 7);
    byte = byte | (size & 0x7F);
    _data[1] = byte;
    // Return success
    return 0;
  }

};

#endif
