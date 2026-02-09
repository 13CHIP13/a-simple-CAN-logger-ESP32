typedef struct {
  struct {
    uint16_t  rpm;   //частота вращения
    uint8_t   speed;  // скорость
    int8_t    temperWater; // температура воды
    int8_t    temperWater1; // температура воды второй метод
  } motor;          // ДВС
  struct {
    uint8_t   gear;   // текущая скорость АКПП, 0 - всё выключено, 1-6 передача вперёд, 7 - передача назад
    bool      gearFlag;  // флаг при переключения передач
    char      mode;      // режим: P, N, D, manual
    uint16_t  rpmR;  // частота вращения чегото, возможно вторичный вал или что то вроде этого ведомое колесо гидротрансформатора
  } akpp;           // АКППкакойто флаг при смене передач
  struct{
    double  angel; // угол руля 
    uint8_t speedW; // угловая скорость
  }eps; // руль
  struct{
    double outTempAir;  // наружная температура воздуха
  }climat;
  struct{
    double voltageABC; // напряжение АКБ
  }power;
} t_decodVal;
t_decodVal decodVal;

/*
  id - CAN_ID не различает стандартный или расширенный
  *b - указатель на массив байт из 8 штук
*/
void decodeCan(uint32_t id, uint8_t *b) {
  switch (id) {
    case 0x350:{
      decodVal.climat.outTempAir = (double)*(b + 3) * 0.5 - 40;
      break;
    }
    case 0x0A0:{
      decodVal.motor.temperWater1 = *(b + 1) - 40;
      break;
    }
    case 0x329:{
      decodVal.motor.temperWater = *(b + 1) * 0.75 - 48;
      break;
    }
    case 0x316:{
      decodVal.motor.rpm = (((uint16_t) *(b + 3) << 8) | (uint16_t) *(b + 2)) >> 2;
      decodVal.motor.speed = *(b + 6);
      break;
    }
    case 0x43F:{
      decodVal.akpp.rpmR = (((uint16_t) *(b + 6) << 8) | (uint16_t) *(b + 5)) >> 2;
      decodVal.akpp.gear = *b & 0x07;
      if (*b & 0x08)
        decodVal.akpp.gearFlag = true;
      else
        decodVal.akpp.gearFlag = false;

      switch(*(b+1) & 0x0F){
        case 0:  decodVal.akpp.mode = 'P';break;         
        case 7:  decodVal.akpp.mode = 'R';break;     
        case 6:  decodVal.akpp.mode = 'N';break; 
        case 5:  decodVal.akpp.mode = 'D';break;
        case 8:  decodVal.akpp.mode = 'S';break;
        default:
          decodVal.akpp.mode = 'E';
      }
      break;
    }
    case 0x2B0:{
      int16_t tmp = (((int16_t) *(b+1))<<8) | ((int16_t) *b);
      decodVal.eps.angel = (double)tmp * 0.1;
      decodVal.eps.speedW = *(b+2);
      break;
    }
    case 0x545:{
      int16_t tmp = (int16_t) *(b+3);
      decodVal.power.voltageABC = (double)tmp * 0.1;
      break;
    }
  }
}

String outDecode() {
  String mes;
  mes = "---------MOTOR---------\n";
  mes += "rpm="; mes += decodVal.motor.rpm; mes += "\n"; 
  mes += "speed="; mes += decodVal.motor.speed;  mes += "\n";
  mes += "tWater="; mes += decodVal.motor.temperWater;  mes += "\n";
  mes += "tWater1="; mes += decodVal.motor.temperWater1;  mes += "\n";
  
  mes += "----------AT----------\n";
  mes += "mode="; mes += String(decodVal.akpp.mode); mes += "\n";
  mes += "gear="; mes += (decodVal.akpp.gear < 7) ? String(decodVal.akpp.gear) : "R"; mes += "\n";
  mes += "gearFlag="; mes += (decodVal.akpp.gearFlag) ? "ON" : "OFF"; mes += "\n";
  mes += "rpmR="; mes += decodVal.akpp.rpmR; mes += "\n";

  mes += "----------EPS----------\n";
  mes += "angel="; mes += decodVal.eps.angel; mes += "\n";
  mes += "speedW="; mes += decodVal.eps.speedW; mes += "\n";

  mes += "--------ENERGE--------\n";
  mes += "volt="; mes += decodVal.power.voltageABC; mes += "\n";

  mes += "--------Climat--------\n";
  mes += "tAir="; mes += decodVal.climat.outTempAir; mes += "\n";

  return mes;
}