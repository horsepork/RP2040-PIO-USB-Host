#ifndef _RP2040_PIO_USB_HOST_
#define _RP2040_PIO_USB_HOST_

#include "Arduino.h"
#include "pio_usb.h"
#include "Adafruit_TinyUSB.h"

// Note: this setup assumes only a single connected USB device
// (have not attempted multiple, not sure whether we would ever need it)
// if we do need it, will need to get rid of "singleton" usb_device and add more parameters to the constructor or begin function

// also assuming low speed USB devices, meaning HID report size should be a max of 8 bytes

// currently only including functionality for a keyboard/keypad
// Will work out mouse or other device if/when necessary

// Another limitation is that keyboard combos are not possible with current setup
// (could potentially add an additional leading byte with the latest modifier byte?)
// Shift works, because it makes 'a' into 'A', but ctrl would not change any
// characters being sent to Unity, I just have a designated (fake) ctrl ascii code


// TODO -- figure out how to reset USB host. So far the only device that causes the USB logic to hang
//         is the black and white keyboard. "Normal" keyboards/keypad/mice/barcode scanners seem to operate normally

void printHIDReport(uint8_t const *report, uint16_t len){ // will function as default HID report callback
    Serial.print("HIDreport : ");
    for (uint16_t i = 0; i < len; i++) {
        printf("0x%02X ", report[i]);
    }
    Serial.println();
}

enum USB_HID_TYPE_ENUM{ // also the protocol number. Not used at the moment
    HID_TYPE_KEYBOARD = 1,
    HID_TYPE_MOUSE = 2,
    HID_TYPE_OTHER
};

class PIO_USB_Host{
    public:
        void begin(uint8_t USB_DP_Pin){ // note that D- pin must be one more than DP
            uint32_t cpu_hz = clock_get_hz(clk_sys);
            if (cpu_hz != 120000000UL && cpu_hz != 240000000UL) {
                while (1) {
                    printf("Error: CPU Clock = %lu, PIO USB require CPU clock must be multiple of 120 Mhz\r\n", cpu_hz);
                    printf("Change your CPU Clock to either 120 or 240 Mhz\r\n\n");
                    delay(2000);
                }
            }
            pio_cfg.pin_dp = USB_DP_Pin;
            USBHost.configure_pio_usb(0, &pio_cfg);
            USBHost.begin(0);
            // note: pretty sure rhport (first parameter in two functions above) is not relevant with a single USB device
        }

        void update(){
            USBHost.task();
        }

        void setConnected(bool _connected){
            connected = _connected;
        }

        bool isConnected(){
            return connected;
        }

        void (*receiveHIDReport)(uint8_t const*, uint16_t) = printHIDReport;

    private:
        Adafruit_USBH_Host USBHost;
        tusb_desc_device_t desc_device;
        pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
        bool connected = false;
        
        
};

PIO_USB_Host USB_Device;

void tuh_mount_cb(uint8_t dev_addr){
    // TODO -- confirm that the below function does what I think it does?
    // that is, does it make the USB host ready to receive from USB device?
    tuh_hid_receive_report(dev_addr, 0);
    USB_Device.setConnected(true);
    Serial.println("mounted");
    // TODO -- confirm that I should be passing dev_addr and 0 to index
}

void tuh_umount_cb(uint8_t dev_addr){
    USB_Device.setConnected(false);
    Serial.println("unmounted");
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len) {
  USB_Device.receiveHIDReport(report, len); // callback to be changed as needed for different USB devices
  tuh_hid_receive_report(dev_addr, 0);
}


// keyboard logic does not include holding keys down. Would need a different type
// for something like a gamepad where pushing right keeps the player moving right, for instance

void receiveAndProcessKeyboardHIDReport(uint8_t const *report, uint16_t len);
void updateKeyboardLEDs();

uint8_t const HID_to_ASCII[128][2] =  { HID_KEYCODE_TO_ASCII };

class PIO_USB_Keyboard{
    public:

        void begin(uint8_t USB_DP_Pin, uint8_t *_data, uint8_t _dataSize){
            USB_Device.begin(USB_DP_Pin);
            data = _data;
            dataSize = _dataSize;
            USB_Device.receiveHIDReport = receiveAndProcessKeyboardHIDReport;
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
            data[0]++; // incrementor byte
            for(int i = dataSize - 1; i > 1; i--){
                data[i] = data[i-1];
            }
            data[1] = newByte;
            
            updated = true;
        }

    private:
        uint8_t* data;
        uint8_t dataSize; // length of tracked chars plus one for "incrementing byte" at index 0
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

void receiveAndProcessKeyboardHIDReport(uint8_t const *report, uint16_t len){
    // printHIDReport(report, len);
    static uint8_t HID_CodeStates[32];
    uint8_t newHID_CodeStates[32];
    for(int i = 0; i < 32; i++) newHID_CodeStates[i] = 0;
    bool phantomKeyPress = false;

    // ctrl, alt, and gui keys (sending as actual key presses with faux ascii codes, not modifiers)
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

    // button releases aren't processing correctly, also keyboard leds dont seem to be turning on
    // cycle through report to check for new key presses
    
    for(int i = 2; i < 8; i++){
        // TODO -- make sure this is the behavior I want with phantom key press
        if(report[i] == 0x00) break;
        if(report[i] == 0x01){
            phantomKeyPress = true;
            break;
        }
        uint8_t byteIndex = report[i] / 8;
        uint8_t bitIndex = report[i] % 8;
        
        bitWrite(newHID_CodeStates[byteIndex], bitIndex, 1);
        if(!bitRead(HID_CodeStates[byteIndex], bitIndex)){ // new key press registered
            printf("i: %i, byte: %i, bit %i\n", i, byteIndex, bitIndex);
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
    
    updateKeyboardLEDs();
}

void updateKeyboardLEDs(){
    static uint8_t LEDState = 0;
    uint8_t newState = USB_Keyboard.numLockOn + USB_Keyboard.capsLockOn << 1 + USB_Keyboard.scrollLockOn << 2;
    if(newState == LEDState) return;
    LEDState = newState;
    if(USB_Device.isConnected()){
        tuh_hid_set_report(1, 0, 0, HID_REPORT_TYPE_OUTPUT, &LEDState, sizeof(LEDState));
    }
}

#endif