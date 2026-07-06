#include <STC15F2K60S2.H>

#define BUTTON P32
#define DEBUG_PIN P33
#define HX711_DOUT P34  
#define HX711_SCK  P35
#define AD7524_WR P36
#define AD7524_CS P37
#define AD7524_DAT P1

#define VREF 5000UL

#define IAP_ZERO_ADDR   0x0400
#define IAP_SPAN_ADDR   0x0404
#define IAP_DACCAL_ADDR 0x0408
#define DAC_CAL_DEN     256
#define DAC_CAL_NOM     256
#define CMD_IDLE    0
#define CMD_READ    1
#define CMD_PROGRAM 2
#define CMD_ERASE   3
#define ENABLE_IAP  0x82

#define LONG_PRESS_THRESH 20

volatile bit button_pressed = 0;

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
	// P32: Quasi-bidirectional, pull-up for button
	BUTTON = 1;
	IT0 = 1;
	EX0 = 1;
	EA  = 1;
	// P33: Push-Pull (debug)
	P3M0 |= 0x08; P3M1 &= ~0x08;
}

void UART_Init(void)
{
	SCON = 0x50;
	AUXR |= 0x01;
	AUXR |= 0x04;
	T2L = 0xE0;
	T2H = 0xFE;
	AUXR |= 0x10;
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

void main(void) {
	unsigned long hx711_val;
	unsigned long zero_offset = 0;
	unsigned long span = 0;
	unsigned int press_cnt;
	long weight_raw;
	unsigned char dac_val;
	unsigned char dac_write;
	unsigned int dac_mv;
	unsigned int dac_cal = DAC_CAL_NOM;

	GPIO_Init();
	UART_Init();
	AD7524_Init();

	// Load calibration from EEPROM
	if (EEPROM_IsValid(IAP_ZERO_ADDR)) {
		zero_offset = EEPROM_ReadLong(IAP_ZERO_ADDR);
	}
	if (EEPROM_IsValid(IAP_SPAN_ADDR)) {
		span = EEPROM_ReadLong(IAP_SPAN_ADDR);
	}
	dac_cal = ((unsigned int)IapReadByte(IAP_DACCAL_ADDR) << 8)
	        |  (unsigned int)IapReadByte(IAP_DACCAL_ADDR + 1);
	if (dac_cal == 0 || dac_cal == 0xFFFF) dac_cal = DAC_CAL_NOM;

	UART_SendStr("Init complete\r\n");
	UART_SendStr("Zero: "); UART_SendUlong(zero_offset);
	UART_SendStr(" Span: "); UART_SendUlong(span);
	UART_SendStr(" Cal: "); UART_SendUint(dac_cal);
	UART_SendStr("\r\n");

	while (1) {
		hx711_val = HX711_Read();
		hx711_val &= ~7UL;              // Discard lower 3 bits for stability

		// Button: short press = tare, long press = span
		if (button_pressed) {
			button_pressed = 0;
			EX0 = 0;
			Delay50ms();                    // Press debounce
			if (BUTTON == 0) {              // Valid press
				press_cnt = 0;
				while (BUTTON == 0) {       // Measure hold duration
					Delay50ms();
					press_cnt++;
				}
				Delay50ms();                // Release debounce

				if (press_cnt < LONG_PRESS_THRESH) {
					// Short press: tare (0g calibration)
					zero_offset = hx711_val;
					IapEraseSector(IAP_ZERO_ADDR);
					EEPROM_WriteLong(IAP_ZERO_ADDR, zero_offset);
					if (span > 0) {
						EEPROM_WriteLong(IAP_SPAN_ADDR, span);
					}
					IapProgramByte(IAP_DACCAL_ADDR,     (unsigned char)(dac_cal >> 8));
					IapProgramByte(IAP_DACCAL_ADDR + 1, (unsigned char)(dac_cal));
					UART_SendStr(">>> TARE: zero=");
					UART_SendUlong(zero_offset);
					UART_SendStr("\r\n");
				} else {
					// Long press: span calibration (100g -> 5V)
					if (hx711_val > zero_offset) {
						span = hx711_val - zero_offset;
						IapEraseSector(IAP_ZERO_ADDR);
						EEPROM_WriteLong(IAP_ZERO_ADDR, zero_offset);
						EEPROM_WriteLong(IAP_SPAN_ADDR, span);
						IapProgramByte(IAP_DACCAL_ADDR,     (unsigned char)(dac_cal >> 8));
						IapProgramByte(IAP_DACCAL_ADDR + 1, (unsigned char)(dac_cal));
						UART_SendStr(">>> SPAN: hx711=");
						UART_SendUlong(hx711_val);
						UART_SendStr(" span=");
						UART_SendUlong(span);
						UART_SendStr("\r\n");
					} else {
						UART_SendStr(">>> SPAN ERR: reading <= zero, tare first\r\n");
					}
				}
			}
			EX0 = 1;
		}

		// Weight to DAC (0g -> 0V, 100g -> 5V)
		if (span > 0 && hx711_val > zero_offset) {
			weight_raw = (long)(hx711_val - zero_offset);
			dac_val = (unsigned char)(((unsigned long)weight_raw * 255) / span);
			if (dac_val > 255) dac_val = 255;
		} else {
			dac_val = 0;
		}
		// Apply DAC gain calibration: dac_cal = 256 / G_actual * 256
		dac_write = (unsigned char)(((unsigned long)dac_val * dac_cal) / DAC_CAL_DEN);
		if (dac_write > 255) dac_write = 255;
		AD7524_WriteDat(dac_write);
		dac_mv = (unsigned int)((unsigned long)dac_val * VREF / 256);

		// Debug output
		UART_SendStr("HX711:"); UART_SendUlong(hx711_val);
		UART_SendStr(" val:"); UART_SendUint(dac_val);
		UART_SendStr(" wr:"); UART_SendUint(dac_write);
		UART_SendStr(" DAC:"); UART_SendUint(dac_mv);
		UART_SendStr("mV\r\n");

		DEBUG_PIN = ~DEBUG_PIN;
		Delay50ms();
	}
}

void INT0_Isr(void) interrupt 0
{
	button_pressed = 1;
}