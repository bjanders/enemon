#include <msp430.h> 
#include <stdio.h>


/* This program measures the interval between the falling edge of pulses on
 * port P2.0 with a 32768 Hz resolution (~0.03 ms). The result is written on
 * the serial port for each pulse in the follwing format:
 * "T <pulses> <timeroverflows> <timer>"
 * where:
 * - <pulses> is the total number of pulses received since startup. The value
 *   will overflow at 0xffff
 * - <timeroverflows> is the number of times the timer has overflown 0xffff
 *    between two pulses
 * - <timer> the timer value
 *
 * The total time between two pulses is
 *    t = <timeroverflows> * 0x10000 + <timer> 
 * in 32786 Hz (0x8000 Hz) increments, i.e., in seconds this is t/32768 Hz
 *    
 * We have Timer1 constantly running from 0 to 0xffff, running at a rate of
 * 32768 Hz. This means the timer will overflow every 2 seconds,
 * 32768 Hz * 0xffff = 2 s. When the timer overflows, timer1_a1() is called
 * where the variable 'overflows' is incrased by 1.
 * 
 * Whenever there is an interrupt on P2.0 timer1_a0() is called. Here the
 * current timer value is recorded in 'curtime'. The value of the timer in the 
 * previous interrupt is recorded in 'prevtime'. 'overflows' is reset to 0.
 */

char timestr[30];
unsigned int overflows = 0;

#pragma vector=USCIAB0TX_VECTOR
interrupt void
uart_tx(void)
{
	static unsigned int i = 0;
	if (timestr[i] != 0) {
		UCA0TXBUF = timestr[i];
		i++;
	}
	if (timestr[i] == 0) {
		/* Disable interrupts if NULL char */
		IE2 &= ~UCA0TXIE;
		i = 0;
	}
}


#pragma vector=TIMER1_A0_VECTOR
interrupt void
timer1_a0(void) {
	static unsigned int pulses = 0;
	static unsigned int prevtime = 0;
	unsigned int curtime;
	unsigned int time;
	/* TACCR0 interrupt vector for TACCR0 CCIFG */
	/* Capture on P2.0*/
	pulses++;
	curtime = TA1CCR0;
	if (curtime < prevtime) {
		overflows--;
		time = 0xffff - (prevtime - curtime);
	} else {
		time = curtime - prevtime;
	}
	prevtime = curtime;
	sprintf(timestr, "T %u %u %u\r\n", pulses, overflows, time);

	/* Enable UART TX Interrupt to start outputting string */
	IE2 |= UCA0TXIE;
	overflows = 0;

	/* Toggle LED to show pulse */
	P1OUT ^= BIT6;
}


#pragma vector=TIMER1_A1_VECTOR
interrupt void
timer1_a1(void)
{
	/* TAIV interrupt vector for all other CCIFG flags and TAIFG */

	/* Here only TAIFG is enabled, called when the timer overflows,
	 * i.e. every 2 seconds (32768 Hz * 0xffff = 2s)
	 */

	/* Read T11IV to clear interrupt */
	TA1IV;
	overflows++;

	/* Toggle LED to show activity */
	P1OUT ^= BIT0;
}



inline void
init_timer_a1(void)
{
	/* Timer1.0 CCI0A on P2.0 */
	P2SEL |= BIT0;

	/* CM_2: Capture of falling edge,
	 * CCIS_0: Input from CCIxA,
	 * SCS: Sync capture with timer clock,
	 * CAP: Capture mode,
	 * CCIE: interrupt enable */
	TA1CCTL0 = CM_2 + CCIS_0 + SCS + CAP + CCIE;

	/* TASSEL_1: Clock source ACLK (32kHz crystal),
	 * MC_2: count to 0xffff,
	 * TAIE: Enable interrupt */
	TA1CTL = TASSEL_1 + MC_2 + TAIE;
}

inline void
init_uart(void)
{
	UCA0CTL1 = UCSWRST;
	UCA0CTL1 |= 0x80; /* Clock source SMCLK */

	/* enable TX on P1.2*/
	P1SEL |= BIT2;
	P1SEL2 |= BIT2;

	/* 9600 baud @ 8 MHz */
	UCA0BR0 = 0x41;
	UCA0BR1 = 0x03;
	UCA0MCTL = 0x04;
	UCA0CTL1 &= ~UCSWRST;
/*	IE2 |= UCA0RXIE; */
}


/*
 * main.c
 */
int
main(void) {
	/* Stop watchdog timer */
	WDTCTL = WDTPW | WDTHOLD;

	/* 12.5 pF oscillator capacitor */
	BCSCTL3 = XCAP_3;

	/* Use calibrated 8MHz settings */
	BCSCTL1= CALBC1_8MHZ;
	DCOCTL = CALDCO_8MHZ;

	/* LEDs */
	P1DIR = BIT0 + BIT6;

	init_uart();
	init_timer_a1();
	_enable_interrupts();

	LPM3;
}
