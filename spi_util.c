/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#include <linux/kernel.h>
#include <linux/delay.h>
#include <asm/io.h>

#include "spi_util.h"
#include "bcm_peri.h"
#include "gpio_util.h"
#include "bcm2835.h"


/*! Return the minimum of 2 numbers */
#ifndef MIN
#define MIN(a, b) (a < b ? a : b)
#endif


static volatile uint32_t* bcm2835_spi0 = NULL;
static int spi0_ref_count = 0;


/* Initialise this library. */
int spi_init(u32 peri_base_addr, int size)
{
    if (spi0_ref_count++ > 0) return 0;

    if(size == 0) size = 0xB0;

    if ((bcm2835_spi0 = (volatile uint32_t *)ioremap(peri_base_addr + BCM2835_SPI0_BASE, size)) == NULL) {
        pr_err("io remap failed");
        spi0_ref_count--;
        return -EINVAL;
    }

    gpio_fsel(RPI_GPIO_P1_26, BCM2835_GPIO_FSEL_ALT0); /* CE1 */
    gpio_fsel(RPI_GPIO_P1_24, BCM2835_GPIO_FSEL_ALT0); /* CE0 */
    gpio_fsel(RPI_GPIO_P1_21, BCM2835_GPIO_FSEL_ALT0); /* MISO */
    gpio_fsel(RPI_GPIO_P1_19, BCM2835_GPIO_FSEL_ALT0); /* MOSI */
    gpio_fsel(RPI_GPIO_P1_23, BCM2835_GPIO_FSEL_ALT0); /* CLK */

    return 0;
}

/* Close this library and deallocate everything */
void spi_close(void)
{
    if (--spi0_ref_count > 0) return;

    // gpio_fsel(RPI_GPIO_P1_26, BCM2835_GPIO_FSEL_INPT); /* CE1 */
    // gpio_fsel(RPI_GPIO_P1_24, BCM2835_GPIO_FSEL_INPT); /* CE0 */
    // gpio_fsel(RPI_GPIO_P1_21, BCM2835_GPIO_FSEL_INPT); /* MISO */
    // gpio_fsel(RPI_GPIO_P1_19, BCM2835_GPIO_FSEL_INPT); /* MOSI */
    // gpio_fsel(RPI_GPIO_P1_23, BCM2835_GPIO_FSEL_INPT); /* CLK */

    if (bcm2835_spi0 != NULL) {
        iounmap((void *)bcm2835_spi0);
        bcm2835_spi0 = NULL;
    }
}    


int spi_begin(int spi_channel)
{
    volatile uint32_t* paddr;

    if (bcm2835_spi0 == NULL)
      return -ENODEV; /* bcm2835_init() failed, or not root */
    
    /* Set the SPI CS register to the some sensible defaults */
    paddr = bcm2835_spi0 + BCM2835_SPI0_CS/sizeof(uint32_t);
    bcm_peri_write(paddr, 0); /* All 0s */
    
    /* Clear TX and RX fifos */
    bcm_peri_write_nb(paddr, BCM2835_SPI0_CS_CLEAR);

    return 0; // OK
}

void spi_end(int spi_channel)
{  
    /* Set all the SPI0 pins back to input */
    // gpio_fsel(RPI_GPIO_P1_26, BCM2835_GPIO_FSEL_INPT); /* CE1 */
    // gpio_fsel(RPI_GPIO_P1_24, BCM2835_GPIO_FSEL_INPT); /* CE0 */
    // gpio_fsel(RPI_GPIO_P1_21, BCM2835_GPIO_FSEL_INPT); /* MISO */
    // gpio_fsel(RPI_GPIO_P1_19, BCM2835_GPIO_FSEL_INPT); /* MOSI */
    // gpio_fsel(RPI_GPIO_P1_23, BCM2835_GPIO_FSEL_INPT); /* CLK */
}

// void spi_setBitOrder(uint8_t __attribute__((unused)) order)
// {
//     /* BCM2835_SPI_BIT_ORDER_MSBFIRST is the only one supported by SPI0 */
// }

/* defaults to 0, which means a divider of 65536.
// The divisor must be a power of 2. Odd numbers
// rounded down. The maximum SPI clock rate is
// of the APB clock
*/
void spi_setClockDivider(uint16_t divider)
{
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CLK/sizeof(uint32_t);
    bcm_peri_write(paddr, divider);
}

void spi_setDataMode(uint8_t mode)
{
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/sizeof(uint32_t);
    /* Mask in the CPO and CPHA bits of CS */
    bcm_peri_set_bits(paddr, mode << 2, BCM2835_SPI0_CS_CPOL | BCM2835_SPI0_CS_CPHA);
}

/* Writes (and reads) a single byte to SPI */
uint8_t spi_transfer(uint8_t value)
{
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/sizeof(uint32_t);
    volatile uint32_t* fifo = bcm2835_spi0 + BCM2835_SPI0_FIFO/sizeof(uint32_t);
    uint32_t ret;

    /* This is Polled transfer as per section 10.6.1
    // BUG ALERT: what happens if we get interupted in this section, and someone else
    // accesses a different peripheral? 
    // Clear TX and RX fifos
    */
    bcm_peri_set_bits(paddr, BCM2835_SPI0_CS_CLEAR, BCM2835_SPI0_CS_CLEAR);

    /* Set TA = 1 */
    bcm_peri_set_bits(paddr, BCM2835_SPI0_CS_TA, BCM2835_SPI0_CS_TA);

    /* Maybe wait for TXD */
    while (!(bcm_peri_read(paddr) & BCM2835_SPI0_CS_TXD)) {}

    /* Write to FIFO, no barrier */
    bcm_peri_write_nb(fifo, value);

    /* Wait for DONE to be set */
    while (!(bcm_peri_read_nb(paddr) & BCM2835_SPI0_CS_DONE)) {}

    /* Read any byte that was sent back by the slave while we sere sending to it */
    ret = bcm_peri_read_nb(fifo);

    /* Set TA = 0, and also set the barrier */
    bcm_peri_set_bits(paddr, 0, BCM2835_SPI0_CS_TA);

    return ret;
}

/* Writes (and reads) an number of bytes to SPI */
void spi_transfernb(char* tbuf, char* rbuf, uint32_t len)
{
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/sizeof(uint32_t);
    volatile uint32_t* fifo = bcm2835_spi0 + BCM2835_SPI0_FIFO/sizeof(uint32_t);
    uint32_t TXCnt = 0;
    uint32_t RXCnt = 0;

    /* This is Polled transfer as per section 10.6.1
    // BUG ALERT: what happens if we get interupted in this section, and someone else
    // accesses a different peripheral? 
    */

    /* Clear TX and RX fifos */
    bcm_peri_set_bits(paddr, BCM2835_SPI0_CS_CLEAR, BCM2835_SPI0_CS_CLEAR);

    /* Set TA = 1 */
    bcm_peri_set_bits(paddr, BCM2835_SPI0_CS_TA, BCM2835_SPI0_CS_TA);

    /* Use the FIFO's to reduce the interbyte times */
    while((TXCnt < len) || (RXCnt < len))
    {
        /* TX fifo not full, so add some more bytes */
        while((bcm_peri_read(paddr) & BCM2835_SPI0_CS_TXD) && (TXCnt < len))
        {
           bcm_peri_write_nb(fifo, tbuf[TXCnt]);
           TXCnt++;
        }
        /* Rx fifo not empty, so get the next received bytes */
        while((bcm_peri_read(paddr) & BCM2835_SPI0_CS_RXD) && (RXCnt < len))
        {
           rbuf[RXCnt] = bcm_peri_read_nb(fifo);
           RXCnt++;
        }
    }
    /* Wait for DONE to be set */
    while (!(bcm_peri_read_nb(paddr) & BCM2835_SPI0_CS_DONE)) {}

    /* Set TA = 0, and also set the barrier */
    bcm_peri_set_bits(paddr, 0x00, BCM2835_SPI0_CS_TA);
}

/* Writes an number of bytes to SPI */
void spi_writenb(const char* tbuf, uint32_t len)
{
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/sizeof(uint32_t);
    volatile uint32_t* fifo = bcm2835_spi0 + BCM2835_SPI0_FIFO/sizeof(uint32_t);
    uint32_t i;

    /* This is Polled transfer as per section 10.6.1
    // BUG ALERT: what happens if we get interupted in this section, and someone else
    // accesses a different peripheral?
    // Answer: an ISR is required to issue the required memory barriers.
    */

    /* Clear TX and RX fifos */
    bcm_peri_set_bits(paddr, BCM2835_SPI0_CS_CLEAR, BCM2835_SPI0_CS_CLEAR);

    /* Set TA = 1 */
    bcm_peri_set_bits(paddr, BCM2835_SPI0_CS_TA, BCM2835_SPI0_CS_TA);

    for (i = 0; i < len; i++)
    {
	/* Maybe wait for TXD */
	while (!(bcm_peri_read(paddr) & BCM2835_SPI0_CS_TXD))
	    ;
	
	/* Write to FIFO, no barrier */
	bcm_peri_write_nb(fifo, tbuf[i]);
	
	/* Read from FIFO to prevent stalling */
	while (bcm_peri_read(paddr) & BCM2835_SPI0_CS_RXD)
	    (void) bcm_peri_read_nb(fifo);
    }
    
    /* Wait for DONE to be set */
    while (!(bcm_peri_read_nb(paddr) & BCM2835_SPI0_CS_DONE)) {
	while (bcm_peri_read(paddr) & BCM2835_SPI0_CS_RXD)
		(void) bcm_peri_read_nb(fifo);
    };

    /* Set TA = 0, and also set the barrier */
    bcm_peri_set_bits(paddr, 0x00, BCM2835_SPI0_CS_TA);
}

/* Writes (and reads) an number of bytes to SPI
// Read bytes are copied over onto the transmit buffer
*/
void spi_transfern(char* buf, uint32_t len)
{
    spi_transfernb(buf, buf, len);
}

void spi_chipSelect(uint8_t cs)
{
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/sizeof(uint32_t);
    /* Mask in the CS bits of CS */
    bcm_peri_set_bits(paddr, cs, BCM2835_SPI0_CS_CS);
}

void spi_setChipSelectPolarity(uint8_t cs, uint8_t active)
{
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/sizeof(uint32_t);
    uint8_t shift = 21 + cs;
    /* Mask in the appropriate CSPOLn bit */
    bcm_peri_set_bits(paddr, active << shift, 1 << shift);
}

void spi_write(uint16_t data) 
{
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/sizeof(uint32_t);
    volatile uint32_t* fifo = bcm2835_spi0 + BCM2835_SPI0_FIFO/sizeof(uint32_t);

    /* Clear TX and RX fifos */
    bcm_peri_set_bits(paddr, BCM2835_SPI0_CS_CLEAR, BCM2835_SPI0_CS_CLEAR);

    /* Set TA = 1 */
    bcm_peri_set_bits(paddr, BCM2835_SPI0_CS_TA, BCM2835_SPI0_CS_TA);

	/* Maybe wait for TXD */
	while (!(bcm_peri_read(paddr) & BCM2835_SPI0_CS_TXD)) {}

	/* Write to FIFO */
	bcm_peri_write_nb(fifo,  (uint32_t) data >> 8);
	bcm_peri_write_nb(fifo,  data & 0xFF);

    /* Wait for DONE to be set */
    while (!(bcm_peri_read_nb(paddr) & BCM2835_SPI0_CS_DONE)) {}

    /* Set TA = 0, and also set the barrier */
    bcm_peri_set_bits(paddr, 0x00, BCM2835_SPI0_CS_TA);
}
