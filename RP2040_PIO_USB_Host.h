#ifndef _RP2040_PIO_USB_HOST_
#define _RP2040_PIO_USB_HOST_

#include "Arduino.h"
#include "pio_usb.h"
#include "Adafruit_TinyUSB.h"
#include "LibPrintf.h"


// This is assuming low speed USB devices, meaning HID report size should be a max of 8 bytes

// keyboard combos are not possible with current setup
// (could potentially add an additional leading byte with the latest modifier byte?)
// Shift works, because it makes 'a' into 'A', but ctrl would not change any
// characters being sent to Unity, I just have a designated (fake) ctrl ascii code


// TODO -- figure out how to reset USB host. So far the only device that causes the USB logic to hang
//         is the black and white keyboard. "Normal" keyboards/keypad/mice/barcode scanners seem to operate normally
//         cheap mouses seem to do it too



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
                    return;
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

void tuh_mount_cb(uint8_t dev_addr){
    
    for(int i = 0; i < 8; i++){
        uint8_t _protocol = tuh_hid_interface_protocol(dev_addr, i);
        if(_protocol != 0){
            connectedDevices[dev_addr].index = i;
            connectedDevices[dev_addr].itfProtocol = (USB_HID_TYPE_ENUM)_protocol;
            break;            
        }
    }
    
    printf("%s connected w/ dev_addr %i and index %i\n", itfProtocolStrings[connectedDevices[dev_addr].itfProtocol], dev_addr, connectedDevices[dev_addr].index);
    
    // tuh_descriptor_get_device(dev_addr, &USB_Device.desc_device, 18, print_device_descriptor, 0);
    if(!tuh_hid_receive_report(dev_addr, connectedDevices[dev_addr].index)){
        Serial.println("Error, can't receive hid report?");
    }

    if(connectedDevices[dev_addr].itfProtocol == HID_TYPE_KEYBOARD){
        updateKeyboardLEDs(dev_addr);
    }
}

void tuh_umount_cb(uint8_t dev_addr){
    printf("%s with dev_addr %i unmounted\n", itfProtocolStrings[connectedDevices[dev_addr].itfProtocol], dev_addr);
}

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

class PIO_USB_MOUSE{ // probably silly to have separate mouse, keyboard, and device classes, but it should be fine for now. Plus, there may be a scenario where we could have multiple usb_devices
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

PIO_USB_MOUSE USB_Mouse;

void receiveAndProcessMouseHIDReport(uint8_t dev_addr, uint8_t const *report, uint16_t len){
    if(len == 3){
        USB_Mouse.updateCursor(report[0], report[1], report[2]);
    }
    else if(len == 6){
        USB_Mouse.updateCursor(report[0], report[1], report[3]);
        USB_Mouse.scroll(report[5]);
    }
}
// keyboard logic does not include holding keys down. Would need a different type
// for something like a gamepad where pushing right keeps the player moving right, for instance



uint8_t const HID_to_ASCII[128][2] =  { HID_KEYCODE_TO_ASCII };

class PIO_USB_Keyboard{
    public:

        void begin(uint8_t USB_DP_Pin, uint8_t *_data, uint8_t _dataSize){
            USB_Device.begin(USB_DP_Pin);
            data = _data;
            dataSize = _dataSize;
        }

        bool update(bool forceUpdate = false){
            USB_Device.update();
            if(updated){
                updated = false;
                return true;
            }
            return forceUpdate;
        }

        void addNewByte(uint8_t newByte){
            if(data == NULL || dataSize == 0) return;
            data[0]++; // incrementor byte
            for(int i = dataSize - 1; i > 1; i--){
                data[i] = data[i-1];
            }
            data[1] = newByte;
            
            updated = true;
        }

    private:
        uint8_t* data;
        uint8_t dataSize = 0; // length of tracked chars plus one for "incrementing byte" at index 0
        bool updated = false;
    
    public:
        bool numLockOn = true;
        bool capsLockOn = false;
        bool scrollLockOn = false;
        
        bool rightAltPressed = false;
        bool leftAltPressed = false;
        bool rightCtrlPressed = false;
        bool leftCtrlPressed = false;
        bool rightGuiPressed = false;
        bool leftGuiPressed = false;

        bool arrowKeysActive = true;
        bool capsLockKeyActive = true;
        bool keypadKeysActive = true;
        bool numLockKeyActive = false;

        // likely won't ever need these, will probably always ignore
        bool scrollLockKeyActive = false;
        bool FNumKeysActive = false;
        bool ctrlKeysActive = false;
        bool altKeysActive = false;
        bool escapeKeyActive = false;
        bool insertKeyActive = false;
        bool homeKeyActive = false;
        bool endKeyActive = false;
        bool pageUpKeyActive = false;
        bool pageDownKeyActive = false;
        bool printScreenKeyActive = false;
        bool guiKeysActive = false;
        
        // if hold logic is desired for the following keys, need to add corresponding boolean output object
        // Will most likely not need to bother implmenting, however
        bool arrowKeysHolding = false;
        bool spaceHolding = false;
        bool enterHolding = false;
        bool backspaceHolding = false;
        bool deleteHolding = false;
        bool ctrlHolding = false;
        bool altHolding = false;
        bool shiftHolding = false;
};

PIO_USB_Keyboard USB_Keyboard;

// actual ascii codes
#define DELETE_ASCII_CODE 0x7F
#define ESCAPE_ASCII_CODE 0x1B

// note:the below items do not have actual ascii characters. Need to define these in Unity as well
#define ARROW_UP_ASCII_CODE 0x80
#define ARROW_DOWN_ASCII_CODE 0x81
#define ARROW_LEFT_ASCII_CODE 0x82
#define ARROW_RIGHT_ASCII_CODE 0x83
const uint8_t F_NUM_KEY_ASCII_CODES[12] = {0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F};
#define INSERT_ASCII_CODE 0x90
#define PRINT_SCREEN_ASCII_CODE 0x91
#define HOME_ASCII_CODE 0x92
#define END_ASCII_CODE 0x93
#define PAGE_UP_ASCII_CODE 0x94
#define PAGE_DOWN_ASCII_CODE 0x95

#define LEFT_CTRL_ASCII_CODE 0x96
#define RIGHT_CTRL_ASCII_CODE 0x97
#define LEFT_ALT_ASCII_CODE 0x98
#define RIGHT_ALT_ASCII_CODE 0x99
#define LEFT_GUI_ASCII_CODE 0x9A
#define RIGHT_GUI_ASCII_CODE 0x9B

#define SHIFT_TAB_ASCII_CODE 0x9C

void receiveAndProcessKeyboardHIDReport(uint8_t dev_addr, uint8_t const *report, uint16_t len){
    
    static uint8_t HID_CodeStates[32];
    uint8_t newHID_CodeStates[32];
    for(int i = 0; i < 32; i++) newHID_CodeStates[i] = 0;
    bool phantomKeyPress = false;

    // ctrl, alt, and gui (windows) keys (sending as actual key presses with faux ascii codes, not modifiers)
    // ctrl
    if(USB_Keyboard.ctrlKeysActive && report[0] & KEYBOARD_MODIFIER_LEFTCTRL){
        if(!USB_Keyboard.leftCtrlPressed){
            USB_Keyboard.leftCtrlPressed = true;
            USB_Keyboard.addNewByte(LEFT_CTRL_ASCII_CODE);
        }
    }
    else{
        USB_Keyboard.leftCtrlPressed = false;
    }
    
    if(USB_Keyboard.ctrlKeysActive && report[0] & KEYBOARD_MODIFIER_RIGHTCTRL){
        if(!USB_Keyboard.rightCtrlPressed){
            USB_Keyboard.rightCtrlPressed = true;
            USB_Keyboard.addNewByte(RIGHT_CTRL_ASCII_CODE);
        }
    }
    else{
        USB_Keyboard.rightCtrlPressed = false;
    }

    // alt
    if(USB_Keyboard.altKeysActive && report[0] & KEYBOARD_MODIFIER_LEFTALT){
        if(!USB_Keyboard.leftAltPressed){
            USB_Keyboard.leftAltPressed = true;
            USB_Keyboard.addNewByte(LEFT_ALT_ASCII_CODE);
        }
    }
    else{
        USB_Keyboard.leftAltPressed = false;
    }

    if(USB_Keyboard.altKeysActive && report[0] & KEYBOARD_MODIFIER_RIGHTALT){
        if(!USB_Keyboard.rightAltPressed){
            USB_Keyboard.rightAltPressed = true;
            USB_Keyboard.addNewByte(RIGHT_ALT_ASCII_CODE);
        }
    }
    else{
        USB_Keyboard.rightAltPressed = false;
    }

    // gui
    if(USB_Keyboard.guiKeysActive && report[0] & KEYBOARD_MODIFIER_LEFTGUI){
        if(!USB_Keyboard.leftGuiPressed){
            USB_Keyboard.leftGuiPressed = true;
            USB_Keyboard.addNewByte(LEFT_GUI_ASCII_CODE);
        }
    }
    else{
        USB_Keyboard.leftGuiPressed = false;
    }

    if(USB_Keyboard.guiKeysActive && report[0] & KEYBOARD_MODIFIER_RIGHTGUI){
        if(!USB_Keyboard.rightGuiPressed){
            USB_Keyboard.rightGuiPressed = true;
            USB_Keyboard.addNewByte(RIGHT_GUI_ASCII_CODE);
        }
    }
    else{
        USB_Keyboard.rightGuiPressed = false;
    }
    
    for(int i = 2; i < 8; i++){
        if(report[i] == 0x00) break;
        if(report[i] == 0x01){
            phantomKeyPress = true;
            break;
        }
        uint8_t byteIndex = report[i] / 8;
        uint8_t bitIndex = report[i] % 8;
        
        bitWrite(newHID_CodeStates[byteIndex], bitIndex, 1);
        if(!bitRead(HID_CodeStates[byteIndex], bitIndex)){ // new key press registered
            bitWrite(HID_CodeStates[byteIndex], bitIndex, 1);

            switch(report[i]){ // for state changing keys and arrow keys
                case HID_KEY_NUM_LOCK:
                    if(USB_Keyboard.numLockKeyActive){
                        USB_Keyboard.numLockOn = !USB_Keyboard.numLockOn;
                    }
                    continue;
                case HID_KEY_CAPS_LOCK:
                    if(USB_Keyboard.capsLockKeyActive){
                        USB_Keyboard.capsLockOn = !USB_Keyboard.capsLockOn;
                    }
                    continue;
                case HID_KEY_SCROLL_LOCK:
                    if(USB_Keyboard.scrollLockKeyActive){
                        USB_Keyboard.scrollLockOn = !USB_Keyboard.scrollLockOn;
                    }
                    continue;
                case HID_KEY_ARROW_UP:
                    if(USB_Keyboard.arrowKeysActive) USB_Keyboard.addNewByte(ARROW_UP_ASCII_CODE);
                    continue;
                case HID_KEY_ARROW_DOWN:
                    if(USB_Keyboard.arrowKeysActive) USB_Keyboard.addNewByte(ARROW_DOWN_ASCII_CODE);
                    continue;
                case HID_KEY_ARROW_LEFT:
                    if(USB_Keyboard.arrowKeysActive) USB_Keyboard.addNewByte(ARROW_LEFT_ASCII_CODE);
                    continue;
                case HID_KEY_ARROW_RIGHT:
                    if(USB_Keyboard.arrowKeysActive) USB_Keyboard.addNewByte(ARROW_RIGHT_ASCII_CODE);
                    continue;
                case HID_KEY_DELETE:
                    USB_Keyboard.addNewByte(DELETE_ASCII_CODE);
                    continue;
                case HID_KEY_KEYPAD_EQUAL:
                    USB_Keyboard.addNewByte(HID_to_ASCII[report[i]][0]);
                    continue;
                case HID_KEY_INSERT:
                    if(USB_Keyboard.insertKeyActive) USB_Keyboard.addNewByte(INSERT_ASCII_CODE);
                    continue;
                case HID_KEY_PRINT_SCREEN:
                    if(USB_Keyboard.printScreenKeyActive) USB_Keyboard.addNewByte(PRINT_SCREEN_ASCII_CODE);
                    continue;
                case HID_KEY_HOME:
                    if(USB_Keyboard.homeKeyActive) USB_Keyboard.addNewByte(HOME_ASCII_CODE);
                    continue;
                case HID_KEY_END:
                    if(USB_Keyboard.endKeyActive) USB_Keyboard.addNewByte(END_ASCII_CODE);
                    continue;
                case HID_KEY_PAGE_DOWN:
                    if(USB_Keyboard.pageDownKeyActive) USB_Keyboard.addNewByte(PAGE_DOWN_ASCII_CODE);
                    continue;
                case HID_KEY_PAGE_UP:
                    if(USB_Keyboard.pageUpKeyActive) USB_Keyboard.addNewByte(PAGE_UP_ASCII_CODE);
                    continue;

            }

            bool shiftKeyPressed = (KEYBOARD_MODIFIER_LEFTSHIFT & report[0]) || (KEYBOARD_MODIFIER_RIGHTSHIFT & report[0]);
            
            if(report[i] == HID_KEY_TAB && shiftKeyPressed){
                USB_Keyboard.addNewByte(SHIFT_TAB_ASCII_CODE);
            }
            // letters
            if(report[i] >= HID_KEY_A && report[i] <= HID_KEY_Z){
                bool capitalized = shiftKeyPressed ^ USB_Keyboard.capsLockOn;
                uint8_t newByte = HID_to_ASCII[report[i]][capitalized];
                USB_Keyboard.addNewByte(newByte);
                continue;
            }

            // numbers and characters modified by shift, also tab, enter, backspace, escape, space
            if(report[i] >= HID_KEY_1 && report[i] <= HID_KEY_SLASH){

                uint8_t newByte = HID_to_ASCII[report[i]][shiftKeyPressed];
                if(report[i] == 0x32){ // fixes errant backslash code
                    newByte = HID_to_ASCII[0x31][shiftKeyPressed];
                }
                USB_Keyboard.addNewByte(newByte);
                continue;
            }

            //keypad keys
            if(report[i] >= HID_KEY_KEYPAD_DIVIDE && report[i] <= HID_KEY_KEYPAD_DECIMAL && USB_Keyboard.keypadKeysActive){
                if(report[i] <= HID_KEY_KEYPAD_ENTER || report[i] == HID_KEY_KEYPAD_5){
                    USB_Keyboard.addNewByte(HID_to_ASCII[report[i]][0]);
                    continue;
                }
                if(USB_Keyboard.numLockOn){
                    USB_Keyboard.addNewByte(HID_to_ASCII[report[i]][0]);
                    continue;
                }
                switch(report[i]){
                    case HID_KEY_KEYPAD_0:
                        if(USB_Keyboard.insertKeyActive) USB_Keyboard.addNewByte(INSERT_ASCII_CODE);
                        continue;
                    case HID_KEY_KEYPAD_1:
                        if(USB_Keyboard.endKeyActive) USB_Keyboard.addNewByte(END_ASCII_CODE);
                        continue;
                    case HID_KEY_KEYPAD_2:
                        if(USB_Keyboard.arrowKeysActive) USB_Keyboard.addNewByte(ARROW_DOWN_ASCII_CODE);
                        continue;
                    case HID_KEY_KEYPAD_3:
                        if(USB_Keyboard.pageDownKeyActive) USB_Keyboard.addNewByte(PAGE_DOWN_ASCII_CODE);
                        continue;
                    case HID_KEY_KEYPAD_4:
                        if(USB_Keyboard.arrowKeysActive) USB_Keyboard.addNewByte(ARROW_LEFT_ASCII_CODE);
                        continue;
                    case HID_KEY_KEYPAD_6:
                        if(USB_Keyboard.arrowKeysActive) USB_Keyboard.addNewByte(ARROW_RIGHT_ASCII_CODE);
                        continue;
                    case HID_KEY_KEYPAD_7:
                        if(USB_Keyboard.homeKeyActive) USB_Keyboard.addNewByte(HOME_ASCII_CODE);
                        continue;
                    case HID_KEY_KEYPAD_8:
                        if(USB_Keyboard.arrowKeysActive) USB_Keyboard.addNewByte(ARROW_UP_ASCII_CODE);
                    continue;
                    case HID_KEY_KEYPAD_9:
                        if(USB_Keyboard.pageUpKeyActive) USB_Keyboard.addNewByte(PAGE_UP_ASCII_CODE);
                        continue;
                    case HID_KEY_KEYPAD_DECIMAL:
                        USB_Keyboard.addNewByte(DELETE_ASCII_CODE);
                        continue;
                }
                
            }

            // F# keys
            if(report[i] >= HID_KEY_F1 && report[i] <= HID_KEY_F12 && USB_Keyboard.FNumKeysActive){
                USB_Keyboard.addNewByte(F_NUM_KEY_ASCII_CODES[report[i] - HID_KEY_F1]);
                continue;
            }
        }
        
    }

    // register key releases. Currently no logic associated w/ release
    if(!phantomKeyPress){ // don't register key presses or releases if in error/phantom state
        for(int i = 0; i < 32; i++) HID_CodeStates[i] &= newHID_CodeStates[i]; 
    }
    
    phantomKeyPress = false;
    
    updateKeyboardLEDs(dev_addr);
}

void updateKeyboardLEDs(uint8_t dev_addr){
    static uint8_t LEDState;
    uint8_t newState = USB_Keyboard.numLockOn + (USB_Keyboard.capsLockOn << 1) + (USB_Keyboard.scrollLockOn << 2);
    if(newState == LEDState) return;
    LEDState = newState;
    tuh_hid_set_report(dev_addr, connectedDevices[dev_addr].index, 0, HID_REPORT_TYPE_OUTPUT, &LEDState, sizeof(LEDState));
    // tuh_hid_set_report(1, 0, 0, HID_REPORT_TYPE_OUTPUT, &LEDState, sizeof(LEDState));
}

#endif