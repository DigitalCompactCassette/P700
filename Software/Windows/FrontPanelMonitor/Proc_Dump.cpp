/****************************************************************************
SPI receiver API
(C) 2024 Jac Goudsmit
Licensed under the MIT license.
See LICENSE for details.
****************************************************************************/


/////////////////////////////////////////////////////////////////////////////
// INCLUDES
/////////////////////////////////////////////////////////////////////////////


#include <windows.h>
#include <cstdio>
#include <cstdint>

#include "Proc.h"


/////////////////////////////////////////////////////////////////////////////
// DATA
/////////////////////////////////////////////////////////////////////////////


static bool chattymode = true;


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
// Print hex data
void static printhex(
  const uint8_t *begin,
  const uint8_t *end)
{
  for (const uint8_t *s = begin; s != end; s++)
  {
    static const char hex[] = "0123456789ABCDEF";

    fputc(hex[*s >> 4], stdout);
    fputc(hex[*s & 0xF], stdout);
    fputc(' ', stdout);
  }
}


//---------------------------------------------------------------------------
// Print VU
//
// VU values are from 0 to 95 and represent the negative dB value of the
// audio channel: 0 is the loudest (0dB), 95 is the quietest (-95dB).
inline const char *vustring(
  uint8_t vu)
{
  const char *vubar = "================";
  const unsigned vu_limit = 45;

  if (vu > vu_limit)
  {
    vu = vu_limit;
  }

  return vubar + (((unsigned)vu * 16) / (vu_limit + 1));
}


//---------------------------------------------------------------------------
// Hexdump a command and response
void static hexdumpmessage(
  uint8_t *cmd,
  size_t cmdlen,
  uint8_t *rsp,
  size_t rsplen)
{
  printhex(cmd, cmd + cmdlen); // Command
  fputs("-- ", stdout);
  printhex(rsp, rsp + rsplen); // Response
  fputs("\r\n", stdout);
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
  if (cmd) *cmd &= 0x7F;
  if (rsp) *rsp &= 0x7F;
  if (cmdlen) cmdlen--;
  if (rsplen) rsplen--;

  switch (*cmd)
  {
    // Shortcuts
#define P(name) printf("%02X %s", cmd[0], name)
#define QCR(name, wantedcmdlen, wantedrsplen) do { P(name); if ((cmdlen != wantedcmdlen) || (rsplen != wantedrsplen) || (rsp[0] != 0)) goto dump; } while(0)
#define QC(name, wantedcmdlen) QCR(name, wantedcmdlen, 1) // Command with parameters, no return data
#define QR(name, wantedrsplen) QCR(name, 1, wantedrsplen) // Command without parameters, has return data
#define Q(name) QCR(name, 1, 1); do { fputs("\r\n", stdout); return; } while(0) // Command with no parameters, no return data

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

  case 0x41:
    // Poll status
    // This is generated often and is very chatty.
    // We cache it and only show it when it changes.
  {
    static uint8_t status[4] = { 0 };

    if ((cmdlen == 1) && (rsplen == sizeof(status)))
    {
      if ((status[0] != rsp[0])
        || ((status[1] & 0xF9) != (rsp[1] & 0xF9)) // Those bits change too often while running. Tachos?
        || (status[2] != rsp[2])
        || (status[3] != rsp[3]))
      {
        P("POLL -> from=");
        printhex(status, status + sizeof(status));
        fputs("to=", stdout);
        printhex(rsp, rsp + rsplen);

        // Interpret bits
        if (rsp[1] & 0x01) fputs("SYSTEM ", stdout); // System (issue Get System State)
        //if (rsp[1] & 0x02) fputs("(A2) ",       stdout); // ignored; toggles too fast.
        //if (rsp[1] & 0x04) fputs("(A4) ",       stdout); // ignored; toggles too fast
        if (rsp[1] & 0x08) fputs("FUNCTION ", stdout); // Function change? (issue Get Function State)
        if (rsp[1] & 0x10) fputs("DRAWER ", stdout); // Drawer change (issue Get Drawer State)
        if (rsp[1] & 0x20) fputs("EOT ", stdout); // End of Tape (sector)
        if (rsp[1] & 0x40) fputs("BOT ", stdout); // Beginning of Tape (sector)
        if (rsp[1] & 0x80) fputs("FAST ", stdout); // Winding/rewinding without heads applied; time is deck time?

        if (rsp[2] & 0x01) fputs("LYRICS ", stdout); // Lyrics (issue Get DCC Long Text)
        if (rsp[2] & 0x02) fputs("MARKER ", stdout); // Marker change (issue Get Marker)
        if (rsp[2] & 0x04) fputs("(B4) ", stdout);
        if (rsp[2] & 0x08) fputs("(B8) ", stdout);
        if (rsp[2] & 0x10) fputs("(B10) ", stdout);
        if (rsp[2] & 0x20) fputs("TRACK ", stdout); // Track info available? Seen when playing past track marker
        if (rsp[2] & 0x40) fputs("ABSTIME ", stdout); // Absolute time known (SUDCC/PDCC)?
        if (rsp[2] & 0x80) fputs("TOTALTIME ", stdout); // Total time known on PDCC? FP issues PREREC TAPE TIME

        if (rsp[3] & 0x80) fputs("DECKTIME ", stdout); // No absolute tape time, using deck time?
        if (rsp[3] & 0x40) fputs("TAPETIME ", stdout); // Using tape time code
        printf("Sector=%u\r\n", rsp[3] & 3);

        memcpy(status, rsp, sizeof(status));
      }
      else
      {
        // Nothing changed; don't print anything
      }

      return;
    }
  }
  break;

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

  case 0x46:
    // Get drawer status
    QR("GET DRAWER STATUS -> ", 2);

    switch (rsp[1])
    {
    case 1: printf("Closed\r\n");         return;
    case 2: printf("Open\r\n");           return;
    case 3: printf("Closing\r\n");        return;
    case 4: printf("Opening\r\n");        return;
    case 5: printf("Blocked\r\n");        return;
    case 6: printf("Unknown\r\n");        return;
    default:
      ; // Nothing
    }

    break;

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

  case 0x51:
    // Get long text
    QCR("GET LONG TEXT: ", 2, 41);

    switch (cmd[1])
    {
    case 0xFA: printf("Track -> ");                 break; // Get track name?
    case 0xE0: printf("TOC track name -> ");        break; // Not sure; Used when rewinding sudcc to beginning, but returns error
    case 0x01: printf("Lyrics / Album Title -> ");  break; // Possibly language number for lyrics?
    case 0x03: printf("Artist -> ");                break; // Album artist on PDCC
    default:   printf("%02X -> ", cmd[1]);
    }

    printstring(rsp + 1, rsp + rsplen);
    fputs("\r\n", stdout);

    return;

  case 0x52:
    // Get track title
    QCR("GET TRACK TITLE: ", 2, 41);

    printf("Track %u -> ", cmd[1]);
    printstring(rsp + 1, rsp + rsplen);
    fputs("\r\n", stdout);

    return;

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

  case 0x58:
    // Get Function State. This is apparently used to update the symbols
    // on the front panel display
    QR("FUNCTION STATE -> ", 2);

    switch (rsp[1])
    {
    case 0x01: printf("OFF \r\n");        return; // Stand by
    case 0x02: printf("STOP\r\n");        return; // Stop
    case 0x03: printf("READ\r\n");        return; // Reading
    case 0x04: printf("PLAY\r\n");        return; // Play

    case 0x0A: printf("FFWD\r\n");        return; // FFWD (sector)
    case 0x0B: printf("REWD\r\n");        return; // Rewind (sector)

    case 0x11: printf("NEXT\r\n");        return; // Search forwards
    case 0x12: printf("PREV\r\n");        return; // Search backwards

    case 0x15: printf("SBY<\r\n");        return; // Search arriving at track
    case 0x16: printf("SBY>\r\n");        return; // Search arriving at track

    case 0x2A: printf("END \r\n");        return; // End of recording marker found

    case 0x30: printf("SKIP\r\n");        return; // Skipping intro

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

  case 0x5E:
    // VU meters, 2 bytes between 00-5F. 5F (95 decimal) is silence.
    // Values are negative decibels for left and right.
    if (chattymode)
    {
      // Cursor off
      fputs("\x1B[?25l", stdout);
      QR("VU -> ", 3);

      // No line feed so the text window doesn't scroll
      printf("%16s %-16s\r", vustring(rsp[1]), vustring(rsp[2]));
      // Cursor on
      fputs("\x1B[?25h", stdout);
    }

    return;

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

  case 0x60:
    // Get Time (and state?) from deck controller. All values in BCD, big endian.
    // byte 0=error 00=ok
    // byte 1=status, 8=play?
    // byte 2=track
    // byte 3/4/5=time in BCD, hh/mm/ss. Negative is indicated in hours-byte; don't remember how :)
    // byte 6=?
    // byte 7/8=tape counter, 0-9999 
    // byte 9=?
  {
    static uint8_t track;

    if ((chattymode) || (rsp[2] != track))
    {
      // Cursor off
      fputs("\x1B[?25l", stdout);
      P("Time -> ");
      //printf("Track %02X Time %X:%02X:%02X Counter %02X%02X [1/2=%02X%X 6=%02X 9=%02X]\r\n", 
      //printf("                                T%02X %X:%02X:%02X C%02X%02X [%02X %X %02X %02X]\r", 
      // ESC [ <n> C is cursor forward by n places
      printf("\x1B[32CT%02X %X:%02X:%02X C%02X%02X [%02X %X %02X %02X]\r",
        rsp[2], rsp[3] & 0xF, rsp[4], rsp[5], rsp[7], rsp[8],
        rsp[1], rsp[3] >> 4, rsp[6], rsp[9]);
      // Cursor on
      fputs("\x1B[?25h", stdout);

      track = rsp[2];
    }
  }
  return;

  case 0x61:
    // Get tape info for prerecorded tape
    QR("PREREC TAPE INFO -> ", 6);

    printf("[1]=0x%02X Tracks=%02X Total time=%02X:%02X:%02X\r\n",
      rsp[1], rsp[2], rsp[3], rsp[4], rsp[5]);

    return;

  default:
    ;// Nothing
  }

dump:
  // If we got here, we don't understand the command. Just dump it.
  // Note, we don't dump the checksums.
  fputs("?? ", stdout);
  hexdumpmessage(cmd, cmdlen, rsp, rsplen);
}


/////////////////////////////////////////////////////////////////////////////
// END
/////////////////////////////////////////////////////////////////////////////
