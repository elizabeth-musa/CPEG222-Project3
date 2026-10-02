/********************************************************************************************
 * CPEG 222 Project 3, 10/2/2026
 * Author: E. MUSA
 * NucleoF466ZE CMSIS STM32F4xx SSD array helper functions
 ********************************************************************************************/
#include "ssd.h"
 
#define SSD_DIGITS 4 // Number of digits in the SSD array
#define SSD_BLANK 10 // Index of the blank entry in digitSegments[]
 
/* Pin description (port and pin number) */
typedef struct
{
    GPIO_TypeDef *port;
    uint8_t pin;
} ssd_pin;
 
/* Segment patterns (1 = segment ON, bit0 = A ... bit6 = G) */
static const uint8_t digitSegments[] = {
    0b0111111, // 0
    0b0000110, // 1
    0b1011011, // 2
    0b1001111, // 3
    0b1100110, // 4
    0b1101101, // 5
    0b1111101, // 6
    0b0000111, // 7
    0b1111111, // 8
    0b1101111, // 9
    0b0000000  // blank/off
};
 
/* Segment pins in order A, B, C, D, E, F, G, DP */
static const ssd_pin segPins[8] = {
    {GPIOG, 9},  // A
    {GPIOF, 12}, // B
    {GPIOF, 13}, // C
    {GPIOG, 14}, // D
    {GPIOE, 8},  // E
    {GPIOF, 15}, // F
    {GPIOB, 4},  // G
    {GPIOF, 14}  // DP
};
 
/* Digit (common anode transistor) pins, leftmost digit first */
static const ssd_pin digitPins[SSD_DIGITS] = {
    {GPIOE, 10}, // CA1
    {GPIOE, 7},  // CA2
    {GPIOB, 5},  // CA3
    {GPIOB, 3}   // CA4
};
 
static volatile uint8_t ssd_digit_value[SSD_DIGITS] = {SSD_BLANK, 0, 0, 0};
static volatile uint8_t ssd_digit_dp[SSD_DIGITS] = {0, 1, 0, 0};
static volatile uint8_t ssd_current_digit = 0;
 
 
// Set a pin as a push-pull output and drive it HIGH (OFF for an active-low pin)
static void SSD_PinInit(const ssd_pin *p)
{
    p->port->MODER &= ~(0x3 << (p->pin * 2));
    p->port->MODER |= (0x1 << (p->pin * 2));
    p->port->BSRR = (1 << p->pin);
}
 
 
// Turn all four digits OFF (base HIGH)
static void SSD_DigitsOff(void)
{
    for (int i = 0; i < SSD_DIGITS; i++)
    {
        digitPins[i].port->BSRR = (1 << digitPins[i].pin);
    }
}
 
 
// Write a segment pattern and decimal point (segment LOW = ON)
static void SSD_WriteSegments(uint8_t pattern, uint8_t dp)
{
    for (int i = 0; i < 8; i++)
    {
        uint8_t on = (i < 7) ? ((pattern >> i) & 1) : dp;
 
        if (on)
        {
            segPins[i].port->BSRR = (1 << (segPins[i].pin + 16)); // Drive LOW
        }
        else
        {
            segPins[i].port->BSRR = (1 << segPins[i].pin); // Drive HIGH
        }
    }
}
 
 
// Initialize SSD GPIO pins and TIM5 multiplex interrupt
void SSD_Init(void)
{
    /* Enable GPIO clocks */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOFEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOGEN;
 
    /* Set segment and digit pins as outputs, all OFF */
    for (int i = 0; i < 8; i++)
    {
        SSD_PinInit(&segPins[i]);
    }
    for (int i = 0; i < SSD_DIGITS; i++)
    {
        SSD_PinInit(&digitPins[i]);
    }
 
    /* Configure TIM5: 1 MHz tick, interrupt every 2.5 ms
       (4 digits x 2.5 ms = each digit refreshed every 10 ms / 100 Hz) */
    RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;
    TIM5->PSC = (SystemCoreClock / 1000000) - 1;
    TIM5->ARR = 2500 - 1;
    TIM5->EGR = TIM_EGR_UG; // Load prescaler
    TIM5->SR = 0;
    TIM5->DIER |= TIM_DIER_UIE;
    NVIC_SetPriority(TIM5_IRQn, 2);
    NVIC_EnableIRQ(TIM5_IRQn);
    TIM5->CR1 |= TIM_CR1_CEN;
}
 
 
// Convert hundredths of a second to XX.XX with the leading zero suppressed
void SSD_DisplayValue(uint16_t value)
{
    if (value > 9999)
    {
        value = 9999;
    }
 
    ssd_digit_value[0] = (value >= 1000) ? (value / 1000) : SSD_BLANK;
    ssd_digit_value[1] = (value / 100) % 10;
    ssd_digit_value[2] = (value / 10) % 10;
    ssd_digit_value[3] = value % 10;
 
    ssd_digit_dp[0] = 0;
    ssd_digit_dp[1] = 1; // Decimal point after the units digit
    ssd_digit_dp[2] = 0;
    ssd_digit_dp[3] = 0;
}
 
 
// Display one digit per call, then advance to the next digit
static void SSD_Refresh(void)
{
    uint8_t d = ssd_current_digit;
 
    SSD_DigitsOff(); // Avoid ghosting while segments change
    SSD_WriteSegments(digitSegments[ssd_digit_value[d]], ssd_digit_dp[d]);
    digitPins[d].port->BSRR = (1 << (digitPins[d].pin + 16)); // Base LOW = digit ON
 
    ssd_current_digit = (d + 1) % SSD_DIGITS;
}
 
 
// TIM5 Interrupt Handler, called every 2.5 ms (one digit per interrupt)
void TIM5_IRQHandler(void)
{
    if (TIM5->SR & TIM_SR_UIF)
    {
        TIM5->SR &= ~TIM_SR_UIF; // Clear update flag
        SSD_Refresh();
    }
}