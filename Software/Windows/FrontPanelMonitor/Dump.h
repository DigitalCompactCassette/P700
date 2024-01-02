/****************************************************************************
SPI receiver API
(C) 2024 Jac Goudsmit
Licensed under the MIT license.
See LICENSE for details.
****************************************************************************/


#pragma once


/////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
/////////////////////////////////////////////////////////////////////////////


//---------------------------------------------------------------------------
// Process a command and response
void ProcessCommandResponse(
  BYTE *cmd,                            // Command
  size_t cmdlen,                        // Number of bytes in command
  BYTE *rsp,                            // Response
  size_t rsplen);                       // Number of bytes in response


/////////////////////////////////////////////////////////////////////////////
// END
/////////////////////////////////////////////////////////////////////////////
