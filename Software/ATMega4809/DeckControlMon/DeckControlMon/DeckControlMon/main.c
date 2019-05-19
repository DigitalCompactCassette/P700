#include <atmel_start.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef _countof
#define _countof(array) (sizeof(array) / sizeof((array)[0]))
#endif

int main(void)
{
	/* Initializes MCU, drivers and middleware */
	atmel_start_init();

	printf("DDU-2113 Deck Control Monitor\r\n");

	/*
		The following code "follows" the traffic between the Digital PCB 
		Microcontroller (DIG MCU) and the DDU-2113 DCC-deck controller.
		That deck controller is used in the DCC-730, DCC-951 and FW-68
		and possibly in other DCC recorders that we don't know about yet.

		The project listens to the RX and TX connections on the side of
		the PCB and prints the values to the debugging serial port.
		The DDU-2113 serial port runs at 38400/8n1.

		Written for the ATMega4809 Curiosity Nano with RX connected to
		PA1 and TX connected to PC1. The Data Visualizer terminal should
		be set to 200,000 bps 8n1. The DIG MCU sends two-byte commands
		so if you see long commands and short answers, you have to two
		wires swapped :-)

		If you see no traffic at all, turn the recorder off and on. The
		DIG MCU apparently stops talking when it sees a glitch on the
		serial port.

		Preliminary conclusions:
		* The MCU sends two-byte commands; probably the first is the actual
		  command and the second byte is a checksum.
		* Commands are sent with intervals of approximately 35 milliseconds
		  (not sure if that matters). Verified with logic analyzer and
		  oscilloscope; we're definitely fast enough to not miss any
		  data :-)
		* The msb of the first byte of the command is alternately 0 and 1.
		  This probably allows the deck controller MCU to detect whether
		  it missed a command. The msb of the first byte of the response
		  also alternates between 0 and 1 and matches the command msb.
		  There are a few holes in the list of commands; I wonder what the
		  missing commands do...
		* The command MCU mostly sends the 0x45/0xC5 command which returns
		  a status update of 11 bytes. When it sends another command
		  (e.g. to start playing), the response is a 2-byte packet.
		  An initialization command is answered with a single byte 00
		  A version command gets a 5 byte reply.
		* The apparent meaning of the bytes in the status packets are:
		  Byte 0: 0x80 or 0x00 (msb is same as first msb in command)
		  Byte 1-2: Deck status? Byte 2 almost (but not quite) looks like
		    it indicates things like the head position, winding direction,
			drawer motion etc. but it's not completely consistent.
		  Byte 3: Wind motor speed: 0=stop 1=play other=wind/rewind.
		    255=invalid
		  Byte 4-5: little-endian absolute tape counter in seconds. 
		    Counts backwards on side B. Counter resets when tape is
			removed and inserted. Only goes between 0000-9999 decimal.
		  Byte 7-9: Hours, minutes and seconds from beginning of side.
		    Counts forwards on both sides. Counter estimates tape
			position when tape is inserted. Negative values are possible
			and are encoded with a negative-flag-bit (not two's complement)
		  Byte 10: Checksum
	*/

	bool selrx = false;
	bool same = false;
	char command[2];
	char response[11];
	unsigned cmdix = 0;
	unsigned rspix = 0;

	char printbuf[80] = "";
	unsigned pix = 0;

	char outline[80] = "";
	unsigned oix = 0;

	for(;;)
	{
		if (USARTPA1_is_rx_ready())
		{
			uint8_t c = USARTPA1_read();

			if (selrx)
			{
				if (!same)
				{
					outline[oix] = '\0';

					// If we're in the middle of printing a line, print a line feed first.
					if (pix < _countof(printbuf))
					{
						printf("...\r\n"); // Indicates overrun (TODO: multiple buffers)
					}

					pix = 0;
					memcpy(printbuf, outline, sizeof(printbuf));
				}

				cmdix = 0;
				same = true;
				selrx = false;
				oix = 0;
			}

			if (cmdix < _countof(command))
			{
				uint8_t c1 = command[cmdix];

				// Compare the byte against the previous packet.
				// For the first byte, ignore the msb. Ignore the last byte.
				// Note: we're not comparing packet sizes but that doesn't matter: in practice
				// when packets of different sizes come in, they're (apparently) never a subsection
				// of a previous packet so that's not a problem.
				if (cmdix ? (c1 != c) : ((c1 & 0x7F) != (c & 0x7F)))
				{
					if (cmdix != _countof(command) - 1)
					{
						same = false;
					}
				}

				command[cmdix] = c;
			}

			oix += sprintf(outline + oix, "%02X ", c);

			if (!cmdix)
			{
				// The first byte is the command
				const char *cmd = NULL;

				switch(c & 0x7F)
				{
				case 0x01: cmd = "INIT"; break; // Init?
				case 0x02: cmd = "STOP"; break; // Stop
				case 0x03: cmd = "PLAY"; break; // Play

				case 0x05: cmd = "FFWD"; break; // Fast Forward (relative to current tape side)
				case 0x06: cmd = "REWD"; break; // Rewind (relative to current tape side)
				case 0x07: cmd = "NEXT"; break; // Fast Forward (rel to cur tape side) with head contact
				case 0x08: cmd = "PREV"; break; // Rewind (rel to cur tape side) with head contact
				
				case 0x0B: cmd = "LOAD"; break; // Close drawer
				case 0x0C: cmd = "OPEN"; break; // Open drawer
				case 0x0D: cmd = "RVRS"; break; // Switch to other side
				case 0x0E: cmd = "RSET"; break; // Reset counter

				case 0x42: cmd = "VERS"; break; // Get firmware version? 5 byte reply
				case 0x45: cmd = "STAT"; break; // Get State
				case 0x46: cmd = "CALI"; break; // Recalculate/recalibrate relative counter
				}

				if (cmd)
				{
					oix += sprintf(outline + oix, "%s ", cmd);
				}
			}

			cmdix++;
		}
		else if (USARTPC1_is_rx_ready())
		{
			uint8_t c = USARTPC1_read();

			if (!selrx)
			{
				rspix = 0;
				selrx = true;
				oix += sprintf(outline + oix, "-- ");
			}

			if (rspix < _countof(response))
			{
				uint8_t c1 = response[rspix];

				// Compare the byte against the previous packet.
				// For the first byte, ignore the msb. Ignore the last byte.
				// Note: we're not comparing packet sizes but that doesn't matter: in practice
				// when packets of different sizes come in, they're (apparently) never a subsection
				// of a previous packet so that's not a problem.
				if (rspix ? (c1 != c) : ((c1 & 0x7F) != (c & 0x7F)))
				{
					if (rspix != _countof(response) - 1)
					{
						same = false;
					}
				}

				response[rspix] = c;
			}

			switch (rspix)
			{
			case 2:
				// Bitmask for status
				{
					// H=heads engaged or fast forward?
					// T=time valid?
					// W=Winding?
					// R=Reverse search
					// S=speed valid?
					// L=drawer loading
					// D=drawer opening
					// ?=unused?
					char changed[] = "?DLSRWTH";

					for (unsigned u = 0; u < 8; u++)
					{
						if (!(c & (1 << u)))
						{
							changed[7 - u] = '_';
						}
					}

					oix += sprintf(outline + oix, "%02X %s ", c, changed);
				}
				break;

			case 3:
				// Wind motor speed
				switch(c)
				{
				case 0:
					oix += sprintf(outline + oix, "STOP "); // Stop
					break;

				case 1:
					oix += sprintf(outline + oix, "PLAY "); // Play
					break;

				case 255:
					oix += sprintf(outline + oix, "CAL? "); // Counter needs recalibration?
					break;

				default:
					oix += sprintf(outline + oix, ">%03u ", c); // Speed (0-63?)
				}
				break;

			case 4:
				// Don't print anything
				break;

			case 5:
				// This and previous = absolute tape time (0000-9999 decimal)
				{
					unsigned u = (((unsigned)c) << 8) + (unsigned)response[rspix - 1];
					oix += sprintf(outline + oix, "A%04u", u);
				}
				break;

			case 6:
				// 6/7/8 (Estimated) tape time relative to current side
				// NOTE: hours are 3 bits plus one sign bit (so "8" stands for "-0")
				oix += sprintf(outline + oix, "%c", " -"[(c & 8) >> 3]);
				c &= 7;
				// fall through

			case 7:
			case 8:
				oix += sprintf(outline + oix, "%02u%c", c, (rspix == 8 ? ' ' : ':'));
				break;

			default:
				oix += sprintf(outline + oix, "%02X ", c);
			}

			rspix++;
		}
		else if (USBSER_is_tx_ready())
		{
			// Done printing?
			if (pix < _countof(printbuf))
			{
				// Print character from the buffer; replace \0 with \r\n
				if (printbuf[pix])
				{
					printf("%c", printbuf[pix++]);
				}
				else
				{
					printf("\r\n");

					// Finish printing
					pix = _countof(printbuf);
				}
			}
		}
	}
}
