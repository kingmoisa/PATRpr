/* Standard includes. */
#include <stdio.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "croutine.h"
#include "semphr.h"

/* Demo application includes. */
#include "BlockQ.h"
#include "crflash.h"
#include "blocktim.h"
#include "integer.h"
#include "comtest2.h"
#include "partest.h"
#include "timertest.h"

// Includes proprii
#include "new_lcd.h"
#include "new_serial.h"
#include "ds18b20.h"
#include "adcDrv1.h"

/* Demo task priorities. */
#define mainBLOCK_Q_PRIORITY                ( tskIDLE_PRIORITY + 2 )
#define mainCHECK_TASK_PRIORITY             ( tskIDLE_PRIORITY + 3 )
#define mainCOM_TEST_PRIORITY               ( 2 )

/* Stack sizes and periods */
#define mainCHECK_TAKS_STACK_SIZE           ( configMINIMAL_STACK_SIZE * 2 )
#define mainCHECK_TASK_PERIOD               ( ( portTickType ) 3000 / portTICK_RATE_MS )
#define mainNUM_FLASH_COROUTINES            ( 5 )
#define mainCOM_TEST_BAUD_RATE              ( 9600 )
#define comBUFFER_LEN                       ( 200 )
#define comNO_BLOCK                         ( ( portTickType ) 0 )
#define comRX_BLOCK_TIME                    ( ( portTickType ) 0xffff )
#define PWM_MIN    625  
#define PWM_CENTER 937  
#define PWM_MAX    1250  

// Select Internal FRC at POR
_FOSCSEL(FNOSC_FRC);
// Enable Clock Switching and Configure
_FOSC(FCKSM_CSECMD & OSCIOFNC_OFF);        
_FWDT(FWDTEN_OFF);      // Watchdog Timer Disabled

/* --- VARIABILE GLOBALE --- */
volatile int aplicatie_pornita = 1; // 1 = pornit (starea default), 0 = oprit
volatile int isAutoMode = 1; // 1 = AUTO, 0 = MANUAL
volatile unsigned int valoarea_conversie_adc_bruta = 0; 
volatile float valoarea_conversie_adc_reala = 0.0;    
volatile float temperatura_curenta = 0.0;
volatile char ultima_comanda = '-';
xTaskHandle hTaskTemp;
xTaskHandle hTaskTens;
xTaskHandle hTaskServo;
xTaskHandle hTaskSerial;
xSemaphoreHandle xMutexLCD = NULL;

xQueueHandle xCoadaTemp;
xQueueHandle xCoadaTens;
static void prvSetupHardware( void );

// Rutina întrerupere Buton S2 (INT0)
void __attribute__((interrupt, no_auto_psv)) _INT0Interrupt(void)
{
    aplicatie_pornita = !aplicatie_pornita;
    IFS0bits.INT0IF = 0;                    // Resetare flag intrerupere
}

void __attribute__((interrupt, no_auto_psv)) _ADC1Interrupt(void)
{
    valoarea_conversie_adc_bruta = ADC1BUF0; // Preia rezultatul conversiei din buffer
    IFS0bits.AD1IF = 0;     // Reseteaza flag-ul pentru a permite o noua întrerupere
}

/* --- TASK CITIRE TEMPERATURA --- */
void vTaskTemperature(void *pvParameters)
{
    float temperatura;
    char buffer_lcd[10];
    for (;;)
    {
        if (aplicatie_pornita)
        {
            temperatura_curenta = ds1820_read();
			xQueueSend(xCoadaTemp, (void *)&temperatura_curenta, 0); 
            sprintf(buffer_lcd, "%.1f", (double)temperatura_curenta);
			if( xSemaphoreTake( xMutexLCD, portMAX_DELAY ) == pdTRUE ){
            LCD_Goto(1, 7); 
            LCD_printf(buffer_lcd);
			xSemaphoreGive( xMutexLCD );
			}
        }
      
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

void TaskTensiune(void *pvParameters)
{
    char sirTensiune[15];

    for (;;)
    {
        if(aplicatie_pornita)
        {
    
            valoarea_conversie_adc_reala = ((float)valoarea_conversie_adc_bruta / 4095.0) * 3.3;
            xQueueSend(xCoadaTens, (void *)&valoarea_conversie_adc_reala, 0);
            sprintf(sirTensiune, "%.2f V", (double)valoarea_conversie_adc_reala);
            if( xSemaphoreTake( xMutexLCD, portMAX_DELAY ) == pdTRUE ){
            LCD_Goto(3, 7);
            LCD_printf(sirTensiune);
			xSemaphoreGive( xMutexLCD );
			}
        }
        
        // Task-ul cedeaza procesorul pentru 300 ms
        vTaskDelay(300 / portTICK_RATE_MS);
    }
}

void TaskServomotor(void *pvParameters)
{
    unsigned int pwmDutyCycle = PWM_CENTER;
    
    // Variabile locale, sigure, folosite DOAR de motor
    float temp_locala = 20.0; 
    float tens_locala = 0.0;

    for (;;)
    {
        if(aplicatie_pornita)
        {
            xQueueReceive(xCoadaTemp, &temp_locala, 0);
            xQueueReceive(xCoadaTens, &tens_locala, 0);

            if (isAutoMode == 1) 
            {
                if (temp_locala <= 20.0) pwmDutyCycle = PWM_MIN;
                else if (temp_locala >= 30.0) pwmDutyCycle = PWM_MAX;
                else {
                    pwmDutyCycle = PWM_MIN + (unsigned int)(((temp_locala - 20.0) * (PWM_MAX - PWM_MIN)) / 10.0);
                }
            }
            else 
            {
                if (tens_locala <= 1.0) pwmDutyCycle = PWM_MIN;
                else if (tens_locala >= 3.0) pwmDutyCycle = PWM_MAX;
                else {
                    pwmDutyCycle = PWM_MIN + (unsigned int)(((tens_locala - 1.0) * (PWM_MAX - PWM_MIN)) / 2.0);
                }
            }
            
            OC1RS = pwmDutyCycle; 
        }
        vTaskDelay(100 / portTICK_RATE_MS); 
    }
}

/* --- TASK COMUNICATIE SERIALA (MENIU) --- */
void TaskSerial(void *pvParameters)
{
    signed char cByteRxed;
    char outStr[64];
    char cmdDisplay[16];

    vSerialPutString(NULL, (signed char *)"\r\n--- MENIU CONTROL DS18B20 ---\r\n", comNO_BLOCK);
    vSerialPutString(NULL, (signed char *)"1. Afiseaza Temperatura\r\n", comNO_BLOCK);
    vSerialPutString(NULL, (signed char *)"2. Schimba Mod (AUTO/MANUAL)\r\n", comNO_BLOCK);
    vSerialPutString(NULL, (signed char *)"3. Status Sistem\r\n", comNO_BLOCK);
    vSerialPutString(NULL, (signed char *)"Astept comanda: ", comNO_BLOCK);

    for (;;)
    {
        if (aplicatie_pornita)
        {
            if (xSerialGetChar(NULL, &cByteRxed, portMAX_DELAY))
            {
                if (cByteRxed == '\r' || cByteRxed == '\n' || cByteRxed == ' ') {
                    continue;
                }

                if (cByteRxed == '1' || cByteRxed == '2' || cByteRxed == '3')
                {
                    ultima_comanda = cByteRxed; 
                    sprintf(cmdDisplay, "Ult.Com: %c", ultima_comanda);
					if( xSemaphoreTake( xMutexLCD, portMAX_DELAY ) == pdTRUE ){
                    LCD_Goto(4, 1);
                    LCD_printf(cmdDisplay);
					xSemaphoreGive( xMutexLCD );
					}
                }

                if (cByteRxed == '1')
                {
                    sprintf(outStr, "\r\n-> Temperatura este: %.1f C\r\n", (double)temperatura_curenta);
                    vSerialPutString(NULL, (signed char *)outStr, comNO_BLOCK);
                }
                else if (cByteRxed == '2')
                {
                    isAutoMode = !isAutoMode; 
                    if( xSemaphoreTake( xMutexLCD, portMAX_DELAY ) == pdTRUE ){
                    LCD_Goto(2, 6);
                    if (isAutoMode) LCD_printf("AUTO  ");
                    else LCD_printf("MANUAL");
					xSemaphoreGive( xMutexLCD );
                    }
                    sprintf(outStr, "\r\n-> Mod schimbat pe: %s\r\n", isAutoMode ? "AUTO" : "MANUAL");
                    vSerialPutString(NULL, (signed char *)outStr, comNO_BLOCK);
                }
                else if (cByteRxed == '3')
                {
                    sprintf(outStr, "\r\n-> STATUS: Mod %s | T: %.1fC | V: %.2fV\r\n", 
                            isAutoMode ? "AUTO" : "MANUAL", 
                            (double)temperatura_curenta, 
                            (double)valoarea_conversie_adc_reala);
                    vSerialPutString(NULL, (signed char *)outStr, comNO_BLOCK);
                }
                else
                {
                    vSerialPutString(NULL, (signed char *)"\r\n-> COMANDA INVALIDA!\r\n", comNO_BLOCK);
                }
               
                vSerialPutString(NULL, (signed char *)"Astept comanda: ", comNO_BLOCK);
            }
        }
        else
        {
            vTaskDelay(200 / portTICK_RATE_MS); 
        }
    }
}

/* --- TASK CONTROL LED --- */
void vTaskSystemState(void *pvParameters)
{
    for (;;)
    {
        // Nu mai avem nevoie de stare_anterioara ?i de Suspend/Resume, 
        // deoarece task-urile se auto-filtreazã prin "if(aplicatie_pornita)" în buclele lor.

        if (aplicatie_pornita == 1)
        {
            _RB11 = 0; // LED aprins continuu
            vTaskDelay(100 / portTICK_RATE_MS); 
        }
        else
        {
            _RB11 = ~_RB11; // Clipire LED
            vTaskDelay(500 / portTICK_RATE_MS); 
        }
    }
}

int main( void )
{
    prvSetupHardware();
	xMutexLCD = xSemaphoreCreateMutex();
    if(xMutexLCD == NULL) {
        while(1); 
    }
	xCoadaTemp = xQueueCreate(1, sizeof(float));
    xCoadaTens = xQueueCreate(1, sizeof(float));
	
	if(xCoadaTemp == NULL || xCoadaTens == NULL) {
        while(1);
    }
	
    xTaskCreate(vTaskTemperature, (signed portCHAR *) "Temp", 300, NULL, tskIDLE_PRIORITY + 2, &hTaskTemp);
    xTaskCreate(TaskTensiune, (signed portCHAR *)"Tens", 300, NULL, tskIDLE_PRIORITY + 3, &hTaskTens);
    xTaskCreate(TaskServomotor, (signed portCHAR *)"Servo", 300, NULL, tskIDLE_PRIORITY + 3, &hTaskServo);
    xTaskCreate(TaskSerial, (signed portCHAR *)"Serial", 300, NULL, tskIDLE_PRIORITY + 2, &hTaskSerial);
    
    xTaskCreate(vTaskSystemState, (signed portCHAR *) "State", 300, NULL, tskIDLE_PRIORITY + 1, NULL);
    
    vTaskStartScheduler();

    return 0;
}

void initPLL(void)
{
    PLLFBD = 41;            // M = 43 FRC
    CLKDIVbits.PLLPOST = 0; // N1 = 2
    CLKDIVbits.PLLPRE = 0;  // N2 = 2

    __builtin_write_OSCCONH(0x01);  // FRC
    __builtin_write_OSCCONL(0x01);

    while (OSCCONbits.COSC != 0b001);   // FRC
    while(OSCCONbits.LOCK != 1) {};
}

void initPWM(void)
{
	T2CONbits.TON = 0;      
    T2CONbits.TCKPS = 0b10;
    PR2 = 12500;            
   
    OC1CONbits.OCM = 0b110;
    OC1CONbits.OCTSEL = 0;  
   
    OC1RS = PWM_CENTER;    
    OC1R = PWM_CENTER;
   
    T2CONbits.TON = 1;   	
}

static void prvSetupHardware( void )
{
    ADPCFG = 0xFFFF;
    
    

    vParTestInitialise();
    initPLL();
    CNPU1 |= 0x0040; 
    output_float(); 
    ONE_WIRE_PIN = 1;// 1-Wire Idle
    LCD_init();
    LCD_line(1);
    LCD_printf("Temp:       C");     
    LCD_line(2);
    LCD_printf("Mod: AUTO ");     
	LCD_line(3);                    
    LCD_printf("Tens:       ");
	LCD_line(4); 
	LCD_printf("Ult.Com:   ");      
	
	TRISBbits.TRISB10 = 0;
    TRISBbits.TRISB11 = 0; 
    _RB11 = 0;            
	RPOR5bits.RP10R = 18; 
    
	TRISBbits.TRISB3 = 1; // RB3 Input pentru ADC
    TRISBbits.TRISB7 = 1;  // Buton Input
    IPC0bits.INT0IP = 4;   // Prioritate
    IFS0bits.INT0IF = 0;   // Stergere flag
    IEC0bits.INT0IE = 1;   // Activare întrerupere

    xSerialPortInitMinimal( mainCOM_TEST_BAUD_RATE, comBUFFER_LEN );

    initAdc1();
	initPWM();
    initTmr3();
}

void vApplicationIdleHook( void )
{
    vCoRoutineSchedule();
}