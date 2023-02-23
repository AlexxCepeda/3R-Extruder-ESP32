// Notas 

// https://www.laboratoriogluon.com/controlar-mosfets-de-potencia-irfz44n-con-3-3v/ configuracion para ventiladores
// https://randomnerdtutorials.com/esp32-load-cell-hx711/ celda de carga

#include <SPI.h>
#include <TFT_eSPI.h>
#include <analogWrite.h>
#include <Preferences.h>
#include "HX711.h"
#include "soc/rtc.h"

TaskHandle_t Task2;
Preferences preferences;

#define EEPROM_SIZE 64

// Pins para encoder
#define ENCODER_CLK 16
#define ENCODER_DT 17
#define ENCODER_SW 0

// Pin para ventilador 
#define FAN 5

// Pins para motores

//Define Pins for Motor 1
#define Motor1_stp 13 
#define Motor1_dir 12 

//Define Pins for Motor 2
#define Motor2_stp 14 
#define Motor2_dir 27 

//Define Pins for Motor 3
#define Motor3_stp 26 
#define Motor3_dir 25

// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 19; // 19
const int LOADCELL_SCK_PIN = 21; // 21

HX711 scale;
int reading;
int lastReading;

//CALIBRATION FACTOR
#define CALIBRATION_FACTOR 2102.054

//Set up timing for motors
unsigned long previousMotor1Time = millis();
unsigned long previousMotor2Time = millis();
unsigned long previousMotor3Time = millis();
long Motor1Interval = 0.2;
long Motor2Interval = 5;
long Motor3Interval = 1;

// Colors
#define TFT_GREY 0x5AEB

TFT_eSPI tft = TFT_eSPI();

// Variables
int counter = 0;
int motorDirection[] = {0, 0 , 0, 0}; // 0 para horario, 1 para antihorario
bool motorEstatus[] = {false, false, false, false}; // false para apagado, true para encendido
int motorRPM[] = {0, 0 , 0, 0}; // valor inicial 0 [0] = motor extrusor, [1] = motor rodillos, [2] = motor lineal, [3] = motor rotacional
//int stepsMotors[] = {STEP1,STEP2};
//int dirMotors[] = {DIR1,DIR2};
int linealSetUp[] = {0,0}; // {desplazamiento,velocidad}
int fanSetUp[] = {0,0}; // {estado, porcentaje} **ESTADO {0 APAGADO, 1 PRENDIDO}**
bool imprimir = true;
bool fanEstatus = false;

void loop2(void *parameter){
  for(;;){
    unsigned long currentMotor1Time = millis();
    unsigned long currentMotor2Time = millis();
    unsigned long currentMotor3Time = millis();
    Motor1Interval = motorRPM[0];
    Motor2Interval = motorRPM[1];
    //digitalWrite(Motor1_stp, LOW);
    //digitalWrite(Motor2_stp, LOW);
    //digitalWrite(Motor3_stp, LOW);
    if(motorEstatus[0]){
      digitalWrite(Motor1_stp, LOW);
      if(currentMotor1Time - previousMotor1Time > Motor1Interval){
        digitalWrite(Motor1_stp, HIGH);
        previousMotor1Time = currentMotor1Time;
      }  
    }
    if(motorEstatus[1]){
      digitalWrite(Motor2_stp, LOW);
      if(currentMotor2Time - previousMotor2Time > Motor2Interval){
        digitalWrite(Motor2_stp, HIGH);
        previousMotor2Time = currentMotor2Time;
      }
    }
    if(motorEstatus[2]){
      digitalWrite(Motor3_stp, LOW);
      if(currentMotor3Time - previousMotor3Time > Motor3Interval){
        digitalWrite(Motor3_stp, HIGH);
        previousMotor3Time = currentMotor3Time;
      }
    }
    (fanEstatus) ? analogWrite(FAN, map(fanSetUp[1],0, 100,255,0)) : analogWrite(FAN, map(100,0, 100,0,255));
    delay(1);
  }
  vTaskDelay(10);
}

void readEncoder()
{
  int dtValue = digitalRead(ENCODER_CLK);

  static unsigned long ultimaInterrupcion = 0; // variable static con ultimo valor de tiempo de interrupcion
  unsigned long tiempoInterrupcion = millis(); // variable almacena valor de func. millis

  if (tiempoInterrupcion - ultimaInterrupcion > 150)
  { // rutina antirebote desestima pulsos menores a 150 mseg.
    if (dtValue == HIGH) counter++; // Clockwise
    if (dtValue == LOW) counter--; // Counterclockwise
    ultimaInterrupcion = tiempoInterrupcion; // guarda valor actualizado del tiempo
  }
}

void setup()
{    
  xTaskCreatePinnedToCore(loop2, "Task_2", 1000, NULL, 1, &Task2, 0);
  preferences.begin("my-extruder", false);
  rtc_clk_cpu_freq_set(RTC_CPU_FREQ_80M);
  
  // Obtenemos los valores de EEPROM guardados de la direccion
  motorDirection[0] = preferences.getInt("direccion1",0);
  motorDirection[1] = preferences.getInt("direccion2",0);
  motorDirection[2] = preferences.getInt("direccion3",0);
  motorDirection[3] = preferences.getInt("direccion4",0);
  
  // Obtenemos los valores de RPM deseadas guardados en EEPROM
  motorRPM[0] = preferences.getInt("RPM0",0);
  motorRPM[1] = preferences.getInt("RPM1",0);
  motorRPM[3] = preferences.getInt("RPM3",0);

  // Obtenemos los valores para el movimiento lineal
  linealSetUp[0] = preferences.getInt("desp",0); //distancia de desplazamiento
  linealSetUp[1] = preferences.getInt("despVel",0); //velocidad de desplazamiento

  // Obtenemos los valores para la ventilacion

  fanSetUp[1] = preferences.getInt("porc",0); //porcentaje de ventilacion
  
  pinMode(FAN,OUTPUT);
  digitalWrite(FAN,HIGH);
  
  pinMode(Motor1_stp, OUTPUT);
  pinMode(Motor1_dir, OUTPUT);

  pinMode(Motor2_stp, OUTPUT);
  pinMode(Motor2_dir, OUTPUT);

  pinMode(Motor3_stp, OUTPUT);
  pinMode(Motor3_dir, OUTPUT);

  digitalWrite(Motor1_dir, LOW);
  digitalWrite(Motor2_dir, LOW); 
  digitalWrite(Motor3_dir, LOW);
  // Initialize encoder pins
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  // Initialize interrupt for encoder
  attachInterrupt(ENCODER_DT, readEncoder, FALLING);

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(CALIBRATION_FACTOR);   
  scale.tare();  
  
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(35, 80);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextSize(3);
  tft.println("BIENVENIDO");
  tft.setCursor(115, 120);
  tft.println("A");
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(25, 160);
  tft.println("3R EXTRUDER");
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(35, 210);
  tft.println("Hecho en UPIITA");
  tft.setCursor(70, 240);
  tft.println("Enjoy :-)");
  delay(1000);
  tft.fillScreen(TFT_BLACK);
  tft.drawRect(0,0,240,320,TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
}

void deleteRectsMenuPrincipal(int row){
  int optionRect[] = {13,73,133,193,253};
  for(int i = 0; i < 5; i++){
    if(i == row){
      tft.drawRect(10,optionRect[row],215,35,TFT_NAVY);
      continue;
    }
    tft.drawRect(10,optionRect[i],215,35,TFT_BLACK);
  }
}

void deleteRectsMenuPreparar(int row){
  int optionRect[] = {60,100,140,180,220,260};
  for(int i = 0; i < 6; i++){
    if(i == row){
      tft.drawRect(10,optionRect[row],215,25,TFT_NAVY);
      continue;
    }
    tft.drawRect(10,optionRect[i],215,25,TFT_BLACK);
  }
}

void deleteRectsVariablesMotorRota(int row){
  int optionRect[] = {60,100,180};
  for(int i = 0; i < 3; i++){
    if(i == row){
      tft.drawRect(10,optionRect[row],215,25,TFT_NAVY);
      continue;
    }
    tft.drawRect(10,optionRect[i],215,25,TFT_BLACK);
  }
}

void deleteRectsFanSpeed(int row){
  int optionRect[] = {60,100};
  for(int i = 0; i < 2; i++){
    if(i == row){
      tft.drawRect(10,optionRect[row],215,25,TFT_NAVY);
      continue;
    }
    tft.drawRect(10,optionRect[i],215,25,TFT_BLACK);
  }
}


void deleteRectsMenuControl(int row){
  int optionRect[] = {45,85,125,165,205,245,285};
  for(int i = 0; i < 7; i++){
    if(i == row){
      tft.drawRect(10,optionRect[row],215,25,TFT_NAVY);
      continue;
    }
    tft.drawRect(10,optionRect[i],215,25,TFT_BLACK);
  }
}

void motorVariablesRota(int motor){
  counter = 0;
  while(true){
    if(counter >= 2) counter = 2;
    if(counter <= 0) counter = 0;
    tft.setCursor(20,65);
    tft.print("Regresar");
    tft.setCursor(20,105);
    tft.print("Sentido");
    tft.setCursor(60,145);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    (motorDirection[motor]==0) ? tft.print("  Horario"):tft.print("Antihorario");
    tft.setCursor(20,185);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print("Velocidad");
    tft.setCursor(90,225);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print(motorRPM[motor]);
    tft.setCursor(160,225);
    tft.print("RPM");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    //tft.print(" ");
    deleteRectsVariablesMotorRota(getCounter());
    if (!digitalRead(ENCODER_SW)) {
      if(getCounter() == 0){
        break;
      }else{
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        switch(getCounter()){
          case 1:
            delay(100);
            tft.drawRect(10,100,215,25,TFT_BLACK);
            (motorDirection[motor] == 0) ? counter = 0: counter = 1;
            while(true){
              if(counter >= 1) counter = 1;
              if(counter <= 0) counter = 0;
              switch(counter){
                case 0:
                  tft.setCursor(60,145);
                  tft.print("  Horario  ");
                  //motorDirection[motor] = 0;
                  break;
                case 1:
                  tft.setCursor(60,145);
                  tft.print("Antihorario");
                  //motorDirection[motor] = 1;
                  break;
              }
              if(!digitalRead(ENCODER_SW)){
                //lcd.clear();
                (counter) ? motorDirection[motor] = 1 : motorDirection[motor] = 0;
                delay(100);
                counter = 1;
                break;
              }
            }
            break;
          case 2: 
            delay(100);
            tft.drawRect(10,180,215,25,TFT_BLACK);
            counter = motorRPM[motor];
            while(true){
              //motorRPM[motor] = getCounter();
              //if(motorRPM[motor] <= 0){
              //  motorRPM[motor] = 0;
              //  counter = 0;
              //}
              if(counter < 0) counter = 0;
              tft.setCursor(90,225);
              tft.setTextColor(TFT_GREEN, TFT_BLACK);
              tft.print(getCounter());
              tft.print(" ");
              if(!digitalRead(ENCODER_SW)){
                motorRPM[motor] = getCounter();
                delay(100);
                counter = 2;
                break;
              }
            }
            break;
        }
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
      }
    }
  }
}

void motorLineal(){
  counter = 0;
  while(true){
    if(counter >= 2) counter = 2;
    if(counter <= 0) counter = 0;
    tft.setCursor(20,65);
    tft.print("Regresar");
    tft.setCursor(20,105);
    tft.print("Desplazamiento");
    tft.setCursor(90,145);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print(linealSetUp[0]);
    tft.setCursor(160,145);
    tft.print("mm");
    tft.setCursor(20,185);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print("Velocidad");
    tft.setCursor(90,225);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print(linealSetUp[1]);
    tft.setCursor(160,225);
    tft.print("mm/s");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    deleteRectsVariablesMotorRota(getCounter());
    if (!digitalRead(ENCODER_SW)) {
      if(getCounter() == 0){
        break;
      }else{
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        switch(getCounter()){
          case 1:            
            delay(100);
            tft.drawRect(10,100,215,25,TFT_BLACK);
            counter = linealSetUp[0];
            while(true){
              linealSetUp[0] = getCounter();
              if(linealSetUp[0] <= 0){
                linealSetUp[0] = 0;
                counter = 0;
              }
              tft.setCursor(90,145);
              tft.setTextColor(TFT_GREEN, TFT_BLACK);
              tft.print(linealSetUp[0]);
              tft.print(" ");
              if(!digitalRead(ENCODER_SW)){
                delay(100);
                counter = 1;
                break;
              }
            }
            break;
          case 2: 
            delay(100);
            tft.drawRect(10,180,215,25,TFT_BLACK);
            counter = linealSetUp[1];
            while(true){
              linealSetUp[1] = getCounter();
              if(linealSetUp[1]<= 0){
                linealSetUp[1] = 0;
                counter = 0;
              }
              tft.setCursor(90,225);
              tft.setTextColor(TFT_GREEN, TFT_BLACK);
              tft.print(linealSetUp[1]);
              tft.print(" ");
              if(!digitalRead(ENCODER_SW)){
                delay(100);
                counter = 2;
                break;
              }
            }
            break;
        }
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
      }
    }
  }
}

void fanSpeed()
{
  counter = 0;
  while (true)
  {
    if (counter >= 1) counter = 1;
    if (counter <= 0) counter = 0;
    tft.setCursor(20, 65);
    tft.print("Regresar");
    tft.setCursor(20, 105);
    tft.print("Porcentaje");
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(105, 155);
    tft.print(fanSetUp[1]);
    tft.print(" % ");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    deleteRectsFanSpeed(getCounter());
    if (!digitalRead(ENCODER_SW))
    {
      if (getCounter() == 0)
      {
        break;
      }
      else
      {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawRect(10, 100, 215, 25, TFT_BLACK);
        counter = fanSetUp[1];
        delay(100);
        while (true)
        {
          //fanSetUp[1] = getCounter();
          if(getCounter() < 0) counter = 0;
          if(getCounter() >= 100) counter = 100;
          tft.setCursor(105, 155);
          tft.print(getCounter());
          tft.print(" % ");
          if (digitalRead(ENCODER_SW) == LOW)
          {
            fanSetUp[1] = getCounter();
            delay(100);
            counter = 1;
            break;
          }
        }
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
      }
    }
  }
}


void menuPrincipal(){
  tft.setTextSize(3);
  tft.setCursor(20,20);
  tft.print("Info");
  tft.setCursor(20,80);
  tft.print("Preparar");
  tft.setCursor(20,140);
  tft.print("Control");
  tft.setCursor(20,200);
  tft.print("Memoria");
  tft.setCursor(20,260);
  tft.print("Acerca de");
}

void opcionesPreparar(){
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0,0,240,30, TFT_NAVY);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(75,6);
  tft.print("Preparar");
  tft.setCursor(20,65);
  tft.print("Regresar");
  tft.setCursor(20,105);
  tft.print("Motor extrusor");
  tft.setCursor(20,145);
  tft.print("Motor rodillos");
  tft.setCursor(20,185);
  tft.print("Motor lineal");
  tft.setCursor(20,225);
  tft.print("Motor rotacional");
  tft.setCursor(20,265);
  tft.print("Ventilacion");
  imprimir = false;
}

void menuPreparar(){
  switch(getCounter()){
    case 0: break;
    case 1:
      tft.fillScreen(TFT_BLACK);
      tft.fillRect(0,0,240,30, TFT_NAVY);
      tft.setCursor(40,6);
      tft.print("Motor extrusor");
      while(true){
        motorVariablesRota(0);
        if(getCounter() == 0){
          delay(100);
          counter = 1;
          break; 
        }
      }break;
    case 2:
      tft.fillScreen(TFT_BLACK);
      tft.fillRect(0,0,240,30, TFT_NAVY);
      tft.setCursor(40,6);
      tft.print("Motor rodillos");
      while(true){
        motorVariablesRota(1);
        if(getCounter() == 0){
          delay(100);
          counter = 2;
          break; 
        }
      }break;
    case 3:
      tft.fillScreen(TFT_BLACK);
      tft.fillRect(0,0,240,30, TFT_NAVY);
      tft.setCursor(50,6);
      tft.print("Motor lineal");
      while(true){
        motorLineal();
        if(!digitalRead(ENCODER_SW)) {
          delay(100);
          counter = 3;
          break;
        }
      }break;
    case 4:
      tft.fillScreen(TFT_BLACK);
      tft.fillRect(0,0,240,30, TFT_NAVY);
      tft.setCursor(25,6);
      tft.print("Motor rotacional");
      while(true){
        motorVariablesRota(3);
        if(getCounter() == 0){
          delay(100);
          counter = 4;
          break; 
        }
      }break;
    case 5:
      tft.fillScreen(TFT_BLACK);
      tft.fillRect(0,0,240,30, TFT_NAVY);
      tft.setCursor(60,6);
      tft.print("Ventilacion");
      while(true){
        fanSpeed();
        if(getCounter() == 0){
          delay(100);
          counter = 5;
          break; 
        }
      }break;
  }  
}

void opcionesControl(){
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0,0,240,30, TFT_NAVY);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(80,6);
  tft.print("Control");
  tft.setCursor(20,50);
  tft.print("Regresar");
  tft.setCursor(20,90);
  (motorEstatus[0]) ? tft.print("Detener extrusor"): tft.print("Iniciar extrusor");
  tft.setCursor(20,130);
  (motorEstatus[1]) ? tft.print("Detener rodillos"): tft.print("Iniciar rodillos");
  tft.setCursor(20,170);
  (motorEstatus[2]) ? tft.print("Detener lineal"): tft.print("Iniciar lineal");
  tft.setCursor(20,210);
  (motorEstatus[3]) ? tft.print("Detener rotac"): tft.print("Iniciar rotac");
  tft.setCursor(20,250);
  (fanEstatus) ? tft.print("Detener vent"): tft.print("Iniciar vent");
  tft.setCursor(20,290);
  tft.print("Tarar bascula");
  imprimir = false;
}

void opcionesMemoria(){
  //tft.fillScreen(TFT_BLACK);
  tft.fillRect(0,0,240,30, TFT_NAVY);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(80,6);
  tft.print("Memoria");
  tft.setCursor(20,65);
  tft.print("Regresar");
  tft.setCursor(20,105);
  tft.print("Actualizar datos");
  tft.setCursor(20,145);
  tft.print("Eliminar datos");
  imprimir = false;
}

void desplegarInformacion(){
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0,0,240,30, TFT_NAVY);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(55,6);
  tft.print("Informacion");
  tft.setCursor(20,50);
  tft.print("M1:");
  tft.setCursor(20,90);
  tft.print("M2:");
  tft.setCursor(20,130);
  tft.print("M3:");
  tft.setCursor(20,170);
  tft.print("M4:");
  tft.setCursor(20,210);
  tft.print("Ventilacion:");
  tft.setCursor(20,250);
  tft.print("Diametro:");
  tft.setCursor(20,290);
  tft.print("Peso:");
  tft.setTextColor(TFT_GREEN);
  tft.setCursor(80,50);
  (motorEstatus[0])? tft.print("ON ") : tft.print("OFF ");
  tft.print(motorRPM[0]); tft.print(" RPM");
  (!motorDirection[0]) ? tft.print(" H"): tft.print(" AH");
  tft.setCursor(80,90);
  (motorEstatus[1])? tft.print("ON ") : tft.print("OFF ");
  tft.print(motorRPM[1]); tft.print(" RPM");
  (!motorDirection[1]) ? tft.print(" H"): tft.print(" AH");
  tft.setCursor(60,130);
  (motorEstatus[2])? tft.print("ON ") : tft.print("OFF ");
  tft.print(linealSetUp[0]); tft.print("mm ");
  tft.print(linealSetUp[1]); tft.print("mm/s ");
  tft.setCursor(80,170);
  (motorEstatus[3])? tft.print("ON ") : tft.print("OFF ");
  tft.print(motorRPM[3]); tft.print(" RPM");
  (!motorDirection[3]) ? tft.print(" H"): tft.print(" AH");
  tft.setCursor(170,210); tft.print(fanSetUp[1]); tft.print(" % ");
  tft.setTextColor(TFT_WHITE);
  imprimir = false;
}

int getCounter()
{
  int result;
  noInterrupts();
  result = counter;
  interrupts();
  return result;
}

void resetCounter()
{
  noInterrupts();
  counter = 0;
  interrupts();
}

void loop()
{    
  menuPrincipal();
  while(true){
    if(getCounter() >= 4) counter = 4;
    if(getCounter() <= 0) counter = 0;
    deleteRectsMenuPrincipal(getCounter());
    if (!digitalRead(ENCODER_SW)) {
      tft.fillScreen(TFT_BLACK);
      delay(100);
      break;
    }
  }
  /*******FIN WHILE(true)*******/
  switch(getCounter()){
    case 0:
      while(true){
        if(imprimir) desplegarInformacion();
        if (scale.wait_ready_timeout(200)){
          reading = round(scale.get_units());
          if(reading < 0) reading = 0;
        }
        tft.setCursor(120,290);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.print(reading);
        tft.print("  gr  ");
        if (!digitalRead(ENCODER_SW)) {
          tft.setTextColor(TFT_WHITE);
          tft.fillScreen(TFT_BLACK);
          delay(100);
          counter = 0;
          imprimir = true;
          break;
        }
    }break;
    case 1:
      counter = 0;
      while(true){
        if(imprimir) opcionesPreparar();
        if(getCounter() >= 5) counter = 5;
        if(getCounter() <= 0) counter = 0;
        deleteRectsMenuPreparar(getCounter());
        if (!digitalRead(ENCODER_SW)) {
          if(getCounter() == 0){
            tft.fillScreen(TFT_BLACK);
            delay(100);
            counter = 1;
            imprimir = true;
            break;
          }else{
            delay(100);
            menuPreparar();
            imprimir = true;
          }
        }
      }break;
    case 2:
      counter = 0;
      while(true){
        if(imprimir) opcionesControl();
        if(getCounter() >= 6) counter = 6;
        if(getCounter() <= 0) counter = 0;
        deleteRectsMenuControl(getCounter());
        if (!digitalRead(ENCODER_SW)) {
          delay(100);
          imprimir = true;
          if(getCounter() == 0){
            tft.fillScreen(TFT_BLACK);
            counter = 2;
            break;  
          }
          if(getCounter() == 1) motorEstatus[0] = !motorEstatus[0];
          if(getCounter() == 2) motorEstatus[1] = !motorEstatus[1];
          if(getCounter() == 3) motorEstatus[2] = !motorEstatus[2];
          if(getCounter() == 4) motorEstatus[3] = !motorEstatus[3];
          if(getCounter() == 5) fanEstatus = !fanEstatus;
          if(getCounter() == 6){ 
            scale.tare();
            tft.fillScreen(TFT_BLACK);
            counter = 2;
            break;
          }
        }
      }break;
    case 3:
      counter = 0;
      while(true){
        if(imprimir) opcionesMemoria();
        if(getCounter() >= 2) counter = 2;
        if(getCounter() <= 0) counter = 0;
        deleteRectsMenuPreparar(getCounter());
        if (!digitalRead(ENCODER_SW)) {
          delay(100);
          imprimir = true;
          if(getCounter() == 0){
            tft.fillScreen(TFT_BLACK);
            counter = 3;
            break;
          }
          if(getCounter() == 1){
              preferences.putInt("direccion1", motorDirection[0]);
              preferences.putInt("direccion2", motorDirection[1]);
              preferences.putInt("direccion3", motorDirection[2]);
              preferences.putInt("direccion4", motorDirection[3]);
              preferences.putInt("RPM0",motorRPM[0]);
              preferences.putInt("RPM1",motorRPM[1] );
              preferences.putInt("RPM3",motorRPM[3]);
              preferences.putInt("desp",linealSetUp[0]); //distancia de desplazamiento
              preferences.putInt("despVel",linealSetUp[1]); //velocidad de desplazamiento
              preferences.putInt("porc",fanSetUp[1]); //porcentaje de ventilacion

              tft.setTextColor(TFT_NAVY, TFT_BLACK);
              tft.setCursor(80,240);
              tft.print("Hecho :)");
              tft.setTextColor(TFT_WHITE, TFT_BLACK);

          }
        }
      }break;
    case 4:
      tft.fillScreen(TFT_BLACK);
      tft.fillRect(0,0,240,30, TFT_NAVY);
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(2);
      tft.setCursor(70,6);
      tft.print("Acerca de");
      tft.setCursor(60,100);
      tft.print("3R EXTRUDER");
      tft.setCursor(95,135);
      tft.print("v1.0");
      tft.setCursor(60,170);
      tft.print("UPIITA-IPN");
      tft.setCursor(20,230);
      tft.print("Contacto:");
      tft.setTextSize(1);
      tft.setCursor(20,260);
      tft.println("acepeda1800@alumno.ipn.mx");
      tft.setCursor(20,275);
      tft.println("asalas1803@alumno.ipn.mx");
      while(true){
            if (!digitalRead(ENCODER_SW)) {
            tft.fillScreen(TFT_BLACK);
            delay(100);
            counter = 4;
            break;
          }
      }break;
  }
}
