#ifndef LED_H
#define LED_H

typedef unsigned char Value;

int led_begin();
int led_write(Value v);
void led_end();

#endif
