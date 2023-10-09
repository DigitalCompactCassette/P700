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
// PUBLIC FUNCTIONS
/////////////////////////////////////////////////////////////////////////////


//---------------------------------------------------------------------------
// Initialize SPI receiver
bool SPIrx_init()
{
  FT_STATUS ftStatus = 0;

  DWORD numOfDevices = 0;
  ftStatus = FT_CreateDeviceInfoList(&numOfDevices);

  for (DWORD iDev = 0; iDev < numOfDevices; ++iDev)
  {
    FT_DEVICE_LIST_INFO_NODE devInfo;
    memset(&devInfo, 0, sizeof(devInfo));

    ftStatus = FT_GetDeviceInfoDetail(iDev, &devInfo.Flags, &devInfo.Type, &devInfo.ID, &devInfo.LocId,
      devInfo.SerialNumber,
      devInfo.Description,
      &devInfo.ftHandle);

    if (FT_OK == ftStatus)
    {
      printf("Dev %d:\n", iDev);
      printf("  Flags= 0x%x, (%s)\n", devInfo.Flags, ":-)"/*DeviceFlagToString(devInfo.Flags).c_str()*/);
      printf("  Type= 0x%x\n", devInfo.Type);
      printf("  ID= 0x%x\n", devInfo.ID);
      printf("  LocId= 0x%x\n", devInfo.LocId);
      printf("  SerialNumber= %s\n", devInfo.SerialNumber);
      printf("  Description= %s\n", devInfo.Description);
      printf("  ftHandle= 0x%p\n", devInfo.ftHandle);

      const std::string desc = devInfo.Description;
//      g_FTAllDevList.push_back(devInfo);

      if (desc == "FT4222" || desc == "FT4222 A")
      {
//        g_FT4222DevList.push_back(devInfo);
      }
    }
  }

  return true;
}


/////////////////////////////////////////////////////////////////////////////
// END
/////////////////////////////////////////////////////////////////////////////