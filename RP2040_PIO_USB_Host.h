#ifndef _RP2040_PIO_USB_HOST_
#define _RP2040_PIO_USB_HOST_

#include "Arduino.h"
#include "pio_usb.h"
#include "Adafruit_TinyUSB.h"
#include "LibPrintf.h"


// This is assuming low speed USB devices, meaning HID report size should be a max of 8 bytes

// TODO -- clean up old logic, make it so we can have multiple mouse or keyboard instances instead of a single one declare in this file

// TODO -- figure out how to reset USB host. So far the only device that can cause the USB logic to hang
//         is the black and white keyboard. "Normal" keyboards/keypad/mice/barcode scanners seem to operate normally
//         cheap mouses also seem to make it hang

// TODO -- pass pointer to LED bit data to keyboard object so it automatically updates as needed


void printHIDReport(uint8_t dev_addr, uint8_t const *report, uint16_t len){
    Serial.print("HIDreport : ");
    for (uint16_t i = 0; i < len; i++) {
        printf("0x%02X ", report[i]);
    }
    Serial.println();
}

void receiveAndProcessMouseHIDReport(uint8_t dev_addr, uint8_t const *report, uint16_t len);
void receiveAndProcessKeyboardHIDReport(uint8_t dev_addr, uint8_t const *report, uint16_t len);
void updateKeyboardLEDs(uint8_t dev_addr);

enum USB_HID_TYPE_ENUM{
    HID_TYPE_OTHER,
    HID_TYPE_KEYBOARD = 1,
    HID_TYPE_MOUSE = 2,
};

const char* itfProtocolStrings[3] = {"other hid device", "keyboard", "mouse"};

struct DeviceStruct{
    uint8_t index; // or instance
    USB_HID_TYPE_ENUM itfProtocol = HID_TYPE_OTHER;
};

DeviceStruct connectedDevices[5]; // index is dev_addr

class PIO_USB_Host{
    public:
        void begin(uint8_t USB_DP_Pin, uint8_t _rhport = 0){ // D- pin must be one more than DP
            
            if(initialized){
                if(USB_DP_Pin != pio_cfg.pin_dp){
                    Serial.println("ERROR! Passed two different D+ pins to same USB device");
                }
                return;
            }
            rhport = _rhport;
            uint32_t cpu_hz = clock_get_hz(clk_sys);
            if (cpu_hz != 120000000UL && cpu_hz != 240000000UL) {
                while (1) {
                    printf("Error: CPU Clock = %lu, PIO USB require CPU clock must be multiple of 120 Mhz\r\n", cpu_hz);
                    printf("Change your CPU Clock to either 120 or 240 Mhz\r\n\n");
                    delay(5000);
                }
            }
            pio_cfg.pin_dp = USB_DP_Pin;
            // USBHost.configure_pio_usb(_rhport, &pio_cfg);
            // USBHost.begin(_rhport);
            tuh_configure(rhport, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);
            tuh_init(rhport);
            initialized = true;
            // note: pretty sure rhport (first parameter in two functions above) is not relevant with a single USB device
        }

        void update(){
            tuh_task();
        }

        void setDebugHID(bool _debugHID){
            debugHID = _debugHID;
            if(debugHID) Serial.println("Printing HID reports");
            else Serial.println("Not printing HID reports");
        }

        bool debugHID = false;

    public:
        tusb_desc_device_t desc_device;
        pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
        bool initialized = false;
        uint8_t rhport;
        
};

PIO_USB_Host USB_Device;

void print_device_descriptor(tuh_xfer_t* xfer);
void getProtocol(uint8_t dev_addr);



void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len) {
    // printf("dev_addr: %i, instance: %i, len: %i\n", dev_addr, instance, len);
    if(USB_Device.debugHID){
        printHIDReport(dev_addr, report, len);
    }
    if(connectedDevices[dev_addr].itfProtocol == HID_TYPE_KEYBOARD && len == 8){
        receiveAndProcessKeyboardHIDReport(dev_addr, report, len);
    }
    else if(connectedDevices[dev_addr].itfProtocol == HID_TYPE_MOUSE && (len == 3 || len == 6)){
        receiveAndProcessMouseHIDReport(dev_addr, report, len);
    }

    
    if(!tuh_hid_receive_report(dev_addr, connectedDevices[dev_addr].index)){
        Serial.println("Error, can't receive hid report?");
    }
}




class PIO_USB_Mouse{ 
    public:
        void begin(uint8_t USB_DP_Pin, uint8_t *_data, uint8_t _dataSize, uint16_t _screenWidthInPixels = 1920, uint16_t _screenHeightInPixels = 1080){
            data = _data;
            dataSize = _dataSize;
            screenWidthInPixels = _screenWidthInPixels;
            screenHeightInPixels = _screenHeightInPixels;
            xCoordinate = screenWidthInPixels / 2;
            yCoordinate = screenHeightInPixels / 2;
            USB_Device.begin(USB_DP_Pin);
        }

        void updateCursor(uint8_t click, int8_t horizontalMotion, int8_t verticalMotion){
            if(data == NULL || dataSize == 0) return;
            verticalMotion *= -1;

            data[0] = click;
            
            if(horizontalMotion > 0){
                xCoordinate = min(xCoordinate + horizontalMotion, screenWidthInPixels);
            }
            else{
                xCoordinate = max(xCoordinate + horizontalMotion, 0);
            }

            if(verticalMotion > 0){
                yCoordinate = min(yCoordinate + verticalMotion, screenHeightInPixels);
            }
            else{
                yCoordinate = max(yCoordinate + verticalMotion, 0);
            }
            updateCoordinateData();
        }

        void scroll(int8_t scrollDirection){
            if(!scrollDirection) return;
            scrollState = (scrollState + scrollDirection) % 16;
            data[0] = (data[0] & 0b00001111) + (scrollState << 4);
        }

        void setCoordinates(uint16_t x, uint16_t y){
            xCoordinate = x;
            yCoordinate = y;
            updateCoordinateData();
        }

        void updateCoordinateData(){
            data[1] = xCoordinate & 0xFF;
            data[2] = xCoordinate >> 8;
            data[3] = yCoordinate & 0xFF;
            data[4] = yCoordinate >> 8;
        }

        void printCoordinates(){
            printf("x: %i, y: %i\n", xCoordinate, yCoordinate);
        }

        bool update(){
            static uint8_t dataAtLastCheck[5];
            bool updated = false;
            for(int i = 0; i < 5; i++){
                if(dataAtLastCheck[i] != data[i]){
                    dataAtLastCheck[i] = data[i];
                    updated = true;
                }
            }
            return updated;
        }
    
    private:
        uint8_t* data = NULL;
        uint8_t dataSize = 0;
        uint16_t screenWidthInPixels;
        uint16_t screenHeightInPixels;
        int xCoordinate;
        int yCoordinate;
        uint8_t scrollState = 0;

};

PIO_USB_Mouse USB_Mouse;

void receiveAndProcessMouseHIDReport(uint8_t dev_addr, uint8_t const *report, uint16_t len){
    if(len == 3){
        USB_Mouse.updateCursor(report[0], report[1], report[2]);
    }
    else if(len == 6){
        USB_Mouse.updateCursor(report[0], report[1], report[3]);
        USB_Mouse.scroll(report[5]);
    }
}

uint8_t const HID_to_ASCII[128][2] =  { HID_KEYCODE_TO_ASCII };

class PIO_USB_Keyboard{
    public:
        uint8_t HID_Data[7]; // discard second (empty) byte
        bool updated = false;
        void begin(uint8_t USB_DP_Pin){
            USB_Device.begin(USB_DP_Pin);
        }

        void setDevAddr(uint8_t _daddr){
            daddr = _daddr;
        }

        bool update(){
            USB_Device.update();
            if(updated){
                updated = false;
                return true;
            }
            return false;
        }
        
        void updateKeyboardLEDs(uint8_t newState) // pass byte containing LED bits
        {
            newState &= 0b111;
            static uint8_t LEDState;
            if(newState == LEDState) return;
            LEDState = newState;
            tuh_hid_set_report(daddr, connectedDevices[daddr].index, 0, HID_REPORT_TYPE_OUTPUT, &LEDState, sizeof(LEDState));
            // tuh_hid_set_report(1, 0, 0, HID_REPORT_TYPE_OUTPUT, &LEDState, sizeof(LEDState));
        }

    private:
        uint8_t daddr;
        bool debugOn = false;
};

PIO_USB_Keyboard USB_Keyboard;

void receiveAndProcessKeyboardHIDReport(uint8_t dev_addr, uint8_t const *report, uint16_t len){
    USB_Keyboard.HID_Data[0] = report[0];
    for(int i = 1; i < 7; i++){
        USB_Keyboard.HID_Data[i] = report[i+1];
    }
    USB_Keyboard.updated = true;
}

void tuh_mount_cb(uint8_t dev_addr){
    Serial.println("connected?");
    for(int i = 0; i < 8; i++){
        uint8_t _protocol = tuh_hid_interface_protocol(dev_addr, i);
        if(_protocol != 0){
            connectedDevices[dev_addr].index = i;
            connectedDevices[dev_addr].itfProtocol = (USB_HID_TYPE_ENUM)_protocol;
            if(connectedDevices[dev_addr].itfProtocol == HID_TYPE_KEYBOARD){
                USB_Keyboard.setDevAddr(dev_addr);
            }
            break;            
        }
    }
    
    printf("%s connected w/ dev_addr %i and index %i\n", itfProtocolStrings[connectedDevices[dev_addr].itfProtocol], dev_addr, connectedDevices[dev_addr].index);
    
    // tuh_descriptor_get_device(dev_addr, &USB_Device.desc_device, 18, print_device_descriptor, 0);
    if(!tuh_hid_receive_report(dev_addr, connectedDevices[dev_addr].index)){
        Serial.println("Error, can't receive hid report?");
    }

    if(connectedDevices[dev_addr].itfProtocol == HID_TYPE_KEYBOARD){
        // old_updateKeyboardLEDs(dev_addr);
    }
}

void tuh_umount_cb(uint8_t dev_addr){
    printf("%s with dev_addr %i unmounted\n", itfProtocolStrings[connectedDevices[dev_addr].itfProtocol], dev_addr);
}

void print_device_descriptor(tuh_xfer_t* xfer)
{
  if ( XFER_RESULT_SUCCESS != xfer->result )
  {
    printf("Failed to get device descriptor\r\n");
    return;
  }

  uint8_t const daddr = xfer->daddr;

  printf("Device %u: ID %04x:%04x\r\n", daddr, USB_Device.desc_device.idVendor, USB_Device.desc_device.idProduct);
  printf("Device Descriptor:\r\n");
  printf("  bLength             %u\r\n"     , USB_Device.desc_device.bLength);
  printf("  bDescriptorType     %u\r\n"     , USB_Device.desc_device.bDescriptorType);
  printf("  bcdUSB              %04x\r\n"   , USB_Device.desc_device.bcdUSB);
  printf("  bDeviceClass        %u\r\n"     , USB_Device.desc_device.bDeviceClass);
  printf("  bDeviceSubClass     %u\r\n"     , USB_Device.desc_device.bDeviceSubClass);
  printf("  bDeviceProtocol     %u\r\n"     , USB_Device.desc_device.bDeviceProtocol);
  printf("  bMaxPacketSize0     %u\r\n"     , USB_Device.desc_device.bMaxPacketSize0);
  printf("  idVendor            0x%04x\r\n" , USB_Device.desc_device.idVendor);
  printf("  idProduct           0x%04x\r\n" , USB_Device.desc_device.idProduct);
  printf("  bcdDevice           %04x\r\n"   , USB_Device.desc_device.bcdDevice);

  // Get String descriptor using Sync API
//   uint16_t temp_buf[128];

//   Serial.printf("  iManufacturer       %u     "     , desc_device.iManufacturer);
//   if (XFER_RESULT_SUCCESS == tuh_descriptor_get_manufacturer_string_sync(daddr, LANGUAGE_ID, temp_buf, sizeof(temp_buf)) )
//   {
//     print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf));
//   }
//   Serial.printf("\r\n");

//   Serial.printf("  iProduct            %u     "     , desc_device.iProduct);
//   if (XFER_RESULT_SUCCESS == tuh_descriptor_get_product_string_sync(daddr, LANGUAGE_ID, temp_buf, sizeof(temp_buf)))
//   {
//     print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf));
//   }
//   Serial.printf("\r\n");

//   Serial.printf("  iSerialNumber       %u     "     , desc_device.iSerialNumber);
//   if (XFER_RESULT_SUCCESS == tuh_descriptor_get_serial_string_sync(daddr, LANGUAGE_ID, temp_buf, sizeof(temp_buf)))
//   {
//     print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf));
//   }
//   Serial.printf("\r\n");

//   Serial.printf("  bNumConfigurations  %u\r\n"     , desc_device.bNumConfigurations);
}

#endif