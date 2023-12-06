void sendPacket(int id, char can_send_buffer[8], int dlc);              // sendPacket constructor

void canReceive(){            // process CAN message, logical filter based on the CAN ID
  if(twai_receive(&RxMessage, pdMS_TO_TICKS(100)==ESP_OK)){
    if(!RxMessage.rtr){
      //Serial.printf("CAN: Received packet with id %X", CAN.packetId());
      switch (RxMessage.identifier){
        case 0x206: canDecodeWheel();
                    break;
        case 0x208: canDecodeAC();
                    break;
        case 0x6c1: canCheckDisplay();
                    break;
        default: break;
      }
    }
  }
}

void canDecodeWheel(){                                  // reads steering wheel buttons for next/prev A2DP control
  Serial.println("CAN: Decoding wheel buttons");
  if(RxMessage.data[0]==0x0 && bt_connected){	          // released have these buttons work only when bluetooth is connected
    switch(RxMessage.data[1]){
      case 0x81:  if(bt_audio_playing){                 // upper left button (box with waves)
                    a2dp_sink.pause(); 
                  } else {
                    a2dp_sink.play();
                  }
                  break;
      case 0x91:  a2dp_sink.next();                     // upper right button (arrow up)
                  break;
      case 0x92:  a2dp_sink.previous();                 // lower right button (arrow down)
                  break;
      default:    break;
    }
	}
}

void canDecodeAC(){                                       // turns off AC compressor (ECO mode) with a single knob press
  Serial.println("CAN: Decoding AC buttons");
  if((RxMessage.data[0]==0x0) && (RxMessage.data[1]==0x17) && (RxMessage.data[2]<0x30)){	// knob held for short time
    int ac_dlc=3, button_delay=200;
    delay(button_delay);
    //down once
    char ac_buffer[8]={0x08, 0x16, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};                           // todo optimize this garbage
    sendPacket(0x208, ac_buffer, ac_dlc);
    sendDummyPacket();
    delay(button_delay);
    //push button
    ac_buffer[0]=0x01;
    ac_buffer[1]=0x17;
    ac_buffer[2]=0x00;
    sendPacket(0x208, ac_buffer, ac_dlc);
    sendDummyPacket();
    delay(button_delay);
    //release button with a time constant
    ac_buffer[0]=0x00;
    ac_buffer[2]=0x10;
    sendPacket(0x208, ac_buffer, ac_dlc);
    sendDummyPacket();
    delay(button_delay);
    //turn down twice
    ac_buffer[0]=0x08;
    ac_buffer[1]=0x16;
    ac_buffer[2]=0x01;
    sendPacket(0x208, ac_buffer, ac_dlc);
    sendDummyPacket();
    delay(button_delay);
    sendPacket(0x208, ac_buffer, ac_dlc);
    sendDummyPacket();
    delay(button_delay);
    //push button
    ac_buffer[0]=0x01;
    ac_buffer[1]=0x17;
    ac_buffer[2]=0x00;
    sendPacket(0x208, ac_buffer, ac_dlc);
    sendDummyPacket();
    delay(button_delay);
    //release button with a time constant
    ac_buffer[0]=0x00;
    ac_buffer[2]=0x10;
    sendPacket(0x208, ac_buffer, ac_dlc);
    sendDummyPacket();
  }
}

void canCheckDisplay(){                             // overwrite any incoming AUX message, needs further checking whether said frame is AUX-related, now intercepts all display calls
  if(md_autoupdate && bt_audio_playing){            // don't bother checking the data if there's no need to update the display
    if(RxMessage.data[0]==0x10){                    // this should be done better, ie calculate packetCount=data[1]/7 and then wait for the last packet, update the display then
      delay(50);
      sendMultiPacket();
    }
  }
}