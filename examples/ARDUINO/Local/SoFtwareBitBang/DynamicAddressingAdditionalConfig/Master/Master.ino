
/* This let the master receive a packet while trying to send another.
   Avoids network instability */
#define PJON_RECEIVE_WHILE_SENDING_BLOCKING true

/* Additional configuration payload that can be included in
   the OSPREY_ID_CONFIRM response sent by the master when accepting a new
   slave in the group */
#define OSPREY_CONFIGURATION_LENGTH 5
/* Example of a configuration payload buffer */
uint8_t config[5] = {'C','I', 'A', 'O', '!'};

#include <PJON.h>
#include <OSPREYMaster.h>

// PJON object - The Master device id is fixed to PJON_MASTER_ID or 254
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
    uint32_t rid =
      (uint32_t)(payload[1]) << 24 |
      (uint32_t)(payload[2]) << 16 |
      (uint32_t)(payload[3]) <<  8 |
      (uint32_t)(payload[4]);
    Serial.print("Addressing request: ");
    Serial.print(payload[0]);
    Serial.print(" RID: ");
    Serial.print(rid);
  }

  // General packet data
  Serial.print(" Header: ");
  Serial.print(packet_info.header, BIN);
  Serial.print(" Length: ");
  Serial.print(length);
  Serial.print(" Sender id: ");
  Serial.print(packet_info.sender_id);

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


void found_slave_handler(uint32_t rid) {
  /* Write configuration that will be sent by master when a new slave
     is accepted in the group */
  for(uint8_t i = 0; i < OSPREY_CONFIGURATION_LENGTH; i++)
    master.configuration[i] = config[i];
  Serial.print("OSPREYMaster found new slave with rid: ");
  Serial.println(rid);
  Serial.println("Updated list of known slaves: ");
  for(uint8_t i = 0; i < OSPREY_MAX_SLAVES; i++) {
    if(!master.ids[i].state) continue;
    Serial.print("Device id: ");
    Serial.print(i + 1);
    Serial.print(" RID: ");
    Serial.println(master.ids[i].rid);
  }
  Serial.println();
}

void loop() {
  master.receive(5000);
  master.update();
};
