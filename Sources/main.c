#include <STC15F2K60S2.H>

#define DEBUG_PIN P33
#define HX711_DOUT P34  
#define HX711_SCK  P35
#define AD7524_WR P36
#define AD7524_CS P37
#define AD7524_DAT P1

void Delay1us(void)	//@11.0592MHz
{
	_nop_();
	_nop_();
	_nop_();
}

void Delay50ms(void)	//@11.0592MHz
{
	unsigned char data i, j, k;
	_nop_();
	_nop_();
	i = 3;
	j = 26;
	k = 223;
	do {
		do {
			while (--k);
		} while (--j);
	} while (--i);
}

void GPIO_Init() {
	// P34: Bidirection
	// P35-P37: Push-Pull
	P3M0 = (P3M0 & ~0x10) | 0xE0; P3M1 &= ~0xF0;
	
	// P10-P17: Push-Pull
	P1M0 = 0xFF; P1M1 = 0x00;
	
	// P33: Push-Pull
	P3M0 |= 0x08; P3M1 &= ~0x08;
}


void UART_Init(void)//波特率9600
{
	SCON = 0x50;		//8位数据,可变波特率
	AUXR |= 0x01;		//串口1选择定时器2为波特率发生器
	AUXR |= 0x04;		//定时器时钟1T模式
	T2L = 0xE0;			//设置定时初始值
	T2H = 0xFE;			//设置定时初始值
	AUXR |= 0x10;		//定时器2开始计时
}

void UART_SendByte(unsigned char byte)
{
  SBUF=byte;
	while(TI==0);
	TI=0;
}

void UART_SendStr(unsigned char *str)
{
	while (*str) {
		UART_SendByte(*str++);
	}
}

void AD7524_Init() {
	AD7524_CS = 0;  // always use cs pin for ad7524
	AD7524_WR = 1;
	_nop_();
}

void AD7524_WriteDat(unsigned char dat) {
	AD7524_DAT = dat;
	AD7524_WR = 0;
	_nop_();
	AD7524_WR = 1;
}

unsigned long HX711_Read(void) {
	unsigned long count; 
	unsigned char i; 
	HX711_DOUT = 1; // free pin and wait for pull-down
	Delay1us();
	HX711_SCK = 0; 
	count = 0; 
	while (HX711_DOUT); 
	for (i = 0; i < 24; i++) { 
		HX711_SCK = 1; 
		count <<= 1; 
		HX711_SCK = 0; 
		if (HX711_DOUT) count++; 
	}
	HX711_SCK = 1;
	count ^= 0x800000;
	Delay1us();
	HX711_SCK = 0;
	return count;	
}

void main(void) {
	static unsigned long weight;
	volatile char pinState;
	GPIO_Init();
	UART_Init();
	UART_SendStr("Init complete\r\n");
	while (1) {
		Delay50ms();
		pinState = DEBUG_PIN;
		DEBUG_PIN = !pinState;
	}
}