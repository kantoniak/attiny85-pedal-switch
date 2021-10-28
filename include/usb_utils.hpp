#ifndef USB_UTILS_HPP
#define USB_UTILS_HPP

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <util/delay.h>

/* Based on article from https://codeandlife.com/2012/02/22/v-usb-with-attiny45-attiny85-without-a-crystal/ */
extern "C" {
    #include "usbdrv.h"
}

#define abs(x) ((x) > 0 ? (x) : (-x))

// Called by V-USB after device reset
void hadUsbReset() {
    int frameLength, targetLength = (unsigned)(1499 * (double)F_CPU / 10.5e6 + 0.5);
    int bestDeviation = 9999;
    uchar trialCal, bestCal, step, region;

    // do a binary search in regions 0-127 and 128-255 to get optimum OSCCAL
    for(region = 0; region <= 1; region++) {
        frameLength = 0;
        trialCal = (region == 0) ? 0 : 128;

        for(step = 64; step > 0; step >>= 1) {
            if(frameLength < targetLength) // true for initial iteration
                trialCal += step; // frequency too low
            else
                trialCal -= step; // frequency too high

            OSCCAL = trialCal;
            frameLength = usbMeasureFrameLength();

            if(abs(frameLength-targetLength) < bestDeviation) {
                bestCal = trialCal; // new optimum found
                bestDeviation = abs(frameLength -targetLength);
            }
        }
    }

    OSCCAL = bestCal;
}

// HID routines

PROGMEM const char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = {
    '\x05', '\x01',                    // USAGE_PAGE (Generic Desktop)
    '\x09', '\x06',                    // USAGE (Keyboard)
    '\xa1', '\x01',                    // COLLECTION (Application)
    '\x75', '\x01',                    //   REPORT_SIZE (1)
    '\x95', '\x08',                    //   REPORT_COUNT (8)
    '\x05', '\x07',                    //   USAGE_PAGE (Keyboard)(Key Codes)
    '\x19', '\xe0',                    //   USAGE_MINIMUM (Keyboard LeftControl)(224)
    '\x29', '\xe7',                    //   USAGE_MAXIMUM (Keyboard Right GUI)(231)
    '\x15', '\x00',                    //   LOGICAL_MINIMUM (0)
    '\x25', '\x01',                    //   LOGICAL_MAXIMUM (1)
    '\x81', '\x02',                    //   INPUT (Data,Var,Abs) ; Modifier byte
    '\x95', '\x01',                    //   REPORT_COUNT (1)
    '\x75', '\x08',                    //   REPORT_SIZE (8)
    '\x81', '\x03',                    //   INPUT (Cnst,Var,Abs) ; Reserved byte
    '\x95', '\x05',                    //   REPORT_COUNT (5)
    '\x75', '\x01',                    //   REPORT_SIZE (1)
    '\x05', '\x08',                    //   USAGE_PAGE (LEDs)
    '\x19', '\x01',                    //   USAGE_MINIMUM (Num Lock)
    '\x29', '\x05',                    //   USAGE_MAXIMUM (Kana)
    '\x91', '\x02',                    //   OUTPUT (Data,Var,Abs) ; LED report
    '\x95', '\x01',                    //   REPORT_COUNT (1)
    '\x75', '\x03',                    //   REPORT_SIZE (3)
    '\x91', '\x03',                    //   OUTPUT (Cnst,Var,Abs) ; LED report padding
    '\x95', '\x06',                    //   REPORT_COUNT (6)
    '\x75', '\x08',                    //   REPORT_SIZE (8)
    '\x15', '\x00',                    //   LOGICAL_MINIMUM (0)
    '\x25', '\x65',                    //   LOGICAL_MAXIMUM (101)
    '\x05', '\x07',                    //   USAGE_PAGE (Keyboard)(Key Codes)
    '\x19', '\x00',                    //   USAGE_MINIMUM (Reserved (no event indicated))(0)
    '\x29', '\x65',                    //   USAGE_MAXIMUM (Keyboard Application)(101)
    '\x81', '\x00',                    //   INPUT (Data,Ary,Abs)
    '\xc0'                           // END_COLLECTION
};

typedef struct {
	uint8_t modifier;
	uint8_t reserved;
	uint8_t keycode[6];
} keyboard_report_t;

static keyboard_report_t keyboard_report; // sent to PC
static uchar idleRate; // repeat rate for keyboards

usbMsgLen_t usbFunctionSetup(uchar data[8]) {
    usbRequest_t* rq = (usbRequest_t*) data;

    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) {
        switch(rq->bRequest) {
        case USBRQ_HID_GET_REPORT: // send "no keys pressed" if asked here
            // wValue: ReportType (highbyte), ReportID (lowbyte)
            usbMsgPtr = (unsigned char*) &keyboard_report; // we only have this one
            keyboard_report.modifier = 0;
            keyboard_report.keycode[0] = 0;
            return sizeof(keyboard_report);
		case USBRQ_HID_SET_REPORT: // if wLength == 1, should be LED state
            return (rq->wLength.word == 1) ? USB_NO_MSG : 0;
        case USBRQ_HID_GET_IDLE: // send idle rate to PC as required by spec
            usbMsgPtr = &idleRate;
            return 1;
        case USBRQ_HID_SET_IDLE: // save idle rate as required by spec
            idleRate = rq->wValue.bytes[1];
            return 0;
        }
    }

    return 0; // by default don't return any data
}

usbMsgLen_t usbFunctionWrite(uint8_t * data, uchar len) {
	// Reads keyboard LED states, but we ignore it
	return 1; // Data read, not expecting more
}

// Now only supports letters 'a' to 'z' and 0 (NULL) to clear buttons
void buildReport(uchar send_key) {
	keyboard_report.modifier = 0;

	if(send_key >= 'a' && send_key <= 'z')
		keyboard_report.keycode[0] = 4+(send_key-'a');
	else
		keyboard_report.keycode[0] = 0;
}

#endif
