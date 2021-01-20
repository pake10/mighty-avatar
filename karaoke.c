/*
 * @ Jimi Käyrä
 * @ Otto Loukkola
 *
 * A library providing karaoke capabilities.
 *
 * Features a function for playing the harmony and the
 * melody. Most importantly, though, provides a dedicated
 * function for playing the famous song Pelimies (~Player) by
 * Martti Vainaa & Sallitut aineet (~Marty Dead & Allowed substances).
 */

#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/mw/display/Display.h>
#include <ti/mw/display/DisplayExt.h>
#include "buzzer.h"
#include "pitches.h"
#include <inttypes.h>

/*
 * Plays the notes at time i: play the harmony notes if they are present
 * or just the melody at that point.
 *
 */
void play_notes(PIN_Handle buzzerHandle, uint16_t *harmony, uint16_t *melody, uint8_t *durations, uint8_t i) {
	if (harmony[1] != 0) { // Assuming we have three notes in the harmony.
		// Play them one by one with a delay of 50 ms.
		buzzerSetFrequency(harmony[0]);
	    Task_sleep(50000 / Clock_tickPeriod);
	    buzzerSetFrequency(harmony[1]);
	    Task_sleep(50000 / Clock_tickPeriod);
	    buzzerSetFrequency(harmony[2]);
	    Task_sleep(50000 / Clock_tickPeriod);
	} else if (harmony[0] != 0) { // Just one note!
		buzzerSetFrequency(harmony[0]);
	    Task_sleep(50000 / Clock_tickPeriod);
	}

	if (melody[i] != 0) { // If we have a melody note...
		buzzerSetFrequency(melody[i]);
	    Task_sleep(durations[i] * 10000 / Clock_tickPeriod); // ...play it with the given duration.
	} else { // However, if we have a musical break...
		buzzerClose(); // ...close the buzzer
	    Task_sleep(durations[i] * 10000 / Clock_tickPeriod); // ...sleep
	    buzzerOpen(buzzerHandle); // ...and reopen the buzzer.
	}
}

/*
 * Plays the legendary song "Pelimies" by Martti Vainaa & Sallitut aineet,
 * also known as Marty Dead & Allowed substances and displays the lyrics.
 * Provides a top-notch karaoke experience!
 */
void play_karaoke(PIN_Handle buzzerHandle, Display_Handle displayHandle) {
	// The melody of the song split into two parts.
	// A zero (0) signifies a musical break.
	// Transcribed by hand.

    uint16_t melody[] = {
    		// "Na-na-na"...
    		NOTE_CS5, NOTE_FS5, NOTE_A5, NOTE_D4, NOTE_CS5, NOTE_FS5, NOTE_A5, NOTE_E4, NOTE_CS5, NOTE_CS5, NOTE_CS5,
            NOTE_B4, NOTE_FS4, NOTE_FS4, NOTE_FS4, NOTE_FS4, NOTE_FS4, NOTE_CS5, NOTE_CS5, NOTE_CS5, NOTE_D5, NOTE_CS5,
            NOTE_B4, NOTE_A4, NOTE_CS5, NOTE_B4, NOTE_CS5, NOTE_CS5, NOTE_CS5, NOTE_B4, NOTE_FS4, NOTE_FS4, NOTE_FS4, NOTE_FS4,
			// "Tänä iltana ei tuu pakkeja"
            NOTE_FS4, NOTE_CS5, NOTE_CS5, NOTE_B4, NOTE_A4, NOTE_FS4, 0, NOTE_CS5, NOTE_CS5, NOTE_B4, NOTE_A4, NOTE_FS4,

			// "Teen mitä vaan, pumppaan rautaa, juosten kierrän maan"
		    NOTE_CS5, NOTE_FS4, NOTE_A4, NOTE_CS5, NOTE_FS4, NOTE_A4, NOTE_D5, NOTE_CS5, NOTE_B4, NOTE_CS5, NOTE_B4, NOTE_A4, NOTE_GS4,
			// "jos niin mä saan sinut innostumaan"
		    NOTE_CS5, NOTE_FS4, NOTE_A4, NOTE_CS5, NOTE_FS4, NOTE_A4, NOTE_GS4, NOTE_A4, NOTE_B4, NOTE_CS5,
			// "teen sulle sen, pienen tempun vanhanaikaisen"
		    NOTE_CS5, NOTE_FS4, NOTE_A4, NOTE_CS5, NOTE_FS4, NOTE_A4, NOTE_D5, NOTE_CS5, NOTE_B4, NOTE_CS5, NOTE_B4, NOTE_A4, NOTE_GS4,
			// "oon pelimies, kovakuntoinen"
		    NOTE_CS5, NOTE_FS4, NOTE_A4, NOTE_CS5, NOTE_FS4, NOTE_A4, NOTE_GS4, NOTE_A4, NOTE_B4
    };

    uint16_t melody2[] = {
    		// "Tahdon koskettaa, voin sen tunnustaa"
    		NOTE_CS5, NOTE_CS5, NOTE_B4, NOTE_FS4, NOTE_FS4, NOTE_CS5, NOTE_CS5, NOTE_B4, NOTE_E4, NOTE_E4,
			// "Saanko ehdottaa, iltaa kanssani?"
    		NOTE_CS5, NOTE_CS5, NOTE_B4, NOTE_FS4, NOTE_FS4, NOTE_B4, NOTE_CS5, NOTE_B4, NOTE_CS5, NOTE_B4, 0,
			// "Tule niin vien sinut kahville"
    		NOTE_CS5, NOTE_CS5, NOTE_CS5, NOTE_B4, NOTE_FS4, NOTE_FS4, NOTE_FS4, NOTE_FS4, NOTE_FS4,
			// "ja nakkikioskille jatkoille"
    		NOTE_CS5, NOTE_CS5, NOTE_CS5, NOTE_D5, NOTE_CS5, NOTE_B4, NOTE_A4, NOTE_CS5, NOTE_B4,
			// "siitä taksiin ja saatille"
    		NOTE_CS5, NOTE_CS5, NOTE_B4, NOTE_FS4, NOTE_FS4, NOTE_FS4, NOTE_FS4, NOTE_FS4,
			// "tänä iltana ei tuu pakkeja"
    	    NOTE_CS5, NOTE_CS5, NOTE_B4, NOTE_A4, NOTE_FS4, 0, NOTE_CS5, NOTE_CS5, NOTE_B4, NOTE_A4, NOTE_FS4
    };

    // The note durations (ms) split into two parts.
    uint8_t durations[] = {15, 15, 150, 150, 15, 15, 150, 150, 15, 15, 15, 50, 15, 15, 30, 30, 50, 15, 15, 15, 50,
                       15, 15, 30, 30, 50, 15, 15, 15, 50, 15, 15, 30, 30, 50, 15, 20, 30, 15, 15, 5, 15, 20,
                       30, 15, 15,

					   50, 20, 15, 50, 25, 20, 30, 30, 30, 30, 30, 30, 50, 50, 20, 15, 50, 30, 20, // "Teen mitä vaan"
					   50, 50, 50, 50, 50, 20, 15, 50, 25, 15, 50, 25, 20, 30, 30, 30, 30, 50, 20, 15, 50, 25,
					   20, 50, 50, 50
    };

    uint8_t durations2[] = {20, 20, 30, 30, 100, 20, 20, 30, 30, 100, 20, 20, 30, 30, 100, 30, 20, 30,
			   20, 30, 20, // "iltaa kanssani"

			   15, 15, 15, 50, 15, 15, 30, 30, 50, 15, 20, 20, 50,
			   15, 15, 30, 30, 50, 20, 20, 30, 20, 20, 30, 30, 50,
			   15, 20, 30, 15, 15, 5, 15, 20, 30, 15, 15
    };

    // Harmony: F#m, D, E. Split into two arrays.
    uint16_t harmony[][3] = {{NOTE_CS3, NOTE_FS3, NOTE_A3}, {0, 0, 0}, {0, 0, 0}, {NOTE_D3, NOTE_FS3, NOTE_A3}, {NOTE_CS3, NOTE_FS3, NOTE_A3}, {0,0,0}, {0,0,0},
                         {NOTE_E3, NOTE_GS3, NOTE_B3}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {NOTE_CS3, NOTE_FS3, NOTE_A3}, {0, 0, 0}, {0, 0, 0}, {NOTE_CS4, 0, 0},
                         {0, 0, 0}, {0, 0, 0}, {NOTE_FS3, 0, 0}, {0, 0, 0}, {0, 0, 0}, {NOTE_D3, NOTE_FS3, NOTE_A3}, {0, 0, 0}, {0, 0, 0}, {NOTE_A3, 0, 0}, {0, 0, 0},
                         {NOTE_E3, NOTE_GS3, NOTE_B3}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {NOTE_CS3, NOTE_FS3, NOTE_A3}, {0, 0, 0}, {0, 0, 0}, {NOTE_CS4, 0, 0},
                         {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {NOTE_D3, NOTE_FS3, NOTE_A3}, {0, 0, 0}, {0, 0, 0}, {0,0,0}, {0, 0, 0}, {0, 0, 0},
                         {NOTE_E3, NOTE_GS3, NOTE_B3}, {0, 0, 0}, {0, 0, 0}, // ei tuu pakkeja

						 {NOTE_CS3, NOTE_FS3, NOTE_A3}, {NOTE_FS3, 0, 0}, {0, 0, 0}, {NOTE_CS4, 0, 0}, {NOTE_FS3, 0, 0}, {0, 0, 0}, {NOTE_D3, NOTE_FS3, NOTE_A3},
						 {0, 0, 0}, {NOTE_A3, 0, 0}, {0, 0, 0}, {NOTE_E3, NOTE_GS3, NOTE_B3}, {0, 0, 0}, {NOTE_B3, 0, 0}, {NOTE_CS3, NOTE_FS3, NOTE_A3}, {NOTE_FS3, 0, 0}, {0, 0, 0},
    					 {NOTE_FS3, 0, 0}, {NOTE_CS3, 0, 0}, {0, 0, 0}, {NOTE_D3, NOTE_FS3, NOTE_A3}, {NOTE_A3, 0, 0}, {NOTE_E3, NOTE_GS3, NOTE_B3}, {NOTE_B3, 0, 0},
						 {NOTE_CS3, NOTE_FS3, NOTE_A3}, {NOTE_FS3, 0, 0}, {0, 0, 0}, {NOTE_CS3, 0, 0}, {NOTE_FS3, 0, 0}, {0, 0, 0}, {NOTE_D3, NOTE_FS3, NOTE_A3}, {0, 0, 0}, {NOTE_A3, 0, 0},
						 {0, 0, 0}, {NOTE_E3, NOTE_GS3, NOTE_B3}, {0, 0, 0}, {NOTE_B3, 0, 0}, {NOTE_CS3, NOTE_FS3, NOTE_A3}, {NOTE_FS3, 0, 0}, {0, 0, 0}, {NOTE_FS3, 0, 0}, {NOTE_CS3, 0, 0},
						 {0, 0, 0}, {NOTE_D3, NOTE_FS3, NOTE_A3}, {NOTE_A3, 0, 0}, {NOTE_E3, NOTE_GS3, NOTE_B3}

    };

    uint16_t harmony2[][3] = { {0, 0, 0}, {0, 0, 0}, {NOTE_D3, NOTE_FS3, NOTE_A3}, {0, 0, 0}, {NOTE_A3, 0, 0}, {0, 0, 0}, {0, 0, 0}, {NOTE_CS3, NOTE_E3, NOTE_A3}, {0, 0, 0},
    						   {NOTE_E3, 0, 0}, {0, 0, 0}, {0, 0, 0}, {NOTE_D3, NOTE_FS3, NOTE_A3}, {0, 0, 0}, {NOTE_A3, 0, 0}, {NOTE_E3, NOTE_GS3, NOTE_B3}, {NOTE_E3, 0, 0}, {0, 0, 0},
							   {NOTE_E3, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {NOTE_CS3, NOTE_FS3, NOTE_A3}, {0, 0, 0}, {0, 0, 0}, {NOTE_CS3, 0, 0}, {0, 0, 0}, {0, 0, 0},
							   {NOTE_FS3, 0, 0}, {0, 0, 0}, {0, 0, 0}, {NOTE_D3, NOTE_FS3, NOTE_A3}, {0, 0, 0}, {0, 0, 0}, {NOTE_A3, 0, 0}, {0, 0, 0}, {NOTE_E3, NOTE_GS3, NOTE_B3},
							   {NOTE_B3, 0, 0}, {0, 0, 0}, {NOTE_CS3, NOTE_E3, NOTE_A3}, {0, 0, 0}, {0, 0, 0}, {NOTE_A3, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
							   {NOTE_D3, NOTE_FS3, NOTE_A3}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {NOTE_E3, NOTE_GS3, NOTE_B3}, {0, 0, 0}, {0, 0, 0}

    };

    Display_clear(displayHandle); // Clear the display.
    buzzerOpen(buzzerHandle); // Open the buzzer.

    uint8_t i = 0;

    for (i = 0; i <= 90; i++) {
    	// Display the lyrics as the song progresses.
        switch (i) {
        	case 0:
        		Display_print0(displayHandle, 3, 0, "Martti Vainaa &");
        		Display_print0(displayHandle, 4, 1, "Sallitut aineet");
        		Display_print0(displayHandle, 6, 3, "Pelimies");
        		break;
            case 7: // Na-na-na...
            	Display_clear(displayHandle);
                Display_print0(displayHandle, 5, 1, "Na-na-na-naa");
                Display_print0(displayHandle, 6, 1, "na-na-nan");
                Display_print0(displayHandle, 7, 1, "nan-naa");
                break;

            case 34: // Tänä iltana...
                Display_clear(displayHandle);
                Display_print0(displayHandle, 5, 1, "Tana iltana");
                Display_print0(displayHandle, 6, 1, "ei tuu pakkeja!");
                break;
            case 45: // Teen mitä vaan..
            	Display_clear(displayHandle);
            	Display_print0(displayHandle, 5, 1, "Teen mita vaan");
            	Display_print0(displayHandle, 6, 1, "pumppaan rautaa");
            	break;
            case 52:
            	Display_clear(displayHandle);
            	Display_print0(displayHandle, 5, 1, "juosten kierran");
            	Display_print0(displayHandle, 6, 1, "maan");
            	break;
            case 59:
            	Display_clear(displayHandle);
            	Display_print0(displayHandle, 5, 1, "jos niin ma");
            	Display_print0(displayHandle, 6, 1, "saan sinut");
            	Display_print0(displayHandle, 7, 1, "innostumaan");
            	break;

            case 69:
            	Display_clear(displayHandle);
            	Display_print0(displayHandle, 5, 1, "Teen sulle sen");
            	Display_print0(displayHandle, 6, 1, "pienen tempun");
            	Display_print0(displayHandle, 7, 1, "vanhanaikaisen");
            	break;

            case 80:
            	Display_clear(displayHandle);
            	Display_print0(displayHandle, 5, 1, "Oon pelimies");
            	Display_print0(displayHandle, 6, 1, "kovakuntoinen");
       }

       play_notes(buzzerHandle, harmony[i], melody, durations, i);
    }

    // Advancing to the second part.
    for (i = 0; i <= 57; i++) {
    	switch (i) {
        	case 0:
        		Display_clear(displayHandle);
        		Display_print0(displayHandle, 5, 2, "Tahdon");
                Display_print0(displayHandle, 6, 1, "koskettaa");
                Display_print0(displayHandle, 7, 1, "(koskettaa)");
                break;

            case 5:
                Display_clear(displayHandle);
                Display_print0(displayHandle, 5, 1, "Voin sen");
                Display_print0(displayHandle, 6, 1, "tunnustaa");
                Display_print0(displayHandle, 7, 1, "(tunnustaa)");
                break;

            case 10:
                Display_clear(displayHandle);
                Display_print0(displayHandle, 5, 1, "Saanko");
                Display_print0(displayHandle, 6, 1, "ehdottaa");
                Display_print0(displayHandle, 7, 1, "(ehdottaa)");
                break;

            case 15:
                Display_clear(displayHandle);
                Display_print0(displayHandle, 5, 1, "iltaa");
                Display_print0(displayHandle, 6, 1, "kanssani?");
                break;

            case 20:
                Display_clear(displayHandle);
                Display_print0(displayHandle, 5, 1, "Tule niin");
                Display_print0(displayHandle, 6, 1, "vien sinut");
                Display_print0(displayHandle, 7, 1, "kahville");
                break;

            case 30:
                Display_clear(displayHandle);
                Display_print0(displayHandle, 5, 1, "ja nakki-");
                Display_print0(displayHandle, 6, 1, "kioskille");
                Display_print0(displayHandle, 7, 1, "jatkoille!");
                break;

            case 38:
                Display_clear(displayHandle);
                Display_print0(displayHandle, 5, 1, "Siita taksiin");
                Display_print0(displayHandle, 6, 1, "ja saatille");
                break;

            case 48:
            	Display_clear(displayHandle);
                Display_print0(displayHandle, 5, 1, "Tana iltana");
                Display_print0(displayHandle, 6, 1, "ei tuu pakkeja!");
                break;
           }

    	play_notes(buzzerHandle, harmony2[i], melody2, durations2, i); // Play the notes.
    }

    buzzerClose(); // Close the buzzer!
}
