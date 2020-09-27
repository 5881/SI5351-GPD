/*
 * тест библиотеки i2c oled дисплея
 * /

/**********************************************************************
 * Секция include и defines
**********************************************************************/
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/adc.h>
#include "si5351.h"
#include "oledi2c.h"
#include "oled_printf.h"
#define MIN_LIMIT 200
#define MAX_LIMIT 170000
#define IF_FEQ 455
/*
11 метров, 25.60 — 26.10 МГц (11,72 — 11,49 метра).
13 метров, 21.45 — 21.85 МГц (13,99 — 13,73 метра).
15 метров, 18.90 — 19.02 МГц (15,87 — 15,77 метра).
16 метров, 17.55 — 18.05 МГц (17,16 — 16,76 метра).
19 метров, 15.10 — 15.60 МГц (19,87 — 18,87 метра).
22 метра, 13.50 — 13.87 МГц (22,22 — 21,63 метра).
25 метров 11.60 — 12.10 МГц (25,86 — 24,79 метра).
31 метр, 9.40 — 9.99 МГц (31,91 — 30,03 метра).
41 метр, 7.20 — 7.50 МГц (41,67 — 39,47 метра).
49 метров, 5.85 — 6.35 МГц (52,36 — 47,66 метра).
60 метров, 4.75 — 5.06 МГц (63,16 — 59,29 метра).
75 метров, 3.90 — 4.00 МГц (76,92 — 75 метров).
90 метров, 3.20 — 3.40 МГц (93,75 — 88,24 метров).
120 метров (средние волны), 2.30 — 2.495 МГц (130,43 — 120,24 метра).
*/
uint16_t band[]={5850,7200,9400,11200,13500,15100,17550};
//#include "si5351.h"
uint32_t encoder=7200+IF_FEQ;
uint16_t coef=10;


static void adc_init(void){
	//На PA0 висит LV358 c делителем 1:2 в режиме повторителя
	
	uint8_t chanel=0;
	rcc_periph_clock_enable(RCC_ADC);
	rcc_periph_clock_enable(RCC_GPIOA);
	gpio_mode_setup(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO0);
	adc_power_off(ADC1);
	adc_set_clk_source(ADC1, ADC_CLKSOURCE_ADC);
	adc_calibrate(ADC1);
	//adc_set_continuous_conversion_mode(ADC1);
	adc_set_operation_mode(ADC1, ADC_MODE_SCAN);
	adc_disable_external_trigger_regular(ADC1);
	adc_set_right_aligned(ADC1);
	//adc_enable_temperature_sensor();
	//adc_enable_vbat_sensor();
	adc_set_sample_time_on_all_channels(ADC1, ADC_SMPTIME_071DOT5);
	adc_set_regular_sequence(ADC1, 1, &chanel);
	adc_set_resolution(ADC1, ADC_RESOLUTION_12BIT);
	adc_disable_analog_watchdog(ADC1);
	adc_power_on(ADC1);

	/* Wait for ADC starting up. */
	int i;
	for (i = 0; i < 800000; i++)__asm__("nop");
	
	//adc_start_conversion_regular(ADC1);//start conversion
}

uint16_t measure(){
	//exti_disable_request(EXTI8);
	uint32_t temp=0;
	for(uint16_t i=0;i<4096;i++){
		adc_start_conversion_regular(ADC1);
		while (!(adc_eoc(ADC1)));
		temp+=ADC_DR(ADC1);
		}
	temp=temp>>12;
	temp=temp*2*3290/4095; //пересчёт в миливольты беез претензий на точность
	//exti_enable_request(EXTI8);
	return (uint16_t)temp;
	
}

void next_band(){
	static uint8_t i=1;
	uint64_t freq;
	freq=band[i]+IF_FEQ;//поправка на ПЧ
	encoder=(uint32_t)freq;//надо переписать значение энкодера
	if(i<6) i++; else i=0;
	si5351_set_freq(freq*100000ULL, SI5351_CLK0);
	}


void led_setup(){
	//отладочный светодиод
	rcc_periph_clock_enable(RCC_GPIOA);
	gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT,
					GPIO_PUPD_NONE, GPIO4);
	gpio_set_output_options(GPIOA, GPIO_OTYPE_PP,
                    GPIO_OSPEED_2MHZ, GPIO4);
    //gpio_set(GPIOA,GPIO4);
}


void exti_encoder_init(){
	//Enable GPIOA clock.
	rcc_periph_clock_enable(RCC_GPIOA);
	/*	энкодер и две кнопки с подтяжкой к питанию.
	 * PA8 энкодер 1
	 * PA7 энклдер 2
	 * 
	 */
	 
	gpio_mode_setup(GPIOA, GPIO_MODE_INPUT,
					GPIO_PUPD_NONE, GPIO8|GPIO7);
	exti_select_source(EXTI8,GPIOA);
	exti_set_trigger(EXTI8,EXTI_TRIGGER_RISING);
	exti_enable_request(EXTI8);
	nvic_enable_irq(NVIC_EXTI4_15_IRQ);
	
	}

static void gpio_setup(void){
	//Enable GPIOA clock.
	rcc_periph_clock_enable(RCC_GPIOA);
	/*	энкодер и две кнопки с подтяжкой к питанию.
	 * PA8 энкодер 1
	 * PA7 энклдер 2
	 * PA6 кнопка энкодера
	 * PA5 кнопка смены диапазона
	 * 
	 */
	 
	gpio_mode_setup(GPIOA, GPIO_MODE_INPUT,
					GPIO_PUPD_PULLUP, GPIO5|GPIO6);
	}
	
void exti4_15_isr(void){
	
	//o_printf_at(0,6,1,0,"ISR_8_pin9=%x",((gpio_get(GPIOA,GPIO7))>>7));
	if(!gpio_get(GPIOA,GPIO7))encoder+=coef; else encoder-=coef;
	if(encoder<MIN_LIMIT)encoder=MIN_LIMIT;
	if(encoder>MAX_LIMIT)encoder=MAX_LIMIT;
	exti_reset_request(EXTI8);
	}	


static void i2c_setup(void){
	/* Enable clocks for I2C1 and AFIO. */
	rcc_periph_clock_enable(RCC_I2C1);
	/* Set alternate functions for the SCL and SDA pins of I2C1.
	 * SDA PA10
	 * SCL PA9
	 *  */
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO9|GPIO10);
	gpio_set_output_options(GPIOA, GPIO_OTYPE_OD,
                    GPIO_OSPEED_2MHZ, GPIO7|GPIO6);
    gpio_set_af(GPIOA,GPIO_AF4,GPIO9|GPIO10);//ремапинг на i2c
	
	/* Disable the I2C before changing any configuration. */
	i2c_peripheral_disable(I2C1);
	i2c_set_speed(I2C1,i2c_speed_fm_400k,48);
	i2c_peripheral_enable(I2C1);
}	





void set_feq_90(uint64_t freq){
	//Используя si5153 можно непосредственно генерировать два сигнала
	//со сдвигом фаз 90 град.
	uint8_t coef=650000000/freq;
	uint64_t pll_freq=coef*freq;
	// We will output 14.1 MHz on CLK0 and CLK1.
	// A PLLA frequency of 705 MHz was chosen to give an even
	// divisor by 14.1 MHz.
	//unsigned long long freq = 14100000 00ULL;
	//unsigned long long pll_freq = 705000000 00ULL;
	// Set CLK0 and CLK1 to output 14.1 MHz with a fixed PLL frequency
	set_freq_manual(freq*100, pll_freq*100, SI5351_CLK0);
	set_freq_manual(freq*100, pll_freq*100, SI5351_CLK1);
	// Now we can set CLK1 to have a 90 deg phase shift by entering
	// 50 in the CLK1 phase register, since the ratio of the PLL to
	// the clock frequency is 50.
	set_phase(SI5351_CLK0, 0);
	set_phase(SI5351_CLK1, coef);
	// We need to reset the PLL before they will be in phase alignment
	pll_reset(SI5351_PLLA);
	}
	
	
	
	
	
void main(){
	rcc_clock_setup_in_hsi_out_48mhz();
	led_setup();
	gpio_setup();
	exti_encoder_init();
	i2c_setup();
	oled_init();
	adc_init();
	//encoder_init();
	//gpio_clear(GPIOB,GPIO8);
	//oled_send_n_bytes_data(0x00,1000);
	gpio_set(GPIOA,GPIO4);
	
	oled_clear();
	o_printf("SIS5351 init");
	si5351_init(SI5351_CRYSTAL_LOAD_10PF, 25000000, 165444);
	si5351_drive_strength(SI5351_CLK0, SI5351_DRIVE_2MA);
	//смеситель аналоговый так что сигнал можно поменьше
	//si5351_drive_strength(SI5351_CLK1, SI5351_DRIVE_2MA);
	//выход сигнала пч
	//si5351_drive_strength(SI5351_CLK2, SI5351_DRIVE_2MA);
	//si5351_set_freq(10200000ULL, SI5351_CLK0);
	//si5351_set_freq(IF_FEQ*100000ULL, SI5351_CLK1);
	//si5351_set_freq(1020000*100ULL, SI5351_CLK2);
	oled_clear();
	next_band();
	uint32_t old_enc=0;
	uint32_t count=0;
	
	while(1){
		if(encoder!=old_enc){
			//set_feq_90(encoder*1000);
			si5351_set_freq(encoder*100000ULL, SI5351_CLK0);
			old_enc=encoder;
			o_printf_at(0,0,1,0,"CNT=%d",count++);
			o_printf_at(0,1,3,0,"%dKHz",encoder-IF_FEQ);
			o_printf_at(0,4,1,0,"F=%dKHz",encoder);
			}
		if(!gpio_get(GPIOA,GPIO6)){
			while(!gpio_get(GPIOA,GPIO6));
			coef*=10;
			if(coef>10)coef=1;
			o_printf_at(0,0,1,0,"coef=>>>%d<<<",coef);
			}
		if(!gpio_get(GPIOA,GPIO5)){
			while(!gpio_get(GPIOA,GPIO5));
			next_band();
			}
		for(uint32_t i=0;i<0xffff;i++)__asm__("nop");
		gpio_toggle(GPIOA,GPIO4);
		//o_printf_at(0,5,1,0,"PORTA=%x",((gpio_get(GPIOA,GPIO8|GPIO7))>>7)&0b11);
		//nvic_disable_irq(NVIC_EXTI4_15_IRQ);
	//	exti_disable_request(EXTI8);
		uint16_t temp2=measure();
	//	exti_enable_request(EXTI8);
		//nvic_enable_irq(NVIC_EXTI4_15_IRQ);
		o_printf_at(0,5,1,0,"V_AGC=%4dmV",temp2);
		
		}
	}

