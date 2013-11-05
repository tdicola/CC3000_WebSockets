/***************************************************
  Web Socket Server
 ****************************************************/
#include <SPI.h>
#include <sha1.h>
#include <Adafruit_CC3000.h>
#include <CC3000_MDNS.h>
#include <WebSocketServer.h>
#include "utility/debug.h"

// These are the interrupt and control pins
#define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
// These can be any two pins
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10
// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIV2); // you can change this clock speed

#define WLAN_SSID       "tdicola"           // cannot be longer than 32 characters!
#define WLAN_PASS       "101tdicola"
// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

#define LISTEN_PORT           80    // What TCP port to listen on for connections.

#define DIGITAL_INPUT         7
#define ANALOG_INPUT          3
#define LED_OUTPUT            8
#define UPDATE_FREQUENCY_MS   500

Adafruit_CC3000_Server server(LISTEN_PORT);
MDNSResponder mdns;

WebSocketServer* socserver = NULL;
boolean clientConnected = false;


int digitalPinState;
int analogPinValue;
int lastStateCheck;

void setup(void)
{ 
  pinMode(DIGITAL_INPUT, INPUT);
  digitalPinState = digitalRead(DIGITAL_INPUT);
  analogPinValue = analogRead(ANALOG_INPUT);
  pinMode(LED_OUTPUT, OUTPUT);
  digitalWrite(LED_OUTPUT, LOW);
  
  Serial.begin(115200);
  Serial.println(F("Hello, CC3000!\n")); 
  
  Serial.print("Free RAM: "); Serial.println(getFreeRam(), DEC);

  // Set up CC3000 and get connected to the wireless network.
  Serial.println(F("\nInitializing..."));
  if (!cc3000.begin())
  {
    Serial.println(F("Couldn't begin()! Check your wiring?"));
    while(1);
  }
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Failed!"));
    while(1);
  }
  while (!cc3000.checkDHCP())
  {
    delay(100);
  }
  Serial.println();
  
  // Start multicast DNS responder
  if (!mdns.begin("cc3kserver", cc3000)) {
    Serial.println(F("Error setting up MDNS responder!"));
    while(1); 
  }
   
  // Start server
  server.begin();
  
  Serial.println(F("Listening for connections..."));
}

void sendDigitalStatus(uint8_t pin, Adafruit_CC3000_ClientRef& client) {
  WebSocketMessage<4> message;
  message[0] = 0x02;  // Opcode, digital read response
  message[1] = 0x02;  // Payload length, 2 bytes
  message[2] = pin;   // Pin
  message[3] = digitalRead(pin) == HIGH ? 0x01 : 0x00; // Pin status, 1 = HIGH, 0 = LOW
  message.write(client); 
}

void sendAnalogValue(uint8_t pin, Adafruit_CC3000_ClientRef& client) {
  WebSocketMessage<5> message;
  message[0] = 0x04;  // Opcode, analog read response
  message[1] = 0x03;  // Payload length, 3 bytes
  message[2] = pin;   // Pin
  int v = analogRead(pin);
  message[3] = (v & 0xFF00) >> 8;  // Most significant byte of analog value
  message[4] = (v & 0x00FF);       // Least significant byte of analog value
  message.write(client);   
}

void loop(void)
{
  // Handle any multicast DNS requests
  mdns.update();
  
  // Handle any connected clients
  if (!clientConnected) {
     Adafruit_CC3000_ClientRef client = server.available();
     if (client) {
       if (socserver != NULL) {
         delete socserver; 
       }
       socserver = new WebSocketServer("/arduino", client);
       clientConnected = true;
       lastStateCheck = millis();
     }
  }
  else {  
    if (socserver->client().connected()) {
      // Check if web socket server is connected to the client.
      if (socserver->connected()) {        
        // Handle any received messages
        uint8_t rxbuffer[32];
        int n = socserver->receive(rxbuffer, 32);
        if (n > 1) {
          // Print received bytes
          //for (int i = 0; i < n; ++i) {
          //  Serial.print("Received: ");
          //  Serial.println(rxbuffer[i], HEX);
          //}
          // Digital read
          if (rxbuffer[0] == 0x01 && rxbuffer[1] == 1 && n == 3) {
             Serial.print(F("Digital read pin "));
             Serial.println(rxbuffer[2], DEC);
             if (rxbuffer[2] == DIGITAL_INPUT) {
               sendDigitalStatus(DIGITAL_INPUT, socserver->client());
             }
          }
          // Analog read
          if (rxbuffer[0] == 0x03 && rxbuffer[1] == 1 && n == 3) {
             Serial.print(F("Analog read pin "));
             Serial.println(rxbuffer[2], DEC);
             if (rxbuffer[2] == ANALOG_INPUT) {
               sendAnalogValue(ANALOG_INPUT, socserver->client());
             }
          }          
          // Digital write
          if (rxbuffer[0] == 0x05 && rxbuffer[1] == 2 && n == 4) {
             Serial.print(F("Digital write to pin "));
             Serial.print(rxbuffer[2], DEC);
             Serial.print(F(" value "));
             Serial.println(rxbuffer[3], DEC);
             if (rxbuffer[2] == LED_OUTPUT) {
               digitalWrite(rxbuffer[2], rxbuffer[3] == 1 ? HIGH : LOW);
             }
          }
        }
        
        // Check for changes in state and send them to the client.
        int time = millis();
        if ((time - lastStateCheck) > UPDATE_FREQUENCY_MS) {
          lastStateCheck = time;
          n = digitalRead(DIGITAL_INPUT);
          if (n != digitalPinState) {
            digitalPinState = n;
            //Serial.print(F("Digital pin changed to state "));
            //Serial.println(digitalPinState);
            sendDigitalStatus(DIGITAL_INPUT, socserver->client());
          }
          n = analogRead(ANALOG_INPUT);
          if (n != analogPinValue) {
            analogPinValue = n;
            //Serial.print(F("Analog pin changed to value "));
            //Serial.println(analogPinValue);
            sendAnalogValue(ANALOG_INPUT, socserver->client());
          }
        }
      }
      else {
        // Web socket server is in the process of handshaking, keep calling update.
        // TODO: Add some kind of timeout for the handshake?
        socserver->update();
      }
    }
    else {
      //Serial.println("Client disconnected");
      clientConnected = false;
    }
  }
}


