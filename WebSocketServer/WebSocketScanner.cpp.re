#include "WebSocketScanner.h"

#include "Adafruit_CC3000_Server.h"
#include "string.h"

WebSocketScanner::WebSocketScanner(Adafruit_CC3000_ClientRef& client)
  : _client(client)
  , _cursor(_buffer)
  , _limit(_buffer)
  , _current(_buffer)
  , _marker(0)
  , _state(-1)
{
  loadBuffer();
  // TODO: Think about what happens when client disconnected.
}

int WebSocketScanner::nextToken() {
  _state = -1;
  _current = _cursor;
  int n = scan();
  if (n == NEED_DATA && _client.available()) {
    // Need more data, drop processed tokens and try to load more data
    dropOldTokens();
    return scan();
  }
  return n;
}

int WebSocketScanner::tokenLength() {
  return _cursor - _current;
}

char* WebSocketScanner::tokenData() {
  return _current;
}

bool WebSocketScanner::bufferAvailable() {
  return (_limit - _buffer) < _bufsize;
}

void WebSocketScanner::loadBuffer() {
  while (_client.available() && bufferAvailable()) {
    *_limit++ = _client.read();
  }
}

void WebSocketScanner::dropOldTokens() {
  // Calculate how much data after the last processed token is in the buffer.
  int remaining = _bufsize - (_current - _buffer);
  if (remaining > 0) {
    // Move remaining data to the front of the buffer, dropping old data.
    memmove(_buffer, _current, remaining);
    // Calculate how much data was dropped from the buffer.
    int dropped = _bufsize - remaining;
    // Clear out new open buffer spaces.
    memset(_buffer + remaining, 0, dropped);
    // Shift pointers back by amount of data dropped.
    _limit -= dropped;
    _cursor -= dropped;
    _marker -= dropped;
    // Move the last processed token pointer to the start.
    _current = _buffer;
    // Load new data.
    loadBuffer();
  }
  else {
    // Need more data but the buffer is full processing the current token.
    // No choice but to kill the data in the buffer and pull in more data.
    // This will lose information, so make sure the buffer is set to a size
    // larger than the smallest identifier measureable.
    // TODO: Think about setting a flag when this happens so the caller knows
    // a token is clipped.
    memset(_buffer, 0, _bufsize);
    _cursor = _buffer;
    _limit = _buffer;
    _current = _buffer;
    _marker = _buffer;
    loadBuffer();
  }
}

#define YYGETSTATE()  _state
#define YYSETSTATE(x)   { _state = x; }
#define YYFILL(n)     { return NEED_DATA; }

int WebSocketScanner::scan()
{
/*!re2c
  re2c:define:YYCTYPE  = "char";
  re2c:define:YYCURSOR = _cursor;
  re2c:define:YYLIMIT  = _limit;
  re2c:define:YYMARKER = _marker;
  re2c:variable:yych   = _ch;
  re2c:indent:top      = 1;

  ":"            { return COLON; }
  [^: \t\r\n]+   { return IDENTIFIER; }
  "\r\n"         { return EOL; }
  [ \t\r\n]+     { _current = _cursor; goto yy0; }
*/
}

//  [^\x00-\x1f\x7f()<>@,;:\x5c\x22\x2f\x5b\x5d?={} ]+    { return 0; }
//  "\r\n"                                                { return 1; }
//  [ \t\r\n]+                                            { return 2; }
//  "/"                                                   { return 3; }