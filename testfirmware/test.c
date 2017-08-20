/*


Power notes:
 * Enabling PLL on T1 costs a ton of power, ~4mA.
 * Kicking frequency from 8 to 16 MHz takes about 2mA.
 * Using T1 AT ALL takes ~.7mA.
 * Using anything other than idle sleep modes takes juice.
 * Enabling timer output to speaker doesn't add anything.
 * Proc going, even at IDLE takes up 1.2mA.
 * Could have saved more power if we had thought to hook speaker to T0.

TODO:
 * Examine possibility of a manual bridge mode.  Current bridge mode uses a lot of power to keep speaker in center.
   Should be possible to hold one wire high while PWMing the other and vice versa.  Would use MUCH less power when quiet.
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include "touch.h"

//#define BRIDGE_MODE  //Uses a lot more power but is louder.
//#define USE_SPI

void delay_ms(uint32_t time) {
  uint32_t i;
  for (i = 0; i < time; i++) {
    _delay_ms(1);
  }
}

#define NOOP asm volatile("nop" ::)

volatile unsigned char ThisCharToSend = 0;
unsigned char rcnt;

static void sendchr( char c )
{
	while( ! (USISR & (1<<USIOIF) ) );
	USISR |= 1<<USIOIF;
	USIDR = c;
}

#define sendstr( s ) {\
	for( rcnt = 0; s[rcnt] != 0; rcnt++ ) \
		sendchr( s[rcnt] ); \
	sendchr( 0x00 ); }\

static void sendhex1( unsigned char i )
{
	sendchr( (i<10)?(i+'0'):(i+'A'-10) );
}
static void sendhex2( unsigned char i )
{
	sendhex1( i>>4 );
	sendhex1( i&0x0f );
}
static void sendhex4( unsigned int i )
{
	sendhex2( i>>8 );
	sendhex2( i&0xFF);
}


static void setup_spi( void )
{
	/* Slave mode */
	USICR = (1<<USIWM0) | (1<<USICS1);// | (1<<USICS0) | (1<<USICLK);

	/*PortB:
	  B0: MOSI      (Input)
	  B1: MISO      (Output)
	  B2: SPI Clock (Input)
	  B3: LED	(Output) */
	DDRB |= (1<<1);
	DDRB &= 0xF2;
}


int freq = 1;
int times;
uint8_t inc;
int sfreq = 0;
int sfoverride = 0;

int  cpl;
int mute;
uint8_t Sine( uint16_t w ) { w>>=3; if( w & 0x100 ) return 255-(w&0xff); else return (w&0xff); }

uint8_t nextocr1d;


ISR( TIMER1_COMPD_vect )
{
}

ISR( TIMER1_OVF_vect )
{
	OCR1D = nextocr1d;
	if( mute ) { TouchNext(); return; }

	//sei();
	
	static int ticktime = 0;
	ticktime = 0;
	cpl+=sfreq;  //64 = 4 per, @ 8kHz so SPS is 32kSPS
	nextocr1d = Sine( cpl );
	//nextocr1d++; //Sine( cpl );
	//if( nextocr1d > 160 ) nextocr1d = 80;

}

int main()
{
	unsigned char i;
	unsigned short s1, s2, s3, s4, s5;
	
	DDRB = 3;  //LEDs output

	cli();

	//Setup clock
	CLKPR = 0x80;	/*Setup CLKPCE to be receptive*/
	CLKPR = 0x00;	/*No scalar*/
	OSCCAL = 0x80;  //0xFF;

	
#ifdef USE_SPI
	setup_spi();
#endif

	sei();


	DDRB |= 3;  //LEDs output
	
	//Pullup bits for inputs.
	DDRB &= ~( _BV(2) | _BV(3) | _BV(6) );
	DDRA &= ~( _BV(0) | _BV(1) | _BV(2) | _BV(3) | _BV(4) );
	PORTB |= _BV(2) | _BV(3) | _BV(6);
	PORTA |= _BV(0) | _BV(1) | _BV(2) | _BV(3) | _BV(4);

	//Timer 1 was mostly used for the speaker.
	DDRB |= _BV(4) | _BV(5); //speaker.  
	TCCR1A = 0;
	TCCR1B = _BV(CS10); //_BV(CS11) | _BV(CS12); // | _BV(CS13);
#ifdef BRIDGE_MODE
	TCCR1C = _BV(PWM1D) | _BV(COM1D0);
	TCCR1E =  _BV(5) | _BV(4); // | _BV(4); No /OC1D.  We don't want to do a full autobridge because we don't have an inductor on the speaker.
#else
	TCCR1C = _BV(PWM1D) | _BV(COM1D1);
	TCCR1E =  _BV(5); // | _BV(4); No /OC1D.  We don't want to do a full autobridge because we don't have an inductor on the speaker.
#endif
	TCCR1D = 0;
	OCR1C = 0xff;  //Set top
	OCR1D = 0x00;

//	PLLCSR = _BV(PCKE) | _BV(PLLE); //Enable PLL - makes max speed 263.7kHz.  WARNING: Adds ~4mA.
	PLLCSR = 0; //Don't use PLL. - max speed 33.35kHz
	TIMSK |= _BV(TOIE1) | _BV(OCIE1D); //Enable overflow interrupt. as well as the D-match interrupt.

//	sfreq = 150;

//Debug for checking to see if touch is working at all.
#if 0
#ifdef USE_SPI
	TIMSK = 0;
	TCCR1C = 0;
	TCCR1E = 0; //Disable PWM
	sendstr( "!!!!" );
	while(1)
	{
		sendhex2( TouchTest5(_BV(5)) );
		sendchr( ',' );
		sendhex2( TouchTest6(_BV(6)) );
		sendchr( ',' );
		sendhex2( TouchTest7(_BV(7)) );
		sendstr( "\n" );
	}
#endif
#endif

//Debug for checking to see if touch is working in the synchronized manner.
#if 0
#ifdef USE_SPI
	while(1)
	{

		sendhex2( touchvals[0] );
		sendchr( ',' );
		sendhex2( touchvals[1] );
		sendchr( ',' );
		sendhex2( touchvals[2] );
		sendchr( ',' );
		sendhex2( TCNT1 );
		sendstr( "\n" ); 
	}
#endif
#endif


	set_sleep_mode(SLEEP_MODE_IDLE);
	//set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	//set_sleep_mode(SLEEP_MODE_ADC);
	cli();
	sleep_enable();
	sei();


	uint8_t buttons;
	while(1)
	{
		//buttons++;
		buttons = ~( (PINA&0x1f) | ((PINB & _BV(2)) << 3) | ((PINB & _BV(3))<<3) | ((PINB&_BV(6))<<1));
		if( buttons & 0x02 ) { PORTB |= _BV(1); } else { PORTB &= ~_BV(1); }
		if( buttons & 0x01 ) { PORTB |= _BV(0); } else { PORTB &= ~_BV(0); }

		if( sfoverride ) sfreq = sfoverride;
		else if( buttons & 1 ) sfreq = 159;
		else if( buttons & 2 ) sfreq = 178;
		else if( buttons & 4 ) sfreq = 212;
		else if( buttons & 8 ) sfreq = 238;
		else if( buttons & 16 ) sfreq = 283;
		else if( buttons & 32 ) sfreq = 317;
		else if( buttons & 64 ) sfreq = 357;
		else if( buttons & 128 ) sfreq = 424;
		else sfreq = 0;

		if( sfreq == 0 ) mute = 1; else mute = 0;

/*
		s1 = GetTime( 5 );
		_delay_us(1000);
		s2 = GetTime( 6 );
		_delay_us(1000);
		s3 = GetTime( 7 );
		_delay_us(1000);

/*
		if( s1 > 5)
			sfoverride = 150;
		else if( s2 > 5 )
			sfoverride = 160;
		else if( s3 > 5 )
			sfoverride = 170;
		else
			sfoverride = 0;
*/

//		sendhex2(s1);
//		sendchr(10);

/*		s2 = GetTime( 6 );
		_delay_us(1000);
		s3 = GetTime( 7 );
		_delay_us(1000);
/*

		int maximumtouch = 0;
		int angle = 0;

		if( s1 < s2 && s1 < s3 )
		{
			//S1 is minimum (baseline)
			s2 -= s1;
			s3 -= s1;

			//Compare S2/S3
			if( s2 > s3 )
			{
				//S2 is max.
				maximumtouch = s2;
				angle = 0 + (30 * (int16_t)s3) / s2;
			}
			else
			{
				//S3 is max.
				maximumtouch = s3;
				angle = 30 + (30 * (int16_t)s2) / s3;
			}
		}
		else if( s2 < s3 )
		{
			s1 -= s2;
			s3 -= s2;
			if( s1 > s3 )
			{
				maximumtouch = s1;
				angle = 60 + (30 * (int16_t)s3) / s1;
			}
			else
			{
				maximumtouch = s3;
				angle = 90 + (30 * (int16_t)s1) / s3;
			}

		}
		else
		{
			s2 -= s3;
			s1 -= s3;
			if( s1 > s2 )
			{
				maximumtouch = s1;
				angle = 60 + (30 * (int16_t)s2) / s1;
			}
			else
			{
				maximumtouch = s2;
				angle = 90 + (30 * (int16_t)s1) / s2;
			}
		}

		if( maximumtouch > 4 )
		{
			sfoverride = 180 + angle;
		}
		else
		{
			sfoverride = 0;
		}
*/

		sleep_enable();
		sleep_cpu();
		sleep_disable();
	}
}

