/*
 * @author Jimi Käyrä
 * @author Otto Loukkola
 */

#ifndef KARAOKE_H_
#define KARAOKE_H_

void play_notes(PIN_Handle buzzerHandle, uint16_t *harmony, uint16_t *melody, uint8_t *durations, uint8_t i);
void play_karaoke(PIN_Handle buzzerHandle, Display_Handle displayHandle);

#endif
