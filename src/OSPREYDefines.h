
          /*\   __   __   __   __   __
          shs- |  | |__  |__| |__| |__  \ /
         dM_d: |__|  __| |    |  \ |__   |  0.1
        dL:KM  Configuration-less, plug-and-play dynamic networking over PJON.
       dM56Mh  EXPERIMENTAL, USE AT YOUR OWN RISK
      yM87MM:
       NM*(Mm          /|  Copyright (c) 2014-2020
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

// Master device id
#ifndef OSPREY_MASTER_ID
  #define OSPREY_MASTER_ID              254
#endif

// Maximum slaves handled by master
#ifndef OSPREY_MAX_SLAVES
  #define OSPREY_MAX_SLAVES              25
#endif

// Configuration payload length added to OSPREY_ID_CONFIRM requests by master
#ifndef OSPREY_CONFIGURATION_LENGTH
  #define OSPREY_CONFIGURATION_LENGTH     0
#endif

#define OSPREY_INDEX_FREE                 0
#define OSPREY_INDEX_RESERVED             1
#define OSPREY_INDEX_ASSIGNED             2

// Dynamic addressing
#define OSPREY_ID_REQUEST               200
#define OSPREY_ID_CONFIRM               201
#define OSPREY_ID_NEGATE                203
#define OSPREY_ID_LIST                  204
#define OSPREY_ID_REFRESH               205

// Errors
#define OSPREY_ID_ACQUISITION_FAIL      105
#define OSPREY_DEVICES_BUFFER_FULL      254

// Dynamic addressing port number
#define OSPREY_DYNAMIC_ADDRESSING_PORT    1

// Master ID_REQUEST and ID_NEGATE timeout
#define OSPREY_ADDRESSING_TIMEOUT   4000000
// Master reception time during LIST_ID broadcast (250 milliseconds)
#define OSPREY_LIST_IDS_TIME         250000
// Slave max collision delay when OSPREY_ID_LIST is received (250 milliseconds)
#define OSPREY_COLLISION_DELAY          250
