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
// TYPES
/////////////////////////////////////////////////////////////////////////////


class FT4222receiver
{
protected:
  FT_HANDLE hft = NULL;

public:
  //-------------------------------------------------------------------------
  // Open connection
  bool Open(DWORD reqIndex)
  {
    FT_STATUS ftStatus;

    if (hft)
    {
      Close();
    }

    // Open the selected device
    ftStatus = FT_Open(reqIndex, &hft);
    if (FT_OK != ftStatus)
    {
      printf("Error %u opening device\n", ftStatus);
      return false;
    }

    // Reset the chip, in case the shift registers are in the wrong state
    ftStatus = FT4222_ChipReset(hft);
    if (FT_OK != ftStatus)
    {
      printf("Error %u resetting chip\n", ftStatus);
      return false;
    }

    FT_Close(hft);
    hft = NULL;
    Sleep(1000);

    // Open the selected device
    reopen:
    ftStatus = FT_Open(reqIndex, &hft);
    if (FT_OK != ftStatus)
    {
      printf("Error %u reopening device\n", ftStatus);
      Sleep(100);
      goto reopen;// return false;
    }

    // Not sure if this is necessary
    ftStatus = FT4222_SetClock(hft, SYS_CLK_80);
    if (FT_OK != ftStatus)
    {
      printf("Error %u setting clock to 80MHz\n", ftStatus);
      return false;
    }

    // Set up for SPI slave mode
    ftStatus = FT4222_SPISlave_InitEx(hft, SPI_SLAVE_NO_PROTOCOL);
    if (FT_OK != ftStatus)
    {
      printf("Error %u opening setting device to SPI slave mode\n", ftStatus);
      return false;
    }

    // Set up for SPI mode 3
    ftStatus = FT4222_SPISlave_SetMode(hft, CLK_IDLE_HIGH, CLK_TRAILING);
    if (FT_OK != ftStatus)
    {
      printf("Error %u setting SPI slave clock mode\n", ftStatus);
      return false;
    }

    // Set driving strength of the pins
    ftStatus = FT4222_SPI_SetDrivingStrength(hft, DS_4MA, DS_4MA, DS_4MA);
    if (FT_OK != ftStatus)
    {
      printf("Error %u setting SPI Slave driving strength\n", ftStatus);
      return false;
    }

    // Set receive buffer size
    ftStatus = FT_SetUSBParameters(hft, 4 * 1024, 0);
    if (FT_OK != ftStatus)
    {
      printf("Error %u setting receive buffer size\n", ftStatus);
      return false;
    }

    return true;
  }


public:
  //-------------------------------------------------------------------------
  // Close connection
  void Close()
  {
    FT4222_UnInitialize(hft);
    FT_Close(hft);
    hft = NULL;
  }


public:
  //-------------------------------------------------------------------------
  // Receive data
  bool Receive(                         // Returns true=success
    BYTE *buffer,                       // Receive buffer
    WORD *pbufsize)                     // Input buf size, output rcvd bytes
  {
    if (!buffer || !pbufsize || !*pbufsize)
    {
      printf("Need buffer and size\n");
      return false;
    }

    uint16 rxSize;
    FT_STATUS ftStatus = FT4222_SPISlave_GetRxStatus(hft, &rxSize);
    if (FT_OK != ftStatus)
    {
      printf("Error %u getting receive status\n", ftStatus);
      return false;
    }

    if (!rxSize)
    {
      *pbufsize = 0;
    }
    else
    {
      if (rxSize > *pbufsize)
      {
        rxSize = *pbufsize;
      }

      ftStatus = FT4222_SPISlave_Read(hft, buffer, rxSize, pbufsize);
      if (FT_OK != ftStatus)
      {
        printf("Error %u receiving data\n", ftStatus);
        return false;
      }
    }

    static const BYTE reverse_byte[] = {
      0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
      0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
      0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
      0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
      0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
      0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
      0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
      0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
      0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
      0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
      0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
      0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
      0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
      0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
      0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
      0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
    };

    BYTE *p = buffer;
    for (int i = *pbufsize; i > 0; i--)
    {
      *p++ = reverse_byte[*p];
    }

    return true;
  }
};


/////////////////////////////////////////////////////////////////////////////
// DATA
/////////////////////////////////////////////////////////////////////////////


FT4222receiver Rx[2];


/////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS
/////////////////////////////////////////////////////////////////////////////


// --------------------------------------------------------------------------
// Look up location in the device info list, or show the list of devices
//
// This should be called after calling FT_CreateDeviceInfoList; that function
// provides the number of devices detected by the FTDI libraries.
//
// The return value is the index of the device with the requested location.
// If the requested location isn't found, the function returns the number
// of devices.
//
// If the location parameter is set to 0, the code prints a list of all
// devices, otherwise it only prints information for the device if the
// requested location was found.
DWORD                                   // Returns index, too high=not found
static LocationToIndex(
  DWORD numdevices,                     // Number of devices detected
  DWORD location)                       // Location to convert, 0=show list
{
  DWORD result;

  for (result = 0; result < numdevices; result++)
  {
    FT_DEVICE_LIST_INFO_NODE devInfo = { 0 };

    FT_STATUS ftStatus = FT_GetDeviceInfoDetail(
      result,
      &devInfo.Flags,
      &devInfo.Type,
      &devInfo.ID,
      &devInfo.LocId,
      devInfo.SerialNumber,
      devInfo.Description,
      &devInfo.ftHandle);

    if (FT_OK == ftStatus)
    {
      if (location == 0 || location == devInfo.LocId)
      {
        printf("Dev %d:\n", result);
        printf("  Flags= 0x%x\n", devInfo.Flags);
        printf("  Type= 0x%x\n", devInfo.Type);
        printf("  ID= 0x%x\n", devInfo.ID);
        printf("  LocId= 0x%x\n", devInfo.LocId);
        printf("  SerialNumber= %s\n", devInfo.SerialNumber);
        printf("  Description= %s\n", devInfo.Description);
        printf("  ftHandle= 0x%p\n", devInfo.ftHandle);

        if (location == devInfo.LocId)
        {
          break;
        }
      }
    }
  }

  return result;
}


/////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
/////////////////////////////////////////////////////////////////////////////


//---------------------------------------------------------------------------
// Initialize SPI receiver
unsigned                                // Returns number of devices opened
SPIrx_init(
  const char **argv)                    // Program arguments
{
  unsigned result;
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
  const char **psLoc;
  for (psLoc = argv + 1, result = 0; (*psLoc) && (result < _countof(Rx)); psLoc++, result++)
  {
    // Read hex number as location
    char *s;
    DWORD reqLoc = strtoul(*psLoc, &s, 16);
    if (*s)
    {
      printf("Invalid location ID %s (expected hex number)\n", *psLoc);
      break;
    }

    // If the location is 0, break out to print a list
    if (!reqLoc)
    {
      break;
    }

    // Find the index of the device with the given location
    DWORD index = LocationToIndex(numOfDevices, reqLoc);
    if (index >= numOfDevices)
    {
      printf("No device found at location 0x%X\n", reqLoc);
      break;
    }

    // Open the device
    if (!Rx[result].Open(index))
    {
      printf("Error opening receiver %u location 0x%X index %u\n", result, reqLoc, index);
      break;
    }

    printf("Opened receiver %u location 0x%X index %u\n", result, reqLoc, index);
  }

  // If there were no parameters or something went wrong, print a list of
  // detected devices
  if ((psLoc == argv + 1) || (*psLoc))
  {
    (void)LocationToIndex(numOfDevices, 0);
    printf("\nSpecify locations on the command line as hex numbers\n");
    return false;
  }

  printf("%u device(s) opened\n", result);

  return result;
}


//---------------------------------------------------------------------------
// Shutdown SPI receiver
void SPIrx_exit()
{
  for (int i = 0; i < _countof(Rx); i++)
  {
    Rx[i].Close();
  }
}


//---------------------------------------------------------------------------
// Receive data
bool                                    // Returns true=success
SPIrx_Receive(
  int rxindex,                          // Receiver index
  BYTE *buffer,                         // Receive buffer
  WORD *pbufsize)                       // Input buf size, output rcvd bytes
{
  if (rxindex >= _countof(Rx))
  {
    printf("Index out of range");
    return false;
  }

  return Rx[rxindex].Receive(buffer, pbufsize);
}


/////////////////////////////////////////////////////////////////////////////
// END
/////////////////////////////////////////////////////////////////////////////
