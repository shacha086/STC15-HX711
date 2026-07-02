#include <STC15F2K60S2.H>

#define BUTTON P32
#define DEBUG_PIN P33
#define HX711_DOUT P34  
#define HX711_SCK  P35
#define AD7524_WR P36
#define AD7524_CS P37
#define AD7524_DAT P1

#define VREF 5000UL

#define IAP_ADDRESS 0x0400
#define CMD_IDLE    0
#define CMD_READ    1
#define CMD_PROGRAM 2
#define CMD_ERASE   3
#define ENABLE_IAP  0x82

unsigned char code sineTable[64] = {
128,140,152,164,176,187,198,208,217,226,233,240,245,249,252,254,
255,254,252,249,245,240,233,226,217,208,198,187,176,164,152,140,
128,115,103, 91, 79, 68, 57, 47, 38, 29, 22, 15, 10,  6,  3,  1,
  1,  1,  3,  6, 10, 15, 22, 29, 38, 47, 57, 68, 79, 91,103,115
};

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
	// P32: Pull-up
	BUTTON = 1;
	// P32: falling-edge interrupting
	IT0 = 1;			//INT0(P3.2)下降沿中断
	EX0 = 1;			//使能INT0中断
	EA  = 1;
	// P33: Push-Pull (test use)
	P3M0 |= 0x08; P3M1 &= ~0x08;
	
}

void UART_Init(void)
{
	SCON = 0x50;			// 8-bit UART, variable baud rate, enable receive
	AUXR |= 0x01;			// serial 1 use timer 2 as baud rate generator
	AUXR |= 0x04;			// timer 2 1T mode
	T2L = 0xE0;				// baud rate 9600 @ 11.0592MHz
	T2H = 0xFE;
	AUXR |= 0x10;			// start timer 2
}

void UART_SendByte(unsigned char byte)
{
	SBUF = byte;
	while (TI == 0);
	TI = 0;
}

void UART_SendStr(unsigned char *str)
{
	while (*str) {
		UART_SendByte(*str++);
	}
}

void UART_SendUlong(unsigned long n)
{
	unsigned char buf[11];
	unsigned char i = 10;
	buf[10] = '\0';
	do {
		buf[--i] = '0' + (unsigned char)(n % 10);
		n /= 10;
	} while (n);
	UART_SendStr(&buf[i]);
}

void UART_SendLong(long n)
{
	unsigned char buf[12];
	unsigned char i = 11;
	buf[11] = '\0';
	if (n < 0) {
		n = -n;
		do {
			buf[--i] = '0' + (unsigned char)(n % 10);
			n /= 10;
		} while (n);
		buf[--i] = '-';
	} else {
		do {
			buf[--i] = '0' + (unsigned char)(n % 10);
			n /= 10;
		} while (n);
	}
	UART_SendStr(&buf[i]);
}

void UART_SendUint(unsigned int n)
{
	unsigned char buf[6];
	unsigned char i = 5;
	buf[5] = '\0';
	do {
		buf[--i] = '0' + (unsigned char)(n % 10);
		n /= 10;
	} while (n);
	UART_SendStr(&buf[i]);
}

void AD7524_Init() {
	AD7524_CS = 0;
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
	HX711_DOUT = 1;
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

void IapIdle()
{
	IAP_CONTR = 0;
	IAP_CMD = 0;
	IAP_TRIG = 0;
	IAP_ADDRH = 0x80;
	IAP_ADDRL = 0;
}

unsigned char IapReadByte(unsigned int addr)
{
	unsigned char dat;
	IAP_CONTR = ENABLE_IAP;
	IAP_CMD = CMD_READ;
	IAP_ADDRL = addr;
	IAP_ADDRH = addr >> 8;
	IAP_TRIG = 0x5A;
	IAP_TRIG = 0xA5;
	_nop_();
	dat = IAP_DATA;
	IapIdle();
	return dat;
}

void IapProgramByte(unsigned int addr, unsigned char dat)
{
	IAP_CONTR = ENABLE_IAP;
	IAP_CMD = CMD_PROGRAM;
	IAP_ADDRL = addr;
	IAP_ADDRH = addr >> 8;
	IAP_DATA = dat;
	IAP_TRIG = 0x5A;
	IAP_TRIG = 0xA5;
	_nop_();
	IapIdle();
}

void IapEraseSector(unsigned int addr)
{
	IAP_CONTR = ENABLE_IAP;
	IAP_CMD = CMD_ERASE;
	IAP_ADDRL = addr;
	IAP_ADDRH = addr >> 8;
	IAP_TRIG = 0x5A;
	IAP_TRIG = 0xA5;
	_nop_();
	IapIdle();
}

unsigned long EEPROM_ReadLong(unsigned int addr)
{
	unsigned long val;
	val  = (unsigned long)IapReadByte(addr) << 24;
	val |= (unsigned long)IapReadByte(addr + 1) << 16;
	val |= (unsigned long)IapReadByte(addr + 2) << 8;
	val |= (unsigned long)IapReadByte(addr + 3);
	return val;
}

void EEPROM_WriteLong(unsigned int addr, unsigned long val)
{
	IapProgramByte(addr,     (unsigned char)(val >> 24));
	IapProgramByte(addr + 1, (unsigned char)(val >> 16));
	IapProgramByte(addr + 2, (unsigned char)(val >> 8));
	IapProgramByte(addr + 3, (unsigned char)(val));
}

unsigned char EEPROM_IsValid(unsigned int addr)
{
	unsigned char i;
	for (i = 0; i < 4; i++) {
		if (IapReadByte(addr + i) != 0xFF)
			return 1;
	}
	return 0;
}

volatile char button_pressed = 0;

void main(void) {
	unsigned long hx711_val;
	unsigned long zero_offset = 0;
	long diff;
	unsigned char sineIdx = 0;
	unsigned char dac_val;
	unsigned int dac_mv;
	unsigned char uart_cmd;

	GPIO_Init();
	UART_Init();
	AD7524_Init();

	// Load zero offset from EEPROM
	if (EEPROM_IsValid(IAP_ADDRESS)) {
		zero_offset = EEPROM_ReadLong(IAP_ADDRESS);
	}

	UART_SendStr("Init complete\r\n");
	UART_SendStr("Zero: ");
	UART_SendUlong(zero_offset);
	UART_SendStr("\r\n");

	while (1) {
		hx711_val = HX711_Read();

		// UART command
		if (RI) {
			uart_cmd = SBUF;
			RI = 0;
			if (uart_cmd == 'Z' || uart_cmd == 'z') {
				zero_offset = hx711_val;
				IapEraseSector(IAP_ADDRESS);
				EEPROM_WriteLong(IAP_ADDRESS, zero_offset);
				UART_SendStr(">>> TARE: zero=");
				UART_SendUlong(zero_offset);
				UART_SendStr("\r\n");
			}
		}

		// DAC sine wave
		dac_val = sineTable[sineIdx];
		sineIdx = (sineIdx + 1) & 0x3F;
		AD7524_WriteDat(dac_val);
		dac_mv = (unsigned int)((unsigned long)dac_val * VREF / 256);

		// Button
		if (button_pressed) {
			button_pressed = 0;
			UART_SendStr("Button pressed!");
		}
		
		DEBUG_PIN = ~DEBUG_PIN;
		Delay50ms();
	}
}

void INT0_Isr(void) interrupt 0
{
	button_pressed = 1;
}