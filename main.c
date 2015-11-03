#include <msp430.h> 
#include <stdio.h>

char timestr[30];
unsigned int seconds = 0;

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
		seconds--;
		time = 0xffff - (prevtime - curtime);
	} else {
		time = curtime - prevtime;
	}
	prevtime = curtime;
	sprintf(timestr, "T %u %u %u\r\n", pulses, seconds, time);

	/* Enable UART TX Interrupt to start outputting string */
	IE2 |= UCA0TXIE;
	seconds = 0;

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
	seconds++;

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
