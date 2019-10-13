/*
  NFC Scanner for Home Assistant
  
  Libraries:
    - PubSubClient: https://github.com/knolleary/pubsubclient
    - PN532:        https://github.com/Seeed-Studio/PN532

  Release Notes:
  Klaas S. - V1.2 - 10.2019
  - Add OTA update feature

  Klaas S. - V1.1 - 02.2019
  - By means of an LED you can now see if the ESP recognizes an NFC tag.
  - Fix use of 7 bytes nfc tags.
  - Add option to use reading UID's only or not.
  
  Samuel M. - v1.0 - 02.2018
  - Initial script that is made by: https://github.com/mertenats/open-home-automation
*/

typedef struct {
  uint8_t uid[8];
  String  tagName;
  bool    isDiscovered;
  long    lastDiscovery;
  bool    toNotify;
  char    mqttTopic[48];
} NFCTag;

#include "config.h"
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <SPI.h>
#include <PN532_SPI.h>
#include "PN532.h"        // https://github.com/Seeed-Studio/PN532
#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient

#if defined(DEBUG_SERIAL)
#define     DEBUG_PRINT(x)    Serial.print(x)
#define     DEBUG_PRINTLN(x)  Serial.println(x)
#else
#define     DEBUG_PRINT(x)
#define     DEBUG_PRINTLN(x)
#endif

WiFiClient    wifiClient;
PubSubClient  mqttClient(wifiClient);
PN532_SPI     pn532spi(SPI, 4);
PN532         nfc(pn532spi);

int ledPin = 15;
// False wil start de main function, True will start the UID read script so you can find out wich UID's your NFC tags have on the serial monitor.
bool readUIDMode = false;

///////////////////////////////////////////////////////////////////////////
//   Read NFC as sensor for HA 
///////////////////////////////////////////////////////////////////////////
void readNFCTag() {
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength; // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    for (uint8_t i = 0; i < NUM_ACCEPTED_UIDS; i++) {
      if (strcmp((char *)uid, (char *)NFCTags[i].uid) == 0) {
        if (!NFCTags[i].isDiscovered) {
          NFCTags[i].isDiscovered = true;
          NFCTags[i].toNotify = true;
          digitalWrite(ledPin, HIGH);
        }
        NFCTags[i].lastDiscovery = millis();
        break;
      }
    }
  }
  
  for (uint8_t i = 0; i < NUM_ACCEPTED_UIDS; i++) {
    if (NFCTags[i].isDiscovered == true && NFCTags[i].lastDiscovery + MAX_NON_DISCOVERED_PERIOD < millis()) {
      NFCTags[i].isDiscovered = false;
      NFCTags[i].toNotify = true;
      digitalWrite(ledPin, LOW);
    }
  }
}

///////////////////////////////////////////////////////////////////////////
//   Read NFC UID's  
///////////////////////////////////////////////////////////////////////////
void readUID() {
  boolean success;
  uint8_t uidLength;   // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID

  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength);

  if (success) {
    digitalWrite(ledPin, HIGH);
    Serial.println("\n-----------\n");
    Serial.println("Found a card!");
    Serial.print("UID Length: ");
    Serial.print(uidLength, DEC);
    Serial.println(" bytes");
    Serial.print("UID Value: ");
    delay(1000);
    digitalWrite(ledPin, LOW);

    for (uint8_t i=0; i < uidLength; i++) {
      Serial.print(" 0x");
      Serial.print(uid[i], HEX);
    }

    // wait until the card is taken away
    while (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength)) {
      yield(); //let ESPcore handle wifi stuff
    }
    
    Serial.println("\n-----------\n");
  }
  else yield(); // PN532 probably timed out waiting for a card.. let's let the ESPcore handle wifi stuff
}
///////////////////////////////////////////////////////////////////////////
//   WiFi
///////////////////////////////////////////////////////////////////////////
/*
   Function called to handle WiFi events
*/
void handleWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case WIFI_EVENT_STAMODE_GOT_IP:
      DEBUG_PRINTLN(F("INFO: Connection successful to the Wi-Fi AP"));
      DEBUG_PRINT(F("INFO: IP address: "));
      DEBUG_PRINTLN(WiFi.localIP());
      break;
    case WIFI_EVENT_STAMODE_DISCONNECTED:
      DEBUG_PRINTLN(F("ERROR: Connection lost from the Wi-Fi AP"));
      /*
         TODO: Doing something smarter than rebooting the device
      */
      delay(5000);
      ESP.restart();
      break;
    default:
      DEBUG_PRINT(F("INFO: WiFi event: "));
      DEBUG_PRINTLN(event);
      break;
  }
}

/*
   Function called to setup the connection to the WiFi AP
*/
void setupWiFi() {
  DEBUG_PRINT(F("INFO: WiFi connecting to: "));
  DEBUG_PRINTLN(WIFI_SSID);

  delay(10);

  WiFi.mode(WIFI_STA);
  WiFi.onEvent(handleWiFiEvent);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  randomSeed(micros());
}

///////////////////////////////////////////////////////////////////////////
//   Arduino OTA
///////////////////////////////////////////////////////////////////////////
void setupOTA() {  
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(OTA_HOSTNAME);

  // No authentication by default
  ArduinoOTA.setPassword(OTA_PASSWORD);
  
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

///////////////////////////////////////////////////////////////////////////
//   MQTT
///////////////////////////////////////////////////////////////////////////
volatile unsigned long lastMQTTConnection = 0;
char MQTT_CLIENT_ID[7] = {0};
char MQTT_AVAILABILITY_TOPIC[sizeof(MQTT_CLIENT_ID) + sizeof(MQTT_AVAILABILITY_TOPIC_TEMPLATE) - 2] = {0};
/*
  Function called to publish to a MQTT topic with the given payload
*/
void publishToMQTT(char* p_topic, char* p_payload) {
  if (mqttClient.publish(p_topic, p_payload, true)) {
    DEBUG_PRINT(F("INFO: MQTT message published successfully, topic: "));
    DEBUG_PRINT(p_topic);
    DEBUG_PRINT(F(", payload: "));
    DEBUG_PRINTLN(p_payload);
  } else {
    DEBUG_PRINTLN(F("ERROR: MQTT message not published, either connection lost, or message too large. Topic: "));
    DEBUG_PRINT(p_topic);
    DEBUG_PRINT(F(" , payload: "));
    DEBUG_PRINTLN(p_payload);
  }
}
/*
  Function called to connect/reconnect to the MQTT broker
*/
void connectToMQTT() {
  if (!mqttClient.connected()) {
    if (lastMQTTConnection < millis()) {
      if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD, MQTT_AVAILABILITY_TOPIC, 0, 1, MQTT_PAYLOAD_UNAVAILABLE)) {
        DEBUG_PRINTLN(F("INFO: The client is successfully connected to the MQTT broker"));
        publishToMQTT(MQTT_AVAILABILITY_TOPIC, MQTT_PAYLOAD_AVAILABLE);
      } else {
        DEBUG_PRINTLN(F("ERROR: The connection to the MQTT broker failed"));
        DEBUG_PRINT(F("INFO: MQTT username: "));
        DEBUG_PRINTLN(MQTT_USERNAME);
        DEBUG_PRINT(F("INFO: MQTT password: "));
        DEBUG_PRINTLN(MQTT_PASSWORD);
        DEBUG_PRINT(F("INFO: MQTT broker: "));
        DEBUG_PRINTLN(MQTT_SERVER);
      }
      lastMQTTConnection = millis() + MQTT_CONNECTION_TIMEOUT;
    }
  }
}

///////////////////////////////////////////////////////////////////////////
//   SETUP() & LOOP()
///////////////////////////////////////////////////////////////////////////
void setup() {
#if defined(DEBUG_SERIAL)
  Serial.begin(115200);
#endif

  pinMode(ledPin, OUTPUT);
  setupOTA();

  if (readUIDMode == false) {
    setupWiFi();
  }

  nfc.begin();
 
  uint32_t nfcReaderFirmwareVersion = nfc.getFirmwareVersion();
  if (!nfcReaderFirmwareVersion) {
    DEBUG_PRINTLN(F("ERROR: Didn't find PN53x board"));
    while (1); // halt
  }
  
  // configure board to read RFID tags
  nfc.SAMConfig();
  
  sprintf(MQTT_CLIENT_ID, "%06X", ESP.getChipId());
  sprintf(MQTT_AVAILABILITY_TOPIC, MQTT_AVAILABILITY_TOPIC_TEMPLATE, MQTT_CLIENT_ID);

  DEBUG_PRINT(F("INFO: MQTT availability topic: "));
  DEBUG_PRINTLN(MQTT_AVAILABILITY_TOPIC);

  char mqttTopic[sizeof(MQTT_CLIENT_ID) + sizeof(MQTT_SENSOR_TOPIC_TEMPLATE) + sizeof(LOCATION) + 12 - 4] = {0};
  for (uint8_t i = 0; i < NUM_ACCEPTED_UIDS; i++) {
      char tmpTagName[sizeof(NFCTags[i].tagName)] = {0};
      NFCTags[i].tagName.toCharArray(tmpTagName, sizeof(tmpTagName));
      sprintf(mqttTopic, MQTT_SENSOR_TOPIC_TEMPLATE, MQTT_CLIENT_ID, LOCATION, tmpTagName);
      memcpy(NFCTags[i].mqttTopic, mqttTopic, sizeof(mqttTopic) + 1);
      DEBUG_PRINT(F("INFO: MQTT sensor topic: "));
      DEBUG_PRINTLN(NFCTags[i].mqttTopic);
  }
  
  mqttClient.setServer(MQTT_SERVER, MQTT_SERVER_PORT);
  if (readUIDMode == false) {
    connectToMQTT();
  }
}

void loop() {
  ArduinoOTA.handle();

  yield();

  if (readUIDMode == true) {
    readUID();
  } else if (readUIDMode == false) {
    connectToMQTT();
    mqttClient.loop();
    readNFCTag();
  }

  yield();

  for (uint8_t i = 0; i < NUM_ACCEPTED_UIDS; i++) {
    if (NFCTags[i].toNotify) {
      if (NFCTags[i].isDiscovered) {
        publishToMQTT(NFCTags[i].mqttTopic, MQTT_PAYLOAD_ON);
      } else {
        publishToMQTT(NFCTags[i].mqttTopic, MQTT_PAYLOAD_OFF);
      }
      NFCTags[i].toNotify = false;
    }
  }
  
  yield();
}
