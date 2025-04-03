#include "Arduino.h"
#include "RP2040_PIO_USB_Host.h" // depends on https://github.com/sekigon-gonnoc/Pico-PIO-USB.git#0.5.3

// look at https://wiki.osdev.org/USB_Human_Interface_Devices for HID report format for both keyboard and mouse

// does not contain exmaple for setting keyboard leds, look at RP2040_PIO_USB_Host.h. Pretty self explanatory

// also I want to note that I am using the earle-philhower Arduino core: https://arduino-pico.readthedocs.io
// so your implementation for using the second core probably looks different

#define PICO_ID "keyboard_anomaly"
UnityPicoComms comms(PICO_ID);

#define USB_DP_PIN 8 // D- pin must be one more than DP, so must use pin 9 in this case
bool _keyboardUpdated = false;
bool _moueUpdated = false;

#define MOUSE_DATA_SIZE 5
uint8_t mouseData[MOUSE_DATA_SIZE];

void setup(){
  Serial.begin(115200);
}

void printKeyboardHID();
void printMouseHID();

void loop(){
    if(_keyboardUpdated){
        printKeyboardHID();
        // do whatever you want to do with the HID data here
        // data accessible via USB_Keyboard.HID_Data[] array, size is 7 bytes
        // normally HID report size is 8 bytes, but second byte is reserved and normally 0, so I discarded for my purposes
        // look at receiveAndProcessKeyboardHIDReport() function to change this
        _keyboardUpdated = false;
    }
    if(_moueUpdated){
      printMouseHID();
      // do whatever you want to do with the mouse data here
      // data accessible via USB_Mouse.data[] array, size is 5 bytes (or mouseData, not sure why I set it up this way?)
      // more some helper functions are in the PIO_USB_Mouse class
      
    }
}

void setup1(){
    USB_Keyboard.begin(USB_DP_PIN); // USB_Keyboard is declared in RP2040_PIO_USB_Host.h
    USB_Mouse.begin(USB_DP_PIN, &mouseData, MOUSE_DATA_SIZE); // USB_Mouse is declared in RP2040_PIO_USB_Host.h
}

void loop1(){ // USB device loop, must be run on dedicated core
  if(USB_Keyboard.update()){
    _keyboardUpdated = true;
  }
  if(USB_Mouse.update()){
    _moueUpdated = true;
  }

}


void printKeyboardHID(){
    Serial.print("Keyboard HID report: ");
    for (uint16_t i = 0; i < 7; i++) {
        printf("0x%02X ", USB_Keyboard.HID_Data[i]);
    }
    Serial.println();
}

void printMouseHID(){
  Serial.print("Mouse HID report: ");
  for (uint16_t i = 0; i < MOUSE_DATA_SIZE; i++) {
      printf("0x%02X ", USB_Mouse.HID_Data[i]);
  }
  Serial.println();
}