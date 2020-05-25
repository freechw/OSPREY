
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

typedef void (* OSPREY_Connected)(const uint8_t *configuration, uint16_t length);
static void OSPREY_dummy_connected(const uint8_t *, uint16_t) {};

template<typename Strategy>
class OSPREYSlave : public PJON<Strategy> {
  public:
    bool connected = false;
    uint8_t configuration[OSPREY_CONFIGURATION_LENGTH];
    uint8_t required_config =
      PJON_ACK_REQ_BIT | PJON_TX_INFO_BIT | PJON_CRC_BIT |
      PJON_PORT_BIT | PJON_MAC_BIT
    ;

    /* OSPREYSlave bus default initialization:
       State: Local (bus_id: 0.0.0.0)
       Acknowledge: active
       Device id: PJON_NOT_ASSIGNED (255)
       Mode: PJON_HALF_DUPLEX
       Sender info: included

       OSPREYSlave initialization with no parameters
       OSPREYSlave<SoftwareBitBang> slave; */

    OSPREYSlave() : PJON<Strategy>() {
      generate_mac();
      set_default();
    };

    /* OSPREYSlave bus default initialization:
       State: Local (bus_id: 0.0.0.0)
       Acknowledge: active
       Device id: PJON_NOT_ASSIGNED (255)
       Mode: PJON_HALF_DUPLEX
       Sender info: included

       OSPREYSlave initialization passing a custom mac:
       uint8_t mac[6] = {1, 2, 3, 4, 5, 6};
       OSPREYSlave<SoftwareBitBang> bus(mac); */

    OSPREYSlave(const uint8_t *mac) : PJON<Strategy>(mac) {
      set_default();
    };

    /* Acquire id in master-slave configuration: */

    bool request_id() {
      connected = false;
      uint8_t response[1] = {OSPREY_ID_REQUEST};
      if(
        this->send_packet_blocking(
          OSPREY_MASTER_ID,
          this->tx.bus_id,
          response,
          1,
          this->config | required_config,
          0,
          OSPREY_DYNAMIC_ADDRESSING_PORT
        ) == PJON_ACK
      ) return true;
      error(OSPREY_ID_ACQUISITION_FAIL, OSPREY_ID_REQUEST);
      return false;
    };

    /* Begin function to be called in setup: */

    void begin() {
      PJON<Strategy>::begin();
      if(PJONTools::id_equality(this->tx.mac, PJONTools::no_mac(), 6))
        generate_mac();
    };

    /* Release device id (Master-slave only): */

    bool discard_device_id() {
      uint8_t request[1] = {OSPREY_ID_NEGATE};
      if(this->send_packet_blocking(
        OSPREY_MASTER_ID,
        this->bus_id,
        request,
        1,
        this->config | required_config,
        0,
        OSPREY_DYNAMIC_ADDRESSING_PORT
      ) == PJON_ACK) {
        this->_device_id = PJON_NOT_ASSIGNED;
        return true;
      }
      error(OSPREY_ID_ACQUISITION_FAIL, OSPREY_ID_NEGATE);
      return false;
    };

    /* Error callback: */

    void error(uint8_t code, uint16_t data) {
      _slave_error(code, data, _custom_pointer);
    };

    /* Filter incoming addressing packets callback: */

    void filter(
      uint8_t *payload,
      uint16_t length,
      const PJON_Packet_Info &packet_info
    ) {
      PJON_Packet_Info p_i;
      memcpy(&p_i, &packet_info, sizeof(PJON_Packet_Info));
      p_i.custom_pointer = _custom_pointer;
      handle_addressing(packet_info, length);
      _slave_receiver(payload, length, p_i);
    };

    /* Generate a new device rid: */

    void generate_mac() {
      for(uint8_t i = 0; i < 6; i++)
        this->tx.mac[i] =
          (uint8_t)(PJON_ANALOG_READ(this->random_seed)) ^
          (uint8_t)(PJON_MICROS()) ^
          (uint8_t)(PJON_RANDOM(256))
        ;
    };

    /* Handle dynamic addressing requests and responses: */

    void handle_addressing(PJON_Packet_Info info, uint16_t length) {
      if( // Handle master-slave dynamic addressing
        (info.header & PJON_PORT_BIT) &&
        (info.header & PJON_TX_INFO_BIT) &&
        (info.header & PJON_CRC_BIT) &&
        (info.header & PJON_MAC_BIT) &&
        (info.port == OSPREY_DYNAMIC_ADDRESSING_PORT) &&
        (info.tx.id == OSPREY_MASTER_ID)
      ) {
        uint8_t overhead =
          this->packet_overhead(info.header);
        uint8_t CRC_overhead =
          (info.header & PJON_CRC_BIT) ? 4 : 1;
        uint8_t offset = overhead - CRC_overhead;
        char response[1 + OSPREY_CONFIGURATION_LENGTH];

        if(!connected && (this->data[offset] == OSPREY_ID_REQUEST)) {
          this->set_id(this->data[offset + 1]);
          response[0] = OSPREY_ID_CONFIRM;
          memcpy(response + 1, configuration, OSPREY_CONFIGURATION_LENGTH);
          _connected(this->data + offset + 2, length - 2);
          if(this->send_packet_blocking(
            OSPREY_MASTER_ID,
            this->tx.bus_id,
            response,
            1 + OSPREY_CONFIGURATION_LENGTH,
            this->config | required_config,
            0,
            OSPREY_DYNAMIC_ADDRESSING_PORT
          ) != PJON_ACK) {
            this->set_id(PJON_NOT_ASSIGNED);
            connected = false;
            error(OSPREY_ID_ACQUISITION_FAIL, OSPREY_ID_CONFIRM);
          } else connected = true;
        }

        if(connected && (this->data[offset] == OSPREY_ID_NEGATE)) {
          this->set_id(PJON_NOT_ASSIGNED);
          connected = false;
        }

        if(this->data[offset] == OSPREY_ID_LIST) {
          if(connected) {
            if(
              (uint32_t)(PJON_MICROS() - _last_request_time) >
              (OSPREY_ADDRESSING_TIMEOUT * 2)
            ) {
              PJON_DELAY(PJON_RANDOM(OSPREY_COLLISION_DELAY));
              _last_request_time = PJON_MICROS();
              response[0] = OSPREY_ID_REFRESH;
              memcpy(response + 1, configuration, OSPREY_CONFIGURATION_LENGTH);
              this->send_packet_blocking(
                OSPREY_MASTER_ID,
                this->tx.bus_id,
                response,
                1 + OSPREY_CONFIGURATION_LENGTH,
                this->config | required_config,
                0,
                OSPREY_DYNAMIC_ADDRESSING_PORT
              );
            }
          } else if(
            (uint32_t)(PJON_MICROS() - _last_request_time) >
            (OSPREY_ADDRESSING_TIMEOUT * 2)
          ) {
            _last_request_time = PJON_MICROS();
            request_id();
          }
        }
      }
    };

    /* Slave receive function: */

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

    /* Set custom pointer: */

    void set_custom_pointer(void *p) {
      _custom_pointer = p;
    }

    /* Set default configuration: */

    void set_default() {
      PJON<Strategy>::set_default();
      PJON<Strategy>::set_custom_pointer(this);
      PJON<Strategy>::set_receiver(static_receiver_handler);
      PJON<Strategy>::set_error(static_error_handler);
      set_connected(OSPREY_dummy_connected);
    };

    /* Slave receiver function setter: */

    void set_receiver(PJON_Receiver r) {
      _slave_receiver = r;
    };

    /* Slave error receiver function: */

    void set_error(PJON_Error e) {
      _slave_error = e;
    };

    /* Set function called when a slave connects to a master: */

    void set_connected(OSPREY_Connected c) {
      _connected = c;
    };

    /* Static receiver hander: */

    static void static_receiver_handler(
      uint8_t *payload,
      uint16_t length,
      const PJON_Packet_Info &packet_info
    ) {
      (
        (OSPREYSlave<Strategy>*)packet_info.custom_pointer
      )->filter(payload, length, packet_info);
    };

    /* Static error hander: */

    static void static_error_handler(
      uint8_t code,
      uint16_t data,
      void *custom_pointer
    ) {
      ((OSPREYSlave<Strategy>*)custom_pointer)->error(code, data);
    };

    /* Slave packet handling update: */

    uint8_t update() {
      return PJON<Strategy>::update();
    };

  private:
    OSPREY_Connected   _connected;
    void               *_custom_pointer;
    uint32_t           _last_request_time;
    uint32_t           _rid = 0;
    PJON_Error         _slave_error;
    PJON_Receiver      _slave_receiver;
};
