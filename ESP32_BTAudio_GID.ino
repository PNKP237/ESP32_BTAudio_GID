/*
  Dedicated to the countless evenings spent drinking and decoding various CAN packets, resulting in working code that GID just wouldn't work with.
*/

//#define GID_COMPATIBILITY
#include "BluetoothA2DPSink.h"
#include "utf8.h"       // library enabling simple UTF8 to UTF16 converion
#include <vector>       // vector is needed for utf8 library
#include "driver/twai.h"


BluetoothA2DPSink a2dp_sink;

// pin defs
const int DACSEL_PIN=27, DIS_SEL0_PIN=16, DIS_SEL1_PIN=17, PCM_MUTE_CTL=23;
// data buffers
static char buffer[128], utf16chars[256], chararray[32][8];
static std::vector<unsigned short> utf16result;
// CAN buffers
uint32_t alerts_triggered;
static twai_message_t RxMessage, TxPacket;
// global metadata switches
bool md_recvd=0, md_autoupdate=0, can_MessageReady=0, can_MessageSent=1;
// global bluetooth switches
bool bt_connected=0, bt_state_changed=0, bt_audio_playing=0, audio_state_changed=0;
// lenght of the main char buffer[128]
int text_lenght=0, bytes_processed=0;
// text constants
char *conn_state_text[2]={"Connected", "Not connected"};

void avrc_metadata_callback(uint8_t data1, const uint8_t *data2) {  // fills the song title buffer with data, updates text_lenght with the amount of chars
  //Serial.printf("%s\n", data2);
  data_clear_buffer();
  text_lenght=snprintf(buffer, sizeof(buffer), "%s", data2);
  md_recvd=1;                                                       // lets the main loop() know that there's a new song title in the buffer
}

void a2dp_connection_state_changed(esp_a2d_connection_state_t state, void *ptr){    // callback for bluetooth connection state change
  if(state==2){                                                                     // state=0 -> disconnected, state=1 -> connecting, state=2 -> connected
    bt_connected=1;
  } else {
    bt_connected=0;
  }
  bt_state_changed=1;
}

void a2dp_audio_state_changed(esp_a2d_audio_state_t state, void *ptr){  // callback for audio playing/stopped
  if(state==2){                                                         //  state=1 -> stopped, state=2 -> playing
    bt_audio_playing=1;
  } else {
    bt_audio_playing=0;
  }
  audio_state_changed=1;
}

void setup() {
  pinMode(DACSEL_PIN, INPUT_PULLUP);            // DAC select, D27 HIGH/NC -> PCM5102, D27 LOW -> internal DAC at D25 and D26
  pinMode(DIS_SEL0_PIN, INPUT_PULLUP);          // LSB
  pinMode(DIS_SEL1_PIN, INPUT_PULLUP);          // MSB
  pinMode(PCM_MUTE_CTL, OUTPUT);
  digitalWrite(PCM_MUTE_CTL, LOW);
  delay(100);
  Serial.begin(921600);                 // serial comms for debug
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_5, GPIO_NUM_4, TWAI_MODE_NORMAL);         // CAN bus set up
  g_config.rx_queue_len=20;     // until I can come up with better multitasking, the incoming message queue has to be long -> make the AC section non-blocking
  g_config.tx_queue_len=10;     // for longer outgoing messages with 
  twai_timing_config_t t_config =  {.brp = 42, .tseg_1 = 15, .tseg_2 = 4, .sjw = 3, .triple_sampling = false};    // set CAN prescalers and time quanta for 95kbit
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();                                                // TODO: set up proper filters
  TxPacket.extd=0;                      // TxPacket settings - not extended ID CAN packet
  TxPacket.rtr=0;                       // not retransmission packet
  TxPacket.ss=1;                        // transmit the packet as a single shot
  TxPacket.data_length_code=8;          // 8 bytes of data
  
  // CAN SETUP
  Serial.print("\nCAN/TWAI SETUP => ");
  if(twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
      Serial.print("DRV_INSTALL: OK ");
  } else {
      Serial.print("DRV_INST: FAIL ");
  }
  if (twai_start() == ESP_OK) {
      Serial.print("DRV_START: OK ");
  } else {
      Serial.print("DRV_START: FAIL ");
  }
  uint32_t alerts_to_enable=TWAI_ALERT_TX_SUCCESS;
  if(twai_reconfigure_alerts(alerts_to_enable, NULL) == ESP_OK){
      Serial.print("ALERTS: OK ");
  } else {
      Serial.print("ALERTS: FAIL \n");
  }

  if(!digitalRead(DACSEL_PIN)){                      // internal DAC or ext DAC
    const i2s_config_t i2s_config = {
        .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
        .sample_rate = 44100,
        .bits_per_sample = (i2s_bits_per_sample_t) 16,
        .channel_format =  I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_MSB,
        .intr_alloc_flags = 0,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true
    };
    a2dp_sink.set_i2s_config(i2s_config);  
  } else {                                                          // ext dac BLCK=26  WS/LRCK=25  DOUT=22
    const i2s_config_t i2s_config = {
        .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 44100,
        .bits_per_sample = (i2s_bits_per_sample_t)16,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = (i2s_comm_format_t) (I2S_COMM_FORMAT_STAND_I2S),
        .intr_alloc_flags = 0,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = true,
        .tx_desc_auto_clear = true
    };
    a2dp_sink.set_i2s_config(i2s_config);  
  }

  a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
  a2dp_sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE);
  a2dp_sink.set_on_connection_state_changed(a2dp_connection_state_changed);
  a2dp_sink.set_on_audio_state_changed(a2dp_audio_state_changed);
  if(DIS_SEL1_PIN){                                                       // shorting D17 LOW disables auto reconnect
    a2dp_sink.set_auto_reconnect(true);
  }
  a2dp_sink.start("Asterka");                                                       // setting up bluetooth audio sink
  for(int i=0;i<sizeof(buffer);i++){
    buffer[i]=0;
  }
  char *startup_text="Ready!";
  text_lenght=snprintf(buffer, sizeof(buffer), "%s", startup_text);
  processDataBuffer();
}


void loop() {
  if(can_MessageReady){       // can_MessageReady is set after packet preamble is sent. This waits for the display to reply with an ACK
    delay(4);
    RxMessage.identifier=0x0;                         // workaround for debugging when the bus is not on
    twai_receive(&RxMessage, pdMS_TO_TICKS(100));
    while(!RxMessage.identifier==0x2C1){
      twai_receive(&RxMessage, pdMS_TO_TICKS(100));
    }
    sendMultiPacketData();
  }
  canReceive();                       // process CAN message, shortcoming: only processes a single message per loop
  if(md_recvd){                       // if there's a new title in the buffer[128] then process the buffer
    if(text_lenght!=0){              // only if song title is longer than 0 chars
      processDataBuffer();
      md_autoupdate=1;
    } else {
      md_autoupdate=0;
    }
    md_recvd=0;
  }
  if(audio_state_changed){
    if(bt_audio_playing){
      digitalWrite(PCM_MUTE_CTL, HIGH);
    } else {
      digitalWrite(PCM_MUTE_CTL, LOW);
    }
    audio_state_changed=0;
  }
  
  if(bt_state_changed){               // crude way of printing Connected/Disconnected messages
    data_clear_buffer();
    if(bt_connected){
      text_lenght=snprintf(buffer, sizeof(buffer), "%s", conn_state_text[0]);
    } else {
      text_lenght=snprintf(buffer, sizeof(buffer), "%s", conn_state_text[1]);
    }
    processDataBuffer();
    bt_state_changed=0;
  }
}

void processDataBuffer(){             // convert buffer[128] (UTF8 chars) to utf16chars[128] (UTF16 chars)
  if(!can_MessageReady){                      // ONLY PREPARE NEW BUFFERS ONCE THE PREVIOUS BUFFER HAS BEEN TRANSMITTED, ELSE THE PREAMBLE ISN'T GOING TO MATCH THE MESSAGE!!!
    for(;text_lenght<10;text_lenght++){
      buffer[text_lenght]=' ';
    }
    utf8_to_utf16();                    // convert UTF8 buffer to UTF16 chars
    prepareMultiPacket();               // stitches together CAN message array
    sendMultiPacket();                  // sends the stored CAN message
  }
}

void utf8_to_utf16(){
  bytes_processed=0;
  for(int i=0; i<sizeof(utf16chars); i++){                  // clearing buffers etc
    utf16chars[i]=0;
  }

  if(digitalRead(DIS_SEL0_PIN)){     // D16 not shorted - command 4000 - works on all displays (hopefully)
    utf16chars[1]=0x40;
  } else {                           // D16 shorted - command C000 - works only on TID/BID
    utf16chars[1]=0xC0;
  }
  // bytes below set up a text message to the display with first few chars being "[fS_gm" (8-20) which produces a left-aligned text (BID/GID/CID)
  // for BID/GID/CID there is also a smaller font used in radio messages, like FM3 and MHz designated with "[fS_dm", simply change [18] to 0x64 to make id a "d" - looks dumb on BID
  // for BID there is also "[cm" which results in centered text
  /*[0] is size / [1] command MSB */ utf16chars[2]=0x00;  /*[3] remaining bytes*/ utf16chars[4]=0x03; utf16chars[5]=0x10; utf16chars[6]=0x01;
  utf16chars[8]=0x1B; utf16chars[9]=0x00; utf16chars[10]=0x5B; utf16chars[11]=0x00; utf16chars[12]=0x66; utf16chars[13]=0x00; utf16chars[14]=0x53; 
  utf16chars[15]=0x00; utf16chars[16]=0x5F; utf16chars[17]=0x00; utf16chars[18]=0x67; utf16chars[19]=0x00; utf16chars[20]=0x6D; 
  int last_utf16char=21;                                // provide next number in order
  bytes_processed+=last_utf16char;
  
  utf16result.clear();                                                          // clearing vector
  utf8::utf8to16(buffer, buffer+text_lenght, std::back_inserter(utf16result)); // converts data from buffer to UTF16
  int last_byte=0;
  for(int i=0; i<utf16result.size(); i++){                          // utf16 chars are split into singular bytes for loading into CAN messages
    utf16chars[i*2+last_utf16char]=(utf16result[i] & 0xFF00) >> 8;
    utf16chars[i*2+(1+last_utf16char)]=utf16result[i] & 0x00FF;
    if((i+1)<utf16result.size()){
      last_byte=(i+1)*2+1+last_utf16char;
    }
    bytes_processed+=2;
  }
  utf16chars[last_byte+1]=0x11;            // end the message with 4 additional bytes as per CD70 AUX message
  utf16chars[last_byte+2]=0x01;
  utf16chars[last_byte+3]=0x00;
  utf16chars[last_byte+4]=0x20;
  utf16chars[last_byte+5]=0x12;
  utf16chars[last_byte+6]=0x01;
  utf16chars[last_byte+7]=0x00;
  utf16chars[last_byte+8]=0x20;
  bytes_processed+=8;
  
  // update payload data in the 1st packet
  utf16chars[0]=bytes_processed;
  utf16chars[3]=bytes_processed-3;
  if(bytes_processed%7==0){                   // if the amount of bytes would result in a full packet (ie no unused bytes), add a char to overflow into the next packet
    utf16chars[last_byte+6]+=1;          // workaround because if the packets are full the display would ignore the message
    utf16chars[last_byte+9]=0x00; utf16chars[last_byte+10]=0x20;
    bytes_processed+=2;
  }
  utf16chars[6]=text_lenght+7;
}

void data_clear_buffer(){
  for(int i=0;i<sizeof(buffer);i++){
    buffer[i]=0;
  }
}