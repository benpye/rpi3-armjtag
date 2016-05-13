/*
 * Derived from code:
 * 
 * https://github.com/swarren/rpi-3-aarch64-demo
 * Copyright (C) 2012 Vikram Narayananan <vikram186@gmail.com>
 * (C) Copyright 2012-2016 Stephen Warren
 * Copyright (C) 1996-2000 Russell King
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 * https://github.com/dwelch67/raspberrypi/tree/master/armjtag
 * Copyright (c) 2012 David Welch dwelch@dwelch.com
 */

#include <stdint.h>

#define BIT(x) (1 << (x))

#define BCM2835_MU_BASE			0x3f215040

struct bcm283x_mu_regs {
	uint32_t io;
	uint32_t iir;
	uint32_t ier;
	uint32_t lcr;
	uint32_t mcr;
	uint32_t lsr;
	uint32_t msr;
	uint32_t scratch;
	uint32_t cntl;
	uint32_t stat;
	uint32_t baud;
};

/* This actually means not full, but is named not empty in the docs */
#define BCM283X_MU_LSR_TX_EMPTY		BIT(5)
#define BCM283X_MU_LSR_RX_READY		BIT(0)

#define __arch_getl(a)			(*(volatile unsigned int *)(a))
#define __arch_putl(v,a)		(*(volatile unsigned int *)(a) = (v))

#define dmb()		__asm__ __volatile__ ("" : : : "memory")
#define __iormb()	dmb()
#define __iowmb()	dmb()

#define readl(c)	({ uint32_t __v = __arch_getl(c); __iormb(); __v; })
#define writel(v,c)	({ uint32_t __v = v; __iowmb(); __arch_putl(__v,c); __v; })

static void bcm283x_mu_serial_putc(const char data)
{
	struct bcm283x_mu_regs *regs = (struct bcm283x_mu_regs *)BCM2835_MU_BASE;

	/* Wait until there is space in the FIFO */
	while (!(readl(&regs->lsr) & BCM283X_MU_LSR_TX_EMPTY))
		;

	/* Send the character */
	writel(data, &regs->io);
}

void dbg_puts(const char *s)
{
	while (*s)
		bcm283x_mu_serial_putc(*s++);
}

void dbg_puthex4(int val)
{
	int c;

	if (val < 10)
		c = val + '0';
	else
		c = val - 10 + 'A';

	bcm283x_mu_serial_putc(c);
}

void dbg_puthex32(uint32_t val)
{
	for (int i = 28; i >= 0; i -= 4)
		dbg_puthex4((val >> i) & 0xf);
}

void dbg_puthex64(uint64_t val)
{
	dbg_puthex32(val >> 32);
	dbg_puthex32(val & 0xffffffffU);
}

uint64_t read_mpidr(void)
{
	uint64_t v;
	__asm__ __volatile__("mrs %0, mpidr_el1" : "=r" (v) : : );
	return v;
}

uint64_t read_currentel(void)
{
	uint64_t v;
	__asm__ __volatile__("mrs %0, currentel" : "=r" (v) : : );
	return v;
}

uint64_t read_spsel(void)
{
	uint64_t v;
	__asm__ __volatile__("mrs %0, spsel" : "=r" (v) : : );
	return v;
}

void PUT32(int addr, uint32_t val)
{
	((uint32_t *)addr)[0] = val;
}

uint32_t GET32(int addr)
{
	return *((uint32_t *)addr);
}

void dummy()
{
	__asm__ __volatile__ ("");
}

#define SYSTIMERCLO 0x3F003004
#define GPFSEL0     0x3F200000
#define GPFSEL1     0x3F200004
#define GPFSEL2     0x3F200008
#define GPSET0      0x3F20001C
#define GPCLR0      0x3F200028
#define GPPUD       0x3F200094
#define GPPUDCLK0   0x3F200098

void enable_jtag()
{
	uint32_t ra, rb;
	
	PUT32(GPPUD,0);
	for(ra=0;ra<150;ra++) dummy(ra);
	PUT32(GPPUDCLK0,(1<<4)|(1<<22)|(1<<24)|(1<<25)|(1<<27));
	for(ra=0;ra<150;ra++) dummy(ra);
	PUT32(GPPUDCLK0,0);

	ra=GET32(GPFSEL0);
	ra&=~(7<<12); //gpio4
	ra|=2<<12; //gpio4 alt5 ARM_TDI
	PUT32(GPFSEL0,ra);

	ra=GET32(GPFSEL2);
	ra&=~(7<<6); //gpio22
	ra|=3<<6; //alt4 ARM_TRST
	ra&=~(7<<12); //gpio24
	ra|=3<<12; //alt4 ARM_TDO
	ra&=~(7<<15); //gpio25
	ra|=3<<15; //alt4 ARM_TCK
	ra&=~(7<<21); //gpio27
	ra|=3<<21; //alt4 ARM_TMS
	PUT32(GPFSEL2,ra);

	// ARM_TRST      22 GPIO_GEN3 P1-15 IN  (22 ALT4)
	// ARM_TDO     5/24 GPIO_GEN5 P1-18 OUT (24 ALT4)
	// ARM_TCK    13/25 GPIO_GEN6 P1-22 OUT (25 ALT4)
	// ARM_TDI     4/26 GPIO_GCLK P1-7   IN ( 4 ALT5)
	// ARM_TMS    12/27 CAM_GPIO  S5-11 OUT (27 ALT4)
}

void app(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3)
{
	dbg_puts("Waiting for JTAG\r\n");
	enable_jtag();
	while (1)
	{
		bcm283x_mu_serial_putc('.');
		for(int i = 0; i < 1000000; i++) dummy();
	}
}