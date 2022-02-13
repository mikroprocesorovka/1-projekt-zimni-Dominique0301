#include "stm8s.h"
#include "assert.h"
#include "delay.h"
#include "milis.h"
#include "stdio.h"
#include "spse_stm8.h"

//bzu��k
#define BZ_PORT GPIOB
#define BZ_PIN GPIO_PIN_0


//Ultrasonic
#define TI1_PORT GPIOD//nelze p�ipojit kamkoliv!!!
#define TI1_PIN  GPIO_PIN_4

#define TRGG_PORT GPIOC
#define TRGG_PIN  GPIO_PIN_7
#define TRGG_ON   GPIO_WriteHigh(TRGG_PORT, TRGG_PIN);
#define TRGG_OFF  GPIO_WriteLow(TRGG_PORT, TRGG_PIN);
#define TRGG_REVERSE GPIO_WriteReverse(TRGG_PORT, TRGG_PIN);

#define MEASURMENT_PERON 444    // perioda m��en� (ms). Doba po kterou �ek�m a pokud se nic nestane tak je m��en� nedom��en�


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
#define DECODE_MODE 9	// Aktivace/Deaktivace znakov� sady (my vol�me v?dy hodnotu DECODE_ALL)
#define INTENSITY 	10	// Nastaven� jasu - argument je c�slo 0 a� 15 (vet�� c�slo vet�� jas)
#define SCAN_LIMIT 	11	// Volba poctu cifer (velikosti displeje) 
#define SHUTDOWN 	12	// Aktivace/Deaktivace displeje (ON / OFF)
#define DISPLAY_TEST 	15	// Aktivace/Deaktivace "testu" (rozsv�t� v�echny segmenty)

// makra argumentu
// argumenty pro SHUTDOWN
#define DISPLAY_ON		1	// zapne displej
#define DISPLAY_OFF		0	// vypne displej
// argumenty pro DISPLAY_TEST
#define DISPLAY_TEST_ON 	1	// zapne test displeje
#define DISPLAY_TEST_OFF 	0	// vypne test displeje
// argumenty pro DECODE_MOD
#define DECODE_ALL		0b11111111 // zap�na znakovou sadu pro vsechny cifry
#define DECODE_NONE		0 // vyp�n� znakovou sadu pro vsechny cifry





//UART komunikace
char putchar (char c)
{
  /* Write a character to the UART1 */
  UART1_SendData8(c);
  /* Loop until the end of transmission */
  while (UART1_GetFlagStatus(UART1_FLAG_TXE) == RESET);

  return (c);
}

char getchar (void) //�te vstup z UART
{

  int c = 0;
  /* Loop until the Read data register flag is SET */
  while (UART1_GetFlagStatus(UART1_FLAG_RXNE) == RESET);
	c = UART1_ReceiveData8();
  return (c);
}



//Povoleni UART1 (Vyuzivane na komunikaci s PC)
void init_uart(void)
{
    UART1_DeInit();         // smazat starou konfiguraci
		UART1_Init((uint32_t)115200, //Nova konfigurace
									UART1_WORDLENGTH_8D, 
									UART1_STOPBITS_1, 
									UART1_PARITY_NO,
									UART1_SYNCMODE_CLOCK_DISABLE, 
									UART1_MODE_TXRX_ENABLE);
    //UART1_Cmd(ENABLE);  // povol� UART1

    //UART1_ITConfig(UART1_IT_RXNE_OR, ENABLE);   // povol� p�eru�en� UART1 Rx

}






void max7219(uint8_t address, uint8_t data)
{
uint16_t mask;
CLR(CS);//za��n�m vys�lat, Chip Select(CS) d�m do 0(CLEAR)

//testuji jestli nejvy��� bit je 1 nebo 0


//adresa
mask = 1<<7;//posunut� 1 o 7 bitu do leva
while(mask){
 CLR(CLK);
 if(address & mask){//logick� sou�in adresy a masky
       SET(DIN);//kdy� je nejvy��� bit 1 zavol�m SET
		} else {
			 CLR(DIN);//kdy� je nejvy��� bit 0 zavol�m CLR
	  }
		SET(CLK);//tik�n� nahoru
		mask >>=1;//posunut� masky o 1 bit do prava
		CLR(CLK);//tik�n� dol�
	}
	
	
//data
mask = 1<<7;
while(mask){
 CLR(CLK);
 if(data & mask){//logick� sou�in dat a masky
       SET(DIN);
		} else {
			 CLR(DIN);
	  }
		SET(CLK);
		mask >>=1;
		CLR(CLK);
	}
	
	SET(CS);//kon�� vys�l�n� chip select d�m do 1
}




void init(void){
	//Inicializace maxu7219
  GPIO_Init(CLK_PORT, CLK_PIN, GPIO_MODE_OUT_PP_LOW_SLOW);
	GPIO_Init(CS_PORT, CS_PIN, GPIO_MODE_OUT_PP_LOW_SLOW);
	GPIO_Init(DIN_PORT, DIN_PIN, GPIO_MODE_OUT_PP_LOW_SLOW);
	GPIO_Init (BZ_PORT, BZ_PIN, GPIO_MODE_OUT_PP_LOW_SLOW);//buzzer
	max7219(DECODE_MODE, DECODE_ALL); // zapnout znakovou sadu na vsech cifrach
  max7219(SCAN_LIMIT, 7); // velikost displeje 8 cifer (poc�tano od nuly, proto je argument cislo 7)
  max7219(INTENSITY, 3); 
  max7219(DISPLAY_TEST, DISPLAY_TEST_OFF); // Funkci "test" nechceme m?t zapnutou
  max7219(SHUTDOWN, DISPLAY_ON); // zapneme displej
	
	CLK_HSIPrescalerConfig(CLK_PRESCALER_HSIDIV1); // taktovat MCU na 16MHz
	
  
	init_milis(); 
	init_uart(); //Povoleni komunikace s PC
	
	
	

	//Inicializace ultrasonicu
    //Trigger setup
    GPIO_Init(TRGG_PORT, TRGG_PIN, GPIO_MODE_OUT_PP_LOW_SLOW); //re�im push pull
		
		//TIM2 setup
    GPIO_Init(TI1_PORT, TI1_PIN, GPIO_MODE_IN_FL_NO_IT);  // kan�l 1 jako vstupn� kan�l
		// pot�ebuji nastavit p�edd�li�ku a strop �asova�e

    TIM2_TimeBaseInit(TIM2_PRESCALER_16, 0xFFFF );//16 bitov�, nechci ho omezovat >strop nastav�m na ffff, 0x>hex soustava ffff=65535
		//Prescaler d�l� frekvenci z master. M�m 16MHz a d�l�m 16 tak�e m�m 1 MHz.Perioda 1 mikro sek.
		//��ta� je 16 bitov�, ka�dou mikro sekundu p�ijde impuls.Tak�e napo��ta max 65536 mikro sekund. 
		
		
    /*TIM2_ITConfig(TIM2_IT_UPDATE, ENABLE);*/
    TIM2_Cmd(ENABLE);
    TIM2_ICInit(TIM2_CHANNEL_1,        // nastavuji CH1 (CaptureRegistr1)
            TIM2_ICPOLARITY_RISING,    // zachycen� se spou�t� pomoc� nab�n� hrany
            TIM2_ICSELECTION_DIRECTTI, // CaptureRegistr1 bude ovl�d�n p��mo (DIRECT)z CH1
            TIM2_ICPSC_DIV1,           // delicka je vypnuta
            0                          // vstupn� filter je vypnuty
        );            
    TIM2_ICInit(TIM2_CHANNEL_2,        // nastavuji CH2 (CaptureRegistr2)
            TIM2_ICPOLARITY_FALLING,   //zachyt�vat se  sestupnou hranou
            TIM2_ICSELECTION_INDIRECTTI, // CaptureRegistr2 bude ovl�d�n  (nep��mo) z CH1
            TIM2_ICPSC_DIV1,           // delicka je vypnuta
            0                          // vstupni filter je vypnuty
        );            
}


//Na maxu budou v�ude nuly
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





typedef enum //datov� typ Enum pro stavy snimace vzdalenosti
{//definuju si stavy
    TRGG_START,       // zah�jen� trigger impulzu
    TRGG_WAIT,        // cek�n� na konec trrigger impulzu
    MEASURMENT_WAIT   // �ek�n� na dokon�en� me�en�
} STATE_TypeDef;


void main(void)
{
    uint32_t mtime_ultrasonic = 0;
    uint32_t delka;
    STATE_TypeDef state = TRGG_START;//��k� mi v jak�m stavu se nach�z�. Nejd�iv ud�l�m trigger impuls a budu �ekat ne� uplyne doba trrg impulsu.

		
    init();
		nuly();
   




    while (1) {//nekone�n� smy�ka  co b�ha dokola
        switch (state) { //Stav snimace
        case TRGG_START:
            if (milis() - mtime_ultrasonic > MEASURMENT_PERON) {// kontroluji zda nep�etekla doba m��en�
                mtime_ultrasonic = milis();// pokud ano zah�j� nov� m��en�
                TRGG_ON;//zah�j�m m��en�
                state = TRGG_WAIT;//jakmile zjist�m, �e jsem nastavil impuls, tak p�ep�n�m do stavu trigger wait.
            }
            break;
						
        case TRGG_WAIT:
            if (milis() - mtime_ultrasonic > 1) {//pokud jsem ve stavu trigger wait a u� uplynula 1ms
                TRGG_OFF;//Tak to vypnu a ud�l�m sestupnou hranu.
                // sma�u v�echny vlajky
                TIM2_ClearFlag(TIM2_FLAG_CC1);
                TIM2_ClearFlag(TIM2_FLAG_CC2); 
                TIM2_ClearFlag(TIM2_FLAG_CC1OF); 
                TIM2_ClearFlag(TIM2_FLAG_CC2OF); 
                state = MEASURMENT_WAIT;
            }
            break;
						
        case MEASURMENT_WAIT:
             /* detekuji sestupnou hranu ECHO sign�lu; vzestupnou hranu 
              * detekovat nemus�m, zachyceni CC1 i CC2 probehne automaticky  */
            if (TIM2_GetFlagStatus(TIM2_FLAG_CC2) == RESET) {//byla u� nastaven� vlajka ???
                TIM2_ClearFlag(TIM2_FLAG_CC1);  // sma�u vlajku CC1
                TIM2_ClearFlag(TIM2_FLAG_CC2);  // sma�u vlajku CC2

                // d�lka impulzu  
                delka = (TIM2_GetCapture2() - TIM2_GetCapture1()); 

								//Vypocet a vypsani vzdalenosti na PC a displej
                delka = (delka * 340)/ 20000; // FixPoint prepocet na cm 
                printf("Vzdalenost: %lu cm\r\n", delka);
						
								max7219(1,delka-(delka/10*10));//jednotky
								max7219(2,delka/10); //Des�tky
								max7219(3,delka/100); //Stovky
								
								if (delka > 10){
									GPIO_WriteHigh(BZ_PORT,BZ_PIN);
								}
			
								
							


                state = TRGG_START;
            }
            break;
        default:
            state = TRGG_START;
        }
				
    }
		
		
}










void assert_failed(uint8_t* file, uint32_t line)// stvd h�zelo n�jakou chybu s assert a tohle pomohlo
 

{
 

  /* User can add his own implementation to report the file name and line number,
 

     ex: printf(''Wrong parameters value: file %s on line %d\r\n'', file, line) */
 

 

  /* Infinite loop */
 

  while (1)
 

  {
 

  }
 

}
