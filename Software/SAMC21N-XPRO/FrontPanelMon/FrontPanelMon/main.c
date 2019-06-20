/**
 * \file
 *
 * \brief Application implement
 *
 * Copyright (c) 2019 Jac Goudsmit
 *
 * MIT License
 *
 */

#include "atmel_start.h"
#include "atmel_start_pins.h"
#include <string.h>
#include <stdio.h>

#include "jg_stdio_redirect.h"

/*
  This program is intended to reverse-engineer the data that goes over the
  bus between the front panel and the digital board microcontroller of the
  DCC-730. It probably also works on the DCC-951, FW-68 and DCC-771 since
  they all use the same hardware (there's a small chance that there are
  differences in the firmware but this is unlikely).
  
  I used a JST PH 7-pin connector and header, some ribbon cable and a
  dual row .1" header to make it easy to tap into the signals. Later on,
  I expect that I'll make a circuit that disconnects the bus if/when
  necessary and allows my microcontroller to take over some of the
  functionality of the dig-mcu.
  
  For reference, here are the signals on header 1605:
  1: SLAVE_OUT  (serial traffic from dig-mcu to front panel)
  2: GNDD       (digital ground)
  3: MESSYNC    (message sync, can be used as Slave Select)
  4: SLAVE_IN   (serial traffic from front panel to dig-mcu)
  5: NRESET     (reset)
  6: CLOCK      (serial clock)
  7: HOLD       (used to let the front panel wait for an operation?)
  
  With a logic analyzer, it was pretty easy to deduct that the SLAVE_OUT,
  SLAVE_IN and CLOCK lines are compatible with SPI mode 3, LSB first. My
  ancient logic analyzer can't do protocol analysis, so that's why I wrote
  this program: to figure out what the messages mean.
*/


#define DEBOUNCE_COUNT 5

typedef struct
{
  uint8_t buf[255];
  uint8_t len; // Total length
  uint8_t rsp; // Index of response

} buf_t;


bool chattymode = false;

struct io_descriptor *spi_cmd; // From front panel
struct io_descriptor *spi_rsp; // From dig-mcu

char cmdbuf[256];
char rspbuf[256];
struct ringbuffer rb_cmd;
struct ringbuffer rb_rsp;


//---------------------------------------------------------------------------
// Receive callback for command input
void cmd_rx_callback(struct spi_s_async_descriptor *spi)
{
  uint8_t rxbyte;

  ringbuffer_get(&spi->rx_rb, &rxbyte); // always successful
  ringbuffer_put(&rb_cmd, rxbyte);
}


//---------------------------------------------------------------------------
// Receive callback for response input
void rsp_rx_callback(struct spi_s_async_descriptor *spi)
{
  uint8_t rxbyte;

  ringbuffer_get(&spi->rx_rb, &rxbyte); // always successful
  ringbuffer_put(&rb_rsp, rxbyte);
}


//---------------------------------------------------------------------------
// Reinitialize hardware
void reinit(void)
{
  atmel_start_init();

  // Redirect stdio to the EXT3 SPI port.
  // This port is connected to the EDBG chip but also to other devices.
  // See the jg_stdio_redirect.c module for info why the module is needed.
  jg_stdio_redirect_init(&SPI_EXT3, SPI_EDBG_SS);

  ringbuffer_init(&rb_cmd, cmdbuf, sizeof(cmdbuf));
  ringbuffer_init(&rb_rsp, rspbuf, sizeof(rspbuf));

  spi_s_async_get_io_descriptor(&SPI_EXT1, &spi_cmd);
  spi_s_async_get_io_descriptor(&SPI_EXT2, &spi_rsp);

  spi_s_async_register_callback(&SPI_EXT1, SPI_S_CB_RX, (FUNC_PTR)cmd_rx_callback);
  spi_s_async_register_callback(&SPI_EXT2, SPI_S_CB_RX, (FUNC_PTR)rsp_rx_callback);
  spi_s_async_enable(&SPI_EXT1);
  spi_s_async_enable(&SPI_EXT2);
}


//---------------------------------------------------------------------------
// Check the button and disable the SPI ports if it's pressed.
//
// The SPI ports are enabled again when the button is released.
//
// Returns true if the function is debouncing the switch input.
bool check_button(void)
{
  static unsigned count;

  if (gpio_get_pin_level(SW0))
  {
    // Button released
    if (count == DEBOUNCE_COUNT)
    {
      //reinit();

      chattymode = !chattymode;
      gpio_set_pin_level(LED0, chattymode);
    }

    count = 0;
  }
  else
  {
    // Button is pressed
    if (count < DEBOUNCE_COUNT)
    {
      count++;
    }
    else if (count == DEBOUNCE_COUNT)
    {
      spi_s_async_disable(&SPI_EXT2);
      spi_s_async_disable(&SPI_EXT1);
    }
  }

  return (count != 0);
}


//---------------------------------------------------------------------------
// Print a string
void printstring(const uint8_t *begin, const uint8_t *end)
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
void printhex(const uint8_t *begin, const uint8_t *end)
{
  for (const uint8_t *s = begin; s != end; s++)
  {
    static const char hex[] = "01234567890ABCDEF";

    fputc(hex[*s >>   4], stdout);
    fputc(hex[*s &  0xF], stdout);
    fputc(' ', stdout);
  }
}


//---------------------------------------------------------------------------
// Print VU
//
// VU values are from 0 to 95 and represent the negative dB value of the
// audio channel: 0 is the loudest (0dB), 95 is the quietest (-95dB).
inline const char *vustring(uint8_t vu)
{
  const char *vubar = "================";

  if (vu > 95)
  {
    vu = 95;
  }

  return vubar + (((uint)vu * 16) / 96);
}


//---------------------------------------------------------------------------
// Parse a buffer
void parsebuffer(buf_t *buf, bool valid)
{
  if ((buf->len < 4) || (buf->rsp < 2))
  {
    // We didn't get at least 2 bytes for command and 2 bytes for response.
    printf("IGNORING: ");
    printhex(buf->buf, buf->buf + buf->rsp);
    fputs("-- ", stdout);
    printhex(buf->buf + buf->rsp, buf->buf + buf->len);
    fputc('\n', stdout);
    return;
  }

  // Aliases for improved readability
  uint8_t *cmd = &buf->buf[0];
  uint8_t *rsp = &buf->buf[buf->rsp];

  // Lengths without checksum bytes
  uint8_t cmdlen = buf->rsp - 1;
  uint8_t rsplen = (buf->len - buf->rsp) - 1;

  if (!valid)
  {
    // Don't try to interpret something that we already know is wrong
    fputs("CHECKSUM ERROR: ", stdout);
  }
  else
  {
    // Reset first bits. These alternate between 1 and 0 on subsequent
    // commands and responses, probably to ensure that the firmware always
    // works on live data, not on stale data that happens when things stop
    // responding because of some technical problem.
    (*cmd) &= 0x7F;
    (*rsp) &= 0x7F;

    // Dump command byte to help navigate the code
    //printhex(cmd, cmd + 1);

    switch(*cmd)
    {
// Shortcuts
#define P(name) printf("%02X %s", cmd[0], name)
#define QCR(name, wantedcmdlen, wantedrsplen) do { P(name); if ((cmdlen != wantedcmdlen) || (rsplen != wantedrsplen) || (rsp[0] != 0)) goto dump; } while(0)
#define QC(name, wantedcmdlen) QCR(name, wantedcmdlen, 1) // Command with parameters, no return data
#define QR(name, wantedrsplen) QCR(name, 1, wantedrsplen) // Command without parameters, has return data
#define Q(name) QCR(name, 1, 1); do { fputc('\n', stdout); return; } while(0) // Command with no parameters, no return data

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
      case 0x01: printf("SIDE A/B\n");      return;
      case 0x02: printf("OPEN/CLOSE\n");    return;
      case 0x03: printf("EDIT\n");          return;
      case 0x04: printf("REC/PAUSE\n");     return;
      case 0x05: printf("STOP\n");          return;
      case 0x06: printf("REPEAT\n");        return;
      case 0x07: printf("DOLBY\n");         return;
      case 0x08: printf("SCROLL\n");        return;
      case 0x09: printf("RECLEVEL-\n");     return;
      case 0x0A: printf("APPEND\n");        return;
      case 0x0B: printf("PLAY\n");          return;
      case 0x0C: printf("PRESETS\n");       return;
      case 0x0D: printf("TIME\n");          return;
      case 0x0E: printf("TEXT\n");          return;
      case 0x0F: printf("RECLEVEL+\n");     return;
      case 0x10: printf("RECORD\n");        return;
      case 0x11: printf("NEXT\n");          return;
      case 0x12: printf("PREV\n");          return;

      // Remote control
      // "PAUSE" and "COUNTER RESET" and "WRITE MARK" don't show up
      //
      // NOTE: Numbers in comments are the codes shown in the service manual
      // for the key test program
      case 0x1C: printf("RC FFWD\n");       return; // 052?
      case 0x1D: printf("RC OPEN/CLOSE\n"); return; // 045
      case 0x1F: printf("RC REWIND\n");     return; // 050?
      case 0x20: printf("RC 0\n");          return; // 000
      case 0x21: printf("RC 1\n");          return; // 001
      case 0x22: printf("RC 2\n");          return; // 002
      case 0x23: printf("RC 3\n");          return; // 003
      case 0x24: printf("RC 4\n");          return; // 004
      case 0x25: printf("RC 5\n");          return; // 005
      case 0x26: printf("RC 6\n");          return; // 006
      case 0x27: printf("RC 7\n");          return; // 007
      case 0x28: printf("RC 8\n");          return; // 008
      case 0x29: printf("RC 9\n");          return; // 009
      case 0x2C: printf("RC STANDBY\n");    return; // 012
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

      switch(cmd[1])
      {
      case 1: printf("None\n");    return;
      case 2: printf("Track\n");   return;
      case 3: printf("All\n");     return;
      default:
        ; // Nothing
      }

      break;

    case 0x2A:
      // Sector. This is issued after the 10 01 (SIDE A/B) command.
      // Presumably the sector can be 1 to 4 inclusive but without 4-sector
      // tapes, we won't know...
      QC("SECTOR: ", 2);

      printf("%u\n", cmd[1]);

      return;

    case 0x2F:
      // Go to track (pdcc only?)
      QC("GO TO TRACK: ", 3);

      printf("To=%u, [2]=%u\n", cmd[1], cmd[2]);

      return;

    case 0x36:
      // Recorder ID. Sent to dig-mcu after reset
      QC("FRONT PANEL ID: ", 42);

      printstring(cmd + 1, cmd + cmdlen);
      putc('\n', stdout);

      return;

    case 0x37:
      // Search relative from current track.
      QC("DECK: SEARCH: ", 3);

      printhex(rsp + 1, rsp + 3);
      fputc('\n', stdout);

      return;
/*
      // What the second parameter byte means is not clear, seems to be always 1
      if (rsp[1] < 100)
      {
        // You can search forwards by 1-99 tracks
        printf("+%u [%02X]\n", rsp[1], rsp[2]);
        return;
      }
      else // if?...
      {
        // Searching backwards it uses EE=-0, ED=-1 etc. Weird.
        printf("-%u [%02X]\n", 0xEE - rsp[1], rsp[2]);
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

      switch(cmd[1])
      {
      case 1: printf("TOTAL TIME\n");     return; // prerec/dcc/acc
      case 2: printf("TOT REM TIME\n");   return; // prerec
      case 3: printf("TRACK TIME\n");     return; // prerec/sudcc

      case 5: printf("REM TIME\n");       return; // non-prerecorded
      default:
        ; // Nothing
      }

      break;

    case 0x39:
      // Read DCC. This is issued after inserting a DCC cassette.
      Q("READ DCC.");

    case 0x41:
      // Poll status
      // This is generated often and is very chatty.
      // We cache it and only show it when it changes.
      {
        static uint8_t status[4] = {0};

        if ((cmdlen == 1) && (rsplen == sizeof(status)))
        {
          if ( ( status[0] !=          rsp[0])
            || ((status[1] & 0xF9) != (rsp[1] & 0xF9)) // Those bits change too often while running. Tachos?
            || ( status[2] !=          rsp[2])
            || ( status[3] !=          rsp[3]))
          {
            P("POLL -> from=");
            printhex(status, status + sizeof(status));
            fputs("to=", stdout);
            printhex(rsp, rsp + rsplen);

            // Interpret bits
            if (rsp[1] & 0x01) fputs("SYSTEM ",     stdout); // System (issue Get System State)
            //if (rsp[1] & 0x02) fputs("(A2) ",       stdout); // ignored; toggles too fast.
            //if (rsp[1] & 0x04) fputs("(A4) ",       stdout); // ignored; toggles too fast
            if (rsp[1] & 0x08) fputs("FUNCTION ",   stdout); // Function change? (issue Get Function State)
            if (rsp[1] & 0x10) fputs("DRAWER ",     stdout); // Drawer change (issue Get Drawer State)
            if (rsp[1] & 0x20) fputs("EOT ",        stdout); // End of Tape (sector)
            if (rsp[1] & 0x40) fputs("BOT ",        stdout); // Beginning of Tape (sector)
            if (rsp[1] & 0x80) fputs("(A80) ",      stdout);

            if (rsp[2] & 0x01) fputs("(B1) ",       stdout);
            if (rsp[2] & 0x02) fputs("MARKER ",     stdout); // Marker change (issue Get Marker)
            if (rsp[2] & 0x04) fputs("(B4) ",       stdout);
            if (rsp[2] & 0x08) fputs("(B8) ",       stdout);
            if (rsp[2] & 0x10) fputs("(B10) ",      stdout);
            if (rsp[2] & 0x20) fputs("(B20) ",      stdout);
            if (rsp[2] & 0x40) fputs("(B40) ",      stdout);
            if (rsp[2] & 0x80) fputs("(B80) ",      stdout);

            if (rsp[3] & 0x80) fputs("DECKTIME ",   stdout); // No absolute tape time, using deck time?
            if (rsp[3] & 0x40) fputs("TAPETIME ",   stdout); // Using tape time code
            printf("Sector=%u\n", rsp[3] & 3);

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

      switch(rsp[1])
      {
      case 0x10: printf("CLEAN HEADS\n"); return;
      default:
        printf("%u\n", rsp[1]);           return;
      }

    case 0x46:
      // Get drawer status
      QR("GET DRAWER STATUS -> ", 2);

      switch(rsp[1])
      {
      case 1: printf("Closed\n");         return;
      case 2: printf("Open\n");           return;
      case 3: printf("Closing\n");        return;
      case 4: printf("Opening\n");        return;
      case 5: printf("Blocked\n");        return;
      case 6: printf("Unknown\n");        return;
      default:
        ; // Nothing
      }

      break;

    case 0x49:
      // Get tape type? This is issued right after the drawer finishes closing
      QR("TAPE TYPE -> ", 2);

      switch(rsp[1])
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
      // These numbers correspond to the numbers that are shown by the
      // "Switches Test" program in Service Mode
      case 0x00: printf("ACC FERRO\n");   return; // 000
      case 0x02: printf("ACC CHROME\n");  return; // 002
      case 0x04: printf("PDCC\n");        return; // 004
      case 0x14: printf("UDCC(PROT)\n");  return; // 020
      case 0x1C: printf("UDCC\n");        return; // 028
      case 0x24: printf("DCC120(PROT)\n");return; // 036
      case 0x2C: printf("DCC120\n");      return; // 044
      case 0x34: printf("DCC105(PROT)\n");return; // 052
      case 0x3C: printf("DCC105\n");      return; // 060
      case 0x44: printf("DCC90(PROT)\n"); return; // 068
      case 0x4C: printf("DCC90\n");       return; // 076
      case 0x54: printf("DCC75(PROT)\n"); return; // 084
      case 0x5C: printf("DCC75\n");       return; // 092
      case 0x64: printf("DCC60(PROT)\n"); return; // 100
      case 0x6C: printf("DCC60\n");       return; // 108
      case 0x74: printf("DCC45(PROT)\n"); return; // 116
      case 0x7B: printf("NO CASSETTE\n"); return; // 123
      case 0x7C: printf("DCC45\n");       return; // 124
      default:
        printf("%02X\n", rsp[1]);         return;
      }

      break;

    case 0x51:
      // Get long title from [s]udcc
      QCR("GET LONG UDCC TEXT -> ", 2, 41);

      if (cmd[1] == 0xFA) // E0 is also used when rewinding sudcc to beginning, but returns error
      {
        printstring(rsp + 1, rsp + rsplen);
        putc('\n', stdout);
        return;
      }
      break;

    case 0x52:
      // Get long title from pdcc
      QCR("GET LONG PDCC TEXT: ", 2, 41);

      printf("Track %u -> ", cmd[1]);
      printstring(rsp + 1, rsp + rsplen);
      putc('\n', stdout);

      return;

    case 0x53:
      // Get short title from [s]udcc
      QCR("GET SHORT UDCC TEXT -> ", 2, 13);

      if (cmd[1] == 0xFA) // E0 is also used when rewinding sudcc to beginning, but returns error
      {
        printstring(rsp + 1, rsp + rsplen);
        putc('\n', stdout);
        return;
      }
      break;

    case 0x54:
      // Get short title from [s]udcc
      QCR("GET SHORT PDCC TEXT: ", 2, 13);

      printf("Track %u -> ", cmd[1]);
      printstring(rsp + 1, rsp + rsplen);
      putc('\n', stdout);

      return;

    case 0x55:
      // Get DDU2113 ID. Issued at startup before sending the front panel ID
      QR("Get DDU ID -> ", 5);

      printhex(rsp + 1, rsp + rsplen);
      fputc('\n', stdout);

      return;

    case 0x57:
      // Get Marker Type?
      QR("MARKER TYPE -> ", 2);

      switch(rsp[1])
      {
      case 0x02: printf("TRACK\n");       return;
      case 0x03: printf("REVERSE\n");     return; // Switch to side B
      case 0x07: printf("SKIP +1\n");     return; // Skip marker? Also seen at beginning of 175-recorded tape
      case 0x0D: printf("INTRO SKIP\n");  return; // Skip over begin of sector 1
      case 0x14: printf("BEGIN SEC\n");   return; // After reversing
      case 0x0E: 
      default:
        printf("%02X\n", rsp[1]);
      }

      return;

    case 0x58:
      // Get Function State. This is apparently used to update the symbols
      // on the front panel display
      QR("FUNCTION STATE -> ", 2);

      switch(rsp[1])
      {
      case 0x02: printf("[]\n");          return; // Stop
      case 0x03: printf("...\n");         return; // Reading
      case 0x04: printf(">\n");           return; // Play
      case 0x0A: printf(">>\n");          return; // FFWD (sector)
      case 0x0B: printf("<<\n");          return; // Rewind (sector)
      case 0x11: printf(">>|\n");         return; // Search forwards
      case 0x12: printf("|<<\n");         return; // Search backwards
      case 0x15: printf("(<<)\n");        return; // Search arriving at track
      case 0x16: printf("(>>)\n");        return; // Search arriving at track
      default:
        printf("%02X\n", rsp[1]);         return; // TODO: decode other codes
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

      printf("%d\n", rsp[1]);           return;

      return;

    case 0x5E:
      // VU meters, 2 bytes between 00-5F. 5F (95 decimal) is silence.
      // Values are negative decibels for left and right.
      if (chattymode)
      {
        QR("VU -> ", 3);

        printf("%16s %-16s\n", vustring(rsp[1]), vustring(rsp[2]));
      }

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
          P("Time -> ");
          printf("Track %02X Time %02X:%02X:%02X Counter %02X%02X [1=%02X 6=%02X 9=%02X]\n", 
            rsp[2], rsp[3], rsp[4], rsp[5], rsp[7], rsp[8], 
            rsp[1], rsp[6], rsp[9]);

          track = rsp[2];
        }
      }
      return;

    case 0x61:
      // Get tape info for prerecorded tape
      QR("PREREC TAPE INFO -> ", 6);

      printf("[1]=0x%02X Tracks=%02X Total time=%02X:%02X:%02X\n",
        rsp[1], rsp[2], rsp[3], rsp[4], rsp[5]);

      return;

    default:
      ;// Nothing
    }
  }

dump:
  // If we got here, we don't understand the command. Just dump it.
  // Note, we don't dump the checksums.
  printhex(cmd, cmd + cmdlen); // Command
  fputs("-- ", stdout);
  printhex(rsp, rsp + rsplen); // Command
  putc('\n', stdout);
}


//---------------------------------------------------------------------------
// Main application
int main(void)
{
  reinit();

  printf("\nFront panel monitor running\n");

  uint8_t checksum = 0;
  bool valid = true;
  buf_t buffer;

  for(;;)
  {
    uint8_t cmdbyte;
    uint8_t rspbyte;
    uint8_t rxbyte;

    if (check_button())
    {
      continue;
    }

    // Data should come in on both connections at the same time.
    // So wait until there's data on both ports.
    while (!ringbuffer_num(&rb_cmd) && !ringbuffer_num(&rb_rsp))
    {
      // Nothing
    }
    
    ringbuffer_get(&rb_cmd, &cmdbyte);
    ringbuffer_get(&rb_rsp, &rspbyte);

    // If one of the bytes is 0xFF but the other one isn't, we know for sure
    // who is sending the data. Otherwise (i.e. when both incoming bytes
    // are 0xFF) we assume that the data came from the same source as the
    // previous byte.
    //
    // That means we get out of sync if the first byte from either side is
    // 0xFF but this is very unlikely and probably easy to recognize. For
    // one thing, the checksum will not match if we get it wrong.
    if ((cmdbyte != 0xFF) && (cmdbyte != 0xEE))
    {
      // Data came from front panel

      // If we already have some response bytes, parse the buffer and clear
      // it before we store the new byte.
      if (buffer.rsp)
      {
        // Check response checksum
        if (checksum != 0xFF)
        {
          valid = false;
        }

        // Note: Interpreting the data may be an expensive operation.
        // That's okay, the receive callbacks will just gather up data
        // into the ringbuffers in the background.
        gpio_toggle_pin_level(LED0);
        parsebuffer(&buffer, valid);
        gpio_toggle_pin_level(LED0);

        buffer.len = buffer.rsp = 0;
        checksum = 0;
        valid = true;
      }

      rxbyte = cmdbyte;
    }
    else if ((rspbyte != 0xFF) && (rspbyte != 0xEE))
    {
      // Data came from dig-mcu

      // Ignore the data if we don't have a command yet.
      // There's no point trying to catch and interpret a response without
      // the command.
/*
      if (!buffer.len)
      {
        continue;
      }
*/

      // If this is the first response byte, mark it
      if (!buffer.rsp)
      {
        // Check command checksum
        if (checksum != 0xFF)
        {
          valid = false;
        }

        buffer.rsp = buffer.len; // Response starts here

        checksum = 0;
      }

      rxbyte = rspbyte;
    }
    else
    {
      // Both bytes were 0xFF, don't change direction
      rxbyte = 0xFF;
    }

    checksum += rxbyte;

    if (buffer.len < sizeof(buffer.buf))
    {
      buffer.buf[buffer.len++] = rxbyte;
//      buffer.buf[buffer.len++] = cmdbyte;
//      buffer.buf[buffer.len++] = rspbyte;
    }
  }
}
