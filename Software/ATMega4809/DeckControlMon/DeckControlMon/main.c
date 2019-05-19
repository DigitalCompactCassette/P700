#include <atmel_start.h>
#include <stdio.h>

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
		  (not sure if that matters). Verified with logicx analyzer and
		  oscilloscope; we're definitely fast enough to not miss any
		  data :-)
		* The msb of the first byte of the command is alternately 0 and 1.
		  This probably allows the deck controller MCU to detect whether
		  it missed a command. The msb of the first byte of the response
		  also alternates between 0 and 1 and matches the command msb.
		* The command MCU mostly sends the 0x45/0xC5 command which returns
		  a status update of 11 bytes. When it sends another command
		  (e.g. to start playing), the response is a 2-byte packet.
		* The apparent meaning of the bytes in the status packets are:
		  Byte 0: 0x80 or 0x00 (msb is same as first msb in command)
		  Byte 1-2: Deck status? Possibly switches/sensors
		  Byte 3: Wind motor speed: 0=stop 1=play other=wind/rewind
		  Byte 4-5: little-endian absolute tape counter in seconds. 
		    Counts backwards on side B. Counter resets when tape is
			removed and inserted.
		  Byte 7-9: Hours, minutes and seconds from beginning of side.
		    Counts forwards on both sides. Counter estimates tape
			position when tape is inserted.
		  Byte 10: Checksum
		* For other commands and responses, I have to change the code to
		  reduce the amount of traffic. But let me save this code first :-)
	*/

	bool selrx = false;

	while (1)
	{
		if (USARTPA1_is_rx_ready())
		{
			if (selrx)
			{
				selrx = false;
				printf("\r\n");
			}

			printf("%02X ", USARTPA1_read());
		}

		if (USARTPC1_is_rx_ready())
		{
			if (!selrx)
			{
				selrx = true;
				printf("-- ");
			}

			printf("%02X ", USARTPC1_read());
		}
	}
}
