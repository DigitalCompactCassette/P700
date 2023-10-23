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
bool SPIrx_init(                        // Returns true=success
  const char **argv);                   // Program arguments


//---------------------------------------------------------------------------
// Shutdown SPI receiver
void SPIrx_exit();


//---------------------------------------------------------------------------
// Receive data
bool SPIrx_Receive(                     // Returns true=success
  BYTE *buffer,                         // Receive buffer
  WORD *pbufsize);                      // Input buf size, output rcvd bytes


/////////////////////////////////////////////////////////////////////////////
// END
/////////////////////////////////////////////////////////////////////////////
