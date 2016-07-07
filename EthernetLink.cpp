#include <Arduino.h>
#include <EthernetLink.h>

// DONE:
// 1. Bidirectional communication with single-socket connections, to utilize the max number of sockets better,
//    and to make firewall traversal easier than with two-ways socket connections.
// 2. Support ENC28J60 based Ethernet shields. Cheap and compact.

// TODO:
// 1. Focus on single_socket without keep_connection from many sites to a master site, with the master
//    site using OSPREY to forward packets between the networks! Should be possible out of the box.
// 2. Make it possible to accept multiple incoming connections also with keep_connection.
//    This would make it possible for one Link to accept connections from multiple links without having created a
//    dedicated Link for each, just like without keep_connection, but with the speed gain.
//    This requires modifying or skipping the EthernetServer/Client classes because only one socket per
//    listening port is possible with these.
// 3. Checksum. Is it needed, as this is also handled on the network layer?
// 4. Retransmission if not ACKED. Or leave this to caller, as the result is immediately available?
// 5. FIFO queue class common with PJON? Or skip built-in queue?
// 6. Call error callback at appropriate times with appropriate codes. Only FAIL and TIMEOUT relevant?
// 7. Encryption. Add extra optional encryption key parameter to add_node, plus dedicated function for server.

// Magic number to verify that we are aligned with telegram start and end
#define HEADER 0x18ABC427ul
#define FOOTER 0x9ABE8873ul
#define SINGLESOCKET_HEADER 0x4E92AC90ul
#define SINGLESOCKET_FOOTER 0x7BB1E3F4ul

// The UIPEthernet library used for the ENC28J60 based Ethernet shields has the correct return value from
// the read call, while the standard Ethernet library does not follow the standard!
#ifdef UIPETHERNET_H
#define NOTHINGREAD 0
#define ERRORREAD -1
#else
#define NOTHINGREAD -1
#define ERRORREAD 0
#endif


//#define DEBUGPRINT


void EthernetLink::init() {
  _server = NULL;
  _keep_connection = false;
  _single_socket = false;
  _local_id = 0;
  _current_device = -1;
  _remote_node_count = 0;
  memset(_local_ip, 0, 4);
  _local_port = DEFAULT_PORT;
  _receiver = NULL;
  _error = NULL;

  for(uint8_t i = 0; i < MAX_REMOTE_NODES; i++) {
    _remote_id[i] = 0;
    memset(_remote_ip[i], 0, 4);
    _remote_port[i] = 0;
  }
};


int16_t EthernetLink::read_bytes(EthernetClient &client, byte *contents, uint16_t length) {
  uint16_t total_bytes_read = 0, bytes_read;
  uint32_t start_ms = millis();
  int16_t avail;
  // NOTE: The recv/read functions return -1 if no data waiting, and 0 if socket closed!
  do {
    while((avail = client.available()) <= 0 && client.connected() && millis() - start_ms < 10000) ;
    bytes_read = client.read(&contents[total_bytes_read], max(0, min(avail, length - total_bytes_read)));
    if(bytes_read > 0) total_bytes_read += bytes_read;
  } while(bytes_read != ERRORREAD && total_bytes_read < length && millis() - start_ms < 10000);
  if(bytes_read == ERRORREAD) stop(client); // Lost connection
  return total_bytes_read;
};


// Do bidirectional transfer of packets over a single socket connection by using a master-slave mode
// where the master connects and delivers packets or a placeholder, then reads packets or placeholder back
// before closing the connection (unless letting it stay open).
uint16_t EthernetLink::single_socket_transfer(EthernetClient &client, int16_t id, bool master, char *contents, uint16_t length) {
#ifndef NO_SINGLE_SOCKET
  #ifdef DEBUGPRINT
//    Serial.print("Single-socket transfer, id="); Serial.print(id);
//    Serial.print(", master="); Serial.println(master);
  #endif
  if(master) { // Creating outgoing connections
    // Connect or check that we are already connected to the correct server
    bool connected = id == -1 ? _client_out.connected() : connect(id);
    #ifdef DEBUGPRINT
      Serial.println(connected ? "Out conn" : "No out conn");
    #endif
    if(!connected) return FAIL;

    // Send singlesocket header and number of outgoing packets
    bool ok = true;
    uint32_t head = SINGLESOCKET_HEADER;
    uint8_t numpackets_out = length > 0 ? 1 : 0;
    char buf[5];
    memcpy(buf, &head, 4);
    memcpy(&buf[4], &numpackets_out, 1);
    if(ok) ok = client.write((byte*) &buf, 5) == 5;
    if(ok) client.flush();

    // Send the packet and read ACK
    if(ok && numpackets_out > 0) {
       ok = send(client, id, contents, length) == ACK;
       #ifdef DEBUGPRINT
         Serial.print("Sent p, ok="); Serial.println(ok);
       #endif
    }

    // Read number of incoming messages
    uint8_t numpackets_in = 0;
    if(ok) ok = read_bytes(client, &numpackets_in, 1) == 1;
    #ifdef DEBUGPRINT
      Serial.print("Read np_in: "); Serial.println(numpackets_in);
    #endif

    // Read incoming packages if any
    for(uint8_t i = 0; ok && i < numpackets_in; i++) {
      while(client.available() < 1 && client.connected());
      ok = receive(client) == ACK;
      #ifdef DEBUGPRINT
        Serial.print("Read p, ok="); Serial.println(ok);
      #endif
    }

    // Write singlesocket footer ("ACK" for the whole thing)
    uint32_t foot = SINGLESOCKET_FOOTER;
    if(ok) ok = client.write((byte*) &foot, 4) == 4;
    if(ok) client.flush();
    #ifdef DEBUGPRINT
      Serial.print("Sent ss foot, ok="); Serial.println(ok);
    #endif

    // Disconnect
    int16_t result = ok ? ACK : FAIL;
    disconnect_out_if_needed(result);
    return result;
  } else { // Receiving incoming connections and packets and request
    // Wait for and accept connection
    bool connected = accept();
    #ifdef DEBUGPRINT
//      Serial.println(connected ? "In conn" : "No in conn");
    #endif
    if(!connected) return FAIL;

    // Read singlesocket header
    bool ok = read_until_header(client, SINGLESOCKET_HEADER);
    #ifdef DEBUGPRINT
      Serial.print("Read ss head, ok="); Serial.println(ok);
    #endif

    // Read number of incoming packets
    uint8_t numpackets_in = 0;
    if(ok) ok = read_bytes(client, (byte*) &numpackets_in, 1) == 1;
    #ifdef DEBUGPRINT
      Serial.print("Read np_in: "); Serial.println(numpackets_in);
    #endif

    // Read incoming packets if any, send ACK for each
    for(uint8_t i = 0; ok && i < numpackets_in; i++) {
      while(client.available() < 1 && client.connected());
      ok = receive(client) == ACK;
      #ifdef DEBUGPRINT
        Serial.print("Read p, ok="); Serial.println(ok);
      #endif
    }

    // Write number of outgoing packets
    uint8_t numpackets_out = length > 0 ? 1 : 0;
    if(ok) ok = client.write((byte*) &numpackets_out, 1) == 1;
    if(ok) client.flush();

    // Write outgoing packets if any
    if(ok && numpackets_out > 0) {
      ok = send(client, id, contents, length) == ACK;
      #ifdef DEBUGPRINT
         Serial.print("Sent p, ok="); Serial.println(ok);
      #endif
    }

    // Read singlesocket footer
    if(ok) {
      uint32_t foot = 0;
      ok = read_bytes(client, (byte*) &foot, 4) == 4;
      if(foot != SINGLESOCKET_FOOTER) ok = 0;
      #ifdef DEBUGPRINT
        Serial.print("Read ss foot, ok="); Serial.println(ok);
      #endif
    }

    // Disconnect
    disconnect_in_if_needed();

    return ok ? ACK : FAIL;
  }
#endif
  return FAIL;
};


bool EthernetLink::accept() {
  // Accept new incoming connection if connection has been lost
  bool connected = _client_in.connected();
  if(!_keep_connection || !connected) {
    stop(_client_in);
    _client_in = _server->available();
    connected = _client_in;
    #ifdef DEBUGPRINT
      if(connected) Serial.println("Accepted");
    #endif
  }
  return connected;
};


// Connect to a server if needed, then read incoming package and send ACK
uint16_t EthernetLink::receive() {
  if(_server == NULL) { // Not listening for incoming connections
    if(_single_socket) { // Single-socket mode.
      // Only read from already established outgoing socket, or create connection if there is only one
      // remote node configured (no doubt about which node to connect to).
      int16_t remote_id = _remote_node_count == 1 ? _remote_id[0] : -1;
      return single_socket_transfer(_client_out, remote_id, true, NULL, 0);
    }
  } else {
    // Accept new incoming connection if connection has been lost
    if(_single_socket) return single_socket_transfer(_client_in, -1, false, NULL, 0);
    else {
      // Accept incoming connected and receive a single incoming packet
      if(!accept()) return FAIL;
      uint16_t result = receive(_client_in);
      disconnect_in_if_needed();
      return result;
    }
  }
  return FAIL;
};


// Read until a specific 4 byte value is found. This will resync if stream position is lost.
bool EthernetLink::read_until_header(EthernetClient &client, uint32_t header) {
  uint32_t head = 0;
  int8_t bytes_read = 0;
  bytes_read = read_bytes(client, (byte*) &head, 4);
  if(bytes_read != 4 || head != header) { // Did not get header. Lost position in stream?
    do { // Try to resync if we lost position in the stream (throw avay all until HEADER found)
      head = head >> 8; // Make space for 8 bits to be read into the most significant byte
      bytes_read = read_bytes(client, &((byte*) &head)[3], 1);
      if(bytes_read != 1) break;
    } while(head != header);
  }
  return head == header;
};


// Read a package from a connected client (incoming or outgoing) and send ACK
uint16_t EthernetLink::receive(EthernetClient &client) {
  int16_t return_value = FAIL;
  if(client.available() > 0) {
    #ifdef DEBUGPRINT
      Serial.println("Recv from cl");
    #endif

    // Locate and read encapsulation header (4 bytes magic number)
    bool ok = read_until_header(client, HEADER);
    #ifdef DEBUGPRINT
      Serial.print("Read header, stat "); Serial.println(ok);
    #endif

    // Read sender device id (1 byte) and length of contents (4 bytes)
    int16_t bytes_read = 0;
    uint8_t sender_id = 0;
    uint32_t content_length = 0;
    if(ok) {
      byte buf[5];
      bytes_read = read_bytes(client, buf, 5);
      if(bytes_read != 5) ok = false;
      else {
        memcpy(&sender_id, buf, 1);
        memcpy(&content_length, &buf[1], 4);
        if(content_length == 0) ok = 0;
      }
    }

    // Read contents and footer
    byte buf[content_length];
    if(ok) {
      bytes_read = read_bytes(client, (byte*) buf, content_length);
      if(bytes_read != content_length) ok = false;
    }

    // Read footer (4 bytes magic number)
    if(ok) {
      uint32_t foot = 0;
      bytes_read = read_bytes(client, (byte*) &foot, 4);
      if(bytes_read != 4 || foot != FOOTER) ok = false;
    }

    #ifdef DEBUGPRINT
      Serial.print("Stat bfr send ACK: "); Serial.println(ok);
    #endif

    // Write ACK
    return_value = ok ? ACK : NAK;
    int8_t acklen = 0;
    if(ok) {
      acklen = client.write((byte*) &return_value, 2);
      if(acklen == 2) client.flush();
    }

    #ifdef DEBUGPRINT
      Serial.print("Sent "); Serial.print(ok ? "ACK: " : "NAK: "); Serial.println(acklen);
    #endif

    // Call receiver callback function
    if(ok) _receiver(sender_id, buf, content_length);
  }
  return return_value;
};


bool EthernetLink::disconnect_in_if_needed() {
  bool connected = _client_in.connected();
  if(!_keep_connection || !connected) {
    #ifdef DEBUGPRINT
      if(connected) Serial.println("Disc. inclient.");
    #endif
    stop(_client_in);
  }
};


uint16_t EthernetLink::receive(uint32_t duration_us) {
  uint32_t start = micros();
  int16_t result = FAIL;
  do {
    result = receive();
  } while(result != ACK && micros() - start <= duration_us);
  return result;
};


uint16_t EthernetLink::poll_receive(uint8_t remote_id) {
  // Create connection if needed but only poll for incoming packet without delivering any
  if(_single_socket) {
    if(!_server) return single_socket_transfer(_client_out, remote_id, true, NULL, 0);
  } else { // Just do an ordinary receive without using the id
    return receive();
  }
  return FAIL;
};


uint16_t EthernetLink::send(uint8_t id, char *packet, uint8_t length, uint32_t timing_us) {
  // Special algorithm for single-socket transfers
  if(_single_socket)
    return single_socket_transfer(_server ? _client_in : _client_out, id, _server ? false : true, packet, length);

  // Connect or check that we are already connected to the correct server
  bool connected = connect(id);

  // Send the packet and read ACK
  int16_t result = FAIL;
  if(connected) result = send(_client_out, id, packet, length);

  // Disconnect
  disconnect_out_if_needed(result);

  return result;
};


bool EthernetLink::connect(uint8_t id) {
  // Locate the node's IP address and port number
  int16_t pos = find_remote_node(id);
  #ifdef DEBUGPRINT
    Serial.print("Send to srv pos="); Serial.println(pos);
  #endif

  // Break existing connection if not connected to the wanted server
  bool connected = _client_out.connected();
  if(connected && _current_device != id) { // Connected, but to the wrong device
    #ifdef DEBUGPRINT
      //if(_keep_connection && _current_device != -1)
      Serial.println("Switch conn to another srv");
    #endif
    stop(_client_out);
    _current_device = -1;
    connected = false;
  }
  if(pos < 0) return false;

  // Try to connect to server if not already connected
  if(!connected) {
    #ifdef DEBUGPRINT
      Serial.println("Conn..");
    #endif
    connected = _client_out.connect(_remote_ip[pos], _remote_port[pos]);
    #ifdef DEBUGPRINT
      Serial.println(connected ? "Conn to srv" : "Failed conn to srv");
    #endif
    if(!connected) {
      stop(_client_out);
      _current_device = -1;
      return false; // Server is unreachable or busy
    }
    _current_device = id; // Remember who we are connected to
  }

  return connected;
};


void EthernetLink::disconnect_out_if_needed(int16_t result) {
  if(result != ACK || !_keep_connection) {
    stop(_client_out);
    #ifdef DEBUGPRINT
      Serial.print("Disconn outcl. OK="); Serial.println(result == ACK);
    #endif
  }
};


uint16_t EthernetLink::send(EthernetClient &client, uint8_t id, char *packet, uint16_t length) {
  // Assume we are connected. Try to deliver the package
  uint32_t head = HEADER, foot = FOOTER, len = length;
  byte buf[9];
  memcpy(buf, &head, 4);
  memcpy(&buf[4], &id, 1);
  memcpy(&buf[5], &len, 4);
  bool ok = client.write(buf, 9) == 9;
  if(ok) ok = client.write((byte*) packet, length) == length;
  if(ok) ok = client.write((byte*) &foot, 4) == 4;
  if(ok) client.flush();

  #ifdef DEBUGPRINT
    Serial.print("Write stat: "); Serial.println(ok);
  #endif

  // If the other side is sending as well, we need to allow it to be read and ACKed,
  // otherwise we have a deadlock where both are waiting for ACK and will time out unsuccessfully.
  if(!_single_socket && _server) receive();

  // Read ACK
  int16_t result = FAIL;
  if(ok) {
    uint16_t code = 0;
    ok = read_bytes(client, (byte*) &code, 2) == 2;
    if(ok && (code == ACK || code == NAK)) result = code;
  }

  #ifdef DEBUGPRINT
    Serial.print("ACK stat: "); Serial.println(result == ACK);
  #endif

  return result;  // FAIL, ACK or NAK
};


int16_t EthernetLink::send_with_duration(uint8_t id, char *packet, uint8_t length, uint32_t duration_us) {
  uint32_t start = micros();
  int16_t result = FAIL;
  while(result != ACK && micros() - start <= duration_us);
    result = send(id, packet, length);

  return result;
};


int16_t EthernetLink::find_remote_node(uint8_t id) {
  for(uint8_t i = 0; i < MAX_REMOTE_NODES; i++) if(_remote_id[i] == id) return i;
  return -1;
};


int16_t EthernetLink::add_node(uint8_t remote_id, const uint8_t remote_ip[], uint16_t port_number) {
  // Find free slot
  int16_t remote_id_index = find_remote_node(0);
  if(remote_id_index < 0) return remote_id_index; // All slots taken
  _remote_id[remote_id_index] = remote_id;
  memcpy(_remote_ip[remote_id_index], remote_ip, 4);
  _remote_port[remote_id_index] = port_number;
  _remote_node_count++;
  return remote_id_index;
};


void EthernetLink::start_listening(uint16_t port_number) {
  if(_server != NULL) return; // Already started

  #ifdef DEBUGPRINT
    Serial.print("Lst on port "); Serial.println(port_number);
  #endif
  _server = new EthernetServer(port_number);
  _server->begin();
};
