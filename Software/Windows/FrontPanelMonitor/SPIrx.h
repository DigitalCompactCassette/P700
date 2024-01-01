/****************************************************************************
SPI receiver API
(C) 2023 Jac Goudsmit
Licensed under the MIT license.
See LICENSE for details.
****************************************************************************/


#pragma once


/////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
/////////////////////////////////////////////////////////////////////////////


//---------------------------------------------------------------------------
// Initialize SPI receiver
unsigned                                // Returns number of devices opened
SPIrx_init(
  const char **argv);                   // Program arguments


//---------------------------------------------------------------------------
// Shutdown SPI receiver
void SPIrx_exit();


//---------------------------------------------------------------------------
// Receive data
bool                                    // Returns true=success
SPIrx_Receive(
  int index,                            // Receiver index
  BYTE *buffer,                         // Receive buffer
  WORD *pbufsize);                      // Input buf size, output rcvd bytes


/////////////////////////////////////////////////////////////////////////////
// END
/////////////////////////////////////////////////////////////////////////////
