#include "WebSocketServer.h"

void WebSocketServer::update() {
  if (_state != NULL) {
    (this->*_state)();
  }
}

bool WebSocketServer::connected() {
  return _connected;
}

Adafruit_CC3000_ClientRef& WebSocketServer::client() {
  return _client;
}

// Receive up to bufferSize bytes from the websocket connection and write them to the
// provided buffer.  Return the number of bytes written to the buffer. If no data is
// available to read, return 0.  If an error occured, return -1.
// TODO: Add a timeout since this is blocking.
int WebSocketServer::receive(uint8_t* buffer, size_t bufferSize) {
  if (!_client.available()) {
    return 0;
  }
  uint8_t byte = _client.read();
  // Check if final bit is set.
  if ((byte >> 7) != 1) {
    Serial.println(F("Not final!"));
    return -1;
  }
  uint8_t opcode = byte & 0x0F;
  if (opcode == 0x08) {
    // Handle close
    return handleClose();
  }
  else if (opcode == 0x09) {
    // Handle ping
    return handlePing();
  }
  else if (opcode == 0x0A) {
    // Handle pong
    Serial.println(F("PONG!"));
    return -1;
  }
  else if (opcode == 0x02) {
    // Handle binary data
    return receiveFrame(buffer, bufferSize);
  }
  else if (opcode == 0x01) {
    // Handle text data
    Serial.println(F("Text!"));
    return -1;
  }
  else {
    Serial.println(F("Unknown!"));
    return -1;
  }
}

bool WebSocketServer::expectTokens(int* tokens, char* identifiers[], int numTokens) {
  int n = _scanner.nextToken();
  if (n != -1) {
    // Check if the scanned token matches the expected token.
    if (tokens[_index] != n) {
      _index = 0;  // TODO: Should bomb out with an error here.
      return false;
    }
    // Check if an identifier matches the expected value.
    if (n == 0) {
      if (strlen(identifiers[_index]) != _scanner.tokenLength() || 
          memcmp(identifiers[_index], _scanner.tokenData(), _scanner.tokenLength()) != 0) {
        _index = 0;  // TODO: Should bomb out with an error here.
        return false;
      }
    }
    // Increment expected state and check if all have succeeded.
    _index++;
    if (_index >= numTokens) {
      return true;
    }
  }
  return false;
}

void WebSocketServer::parseRequestLine() {
  static int requestTokens[] { IDENTIFIER, IDENTIFIER, IDENTIFIER, EOL };
  static char* requestIdentifiers[] = {
    "GET",
    NULL,
    "HTTP/1.1"
  };
  requestIdentifiers[1] = _serverPath;
  if (expectTokens(requestTokens, requestIdentifiers, 4)) {
    _index = 0;
    _state = &WebSocketServer::parseHeaderLine;
  }
}

void WebSocketServer::parseHeaderLine() {
  // Parse an identifier, colon, *, newline
  int n = _scanner.nextToken();
  // Peek at the token being processed and check if it's the end of headers.
  if (n == NEED_DATA && tokenCompare(PSTR("\r\n\r\n"))) {
    // Check if successfully parsed all the expected headers.
    if (_upgradeHeader == 1 && _connectionHeader == 1 && _webSocKeyHeader == 1 && _webSocVersionHeader == 1) {
      _client.fastrprint(F("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: "));
      // Convert key hash to base64
      // Adapted from Adafruit code for SendTweet example.
      uint8_t *in, out, i;
      char b64[29];
      static const char PROGMEM b64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
      for(in = (uint8_t*)_keyResponse, out=0; ; in += 3) { // octets to sextets
        b64[out++] =   in[0] >> 2;
        b64[out++] = ((in[0] & 0x03) << 4) | (in[1] >> 4);
        if(out >= 26) break;
        b64[out++] = ((in[1] & 0x0f) << 2) | (in[2] >> 6);
        b64[out++] =   in[2] & 0x3f;
      }
      b64[out] = (in[1] & 0x0f) << 2;
      // Remap sextets to base64 ASCII chars
      for(i=0; i<=out; i++) b64[i] = pgm_read_byte(&b64chars[(unsigned char)b64[i]]);
      b64[i++] = '=';
      b64[i++] = 0;
      _client.fastrprint(b64);
      _client.fastrprint(F("\r\n\r\n"));
      _connected = true;
      _state = NULL;
    }
  }
  // Parse a header
  if (n != NEED_DATA) {
    // Serial.print(_index);
    // Serial.print(F(" "));
    // Serial.print(n);
    // Serial.print(F(" "));
    // for (int i = 0; i < _scanner.tokenLength(); ++i) {
    //   Serial.print(_scanner.tokenData()[i]);
    // }
    // Serial.print(F(" "));
    // Serial.print(_scanner.tokenLength());
    // Serial.println(F(""));
    if ((_index == 0 && n == IDENTIFIER) || 
        (_index == 1 && n == COLON) ||
        (_index == 2 && n == IDENTIFIER) ||
        (_index >= 3))
    {
      // Start parsing appropriate headers based on their name.
      if (_index == 0) {
        if      (tokenCompare(PSTR("Upgrade")))               _upgradeHeader        = 0;
        else if (tokenCompare(PSTR("Connection")))            _connectionHeader     = 0;
        else if (tokenCompare(PSTR("Sec-WebSocket-Key")))     _webSocKeyHeader      = 0;
        else if (tokenCompare(PSTR("Sec-WebSocket-Version"))) _webSocVersionHeader  = 0;
      }
      // Parse header values
      if (_index == 2) {
        if (_upgradeHeader == 0) {
          if (tokenCompare(PSTR("websocket"))) _upgradeHeader = 1;
          else _upgradeHeader = -1;
        }
        if (_connectionHeader == 0) {
          if (tokenCompare(PSTR("Upgrade"))) _connectionHeader = 1;
          else _connectionHeader = -1;
        }
        if (_webSocKeyHeader == 0) {
          Sha1.init();
          //Sha1.print(value);
          for (int i = 0; i < _scanner.tokenLength(); ++i) {
            Sha1.print(_scanner.tokenData()[i]);
          }
          Sha1.print(F("258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
          memcpy(_keyResponse, Sha1.result(), HASH_LENGTH);
          _webSocKeyHeader = 1;
        }
        if (_webSocVersionHeader == 0) {
          if (tokenCompare(PSTR("13"))) _webSocVersionHeader = 1;
          else _webSocVersionHeader = -1;
        }
      }
      // Finish parsing header once end of line is received.
      _index++;
      if (n == EOL) {
        _index = 0;
      }
    }
    else {
      // TODO: Return error
      _state = NULL;
      return;
    }
  }
}

bool WebSocketServer::tokenCompare(const PROGMEM char* identifier) {
  return (_scanner.tokenLength() == strlen_P(identifier) && 
          memcmp_P(_scanner.tokenData(), identifier, strlen_P((const char*)identifier)) == 0);
}

int WebSocketServer::receiveFrame(uint8_t* buffer, size_t bufferSize) {
  // Check if next byte specifies masked, and get payload size.
  uint8_t byte = _client.read();
  bool masked = (byte >> 7) == 1;
  if (!masked) {
    //Serial.println(F("Not masked!"));
    return -1;
  }
  uint8_t size = byte & 0x7F;
  // TODO: Handle if size = 127 and extended payload is present.
  // TODO: Handle unmasked?
  // Read mask bytes
  uint8_t mask[4];
  for (int i = 0; i < 4; ++i) {
    mask[i] = _client.read();
  }
  // Read payload bytes
  if (size > bufferSize) {
    //Serial.println(F("Buffer too small!"));
    return -1;
  }
  for (int i = 0; i < size; ++i) {
    // Unmask data by taking xor of read byte with the mask byte at
    // payload position modulo 4.
    uint8_t ch = _client.read();
    if (buffer != NULL) {
      buffer[i] = ch ^ mask[i % 4];
    }
  }
  return size;
}

int WebSocketServer::handleClose() {
  // Swallow the rest of the body
  receiveFrame(NULL, 0);
  // Send a close response
  uint8_t resp[2];
  resp[0] = 0x88; // Fin bit = 1, opcode = 8 (close)
  resp[1] = 0x00; // Masked = 0, size = 0
  _client.write((void*)resp, 2);
  // Close the connection
  _client.close();
  _connected = false;
  return -1;
}

int WebSocketServer::handlePing() {
  Serial.println(F("Ping!"));
  // Receive the ping frame payload
  // TODO: Is 64 bytes enough?
  uint8_t pingbuffer[66];
  int n = receiveFrame(pingbuffer+2, 64);
  if (n < 0) {
    Serial.println(F("PING ERROR"));
    return -1;
  }
  // Send a pong response
  pingbuffer[0] = 0x8A;       // Fin bit = 1, opcode = A (pong)
  pingbuffer[1] = (uint8_t)n; // Masked = 0, size = ping data size
  _client.write(pingbuffer, 2+n);
  return -1;
}
