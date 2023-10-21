/****************************************************************************
SPI receiver using FTDI FT-4222 under Windows
(C) 2023 Jac Goudsmit
Licensed under the MIT license.
See LICENSE file for details.
****************************************************************************/


/////////////////////////////////////////////////////////////////////////////
// INCLUDES
/////////////////////////////////////////////////////////////////////////////


#include <windows.h>
#include <cstdio>
#include <string>

#include "ftd2xx.h"
#include "LibFT4222.h"

#include "SPIrx.h"

#pragma comment(lib, "ftd2xx.lib")
#pragma comment(lib, "libft4222.lib")


/////////////////////////////////////////////////////////////////////////////
// DATA
/////////////////////////////////////////////////////////////////////////////


FT_HANDLE rx1 = NULL;


/////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
/////////////////////////////////////////////////////////////////////////////


//---------------------------------------------------------------------------
// Initialize SPI receiver
bool SPIrx_init(const char **argv)
{
  FT_STATUS ftStatus = 0;

  // Find out how many devices there are
  DWORD numOfDevices = 0;
  ftStatus = FT_CreateDeviceInfoList(&numOfDevices);
  if (!numOfDevices)
  {
    printf("No devices found\n");
    return false;
  }

  // Parse the command line
  DWORD reqLoc = 0;
  DWORD reqIndex = numOfDevices;
  if (argv[1])
  {
    char *s;
    reqLoc = strtoul(argv[1], &s, 16);
    if (*s)
    {
      printf("Invalid Location ID %s (expected hex number)", argv[1]);
      reqLoc = numOfDevices;
    }
  }

  // Either print the entire list of devices or find the selected device
  for (DWORD iDev = 0; iDev < numOfDevices; ++iDev)
  {
    FT_DEVICE_LIST_INFO_NODE devInfo = { 0 };

    ftStatus = FT_GetDeviceInfoDetail(
      iDev,
      &devInfo.Flags,
      &devInfo.Type,
      &devInfo.ID,
      &devInfo.LocId,
      devInfo.SerialNumber,
      devInfo.Description,
      &devInfo.ftHandle);

    if (FT_OK == ftStatus)
    {
      if (reqLoc == 0 || reqLoc == devInfo.LocId)
      {
        printf("Dev %d:\n", iDev);
        printf("  Flags= 0x%x\n", devInfo.Flags);
        printf("  Type= 0x%x\n", devInfo.Type);
        printf("  ID= 0x%x\n", devInfo.ID);
        printf("  LocId= 0x%x\n", devInfo.LocId);
        printf("  SerialNumber= %s\n", devInfo.SerialNumber);
        printf("  Description= %s\n", devInfo.Description);
        printf("  ftHandle= 0x%p\n", devInfo.ftHandle);

        if (reqLoc == devInfo.LocId)
        {
          printf("Opening...\n");
          reqIndex = iDev;
        }
      }
    }
  }

  // If we didn't find the requested device, bail out
  if (reqIndex == numOfDevices)
  {
    if (reqLoc == 0)
    {
      printf("Select a receiver device by putting a hex location ID on the command line\n");
    }
    else
    {
      printf("Location 0x%X is invalid\n", reqLoc);
    }

    return false;
  }

  // Open the selected device
  ftStatus = FT_Open(reqIndex, &rx1);
  if (FT_OK != ftStatus)
  {
    printf("Error %u opening device", ftStatus);
    return false;
  }

/* This shouldn't be necessary
  ftStatus = FT4222_SetClock(ftHandle, SYS_CLK_80);
  if (FT_OK != ftStatus)
  {
    printf("Error %u setting clock to 80MHz\n", ftStatus);
    return false;
  }
*/

  // Set up for SPI slave mode
  ftStatus = FT4222_SPISlave_InitEx(rx1, SPI_SLAVE_NO_PROTOCOL);
  if (FT_OK != ftStatus)
  {
    printf("Error %u opening setting device to SPI slave mode", ftStatus);
    return false;
  }

  // Set driving strength of the pins
  ftStatus = FT4222_SPI_SetDrivingStrength(rx1, DS_4MA, DS_4MA, DS_4MA);
  if (FT_OK != ftStatus)
  {
    printf("Error %u setting SPI Slave driving strength\n", ftStatus);
    return false;
  }

  // Set receive buffer size
  ftStatus = FT_SetUSBParameters(rx1, 4 * 1024, 0);
  if (FT_OK != ftStatus)
  {
    printf("Error %u setting receive buffer size\n", ftStatus);
    return false;
  }

  // Done
  printf("Location 0x%X opened successfully", reqLoc);
  return true;
}


/////////////////////////////////////////////////////////////////////////////
// END
/////////////////////////////////////////////////////////////////////////////