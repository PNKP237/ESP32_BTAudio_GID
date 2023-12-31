void sendPacket(int id, char can_send_buffer[8], int dlc=8){
  TxPacket.identifier=id;
  TxPacket.data_length_code=dlc;                      // CAN packets set to 8 bytes by default, can be overridden, for emulating button presses and such
  //TxPacket.self=1;
  for(int i=0; i<dlc; i++){                             // load data into message, queue the message, then read alerts
    TxPacket.data[i]=can_send_buffer[i];
  }
  if (twai_transmit(&TxPacket, pdMS_TO_TICKS(100)) == ESP_OK) {
      Serial.print(" Q:OK ");
  } else {
      Serial.print("Q:FAIL ");
  }

  int alert_result=twai_read_alerts(&alerts_triggered, pdMS_TO_TICKS(100));    // read stats
  if(alert_result==ESP_OK){
      Serial.print("AR:OK ");
      if(alerts_triggered & TWAI_ALERT_TX_SUCCESS){
          Serial.print("TX:OK ");
      } else {
          Serial.print("TX:FAIL ");
      }
  } else {
    Serial.print("AR:FAIL:");
    if(alert_result==ESP_ERR_INVALID_ARG){
      Serial.print("INV_ARG");
    }
    if(alert_result==ESP_ERR_INVALID_STATE){
      Serial.print("INV_STATE");
    }
    if(alert_result==ESP_ERR_TIMEOUT){
      Serial.print("TIMEOUT");
    }
  }
}

void prepareMultiPacket(){
  // clear chararray[16][8]
  for(int i=0; i<32; i++){          // clears the packet buffer
    for(int j=0; j<8; j++){
      chararray[i][j]=0x00;
    }
  }
  chararray[0][0]=0x10;
  int bytecounter=0, packetcounter=0;
  for(int i=0;i<32 && bytecounter<bytes_processed; i++){     // fill the rest of the packets with data, skipping chararray[]
    for(int j=1;j<8 && bytecounter<bytes_processed;j++){
      chararray[i][j]=utf16chars[bytecounter];                              // read split UTF-16 chars into the array byte by byte
      bytecounter++;
      //Serial.printf("Processing message: Packet i=%d, byte j=%d, byte number=%d out of %d \n", i, j, bytecounter, bytes_processed);
    }
    if(i!=0){   // 1st packe requires 0x10;
      chararray[i][0]=0x21+packetcounter;                                     // label consecutive packets appropriately, rolls back to 0x20 in case of long messages
      if(chararray[i][0]==0x2F){
        packetcounter=-1;
      } else {
        packetcounter++;
      }
    }
  }
}

void sendMultiPacket(){     // send preamble, set the switch to wait for ACK from 2C1, main loop shall decide when to send the following data
  sendPacketSerial(0x6C1, chararray[0]);
  sendPacket(0x6C1, chararray[0]);
  can_MessageReady=1;
}

void sendMultiPacketData(){   // only executed after the display acknowledges the packet
  for(int i=1;(i<32) && (chararray[i][0]!=0x00);i++){                 // this loop will stop sending data once the next packet doesn't contain a label
    sendPacketSerial(0x6C1, chararray[i]);
    sendPacket(0x6C1, chararray[i]);
    delay(1);
  }
  Serial.println();
  can_MessageReady=0;
}

void sendPacketSerial(int id, char can_send_buffer[8]){         // debug, sends formatted CAN packets over serial ABC # B0 B1 B2 B3 B4 B5 B6 B7 with status
  Serial.println();
  Serial.printf("%03X # %02X %02X %02X %02X %02X %02X %02X %02X", id, can_send_buffer[0], can_send_buffer[1], can_send_buffer[2], can_send_buffer[3], can_send_buffer[4], can_send_buffer[5], can_send_buffer[6], can_send_buffer[7]);
}

void sendDummyPacket(){                   // debug, for ease of reading CAN bus dumps - lets me know which messages were generated by me
  char dummy_array[8];
  dummy_array[0]=0xFF;
  sendPacket(0x555, dummy_array, 1);
}