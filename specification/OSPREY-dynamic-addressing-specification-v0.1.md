## OSPREY dynamic addressing 2.0
```
Invented by Giovanni Blu Mitolo
Originally published: 02/10/2016, latest revision: 24/05/2020
Related implementation: https://github.com/gioblu/OSPREY/
Compliant versions: OSPREY 0.1 and following
Released into the public domain
```
This document defines the dynamic addressing procedure used by a master and its slaves, in a master-slave configuration. All communication related to the addressing procedure must be transmitted on the `OSPREY_DYNAMIC_ADDRESSING_PORT` or port `1`. OSPREY is an Open Standard designed to enable quick, configuration-less, plug-and-play dynamic networking over PJON. The MAC address feature included in PJON v4.0 is used to initially identify slaves and to assign device ids to them. The dynamic addressing procedure can occur in both local or shared mode.

### Master-slave dynamic addressing
```cpp  
 _________________    __________________    
| ID            1 |  | ID             2 |   
| MAC 1.2.1.9.4.6 |  | MAC  2.9.4.3.7.6 |   
|_________________|  |__________________|   __________________
         |                    |            |  MASTER          |
_________|____________________|____________| ID           254 |
         |                    |            | MAC  0.0.0.0.0.0 |
 ________|________    ________|________    |__________________|
| ID            3 |  | ID            4 |    
| MAC 7.2.3.4.0.1 |  | MAC 7.2.3.4.0.1 |    
|_________________|  |_________________|    
```

#### Procedure
All communication to dynamically assign or request ids must be transmitted using CRC32 on the `OSPREY_DYNAMIC_ADDRESSING` port (decimal 1).

Slave sends a `OSPREY_ID_REQUEST` to get a new id:
```cpp  
 ______ ________ ______ ___ ________ ____ _____ __________ ___  ___
|MASTER| HEADER |      |   |  NOT   |PORT| MAC |          |   ||   |
|  ID  |00110110|LENGTH|CRC|ASSIGNED| 1  |     |ID_REQUEST|CRC||ACK|
|______|________|______|___|________|____|_____|__________|___||___|
```
Master transmits back a `OSPREY_ID_REQUEST` containing the new device id reserved for the requester and its configuration:
```cpp  
 ________ ________ ______ ___ ______ ____ _____ __________ __ ______ ___
|  NOT   | HEADER |      |   |MASTER|PORT| MAC |          |  |      |   |
|ASSIGNED|00110010|LENGTH|CRC|  ID  | 1  |     |ID_REQUEST|ID| CONF |CRC|
|________|________|______|___|______|____|_____|__________|__|______|___|
```
Slave confirms the id acquisition sending a `PJON_ID_CONFIRM` request to master containing its configuration:
```cpp  
 ______ ________ ______ ___ __ ____ _____ __________ ______ ___  ___
|MASTER| HEADER |      |   |  |PORT| MAC |          |      |   ||   |
|  ID  |00110110|LENGTH|CRC|ID| 1  |     |ID_CONFIRM| CONF |CRC||ACK|
|______|________|______|___|__|____|_____|__________|______|___||___|
```
If master experiences temporary disconnection or reboot, at start up sends a `OSPREY_ID_LIST` broadcast request:
```cpp  
 _________ ________ ______ ___ _________ ____ _____ _______ ___
|         | HEADER |      |   |         |PORT| MAC |       |   |
|BROADCAST|00110010|LENGTH|CRC|MASTER_ID| 1  |     |ID_LIST|CRC|
|_________|________|______|___|_________|____|_____|_______|___|
```
Each slave answers to a `OSPREY_ID_LIST` broadcast request transmitting a `OSPREY_ID_REFRESH` request to the master containing its configuration:
```cpp  
 ______ ________ ______ ___ __ ____ _____ __________ ______ ___  ___
|MASTER| HEADER |      |   |  |PORT| MAC |          |      |   ||   |
|  ID  |00110110|LENGTH|CRC|ID| 1  |     |ID_REFRESH| CONF |CRC||ACK|
|______|________|______|___|__|____|_____|__________|______|___||___|
```
If the id requested is free in the master's reference, id is approved and the exchange ends. If the id is already in use, master sends a `OSPREY_ID_NEGATE` request forcing the slave to acquire a new id through a `OSPREY_ID_REQUEST`:

Master sends `OSPREY_ID_NEGATE` request to slave:
```cpp  
 _____ ________ ______ ___ _________ ____ _____ _________ ___  ___
|SLAVE| HEADER |      |   |         |PORT| MAC |         |   ||   |
| ID  |00110110|LENGTH|CRC|MASTER_ID| 1  |     |ID_NEGATE|CRC||ACK|
|_____|________|______|___|_________|____|_____|_________|___||___|
```
Slaves must send a `OSPREY_ID_NEGATE` request to the master to free the id before leaving the bus:
```cpp  
 ______ ________ ______ ___ __ ____ _____ _________ ___  ___
|MASTER| HEADER |      |   |  |PORT| MAC |         |   ||   |
|  ID  |00110110|LENGTH|CRC|ID| 1  |     |ID_NEGATE|CRC||ACK|
|______|________|______|___|__|____|_____|_________|___||___|
```
