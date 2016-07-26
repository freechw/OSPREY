
            /*/\      __   __   __   __   __
            shs-    |  | |__  |__| |__| |__  \ /
           dM_d:    |__|  __| |    |  \ |__   |  0.1
          dL:KM     Arduino compatible open-source mesh network framework
         dM56Mh     based on the PJON standard. Giovanni Blu Mitolo 2016
        yM87MM:     gioscarab@gmail.com
        dgfi3h
        mMfdas-
         NM*(Mm          /|  Copyright (c) 2014-2016,
     ___yM(U*MMo        /j|  Giovanni Blu Mitolo All rights reserved.
   _/OF/sMQWewrMNhfmmNNMN:|  Licensed under the Apache License, Version 2.0 (the "License");
  |\_\+sMM":{rMNddmmNNMN:_|  you may not use this file except in compliance with the License.
         yMMMMso         \|  You may obtain a copy of the License at
         gtMfgm              http://www.apache.org/licenses/LICENSE-2.0
        mMA@Mf
        MMp';M      Unless required by applicable law or agreed to in writing, software
        ysM1MM:     distributed under the License is distributed on an "AS IS" BASIS,
         sMM3Mh     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied
          dM6MN     See the License for the specific language governing permissions and
           dMtd:    limitations under the License.
            shs
            \/*/

#include "OSPREY.h"

OSPREY::OSPREY() {
  set_routing_handler(dummy_handler);

  for(int b = 0; b < MAX_BUSES; b++)
    for(int r = 0; r < MAX_KNOWN_DEVICES; r++)
      buses[b].known_devices[r].active = false;

  for(uint8_t p = 0; p < MAX_PACKAGE_REFERENCES; p++)
    package_references[p].package_id = 0;

  // TODO - Set receiver
  // TODO - Set error
};


/* Add a bus to the OSPREY bus list. The bus will be automatically handled by
   OSPREY: */

uint8_t OSPREY::add_bus(PJON<> *link, uint8_t *bus_id, boolean router) {
  for(uint8_t b = 0; b < MAX_BUSES; b++)
    if(!buses[b].active) {
      buses[b].active = true;
      buses[b].link = link;
      buses[b].link->set_packet_auto_deletion(false);
      buses[b].link->set_router(router);
      // TODO - Manage auto-addressing strategy
    }
};


/* Create a reference between the dispatched PJON packet on a certain bus to a OSPREY package id: */

uint16_t OSPREY::add_package_reference(uint8_t bus_id[4], uint16_t package_id, uint8_t packet_index) {
  for(uint8_t p = 0; p < MAX_PACKAGE_REFERENCES; p++)
    if(!package_references[p].package_id) {
      for(uint8_t b = 0; b < 4; b++)
        package_references[p].bus_id[b] = bus_id[b];

      package_references[p].package_id = package_id;
      package_references[p].packet_index = packet_index;
      return p;
    }
  // TODO - detect memory leak error
};


/* Compute bus id equality: */

boolean OSPREY::bus_id_equality(uint8_t *id_one, uint8_t *id_two) {
  for(uint8_t i = 0; i < 4; i++)
    if(id_one[i] != id_two[i])
      return false;
  return true;
};


/* Count the active buses in the OSPREY bus list: */

uint8_t OSPREY::count_active_buses() {
  uint8_t result = 0;
  for(uint8_t b = 0; b < MAX_BUSES; b++)
    if(buses[b].active)
      result++;
  return result;
};


/* Generate a auto-increment package id: */

uint16_t OSPREY::generate_package_id() {
  if (_package_id_source + 1 > _package_id_source)
    return _package_id_source++;
  return 1;
};


/* Handle PJON packet state change: */

void OSPREY::handle_packet(uint8_t bus_id[4], uint8_t packet_index, uint8_t state) {
  if(state == DISPATCHED /* && bus(bus_id).packets[packet_index].content[9] == ACK */) // So if ack
    remove_package_reference(bus_id, packet_index);
  // TODO - Hanle sending failure
};


/* Receive from all the added PJON buses for a predefined duration: */

void OSPREY::receive(uint32_t duration) {
  uint32_t time_per_bus = duration / count_active_buses();

  for(uint8_t b = 0; b < MAX_BUSES; b++) {
    uint32_t time = micros();
    if(buses[b].active)
      while((uint32_t)(time + time_per_bus) >= micros())
        buses[b].link->receive();
  }
};


void OSPREY::received(uint8_t *payload, uint8_t length, const PacketInfo *packet_info) {
  if(!(packet_info.header & OSPREY_BIT)) return; // return if non-OSPREY package

  uint16_t package_id = payload[12] << 8 | payload[13] & 0xFF;
  uint8_t recipient_bus = find_bus(packet_info.recipient_bus_id, packet_info.recipient_device_id);

  if(recipient_bus != FAIL) {
    if(type == ACK) return remove_package_reference(packet_info.sender_bus_id, packet_info.sender_device_id);
    _receiver(payload, length, packet_info); // Call OSPREY receiver
    return send(
      recipient_bus,
      packet_info.recipient_bus_id,
      packet_info.recipient_device_id,
      packet_info.sender_bus_id,
      packet_info.sender_device_id,
      ACK,
      0,
      package_id,
      ACK,
      1
    );
  }

  if(packet_info.header & ROUTE_REQUEST_BIT) {
    // Route package
  }
};


uint8_t OSPREY::find_bus(uint8_t *bus_id, uint8_t device_id) {
  for(uint8_t b = 0; b < MAX_BUSES; b++)
    if(buses[b].active && bus_id_equality(bus_id, buses[b].link->bus_id) && device_id == buses[b].link->device_id())
      return b;
  return FAIL;
};


void OSPREY::remove_package_reference(uint8_t bus_id[4], uint16_t package_id, bool direct) {
  for(uint8_t p = 0; p < MAX_PACKAGE_REFERENCES; p++)
    if(bus_id_equality(package_references[p].bus_id, bus_id) && package_references[p].package_id == package_id) {
      package_references[p].bus_id[0] = 0;
      package_references[p].bus_id[1] = 0;
      package_references[p].bus_id[2] = 0;
      package_references[p].bus_id[3] = 0;
      package_references[p].packet_index = 0;
      package_references[p].package_id = 0;
      package_references[p].direct = direct;
      for(uint8_t b = 0; b < MAX_BUSES; b++)
        if(bus_id_equality(package_references[p].bus_id, buses[b].link->bus_id))
          buses[b].link->remove(package_references[p].packet_index);
    }
};


/*  PJON packet used to transmit an encapsulated OSPREY package:

                   |  HEADER  | RECEIVER INFO | SENDER INFO |
   _______ ________|__________|_______________|_____________|_________ _______
  |       |        |          |               |         |   |         |       |
  |  id   | length | 10000111 |    0.0.0.2    | 0.0.0.2 | 1 | content |  CRC  |
  |_______|________|__________|_______________|_________|___|_________|_______|
                                                              |
                                                              |
  PJON bus used as a link for an OSPREY network               |
  Here an example of OSPREY package:                          |
     _________________________________________________________|
   _|_________________________________
  | PACKAGE INFO            | CONTENT |
  |_________________________|_________|
  | type | hops | packet id |         |
  |______|______|___________|_________|
  |  __  |  __  |  __  __   |   __    |
  | |  | | |  | | |  ||  |  |  |  |   |
  | |__| | |__| | |__||__|  |  |__|   |
  |  102 |   1  |   0   1   |   64    |
  |______|______|___________|_________|________________________________________
  |                                                                            |
  |  Package example:                                                          |
  |  Device 1 in bus 0.0.0.2 is sending a REQUEST (value 102) to device 1      |
  |  (present in the lower level PJON's packet) in bus 0.0.0.1 the first       |
  |  package since started (package id 1) containing the content "@" or        |
  |  decimal 64.                                                               |
  |____________________________________________________________________________| */


uint16_t OSPREY::dispatch(
  uint8_t  header,
  uint8_t  bus_index,
  uint8_t  *sender_bus_id,
  uint8_t  sender_device_id,
  uint8_t  *recipient_bus_id,
  uint8_t  recipient_device_id,
  uint8_t  type,
  uint8_t  hops,
  uint16_t package_id,
  char     *content,
  uint8_t  length
) {
  hops += 1;
  if(hops >= MAX_HOPS) return // error HOPS_LIMIT;

  uint16_t reference;
  char *payload = (char *) malloc(length + 13);
  if(payload == NULL) return /* error MEMORY_FULL */;

  recipient_bus_id = (recipient_device_id ? recipient_bus_id : buses[bus_index].link->bus_id;
  recipient_device_id = recipient_device_id ? recipient_device_id : buses[bus_index].link->device_id();

  memcpy(sender_bus_id, bus_id, 4);
  payload[4] = sender_device_id;
  memcpy(payload + 5, recipient_bus_id, 4);
  payload[9] = recipient_device_id;
  payload[10] = type;
  payload[11] = hops;
  payload[12] = package_id  >> 8;
  payload[13] = package_id & 0xFF;
  memcpy(payload + 14, content, length);

  reference = add_package_reference(
    bus_id,
    package_id,
    buses[bus_index].link->dispatch(device_id, bus_id, payload, length + 13, header);
  );

  free(payload);

  return reference;
};


uint16_t OSPREY::send(
  uint8_t *recipient_bus_id,
  uint8_t recipient_device_id,
  uint8_t *sender_bus_id,
  uint8_t sender_device_id,
  uint8_t type,
  char *content,
  uint8_t length
) {
  int16_t package_id = generate_package_id();
  // 1 First network id lookup with direct bus connections
  for(uint8_t b = 0; b < MAX_BUSES; b++)
    if(buses[b].active && bus_id_equality(buses[b].link->bus_id, recipient_bus_id) || type == INFO) {
      // First level connection detected
      // Send OSPREY Package as PJON packet to the directly connected PJON bus
      dispatch(DEFAULT_HEADER, b, recipient_bus_id, recipient_device_id, type, 0, package_id, content, length);
      if(type != INFO) return;
    }
  if(type == INFO) return;

  // 2 Network id lookup in every router's connected bus list
  for(uint8_t b = 0; b < MAX_BUSES; b++)
    if(buses[b].active)
      for(uint8_t d = 0; d < MAX_KNOWN_DEVICES; d++)
        if(buses[b].known_devices[d].active)
          for(uint8_t k = 0; k < MAX_KNOWN_BUSES; k++)
            if(bus_id_equality(buses[b].known_devices[d].known_bus_ids[k], recipient_bus_id) || type == INFO) {
              return dispatch(
                _default_header & ROUTE_REQUEST_BIT,
                b,
                recipient_bus_id,
                recipient_device_id,
                type,
                0,
                package_id,
                content,
                length,
                true
              );
              // Second level connection detected
              // Send OSPREY Package as PJON packet to the router connected to the target PJON bus

  return BUS_UNREACHABLE;
};


void OSPREY::set_routing_handler(routing_handler h) {
  _routing_handler = h;
};


void OSPREY::update() {
  for(uint8_t b = 0; b < MAX_BUSES; b++)
    if(buses[b].active)
      buses[b].link->update();
};
