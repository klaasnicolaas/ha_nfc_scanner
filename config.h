///////////////////////////////////////////////////////////////////////////
//  CONFIGURATION
///////////////////////////////////////////////////////////////////////////
#define NUM_ACCEPTED_UIDS 2 //<-----------------CHANGE THIS IF ADDING CARDS!!
NFCTag NFCTags[NUM_ACCEPTED_UIDS] = {
  // It is neccessary to account for UID length since a 7 byte ID could contain a 4 byte ID, which would cause a misfire
 //  #0    #1    #2    #3    #4    #5    #6   #Tagname
  {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, "Tag1", false, 0, false, {0}},
  {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, "Tag2", false, 0, false, {0}},
};

// send OFF payload after x ms
#define MAX_NON_DISCOVERED_PERIOD 500

// Location of the BLE scanner
#define LOCATION "living_room"

// Debug output
#define DEBUG_SERIAL

// Wi-Fi credentials
#define WIFI_SSID     ""
#define WIFI_PASSWORD ""

// MQTT
#define MQTT_USERNAME     ""
#define MQTT_PASSWORD     ""
#define MQTT_SERVER       ""
#define MQTT_SERVER_PORT  1883

#define MQTT_CONNECTION_TIMEOUT 5000 // [ms]

// MQTT availability: available/unavailable
#define MQTT_AVAILABILITY_TOPIC_TEMPLATE  "%s/availability"
// MQTT binary sensor: <CHIP_ID>/sensor/<LOCATION>/<BLE_ADDRESS>
#define MQTT_SENSOR_TOPIC_TEMPLATE        "%s/sensor/%s/%s/state"

#define MQTT_PAYLOAD_ON   "ON"
#define MQTT_PAYLOAD_OFF  "OFF"

#define MQTT_PAYLOAD_AVAILABLE    "online"
#define MQTT_PAYLOAD_UNAVAILABLE  "offline"

