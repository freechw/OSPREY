
          /*\   __   __   __   __   __
          shs- |  | |__  |__| |__| |__  \ /
         dM_d: |__|  __| |    |  \ |__   |  0.1
        dL:KM  Configuration-less, plug-and-play dynamic networking over PJON.
       dM56Mh  EXPERIMENTAL, USE AT YOUR OWN RISK
      yM87MM:
       NM*(Mm          /|  Copyright (c) 2014-2019
   ___yM(U*MMo        /j|  Giovanni Blu Mitolo All rights reserved.
 _/OF/sMQWewrMNhfmmNNMN:|  Licensed under the Apache License, Version 2.0
|\_\+sMM":{rMNddmmNNMN:_|  You may obtain a copy of the License at
       yMMMMso         \|  http://www.apache.org/licenses/LICENSE-2.0
       gtMfgm
      mMA@Mf   Thanks to the support, expertise, kindness and talent of the
      MMp';M   following contributors, the documentation, specification and
      ysM1MM:  implementation have been tested, enhanced and verified:
       sMM3Mh  Fred Larsen, Jeff Gueldre
        dM6MN
         dMtd:
          \*/

#pragma once
#include "OSPREYDefines.h"

typedef void (* OSPREY_found_slave)(
  PJON_Endpoint endpoint,
  const uint8_t *configuration,
  uint16_t length
);

static void OSPREY_dummy_found_slave(
  PJON_Endpoint,
  const uint8_t *,
  uint16_t
) {};

/* Reference to device */
struct Device_reference {
  uint8_t  mac[6] = {0, 0, 0, 0, 0, 0};
  uint8_t  state  = 0;
  uint32_t registration = 0;
};

template<typename Strategy>
class OSPREYMaster : public PJON<Strategy> {
  public:
    Device_reference ids[OSPREY_MAX_SLAVES];
    uint8_t configuration[OSPREY_CONFIGURATION_LENGTH];
    uint8_t required_config =
      PJON_TX_INFO_BIT | PJON_CRC_BIT | PJON_ACK_REQ_BIT |
      PJON_PORT_BIT | PJON_MAC_BIT;

    /* OSPREYMaster bus default initialization:
       State: Local (bus_id: 0.0.0.0)
       Acknowledge: true (Acknowledge is requested)
       device id: MASTER (254)
       Mode: PJON_HALF_DUPLEX
       Sender info: true (Sender info are included in the packet) */

    OSPREYMaster() : PJON<Strategy>(OSPREY_MASTER_ID) {
      set_default();
    };

    /* OSPREYMaster initialization passing bus and device id:
       uint8_t my_bus = {1, 1, 1, 1};
       OSPREYMaster master(my_bys); */

    OSPREYMaster(const uint8_t *bus) : PJON<Strategy>(bus, OSPREY_MASTER_ID) {
      set_default();
    };

    /* Add a device reference: */

    bool add_id(uint8_t id, const uint8_t *mac) {
      if(
        PJONTools::id_equality(ids[id - 1].mac, mac, 6) ||
        (ids[id - 1].state == OSPREY_INDEX_FREE)
      ) {
        PJONTools::copy_id(ids[id - 1].mac, mac, 6);
        ids[id - 1].state = OSPREY_INDEX_ASSIGNED;
        return true;
      }
      return false;
    };

    /* Master begin function: */

    void begin() {
      PJON<Strategy>::begin();
      delete_id_reference();
      _list_time = PJON_MICROS();
      uint8_t request = OSPREY_ID_LIST;
      _list_id = PJON<Strategy>::send_repeatedly(
        PJON_BROADCAST,
        this->tx.bus_id,
        &request,
        1,
        OSPREY_LIST_IDS_TIME,
        PJON<Strategy>::config | required_config,
        0,
        OSPREY_DYNAMIC_ADDRESSING_PORT
      );
    };

    /* Confirm device ID insertion in list: */

    bool confirm_id(uint8_t id, const uint8_t *mac) {
      if(
        PJONTools::id_equality(ids[id - 1].mac, mac, 6) &&
        (ids[id - 1].state == OSPREY_INDEX_RESERVED)
      ) {
        add_id(id, mac);
        return true;
      }
      return false;
    };

    /* Count active slaves in buffer: */

    uint8_t count_slaves() {
      uint8_t result = 0;
      for(uint8_t i = 0; i < OSPREY_MAX_SLAVES; i++)
        if(ids[i].state == OSPREY_INDEX_ASSIGNED) result++;
      return result;
    };

    /* Empty a single element or the whole buffer: */

    void delete_id_reference(uint8_t id = 0) {
      if(!id) {
        for(uint8_t i = 0; i < OSPREY_MAX_SLAVES; i++) {
          PJONTools::copy_id(ids[i].mac, PJONTools::no_mac(), 6);
          ids[i].state = false;
          ids[i].registration = 0;
        }
      } else if(id > 0 && id < OSPREY_MAX_SLAVES) {
        PJONTools::copy_id(ids[id - 1].mac, PJONTools::no_mac(), 6);
        ids[id - 1].state = false;
        ids[id - 1].registration = 0;
      }
    };

    /* Master error handler: */

    void error(uint8_t code, uint16_t data) {
      uint8_t id = PJON<Strategy>::packets[data].content[0];
      _master_error(code, data, _custom_pointer);
      if(
        (code == PJON_CONNECTION_LOST) &&
        (id != PJON_BROADCAST) &&
        (id != PJON_NOT_ASSIGNED)
      ) delete_id_reference(id);
    };

    static void static_error_handler(uint8_t code, uint16_t data, void *cp) {
      ((OSPREYMaster<Strategy>*)cp)->error(code, data);
    };

    /* Filter addressing packets from receive callback: */

    void filter(
      uint8_t *payload,
      uint16_t length,
      const PJON_Packet_Info &packet_info
    ) {
      PJON_Packet_Info p_i;
      memcpy(&p_i, &packet_info, sizeof(PJON_Packet_Info));
      p_i.custom_pointer = _custom_pointer;
      handle_addressing(packet_info, length);
      _master_receiver(payload, length, p_i);
    };

    /* Remove reserved id which expired (Remove never confirmed ids): */

    void free_reserved_ids_expired() {
      for(uint8_t i = 0; i < OSPREY_MAX_SLAVES; i++)
        if(ids[i].state == OSPREY_INDEX_RESERVED) {
          if(
            (uint32_t)(PJON_MICROS() - ids[i].registration) <
            OSPREY_ADDRESSING_TIMEOUT
          ) continue;
          else delete_id_reference(i + 1);
        }
    };

    /* Handle addressing procedure if related: */

    bool handle_addressing(PJON_Packet_Info info, uint16_t length) {
      bool filter = false;
      uint8_t overhead = PJON<Strategy>::packet_overhead(this->data[1]);
      uint8_t CRC_overhead = (this->data[1] & PJON_CRC_BIT) ? 4 : 1;
      uint8_t offset = overhead - CRC_overhead;

      if(
        (info.header & PJON_PORT_BIT) &&
        (info.header & PJON_TX_INFO_BIT) &&
        (info.header & PJON_CRC_BIT) &&
        (info.header & PJON_MAC_BIT) &&
        (info.port == OSPREY_DYNAMIC_ADDRESSING_PORT)
      ) {
        filter = true;
        uint8_t request = this->data[offset];

        if(request == OSPREY_ID_REQUEST) reserve_id(info.tx.mac);

        if(request == OSPREY_ID_CONFIRM) {
          if(!confirm_id(info.tx.id, info.tx.mac))
            negate_id(info.tx.id, info.tx.mac);
          else _found_slave(info.tx, this->data + offset + 1, length - 1);
        }

        if(request == OSPREY_ID_REFRESH) {
          if(!add_id(info.tx.id, info.tx.mac))
            negate_id(info.tx.id, info.tx.mac);
          else _found_slave(info.tx, this->data + offset + 1, length - 1);
        }

        if(request == OSPREY_ID_NEGATE)
          if(PJONTools::id_equality(info.tx.mac, ids[info.tx.id - 1].mac, 6))
            delete_id_reference(info.tx.id);
      }
      return filter;
    };

    /* Get device index in buffer from MAC: */

    uint8_t get_index_from_mac(const uint8_t *mac) {
      for(uint8_t i = 0; i < OSPREY_MAX_SLAVES; i++)
        if(PJONTools::id_equality(mac, ids[i].mac, 6)) return i;
      return PJON_NOT_ASSIGNED;
    };

    /* Negates a device id: */

    void negate_id(uint8_t id, const uint8_t *mac) {
      uint8_t response[1] = {OSPREY_ID_NEGATE};
      PJON_Packet_Info info;
      info.rx.id = id;
      PJONTools::copy_id(info.rx.bus_id, this->tx.bus_id, 4);
      info.header = PJON<Strategy>::config | required_config;
      info.port = OSPREY_DYNAMIC_ADDRESSING_PORT;
      PJONTools::copy_id(info.rx.mac, mac, 6);
      PJON<Strategy>::send(info, response, 1);
    };

    /* Reserve a device id and wait for its confirmation: */

    uint16_t reserve_index(const uint8_t *mac) {
      uint8_t in = get_index_from_mac(mac);
      for(
        uint8_t i = (in != PJON_NOT_ASSIGNED) ? in : 0;
        i < OSPREY_MAX_SLAVES;
        i++
      ) if((ids[i].state == OSPREY_INDEX_FREE) || (in == i)) {
          PJONTools::copy_id(ids[i].mac, mac, 6);
          ids[i].state = OSPREY_INDEX_RESERVED;
          ids[i].registration = PJON_MICROS();
          return i + 1;
        }
      error(OSPREY_DEVICES_BUFFER_FULL, OSPREY_MAX_SLAVES);
      return OSPREY_DEVICES_BUFFER_FULL;
    };

    /* Reserves a device id and transmits back a OSPREY_ID_REQUEST containing
       the device id to the requester:
    OSPREY_ID_REQUEST - DEVICE ID (the new reserved) */

    void reserve_id(const uint8_t *mac) {
      uint8_t response[2 + OSPREY_CONFIGURATION_LENGTH];
      uint16_t state = reserve_index(mac);
      if(state == OSPREY_DEVICES_BUFFER_FULL) return;
      if(state == PJON_FAIL)
        return negate_id(PJON_NOT_ASSIGNED, mac);
      response[0] = OSPREY_ID_REQUEST;
      response[1] = (uint8_t)(state);
      for(uint8_t i = 0; i < OSPREY_CONFIGURATION_LENGTH; i++)
        response[2 + i] = configuration[i];

      PJON_Packet_Info info;
      info.rx.id = PJON_NOT_ASSIGNED;
      PJONTools::copy_id(info.rx.bus_id, this->tx.bus_id, 4);
      PJONTools::copy_id(info.rx.mac, mac, 6);
      info.port = OSPREY_DYNAMIC_ADDRESSING_PORT;
      info.header = PJON<Strategy>::config | required_config;
      PJON<Strategy>::send(info, response, 2 + OSPREY_CONFIGURATION_LENGTH);
    };

    /* Master receive function: */

    uint16_t receive() {
      return PJON<Strategy>::receive();
    };

    /* Try to receive a packet repeatedly with a maximum duration: */

    uint16_t receive(uint32_t duration) {
      uint32_t time = PJON_MICROS();
      while((uint32_t)(PJON_MICROS() - time) <= duration)
        if(receive() == PJON_ACK) return PJON_ACK;
      return PJON_FAIL;
    };

    /* Static receiver hander: */

    static void static_receiver_handler(
      uint8_t *payload,
      uint16_t length,
      const PJON_Packet_Info &packet_info
    ) {
      (
        (OSPREYMaster<Strategy>*)packet_info.custom_pointer
      )->filter(payload, length, packet_info);
    };

    /* Set custom pointer: */

    void set_custom_pointer(void *p) {
      _custom_pointer = p;
    };

    /* Set default configuration: */

    void set_default() {
      PJON<Strategy>::set_default();
      PJON<Strategy>::set_custom_pointer(this);
      PJON<Strategy>::set_error(static_error_handler);
      PJON<Strategy>::set_receiver(static_receiver_handler);
      set_found_slave(OSPREY_dummy_found_slave);
      delete_id_reference();
    };

    /* Master receiver function setter: */

    void set_receiver(PJON_Receiver r) {
      _master_receiver = r;
    };

    /* Master error receiver function: */

    void set_error(PJON_Error e) {
      _master_error = e;
    };

    /* Set a function to be called each time a new slave is found */

    void set_found_slave(OSPREY_found_slave f) {
      _found_slave = f;
    };

    /* Master packet handling update: */

    uint8_t update() {
      if(
        (_list_id != PJON_MAX_PACKETS) &&
        (OSPREY_ADDRESSING_TIMEOUT < (uint32_t)(PJON_MICROS() - _list_time))
      ) {
        PJON<Strategy>::remove(_list_id);
        _list_id = PJON_MAX_PACKETS;
      }
      free_reserved_ids_expired();
      return PJON<Strategy>::update();
    };

  private:
    OSPREY_found_slave _found_slave;
    uint16_t           _list_id = PJON_MAX_PACKETS;
    uint32_t           _list_time;
    void              *_custom_pointer;
    PJON_Receiver      _master_receiver;
    PJON_Error         _master_error;
};
