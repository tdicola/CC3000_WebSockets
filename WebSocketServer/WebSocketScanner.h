#ifndef WEBSOCKETSCANNER_H
#define WEBSOCKETSCANNER_H

#include "Adafruit_CC3000_Server.h"

#define NEED_DATA -1
#define IDENTIFIER 0
#define EOL 1
#define COLON 2

class WebSocketScanner {
public:
  WebSocketScanner(Adafruit_CC3000_ClientRef& client);
  // Process a token and return the type.  Returns -1 if more data is necessary
  // to finish processing the token.
  int nextToken();
  // Get the length of the last found token.
  int tokenLength();
  // Get a pointer to the start of the last found token.
  char* tokenData();

private:
  // Client instance to pull new data from.
  Adafruit_CC3000_ClientRef _client;
  // Buffer for the currently processed tokens.
  static const int _bufsize = 64;
  char _buffer[_bufsize];
  // Current points to the start of the last parsed token.
  char* _current;
  // Cursor points to the end of the last parsed token / start of in
  // process token.
  char* _cursor;
  // Limit points one location past the last used part of the buffer.
  char* _limit;
  // Marker is used internally by re2c for backtracking.
  char* _marker;
  // Marker, state, and ch are internal state used by the re2c scanning function.
  int _state;
  char _ch;

  // Return true if data can be added to the buffer.
  bool bufferAvailable();
  // Pull in as much available data as possible to the buffer.
  void loadBuffer();
  // Drop tokens that have been parsed and refill the buffer.
  void dropOldTokens();
  // Scanning function implemented by re2c.
  int scan();

};

#endif