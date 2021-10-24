/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#ifndef __KERNEL__
#define __KERNEL__
#endif

#ifndef MODULE
#define MODULE
#endif

#define VENDOR_ID       (0xA042)
#define PRODUCT_ID      (0x00FF)
#define PRODUCT_VERSION (0x0100)
#define DEVICE_NAME     "Amos Test"


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/mutex.h>
#include <linux/slab.h>


MODULE_AUTHOR("Amos42");
MODULE_DESCRIPTION("GPIO Test Driver");
MODULE_LICENSE("GPL");


/*============================================================*/

//#include "../bcm_peri.h"
#include "../bcm_peri.c"

//#include "../gpio_util.h"
#include "../gpio_util.c"
//#include "../i2c_util.h"
#include "../i2c_util.c"
//#include "../spi_util.h"
#include "../spi_util.c"



/*
 * ADS1115 Defines
 */
/*=========================================================================
    I2C ADDRESS/BITS
    -----------------------------------------------------------------------*/
#define ADS1X15_ADDRESS (0x48) ///< 1001 000 (ADDR = GND)
/*=========================================================================*/

/*=========================================================================
    POINTER REGISTER
    -----------------------------------------------------------------------*/
#define ADS1X15_REG_POINTER_MASK (0x03)      ///< Point mask
#define ADS1X15_REG_POINTER_CONVERT (0x00)   ///< Conversion
#define ADS1X15_REG_POINTER_CONFIG (0x01)    ///< Configuration
#define ADS1X15_REG_POINTER_LOWTHRESH (0x02) ///< Low threshold
#define ADS1X15_REG_POINTER_HITHRESH (0x03)  ///< High threshold
/*=========================================================================*/

/*=========================================================================
    CONFIG REGISTER
    -----------------------------------------------------------------------*/
#define ADS1X15_REG_CONFIG_OS_MASK (0x8000) ///< OS Mask
#define ADS1X15_REG_CONFIG_OS_SINGLE (0x8000) ///< Write: Set to start a single-conversion
#define ADS1X15_REG_CONFIG_OS_BUSY (0x0000) ///< Read: Bit = 0 when conversion is in progress
#define ADS1X15_REG_CONFIG_OS_NOTBUSY (0x8000) ///< Read: Bit = 1 when device is not performing a conversion

#define ADS1X15_REG_CONFIG_MUX_MASK (0x7000) ///< Mux Mask
#define ADS1X15_REG_CONFIG_MUX_DIFF_0_1 (0x0000) ///< Differential P = AIN0, N = AIN1 (default)
#define ADS1X15_REG_CONFIG_MUX_DIFF_0_3 (0x1000) ///< Differential P = AIN0, N = AIN3
#define ADS1X15_REG_CONFIG_MUX_DIFF_1_3 (0x2000) ///< Differential P = AIN1, N = AIN3
#define ADS1X15_REG_CONFIG_MUX_DIFF_2_3 (0x3000) ///< Differential P = AIN2, N = AIN3
#define ADS1X15_REG_CONFIG_MUX_SINGLE_0 (0x4000) ///< Single-ended AIN0
#define ADS1X15_REG_CONFIG_MUX_SINGLE_1 (0x5000) ///< Single-ended AIN1
#define ADS1X15_REG_CONFIG_MUX_SINGLE_2 (0x6000) ///< Single-ended AIN2
#define ADS1X15_REG_CONFIG_MUX_SINGLE_3 (0x7000) ///< Single-ended AIN3

#define ADS1X15_REG_CONFIG_PGA_MASK (0x0E00)   ///< PGA Mask
#define ADS1X15_REG_CONFIG_PGA_6_144V (0x0000) ///< +/-6.144V range = Gain 2/3
#define ADS1X15_REG_CONFIG_PGA_4_096V (0x0200) ///< +/-4.096V range = Gain 1
#define ADS1X15_REG_CONFIG_PGA_2_048V (0x0400) ///< +/-2.048V range = Gain 2 (default)
#define ADS1X15_REG_CONFIG_PGA_1_024V (0x0600) ///< +/-1.024V range = Gain 4
#define ADS1X15_REG_CONFIG_PGA_0_512V (0x0800) ///< +/-0.512V range = Gain 8
#define ADS1X15_REG_CONFIG_PGA_0_256V (0x0A00) ///< +/-0.256V range = Gain 16

#define ADS1X15_REG_CONFIG_MODE_MASK (0x0100)   ///< Mode Mask
#define ADS1X15_REG_CONFIG_MODE_CONTIN (0x0000) ///< Continuous conversion mode
#define ADS1X15_REG_CONFIG_MODE_SINGLE (0x0100) ///< Power-down single-shot mode (default)

#define ADS1X15_REG_CONFIG_RATE_MASK (0x00E0) ///< Data Rate Mask

#define ADS1X15_REG_CONFIG_CMODE_MASK (0x0010) ///< CMode Mask
#define ADS1X15_REG_CONFIG_CMODE_TRAD (0x0000) ///< Traditional comparator with hysteresis (default)
#define ADS1X15_REG_CONFIG_CMODE_WINDOW (0x0010) ///< Window comparator

#define ADS1X15_REG_CONFIG_CPOL_MASK (0x0008) ///< CPol Mask
#define ADS1X15_REG_CONFIG_CPOL_ACTVLOW (0x0000) ///< ALERT/RDY pin is low when active (default)
#define ADS1X15_REG_CONFIG_CPOL_ACTVHI (0x0008) ///< ALERT/RDY pin is high when active

#define ADS1X15_REG_CONFIG_CLAT_MASK (0x0004) ///< Determines if ALERT/RDY pin latches once asserted
#define ADS1X15_REG_CONFIG_CLAT_NONLAT (0x0000) ///< Non-latching comparator (default)
#define ADS1X15_REG_CONFIG_CLAT_LATCH (0x0004) ///< Latching comparator

#define ADS1X15_REG_CONFIG_CQUE_MASK (0x0003) ///< CQue Mask
#define ADS1X15_REG_CONFIG_CQUE_1CONV (0x0000) ///< Assert ALERT/RDY after one conversions
#define ADS1X15_REG_CONFIG_CQUE_2CONV (0x0001) ///< Assert ALERT/RDY after two conversions
#define ADS1X15_REG_CONFIG_CQUE_4CONV (0x0002) ///< Assert ALERT/RDY after four conversions
#define ADS1X15_REG_CONFIG_CQUE_NONE (0x0003) ///< Disable the comparator and put ALERT/RDY in high state (default)
/*=========================================================================*/

/** Gain settings */
typedef enum {
  GAIN_TWOTHIRDS = ADS1X15_REG_CONFIG_PGA_6_144V,
  GAIN_ONE = ADS1X15_REG_CONFIG_PGA_4_096V,
  GAIN_TWO = ADS1X15_REG_CONFIG_PGA_2_048V,
  GAIN_FOUR = ADS1X15_REG_CONFIG_PGA_1_024V,
  GAIN_EIGHT = ADS1X15_REG_CONFIG_PGA_0_512V,
  GAIN_SIXTEEN = ADS1X15_REG_CONFIG_PGA_0_256V
} adsGain_t;

/** Data rates */
#define RATE_ADS1015_128SPS (0x0000)  ///< 128 samples per second
#define RATE_ADS1015_250SPS (0x0020)  ///< 250 samples per second
#define RATE_ADS1015_490SPS (0x0040)  ///< 490 samples per second
#define RATE_ADS1015_920SPS (0x0060)  ///< 920 samples per second
#define RATE_ADS1015_1600SPS (0x0080) ///< 1600 samples per second (default)
#define RATE_ADS1015_2400SPS (0x00A0) ///< 2400 samples per second
#define RATE_ADS1015_3300SPS (0x00C0) ///< 3300 samples per second

#define RATE_ADS1115_8SPS (0x0000)   ///< 8 samples per second
#define RATE_ADS1115_16SPS (0x0020)  ///< 16 samples per second
#define RATE_ADS1115_32SPS (0x0040)  ///< 32 samples per second
#define RATE_ADS1115_64SPS (0x0060)  ///< 64 samples per second
#define RATE_ADS1115_128SPS (0x0080) ///< 128 samples per second (default)
#define RATE_ADS1115_250SPS (0x00A0) ///< 250 samples per second
#define RATE_ADS1115_475SPS (0x00C0) ///< 475 samples per second
#define RATE_ADS1115_860SPS (0x00E0) ///< 860 samples per second




static uint16_t __readRegister(int i2c_addr, uint8_t reg) 
{
    uint8_t buffer[2];
    i2c_read(i2c_addr, reg, buffer, 2);
    return ((buffer[0] << 8) | buffer[1]);
}

static void __writeRegister(int i2c_addr, uint8_t reg, uint16_t value) 
{
    uint8_t buffer[2];
    buffer[0] = value >> 8;
    buffer[1] = value & 0xFF;
    i2c_write(i2c_addr, reg, buffer, 2);
}


static int16_t __readADC_SingleEnded(int i2c_addr, uint8_t channel) 
{
  uint16_t config;

  if (channel > 3) {
    return 0;
  }

  // Start with default values
  config = ADS1X15_REG_CONFIG_CQUE_NONE |    // Disable the comparator (default val)
           ADS1X15_REG_CONFIG_CLAT_NONLAT |  // Non-latching (default val)
           ADS1X15_REG_CONFIG_CPOL_ACTVLOW | // Alert/Rdy active low   (default val)
           ADS1X15_REG_CONFIG_CMODE_TRAD |   // Traditional comparator (default val)
           ADS1X15_REG_CONFIG_MODE_SINGLE;   // Single-shot mode (default)

  int m_gain = GAIN_ONE; /* +/- 4.096V range (limited to VDD +0.3V max!) */

  // Set PGA/voltage range
  config |= m_gain;

  // Set data rate
  config |= RATE_ADS1015_2400SPS;

  // Set single-ended input channel
  switch (channel) {
  case (0):
    config |= ADS1X15_REG_CONFIG_MUX_SINGLE_0;
    break;
  case (1):
    config |= ADS1X15_REG_CONFIG_MUX_SINGLE_1;
    break;
  case (2):
    config |= ADS1X15_REG_CONFIG_MUX_SINGLE_2;
    break;
  case (3):
    config |= ADS1X15_REG_CONFIG_MUX_SINGLE_3;
    break;
  }

  // Set 'start single-conversion' bit
  config |= ADS1X15_REG_CONFIG_OS_SINGLE;

  // Write config register to the ADC
  __writeRegister(i2c_addr, ADS1X15_REG_POINTER_CONFIG, config);

  // Wait for the conversion to complete
  while ((__readRegister(i2c_addr, ADS1X15_REG_POINTER_CONFIG) & 0x8000) == 0) {}

  // Read the conversion results
  return __readRegister(i2c_addr, ADS1X15_REG_POINTER_CONVERT);
}


static int am_test_init(void)
{
    int err;
    unsigned short v;
    int v2;

    err = gpio_init(bcm_peri_base_probe());
    if (err < 0) {
        goto err_free_dev;
    }

    err = i2c_init(bcm_peri_base_probe(), 0xB0);
    if (err < 0) {
        goto err_free_dev;
    }

    printk("test module loaded");

    // 4.096 / 32767 / 3.3 = 4.096 / (32767 * 3.3) = 4096 / (32767 * 3300) = 0 ~ 1
    v = __readADC_SingleEnded(0x48, 0);
    v2 = v * 4096 / 32767; // 4.096v
    printk(">>>>>>>>>>>>>> %d (%d %%) %d.%dv", v, v2 * 100 / 3300, v2 / 1000, v2 % 1000);

    v = __readADC_SingleEnded(0x48, 1);
    v2 = v * 4096 / 32767; // 4.096v
    printk(">>>>>>>>>>>>>> %d (%d %%) %d.%dv", v, v2 * 100 / 3300, v2 / 1000, v2 % 1000);

    return 0;

err_free_dev:
    i2c_close();
    gpio_clean();
    return err;
}

static void am_test_exit(void)
{
    i2c_close();
    gpio_clean();
    printk("test module unloaded");
}


module_init(am_test_init);
module_exit(am_test_exit);
