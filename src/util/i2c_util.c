#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <asm/io.h>

#include "i2c_util.h"
#include "gpio_util.h"


/*
 * Defines for I2C peripheral (aka BSC, or Broadcom Serial Controller)
 */

#define BSC1_C		(*(bsc1 + 0x00))
#define BSC1_S		(*(bsc1 + 0x01))
#define BSC1_DLEN	(*(bsc1 + 0x02))
#define BSC1_A		(*(bsc1 + 0x03))
#define BSC1_FIFO	(*(bsc1 + 0x04))

#define BSC_C_I2CEN	(1 << 15)
#define BSC_C_INTR	(1 << 10)
#define BSC_C_INTT	(1 << 9)
#define BSC_C_INTD	(1 << 8)
#define BSC_C_ST	(1 << 7)
#define BSC_C_CLEAR	(1 << 4)
#define BSC_C_READ	(1)

#define START_READ	(BSC_C_I2CEN | BSC_C_ST | BSC_C_CLEAR | BSC_C_READ)
#define START_WRITE	(BSC_C_I2CEN | BSC_C_ST)

#define BSC_S_CLKT	(1 << 9)
#define BSC_S_ERR	(1 << 8)
#define BSC_S_RXF	(1 << 7)
#define BSC_S_TXE	(1 << 6)
#define BSC_S_RXD	(1 << 5)
#define BSC_S_TXD	(1 << 4)
#define BSC_S_RXR	(1 << 3)
#define BSC_S_TXW	(1 << 2)
#define BSC_S_DONE	(1 << 1)
#define BSC_S_TA	(1)

#define CLEAR_STATUS	(BSC_S_CLKT | BSC_S_ERR | BSC_S_DONE)

static volatile unsigned* bsc1;


#define BSC1_BASE		(PERI_BASE + 0x804000)


int i2c_init(void) 
{
    /* Set up i2c pointer for direct register access */
    if ((bsc1 = ioremap(BSC1_BASE, 0xB0)) == NULL) {
        pr_err("io remap failed\n");
        return -EBUSY;
    }

    INP_GPIO(2);
    SET_GPIO_ALT(2, 0);
    INP_GPIO(3);
    SET_GPIO_ALT(3, 0);

    return 0;
}

void wait_i2c_done(void) 
{
    while (!(BSC1_S & BSC_S_DONE)) {
        udelay(100);
    }
}

// Function to write data to an I2C device via the FIFO.  This doesn't refill the FIFO, so writes are limited to 16 bytes
// including the register address. len specifies the number of bytes in the buffer.
void i2c_write(char dev_addr, char reg_addr, char *buf, int len) 
{
    int i;

    BSC1_A = dev_addr;
    BSC1_DLEN = len + 1; // one byte for the register address, plus the buffer length

    BSC1_FIFO = reg_addr; // start register address
    for (i = 0; i < len; i++) {
        BSC1_FIFO = buf[i];
    }

    BSC1_S = CLEAR_STATUS; // Reset status bits (see #define)
    BSC1_C = START_WRITE; // Start Write (see #define)

    wait_i2c_done();
}

// Function to read a number of bytes into a  buffer from the FIFO of the I2C controller
void i2c_read(char dev_addr, char reg_addr, char *buf, int len) 
{
    int bufidx;

    i2c_write(dev_addr, reg_addr, NULL, 0);

    BSC1_DLEN = (unsigned char)len;
    BSC1_S = CLEAR_STATUS; // Reset status bits (see #define)
    BSC1_C = START_READ; // Start Read after clearing FIFO (see #define)

    bufidx = 0;
    while (!(BSC1_S & BSC_S_DONE)) {
        // Wait for some data to appear in the FIFO
        while ((BSC1_S & BSC_S_TA) && !(BSC1_S & BSC_S_RXD));

        // Consume the FIFO
        while ((BSC1_S & BSC_S_RXD) && (bufidx < len)) {
            buf[bufidx++] = BSC1_FIFO;
        }
    }

    wait_i2c_done();
}
