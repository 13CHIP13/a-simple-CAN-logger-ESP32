//----------------------------------------------------------------------------------------
//  Board Check
//----------------------------------------------------------------------------------------

#ifndef ARDUINO_ARCH_ESP32
  #error "Select an ESP32 board"
#endif

//----------------------------------------------------------------------------------------
//   Include files
//----------------------------------------------------------------------------------------

#include <ACAN_ESP32.h>
#include "BluetoothSerial.h"
#include <EEPROM.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

TaskHandle_t TaskBT;

#define SIZE_FIFO_BUF_WORD 128 // размер FIFO буфера
typedef struct{
  uint32_t id; // самый старший бит признак стандартного или расширенного ID
  uint8_t rtr;
  uint8_t dlc;
  uint8_t dat[8];
}canFrame;
typedef struct{
  uint16_t count; // счётчик колличества элементов очереди
  uint16_t font ; // указатель головы очереди
  uint16_t back ; // указатель на следующий элемент хвоста для заполнения очереди, т.е. последний элемент + 1 по сути тойже головы при переполнении
  canFrame dat[SIZE_FIFO_BUF_WORD]; // максимальный размер очереди
} FIFO_RING_BUFFER_WORD;

void Clear_queue_word(FIFO_RING_BUFFER_WORD *queue_val);
uint16_t PutChar_word(FIFO_RING_BUFFER_WORD *queue_val, const canFrame sym);
canFrame GetChar_word(FIFO_RING_BUFFER_WORD *queue_val);

FIFO_RING_BUFFER_WORD canToBT;

BluetoothSerial SerialBT;
//----------------------------------------------------------------------------------------
//  ESP32 Desired Bit Rate
//----------------------------------------------------------------------------------------

static const uint32_t DESIRED_BIT_RATE = 500UL * 1000UL ; // 500 kb/s
const uint8_t gpioRxCan = GPIO_NUM_4;
const uint8_t gpioTxCan = GPIO_NUM_5;

bool filterFlag = false; // включить фильтр по filterID
uint32_t filterID=0;

const uint8_t sizeMassDisableId = 64;
uint32_t massDisableId[sizeMassDisableId]; // массив где храняться исключаемые адреса из приёма
uint8_t countDisableId = 0; // количество исключаемых адресов в данный момент
bool flagOnlyDecodVisial = false; // флаг вывода уже известных декодированых сообщений

//----------------------------------------------------------------------------------------
//   SETUP
//----------------------------------------------------------------------------------------

void setup () {
  // Start eeprom
  EEPROM.begin(sizeMassDisableId*4+1);
  countDisableId = EEPROM.read(0);
  if(countDisableId > sizeMassDisableId){
    countDisableId = 0;
    EEPROM.write(0, countDisableId);  // Запись данных
    EEPROM.commit();                     // Сохранение изменений
  }else{
    for(int i = 0; i < (sizeMassDisableId*4); i++){
      *((uint8_t *)massDisableId+i) = EEPROM.read(i+1); 
    }
  }
//--- Start serial
  Serial.begin (115200) ;
  delay (100) ;
//--- Configure ESP32 CAN
  Serial.println ("Configure ESP32 CAN") ;
  Serial.println ("");
  ACAN_ESP32_Settings settings (DESIRED_BIT_RATE);           // CAN bit rate
  settings.mRxPin = (gpio_num_t)gpioRxCan ; // Optional, default Tx pin is GPIO_NUM_4
  settings.mTxPin = (gpio_num_t)gpioTxCan; // Optional, default Rx pin is GPIO_NUM_5
  const uint32_t errorCode = ACAN_ESP32::can.begin (settings) ;
  if (errorCode == 0) {
    Serial.print ("Can start : ") ;
  }else{
    Serial.print ("Configuration error 0x") ;
    Serial.println (errorCode, HEX) ;
  }

  xTaskCreatePinnedToCore(
    Task1codeBT,   // Функция задачи 
    "Task1BT",     // Название задачи 
    50000,       // Размер стека задачи 
    NULL,        // Параметр задачи 
    2,           // Приоритет задачи (чем больше, тем важнее)
    &TaskBT,      // Идентификатор задачи, чтобы ее можно было отслеживать 
    1);          // Ядро для выполнения задачи (0) 

  Serial.println ("Setup end") ;
  Serial.println ("") ;
}
//----------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------
//   LOOP
//----------------------------------------------------------------------------------------
bool flagOnlyDecodVisialOld=false; // предыдущий статус
uint32_t countTime=0;
void loop() {
  CANMessage frame ;
  canFrame frameFIFO ;
  if ( countTime< millis ()) {
     countTime+= 2000 ;

  } 
  while (ACAN_ESP32::can.receive (frame)) {
    if(frame.ext){
      Serial.print("ext ");
      frameFIFO.id = 0x80000000;
    }else
      frameFIFO.id = 0;

    frameFIFO.id |= frame.id;
    frameFIFO.dlc = frame.len;

    if (frame.rtr) {
    // Remote transmission request, packet contains no data
      Serial.print("RTR ");
      frameFIFO.rtr = 1;
    }else
      frameFIFO.rtr = 0;

    Serial.print("id=0x");
    Serial.print(frame.id, HEX);

    Serial.print(" L=");
    Serial.print(frameFIFO.dlc);

    // only print packet data for non-RTR packets
    if(false == frame.rtr){
      Serial.print("   "); 
       
      uint8_t i = 0;
      for (int i=0; i < frame.len; i++) {
        Serial.print(frame.data[i], HEX); if((i+1) < frame.len) Serial.print(" ");
        frameFIFO.dat[i] = frame.data[i];
      }
      Serial.println();
    }

    Serial.println();
    decodeCan(frame.id,frame.data);
    // сюда вставим условие фильтра, чтоб очередь зря не заполнять
    if(false == flagOnlyDecodVisial && (frame.id & 0x7FFFFFFF) && frame.len && (false == filterFlag || (true == filterFlag) && (frame.id & 0x7FFFFFFF) == filterID)){
      bool flagOn = true;
      for(int i = 0; i < countDisableId; i++){
        if(frame.id == massDisableId[i]){
          flagOn = false;
          if(true == filterFlag && (frame.id & 0x7FFFFFFF) == filterID) flagOn = true;
          break;
        }
      }
      if(flagOn) PutChar_word(&canToBT, frameFIFO);
    }else if(flagOnlyDecodVisial){
      if(flagOnlyDecodVisialOld != flagOnlyDecodVisial)
        Clear_queue_word(&canToBT);
    }
    flagOnlyDecodVisialOld = flagOnlyDecodVisial;
  }
}
//----------------------------------------------------------------------------------------
