/****************************************************************************
Processing module using the screen
(C) 2024 Jac Goudsmit
Licensed under the MIT license.
See LICENSE for details.
****************************************************************************/


/////////////////////////////////////////////////////////////////////////////
// INCLUDES
/////////////////////////////////////////////////////////////////////////////


#include <iostream>
#include <cstdint>
#include <windows.h>

#include "Proc.h"

using namespace std;


/////////////////////////////////////////////////////////////////////////////
// MACROS
/////////////////////////////////////////////////////////////////////////////


// Virtual Terminal Sequences
#define CSI "\x1B["                     // Control Sequence Introducer
#define CUPY(y) CSI << y << "H"         // Cursor Position (Y only)(1=top)
#define CUP(y, x) CSI << y << ";" << x << "H"
                                        // Cursor Position (X/Y)(1=top/left)


#define PRE_VU(ch) CUPY(1 + ch)
#define PRE_DECKSTATE CUPY(3)
#define PRE_DECKFUNCTION CUPY(4)
#define PRE_POLLSTATUS CUPY(5) // 3 lines
#define PRE_DRAWERSTATUS CUPY(8)
#define PRE_TRACKTITLE CUPY(9)
#define PRE_LONGTEXT(y) CUPY(10 + y) // 5 lines


/////////////////////////////////////////////////////////////////////////////
// LOCAL FUNCTIONS
/////////////////////////////////////////////////////////////////////////////


//---------------------------------------------------------------------------
// Print a string
void static printstring(
  const uint8_t *begin,
  const uint8_t *end)
{
  putc('\"', stdout);

  for (const uint8_t *s = begin; s != end; s++)
  {
    printf((((*s < 32) || (*s >= 0x7E)) ? "\\x%02X" : "%c"), *s);
  }

  putc('\"', stdout);
}


//---------------------------------------------------------------------------
// Command 0x5E: Show VU meters
//
// Data:
// [0] = error code (0=ok)
// [1] = left channel VU meter value
// [2] = right channel VU meter value
//
// Values are absolute dB values, i.e. 0=loudest, 95=silence
void ShowVU(
  uint8_t *rsp)                         // Response data
{
  // Lookup table to generate the number of segments to light up on a
  // 40-segment dBFS scale meter based on the absolute segment value
  // (0=full scale, 97=quietest)
  static const unsigned dblut40[] =
  {
    40, // 0 dB
    39,
    38,
    37,
    36,
    35, // -5 dB
    34,
    33,
    32,
    31,
    30, // -10 dB
    29,
    28,
    27,
    26,
    25, // -15 dB
    24,
    23,
    22,
    21,
    20, // -20 dB
    19,
    18,
    18,
    17,
    16, // -25 dB
    15,
    14,
    13,
    12,
    12, // -30 dB
    11,
    11,
    10,
    9,
    9, // -35 dB
    8,
    7,
    7,
    6,
    6, // -40 dB
    5,
    5,
    5,
    4,
    4, // -45 dB
    4,
    3,
    3,
    3,
    3, // -50 dB
    2,
    2,
    2,
    2,
    2, // -55dB
    2,
    2,
    2,
    2,
    2, // -60dB
    1,
    1,
    1,
    1,
    1, // -65 dB
    1,
    1,
    1,
    1,
    1, // -70 dB
    1,
    1,
    1,
    1,
    1, // -75 dB
    1,
    1,
    1,
    1,
    1, // -80 dB
    1,
    1,
    1,
    1,
    1, // -85 dB
    1,
    1,
    1,
    1,
    1, // -90 dB
    1,
    1,
    1,
    1,
    0, // -95 dB
  };
  const char vu[] = "========================================";
  const char bl[] = "                                        ";

  for (int i = 0; i < 2; i++)
  {
    unsigned level = rsp[i + 1];
    unsigned u = level < 95 ? dblut40[level] : 0;
    cout << PRE_VU(i) << &vu[40 - u] << &bl[u];
  }
}


//---------------------------------------------------------------------------
// Command 0x60: Show deck controller state
//
// Data:
// [0] Error code (0=ok)
// [1] Status? 8=play?
// [2] Track number
// [3] hours (negative is indicated in a weird way, I forgot how)
// [4] minutes
// [5] seconds
// [6] unknown
// [7] tape counter, high part
// [8] tape counter, low part
// [9] unknown
//
// All values are in big-endian BCD, e.g. 0x59 should be displayed as "59".
void ShowDeckState(
  BYTE *rsp)                            // Response data
{
  cout << PRE_DECKSTATE;
  printf("T%02X %X:%02X:%02X C%02X%02X [%02X %X %02X %02X]",
    rsp[2], rsp[3] & 0xF, rsp[4], rsp[5], rsp[7], rsp[8],
    rsp[1], rsp[3] >> 4, rsp[6], rsp[9]);
}


//---------------------------------------------------------------------------
// Command 0x58: Show deck function
//
// Data
// [0] Error code (0=ok)
// [1] Deck function, see below
//
// Deck functions are:
// 0x01=Stand by
// 0x02=Stop
// 0x03=Read
// 0x04=Play
//
// 0x0A=Fast forward
// 0x0B=Rewind
//
// 0x0E=Unknown (seen during Append)
// 
// 0x11=Skip Forward
// 0x12=Skip Backwards
//
// 0x15=Arriving at track, going backwards
// 0x16=Arriving at track, going forwards
//
// 0x19=Unknown (seen during Append)
// 
// 0x22=Unknown (seen while recording title)
// 0x23=Unknown (seen while recording title)
// 0x24=Unknown (seen while recording title)
// 0x26=Unknown (seen while recording title)
// 
// 0x2A=End of recording marker found
// 0x2B=Ready for recording? Seen during Rec/Pause
//
// 0x30=Skipping intro (lead in)
//
// 0x32=Unknown (seen during Append)
// 
// 0x34=Unknown (seen during Append)
void ShowDeckFunction(
  BYTE *rsp)                            // Response data
{
  cout << PRE_DECKFUNCTION;

  switch (rsp[1])
  {
  case 0x01: printf("OFF ");        return; // Stand by
  case 0x02: printf("STOP");        return; // Stop
  case 0x03: printf("READ");        return; // Reading
  case 0x04: printf("PLAY");        return; // Play

  case 0x0A: printf("FFWD");        return; // FFWD (sector)
  case 0x0B: printf("REWD");        return; // Rewind (sector)

  case 0x11: printf("NEXT");        return; // Search forwards
  case 0x12: printf("PREV");        return; // Search backwards

  case 0x15: printf("SBY<");        return; // Search arriving at track
  case 0x16: printf("SBY>");        return; // Search arriving at track

  case 0x2A: printf("END ");        return; // End of recording marker found

  case 0x30: printf("SKIP");        return; // Skipping intro

  case 0x22:
  case 0x23:
  case 0x24:
  case 0x26: // All used when recording title

  case 0x19:
  case 0x0E:
  case 0x32:
  case 0x34: // Seen during APPEND

  case 0x2B: // REC/PAUSE?

  default:
    printf("%02X\r\n", rsp[1]);         return; // TODO: decode other codes
  }

  return;
}


//---------------------------------------------------------------------------
// Command 0x41: Show poll status
void ShowPollStatus(
  uint8_t *rsp)                         // Response data
{
  cout << PRE_POLLSTATUS;

  // Interpret bits
  uint8_t a = rsp[1];
  cout << ((a & 0x01) ? "SYSTEM " : "       "); // System (issue Get System State)
  cout << ((a & 0x02) ? "COUNTER " : "        "); // Counter update (issue Get Deck State)
  cout << ((a & 0x04) ? "TIME ": "     "); // Time update (issue Get Deck State)
  cout << ((a & 0x08) ? "FUNCTION ": "         "); // Function change? (issue Get Function State)
  cout << ((a & 0x10) ? "DRAWER ": "       "); // Drawer change (issue Get Drawer State)
  cout << ((a & 0x20) ? "EOT ": "    "); // End of Tape (sector)
  cout << ((a & 0x40) ? "BOT ": "    "); // Beginning of Tape (sector)
  cout << ((a & 0x80) ? "FAST ": "     "); // Winding/rewinding without heads applied; time is deck time?

  cout << endl;
  a = rsp[2];
  cout << ((a & 0x01) ? "LYRICS ": "       "); // Lyrics (issue Get DCC Long Text)
  cout << ((a & 0x02) ? "MARKER ": "       "); // Marker change (issue Get Marker)
  cout << ((a & 0x04) ? "(B4) ": "     "); // unknown
  cout << ((a & 0x08) ? "(B8) ": "     "); // unknown
  cout << ((a & 0x10) ? "(B10) ": "      "); // unknown
  cout << ((a & 0x20) ? "TRACK ": "      "); // Track info available? Seen when playing past track marker
  cout << ((a & 0x40) ? "ABSTIME ": "        "); // Absolute time known (SUDCC/PDCC)?
  cout << ((a & 0x80) ? "TOTALTIME ": "          "); // Total time known on PDCC? FP issues PREREC TAPE TIME

  cout << endl;
  a = rsp[3];
  cout << ((a & 0x80) ? "DECKTIME ": "         "); // No absolute tape time, using deck time?
  cout << ((a & 0x40) ? "TAPETIME ": "         "); // Using tape time code
  
  cout << "Sector=" << (a & 3);
}


//---------------------------------------------------------------------------
// Command 0x46: Get drawer status
void ShowDrawerStatus(
  uint8_t *rsp)
{
  cout << PRE_DRAWERSTATUS << "Drawer ";

  switch (rsp[1])
  {
  case 1: printf("Closed ");        return;
  case 2: printf("Open   ");        return;
  case 3: printf("Closing");        return;
  case 4: printf("Opening");        return;
  case 5: printf("Blocked");        return;
  default:
          printf("Unknown");        return;
  }
}


//---------------------------------------------------------------------------
// Command 0x51: Show Long Text
void ShowLongText(
  uint8_t *cmd,
  uint8_t *rsp,
  size_t rsplen)
{
  switch (cmd[1])
  {
  case 0xFA: cout << PRE_LONGTEXT(0) << "Track           -> "; break; // Get track name?
  case 0xE0: cout << PRE_LONGTEXT(1) << "TOC track name? -> "; break; // Not sure; Used when rewinding sudcc to beginning, but returns error
  case 0x01: cout << PRE_LONGTEXT(2) << "Lyrics / Album? -> "; break; // Possibly language number for lyrics?
  case 0x03: cout << PRE_LONGTEXT(3) << "Artist          -> "; break; // Album artist on PDCC
  default:   cout << PRE_LONGTEXT(4) << cmd[1] << " -> ";
  }

  printstring(rsp + 1, rsp + rsplen);
}


//---------------------------------------------------------------------------
// Command 0x52: Get Track Title
void ShowTrackTitle(
  uint8_t *cmd,
  uint8_t *rsp,
  size_t rsplen)
{
  cout << PRE_TRACKTITLE;

  printf("Track %2u -> ", cmd[1]);
  printstring(rsp + 1, rsp + rsplen);
}


/////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
/////////////////////////////////////////////////////////////////////////////


//---------------------------------------------------------------------------
// Parse a buffer
void ProcessCommandResponse(
  BYTE *cmd,
  size_t cmdlen,
  BYTE *rsp,
  size_t rsplen)
{
  {
    static bool screencleared = false;

    if (!screencleared)
    {
      // 2J=clear screen, H=home, ?25l=cursor off
      printf("\x1B[2J\x1B[H\x1B[?25l");
      screencleared = true;
    }
  }

  if (cmd) *cmd &= 0x7F;
  if (rsp) *rsp &= 0x7F;
  if (cmdlen) cmdlen--;
  if (rsplen) rsplen--;

  switch (*cmd)
  {
    // Shortcuts
// #define P(name) printf("%02X %s", cmd[0], name)
// #define QCR(name, wantedcmdlen, wantedrsplen) do { P(name); if ((cmdlen != wantedcmdlen) || (rsplen != wantedrsplen) || (rsp[0] != 0)) goto dump; } while(0)
// #define QC(name, wantedcmdlen) QCR(name, wantedcmdlen, 1) // Command with parameters, no return data
// #define QR(name, wantedrsplen) QCR(name, 1, wantedrsplen) // Command without parameters, has return data
// #define Q(name) QCR(name, 1, 1); do { fputs("\r\n", stdout); return; } while(0) // Command with no parameters, no return data

#if 0
  case 0x02: Q("DECK: STOP");
  case 0x03: Q("DECK: PLAY");
  case 0x05: Q("DECK: FFWD");
  case 0x06: Q("DECK: REWIND");
  case 0x0B: Q("DECK: CLOSE");
  case 0x0C: Q("DECK: OPEN");

  case 0x10:
    // Key or remote command. These are ignored by the dig MCU
    QC("KEY/RC: ", 2);

    switch (cmd[1])
    {
      // Note: These numbers correspond to the numbers shown on the front
      // panel during the Key Test program of the Service Mode
    case 0x01: printf("SIDE A/B\r\n");      return;
    case 0x02: printf("OPEN/CLOSE\r\n");    return;
    case 0x03: printf("EDIT\r\n");          return;
    case 0x04: printf("REC/PAUSE\r\n");     return;
    case 0x05: printf("STOP\r\n");          return;
    case 0x06: printf("REPEAT\r\n");        return;
    case 0x07: printf("DOLBY\r\n");         return;
    case 0x08: printf("SCROLL\r\n");        return;
    case 0x09: printf("RECLEVEL-\r\n");     return;
    case 0x0A: printf("APPEND\r\n");        return;
    case 0x0B: printf("PLAY\r\n");          return;
    case 0x0C: printf("PRESETS\r\n");       return;
    case 0x0D: printf("TIME\r\n");          return;
    case 0x0E: printf("TEXT\r\n");          return;
    case 0x0F: printf("RECLEVEL+\r\n");     return;
    case 0x10: printf("RECORD\r\n");        return;
    case 0x11: printf("NEXT\r\n");          return;
    case 0x12: printf("PREV\r\n");          return;

      // Remote control
      // "PAUSE" and "COUNTER RESET" and "WRITE MARK" don't show up
      //
      // NOTE: Numbers in comments are the codes shown in the service manual
      // for the key test program
    case 0x1C: printf("RC FFWD\r\n");       return; // 052?
    case 0x1D: printf("RC OPEN/CLOSE\r\n"); return; // 045
    case 0x1F: printf("RC REWIND\r\n");     return; // 050?
    case 0x20: printf("RC 0\r\n");          return; // 000
    case 0x21: printf("RC 1\r\n");          return; // 001
    case 0x22: printf("RC 2\r\n");          return; // 002
    case 0x23: printf("RC 3\r\n");          return; // 003
    case 0x24: printf("RC 4\r\n");          return; // 004
    case 0x25: printf("RC 5\r\n");          return; // 005
    case 0x26: printf("RC 6\r\n");          return; // 006
    case 0x27: printf("RC 7\r\n");          return; // 007
    case 0x28: printf("RC 8\r\n");          return; // 008
    case 0x29: printf("RC 9\r\n");          return; // 009
    case 0x2C: printf("RC STANDBY\r\n");    return; // 012
      // I couldn't reproduce the following codes from the service manual
      // with my Logitech Harmony universal remote control. I may verify
      // these later.
                                                    // 011 TIME
                                                    // 047 SIDE A/B
                                                    // 028 REPEAT
                                                    // 054 STOP
                                                    // 053 PLAY
                                                    // 040 REC SELECT/PAUSE
                                                    // 117 APPEND
                                                    // 055 RECORD
                                                    // 121 EDIT
                                                    // 103 REC LEVEL -
                                                    // 102 REC LEVEL +
                                                    // 015 SCROLL/DEMO
                                                    // 122 TEXT
                                                    // 063 DCC
    default:
      ; // Nothing
    }

    break;

  case 0x23:
    // Set repeat mode
    QC("REPEAT MODE: ", 2);

    switch (cmd[1])
    {
    case 1: printf("None\r\n");    return;
    case 2: printf("Track\r\n");   return;
    case 3: printf("All\r\n");     return;
    default:
      ; // Nothing
    }

    break;

  case 0x29:
    // Rec/pause?
    break;

  case 0x2A:
    // Sector. This is issued after the 10 01 (SIDE A/B) command.
    // Presumably the sector can be 1 to 4 inclusive but without 4-sector
    // tapes, we won't know...
    QC("SECTOR: ", 2);

    printf("%u\r\n", cmd[1]);

    return;

  case 0x2F:
    // Go to track (pdcc only?)
    QC("GO TO TRACK: ", 3);

    printf("To=%u, [2]=%u\r\n", cmd[1], cmd[2]);

    return;

  case 0x35:
    // 35 18 42 10 seen for REC/PAUSE. Configure inputs perhaps?
    break;

  case 0x36:
    // Send text
    QC("SET TEXT: ", 42);

    switch (cmd[1])
    {
    case 0xFD: printf("DECKID="); break;  // Sent at initialization time to store deck ID
    case 0xFA: printf("TITLE=");  break;  // Sent when editing title of current song
    default:
      printf("%02X ", cmd[1]);
    }

    printstring(cmd + 2, cmd + cmdlen);
    fputs("\r\n", stdout);

    return;

  case 0x37:
    // Search relative from current track.
    QC("DECK: SEARCH: ", 3);

    printhex(rsp + 1, rsp + 3);
    fputs("\r\n", stdout);

    return;
    /*
        // What the second parameter byte means is not clear, seems to be always 1
        if (rsp[1] < 100)
        {
          // You can search forwards by 1-99 tracks
          printf("+%u [%02X]\r\n", rsp[1], rsp[2]);
          return;
        }
        else // if?...
        {
          // Searching backwards it uses EE=-0, ED=-1 etc. Weird.
          printf("-%u [%02X]\r\n", 0xEE - rsp[1], rsp[2]);
          return;
        }

        break;
    */

  case 0x38:
    // Time mode. This is issued after the TIME command (10 0D)
    // Note: for [super] user tapes, the display has a VU mode too:
    // Press TIME enough times and it will only send TIME but no time mode
    // command.
    // For ACC's a counter will appear in one of the modes, this also
    // doesn't issue a time mode command.
    QC("TIME MODE: ", 2);

    switch (cmd[1])
    {
    case 1: printf("TOTAL TIME\r\n");     return; // prerec/dcc/acc
    case 2: printf("TOT REM TIME\r\n");   return; // prerec
    case 3: printf("TRACK TIME\r\n");     return; // prerec/sudcc

    case 5: printf("REM TIME\r\n");       return; // non-prerecorded
    default:
      ; // Nothing
    }

    break;

  case 0x39:
    // Read DCC. This is issued after inserting a DCC cassette.
    Q("READ DCC.");

  case 0x3C:
    // Write DCC. This is issued after setting the text for the current track
    Q("WRITE DCC.");

#endif
  case 0x41:
    // Poll status
    ShowPollStatus(rsp);
    break;

#if 0
  case 0x44:
    QR("GET SYSTEM STATUS -> ", 2);

    switch (rsp[1])
    {
    case 0x06: printf("CHECK DIG IN\r\n");  return;
    case 0x10: printf("CLEAN HEADS\r\n");   return;
    case 0x1F: printf("POWER FAIL\r\n");    return;

    case 0x0D: // Seen when pushing A/B on remote just after pushing open/close twice in quick succession. No text on screen.
    case 0x1A: // Seen while playing DCC175 recorded tape in service mode
    default:
      printf("%02X\r\n", rsp[1]);           return;
    }

#endif

  case 0x46:
    // Get drawer status
    ShowDrawerStatus(rsp);
    break;

#if 0

  case 0x49:
    // Get tape type? This is issued right after the drawer finishes closing
    QR("TAPE TYPE -> ", 2);

    printf("(%02X) ", rsp[0]);

    switch (rsp[1])
    {
      // Meanings of bits:
      //  0x01: No cassette
      //  0x02: Chrome
      //  0x04: DCC
      //  0x08: Recording Allowed
      //  0x10: Length Hole "3" (45/75/105/Undefined)
      //  0x20: Length Hole "4" (45/60/105/120)
      //  0x40: Length Hole "5" (45/60/75/90)
      // Length can be ("5"/"4"/"3"):
      //  45  minutes  (1/1/1)
      //  60  minutes  (1/1/0)
      //  75  minutes  (1/0/1)
      //  90  minutes  (1/0/0)
      //  105 minutes  (0/1/1)
      //  120 minutes  (0/1/0)
      //  Undefined    (0/0/1) Reserved
      //  Undefined    (0/0/0) Also used for prerecorded DCC
      // These numbers correspond to the decimal numbers that are shown by
      // the "Switches Test" program in Service Mode
    case 0x00: printf("ACC FERRO\r\n");   return; // 000
    case 0x02: printf("ACC CHROME\r\n");  return; // 002
    case 0x04: printf("PDCC\r\n");        return; // 004
    case 0x14: printf("UDCC(PROT)\r\n");  return; // 020
    case 0x1C: printf("UDCC\r\n");        return; // 028
    case 0x24: printf("DCC120(PROT)\r\n"); return; // 036
    case 0x2C: printf("DCC120\r\n");      return; // 044
    case 0x34: printf("DCC105(PROT)\r\n"); return; // 052
    case 0x3C: printf("DCC105\r\n");      return; // 060
    case 0x44: printf("DCC90(PROT)\r\n"); return; // 068
    case 0x4C: printf("DCC90\r\n");       return; // 076
    case 0x54: printf("DCC75(PROT)\r\n"); return; // 084
    case 0x5C: printf("DCC75\r\n");       return; // 092
    case 0x64: printf("DCC60(PROT)\r\n"); return; // 100
    case 0x6C: printf("DCC60\r\n");       return; // 108
    case 0x74: printf("DCC45(PROT)\r\n"); return; // 116
    case 0x7B: printf("NO CASSETTE\r\n"); return; // 123
    case 0x7C: printf("DCC45\r\n");       return; // 124
    default:
      printf("%02X\r\n", rsp[1]);         return;
    }

    break;

#endif

  case 0x51:
    // Get long text (invoked when you push the Text button on the front panel)
    ShowLongText(cmd, rsp, rsplen);
    return;

  case 0x52:
    // Get track title
    ShowTrackTitle(cmd, rsp, rsplen);
    return;

#if 0
  case 0x53:
    // Get short text
    QCR("GET SHORT TEXT -> ", 2, 13);

    switch (cmd[1])
    {
    case 0xFA: printf("Track -> ");                 break; // Get track name
      // Other codes see 0x51?
    default:   printf("%02X -> ", cmd[1]);
    }

    printstring(rsp + 1, rsp + rsplen);
    fputs("\r\n", stdout);

    return;

  case 0x54:
    // Get short track title
    QCR("GET SHORT TRACK TITLE: ", 2, 13);

    printf("Track %u -> ", cmd[1]);
    printstring(rsp + 1, rsp + rsplen);
    fputs("\r\n", stdout);

    return;

  case 0x55:
    // Get DDU2113 ID. Issued at startup before sending the front panel ID
    QR("Get DDU ID -> ", 5);

    printhex(rsp + 1, rsp + rsplen);
    fputs("\r\n", stdout);

    return;

  case 0x57:
    // Get Marker Type?
    QR("MARKER TYPE -> ", 2);

    switch (rsp[1])
    {
    case 0x02: printf("TRACK\r\n");       return;
    case 0x03: printf("REVERSE\r\n");     return; // Switch to side B
    case 0x07: printf("SKIP +1\r\n");     return; // Skip marker? Also seen at beginning of 175-recorded tape
    case 0x0B: printf("REUSE\r\n");       return; // End of recording, beginning of reusable tape
    case 0x0D: printf("INTRO SKIP\r\n");  return; // Skip over begin of sector 1
    case 0x14: printf("BEGIN SEC\r\n");   return; // After reversing
    case 0x0E:
    default:
      printf("%02X\r\n", rsp[1]);
    }

    return;
#endif

  case 0x58:
    ShowDeckFunction(rsp);
    return;

#if 0
  case 0x5B:
    // Set something (at search time). cmdlen=2 rsplen=4
    break;

  case 0x5D:
    // Get Target Track number
    // This is the positive or negative number that's shown in the track
    // display during search. It also gets polled when playing past a
    // marker.
    QR("GET TARGET TRACK -> ", 2);

    printf("%d\r\n", rsp[1]);           return;

    return;
#endif
  case 0x5E:
    ShowVU(rsp);
    return;

#if 0
  case 0x5F:
    // Service mode playback error reporting.
    // The command parameter byte indicates the requested track (1-9)
    // or is set to 0x10 to request a bit pattern of all the (main) heads.
    // The bit order is: head 1=0x80, head 2=0x40, head 3=0x20 etc.
    // The first byte of the response is always 0 (so the AUX track is
    // not part of the result for more 0x10). 
    // The second response byte is between 0 and 20 decimal for individual
    // heads. According to the service manual, the individual head error
    // counts can basically be multiplied by 5 to get the percentage of
    // errors.
    QCR("BITS ", 2, 2);

    printf("%02X -> %02X %02X\r\n", cmd[1], rsp[0], rsp[1]);

    return;
#endif
  case 0x60:
    ShowDeckState(rsp);
    return;

#if 0
  case 0x61:
    // Get tape info for prerecorded tape
    QR("PREREC TAPE INFO -> ", 6);

    printf("[1]=0x%02X Tracks=%02X Total time=%02X:%02X:%02X\r\n",
      rsp[1], rsp[2], rsp[3], rsp[4], rsp[5]);

    return;
#endif
  default:
    ;// Nothing
  }

/*
dump:
  // If we got here, we don't understand the command. Just dump it.
  // Note, we don't dump the checksums.
  fputs("?? ", stdout);
  hexdumpmessage(cmd, cmdlen, rsp, rsplen);
*/
}


/////////////////////////////////////////////////////////////////////////////
// END
/////////////////////////////////////////////////////////////////////////////
