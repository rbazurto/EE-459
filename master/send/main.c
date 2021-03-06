#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h> 
#include <stdlib.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include "../../RFM69/rfm69.h"
#include "../../serial_test/serial.h"

// define parameters
#define FOSC 9830400 // clock frequency
#define BAUD 9600 // for serial interface
#define MYUBRR 47

struct GPS{
    char buffer[100], index;
    int quality, satellites; 
    float latitude, longitude, altitude; 
};

// initialize the serial port. Takes in no parameters.
void gps_serial_init(uint8_t ubrr){
  UBRR0 = ubrr; // set the BAUD rate
  // UCSR0B |= (1 << TXEN0) | (1 << RXEN0) | (1 << RXCIE0); // turn on the transmitter
  UCSR0B |= (1 << TXEN0) | (1 << RXEN0);
  UCSR0C = (3 << UCSZ00); // set for async operation 8 data bits, one stop bit
}

char gps_serial_in(){
  while (!(UCSR0A & (1 << RXC0)));
  return UDR0;
}

void gps_serial_out(char ch){
  while((UCSR0A & (1 << UDRE0)) == 0);
  UDR0 = ch;
}

void serial_outputString (char* text){
    //char i; 
    int8_t i;
    for (i = 0; i < strlen(text); i++){
        gps_serial_out(text[i]);
    }
    gps_serial_out(0x0D);
    gps_serial_out(0x0A);
}

void convertGPPGA(struct GPS* gps){
    char* section;
    char* GPSString[14];
    //char i = 0;
    int8_t i = 0;
    section = strtok(gps->buffer, ",");
    while(section != NULL){
        GPSString[i++] = section;
        section = strtok(NULL, ",");
        //GPSString[i++] = section;
    }
    
    gps->quality = atoi(GPSString[5]);
    gps->satellites = atoi(GPSString[6]);
    gps->altitude = atof(GPSString[8]);

    //If data invalid, return NULL
    if (gps->quality == 0)
        return;

    //Parse Coordinates
    int degrees;
    float minutes;
    //Latitude
    char lat[3] = {GPSString[1][0], GPSString[1][1]};
    degrees = atoi(lat); 
    minutes = atof(&(GPSString[1][2])) / 60;
    if (strcmp(GPSString[2],"S"))
        gps->latitude = degrees + minutes;
    else
        gps->latitude = degrees + minutes;

    //Longitude
    char log[4] = {GPSString[3][0], GPSString[3][1], GPSString[3][2]};
    degrees = atoi(log); 
    minutes = atof(&(GPSString[3][3])) / 60;
    if (strcmp(GPSString[4],"W"))
        gps->longitude = (degrees + minutes) * -1;
    else
        gps->longitude = (degrees + minutes) * -1;
    return;
}

void readSerial(struct GPS* gps){
    char serialInput;
    int GPGGA = 0;
    while (!(GPGGA == 6 && serialInput == 0x0D)){
        serialInput = gps_serial_in(); 
        if(serialInput == 0x24 & GPGGA == 0) //$ Check
            GPGGA++;
        else if(serialInput == 0x47 && GPGGA == 1) //G Check
            GPGGA++;
        else if(serialInput == 0x50 && GPGGA == 2) //P Check
            GPGGA++;
        else if(serialInput == 0x47 && GPGGA == 3) //G Check
            GPGGA++;
        else if(serialInput == 0x47 && GPGGA == 4) //G Check
            GPGGA++;
        else if(serialInput == 0x41 && GPGGA == 5) //A Check
            GPGGA++;
        else if(GPGGA == 6){
            gps->buffer[gps->index++] = serialInput; 
        }
        else
            GPGGA = 0;
    }
    convertGPPGA(gps); 
    gps->index = 0; 
}

void stringConvert(float f, char *str, char size){
    //char pos = 0;
    int8_t pos = 0;
    char len;
    char* curr;
    int value;

    value = (int)f;
    itoa(value,str,10);

    if (f < 0){
        f *= -1;
        value *= -1;
    }

    len = strlen(str); 
    pos = len;
    str[pos++] = '.';

    while(pos < (size + len + 1)){
        f = f - (float)value;
        f *= 10;
        value = (int)f;
        itoa(value, curr,10);
        str[pos++] = *curr;
    }
 }

void printData(struct GPS* gps){
    char buffer[50];
    stringConvert(gps->latitude, buffer, 6);
    serial_outputString("Latitude: ");
    serial_outputString(buffer);

    stringConvert(gps->longitude, buffer, 6);
    serial_outputString("Longitude: ");
    serial_outputString(buffer); 
}

struct RFM69 radio; // radio object

void interruptInit(){
	DDRD &= ~(1 <<DDD2) ; 
    PORTD |= (1<<PORTD2); 
    EICRA |= (1<<ISC00) | (1<<ISC01); // set it for rising edge 
    EIMSK |= (1 << INT0); 
    sei();
    PCMSK0 |= 0x80;
}

int main(void){
	interruptInit(); // initialize interrupt
	gps_serial_init(MYUBRR);
	SPI_MasterInit(); // initialize SPI
	
	// initial GPS parameters
    //struct GPS gps;
	//gps.index = 0;
	
	// initial radio parameters
	radio.slaveSelectPin = 16;
    radio.currentMode = 0;
    radio.buffer_length = 0;
    radio.packet_sent = 0;
    
    // reset the RFM69
    DDRC |= (1 << DDC1);
    PORTC &= ~(1 << PC1); // set pin to LOW
    _delay_ms(100);
    PORTC |= (1 << PC1); // set pin to HIGH
    _delay_ms(100);
    
    // initialize the radio
    RFM_spiConfig(); // configure SPI for RFM69
    RFM_init(); // initialize RFM69

	// message that acts as a dummy 
	char msg[] = "Friend's Latitude: 32.0204 Friend's Longitude: -118.2891";
	char msgSize = sizeof(msg);
	
    while (1){
    	// send an indicator message to the receiver
    	RFM_send(msg, &radio.currentMode, msgSize);
 	}
    return 0;   /* never reached */
}

ISR(INT0_vect){
	// serial_outputString("interrupt");
	radio.receiveDataFlag = RFM_interruptHandler(radio, &radio.currentMode);
}
