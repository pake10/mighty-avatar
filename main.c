/*
 * @author Jimi Käyrä
 * @author Otto Loukkola
 *
 * Final project for the course Introduction to Computer Systems.
 * Determines the direction of movement and provides a wireless
 * controller (6LoWPAN) for the TRON game.
 *
 * Also provides some additional amenities:
 *  - menu (can be used with gestures)
 *  - karaoke
 *  - stopwatch
 *  - photo gallery ("slideshow") with some pictures
 *  - a galaxy animation
 *  - your favourite songs (Pelimies & Sexbomb)
 *  - maze.
 *
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
#include "sensors/mpu9250.h"
#include "sensors/tmp007.h"
#include "wireless/comm_lib.h"
#include "buzzer.h"
#include "karaoke.h"
#include "game.h"
#include "pitches.h"
#include "ui.h"

#define TASKSTACKSIZE   2048

Char commTaskStack[TASKSTACKSIZE];
Char uiTaskStack[TASKSTACKSIZE];
Char sensorTaskStack[TASKSTACKSIZE];

static PIN_Handle buttonHandle;
static PIN_State buttonState;

static PIN_Handle abuttonHandle;
static PIN_State abuttonState;

static PIN_Handle buzzerHandle;
static PIN_Handle ledHandle;

Clock_Handle btn0_clkHandle;
Clock_Handle read_clkHandle;
Clock_Handle ignore_clkHandle;
Clock_Handle btn1_clkHandle;
Clock_Handle timer_clkHandle;
Clock_Handle debounce_clkHandle;

/*
 * Initializing the program states.
 *   WAIT: display the "Calibrating..." prompt before entering MENU.
 *   MENU: draw the menu and move to the next option if necessary.
 *   MENU_READ: read the MPU in order to move in the menu using gestures.
 *   KARAOKE: play the song and display the lyrics, then return to the MENU.
 *   GAME: draw the game UI when requested.
 *   READ: while we're in the game, read the MPU and request a display update if necessary.
 *   CLOCK: display the stopwatch.
 *   MAZE: display the maze.
 *   SLIDESHOW: display the slideshow, then return to MENU.
 *   SHUTDOWN: "turn off" the device.
 *   ILLEGAL_MOVE: discourage the user when he/she has lost either game.
 *   WIN: celebrate and encourage the user when he/she was won either game.
 *   REVERSE: if we were in state GAME, backtrack the route after WINning.
 *
 */
enum state {MENU=1, KARAOKE, GAME, READ, MENU_READ, CLOCK, MAZE, SLIDESHOW, SHUTDOWN, WAIT, ILLEGAL_MOVE, WIN, REVERSE};
enum state mainState = WAIT;

enum clock {START=1, RUN, STOP};
enum clock clockState = START;

char temp_str[10]; // String for displaying the temperature data.

uint8_t update = 1; // Flag for updating the display when necessary.
uint8_t ignore = 0; // Avoid detecting another gesture right after the last one.
uint8_t send = 0; // Flag for sending a message.
uint8_t moves = 0; // Counting the moves.

float z_data[10]; // Storing the accelerometer z-values in an array.
uint8_t z_index = 0; // The array index.

uint16_t seconds = 0; // Seconds of the stopwatch.
uint8_t button_wait = 0;

uint8_t maze = 0; // "Remembers" if we have come from the maze or not.

PIN_Config buttonConfig[] = {
   Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_HYSTERESIS | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE
};

PIN_Config abuttonConfig[] = {
   Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP  | PIN_HYSTERESIS | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE
};

PIN_Config powerWakeConfig[] = {
   Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PINCC26XX_WAKEUP_NEGEDGE,
   PIN_TERMINATE
};

PIN_Config buzzerConfig[] = {
    Board_BUZZER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

PIN_Config ledConfig[] = {
   Board_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   PIN_TERMINATE
};

static PIN_Handle hMpuPin;
static PIN_State MpuPinState;
static PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER  | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};

Void btn0_clkFxn(UArg arg0) {
	// Debounce: if button still down...
	if( (mainState == MENU || mainState == MENU_READ) && !PIN_getOutputValue(Board_BUTTON0)) {
		menu_increment(); // Move to the next menu item.
	    update = 1; // ...and request a display update.
	} else if ( (mainState == GAME || mainState == READ) && !PIN_getOutputValue(Board_BUTTON0)) {
		mainState = MENU; // Back button: return to main menu.
		clear_direction(); // ...clear the current direction
		update = 1; //... and request a display update.
		System_flush();
	} else if (mainState == CLOCK && !PIN_getOutputValue(Board_BUTTON0)) {
		seconds = 0; // Set stopwatch seconds to 0...
		clockState = START; // Reset stopwatch state...
		mainState = MENU; // Return to the menu...
		update = 1; // ... and request a display update.

		// Ensure that the LEDs are off.
		PIN_setOutputValue(ledHandle, Board_LED0, 0);
		PIN_setOutputValue(ledHandle, Board_LED1, 0);

		// No need to run the stopwatch clock.
		Clock_stop(timer_clkHandle);
	}

	// No need to run this clock.
	Clock_stop(btn0_clkHandle);

	button_wait = 1;
	Clock_start(debounce_clkHandle); // Ignore subsequent button presses for a certain time period.
}

Void read_clkFxn(UArg arg0) {
	// Switch to sensor read mode if in state GAME and no display update is requested.
	if (mainState == GAME && update == 0) {
		mainState = READ;
	// Also switch to sensor read if we're in the menu.
	} else if (mainState == MENU && update == 0) {
		mainState = MENU_READ;
	}
}

Void ignore_clkFxn(UArg arg0) { // Stop ignoring accelerometer data after a certain time period.
	ignore = 0; // Turn off ignore flag...
	z_index = 0; // Set the z-data index to 0...
	memset(z_data, 0, sizeof(z_data)); // Empty the array just to be sure...
	Clock_stop(ignore_clkHandle); // ...and stop the clock.
}

Void btn1_clkFxn(UArg arg0) { // Logic for the second button.
	if(mainState == CLOCK && !PIN_getOutputValue(Board_BUTTON1)) { // Handle the clock.
		switch (clockState) {
			case START:
				clockState = RUN; // Start the watch!
				PIN_setOutputValue(ledHandle, Board_LED0, 1); // Inform and encourage the user with the LED.
				PIN_setOutputValue(ledHandle, Board_LED1, 0);

				Clock_start(timer_clkHandle); // Start the stopwatch clock.
				break;
			case RUN:
				clockState = STOP; // Stop the time.

				PIN_setOutputValue(ledHandle, Board_LED0, 0);
				PIN_setOutputValue(ledHandle, Board_LED1, 1); // Inform the user.

				break;
			case STOP:
				clockState = START; // Resetting the watch.

				PIN_setOutputValue(ledHandle, Board_LED0, 0);
				PIN_setOutputValue(ledHandle, Board_LED1, 1); // Inform the user.

				seconds = 0; // Set seconds to zero.
		}

		update = 1; // Request a display update.
	// Handling MENU / MENU_READ state.
	} else if ( (mainState == MENU || mainState == MENU_READ) && !PIN_getOutputValue(Board_BUTTON1)) {
		switch (get_index()) { // Change the state according to the chosen menu item.
			case 0:
				mainState = GAME;
			    update = 1;
			    break;
			case 1:
				mainState = MAZE;
				update = 1;
				break;
			case 2:
			    mainState = KARAOKE;
			  	break;
			case 3:
			    mainState = CLOCK;
			    Clock_start(timer_clkHandle);
			  	update = 1;
			    break;
			case 4:
				mainState = SLIDESHOW;
				break;
			case 5:
			    mainState = SHUTDOWN;
		 }
	// Selecting a random direction in state GAME or READ.
	} else if ( (mainState == GAME || mainState == READ) && !PIN_getOutputValue(Board_BUTTON1)) {
	    random_direction();
		update = 1;
		send = 1;
		moves++;
	}

	Clock_stop(btn1_clkHandle);

	button_wait = 1;
	Clock_start(debounce_clkHandle);
}

Void timer_clkFxn(UArg arg0) {
	if (mainState == CLOCK && clockState == RUN) { // If we're in the clock and state RUN...
		seconds++; // ...increment the seconds.
		update = 1; // ...and request a display update.
	} else if (mainState == REVERSE) { // If we're in state REVERSE...
		if (send == 0) { // ...and no send flag...
			if (reverse_direction() == 0) { // ...and we haven't traversed through all the directions
				update = 1; // ...request a display update.
				send = 1; // ...and a message transmit.
			} else { // If there are no more directions...
				mainState = MENU; // ...transition to state MENU.
				update = 1; // ...request a display update.
				Clock_stop(timer_clkHandle); // Stop the timer.
			}
		}
	}
}

Void debounce_clkFxn(UArg arg0) {
	button_wait = 0;
	Clock_stop(debounce_clkHandle);
}

Void sensorTask() {
	I2C_Handle i2cMPU;
	I2C_Params i2cMPUParams;

	I2C_Handle i2c; // Interface for the temperature sensor.
	I2C_Params i2cParams;
	I2C_Params_init(&i2cParams);
	i2cParams.bitRate = I2C_400kHz;

	I2C_Params_init(&i2cMPUParams); // Interface for the MPU.
	i2cMPUParams.bitRate = I2C_400kHz;
	i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;

	i2cMPU = I2C_open(Board_I2C, &i2cMPUParams); // Open the I2C.

	PIN_setOutputValue(hMpuPin, Board_MPU_POWER, Board_MPU_POWER_ON); // Power on the MPU.
	Task_sleep(100000 / Clock_tickPeriod);

	mpu9250_setup(&i2cMPU); // Setup the MPU.
	I2C_close(i2cMPU);

	i2c = I2C_open(Board_I2C, &i2cParams); // Close and reopen the I2C for TMP.

	tmp007_setup(&i2c); // Setup the TMP.
	I2C_close(i2c);

	mainState = MENU; // Calibration OK! Move to the menu.
	update = 1; // ...and request a display update.

	float ax, ay, az, gx, gy, gz, magnitude; // Storing sensor data and magnitude.
	float init_ax, init_ay; // Storing the initial acceleration vector (x, y).
	double temp; // Storing the temperature.
	float var; // Storing the calculated variance.

	memset(&temp_str[0], 0, sizeof(temp_str)); // Prepare the string for displaying the temperature data.

	while (1) {
		if (mainState == READ) { // Reading the MPU data.
			i2cMPU = I2C_open(Board_I2C, &i2cMPUParams); // Open the I2C.
			mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz); // Read the data.
			I2C_close(i2cMPU);
			ax *= 9.81; ay *= 9.81; az *= 9.81; // Convert the g values into m/s^2.

			magnitude = sqrt( pow(ax, 2) + pow(ay, 2) ); // ...and calculate the magnitude of vector (ax, ay).

			// If the magnitude exceeds 8 m/s^2, z component is less than -6 m/s^2 and no ignore...
			if (magnitude > 8.0 && az < -6.0 && !ignore) {
				init_ax = ax;
				init_ay = ay; // Store the initial direction.

				ignore = 1; // ...and wait for the peak in the opposite direction.
				Clock_start(ignore_clkHandle); // Stop ignoring after a certain time period.
			}

			if (ignore) { // If there has been a peak in acceleration, start storing the z-component data.
				z_data[z_index] = az;
				z_index = (z_index + 1) % 10;
			}

			var = variance(z_data, z_index); // Calculate the variance.

			// Peak in the opposite direction: hitting the magnitude threshold and sign change in one of the components.
			// In addition, we require a sufficiently low variance.
			// This prevents detecting erraneous movements.
			if (var < 50 && ignore && magnitude > 8.0 && ( (ax > 0 && init_ax < 0) || (ax < 0 && init_ax > 0) || (ay > 0 && init_ay < 0) || (ay < 0 && init_ay > 0) )) {
				z_index = 0;
				memset(z_data, 0, sizeof(z_data));

				determine_direction(init_ax, init_ay); // Determine the direction.
				update = 1; // ...request a display update
				send = 1; // ...request a message transmit

				moves++; // ...increment the move count.
			}

			mainState = GAME; // Move back to the state GAME.
		} else if (mainState == MENU_READ) { // Reading the MPU sensor in the menu.
			i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
			mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);
			I2C_close(i2cMPU);
			ax *= 9.81; ay *= 9.81; az *= 9.81;

			mainState = MENU; // Move back to the state MENU.

			if (az > 6.0 && ay > 8.0 && ax < 5.0) { // If there was a swing to the front...
				switch (get_index()) { // ...enter the corresponding menu functionality.
					case 0:
						mainState = GAME;
						update = 1;
						break;
					case 1:
						mainState = MAZE;
						update = 1;
						break;
					case 2:
						mainState = KARAOKE;
						break;
					case 3:
						mainState = CLOCK;
						Clock_start(timer_clkHandle);
						update = 1;
						break;
					case 4:
						mainState = SLIDESHOW;
						break;
					case 5:
						mainState = SHUTDOWN;
				}
			} else if (sqrt( pow(gx, 2) + pow(gy, 2) + pow(gz, 2) ) > 250) { // If gyroscope magnitude exceeds 250...
				menu_increment(); // ...increment the menu
				update = 1; // ...and update the display.
			}

		} else if (mainState == CLOCK && clockState == RUN) { // If the clock is running...
			i2c = I2C_open(Board_I2C, &i2cParams); // Open the I2C.
			temp = tmp007_get_data(&i2c); // Get temperature data.
		    I2C_close(i2c);

			sprintf(temp_str, "%.0f C", temp); // Prepare the temperature string for displaying on the screen.
		} else if (mainState == MAZE) { // If we're in the maze...
			i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
		    mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz); // Get MPU data.
		    I2C_close(i2cMPU);
		    ax *= 9.81; ay *= 9.81; az *= 9.81;

		    update = 1; // Update the display after handling the labyrinth.

		    switch (handle_labyrinth(ax, ay)) { // Call the function and proceed accordingly.
		    	case 0:
		    		mainState = ILLEGAL_MOVE; // Lost the game.
		    		maze = 1; // Let the uiTask know that we were in the maze.
		    		break;
		    	case 1:
		    		mainState = WIN; // Won the game!
		    		maze = 1; // Let the uiTask know that we were in the maze.
		    		break;
		    }
		}

	    Task_sleep(100000 / Clock_tickPeriod); // Sleep is crucial even for a machine's wellbeing.
	}
}


Void commTask() {
	char *p;
	char parts[4][10];
	char message[16];

	uint8_t part_index = 0; // Index for the parts.
	uint16_t senderAddr; // Sender address.

	StartReceive6LoWPAN(); // Receiving mode!

    while (1) {
    	// If we have a message and the state is GAME or READ...
        if (GetRXFlag() && (mainState == GAME || mainState == READ)) {
        	part_index = 0;
        	memset(message, 0, 16);
        	Receive6LoWPAN(&senderAddr, message, 16); // Go ahead and receive the message!
        	System_printf(message);
        	System_flush();

  	  	  	p = strtok(message, ",");

  	  	  	// Extact the parts into the array.
  	  	    while (p != NULL) {
  	  	    	strcpy(parts[part_index], p);

    			p = strtok(NULL, ",");
    			part_index++;
  	  	  	}

  	  	    // If 251 has won...
  	  	    if (strcmp(parts[1], "WIN") == 0 && strcmp(parts[0], "251") == 0) {
  	  	    	mainState = WIN;
  	  	    	maze = 0;
  	  	    }

  	  	    // if 251 has lost...
  	  	    if (strcmp(parts[1], "LOST GAME") == 0 && strcmp(parts[0], "251") == 0) {
  	  	    	mainState = ILLEGAL_MOVE;
  	  	    	maze = 0;
  	  	    }
        }
    }
}

Void uiTask() {
   Display_Params params;
   Display_Params_init(&params); // Initialize the display.
   params.lineClearMode = DISPLAY_CLEAR_NONE; // Don't clear the lines.
   Display_Handle displayHandle = Display_open(Display_Type_LCD, &params); // Open the display.

   while (1) {
	   	if (send) { // If we were requested to send a message...
	   		if (mainState == REVERSE) { // ...and we're in state REVERSE...
	   			send_message(1); // ...send the message without WIN.
	   		} else {
	   			send_message(0); // We haven't won: send the message.
	   		}

	   		StartReceive6LoWPAN(); // Back to receiving mode!
	   		send = 0; // And no need to resend the message.
	    }

	   	// No switch-structure for clarity: the update parameter
	   	// doesn't have to be checked always.
	    if (mainState == WAIT && update == 1) { // If we're just waiting...
	    	draw_calibrate(displayHandle); // ...draw the calibration screen.
	    	update = 0;
	    } else if (mainState == MENU && update == 1) { // If we're in the menu...
        	draw_menu(displayHandle); // ...unsurprisingly, draw the menu!
            update = 0;
        } else if (mainState == KARAOKE) { // If karaoke has been selected...
            play_karaoke(buzzerHandle, displayHandle); // ...play the karaoke!
            mainState = MENU; // ...move to the menu after it's over
            update = 1; // ...and request a display update.
        } else if (mainState == CLOCK && update == 1) { // If we're in the clock...
        	draw_clock(displayHandle, seconds, temp_str); // ...draw the clock face.
        	update = 0; // No update by default if not requested elsewhere (reset or seconds increment).
        } else if (mainState == GAME && update == 1) { // If we're in the main game...
        	draw_game(displayHandle, buzzerHandle, moves); // Draw the game screen.
        	update = 0; // No unnecessary display updates, please.

        	PIN_setOutputValue(ledHandle, Board_LED0, 1); // Flash the green LED in order to encourage the user.
        	Task_sleep(100000 / Clock_tickPeriod);
        	PIN_setOutputValue(ledHandle, Board_LED0, 0);
        } else if (mainState == SLIDESHOW) { // If the slideshow has been selected...
        	slideshow(displayHandle); // ...play it!
        	mainState = MENU; // And return to the menu afterwards.
        	update = 1;
        } else if (mainState == SHUTDOWN) { // A shutdown has been requested.
        	Display_clear(displayHandle);
        	Display_close(displayHandle); // Clear and close the display.
        	Task_sleep(100000 / Clock_tickPeriod);

        	PIN_close(buttonHandle);
            PINCC26XX_setWakeup(powerWakeConfig); // Set the wakeup configuration.
        	Power_shutdown(NULL, 0); // ...and perform the shutdown.
        } else if (mainState == ILLEGAL_MOVE) { // Illegal move!
        	// Notify the user about an illegal move and discourage him/her with a condescending tone.
        	illegal_move(buzzerHandle, displayHandle, ledHandle);
        	update = 1;

        	if (!maze) { // If we weren't in the maze...
        		empty_all(); // ...clear the game data.
        		moves = 0;
        	}

        	mainState = MENU; // ...and transition to the MENU state.
        	maze = 0; // Enough of the maze already!
        } else if (mainState == WIN) {
        	win(buzzerHandle, displayHandle, ledHandle); // We won!

        	if (maze) { // If we came from the maze...
        		mainState = MENU; // ...return to the menu.
        		update = 1;
        	} else { // However, if we came from the main game...
        		clear_direction(); // ...clear the direction
        		mainState = REVERSE; // ...set the REVERSE state
        		moves = 0;
        		Clock_start(timer_clkHandle); // ...and start iterating through the moves!
        	}

        	maze = 0;
        } else if (mainState == REVERSE && update == 1) { // If in the REVERSE state...
        	Display_clear(displayHandle);
        	draw_arrows(displayHandle, buzzerHandle); // Show the current move.
        	Display_print0(displayHandle, 5, 5, "Encore!"); // ...and inform the user about the playback.
        	update = 0;
        } else if (mainState == MAZE && update == 1) { // In the maze!
        	draw_labyrinth(displayHandle); // Draw the labyrinth if requested to update.
        }

        Task_sleep(100000 / Clock_tickPeriod); // Remember to sleep!
    }
}

void buttonFxn(PIN_Handle handle, PIN_Id pinId) {
	// Debounce logic for eliminating erratic button behaviour:
	// start a clock and check if button still down after 200 ms.
	if (!button_wait) {
		Clock_start(btn0_clkHandle);
		button_wait = 1;
	}
}

void abuttonFxn(PIN_Handle handle, PIN_Id pinId) {
	// Debounce logic for eliminating erratic button behaviour:
	// start a clock and check if button still down after 200 ms.
	if (!button_wait) {
		Clock_start(btn1_clkHandle);
		button_wait = 1;
	}
}


int main(void)
{
   srand(Clock_getTicks()); // Random seed.

   // Initializing the task params and handles.
   Task_Params uiTaskParams;
   Task_Handle uiTaskHandle;

   Task_Params sensorTaskParams;
   Task_Handle sensorTaskHandle;

   Task_Params commTaskParams;
   Task_Handle commTaskHandle;

   Board_initGeneral();

   // Clock for button 0 debounce logic.
   Clock_Params btn0_clkParams;
   Clock_Params_init(&btn0_clkParams);
   btn0_clkParams.period = 200000 / Clock_tickPeriod;
   btn0_clkParams.startFlag = FALSE;
   btn0_clkHandle = Clock_create((Clock_FuncPtr) btn0_clkFxn, 200000 / Clock_tickPeriod, &btn0_clkParams, NULL);

   //Clock for button 1 debounce logic.
   Clock_Params btn1_clkParams;
   Clock_Params_init(&btn1_clkParams);
   btn1_clkParams.period = 200000 / Clock_tickPeriod;
   btn1_clkParams.startFlag = FALSE;
   btn1_clkHandle = Clock_create((Clock_FuncPtr) btn1_clkFxn, 200000 / Clock_tickPeriod, &btn1_clkParams, NULL);

   // Clock for transitioning to sensor read mode every 100 ms.
   Clock_Params read_clkParams;
   Clock_Params_init(&read_clkParams);
   read_clkParams.period = 100000 / Clock_tickPeriod;
   read_clkParams.startFlag = TRUE;
   read_clkHandle = Clock_create((Clock_FuncPtr) read_clkFxn, 100000 / Clock_tickPeriod, &read_clkParams, NULL);

   // Clock for "ignoring" sensor data for 1 s after detecting a motion.
   Clock_Params ignore_clkParams;
   Clock_Params_init(&ignore_clkParams);
   ignore_clkParams.period = 1000000 / Clock_tickPeriod;
   ignore_clkParams.startFlag = FALSE;
   ignore_clkHandle = Clock_create((Clock_FuncPtr) ignore_clkFxn, 1000000 / Clock_tickPeriod, &ignore_clkParams, NULL);

   // Clock for the stopwatch.
   Clock_Params timer_clkParams;
   Clock_Params_init(&timer_clkParams);
   timer_clkParams.period = 1000000 / Clock_tickPeriod;
   timer_clkParams.startFlag = FALSE;
   timer_clkHandle = Clock_create((Clock_FuncPtr) timer_clkFxn, 1000000 / Clock_tickPeriod, &timer_clkParams, NULL);

   // Clock for the button debounce.
   Clock_Params debounce_clkParams;
   Clock_Params_init(&debounce_clkParams);
   debounce_clkParams.period = 100000 / Clock_tickPeriod;
   debounce_clkParams.startFlag = FALSE;
   debounce_clkHandle = Clock_create((Clock_FuncPtr) debounce_clkFxn, 200000 / Clock_tickPeriod, &debounce_clkParams, NULL);

   Task_Params_init(&uiTaskParams);
   uiTaskParams.stackSize = TASKSTACKSIZE;
   uiTaskParams.stack = &uiTaskStack;
   uiTaskParams.priority = 2;

   Task_Params_init(&sensorTaskParams);
   sensorTaskParams.stackSize = TASKSTACKSIZE;
   sensorTaskParams.stack = &sensorTaskStack;
   sensorTaskParams.priority = 2;

   Init6LoWPAN(); // Initialize the wireless communication.

   Task_Params_init(&commTaskParams);
   commTaskParams.stackSize = TASKSTACKSIZE;
   commTaskParams.stack = &commTaskStack;
   commTaskParams.priority = 1;

   // Create the tasks.
   uiTaskHandle = Task_create((Task_FuncPtr) uiTask, &uiTaskParams, NULL);
   sensorTaskHandle = Task_create((Task_FuncPtr) sensorTask, &sensorTaskParams, NULL);
   commTaskHandle = Task_create((Task_FuncPtr) commTask, &commTaskParams, NULL);

   buttonHandle = PIN_open(&buttonState, buttonConfig);
   abuttonHandle = PIN_open(&abuttonState, abuttonConfig);

   if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0) {
      System_abort("Couldn't register button callback function!");
   }

   if (PIN_registerIntCb(abuttonHandle, &abuttonFxn) != 0) {
      System_abort("Couldn't register button callback function!");
   }

   Board_initI2C(); // Initialize I2C communication.

   hMpuPin = PIN_open(&MpuPinState, MpuPinConfig); // MPU pin.
   BIOS_start(); // Start the BIOS!

   return (0);
}
