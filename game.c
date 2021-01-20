/*
 * @ Jimi Käyrä
 * @ Otto Loukkola
 *
 * A library utilizing some important functions related closely
 * to the TRON game itself.
 *
 * Provides capabilities for drawing the game UI, determining the
 * direction of movement, messaging to the backend and backtracking
 * the path traversed by the player (TRON).
 *
 * In addition, this library has the necessary functions for handling
 * the maze game (generating the maze, drawing it and moving in it).
 */

#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// BIOS header files
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>

// TI-RTOS header files
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/mw/display/Display.h>
#include <ti/mw/display/DisplayExt.h>

// Board header files
#include "Board.h"

#include <driverlib/timer.h>
#include "wireless/comm_lib.h"
#include "buzzer.h"
#include "pitches.h"
#include "karaoke.h"
#include "game.h"

#define PI 3.14159265 // This is rather self-explanatory...

// Numerical values: 0 * PI/4, 1 * PI/4, 2 * PI/4, ...
enum direction {DOWN=0, DOWNRIGHT, RIGHT, UPRIGHT, UP, UPLEFT, LEFT, DOWNLEFT, NONE};
enum direction dir = NONE; // Storing the current direction.

enum direction dir_list[100]; // List of directions; used for backtracing.

uint8_t index = 0; // Index for the array containing the directions.

uint8_t ball_x = 8;
uint8_t ball_y = 56; // Display coordinates for the maze "ball".

uint8_t end_i;
uint8_t end_j; // (i,j)-coordinates for the winning block.

uint8_t clear = 1; // If starting a new game, clear the maze display.
uint8_t blocks[6][6]; //

/*
 * Notifying the user about an illegal move: displays a cross, asks the user how many she/he has
 * taken and punishes with discouraging music.
 */
void illegal_move(PIN_Handle buzzerHandle, Display_Handle displayHandle, PIN_Handle ledHandle) {
	Display_clear(displayHandle);
	tContext *pContext = DisplayExt_getGrlibContext(displayHandle);

	uint8_t i = 0;
	uint8_t j = 0;

	// Draws the cross:
	//  a circle with no pixels on the diagonal or on the perpendicular line segment.
	for (i = 28; i <= 68; i++) {
		for (j = 10; j <= 50; j++) {
			if ( (pow(48 - i, 2) + pow(30 - j, 2)) < 400) { // Inside the circle.
				if ( (i - 28) == (j - 10) || (i + j) == 78 || (i + j) == 77 || (i - 28) == (j - 9) ) { // On the diagonal
					if (i > 37 && i < 59 && j > 19 && j < 41) {
					    continue;
					}
				}

				GrPixelDraw(pContext, i, j);
			}
		}
	}

	GrFlush(pContext);

	Display_print0(displayHandle, 8, 2, "Montako olet");
	Display_print0(displayHandle, 9, 1, "oikein ottanut?");

	// Flash the red LED in a discouraging fashion.
	// ...and some discouraging tones to boot.
	PIN_setOutputValue(ledHandle, Board_LED1, 1);
	buzzerOpen(buzzerHandle);
	buzzerSetFrequency(NOTE_B4);
	Task_sleep(500000 / Clock_tickPeriod);
	PIN_setOutputValue(ledHandle, Board_LED1, 0);
	buzzerSetFrequency(NOTE_AS4);
	Task_sleep(500000 / Clock_tickPeriod);
	PIN_setOutputValue(ledHandle, Board_LED1, 1);
	buzzerSetFrequency(NOTE_A4);
	Task_sleep(800000 / Clock_tickPeriod);
	PIN_setOutputValue(ledHandle, Board_LED1, 0);
	buzzerClose();

	Task_sleep(2000000 / Clock_tickPeriod);
}

/*
 * Informing the user about winning the game: displays a galaxy animation and
 * further celebrates by playing the chorus of the iconic Sexbomb by Tom Jones.
 */
void win(PIN_Handle buzzerHandle, Display_Handle displayHandle, PIN_Handle ledHandle) {
	Display_clear(displayHandle);
	tContext *pContext = DisplayExt_getGrlibContext(displayHandle);
	Display_print0(displayHandle, 9, 1, "Voitto kotiin!"); // Notify about winning the game.

	uint8_t i = 0;
	uint8_t j = 0;
	uint8_t point_index = 0;

	/*
	 * Melody, durations and harmony for Sexbomb.
	 * Used the sheet music from https://sheets-piano.ru/wp-content/uploads/2012/02/Tom-Jones-Sexbomb.pdf.
	 */
	uint16_t melody[] = {
	    NOTE_B4, NOTE_GS4, NOTE_B4, NOTE_GS4, 0, NOTE_CS5, NOTE_B4, NOTE_DS5, NOTE_B4, NOTE_CS5,
	    0, NOTE_B4, NOTE_GS4, NOTE_B4, NOTE_B4, NOTE_B4, NOTE_B4, NOTE_B4, NOTE_B4, NOTE_AS4,
	    NOTE_AS4, NOTE_AS4, NOTE_GS4, NOTE_B4, NOTE_GS4, NOTE_B4, NOTE_GS4, NOTE_B4, NOTE_GS4, 0,
	    NOTE_CS5, NOTE_B4, NOTE_DS5, NOTE_B4, NOTE_CS5, 0, NOTE_E4, NOTE_B4, NOTE_GS4, NOTE_B4,
	    NOTE_DS4, NOTE_FS4, NOTE_FS4, NOTE_GS4
	};

	uint8_t durations[] = {
	    30*1.25, 30*1.25, 30*1.25, 30*1.25, 15*1.25, 30*1.25, 15*1.25, 30*1.25, 15*1.25, 30*1.25, 15*1.25, 15*1.25, 15*1.25, 15*1.25, 15*1.25, 15*1.25, 15*1.25, 15*1.25, 15*1.25, 15*1.25, 15*1.25, 15*1.25, 15*1.25,
	    15*1.25, 15*1.25, 30*1.25, 30*1.25, 30*1.25, 30*1.25, 15*1.25, 30*1.25, 15*1.25, 30*1.25, 15*1.25, 15*1.25, 15*1.25, 15*1.25, 15*1.25, 15*1.25, 15*1.25, 15*1.25, 30*1.25, 15*1.25, 45*1.25
	};

	uint16_t harmony[][3] = {
	    {0, 0, 0}, {NOTE_DS3, NOTE_GS3, NOTE_B3}, {0, 0, 0}, {NOTE_GS3, 0, 0}, {0, 0, 0}, {NOTE_CS3, NOTE_E3, NOTE_GS3},
	    {0, 0, 0}, {0, 0, 0}, {NOTE_CS3, 0, 0}, {0, 0, 0}, {0, 0, 0}, {NOTE_DS3, NOTE_GS3, NOTE_B3},
	    {0, 0, 0}, {0, 0, 0}, {NOTE_GS3, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
	    {NOTE_DS3, NOTE_G3, NOTE_AS3}, {0, 0, 0}, {0, 0, 0}, {NOTE_DS3, 0, 0}, {0, 0, 0}, {0, 0, 0},
	    {0, 0, 0}, {NOTE_DS3, NOTE_GS3, NOTE_B3}, {0, 0, 0}, {NOTE_GS3, 0, 0}, {0, 0, 0}, {NOTE_CS3, NOTE_E3, NOTE_GS3},
	    {0, 0, 0}, {0, 0, 0}, {NOTE_CS3, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {NOTE_DS3, NOTE_GS3, NOTE_B3},
	    {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {NOTE_DS3, NOTE_G3, NOTE_AS3}, {0, 0, 0}, {NOTE_DS3, NOTE_GS3, NOTE_B3}
	};

	buzzerOpen(buzzerHandle);

	for (i = 0; i <= 43; i++) {
		play_notes(buzzerHandle, harmony[i], melody, durations, i);

	    // Draw the animation simultaneously.
	    for (j = 1; j <= 3; j++) {
	    	if (point_index <= 100) {
	    		GrCircleDraw(pContext, 48 + point_index * 1/3 * cos(point_index * 1/3), 96 - (60 + point_index * 1/3 * sin(point_index * 1/3)), 1.5);
	    		point_index++;
	    	}
	    }

	    GrFlush(pContext);
	}

	buzzerClose();
}

/*
 * Rotates a point around the center of the screen.
 * Assumes the input coordinates in a plane with origin placed at the bottom left corner of the screen.
 * Output coordinates have been transformed considering the placement of the origin at the top left corner.
 */
double rotate_point(uint8_t coord, float x, float y, float angle) {
	if (coord == 0) {
		return roundf((x - 48) * cos(angle) - (y - 48) * sin(angle) + 48); // Pivot point (48, 48).
	}

	return roundf(96 - ((x - 48) * sin(angle) + (y - 48) * cos(angle) + 48));
}

/*
 * Calculates the variance of a dataset.
 */
float variance(float *data, uint8_t size) {
	if (size == 0) return 0; // If there are no elements, return 0.

	float mean = 0;
	float var = 0;
	uint8_t i = 0;

	for (i = 0; i < size; i++) {
		mean += data[i]; // Sum of the elements.
	}

	mean /= size; // Calculate the mean.

	for (i = 0; i < size; i++) {
		var += pow(data[i] - mean, 2); // Sum of the squared differences.
	}

	var /= size; // Variance.

	return var;
}

/*
 * Draws the arrows on the screen and highlights the chosen direction.
 */
void draw_arrows(Display_Handle displayHandle, PIN_Handle buzzerHandle) {
	uint8_t i = 0;
	uint8_t j = 0;

	tContext *pContext = DisplayExt_getGrlibContext(displayHandle);

	// Draws the arrows.
	// Begins from the top arrow and rotates it n * PI/4 degrees.
	for (i = 0; i <= 7; i++) {
		GrLineDraw(pContext, rotate_point(0, 45, 10, i * PI/4), rotate_point(1, 45, 10, i * PI/4), rotate_point(0, 45, 20, i * PI/4), rotate_point(1, 45, 20, i * PI/4));
		GrLineDraw(pContext, rotate_point(0, 51, 10, i * PI/4), rotate_point(1, 51, 10, i * PI/4), rotate_point(0, 51, 20, i * PI/4), rotate_point(1, 51, 20, i * PI/4));

		GrLineDraw(pContext, rotate_point(0, 41, 12, i * PI/4), rotate_point(1, 41, 12, i * PI/4), rotate_point(0, 48, 6, i * PI/4), rotate_point(1, 48, 6, i * PI/4));
		GrLineDraw(pContext, rotate_point(0, 55, 12, i * PI/4), rotate_point(1, 55, 12, i * PI/4), rotate_point(0, 48, 6, i * PI/4), rotate_point(1, 48, 6, i * PI/4));
	}

	if (dir != NONE) { // If a direction has been chosen, fill the corresponding arrow.
		for (i = 45; i <= 51; i++) { // Fill the rectangular part.
			for (j = 10; j <= 20; j++) {
				GrPixelDraw(pContext, rotate_point(0, i, j, dir * PI/4), rotate_point(1, i, j, dir * PI/4));
			}
		}

		for (i = 1; i <= 4; i++) { // Fill the triangular shape on top of the arrow.
			for (j = 45 + i; j <= 51 - i; j++) {
				GrPixelDraw(pContext, rotate_point(0, j, 10 - i, dir * PI/4), rotate_point(1, j, 10 - i, dir * PI/4));
			}
		}

		// Notify the user with a weird sound.
		for (i = 0; i < 10; i++){
			buzzerOpen(buzzerHandle);
		    buzzerSetFrequency(500 + 400*sin(2 * i));
		    Task_sleep((20000 + 20000 * sin(i)) / Clock_tickPeriod);
		    buzzerClose();
		}
	}

	GrFlush(pContext);
}

/*
 * Draws the maze and the ball on the display.
 * If starting a new game, generate the maze and draw it once.
 * On subsequent calls, draw just the ball at its updated location.
 *
 * The maze is generated with a simple algorithm.
 * The 96x96-display is divided into 16x16 blocks (36);
 * the origin is at the bottom-left corner.
 *
 * (0,2) is used as a starting point in (i,j)-coordinates;
 * on each iteration, select a random adjacent block (up, right, down)
 * until an accessible block is found. Iterate 30 times and
 * use the last block as the end point.
 *
 * Could surely be further optimized but suffices for a small
 * array like this.
 */
void draw_labyrinth(Display_Handle displayHandle) {
	tContext *pContext = DisplayExt_getGrlibContext(displayHandle);

	uint8_t i;
	uint8_t j;

	int8_t i_temp = 0;
	int8_t j_temp = 0;

	uint8_t count = 0;
	uint8_t rand_dir;

	srand(Clock_getTicks());

	if (clear) { // If it's a new game...
		Display_clear(displayHandle); // Clear the display to begin with.

		// Iterate through all the blocks (6 * 6 = 36).
		for (i = 0; i < 6; i++) {
		    for (j = 0; j < 6; j++) {
		        if (i == 0 && j == 2) {
		            blocks[i][j] = 1; // If starting point, set it accessible.
		        } else {
		            blocks[i][j] = 0; // Else set the point non-accessible.
		        }
		    }
		}

		i = 0;
		j = 2; // Begin iterating from the starting point.

		while (count < 30) { // Iterate 30 times.
		    rand_dir = rand() % 3; // Select a (pseudo-)random direction.

		    switch (rand_dir) {
		        case 0: // Up!
		            i_temp = i;
		            j_temp = j + 1;
		            break;
		        case 1: // Right!
		            i_temp = i + 1;
		            j_temp = j;
		            break;
		        case 2: // Down!
		            i_temp = i;
		            j_temp = j - 1;
		            break;
		    }

		    // The block might be out of bounds: select another direction if this is the case.
		    while (i_temp < 0 || i_temp > 5 || j_temp < 0 || j_temp > 5) {
		        rand_dir = rand() % 3; // Select a (pseudo-)random direction.

		        switch (rand_dir) {
		            case 0: // Up!
		                i_temp = i;
		                j_temp = j + 1;
		                break;
		            case 1: // Right!
		                i_temp = i + 1;
		                j_temp = j;
		                break;
		            case 2: // Down!
		                i_temp = i;
		                j_temp = j - 1;
		                break;
		        }
		    }

		    blocks[i_temp][j_temp] = 1; // Set the block (i_temp, j_temp) accessible.
		    i = i_temp;
		    j = j_temp; // Begin the new iteration from the new accessible block.

		    end_i = i;
		    end_j = j; // The last block handled by the algorithm will be stored here.

		    count++; // Ensure that the algorithm won't run forever...
		}

		clear = 0; // The game isn't new after generating the maze and drawing it.

		// Draw the maze.
		for (i = 0; i < 6; i++) { // Yet another pair of nested for-loops...
		    for (j = 0; j < 6; j++) {
		        if (blocks[i][j] == 0) { // If the block is non-accessible...
		        	// Setting the dimensions.
		        	// 96 - y is used to account for the different placement of the origin.
		            tRectangle rect = {16 * i, 96 - 16 * j, 16 * (i + 1), 96 - 16 * (j + 1)};
		            GrRectDraw(pContext, &rect); // Draw a rectangle on it...
		            GrRectFill(pContext, &rect); // ...and fill it.
		        }
		    }
		}

		// Draw the end block icon.
		GrCircleDraw(pContext, 16 * end_i + 8, 96 - (16 * end_j + 8), 4);
		GrCircleDraw(pContext, 16 * end_i + 8, 96 - (16 * end_j + 8), 2);
		GrCircleDraw(pContext, 16 * end_i + 8, 96 - (16 * end_j + 8), 1);
	}

	// On every call, draw a small circle on the new ball location.
	// The circles form a trace.
	GrCircleDraw(pContext, ball_x, ball_y, 1);
	GrFlush(pContext);
}

/*
 * Handles the labyrinth. Uses accelerometer values (ax, ay) to
 * update the coordinates of the ball. Also checks if we have lost
 * or won the game based on the location of the ball.
 *
 * Return values:
 *  0, lost the game.
 *  1, won the game!
 *  2, nothing special.
 *
 */
uint8_t handle_labyrinth(float ax, float ay) {
	uint8_t i = 0;
	uint8_t j = 0;

	// Move the ball if enough movement was detected (over 0.5 m/s^2 in either direction).
	if (abs(ax) > 0.5 || abs(ay) > 0.5) {
		// However, move it only if we won't exceed the display bounds (0 and 96).
		if (ball_x + ax > 0 && ball_x + ax < 96) {
			ball_x = ball_x + ax;
		}

		if (ball_y + ay > 0 && ball_y + ay < 96) {
			ball_y = ball_y + ay;
		}
	}

	// Iterate through all the blocks.
	for (i = 0; i < 6; i++) { // Yet another nested for-loop...
		for (j = 0; j < 6; j++) {
			if (blocks[i][j] == 0 && !clear) { // If the block is non-accessible and the maze has been initialized...
				// ...check if the ball is in a non-accessible block.
				if (ball_x < 16 * (i + 1) && ball_x > 16 * i && ball_y < 96 - 16 * j && ball_y > 96 - 16 * (j + 1)) {
					ball_x = 8;
					ball_y = 56;
					clear = 1; // Prepare for a new game.

					return 0; // Lost the game!
				}
			}
		}
	}

	// Check if the ball is is in the winning block.
	if (ball_x < 16 * (end_i + 1) && ball_x > 16 * end_i && ball_y < 96 - 16 * end_j && ball_y > 96 - 16 * (end_j + 1)) {
		ball_x = 8;
		ball_y = 56;
		clear = 1; // Prepare for a new game.

		return 1; // Won the game!
	}

	return 2; // If nothing special emerged, return 2.
}

/*
 * Determines the direction of the movement from
 * the x and y acceleration.
 */
void determine_direction(float ax, float ay) {
	float angle = atan2(ay, -ax); // Determine the argument of the vector (-ax, ay).

	// Divide the circle into 8 equal pieces.
	// ...and determine the corresponding interval.
    if (angle < PI/8 && angle > -PI/8) {
        dir = RIGHT;
    } else if (angle > PI/8 && angle < 3 * PI/8) {
        dir = UPRIGHT;
    } else if (angle >= 3 * PI/8 && angle < 5 * PI/8) {
        dir = UP;
    } else if (angle >= 5 * PI/8 && angle < 7 * PI/8) {
        dir = UPLEFT;
    } else if (angle <= -PI/8 && angle > -3 * PI/8) {
        dir = DOWNRIGHT;
    } else if (angle <= -3 * PI/8 && angle > -5 * PI/8) {
        dir = DOWN;
    } else if (angle <= -5 * PI/8 && angle > -7 * PI/8) {
        dir = DOWNLEFT;
    } else {
        dir = LEFT;
    }
}

/*
 * Clears the direction
 * by setting it to NONE.
 */
void clear_direction() {
	dir = NONE;
}

/*
 * Sets a random direction.
 */
void random_direction()  {
	dir = (enum direction) rand() % 8;
}

/*
 * Starts from the end of the array and reverses the last direction.
 * Combines two successive directions into one if present, for instance DOWN, LEFT -> DOWNLEFT.
 * Returns 1 if there are no more directions in the array; else 0.
 */
uint8_t reverse_direction() {
	// If we have traversed through the whole array, empty the array and return 1.
    if (index <= 0) {
        dir = NONE;
        memset(dir_list, 0, sizeof(dir_list));
        index = 0;

        return 1;
    }

	int8_t i = 0;
	int8_t j = 0;

	// Start from the end of the array and find the first non-empty occurence.
	// The corresponding index is stored in i.
	for (i = index - 1; i >= 0; i--) {
		if (dir_list[i] != NONE) {
			break;
		}
	}

	// Find the second occurence in a similar fashion, starting from i - 1.
	for (j = i - 1; j >= 0; j--) {
		if (dir_list[j] != NONE) {
			break;
		}
	}

	// If i = 0, then j = -1 (there's no second occurence to be found).
	// Set j = 0 and apply the same logic.
	if (j < 0) j = 0;

	// Determine the correct direction (reverse).
	// Set the index to the second occurence if there was an acceptable combination;
	// else to the first occurence.
	switch (dir_list[i]) {
		case UP:
			if (dir_list[j] == RIGHT) { // UPRIGHT -> DOWNLEFT
				dir = DOWNLEFT;
				index = j;
			} else if (dir_list[j] == LEFT) { // UPLEFT -> DOWNRIGHT
				dir = DOWNRIGHT;
				index = j;
			} else { // UP -> DOWN
				dir = DOWN;
				index = i;
			}

			break;
		case RIGHT:
			if (dir_list[j] == UP) { // UPRIGHT -> DOWNLEFT
				dir = DOWNLEFT;
				index = j;
			} else if (dir_list[j] == DOWN) { // DOWNRIGHT -> UPLEFT
				dir = UPLEFT;
				index = j;
			} else { // RIGHT -> LEFT
				dir = LEFT;
				index = i;
			}

			break;
		case LEFT:
			if (dir_list[j] == UP) { // UPLEFT -> DOWNRIGHT
				dir = DOWNRIGHT;
				index = j;
			} else if (dir_list[j] == DOWN) { // DOWNLEFT -> UPRIGHT
				dir = UPRIGHT;
				index = j;
			} else { // LEFT -> RIGHT
				dir = RIGHT;
				index = i;
			}

			break;

		case DOWN:
			if (dir_list[j] == RIGHT) { // DOWNRIGHT -> UPLEFT
				dir = UPLEFT;
				index = j;
			} else if (dir_list[j] == LEFT) { // DOWNLEFT -> UPRIGHT
				dir = UPRIGHT;
				index = j;
			} else { // DOWN -> UP
				dir = UP;
				index = i;
			}
	}

	return 0; // Success!
}

/*
 * Set the system back to the original state:
 * no direction chosen and the direction list is empty.
 */
void empty_all() {
	dir = NONE;
	memset(dir_list, 0, sizeof(dir_list));
	index = 0;
}

/*
 * Sends a message to the backend.
 * Stores the directions in an array if not in win mode.
 */
void send_message(uint8_t win) {
	char msg_1[16];
	memset(&msg_1[0], 0, sizeof(msg_1));
	sprintf(msg_1, "event:"); // Prepare the first string.

	char msg_2[16];
	memset(&msg_2[0], 0, sizeof(msg_2));

	switch (dir) {
		case RIGHT:
			strcat(msg_1, "RIGHT");
			if (win == 0) dir_list[index] = RIGHT; // If not in win state, add the direction to the list.
			break;
		case LEFT:
			strcat(msg_1, "LEFT");
			if (win == 0) dir_list[index] = LEFT;
			break;
		case UP:
			strcat(msg_1, "UP");
			if (win == 0) dir_list[index] = UP;
			break;
		case DOWN:
			strcat(msg_1, "DOWN");
			if (win == 0) dir_list[index] = DOWN;
			break;
		case UPRIGHT:
			strcat(msg_1, "UP");

			if (win == 0) {
				dir_list[index] = UP;
				index++;
			}

			sprintf(msg_2, "event:RIGHT");

			if (win == 0) dir_list[index] = RIGHT;
			break;
		case UPLEFT:
			strcat(msg_1, "UP");

			if (win == 0) {
				dir_list[index] = UP;
				index++;
			}

			sprintf(msg_2, "event:LEFT");

			if (win == 0) dir_list[index] = LEFT;
			break;
		case DOWNLEFT:
			strcat(msg_1, "DOWN");

			if (win == 0) {
				dir_list[index] = DOWN;
				index++;
			}

			sprintf(msg_2, "event:LEFT");

			if (win == 0) {
				dir_list[index] = LEFT;
			}

			break;
		case DOWNRIGHT:
			strcat(msg_1, "DOWN");

			if (win == 0) {
				dir_list[index] = DOWN;
				index++;
			}

			sprintf(msg_2, "event:RIGHT");

			if (win == 0) {
				dir_list[index] = RIGHT;
			}

			break;
	}

	Send6LoWPAN(IEEE80154_SERVER_ADDR, msg_1, strlen(msg_1)); // Send the first message.

	Task_sleep(100000 / Clock_tickPeriod); // Err on the side of caution: sleep between messages.

	if (msg_2[0] == 'e') {
		Send6LoWPAN(IEEE80154_SERVER_ADDR, msg_2, strlen(msg_2)); // Send the second message if present.
	}

	if (win == 0) index++; // If not in WIN mode, increment the index.
}
