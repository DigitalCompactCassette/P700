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


/////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
/////////////////////////////////////////////////////////////////////////////


//---------------------------------------------------------------------------
// Main program
int main(int argc, char const **argv)
{
  HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
  if (hStdIn == INVALID_HANDLE_VALUE)
  {
    fprintf(stderr, "Error getting standard input handle\n");
    return 1;
  }

  if (!SPIrx_init(argv))
  {
    fprintf(stderr, "Error initializing SPI receiver\n");
    return 1;
  }

  BYTE rxbuf[4096];

  for (;;)
  {
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
    WORD len = (WORD)sizeof(rxbuf);

    if (SPIrx_Receive(rxbuf, &len))
    {
      if (len)
      {
        for (size_t i = 0; i < len; i++)
        {
          printf("%02X ", rxbuf[i]);
        }

        printf("\n");
      }
      else
      {
        Sleep(10);
      }
    }
    else
    {
      fprintf(stderr, "Error receiving data\n");
      break;
    }
  }
Quit:

  SPIrx_exit();

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
// END
/////////////////////////////////////////////////////////////////////////////
