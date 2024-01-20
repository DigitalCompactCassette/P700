/****************************************************************************
Main program
(C) 2023 Jac Goudsmit
Licensed under the MIT license.
See LICENSE for details.
****************************************************************************/


/////////////////////////////////////////////////////////////////////////////
// INCLUDES
/////////////////////////////////////////////////////////////////////////////


#include <windows.h>
#include <cstdio>

#include "SPIrx.h"
#include "Proc.h"


/////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
/////////////////////////////////////////////////////////////////////////////


//---------------------------------------------------------------------------
// Main program
int main(int argc, char const **argv)
{
  // Get the standard input handle so we can wait for it to handle keys
  HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
  if (hStdIn == INVALID_HANDLE_VALUE)
  {
    fprintf(stderr, "Error getting standard input handle\n");
    exit(1);
  }

  // Get the standard output handle so we can enable terminal processing
  HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hStdOut == INVALID_HANDLE_VALUE)
  {
    fprintf(stderr, "Error getting standard output handle\n");
    exit(1);
  }

  // Enable terminal processing
  DWORD dwOutMode;
  if (!GetConsoleMode(hStdOut, &dwOutMode))
  {
    fprintf(stderr, "Error getting console output mode\n");
    exit(1);
  }
  if (!SetConsoleMode(hStdOut, dwOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
  {
    fprintf(stderr, "Error setting console output mode\n");
    exit(1);
  }

  // Initialize receivers
  unsigned numReceivers = SPIrx_init(argv);
  if (!numReceivers)
  {
    fprintf(stderr, "No receivers initialized.\n");
    exit(1);
  }

  // Main loop
  BYTE rxbuf[2][4096]; // Input buffers
  unsigned len[2] = { 0 }; // Number of bytes in each buffer

  const char *fmtCommand = (numReceivers > 1) ? "\x1B[31;1m" : "";
  const char *fmtResponse = (numReceivers > 1) ? "\x1B[32;1m" : "";
  const char *fmtDefault = (numReceivers > 1) ? "\x1B[0m" : "";

  bool screencleared = false;

  for (;;)
  {
    // Check for keyboard input in the console window
    if (WaitForSingleObject(hStdIn, 0) == WAIT_OBJECT_0)
    {
      INPUT_RECORD ir;
      DWORD n;
      if (ReadConsoleInput(hStdIn, &ir, 1, &n) && n && ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown)
      {
        switch (ir.Event.KeyEvent.uChar.AsciiChar)
        {
        case 'q':
        case 'Q':
          goto Quit;
        }
      }
    }

    // Read data from all receivers
    WORD readLen[2] = { 0 };
    for (unsigned rxindex = 0; rxindex < numReceivers; rxindex++)
    {
      // Allow remaining buffer size for reading
      readLen[rxindex] = (WORD)(sizeof(rxbuf[rxindex]) - len[rxindex]);

      if (readLen[rxindex])
      {
        if (!SPIrx_Receive(rxindex, rxbuf[rxindex] + len[rxindex], &readLen[rxindex]))
        {
          printf("Error reading from device %u\n", rxindex);
          exit(1);
        }

        len[rxindex] += (unsigned)readLen[rxindex];
      }
    }

    // Find how many bytes are in all buffers
    unsigned bufLen = len[0];
    for (unsigned rxindex = 1; rxindex < numReceivers; rxindex++)
    {
      if (bufLen > len[rxindex])
      {
        bufLen = len[rxindex];
      }
    }

    // Analyze the buffers
    // We assume that the buffers start with a command of 0 or more bytes,
    // followed by a response of 1 or more bytes,
    // followed by the next command of 1 or more bytes.
    // Assuming that the first command can be zero bytes is to deal with the
    // possibility that at startup, we may be receiving bytes of an
    // incomplete response to a command that we missed.

    // Search for the end of the command
    unsigned resStart = bufLen;
    if (bufLen)
    {
      for (unsigned byteindex = 0; byteindex < resStart; byteindex++)
      {
        // Search for any significant bytes in the buffers other than 0
        for (unsigned rxindex = 1; rxindex < numReceivers; rxindex++)
        {
          if (rxbuf[rxindex][byteindex] != 0xFF)
          {
            resStart = byteindex;
            break;
          }
        }
      }
    }

    // Search for the end of the response
    unsigned nextCmdStart = bufLen;
    if (bufLen)
    {
      for (unsigned byteindex = resStart; byteindex < bufLen; byteindex++)
      {
        if (rxbuf[0][byteindex] != 0xFF)
        {
          nextCmdStart = byteindex;
          break;
        }
      }
    }

    // If we have an entire command and response, process the buffer
    if (bufLen && ((numReceivers == 1) || (nextCmdStart < bufLen)))
    {

      // Dump the buffer
      if (nextCmdStart)
      {
        ProcessCommandResponse(rxbuf[0], resStart, rxbuf[1] + resStart, nextCmdStart - resStart);
/*
        unsigned byteindex;

        printf("%s%s", (screencleared ? "" : "\x1B[2J\x1B[0;0f"), fmtCommand);
        screencleared = true;

        for (byteindex = 0; byteindex < resStart; byteindex++)
        {
          printf("%02X ", rxbuf[0][byteindex]);
        }

        printf("%s", fmtResponse);
        for (; byteindex < nextCmdStart; byteindex++)
        {
          printf("%02X ", rxbuf[1][byteindex]);
        }

        printf("%s\n", fmtDefault);
*/
      }

      // Remove all processed data from the buffers
      for (unsigned rxindex = 0; rxindex < numReceivers; rxindex++)
      {
        memmove(rxbuf[rxindex], rxbuf[rxindex] + nextCmdStart, len[rxindex] - nextCmdStart);
        len[rxindex] -= nextCmdStart;
      }
    }
    else
    {
      Sleep(10);
    }
  }
Quit:

  SPIrx_exit();

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
// END
/////////////////////////////////////////////////////////////////////////////
