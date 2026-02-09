void checkError(uint32_t errorCode, uint32_t bitRateOut){
  if (0 == errorCode ) {
    SerialBT.print ("BitRateSet to ") ;
    SerialBT.println (bitRateOut) ;
  }  else{
    SerialBT.print ("ErrorCode =  ") ;
    SerialBT.println (errorCode, HEX) ;
  }
}

/*
  Вывод уже расшифрованых значений
*/
void visial(){

}

void Task1codeBT( void * pvParameters ){
    portTickType xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    SerialBT.begin("ESP32test"); //Bluetooth device name
    uint8_t countRx=0;
    uint16_t timeOut = 5;
    for(;;){
      countRx = 0;
      if(flagOnlyDecodVisial) 
        timeOut = 500;
      else 
        timeOut = 5;

      if(canToBT.count){
        countRx++;
        canFrame frame = GetChar_word(&canToBT);
        if((frame.id & 0x7FFFFFFF) && frame.dlc && (false == filterFlag || (true == filterFlag) && (frame.id & 0x7FFFFFFF) == filterID)){
          if (frame.id & 0x80000000)
            SerialBT.print("e. ");   
          if(frame.rtr)
            SerialBT.print("RTR ");
          SerialBT.print("0x"); 

          // если std ID то дополняем нулями для выравнивания, ext не предполагается в моём случае
          if((frame.id & 0x80000000) == 0){
            if(frame.id < 0x10)         
              SerialBT.print("00");
            else if(frame.id < 0x100)         
              SerialBT.print("0");
          }
          frame.id &= 0x7FFFFFFF; // убрали индикатор расширенного ID           

          SerialBT.print(frame.id,HEX);
          SerialBT.print(" ");
          SerialBT.print(frame.dlc);
          SerialBT.print(" ");
          if (!frame.rtr && frame.dlc) {
            // only print packet data for non-RTR packets
            for (int i=0; i < frame.dlc; i++) {
              if(frame.dat[i] < 0x10) SerialBT.print("0");
              SerialBT.print(frame.dat[i], HEX);
              if((i+1) < frame.dlc) 
                SerialBT.print(" ");
            }
          }
          SerialBT.println("");
        }
        if(countRx >= 10) // просто магическое число
          vTaskDelayUntil( &xLastWakeTime, ( 1 / portTICK_RATE_MS ) ); // чтобы поток отдавал управление хоть как то если сильно засрали FIFO
      }

      if(flagOnlyDecodVisial) 
        SerialBT.println(outDecode());

      if(SerialBT.available()){
        char dat = SerialBT.read();

        switch(dat){
          // скорость CAN (B500 - 500 кбит/с, B250 - 250 кбит/с, B125 - 125 кбит/с и т.п.)
          case 'b': 
          case 'B':{
            uint32_t baudrate = SerialBT.parseInt();
            ACAN_ESP32_Settings settings (baudrate*1000);           // CAN bit rate            
            settings.mRxPin = (gpio_num_t)gpioRxCan ; // Optional, default Tx pin is GPIO_NUM_4
            settings.mTxPin = (gpio_num_t)gpioTxCan; // Optional, default Rx pin is GPIO_NUM_5
            checkError(ACAN_ESP32::can.begin (settings),baudrate*1000);
            break;
          }
          // фильтр, приём только одного ID (F0 - выключить фильтр, F1=34 - включить фильтр на ID=0x34), 
          //более приоритетная операция чем выключение ID из приёма, т.е. соответствующий ID пролезет на вывод если он выключен был опцией D1=xxxxx
          case 'f':
          case 'F':{ 
            char onOff = SerialBT.read();
            if( '0' == onOff ){
              filterFlag = false;
              SerialBT.println("Filter off");
            }else if('1' == onOff && SerialBT.read() == '='){                         
              String valStrim = SerialBT.readStringUntil('\n');
              valStrim = "0x" + valStrim;
              filterFlag = true;
              filterID = strtol(valStrim.c_str(),NULL,0);
              SerialBT.print("Filter on 0x"); SerialBT.println(filterID, HEX);
            }else
              SerialBT.println ("Error comand") ;
            break;
          }
          // выключить ID из приёма (D2 - все ID разрешены, D0=34 - выключить из приёма ID=0x34, D1=34 - включить приём ID=0x34, D9 - запрос включеных исключений), Можно выключить максимум 64 адресов, разделения стандартный или расширенный нету
          case 'd':
          case 'D':{ 
            char onOff = SerialBT.read();
            bool fl = false;
            if( '2' == onOff ){
              countDisableId = 0;
              SerialBT.println("All ID on rx");
              fl = true;
            }else if('0' == onOff && SerialBT.read() == '='){  
              if(countDisableId >= sizeMassDisableId){
                SerialBT.println("Disable ID mass full");  
              }else{                      
                String valStrim = SerialBT.readStringUntil('\n');
                valStrim = "0x" + valStrim;
                // пробегаем на наличие такого же ID в массиве
                uint32_t dId = strtol(valStrim.c_str(),NULL,0);
                bool flag = false;
                for(int i = 0; i < countDisableId; i++){
                  if(massDisableId[i] == dId) 
                    flag = true;
                }
                if(flag){
                  SerialBT.println("ID exists");
                }else{
                  massDisableId[countDisableId] = dId;
                  SerialBT.print("Add ID 0x"); SerialBT.println(dId, HEX);
                  countDisableId++;
                  fl = true;
                }
              }
            }else if('1' == onOff && SerialBT.read() == '='){  
              if(countDisableId == 0){
                SerialBT.println("Disable ID mass empty");  
              }else{                     
                String valStrim = SerialBT.readStringUntil('\n');
                valStrim = "0x" + valStrim;
                // пробегаем на наличие такого же ID в массиве
                uint32_t dId = strtol(valStrim.c_str(),NULL,0);
                bool flag = false;
                for(int i = 0; i < countDisableId; i++){
                  if(flag){
                    massDisableId[i-1] = massDisableId[i];  // на первом проходе флаг не установиться полюбому
                  }
                  if(massDisableId[i] == dId){ 
                    flag = true;
                  }
                }
                if(flag){
                  SerialBT.println("ID del");
                  countDisableId--;
                  fl = true;
                }else{
                  SerialBT.println("ID not find"); 
                }     
              }
            }else if( '9' == onOff ){
              if(0 == countDisableId)
                SerialBT.println("Disable ID mass empty");
              else{
                SerialBT.print("Number ID = "); SerialBT.println(countDisableId);
                SerialBT.println("List ID:");
                for(int i = 0; i < countDisableId; i++){
                  SerialBT.println(massDisableId[i], HEX);
                }
              }
            }else
              SerialBT.println ("Error comand") ;
            
            if(fl){
              EEPROM.write(0, countDisableId);  // Запись данных
              for(int i = 0; i < (sizeMassDisableId*4); i++){
                EEPROM.write(i+1, *((uint8_t *)massDisableId+i) );
              }
              EEPROM.commit();                 // Сохранение изменений
            }
            break;
          } 
          case 'v': // V1 - включить вывод декодидированых значений, V0 -выключить
          case 'V':{ 
            char onOff = SerialBT.read();
            bool fl = false;
            if( '0' == onOff ){
              flagOnlyDecodVisial = false;
              SerialBT.println ("Visial decode OFF") ;
            }else if( '1' == onOff ){ 
              flagOnlyDecodVisial = true;
              SerialBT.println ("Visial decode ON") ;
          }else 
              SerialBT.println ("Error comand") ;
            break;
          }
          default:
            SerialBT.println ("Error comand") ;
        }   
        while(SerialBT.available())
          SerialBT.read();
      }
      vTaskDelayUntil( &xLastWakeTime, ( timeOut / portTICK_RATE_MS ) ); // чтобы поток отдавал управление хоть как то
   }
}