
/* Additional configuration payload that can be included by the slave */

#define OSPREY_CONFIGURATION_LENGTH 5

// Include only SoftwareBitBang
#define PJON_INCLUDE_SWBB

// OSPREY requires the PJON's MAC and PORT optional features to operate
#define PJON_INCLUDE_MAC
#define PJON_INCLUDE_PORT

#include <EEPROM.h>
#include <PJON.h>
#include <OSPREYSlave.h>


// PJON object
OSPREYSlave<SoftwareBitBang> slave;

// Example of a configuration payload buffer
uint8_t config[OSPREY_CONFIGURATION_LENGTH] = {'H','I', '!', '!', '!'};

/* A string is used to signal if the slave
   was previously initialized */
char initializer[] = "SLAVE";
uint8_t eeprom_mac[6] = {0, 0, 0, 0, 0, 0};

// Bus id used for communication
uint8_t bus_id[4] = {0, 0, 0, 1};

// State of the device id aquisition
bool acquired = false;

// Check if the device was previously initialized
bool is_initialized() {
  for(uint8_t i = 0; i < 5; i++)
    if(initializer[i] != EEPROM.read(i)) return false;
  return true;
}

void write_default_configuration() {
  for(uint8_t i = 0; i < 5; i++)
    EEPROM.update(i, initializer[i]);
  // Clean memory where to store RID
  EEPROM.update(5, 0);
  EEPROM.update(6, 0);
  EEPROM.update(7, 0);
  EEPROM.update(8, 0);
};

void connected_handler(const uint8_t *configuration, uint16_t length) {
  Serial.println("Slave connected to master!");
  Serial.print("Configuration included: ");
  for(uint8_t i = 0; i < length; i++)
    Serial.print((char)configuration[i]);
  Serial.println();
  Serial.print("Acquired device id: ");
  Serial.println(slave.device_id());
  Serial.flush();
  EEPROM.put(5, slave.tx.mac);
};

void receiver_handler(uint8_t *payload, uint16_t length, const PJON_Packet_Info &packet_info) {
  /* If the packet contains the OSPREY_DYNAMIC_ADDRESSING_PORT port it is
     part of the dynamic addressing procedure */
  if(packet_info.port == OSPREY_DYNAMIC_ADDRESSING_PORT) {
    Serial.print("Addressing request: ");
    Serial.print(payload[0]);
    Serial.print(" Length: ");
    Serial.print(length);
  } else { // All other packets
    Serial.print("Received: ");
    for(uint16_t i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
      Serial.print(" ");
    }
  }
  Serial.println();
  Serial.flush();
};

void error_handler(uint8_t code, uint16_t data, void *custom_pointer) {
  if(code == PJON_CONNECTION_LOST) {
    Serial.print("Connection lost with device ");
    Serial.println((uint8_t)slave.packets[data].content[0], DEC);
  }
  if(code == OSPREY_ID_ACQUISITION_FAIL) {
    if(data == OSPREY_ID_CONFIRM)
      Serial.println("OSPREYSlave error: master-slave id confirmation failed.");
    if(data == OSPREY_ID_NEGATE)
      Serial.println("OSPREYSlave error: master-slave id release failed.");
    if(data == OSPREY_ID_REQUEST)
      Serial.println("OSPREYSlave error: master-slave id request failed.");
  }
  Serial.flush();
};

void setup() {
  if(!is_initialized()) write_default_configuration();
  else memcpy(slave.tx.mac, EEPROM.get(5, eeprom_mac), 6);
  Serial.begin(115200);
  slave.set_error(error_handler);
  slave.set_receiver(receiver_handler);
  slave.set_connected(connected_handler);

  // Configure the network to operate in shared mode and use bus id 0.0.0.1
  slave.set_shared_network(true);
  memcpy(slave.tx.bus_id, bus_id, 4);
  
  /* Write configuration that will be sent by the slave to the master
     when requesting a device id */
  memcpy(slave.configuration, config, OSPREY_CONFIGURATION_LENGTH);

  slave.strategy.set_pin(12);
  slave.begin();
  slave.request_id();
}

void loop() {
  slave.update();
  slave.receive(5000);
};
