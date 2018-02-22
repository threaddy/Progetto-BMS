/***************************************************************
File: data_monitor.ino
Authors: F.Garbuglia, M.Unterhorst
Created: Feb 2018

Description:
Controls BMSino through external commands,
using Arduino's serial port

External Commands list:
- mVCELL A  gives the cell's voltage (A stands for All).
- TCELL A   gives the cell's temperature in °C (A stands for All).
- RBCELL A  returns the balancing status (1 stands for balance active
            0 for not active).
- SBCELL x  sets the balancing register (x is the 6-bit bitmask,
            i.e. 000101 for first and third cell balance).
- TBMS      returns the board temperature in °C.

***************************************************************/

#include <SPI.h>
#include <stdint.h>
#include <Arduino.h>
#include <stdio.h>
#include "AD7280.h"
#include "thermistor.h"
#include "current_sensor.h"
#include "ADserial.h"
#include "psu.h"


#ifndef PINS
#define _SS 10
#define _EN_PIN	5
#define _ONBOARD_NTC_PIN A4
#define _CURRENT_OUT_PIN A0
#define _CURRENT_REF_PIN A1
#define CELLS 6
#define CHANNELS 12
#define SEPARATOR ' '
#endif




long int startup_time;
AD7280 myAD;                  //1 AD class allocation
THERMISTOR res;              // 1 onboard resistor class allocation
THERMISTOR ntc;            // 6 cell resistor class allocation
CURRENT_SENSOR curr_val;


byte a;

int cell_current;
long int curr_time;
int16_t board_temp;


uint16_t adc_channel[CHANNELS];
uint16_t cell_temperatures[CELLS];
uint16_t cell_voltages[CELLS];
int battery_voltage_mv=0;


char received[50];
char command[25];
char value[25];
int value_int;
byte value_bin;
int balance_reg;
int l;
byte LED;






void setup() {
        pinMode(A5, OUTPUT);
        pinMode(_EN_PIN, INPUT);
        digitalWrite(_EN_PIN, HIGH);

        myAD.init(_SS);        //initialize slave AD on pin 10
        startup_time = millis();

        Serial.begin(115200);
        delay(1000);

        res.init(ONBOARD_NTC);     //initialize resistor as onboard ntc
        ntc.init(CELL_NTC);      //initialize resistor as cell ntc
}


void loop() {
        //led blink
        if(l % 100 == 0){
          LED = LED ^ 1;
          digitalWrite(A5, LED);
        }
        l++;

        //Serial.print(Serial.available());
        if (Serial.available() > 0){

                int i,k = 0;
                int separator_found = 0;
                a = Serial.read();
                while (a != '\n') {
                        received[i]= a;
                        a = Serial.read();
                        if (received[i] == SEPARATOR){
                          separator_found = 1;
                        }
                        i++;
                }

                received[i] = '\n';


                if (separator_found == 1) {

                  i = 0;

                  while (received[i] != SEPARATOR){
                          command[i] = received[i];
                          i++;
                        }

                        command[i] = '\0';


                        k = 0;
                        i++;
                        while (received[i] != '\n') {
                          value[k] = received[i];
                          i++;
                          k++;

                        }
                        value[k]= '\0';
                }
                else if (separator_found == 0){

                  i = 0;
                  while (received[i] != '\n') {
                    command[i] = received[i];
                    i++;
                  }
                  command[i] = '\0';
                }

                // Serial.print(command);
                // Serial.print('\t');
                // Serial.print(SEPARATOR);
                // Serial.print('\t');

                // Serial.print('\n');

                sscanf(value, "%d", &value_int);        //convert value string to integer
                //Serial.print(value_int, DEC);

                myAD.read_all(CHANNELS, &adc_channel[0]);          //read all channels
                curr_time = millis()- startup_time;

                //separate voltages from temperatures
                for (i=0;i<CELLS; i++){
                cell_voltages[i]=(adc_channel[i]*0.976)+1000;
                battery_voltage_mv = battery_voltage_mv + cell_voltages[i] ;    //calc total voltage across the battery
                }
                k = 0;

                for (i=CHANNELS/2; i<CHANNELS/2+CELLS; i++){
                    cell_temperatures[k] = ntc.getTemperature(adc_channel[i]);        //INSERTTT   convert cell temperature to degrees
                    k++;
                }

                board_temp= res.getTemperature(analogRead(_ONBOARD_NTC_PIN));       //board temperature acquisition and convertion


                // TO PRINT VOLTAGES

                if (strcmp(command, "mVCELL") == 0) {
                        if (strcmp(value, "A")==0){
                            for (i=0; i< CHANNELS/2; i++ ) {
                                    Serial.print(cell_voltages[i], DEC);
                                    Serial.print(SEPARATOR);
                            }
                            Serial.print('\n');
                        }
                        else if ((value_int <= CELLS)&&(value_int > 0)) {
                            Serial.println(cell_voltages[value_int]);
                        }
                        else    Serial.println("ERROR");
                }


                //TO PRINT CELL TEMPERATURES

                else if (strcmp(command, "TCELL") == 0) {
                                if (strcmp(value, "A")==0){
                                        for (i=0; i< CELLS; i++ ) {
                                                Serial.print((cell_temperatures[i]), DEC);
                                                Serial.print(SEPARATOR);
                                        }
                                        Serial.print('\n');
                                }
                                else if ((value_int <= CELLS)&&(value_int > 0)) {
                                    Serial.println(cell_temperatures[value_int], DEC);
                                }
                        else    Serial.print("ERROR");
                }


                //TO PRINT BALANCING MOSFET STATUS

                else if (strcmp(command, "RBCELL") == 0) {
                        balance_reg = myAD.readreg(0, 0x14);            //read from balance register
                        Serial.print(balance_reg, BIN);
                        Serial.print('\n');
                }


                //TO ENABLE BALANCING FROM CELL BITMASK

                else if (strcmp(command, "SBCELL") == 0) {
                        //Serial.println(value);
                        int error=0;
                        value_bin= 0x00;
                        for (int i=0; value[i]!=NULL; i++){
                                if (value[i]== '1') {
                                        value_bin = value_bin | (0b100000 >> i);
                                }
                                else {
                                        if (value[i] != '0') error =1;
                                }
                        }
                        if (error == 1) Serial.println("WRONG CELL BALANCING BITMASK");
                        else myAD.balance_all(value_bin, 0 );
                }


                //TO PRINT PCB TEMPERATURE

                else if (strcmp(command, "TBMS") == 0) {
                        Serial.print(board_temp, DEC);
                        Serial.print('\n');
                }

//                else
//                  Serial.println("UNKNOWN COMMAND");
        }
        delay(10);
        return;
}
