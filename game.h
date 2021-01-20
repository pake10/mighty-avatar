/*
 * @ Jimi Käyrä
 * @ Otto Loukkola
 */

#ifndef GAME_H_
#define GAME_H_

#define PI 3.14159265

void illegal_move(PIN_Handle buzzerHandle, Display_Handle displayHandle, PIN_Handle ledPin);
void draw_arrows(Display_Handle displayHandle, PIN_Handle buzzerHandle);
void win(PIN_Handle buzzerHandle, Display_Handle displayHandle, PIN_Handle ledHandle);
void determine_direction(float ax, float ay);
double rotate_point(uint8_t coord, float x, float y, float angle);
void clear_direction();
void random_direction();
void remove_illegal(char dire);
uint8_t handle_labyrinth(float ax, float ay);
void draw_labyrinth(Display_Handle displayHandle);
float variance(float *data, uint8_t size);
void empty_all();
uint8_t reverse_direction();
void send_message(uint8_t win);

#endif
