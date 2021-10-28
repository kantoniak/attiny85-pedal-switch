#include "usb_utils.hpp"

#define STATE_WAIT 0
#define STATE_SEND_KEY 1
#define STATE_PRESSING 2
#define STATE_RELEASE_KEY 3

int main() {
	uchar i;
    uchar button_release_counter = 0;
    uchar state = STATE_WAIT;

    DDRB &= ~(1 << PB4);
    DDRB &= ~(1 << PB3);
	PORTB |= 1 << PB4; // PB4 is input with internal pullup resistor activated (plate A)
    PORTB |= 1 << PB3; // PB3 is input with internal pullup resistor activated (plate B)xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx

    // Zero-fill report
    for(i=0; i<sizeof(keyboard_report); i++) {
        ((uchar*) &keyboard_report)[i] = 0;
    }

    wdt_enable(WDTO_1S);
    usbInit();

    usbDeviceDisconnect(); // enforce re-enumeration
    for(i = 0; i<250; i++) { // wait 500 ms
        wdt_reset(); // keep the watchdog happy
        _delay_ms(2);
    }
    usbDeviceConnect();

    TCCR0B |= (1 << CS01); // timer 0 at clk/8 will generate randomness

    sei(); // Enable interrupts after re-enumeration

    while(1) {
        wdt_reset(); // keep the watchdog happy
        usbPoll();

        const bool plate_a_connected = !(PINB & (1<<PB4));
        const bool plate_b_connected = !(PINB & (1<<PB3));
        const bool pedal_down = plate_a_connected && !plate_b_connected;

		if (state == STATE_WAIT && pedal_down) {
			// also check if some time has elapsed since last button press
			if (button_release_counter == 255) {
				state = STATE_SEND_KEY;
            }
            button_release_counter = 0; // now button needs to be released a while until retrigger
		}

        if (state == STATE_PRESSING && !pedal_down) {
			// also check if some time has elapsed since last button press
			if (button_release_counter == 255) {
				state = STATE_RELEASE_KEY;
            }
            button_release_counter = 0; // now button needs to be released a while until retrigger
		}

		if (button_release_counter < 255) {
			button_release_counter++; // increase release counter
        }

        // characters are sent when messageState == STATE_SEND and after receiving
        // the initial LED state from PC (good way to wait until device is recognized)
        if(usbInterruptIsReady() && state != STATE_WAIT) {
			switch(state) {
			case STATE_SEND_KEY:
				buildReport('x');
				state = STATE_PRESSING; // release next
				break;
            case STATE_PRESSING:
                break;
			case STATE_RELEASE_KEY:
				buildReport('\x0');
				state = STATE_WAIT; // go back to waiting
				break;
			default:
				state = STATE_WAIT;
			}

			// start sending
            usbSetInterrupt((unsigned char*) &keyboard_report, sizeof(keyboard_report));
        }
    }

    return 0;
}
