// Secondary

#include "CRC16.h"

#define A_PIN 5
#define B_PIN 6

#define MAX_PAYLOAD_SIZE 58 // in bytes
#define MAX_FRAME_LENGTH 64 // MAX_PAYLOAD_SIZE + 6 (Address + Control + 2*CRC + 2*Flags)

#define DEBUG_LEVEL 1 // 1 show frames, 2 show frames + received byte

#define LOCAL_ADDRESS 0x03
// ***** reserved addresses: *****
//         0x00 (0)   - Default (route to Primary device)
//         0xff (255) - Broadcast (unimplemented)

// ##### FRAME #####
uint16_t send_seq_num;  // shall be incremented each sucessive information frame sent, reset in link inicialization
uint16_t recv_seq_num;  // represents next expected information frame in sequence
char payload_[MAX_PAYLOAD_SIZE]; // message to sent 
uint16_t auto_payload_size;

uint8_t linked_device = 0x01; // Primary address

CRC16 crc; // error check

// ##### TEMPLATES #####
uint8_t flag =  0b01111110; // 0x7e
uint8_t info =  0b00000000; // 0[7],    N(s)[6:4], (P/F)[3], N(r)[2:0]
uint8_t super = 0b10000000; // 1[7], 0[6], S[5:4], (P/F)[3], N(r)[2:0]
uint8_t unnum = 0b11000000; // 1[7], 1[6], M[5:4], (P/F)[3], M[2:0]

// ### SETS ###
struct bytedata {bool is_refreshed; uint8_t value;};
struct framedata {bool ready; uint16_t lenght; uint8_t buffer[MAX_FRAME_LENGTH];};

bytedata  received_byte;
framedata frame;

// ******************************************************************

// ##########################
// #####      RS485     #####
// ##########################
void sendByte(uint8_t byte_, bool set_receive = false){
  // start bit: b'0 (A > B)
  pinMode(A_PIN, OUTPUT); pinMode(B_PIN, OUTPUT); // set hardware send mode
  digitalWrite(A_PIN, 255); digitalWrite(B_PIN, 0); // b'0
  delay(5); // 200hz bit delay
  
  // content: 8 bits
  for (uint8_t bit_pos = 0; bit_pos < 8; bit_pos++) {
    digitalWrite(A_PIN, not getBit(byte_, (7-bit_pos)));
    digitalWrite(B_PIN,     getBit(byte_, (7-bit_pos)));
    delay(5); // 200hz bit delay
  }  
  
  // final bit: b'1 (B > A)
  digitalWrite(A_PIN, 0); digitalWrite(B_PIN, 255); // b'1
  delay(5); // 200hz bit delay
  if (set_receive){
    pinMode(A_PIN, INPUT); pinMode(B_PIN, INPUT_PULLUP); // set hardware receive mode
  }
  if (DEBUG_LEVEL > 0) {
    Serial.print(byte_ > 0xf ? " 0x" : " 0x0"); 
    Serial.print(byte_, HEX);
  }
}

bytedata scanByte(uint8_t timeout_ms = 100, uint16_t scan_delay_us = 100){
  // return false and 0 if not receive start bit within the timeout
  uint8_t byte_ = 0; // alocate received byte

  unsigned long init_time = millis();
  while (init_time + timeout_ms > millis()){
    if (digitalRead(A_PIN) && not digitalRead(B_PIN)){ // start bit: b'0 (A > B)
      delay(6); // wait for MSB set
      
      if (DEBUG_LEVEL > 1) Serial.print("\nBYTE RECV: (MSB)");
      
      for (int bit_pos = 0; bit_pos < 8; bit_pos++) { // sequentially records the expected 8 bits
        if (not digitalRead(A_PIN) && digitalRead(B_PIN)){ // if A=0 and B=1 b'1 otherwise b'0
          bitSet(byte_, (7-bit_pos)); // MSB first
        }
        if (DEBUG_LEVEL > 1) Serial.print(getBit(byte_, (7-bit_pos))); // print received bit
        delay(5); // 200hz bit delay
      } 
      if (DEBUG_LEVEL > 1) {
        Serial.print(byte_ > 0xf ? " 0x" : " 0x0"); 
        Serial.print(byte_, HEX);
      }
      received_byte.is_refreshed = true;
      received_byte.value = byte_;
      return received_byte;
    }
    delayMicroseconds(scan_delay_us);
  }
  bool byte_is_refreshed = false;
  received_byte.is_refreshed = false;
  received_byte.value = 0;
  return received_byte;
}

// ******************************************************************


// ##########################
// #####      HDCL      #####
// ##########################
framedata addToFrameBuffer(uint8_t byte_){
  if (frame.ready) return frame;
  if (not frame.lenght and byte_ != flag) return frame;  // drop first byte except flag

  frame.buffer[frame.lenght] = byte_;
  frame.lenght++;
  frame.ready = (frame.lenght > 1 and byte_ == flag) or (frame.lenght >= MAX_FRAME_LENGTH);
}

void resetFrameBuffer(){
  frame.ready = false;
  frame.lenght = 0;
  memset(frame.buffer, 0, MAX_FRAME_LENGTH * sizeof(uint8_t));
}

void sendFlag(bool final_flag = false){
  crc.restart();
  sendByte(flag, final_flag);
}

void sendAddress(uint8_t address){
  linked_device = address;
  sendByte(address);
  crc.add(address);
}

void sendControl(uint8_t control, uint8_t sqn, bool pool_or_final, uint8_t rqn, uint8_t super_command = 0b00){
  // control input is a template
  // sqn = send sequence number, rqn = receive sequence number
  // pool mean the Primary is requesting a response, consenting to destine permission to use the link 
  // final mean the Secondary finished your queue, and abdicate the link permission
  
  if (control == info){
    if (getBit(rqn, 0)) bitSet(control, 0);
    if (getBit(rqn, 1)) bitSet(control, 1);
    if (getBit(rqn, 2)) bitSet(control, 2);
    if (pool_or_final)  bitSet(control, 3);
    if (getBit(sqn, 0)) bitSet(control, 4);
    if (getBit(sqn, 1)) bitSet(control, 5);
    if (getBit(sqn, 2)) bitSet(control, 6);
  }

  if (control == super){
    // super_command : 0b00-RR(Receive ready); 0b01-REJ(Reject); 0b10-RNR(Receive not ready); 0b11-SREJ(Selective reject)
    if (getBit(rqn, 0)) bitSet(control, 0);
    if (getBit(rqn, 1)) bitSet(control, 1);
    if (getBit(rqn, 2)) bitSet(control, 2);
    if (pool_or_final)  bitSet(control, 3);
    if (getBit(super_command, 0)) bitSet(control, 4);
    if (getBit(super_command, 1)) bitSet(control, 5);
  }
 
  sendByte(control);
  crc.add(control);
}

void sendControlInfo(bool pool_or_final = true){ 
  sendControl(info, send_seq_num, pool_or_final, recv_seq_num);
}

void sendControlRR(bool final = true){ 
  sendControl(super, send_seq_num, final, recv_seq_num, 0b00); // RR
}

void sendControlREJ(bool final = true){ 
  sendControl(super, send_seq_num, final, recv_seq_num, 0b01); // REJ
}

void sendPayload(uint16_t payload_size = auto_payload_size, uint16_t init_index = 0){
  // Big Endian: send first the most significant byte in payload index range
  if (not payload_size or init_index > MAX_PAYLOAD_SIZE-1) {
    sendByte(0); // send at least an empty byte, once is an information frame
    crc.add(0);
  }
  else { // general case
    for (uint16_t index = init_index;
        index < (payload_size > MAX_PAYLOAD_SIZE ? MAX_PAYLOAD_SIZE : payload_size); // index overflow handler
        index++) {  // swapped input handler 
      sendByte(payload_[index]);
      crc.add(payload_[index]);
    }
  }
}

void addToPayloadBuffer(String input_, bool reset = true, uint16_t init_index = 0, bool set_payload_size = true){
  if (reset) memset(payload_, 0, MAX_PAYLOAD_SIZE * sizeof(char)); // reset payload buffer
  if (set_payload_size) auto_payload_size = (input_.length() < MAX_PAYLOAD_SIZE ? input_.length() : MAX_PAYLOAD_SIZE);
  if (init_index < MAX_PAYLOAD_SIZE) { // index overflow handler
    for (uint16_t index = init_index;
         index < (sizeof(input_) > MAX_PAYLOAD_SIZE-init_index ? MAX_PAYLOAD_SIZE : input_.length());
         index++) {  
      payload_[index] = input_[index];
    } 
  }
}

void sendCRC(){
  // cyclic redundancy check (CRC) is an error-detecting code
  uint16_t crc_result = crc.calc();
  sendByte(crc_result>>8);
  sendByte((crc_result<<8)>>8);
}

void sendInfoFrame(uint8_t forward_address, bool pool_or_final = true, bool reset_sequence = true, bool increment_sequence = true){
  if (reset_sequence)     send_seq_num = 1; else
  if (increment_sequence) send_seq_num++;
  if (DEBUG_LEVEL > 0) Serial.print("\nSEND:");
  sendFlag();
  sendAddress(forward_address);
  sendControlInfo();
  sendPayload();
  sendCRC();
  sendFlag(true);
  if (DEBUG_LEVEL > 0) Serial.print((String)" n(s)="+send_seq_num+" n(r)="+recv_seq_num+" ASCII:'"+payload_+"' P/F:"+pool_or_final+" (Info)");
}

void sendSuperFrame(uint8_t forward_address, uint8_t command = 0b00, bool pool_or_final = true){
  if (DEBUG_LEVEL > 0) Serial.print("\nSEND:");
  sendFlag();
  sendAddress(forward_address);
  if (command == 0b00) sendControlRR(pool_or_final); else sendControlREJ(pool_or_final);
  sendCRC();
  sendFlag(true);
  if (DEBUG_LEVEL > 0) Serial.print((String)" n(r)="+recv_seq_num+" P/F:"+pool_or_final+" (Super - "+(command == 0b00 ? "RR)" : (command == 0b01 ? "REJ)" : "ERR)")));
}

uint8_t processFrame(bool reset_recv_seq_num = true){
  // returns, also output_code
  // - 0: None
  // - 1: INFO (CRC OK)
  // - 2: INFO (CRC REJ)
  // - 3: SUPER RR  (CRC OK)
  // - 4: SUPER RR  (CRC REJ)
  // - 5: SUPER REJ (CRC OK)
  // - 6: SUPER REJ (CRC REJ) 
  // - 7: ERROR
  // - 8: Response READY
  // - 9: Other Address

  if (reset_recv_seq_num) recv_seq_num = 1; // reset expected counter
  if (not frame.ready) return 0;

  bool pool_or_final = getBit(frame.buffer[2], 3);
  
  if (DEBUG_LEVEL > 0) { // print received bytes
    Serial.print("\nRECV:");
    for (uint8_t index = 0; index < frame.lenght; index++) {
      Serial.print(frame.buffer[index] > 0xf ? " 0x" : " 0x0"); 
      Serial.print(frame.buffer[index], HEX);
    }
  }

  if (frame.buffer[1] != LOCAL_ADDRESS) {
    if (DEBUG_LEVEL > 0) Serial.print(" <drop>"); 
    resetFrameBuffer();
    return 9; // drop frame cause different address
  }

  if (not getBit(frame.buffer[2], 7)){ // Information frame process
    char payload_[MAX_PAYLOAD_SIZE];
    memset(payload_, 0, MAX_PAYLOAD_SIZE * sizeof(char)); // clean payload buffer

    // extract payload
    for (uint8_t index = 0; index < frame.lenght - 6; index++) payload_[index] = frame.buffer[index+3];
    
    // test crc
    crc.restart();
    for (uint8_t index = 0; index < frame.lenght - 4; index++) crc.add(frame.buffer[index+1]);
    uint16_t crc_result = crc.calc(); 

    bool crc_ok = false;
    if (crc_result>>8     == frame.buffer[frame.lenght-3] and 
       (crc_result<<8)>>8 == frame.buffer[frame.lenght-2]) crc_ok = true; // TEST / SIMULATE CRC FAIL HERE
    
    crc.restart();

    if (DEBUG_LEVEL > 0) Serial.print((String)" n(s)="+send_seq_num+" n(r)="+recv_seq_num+" ASCII:'"+payload_+"' CRC:"+(crc_ok ? "OK" : "REJ")+" P/F:"+pool_or_final+" (Info)");

    Serial.print((String)"\n>> recv:'"+(String)payload_+"' CRC:"+(crc_ok ? "OK" : "REJ")+" n(s)="+send_seq_num+" n(r)="+recv_seq_num+" from"); 
    Serial.print(linked_device > 0xf ? " 0x" : " 0x0"); Serial.print(linked_device, HEX);
    Serial.print((String)"\n>> resp: "+(crc_ok ? "RR" : "REJ")+" n(r)="+recv_seq_num+" to"); 
    Serial.print(linked_device > 0xf ? " 0x" : " 0x0"); Serial.print(linked_device, HEX);
    
    sendSuperFrame(linked_device, (crc_ok ? 0b00 : 0b01), not((String)payload_ == "ok?" or (String)payload_ == "send your route request" or (String)payload_ == "Heyyy!" or (String)payload_ == "Byyye!" )); 
    resetFrameBuffer();

    if ((String)payload_ == "ok?" and crc_ok){
      String message = "ok";
      Serial.print((String)"\n>> send:'"+message+"' to Primary");
      addToPayloadBuffer(message);
      delay(100);
      return 8; // Response READY 
    }

    if ((String)payload_ == "send your route request" and crc_ok){
      String message = "route hELLooo! to 0x02";
      Serial.print((String)"\n>> send:'"+message+"' to Primary");
      addToPayloadBuffer(message);
      delay(100);
      return 8; // Response READY 
    }

    if ((String)payload_ == "Heyyy!" and crc_ok){
      String message = "route hELLooo! to 0x02";
      Serial.print((String)"\n>> send:'"+message+"' to Primary");
      addToPayloadBuffer(message);
      delay(100);
      return 8; // Response READY 
    }

    if ((String)payload_ == "Byyye!" and crc_ok){
      String message = "route See u! to 0x02";
      Serial.print((String)"\n>> send:'"+message+"' to Primary");
      addToPayloadBuffer(message);
      delay(100);
      return 8; // Response READY 
    }

    if (crc_ok) {
      recv_seq_num++;
      return 1; // INFO (CRC Ok)
    }
    return 2;   // INFO (CRC REJ)
  }

  if (getBit(frame.buffer[2], 7) and not getBit(frame.buffer[2], 6)){ // Supervisory frame process
    uint8_t command = 0b00;
    if (getBit(frame.buffer[2], 4)) bitSet(command, 0);
    if (getBit(frame.buffer[2], 5)) bitSet(command, 1);

    // test crc
    crc.restart();
    for (uint8_t index = 0; index < frame.lenght - 4; index++) crc.add(frame.buffer[index+1]);
    uint16_t crc_result = crc.calc(); 

    bool crc_ok = false;
    if (crc_result>>8     == frame.buffer[frame.lenght-3] and 
       (crc_result<<8)>>8 == frame.buffer[frame.lenght-2]) crc_ok = true;
    
    crc.restart();

    if (DEBUG_LEVEL > 0) Serial.print((String)" n(r)="+recv_seq_num+" CRC:"+(crc_ok ? "OK" : "REJ")+" P/F:"+pool_or_final+" (Super - "+(command == 0b00 ? "RR)" : (command == 0b01 ? "REJ)" : "ERR)")));
  
    Serial.print((String)"\n>> recv: "+(command == 0b00 ? "RR": "REJ")+" CRC:"+(crc_ok ? "OK" : "REJ")+" n(r)="+recv_seq_num+" from"); 
    Serial.print(linked_device > 0xf ? " 0x" : " 0x0"); Serial.print(linked_device, HEX);

    resetFrameBuffer();

    if (command == 0b00 and crc_ok)     return 3; // SUPER RR  (CRC OK)
    if (command == 0b00 and not crc_ok) return 4; // SUPER RR  (CRC REJ)
    if (command == 0b01 and crc_ok)     return 5; // SUPER REJ (CRC OK)
    if (command == 0b01 and not crc_ok) return 6; // SUPER REJ (CRC REJ)
    return 7;   // INFO (CRC REJ)
  }
}

bool sendMessage(uint8_t forward_address, uint16_t timeout_ms = 3000, uint16_t retry_delay_ms = 500, int retry_times = 4){ 
  // This function handles the transmission of messages, 
  // performing loop attempts according to the input parameters, 
  // if it receives REJ or reaches the timeout
  // * negative retry_times mean non stop loop
  
  uint8_t  output_code = 0;
  unsigned long init_time = 0;

  while (output_code != 3){
    if (not init_time) sendInfoFrame(forward_address); // first emission
    else {
      if (retry_times > 0) retry_times --; else
      if (not retry_times) break; // cancel
      delay(retry_delay_ms);
      sendInfoFrame(forward_address, true, false, false); // retry
    }
    output_code = 0;
    init_time = millis();
    while (millis() < init_time + timeout_ms){
      scanByte();
      if (received_byte.is_refreshed) addToFrameBuffer(received_byte.value);
      if (frame.ready) {
        output_code = processFrame();
        if (output_code == 3) return true; // SUPER RR  (CRC OK)
        break;
      }
    }
    if (DEBUG_LEVEL > 0) Serial.print((output_code ? " <REJ/ERR>" : " <timeout>")); 

  }
  if (DEBUG_LEVEL > 0) Serial.print(" <canceled>"); 
  return false;
}

// ******************************************************************

// ##########################
// ##### MAIN FUNCTIONS #####
// ##########################

void setup(){
  Serial.begin(9600);
  Serial.print((String)"\nSecondary Device - Initing...\nMy Address: "+LOCAL_ADDRESS);

  pinMode(A_PIN, INPUT); pinMode(B_PIN, INPUT_PULLUP); // initial state is receive mode
  memset(payload_, 0, MAX_PAYLOAD_SIZE * sizeof(char)); // clean payload buffer
  resetFrameBuffer();
  received_byte.is_refreshed = false;
  received_byte.value = 0;
  crc.restart();

  send_seq_num = 1;
  recv_seq_num = 1;
}

void loop(){
  scanByte();
  if (received_byte.is_refreshed) addToFrameBuffer(received_byte.value);
  if (frame.ready and processFrame() == 8) { // "8" code is Response READY
    delay(500); // delay to give time to notice as different serial print outputs, or different frames in the osciloscope
    sendMessage(0x01);
    resetFrameBuffer();
  }
}

// ##### SUPPORT FUNCTIONS #####
bool getBit(unsigned char byte, int position){
  return (byte >> position) & 0x1; // position 0-7
}
