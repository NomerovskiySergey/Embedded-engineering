#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>

#define LED_PIN PB5
#define BUTTON_PIN PD2

#define DEBOUNCE_MS 30U
#define LONG_PRESS_MS 2000U

#define MODE_SLOW_BLINK 0U
#define MODE_FAST_BLINK 1U
#define MODE_MORSE_HELLO 2U
#define MODE_COUNT 3U

static uint8_t current_mode = MODE_SLOW_BLINK;

void led_init(void);
void led_on(void);
void led_off(void);
void led_toggle(void);

void button_init(void);
bool button_is_pressed(void);
bool button_long_press_detected(void);

bool wait_ms_with_button_check(uint16_t ms);
void next_mode(void);

void mode_slow_blink(void);
void mode_fast_blink(void);
void mode_morse_hello(void);

void morse_dot(void);
void morse_dash(void);
void morse_symbol_gap(void);
void morse_letter_gap(void);
void morse_word_gap(void);

void led_init(void)
{
    /* Set PB5 as output. DDRB bit = 1 means output, bit = 0 means input. */
    DDRB |= (1 << LED_PIN);

    /* Start with the LED off by clearing PB5 in PORTB. */
    PORTB &= ~(1 << LED_PIN);
}

void led_on(void)
{
    /* Drive PB5 HIGH to turn on the built-in LED on Arduino Uno D13. */
    PORTB |= (1 << LED_PIN);
}

void led_off(void)
{
    /* Drive PB5 LOW to turn the LED off. */
    PORTB &= ~(1 << LED_PIN);
}

void led_toggle(void)
{
    /* Writing a 1 to PINB toggles the matching output bit on AVR MCUs. */
    PINB = (1 << LED_PIN);
}

void button_init(void)
{
    /* Set PD2 as input. DDRD bit = 0 means input. */
    DDRD &= ~(1 << BUTTON_PIN);

    /* Enable the internal pull-up resistor on PD2.
       With this wiring, released = HIGH and pressed = LOW. */
    PORTD |= (1 << BUTTON_PIN);
}

bool button_is_pressed(void)
{
    /* Read PIND to sample the actual voltage on PD2. Pressed connects to GND. */
    return (PIND & (1 << BUTTON_PIN)) == 0;
}

bool button_long_press_detected(void)
{
    static bool last_stable_pressed = false;
    static bool long_press_reported = false;
    static uint16_t pressed_time_ms = 0;
    bool pressed_now;

    pressed_now = button_is_pressed();
    _delay_ms(DEBOUNCE_MS);

    if (pressed_now != button_is_pressed()) {
        return false;
    }

    if (pressed_now) {
        if (!last_stable_pressed) {
            pressed_time_ms = 0;
            long_press_reported = false;
        }

        last_stable_pressed = true;

        if (pressed_time_ms < LONG_PRESS_MS) {
            pressed_time_ms += DEBOUNCE_MS;
        }

        if (pressed_time_ms >= LONG_PRESS_MS && !long_press_reported) {
            long_press_reported = true;
            return true;
        }
    } else {
        last_stable_pressed = false;
        long_press_reported = false;
        pressed_time_ms = 0;
    }

    return false;
}

bool wait_ms_with_button_check(uint16_t ms)
{
    while (ms > 0) {
        if (button_long_press_detected()) {
            next_mode();
            return true;
        }

        if (ms >= DEBOUNCE_MS) {
            ms -= DEBOUNCE_MS;
        } else {
            _delay_ms(ms);
            ms = 0;
        }
    }

    return false;
}

void next_mode(void)
{
    current_mode++;
    if (current_mode >= MODE_COUNT) {
        current_mode = MODE_SLOW_BLINK;
    }

    led_off();
}

void mode_slow_blink(void)
{
    led_toggle();
    (void)wait_ms_with_button_check(1000);
}

void mode_fast_blink(void)
{
    led_toggle();
    (void)wait_ms_with_button_check(200);
}

void mode_morse_hello(void)
{
    /* H: .... */
    morse_dot();
    if (current_mode != MODE_MORSE_HELLO) return;
    morse_dot();
    if (current_mode != MODE_MORSE_HELLO) return;
    morse_dot();
    if (current_mode != MODE_MORSE_HELLO) return;
    morse_dot();
    if (current_mode != MODE_MORSE_HELLO) return;
    morse_letter_gap();
    if (current_mode != MODE_MORSE_HELLO) return;

    /* E: . */
    morse_dot();
    if (current_mode != MODE_MORSE_HELLO) return;
    morse_letter_gap();
    if (current_mode != MODE_MORSE_HELLO) return;

    /* L: .-.. */
    morse_dot();
    if (current_mode != MODE_MORSE_HELLO) return;
    morse_dash();
    if (current_mode != MODE_MORSE_HELLO) return;
    morse_dot();
    if (current_mode != MODE_MORSE_HELLO) return;
    morse_dot();
    if (current_mode != MODE_MORSE_HELLO) return;
    morse_letter_gap();
    if (current_mode != MODE_MORSE_HELLO) return;

    /* L: .-.. */
    morse_dot();
    if (current_mode != MODE_MORSE_HELLO) return;
    morse_dash();
    if (current_mode != MODE_MORSE_HELLO) return;
    morse_dot();
    if (current_mode != MODE_MORSE_HELLO) return;
    morse_dot();
    if (current_mode != MODE_MORSE_HELLO) return;
    morse_letter_gap();
    if (current_mode != MODE_MORSE_HELLO) return;

    /* O: --- */
    morse_dash();
    if (current_mode != MODE_MORSE_HELLO) return;
    morse_dash();
    if (current_mode != MODE_MORSE_HELLO) return;
    morse_dash();
    if (current_mode != MODE_MORSE_HELLO) return;
    morse_word_gap();
}

void morse_dot(void)
{
    led_on();
    (void)wait_ms_with_button_check(200);
    led_off();
    morse_symbol_gap();
}

void morse_dash(void)
{
    led_on();
    (void)wait_ms_with_button_check(600);
    led_off();
    morse_symbol_gap();
}

void morse_symbol_gap(void)
{
    (void)wait_ms_with_button_check(200);
}

void morse_letter_gap(void)
{
    (void)wait_ms_with_button_check(600);
}

void morse_word_gap(void)
{
    (void)wait_ms_with_button_check(1400);
}

int main(void)
{
    led_init();
    button_init();

    while (1) {
        if (current_mode == MODE_SLOW_BLINK) {
            mode_slow_blink();
        } else if (current_mode == MODE_FAST_BLINK) {
            mode_fast_blink();
        } else {
            mode_morse_hello();
        }
    }
}
