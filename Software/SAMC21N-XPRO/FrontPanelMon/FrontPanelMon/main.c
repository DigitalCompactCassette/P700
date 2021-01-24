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

//---------------------------------------------------------------------------
// Buffer type to hold two sequences of data
//
// For the front side bus, the two sequences are a command and a response.
// For the L3 bus, the two sequences are address and data.
typedef struct
{
  uint8_t buf[255];
  uint8_t len; // Total number of bytes stored length
  uint8_t rsp; // Index of the start of the second sequence
  bool ready; // Incoming data complete

} buf_t;

bool chattymode = true;
bool enablefp = true;
bool enablel3 = false;

struct io_descriptor *spi_cmd; // From front panel
struct io_descriptor *spi_rsp; // From dig-mcu

char cmdbuf[256];
char rspbuf[256];
struct ringbuffer rb_cmd;
struct ringbuffer rb_rsp;

struct io_descriptor *spi_l3;  // L3 bus

buf_t l3buf[16];
bool l3mode; // false=address, true=data
buf_t *pl3buf; // current receive buffer
uint l3overrun;


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
// L3 mode IRQ handler; this is called when the L3MODE pin changes
void l3mode_callback(void)
{
  l3mode = gpio_get_pin_level(L3MODE);

  // Is this the start of a new sequence?
  if ((!l3mode) && ((!pl3buf) || (pl3buf->len)))
  {
    // Do we already have a buffer?
    if (pl3buf)
    {
      // If we overran the buffers, wait patiently until the next buffer
      // is freed by the reader
      if (pl3buf->ready)
      {
        return;
      }
      
      // Check if the buffer has an address and data.
      if (pl3buf->rsp) // Only nonzero if at least one data byte seen
      {
        // Mark the buffer as finished so the reader will know it can start
        // parsing it.
        pl3buf->ready = true;

        // Switch to the next buffer
        if ((pl3buf - l3buf) < ARRAY_SIZE(l3buf) - 1)
        {
          pl3buf++;
        }
        else
        {
          pl3buf = &l3buf[0];
        }
      }
      else
      {
        // The current buffer doesn't have any data in it -- the L3MODE must
        // have gone HIGH, then LOW again. Continue writing to the same
        // buffer
        return;
      }
    }
    else
    {
      pl3buf = &l3buf[0];
    }

    // We now moved to a new buffer. Check if it's free.
    if (pl3buf->ready)
    {
      // The read code hasn't been here yet.
      l3overrun++;
    }
  }

  // TODO: Read SPI data register using _spi_s_async_read_one to sync the shift register?
}


//---------------------------------------------------------------------------
void l3spi_callback(struct spi_s_async_descriptor *spi)
{
  // Make sure we have a buffer first. The buffer pointer is not assigned
  // until the L3MODE line goes low.
  // Also don't add bytes to a buffer that was already finished; that's an
  // overrun which is dealt with by the L3MODE interrupt handler.
  if (pl3buf && !pl3buf->ready)
  {
    // Make sure there is space
    if (pl3buf->len < ARRAY_SIZE(pl3buf->buf))
    {
      uint8_t rxbyte;

      ringbuffer_get(&spi->rx_rb, &rxbyte); // always successful or we wouldn't be here

      // If this is the first data (not address) byte, set the index
      if (l3mode && !pl3buf->rsp)
      {
        pl3buf->rsp = pl3buf->len;
      }

      // Store the byte
      pl3buf->buf[pl3buf->len++] = rxbyte;
    }
  }
}


//---------------------------------------------------------------------------
// Reinitialize hardware
void reinit(void)
{
  atmel_start_init();

  printf("\r\nHardware initialized\r\n");

  // Front panel bus
  
  ringbuffer_init(&rb_cmd, cmdbuf, sizeof(cmdbuf));
  ringbuffer_init(&rb_rsp, rspbuf, sizeof(rspbuf));

  spi_s_async_get_io_descriptor(&SPI_EXT1, &spi_cmd);
  spi_s_async_get_io_descriptor(&SPI_EXT2, &spi_rsp);

  spi_s_async_register_callback(&SPI_EXT1, SPI_S_CB_RX, (FUNC_PTR)cmd_rx_callback);
  spi_s_async_register_callback(&SPI_EXT2, SPI_S_CB_RX, (FUNC_PTR)rsp_rx_callback);
  spi_s_async_enable(&SPI_EXT1);
  spi_s_async_enable(&SPI_EXT2);

  // L3 bus

  ext_irq_register(L3MODE, l3mode_callback);
  ext_irq_enable(L3MODE);

  spi_s_async_get_io_descriptor(&SPI_EXT3, &spi_l3);

  spi_s_async_register_callback(&SPI_EXT3, SPI_S_CB_RX, (FUNC_PTR)l3spi_callback);
  spi_s_async_enable(&SPI_EXT3);
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
      reinit();

      // Toggle the enablefp and enablel3 flags:
      // FP off / L3 on  => FP on  / L3 on
      // FP on  / L3 on  => FP on  / L3 off
      // FP on  / L3 off => FP off / L3 on
      // The two flags are never both off (unless initialized that way)
      if (!enablefp)
      {
        enablefp = true;
      }
      else if (enablel3)
      {
        enablel3 = false;
      }
      else
      {
        enablefp = false;
        enablel3 = true;
      }
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
    static const char hex[] = "0123456789ABCDEF";

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
  const uint vu_limit = 45;

  if (vu > vu_limit)
  {
    vu = vu_limit;
  }

  return vubar + (((uint)vu * 16) / (vu_limit + 1));
}


//---------------------------------------------------------------------------
// Hexdump a command and response
void hexdumpmessage(
  uint8_t *cmd,
  uint8_t cmdlen,
  uint8_t *rsp,
  uint8_t rsplen)
{
  printhex(cmd, cmd + cmdlen); // Command
  fputs("-- ", stdout);
  printhex(rsp, rsp + rsplen); // Response
  fputs("\r\n", stdout);
}


//---------------------------------------------------------------------------
// Alternative for the above, using a buf_t as parameter
void hexdumpbuf(
  buf_t *pbuf)
{
  hexdumpmessage(pbuf->buf, pbuf->rsp, pbuf->buf + pbuf->rsp, pbuf->len - pbuf->rsp);
}

//---------------------------------------------------------------------------
// Parse a buffer
void dumpfrontpanelmessage(
  uint8_t *cmd,
  uint8_t cmdlen,
  uint8_t *rsp,
  uint8_t rsplen)
{
  switch(*cmd)
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

    switch(cmd[1])
    {
    case 1: printf("None\r\n");    return;
    case 2: printf("Track\r\n");   return;
    case 3: printf("All\r\n");     return;
    default:
      ; // Nothing
    }

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

  case 0x36:
    // Recorder ID. Sent to dig-mcu after reset
    QC("FRONT PANEL ID: ", 42);

    printstring(cmd + 1, cmd + cmdlen);
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

    switch(cmd[1])
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

          if (rsp[2] & 0x01) fputs("LYRICS ",     stdout); // Lyrics (issue Get DCC Long Text)
          if (rsp[2] & 0x02) fputs("MARKER ",     stdout); // Marker change (issue Get Marker)
          if (rsp[2] & 0x04) fputs("(B4) ",       stdout);
          if (rsp[2] & 0x08) fputs("(B8) ",       stdout);
          if (rsp[2] & 0x10) fputs("(B10) ",      stdout);
          if (rsp[2] & 0x20) fputs("(B20) ",      stdout);
          if (rsp[2] & 0x40) fputs("(B40) ",      stdout);
          if (rsp[2] & 0x80) fputs("(B80) ",      stdout);

          if (rsp[3] & 0x80) fputs("DECKTIME ",   stdout); // No absolute tape time, using deck time?
          if (rsp[3] & 0x40) fputs("TAPETIME ",   stdout); // Using tape time code
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

    switch(rsp[1])
    {
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

    switch(rsp[1])
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
    // These numbers correspond to the decimal numbers that are shown by
    // the "Switches Test" program in Service Mode
    case 0x00: printf("ACC FERRO\r\n");   return; // 000
    case 0x02: printf("ACC CHROME\r\n");  return; // 002
    case 0x04: printf("PDCC\r\n");        return; // 004
    case 0x14: printf("UDCC(PROT)\r\n");  return; // 020
    case 0x1C: printf("UDCC\r\n");        return; // 028
    case 0x24: printf("DCC120(PROT)\r\n");return; // 036
    case 0x2C: printf("DCC120\r\n");      return; // 044
    case 0x34: printf("DCC105(PROT)\r\n");return; // 052
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

    switch(cmd[1])
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

    switch(cmd[1])
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

    switch(rsp[1])
    {
    case 0x02: printf("TRACK\r\n");       return;
    case 0x03: printf("REVERSE\r\n");     return; // Switch to side B
    case 0x07: printf("SKIP +1\r\n");     return; // Skip marker? Also seen at beginning of 175-recorded tape
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

    switch(rsp[1])
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
      
    case 0x30: printf("SKIP\r\n");        return; // Skipping intro
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
      QR("VU -> ", 3);

      // No line feed so the text window doesn't scroll
      printf("%16s %-16s\r", vustring(rsp[1]), vustring(rsp[2]));
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
        P("Time -> ");
        //printf("Track %02X Time %X:%02X:%02X Counter %02X%02X [1/2=%02X%X 6=%02X 9=%02X]\r\n", 
        printf("                                T%02X %X:%02X:%02X C%02X%02X [%02X %X %02X %02X]\r", 
          rsp[2], rsp[3] & 0xF, rsp[4], rsp[5], rsp[7], rsp[8],
          rsp[1], rsp[3] >> 4, rsp[6], rsp[9]);

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


//---------------------------------------------------------------------------
// Parse command and response bytes for the front panel bus
void capturefrontpanel()
{
  // Data should come in on both front panel connections at the same time.
  // Ignore the call if there is no data on one of the connections
  if (!ringbuffer_num(&rb_cmd) || !ringbuffer_num(&rb_rsp))
  {
    return;
  }

  uint8_t cmdbyte;
  uint8_t rspbyte;

  ringbuffer_get(&rb_cmd, &cmdbyte);
  ringbuffer_get(&rb_rsp, &rspbyte);

  static uint8_t checksum = 0;
  static bool valid = true;
  static buf_t buffer;
  static uint8_t rxbyte;

  // If one of the bytes is 0xFF but the other one isn't, we know for sure
  // who is sending the data. Otherwise (i.e. when both incoming bytes
  // are 0xFF) we assume that the data came from the same source as the
  // previous byte.
  //
  // That means we get out of sync if the first byte from either side is
  // 0xFF but this is very unlikely and probably easy to recognize. For
  // one thing, the checksum will not match if we get it wrong.
  if (cmdbyte != 0xFF)
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

      // clear msb's. These alternate between 1 and 0 on subsequent
      // commands and responses, probably to ensure that the firmware always
      // works on live data, not on stale data that happens when things stop
      // responding because of some technical problem.
      buffer.buf[0] &= 0x7F;
      buffer.buf[buffer.rsp] &= 0x7F;

      // The VU meter command often responds with a checksum error. There is
      // probably a bug in the firmware; maybe the code that updates the VU
      // values and the code that calculates the checksums aren't synchronized
      // so that checksums don't match the data.
      // And maybe that's why the 3rd generation decks don't have a VU meter?
      // I guess we'll never know.
      if ((!valid) && (buffer.rsp != 2) && (buffer.buf[0] != 0x5E)) // Ignore checksum for VU updates
      {
        // Don't try to interpret something that we already know is wrong
        fputs("CHECKSUM ERROR: ", stdout);
        hexdumpbuf(&buffer);
      }
      else if ((buffer.len < 4) || (buffer.rsp < 2))
      {
        // We didn't get at least 2 bytes for command and 2 bytes for response.
        printf("IGNORING: ");
        hexdumpbuf(&buffer);
      }
      else
      {
        // Note: Interpreting the data may be an expensive operation.
        // That's okay, the receive callbacks will just gather up data
        // into the ringbuffers in the background.
        dumpfrontpanelmessage(&buffer.buf[0], buffer.rsp - 1, &buffer.buf[buffer.rsp], (buffer.len - buffer.rsp) - 1);
      }
      
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


//---------------------------------------------------------------------------
// Parse L3 address and data
void dol3command(buf_t *pbuf)
{
  // The buffer pointer should always be valid and the buffer should always
  // have a nonzero response index
  // If there are addresses without response, they are combined
  // together in the same buffer. It's impossible (because of the interrupt
  // handler logic) to have a buffer that has data but no address.
  if (!pbuf || !pbuf->rsp)
  {
    return;
  }

/* Commented out: This shows a lot of data but I suspect it's misinterpreting it.

  for (uint cmdindex = 0; cmdindex < pbuf->rsp; cmdindex++)
  {
    uint8_t cmd = pbuf->buf[cmdindex];

    switch (cmd)
    {
      case 0x00: fputs("DRP TFE RDSPEED\r\n",     stdout); return; // Read SPEED register
      case 0x10: fputs("DRP TFE LDSET0\r\n",      stdout); return; // Load new TFE settings register 0
      case 0x11: fputs("DRP TFE LDSET1\r\n",      stdout); return; // Load new TFE settings register 1
      case 0x12: fputs("DRP TFE LDSET2\r\n",      stdout); return; // Load new TFE settings register 2
      case 0x13: fputs("DRP TFE LDSET3\r\n",      stdout); return; // Load new TFE settings register 3
      case 0x15: fputs("DRP TFE LDSPDDTY\r\n",    stdout); return; // Load SPDDTY register
      case 0x17: fputs("DRP TFE LDBYTCNT\r\n",    stdout); return; // Load BYTCNT register
      case 0x18: fputs("DRP TFE LDRACCNT\r\n",    stdout); return; // Load RACCNT register
      case 0x20: fputs("DRP TFE RDAUX\r\n",       stdout); return; // Read AUXILIARY information
      case 0x21: fputs("DRP TFE RDSYS\r\n",       stdout); return; // Read SYSINFO
      case 0x22:
      case 0x62:
      case 0xA2:
      case 0xE2: fputs("DRP TFE RDDRAC\r\n",      stdout); return; // Read RAM data bytes (8 bits) from quarter YZ
      case 0x23:
      case 0x63:
      case 0xA3:
      case 0xE3: fputs("DRP TFE RDWDRAC\r\n",     stdout); return; // Read RAM data words (12 bits) from quarter YZ
      case 0x30: fputs("DRP TFE WRAUX\r\n",       stdout); return; // Write AUXILIARY information
      case 0x31: fputs("DRP TFE WRSYS\r\n",       stdout); return; // Write SYSINFO
      case 0x32:
      case 0x72:
      case 0xB2:
      case 0xF2: fputs("DRP TFE WRDRAC\r\n",      stdout); return; // Write RAM data bytes (8 bits) to quarter YZ
      case 0x33:
      case 0x73:
      case 0xB3:
      case 0xF3: fputs("DRP TFE WRWDRAC\r\n",     stdout); return; // Write RAM data words (12 bits) to quarter YZ

      case 0x30: fputs("DRP DEQ WRCOEF\r\n",      stdout); return; // Write FIR coefficients to the digital equalizer buffer bank
      case 0x20: fputs("DRP DEQ RDCOEF\r\n",      stdout); return; // Read FIR coefficients from the digital equalizer active bank
      case 0x13: fputs("DRP DEQ LDCOEFCNT\r\n",   stdout); return; // Load FIR coefficient counter
      case 0x14: fputs("DRP DEQ LDFCTRL\r\n",     stdout); return; // Load filter control register
      case 0x16: fputs("DRP DEQ LDT1SEL\r\n",     stdout); return; // Load CHTST1 pin selection register
      case 0x17: fputs("DRP DEQ LDT2SEL\r\n",     stdout); return; // Load CHTST2 pin selection register
      case 0x18: fputs("DRP DEQ LDTAEYE\r\n",     stdout); return; // Load ANAEYE channel selection register
      case 0x19: fputs("DRP DEQ LDAEC\r\n",       stdout); return; // Load AEC counter
      case 0x22: fputs("DRP DEQ RDAEC\r\n",       stdout); return; // Read AEC counter
      case 0x24: fputs("DRP DEQ RDSSPD\r\n",      stdout); return; // Read SEARCH speed register
      case 0x12: fputs("DRP DEQ LDINTMSK\r\n",    stdout); return; // Load interrupt mask register
      case 0x10: fputs("DRP DEQ LDDEQ3SET\r\n",   stdout); return; // Load digital equalizer settings register
      case 0x11: fputs("DRP DEQ LDCLKSET\r\n",    stdout); return; // Load PLL clock extraction settings register
* /
      default:
        return;
    }
*/

    hexdumpbuf(pbuf);
//  }

}

//---------------------------------------------------------------------------
// Parse command and response for the L3 bus
void capturel3()
{
  static uint index;
  buf_t *pbuf = &l3buf[index];

  if (l3overrun)
  {
    printf("L3 bus overrun %u\r\n", l3overrun);
    l3overrun = 0;
  }

  if (pbuf->ready)
  {
    dol3command(pbuf);

    pbuf->len = pbuf->rsp = 0;
    pbuf->ready = false;
    if (++index >= ARRAY_SIZE(l3buf))
    {
      index = 0;
    }
  }
}

//---------------------------------------------------------------------------
// Main application
int main(void)
{
  reinit();

  printf("\r\nFront panel monitor running\r\n");

  for(;;)
  {
    if (check_button())
    {
      // Nothing
    }

    if (enablefp)
    {
      capturefrontpanel();
    }

    if (enablel3)
    {
      capturel3();
    }

    gpio_toggle_pin_level(LED0);
  }
}
