
// Include only SoftwareBitBang
#define PJON_INCLUDE_SWBB

// OSPREY requires the PJON's MAC and PORT optional features to operate
#define PJON_INCLUDE_MAC
#define PJON_INCLUDE_PORT

/* Additional configuration payload that can be included in
   the OSPREY_ID_CONFIRM response sent by the master when accepting a new
   slave in the group */

#define OSPREY_CONFIGURATION_LENGTH 5

// Include libraries
#include <PJON.h>
#include <OSPREYMaster.h>

// Example of a configuration payload buffer
uint8_t config[5] = {'C','I', 'A', 'O', '!'};

// OSPREY object, the master device id is fixed to OSPREY_MASTER_ID or 254
OSPREYMaster<SoftwareBitBang> master;

uint32_t t_millis;

void setup() {
  Serial.begin(115200);
  master.strategy.set_pin(12);
  master.set_receiver(receiver_function);
  master.set_found_slave(found_slave_handler);
  master.set_error(error_handler);
  master.begin();

  /* Send a continuous greetings every second
     to showcase the receiver function functionality */
  master.send_repeatedly(PJON_BROADCAST, "Master says hi!", 15, 2500000);

  /* Write configuration that will be sent by master when a new slave
     is accepted in the group */
  memcpy(master.configuration, config, OSPREY_CONFIGURATION_LENGTH);

  t_millis = millis();
};

void error_handler(uint8_t code, uint16_t data, void *custom_pointer) {
  if(code == PJON_CONNECTION_LOST) {
    Serial.print("PJON error: connection lost with device id ");
    Serial.println((uint8_t)master.packets[data].content[0], DEC);
  }
  if(code == OSPREY_DEVICES_BUFFER_FULL) {
    Serial.print("OSPREYMaster error: master slaves buffer is full with a length of ");
    Serial.println(data);
  }
};

void receiver_function(uint8_t *payload, uint16_t length, const PJON_Packet_Info &packet_info) {
  /* Make use of the payload before sending something, the buffer where payload points to is
     overwritten when a new message is dispatched */

  // OSPREY addressing packets
  if(packet_info.port == OSPREY_DYNAMIC_ADDRESSING_PORT) {
    Serial.print("Addressing request: ");
    Serial.print(payload[0]);
    Serial.print(" Sender's mac: ");
    for(uint8_t i = 0; i < 6; i++) {
      Serial.print(packet_info.tx.mac[i]);
      Serial.print(" ");
    }
  }

  // General packet data
  Serial.print(" Header: ");
  Serial.print(packet_info.header, BIN);
  Serial.print(" Length: ");
  Serial.print(length);
  Serial.print(" Sender id: ");
  Serial.print(packet_info.tx.id);

  // Packet content
  Serial.print(" Packet: ");
  for(uint8_t i = 0; i < length; i++) {
    Serial.print(payload[i]);
    Serial.print(" ");
  }
  Serial.print("Packets in buffer: ");
  Serial.print(master.update());
  Serial.print(" Devices in buffer: ");
  Serial.println(master.count_slaves());
};


void found_slave_handler(PJON_Endpoint info, const uint8_t *slave_config, uint16_t length) {
  Serial.print("OSPREYMaster found new slave with device id: ");
  Serial.print(info.id);
  Serial.print(" MAC address: ");
  for(uint8_t i = 0; i < 6; i++) {
    Serial.print(info.mac[i]);
    Serial.print(" ");
  }
  Serial.println();
  Serial.print("Slave configuration: ");
  for(uint8_t i = 0; i < length; i++) {
    Serial.print((char)slave_config[i]);
    Serial.print(" ");
  }
  Serial.println();
}

void loop() {
  if(millis() - t_millis > 5000) {
    Serial.println("Updated list of known slaves: ");
    for(uint8_t i = 0; i < OSPREY_MAX_SLAVES; i++) {
      if(master.ids[i].state != OSPREY_INDEX_ASSIGNED) continue;
      Serial.print("Device id: ");
      Serial.print(i + 1);
      Serial.print(" mac: ");
      for(uint8_t m = 0; m < 6; m++) {
        Serial.print(master.ids[i].mac[m]);
        Serial.print(" ");
      }
    }
    Serial.println();
    t_millis = millis();
  }
  master.receive(1000);
  master.update();
};
