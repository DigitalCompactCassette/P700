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
  3: MESSYNC    (message sync, probably used to indicate first byte of cmd?)
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
      spi_s_async_enable(&SPI_EXT1);
      spi_s_async_enable(&SPI_EXT2);
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


typedef struct
{
  uint8_t buf[255];
  uint8_t len; // Total length
  uint8_t rsp; // Index of response

} buf_t;


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
// Parse a buffer
void parsebuffer(buf_t *buf, bool valid)
{
  if (!valid)
  {
    fputs("CHECKSUM ERROR\n", stdout);
  }
  else if ((buf->len > 4) && (buf->rsp >= 2)) // At least one valid byte each, plus checksum bytes
  {
    // Aliases for improved readability
    uint8_t *cmd = &buf->buf[0];
    uint8_t *rsp = &buf->buf[buf->rsp];

    // Lengths without checksum bytes
    uint8_t cmdlen = buf->rsp - 1;
    uint8_t rsplen = (buf->len - buf->rsp) - 1;

    // Flip first bits. These alternate between subsequent commands and
    // responses, probably to ensure that the firmware always works on live
    // data, not on stale data that happens when things stop responding
    // because of some technical problem.
    (*cmd) &= 0x7F;
    (*rsp) &= 0x7F;

    switch(*cmd)
    {
    case 0x10:
      // Key or remote command
      printf("COMMAND: ");
      if ((cmdlen ==2) && (rsplen == 1) && (rsp[0] == 0))
      {
        switch (cmd[1])
        {
        case 0x01: printf("SIDE A/B\n");    return;
        case 0x02: printf("OPEN/CLOSE\n");  return;
        case 0x03: printf("EDIT\n");        return;
        case 0x04: printf("REC/PAUSE\n");   return;
        case 0x05: printf("STOP\n");        return;
        case 0x06: printf("REPEAT\n");      return;
        case 0x07: printf("DOLBY\n");       return;
        case 0x08: printf("SCROLL\n");      return;
        case 0x09: printf("RECLEVEL-\n");   return;
        case 0x0A: printf("APPEND\n");      return;
        case 0x0B: printf("PLAY\n");        return;
        case 0x0C: printf("PRESETS\n");     return;
        case 0x0D: printf("TIME\n");        return;
        case 0x0E: printf("TEXT\n");        return;
        case 0x0F: printf("RECLEVEL+\n");   return;
        case 0x10: printf("RECORD\n");      return;
        case 0x11: printf("NEXT\n");        return;
        case 0x12: printf("PREV\n");        return;

        // Remote control
        // "PAUSE" and "COUNTER RESET" and "WRITE MARK" don't show up
        //
        // BUG? If you press TEXT on the remote control repeatedly, when it
        // goes to ARTIST, it says NO TEXT INFO (instead of the artist)
        // and then the front panel starts sending 51 03 to the dig MCU
        // repeatedly.
        case 0x1C: printf("FFWD\n");        return;
        case 0x1D: printf("EJECT\n");       return; // Duplicate of 10 02
        case 0x1F: printf("REWIND\n");      return;
        case 0x20: printf("NUMBER 0\n");    return;
        case 0x21: printf("NUMBER 1\n");    return;
        case 0x22: printf("NUMBER 2\n");    return;
        case 0x23: printf("NUMBER 3\n");    return;
        case 0x24: printf("NUMBER 4\n");    return;
        case 0x25: printf("NUMBER 5\n");    return;
        case 0x26: printf("NUMBER 6\n");    return;
        case 0x27: printf("NUMBER 7\n");    return;
        case 0x28: printf("NUMBER 8\n");    return;
        case 0x29: printf("NUMBER 9\n");    return;
        case 0x2C: printf("STANDBY\n");     return;
        default:
          ; // Nothing
        }
      }
      break;

    case 0x23:
      // Set repeat mode
      printf("REPEAT MODE: ");
      if ((cmdlen == 2) && (rsplen == 1) && (rsp[0] == 0))
      {
        switch(cmd[1])
        {
        case 1: printf("Repeat None\n");    return;
        case 2: printf("Repeat Track\n");   return;
        case 3: printf("Repeat All\n");     return;
        default:
          ; // Nothing
        }
      }
      break;

    case 0x2A:
      // Sector. This is issued after the 10 01 (SIDE A/B) command.
      // Presumably the sector can be 1 to 4 inclusive but without 4-sector
      // tapes, we won't know...
      printf("SECTOR: ");
      if ((cmdlen ==2) && (rsplen == 1) && (rsp[0] == 0))
      {
        printf("%u\n", cmd[1]);
        return;
      }
      break;

    case 0x2F:
      // Go to track (pdcc only?)
      printf("GO TO TRACK: ");
      if ((cmdlen == 3) && (rsplen == 1) && (rsp[0] == 0))
      {
        printf("To=%u, [2]=%u\n", cmd[1], cmd[2]);
        return;
      }
      break;

    case 0x36:
      // Recorder ID. Sent to dig-mcu after reset
      printf("ID: ");
      if ((cmdlen == 42) && (rsplen == 1) && (rsp[0] == 0))
      {
        printstring(cmd + 1, cmd + cmdlen);
        putc('\n', stdout);
        return;
      }
      break;

    case 0x38:
      // Time mode. This is issued after the TIME command (10 0D)
      // Note: for [super] user tapes, the display has a VU mode too:
      // Press TIME enough times and it will only send TIME but no time mode
      // command.
      // For ACC's a counter will appear in one of the modes, this also
      // doesn't issue a time mode command.
      printf("TIME MODE: ");
      if ((cmdlen == 2) && (rsplen == 1) && (rsp[0] == 0))
      {
        switch(cmd[1])
        {
        case 1: printf("TOTAL TIME\n");     return; // prerec/dcc/acc
        case 2: printf("TOT REM TIME\n");   return; // prerec
        case 3: printf("TRACK TIME\n");     return; // prerec/sudcc

        case 5: printf("REM TIME\n");       return; // non-prerecorded
        default:
          ; // Nothing
        }
      }
      break;

    case 0x41:
      // Deck status
      //if ((cmdlen == 1) && (rsplen == 4) && (!memcmp(rsp, "\0\0\xC0\x41", rsplen)))
      {
        return;
      }
      break;

    case 0x46:
      // Get drawer status
      printf("GET DRAWER STATUS -> ");
      if ((cmdlen == 1) && (rsplen == 2) && (rsp[0] == 0))
      {
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
      }
      break;

    case 0x51:
      // Get long title from [s]udcc
      printf("GET LONG UDCC TEXT -> ");
      if ((cmdlen == 2) && (cmd[1] == 0xFA) && (rsplen == 41) && (rsp[0] == 0))
      {
        printstring(rsp + 1, rsp + rsplen);
        putc('\n', stdout);
        return;
      }
      break;

    case 0x52:
      // Get long title from pdcc
      printf("GET LONG PDCC TEXT: ");
      if ((cmdlen == 2) && (rsplen == 41) && (rsp[0] == 0))
      {
        printf("Track %u -> ", cmd[1]);
        printstring(rsp + 1, rsp + rsplen);
        putc('\n', stdout);
        return;
      }
      break;

    case 0x53:
      // Get short title from [s]udcc
      printf("GET SHORT UDCC TEXT -> ");
      if ((cmdlen == 2) && (cmd[1] == 0xFA) && (rsplen == 13) && (rsp[0] == 0))
      {
        printstring(rsp + 1, rsp + rsplen);
        putc('\n', stdout);
        return;
      }
      break;

    case 0x54:
      // Get short title from [s]udcc
      printf("GET SHORT PDCC TEXT: ");
      if ((cmdlen == 2) && (rsplen == 13) && (rsp[0] == 0))
      {
        printf("Track %u -> ", cmd[1]);
        printstring(rsp + 1, rsp + rsplen);
        putc('\n', stdout);
        return;
      }
      break;

    case 0x5E:
      // VU meters, 2 bytes between 00-5F. 5F (95 decimal) is silence.
      // So values are probably negative decibels for left and right.
      //if ((cmdlen == 1) && (rsplen == 3) && (!memcmp(prsp, "\0\x5F\x5F", rsplen)))
      {
        return;
      }
      break;

    case 0x60:
      // byte 0=error 00=ok
      // byte 1=status, 8=play?
      // byte 2=track
      // byte 3=?
      // byte 4/5=time in BCD, mm/ss
      // byte 6=?
      // byte 7/8=tape counter in BCD Big-endian, 0-9999 
      // byte 9=?
/*
      printf("Time: Track %u Time %02X:%02X Counter %02X%02X [1=%02X 3=%02X 6=%02X 9=%02X]\n", 
        rsp[2], rsp[4], rsp[5], rsp[7], rsp[8], 
        rsp[1], rsp[3], rsp[6], rsp[9]);
*/
      return;
      break;

    case 0x61:
      // Get tape info for prerecorded tape
      printf("PREREC TAPE INFO -> ");
      if ((cmdlen == 1) && (rsplen == 6) && (rsp[0] == 0))
      {
        printf("[1]=0x%02X Tracks=%02X Total time=%02X:%02X:%02X\n",
          rsp[1], rsp[2], rsp[3], rsp[4], rsp[5]);
        return;
      }
      break;

    default:
      ;// Nothing
    }

    for (uint8_t u = 0; u < buf->len - 1; u++)
    {
      if (u == buf->rsp - 1)
      {
        printf("-- ");
        continue; // ignore last byte of command
      }

      printf("%02X ", buf->buf[u] & 0xFF);
    }
  }

  putc('\n', stdout);
}


//---------------------------------------------------------------------------
// Main application
int main(void)
{
  atmel_start_init();

  ringbuffer_init(&rb_cmd, cmdbuf, sizeof(cmdbuf));
  ringbuffer_init(&rb_rsp, rspbuf, sizeof(rspbuf));

  struct io_descriptor *spi_cmd; // From front panel
  struct io_descriptor *spi_rsp; // From dig-mcu

  spi_s_async_get_io_descriptor(&SPI_EXT1, &spi_cmd);
  spi_s_async_get_io_descriptor(&SPI_EXT2, &spi_rsp);

  spi_s_async_register_callback(&SPI_EXT1, SPI_S_CB_RX, (FUNC_PTR)cmd_rx_callback);
  spi_s_async_register_callback(&SPI_EXT2, SPI_S_CB_RX, (FUNC_PTR)rsp_rx_callback);
  spi_s_async_enable(&SPI_EXT1);
  spi_s_async_enable(&SPI_EXT2);

  printf("\nFront panel monitor\n");

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

        // Note: Interpreting the data may be an expensive operation.
        // That's okay, the receive callbacks will just gather up data
        // into the ringbuffers in the background.
        gpio_set_pin_level(LED0, true);
        parsebuffer(&buffer, valid);
        gpio_set_pin_level(LED0, false);

        buffer.len = buffer.rsp = 0;
        checksum = 0;
        valid = true;
      }

      checksum += (rxbyte = cmdbyte);
    }
    else if (rspbyte != 0xFF)
    {
      // Data came from dig-mcu

      // Ignore the data if we don't have a command yet.
      // There's no point trying to catch and interpret a response without
      // the command.
      if (!buffer.len)
      {
        continue;
      }

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

      checksum += (rxbyte = rspbyte);
    }
    else
    {
      // Both bytes were 0xFF, don't change direction
      checksum += (rxbyte = 0xFF);
    }

    if (buffer.len < sizeof(buffer.buf))
    {
      buffer.buf[buffer.len++] = rxbyte;
    }
  }
}
