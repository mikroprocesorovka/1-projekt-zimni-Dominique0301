#include "stm8s.h"
#include "assert.h"
#include "delay.h"
#include "milis.h"
#include "stdio.h"
#include "spse_stm8.h"

//bzuèák
#define BZ_PORT GPIOB
#define BZ_PIN GPIO_PIN_0


//Ultrasonic
#define TI1_PORT GPIOD
#define TI1_PIN  GPIO_PIN_4

#define TRGG_PORT GPIOC
#define TRGG_PIN  GPIO_PIN_7
#define TRGG_ON   GPIO_WriteHigh(TRGG_PORT, TRGG_PIN);
#define TRGG_OFF  GPIO_WriteLow(TRGG_PORT, TRGG_PIN);
#define TRGG_REVERSE GPIO_WriteReverse(TRGG_PORT, TRGG_PIN);

#define MASURMENT_PERON 444    // maximalni celkovy cas mereni (ms)


//Max7219
#define CLK_PORT GPIOG
#define CLK_PIN GPIO_PIN_2

#define CS_PORT GPIOG
#define CS_PIN GPIO_PIN_3

#define DIN_PORT GPIOE
#define DIN_PIN GPIO_PIN_3


#define SET(BAGR) GPIO_WriteHigh(BAGR##_PORT, BAGR##_PIN)
#define CLR(BAGR) GPIO_WriteLow(BAGR##_PORT, BAGR##_PIN)


#define NOOP 		  0  	
#define DIGIT0 		1	
#define DIGIT1 		2	
#define DIGIT2 		3	
#define DIGIT3 		4	
#define DIGIT4 		5	
#define DIGIT5 		6	
#define DIGIT6 		7	
#define DIGIT7 		8	
#define DECODE_MODE 9	// Aktivace/Deaktivace znakové sady (my volíme v?dy hodnotu DECODE_ALL)
#define INTENSITY 	10	// Nastavení jasu - argument je císlo 0 až 15 (vetší císlo vetší jas)
#define SCAN_LIMIT 	11	// Volba poctu cifer (velikosti displeje) - argument je c?slo 0 a? 7 (my d?v?me v?dy 7)
#define SHUTDOWN 	12	// Aktivace/Deaktivace displeje (ON / OFF)
#define DISPLAY_TEST 	15	// Aktivace/Deaktivace "testu" (rozsv?t? v?echny segmenty)

// makra argumentu
// argumenty pro SHUTDOWN
#define DISPLAY_ON		1	// zapne displej
#define DISPLAY_OFF		0	// vypne displej
// argumenty pro DISPLAY_TEST
#define DISPLAY_TEST_ON 	1	// zapne test displeje
#define DISPLAY_TEST_OFF 	0	// vypne test displeje
// argumenty pro DECODE_MOD
#define DECODE_ALL		0b11111111 // zapína znakovou sadu pro vsechny cifry
#define DECODE_NONE		0 // vypíná znakovou sadu pro vsechny cifry





//UART komunikace
char putchar (char c)
{
  /* Write a character to the UART1 */
  UART1_SendData8(c);
  /* Loop until the end of transmission */
  while (UART1_GetFlagStatus(UART1_FLAG_TXE) == RESET);

  return (c);
}

char getchar (void) //funkce cte(prijma data) vstup z UART
{

  int c = 0;
  /* Loop until the Read data register flag is SET */
  while (UART1_GetFlagStatus(UART1_FLAG_RXNE) == RESET);
	c = UART1_ReceiveData8();
  return (c);
}



//Povoleni UART1 (Vyuzivane na komunikaci s PC)
void init_uart1(void)
{
    UART1_DeInit();         // smazat starou konfiguraci
		UART1_Init((uint32_t)115200, //Nova konfigurace
									UART1_WORDLENGTH_8D, 
									UART1_STOPBITS_1, 
									UART1_PARITY_NO,
									UART1_SYNCMODE_CLOCK_DISABLE, 
									UART1_MODE_TXRX_ENABLE);
    //UART1_Cmd(ENABLE);  // povolí UART1

    //UART1_ITConfig(UART1_IT_RXNE_OR, ENABLE);   // povolí pøerušení UART1 Rx

}






void max7219(uint8_t address, uint8_t data)
{
uint16_t mask;
CLR(CS);//zaèínám vysílat, Chip Select(CS) dám do 0(CLEAR)

//testuji jestli nejvyšší bit je 1 nebo 0


//adresa
mask = 1<<7;//posunutí 1 o 7 bitu do leva
while(mask){
 CLR(CLK);
 if(address & mask){//logický souèin adresy a masky
       SET(DIN);//když je nejvyšší bit 1 zavolám SET
		} else {
			 CLR(DIN);//když je nejvyšší bit 0 zavolám CLR
	  }
		SET(CLK);//tikání nahoru
		mask >>=1;//posunutí masky o 1 bit do prava
		CLR(CLK);//tikání dolù
	}
	
	
//data
mask = 1<<7;
while(mask){
 CLR(CLK);
 if(data & mask){//logický souèin dat a masky
       SET(DIN);
		} else {
			 CLR(DIN);
	  }
		SET(CLK);
		mask >>=1;
		CLR(CLK);
	}
	
	SET(CS);//konèí vysílaní, chip select dám do 1
}




void init(void){
	//Inicializace maxu7219
  GPIO_Init(CLK_PORT, CLK_PIN, GPIO_MODE_OUT_PP_LOW_SLOW);
	GPIO_Init(CS_PORT, CS_PIN, GPIO_MODE_OUT_PP_LOW_SLOW);
	GPIO_Init(DIN_PORT, DIN_PIN, GPIO_MODE_OUT_PP_LOW_SLOW);
	max7219(DECODE_MODE, DECODE_ALL); // zapnout znakovou sadu na vsech cifrach
  max7219(SCAN_LIMIT, 7); // velikost displeje 8 cifer (pocítano od nuly, proto je argument cislo 7)
  max7219(INTENSITY, 3); 
  max7219(DISPLAY_TEST, DISPLAY_TEST_OFF); // Funkci "test" nechceme m?t zapnutou
  max7219(SHUTDOWN, DISPLAY_ON); // zapneme displej
	
	CLK_HSIPrescalerConfig(CLK_PRESCALER_HSIDIV1); // taktovat MCU na 16MHz
	
  init_milis(); 

	
	
	init_uart1(); //Povoleni komunikace s PC
	
	
	

	//Inicializace ultrasonicu
    /*----          trigger setup           ---------*/
    GPIO_Init(TRGG_PORT, TRGG_PIN, GPIO_MODE_OUT_PP_LOW_SLOW);

    /*----           TIM2 setup           ---------*/
    GPIO_Init(TI1_PORT, TI1_PIN, GPIO_MODE_IN_FL_NO_IT);  // kanál 1 jako vstup

    TIM2_TimeBaseInit(TIM2_PRESCALER_16, 0xFFFF );
    /*TIM2_ITConfig(TIM2_IT_UPDATE, ENABLE);*/
    TIM2_Cmd(ENABLE);
    TIM2_ICInit(TIM2_CHANNEL_1,        // nastavuji CH1 (CaptureRegistr1)
            TIM2_ICPOLARITY_RISING,    // nabìžná hrana
            TIM2_ICSELECTION_DIRECTTI, // CaptureRegistr1 bude ovládán z CH1
            TIM2_ICPSC_DIV1,           // delicka je vypnuta
            0                          // vstupní filter je vypnuty
        );            
    TIM2_ICInit(TIM2_CHANNEL_2,        // nastavuji CH2 (CaptureRegistr2)
            TIM2_ICPOLARITY_FALLING,   // sestupna hrana
            TIM2_ICSELECTION_INDIRECTTI, // CaptureRegistr2 bude ovl?d?n z CH1
            TIM2_ICPSC_DIV1,           // delicka je vypnuta
            0                          // vstupni filter je vypnuty
        );            
}



void nuly(void)
{
	max7219(1,0);
	max7219(2,0);
	max7219(3,0);
	max7219(4,0);
	max7219(5,0);
	max7219(6,0);
	max7219(7,0);
	max7219(8,0);
}





typedef enum //Enum pro stavy snimace vzdalenosti
{
    TRGG_START,       // zahájení trigger impulzu
    TRGG_WAIT,        // cekání na konec trrigger impulzu
    MEASURMENT_WAIT   // èekání na dokonèení meøení
} STATE_TypeDef;


void main(void)
{
    uint32_t mtime_ultrasonic = 0;
    uint32_t diff;
    STATE_TypeDef state = TRGG_START;

		
    init();
		nuly();
    printf("Start programu\r\n");




    while (1) {
        switch (state) { //Stav snimace
        case TRGG_START:
            if (milis() - mtime_ultrasonic > MASURMENT_PERON) {
                mtime_ultrasonic = milis();
                TRGG_ON;
                state = TRGG_WAIT;
            }
            break;
        case TRGG_WAIT:
            if (milis() - mtime_ultrasonic > 1) {
                TRGG_OFF;
                // smažu všechny vlajky
                TIM2_ClearFlag(TIM2_FLAG_CC1);
                TIM2_ClearFlag(TIM2_FLAG_CC2); 
                TIM2_ClearFlag(TIM2_FLAG_CC1OF); 
                TIM2_ClearFlag(TIM2_FLAG_CC2OF); 
                state = MEASURMENT_WAIT;
            }
            break;
        case MEASURMENT_WAIT:
             /* detekuji sestupnou hranu ECHO signálu; vzestupnou hranu 
              * detekovat nemusím, zachycen? CC1 i CC2 probehne automaticky  */
            if (TIM2_GetFlagStatus(TIM2_FLAG_CC2) == RESET) {
                TIM2_ClearFlag(TIM2_FLAG_CC1);  // sma?u vlajku CC1
                TIM2_ClearFlag(TIM2_FLAG_CC2);  // sma?u vlajku CC2

                // délka impulzu  
                diff = (TIM2_GetCapture2() - TIM2_GetCapture1()); 

								//Vypocet a vypsani vzdalenosti na PC a displej
                //diff = (diff * 340 + 10000)/ 20000; // FixPoint prepocet na cm -- korektne zaokrouhluje 
                diff = (diff * 340)/ 20000; // FixPoint prepocet na cm -- zaokrouhluje v?dy dolu
                printf("Vzdalenost: %lu cm\r\n", diff);
						
								max7219(1,diff-(diff/10*10));//jednotky
								max7219(2,diff/10); //Desítky
								max7219(3,diff/100); //Stovky


                state = TRGG_START;
            }
            break;
        default:
            state = TRGG_START;
        }
				
    }
}










void assert_failed(uint8_t* file, uint32_t line)
 

{
 

  /* User can add his own implementation to report the file name and line number,
 

     ex: printf(''Wrong parameters value: file %s on line %d\r\n'', file, line) */
 

 

  /* Infinite loop */
 

  while (1)
 

  {
 

  }
 

}
