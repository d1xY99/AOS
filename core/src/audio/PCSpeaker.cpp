#include "PCSpeaker.h"
#include "ports.h"
#include "utypes.h"

#define ONE_KHZ 1000

// PIT (Programmable Interval Timer)
#define PIT_CHANNEL2 0x42
#define PIT_COMMAND 0x43

// Speaker control port
#define PORT_SPEAKER 0x61

// Speaker bit masks
#define SPEAKER_GATE 0b01   // Bit 0 — enable PIT channel 2 output
#define SPEAKER_ENABLE 0b10 // Bit 1 — connect output to speaker

// PIT command byte for:
// Pipe 2 | Access L+H | Mode 3 | Binary
#define PIT_CMD_SQUARE_WAVE 0b10110110

void PCSpeaker::Beep() {
  // Configure PIT channel 2 for a square wave
  outportb(PIT_COMMAND, PIT_CMD_SQUARE_WAVE);

  // Set frequency divisor
  uint32_t divisor = 1193180 / ONE_KHZ;

  // PIT only accepts 8 bit writes, so we have to split the divisor
  outportb(PIT_CHANNEL2, divisor);      // LSB
  outportb(PIT_CHANNEL2, divisor >> 8); // MSB

  // Enable speaker output (set bits 0 and 1)
  uint8_t val = inportb(PORT_SPEAKER);
  outportb(PORT_SPEAKER, val | (SPEAKER_GATE | SPEAKER_ENABLE));
}

void PCSpeaker::StopBeep() {
  uint8_t val = inportb(PORT_SPEAKER);
  outportb(PORT_SPEAKER, val & ~(SPEAKER_GATE | SPEAKER_ENABLE));
}
