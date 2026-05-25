#if defined(__dsPIC33F__)
#include "p33fxxxx.h"
#endif

#include "adcDrv1.h"

#define  SAMP_BUFF_SIZE	 		10		// Dimensiunea buffer-ului in care se salveaza rezultatele conversiei

//INITIALIZARE ADC PENTRU SCANARE CANAL AN4(RB2)
void initAdc1(void)
{
	AD1CON1bits.ADON = 0;  
    AD1CON1bits.AD12B = 1;
    AD1CON1bits.SSRC = 2;
    AD1CON1bits.ASAM = 1;
    AD1CON2bits.CSCNA = 1;
    AD1CON3bits.ADRC = 0;
    AD1CON3bits.ADCS = 63;
    AD1CSSL = 0x0020;
    AD1PCFGL = 0xFFFF;      
    AD1PCFGLbits.PCFG5 = 0;
    _AD1IF = 0;
    _AD1IE = 1;
    AD1CON1bits.ADON = 1;  
}

// Timer-ul 3 este setat sa starteze conversia AD la fiecare 125 microsecunde (8Khz Rate).
void initTmr3() 
{
        TMR3 = 0;
        PR3 = 50000;
        T3CONbits.TON = 1;	// Start Timer 3
}

int  ain4Buff[SAMP_BUFF_SIZE];
int  sampleCounter=0;

/*
// rutina de tratare a intreruperii convertorului AD
void __attribute__((interrupt, no_auto_psv)) _ADC1Interrupt(void)
{
	ain4Buff[sampleCounter++]=ADC1BUF0; 
	if(sampleCounter==SAMP_BUFF_SIZE)
		sampleCounter=0;
    IFS0bits.AD1IF = 0;		// Achita intreruperea convertorului AD
}
*/