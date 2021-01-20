/*
 * @ Jimi Käyrä
 * @ Otto Loukkola
 */

#ifndef UI_H_
#define UI_H_

void draw_upper_box(Display_Handle displayHandle);
void draw_lower_box(Display_Handle displayHandle);
void draw_menu(Display_Handle displayHandle);
void draw_clock(Display_Handle displayHandle, int seconds, char temp_str[]);
void draw_game(Display_Handle displayHandle, PIN_Handle buzzerHandle, int moves);
void draw_calibrate(Display_Handle displayHandle);
void slideshow(Display_Handle displayHandle);
void menu_increment();
void set_index(int set);
int get_index();

#endif
