// i2c_led.c
// LED handling through i2c bus

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "i2c_led.h"

//////////////////////////////////////////////////////////////////////////
// local defines
////////////////////////////////////////////////////////////////////////// 

#define I2C_ADDRESS        (0x28U)
#define GPIO_ENABLE_REG    (0xF6U)
#define GPIO_ENABLE_VAL    (0x0CU) // CS2 & CS3
#define GPIO_CONFIG_REG    (0xF7U)
#define GPIO_CONFIG_VAL    (0x50U) // set CS2 & CS3 as push-pull
#define GPIO_WRITE_REG     (0xF4U)
#define GPIO_WRITE_RED     (0x04U)
#define GPIO_WRITE_GREEN   (0x08U)
#define GPIO_WRITE_ORANGE  (0x00U)
#define GPIO_WRITE_NOTHING (0x0CU)
#define SPI_I2C_CONFIG_REG (0xF0U)
#define SPI_I2C_CONFIG_VAL (0x00U)

#define MAX_DATA_SIZE      (32U)
#define SPI_WRITE_DATA_LEN (2U)

// --------------------------------------------------------------------------
// static function prototypes
// --------------------------------------------------------------------------

static void i2c_smbus_write_byte_data(int fd, uint8_t command, uint8_t value);
static void i2c_access(int fd, struct i2c_msg *msg);

// --------------------------------------------------------------------------
// static variables
// --------------------------------------------------------------------------
static uint8_t i2c_data[MAX_DATA_SIZE] = { 0 };
static bool b_disp_error = true;

int open_led()
{
  int i2c_file = open("/dev/i2c-1", O_RDWR);
  if (i2c_file > 0)
  {
    i2c_smbus_write_byte_data(i2c_file, GPIO_ENABLE_REG, GPIO_ENABLE_VAL);
    i2c_smbus_write_byte_data(i2c_file, GPIO_CONFIG_REG, GPIO_CONFIG_REG);
    i2c_smbus_write_byte_data(i2c_file, SPI_I2C_CONFIG_REG, SPI_I2C_CONFIG_VAL);
  }
  return i2c_file;
}

void close_led(int fd)
{
  if (fd > 0)
  {
    close(fd);
  }
}

void set_led(int fd, uint16_t id, bool on)
{
  if (on == false)
  {
    i2c_smbus_write_byte_data(fd, GPIO_WRITE_REG, GPIO_WRITE_GREEN);
  }
  else
  {
    if (id == 2)
    {
      i2c_smbus_write_byte_data(fd, GPIO_WRITE_REG, GPIO_WRITE_GREEN);
    }
    else if (id == 1)
    {
      i2c_smbus_write_byte_data(fd, GPIO_WRITE_REG, GPIO_WRITE_ORANGE);
    }
    else
    {
      i2c_smbus_write_byte_data(fd, GPIO_WRITE_REG, GPIO_WRITE_RED);
    }
  }
}

//////////////////////////////////////////////////////////////////////////

void i2c_smbus_write_byte_data(int fd, uint8_t command, uint8_t value)
{
  i2c_data[0] = command;
  i2c_data[1] = value;
  struct i2c_msg msg;
  msg.addr = I2C_ADDRESS;
  msg.flags = 0;
  msg.len = SPI_WRITE_DATA_LEN;
  msg.buf = i2c_data;

  i2c_access(fd, &msg);
}

void i2c_access(int fd, struct i2c_msg *msg)
{
  if (fd > 0)
  {
    struct i2c_rdwr_ioctl_data args;

    args.msgs = msg;
    args.nmsgs = 1U;

    int32_t err = ioctl(fd, I2C_RDWR, &args);
    if ((err != 1) && (b_disp_error))
    {
      printf("[ERROR] LED access fails with err %i: %s\r\nAddress 0x%X %s len %u data[0] 0x%X\r\n",
              err,
              strerror(errno),
              msg->addr,
              msg->flags == 0 ? "W" : "R",
              msg->len,
              msg->buf[0]);
      b_disp_error = false;
    }
  }
}

