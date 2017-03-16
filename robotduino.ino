#include <SoftwareSerial.h>
#include <stdint.h>

#define MD49_RX 8
#define MD49_TX 9
const int SABER_TX = 10;
const int SABER_UNUSED = 11;

// constants for MD49 commands
#define GET_SPEED_1 0x21
#define GET_SPEED_2 0x22
#define GET_ENCODERS 0x25
#define SET_SPEED_1 0x31
#define SET_SPEED_2 0x32
#define SET_MODE 0x34
#define RESET_ENCODERS 0x35
#define DISABLE_TIMEOUT 0x38

// see MD49 docs for mode
#define MODE_SIMPLE 0
#define MODE_SIMPLE_SIGNED 1
#define MODE_MANAGED 2
#define MODE_MANAGED_SIGNED 3

const int CMD_MD49 = 0x01;
const int CMD_SABER = 0x02;
const int CMD_COMP = 0x03;

const int COMP_CMD_MOVE = 0x01;
const int COMP_CMD_START = 0x02;

#define SYNC 0

struct Instruction {
  int32_t enc1;
  int32_t enc2;
};

struct CompState {
  uint8_t cmd;
  uint8_t cmddata[8];
  uint8_t data_pos;
};

const struct Instruction insts[] = {
  {1874, 1861},
  {194, 181}
};
struct CompState compstate;


SoftwareSerial md49(MD49_RX, MD49_TX);
SoftwareSerial sabertooth(SABER_UNUSED, SABER_TX);

uint8_t mode = 0;

void md49_move(int32_t *desired_encs) {
  /*Serial.println("md49_move");
  Serial.println(desired_encs[0]);
  Serial.println(desired_encs[1]);*/
  int32_t enc_vals[2];
  boolean done[2] = {false, false};
  if (desired_encs[0] < 0) {
    md49_set_speed_1(-30);
  } else if (desired_encs[0] > 0) {
      md49_set_speed_1(30);
  } else if (desired_encs[0] == 0) {
    done[0] = true;
  }

  if (desired_encs[1] < 0) {
    md49_set_speed_2(-30);
  } else if (desired_encs[1] > 0) {
    md49_set_speed_2(30);
  } else if (desired_encs[1] == 0) {
    done[1] = true;
  }
  while (!(done[0] && done[1])) {
    md49_get_encoders(enc_vals);
    /*Serial.write("ONE");
    Serial.println(enc_vals[0]);
    Serial.write("TWO");
    Serial.println(enc_vals[1]);*/
    for (uint8_t i = 0; i <= 1; ++i) {
      /*Serial.write("TEST");
      Serial.println((uint16_t)i);*/
      if (!done[i] && ((desired_encs[i] > 0 && enc_vals[i] >= desired_encs[i]) ||
                       (desired_encs[i] < 0 && enc_vals[i] <= desired_encs[i]))) {
        if (i == 0 && !done[0]) {
          md49_set_speed_1(0);
        } else if (i == 1 && !done[1]) {
          md49_set_speed_2(0);
        }
       done[i] = true;
      }
    }
  }
  md49_reset_encoders();
}

void md49_wait_byte() {
  // block until byte available
  while (!md49.available()) {
  }
}


void comp_wait_byte() {
  // block until byte available
  while (!Serial.available()) {
  }
}

void md49_send(uint8_t cmd, uint8_t *data, size_t data_len) {
  if (data_len > 2) {
    return;
  }
  /*Serial.println("md49_send");
  Serial.write("cmd");
  Serial.println((uint16_t)cmd);*/
  uint8_t cmd_out[4];
  cmd_out[0] = SYNC;
  cmd_out[1] = cmd;
  if (data_len > 0) {
    memcpy(cmd_out + 2, data, data_len);
  }
  /*for (uint8_t i = 0; i < data_len + 2; ++i) {
    Serial.println((uint16_t)cmd_out[i]);
  }*/
  md49.write(cmd_out, data_len + 2);
}

int8_t md49_get_speed_1() {
  md49_send(GET_SPEED_1, NULL, 0);
  md49_wait_byte();
  byte speed = md49.read();
  return speed;
}

int8_t md49_get_speed_2() {
  md49_send(GET_SPEED_2, NULL, 0);
  md49_wait_byte();
  byte speed = md49.read();
  return speed;
}

void md49_set_speed_1(int8_t speed) {
  //Serial.write("ss1");
  //Serial.println(speed);
  uint8_t pckt[1];
  pckt[0] = speed;
  //Serial.println(pckt[0], DEC);
  md49_send(SET_SPEED_1, pckt, 1);
}

void md49_set_speed_2(int8_t speed) {
  uint8_t pckt[1];
  pckt[0] = speed;
  md49_send(SET_SPEED_2, pckt, 1);
}

int32_t translate_long_resp(uint8_t *resp) {
  int32_t r = resp[0];
  int32_t out = r << 24ul;
  r = resp[1];
  out += r << 16ul;
  r = resp[2];
  out += r << 8ul;
  r = resp[3];
  out += r;
  return out;
}

void long_to_bytes(byte* buf, long l) {
  buf[0] = (char)((l >> 24) & 0xff);
  buf[1] = (char)((l >> 16) & 0xff);
  buf[2] = (char)((l >> 8) & 0xff);
  buf[3] = (char)(l & 0xff);
}

void md49_get_encoders(int32_t *buf) {
  md49_send(GET_ENCODERS, NULL, 0);
  byte bytes[8];
  for (unsigned char i = 0; i < 8; ++i) {
    md49_wait_byte();
    bytes[i] = md49.read();
  }
  buf[0] = translate_long_resp(bytes);
  buf[1] = translate_long_resp(bytes + 4);
}

void md49_reset_encoders() {
  md49_send(RESET_ENCODERS, NULL, 0);
}

void md49_disable_timeout() {
  md49_send(DISABLE_TIMEOUT, NULL, 0);
}

void md49_set_mode(byte mode) {
  md49_send(SET_MODE, &mode, 1);
}

void start_move() {
  uint8_t moves = sizeof (insts) / sizeof (Instruction);
  int32_t encs[2];
  for (uint8_t i = 0; i < moves; ++i) {
    struct Instruction inst = insts[i];
    encs[0] = inst.enc1;
    encs[1] = inst.enc2;
    Serial.println(inst.enc1, inst.enc2);
    md49_move(encs);
    delay(500);
  }
}

void comp_cmd(byte input) {
  /*Serial.write("i");
  Serial.println(input);*/
  if (compstate.cmd == 0) {
    compstate.cmd = input;
    return;
  }
  switch (compstate.cmd) {
  case COMP_CMD_MOVE:
    //Serial.println(input);
    /*Serial.write("a");
    Serial.println(compstate.data_pos);*/
    if (compstate.data_pos <= 8) {
      compstate.cmddata[compstate.data_pos] = input;
      ++compstate.data_pos;
      if (compstate.data_pos < 8) {
        return;
      }
    }
    int32_t desired_encs[2];
    desired_encs[0] = translate_long_resp(compstate.cmddata);
    desired_encs[1] = translate_long_resp(compstate.cmddata + 4);
    md49_move(desired_encs);
    compstate.cmd = 0;
    compstate.data_pos = 0;
    return;
  case COMP_CMD_START:
    start_move();
    compstate.cmd = 0;
    compstate.data_pos = 0;
    return;
  }
}

void cmd_loop() {
  if (md49.available() > 0) {
    Serial.write(md49.read());
  }
  if (Serial.available() > 0) {
    if (mode == 0) {
      mode = Serial.read();
    }
    if (mode == CMD_MD49) {
      byte received = Serial.read();
      md49.write(received);
      mode = 0;
    } else if (mode == CMD_SABER) {
      byte received = Serial.read();
      sabertooth.write(received);
      mode = 0;
    } else if (mode == CMD_COMP) {
      byte received = Serial.read();
      comp_cmd(received);
      mode = 0;
    }
  }
}

void setup() {
  compstate.cmd = 0;
  compstate.data_pos = 0;
  
  Serial.begin(9600);
  while (!Serial);
  sabertooth.begin(9600);
  md49.begin(9600);

  md49.listen();

  md49_reset_encoders();
  md49_set_mode(MODE_SIMPLE_SIGNED);
  md49_disable_timeout();
  //Serial.println("INIT DONE");
}

void loop() {
  cmd_loop();
}
