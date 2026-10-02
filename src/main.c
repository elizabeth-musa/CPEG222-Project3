/********************************************************************************************
 * CPEG 222 Project 3, 10/2/2026
 * Author: E. MUSA
 * NucleoF466ZE CMSIS STM32F4xx Stopwatch on CPEG222 Shield
 ********************************************************************************************/
#include "stm32f4xx.h" // Include the STM32F4xx CMSIS header file
#include "ssd.h"       // SSD array helper functions
 
/*Left, Up, Center buttons*/
#define LFT_UP_CNTR_PORT GPIOF
 
/*Right, Down Buttons*/
#define RIGHT_DOWN_PORT GPIOE
 
#define MAX_TIME 9999 // 99.99 seconds in hundredths
 
 
// Define STATES
typedef enum
{
    COUNT_UP,
    COUNT_DOWN,
    PAUSE
} state;
 
volatile state current_state = PAUSE;
volatile uint16_t time_hundredths = 0;
 
 
// SysTick Interrupt Handler, called every 10 ms
void SysTick_Handler(void)
{
    if (current_state == COUNT_UP)
    {
        if (time_hundredths < MAX_TIME)
        {
            time_hundredths++;
        }
        if (time_hundredths >= MAX_TIME) // Stop at 99.99
        {
            current_state = PAUSE;
        }
    }
    else if (current_state == COUNT_DOWN)
    {
        if (time_hundredths > 0)
        {
            time_hundredths--;
        }
        if (time_hundredths == 0) // Stop at 0.00
        {
            current_state = PAUSE;
        }
    }
}
 
 
// Button Interrupt Handler
void EXTI9_5_IRQHandler(void)
{
    if (EXTI->PR & (1 << 7)) // UP button counts up
    {
        current_state = COUNT_UP;
        EXTI->PR = (1 << 7);
    }
 
    if (EXTI->PR & (1 << 5)) // DOWN button counts down
    {
        current_state = COUNT_DOWN;
        EXTI->PR = (1 << 5);
    }
 
    if (EXTI->PR & (1 << 8)) // CENTER button pauses
    {
        current_state = PAUSE;
        EXTI->PR = (1 << 8);
    }
 
    if (EXTI->PR & (1 << 6)) // RIGHT button sets 99.99 and pauses
    {
        current_state = PAUSE;
        time_hundredths = MAX_TIME;
        EXTI->PR = (1 << 6);
    }
 
    if (EXTI->PR & (1 << 9)) // LEFT button sets 0.00 and pauses
    {
        current_state = PAUSE;
        time_hundredths = 0;
        EXTI->PR = (1 << 9);
    }
}
 
 
int main(void)
{
    /* Enable GPIO clocks */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOFEN;
 
 
    /* Set Buttons as inputs */
    LFT_UP_CNTR_PORT->MODER &= ~(0x3 << (9 * 2)); // LEFT button
    LFT_UP_CNTR_PORT->MODER &= ~(0x3 << (8 * 2)); // CENTER button
    LFT_UP_CNTR_PORT->MODER &= ~(0x3 << (7 * 2)); // UP button
    RIGHT_DOWN_PORT->MODER &= ~(0x3 << (6 * 2));  // RIGHT button
    RIGHT_DOWN_PORT->MODER &= ~(0x3 << (5 * 2));  // DOWN button
 
 
    /* Set buttons to pull-up */
    LFT_UP_CNTR_PORT->PUPDR &= ~(0x3 << (9 * 2));
    LFT_UP_CNTR_PORT->PUPDR |= (0x1 << (9 * 2)); // LEFT button
    LFT_UP_CNTR_PORT->PUPDR &= ~(0x3 << (8 * 2));
    LFT_UP_CNTR_PORT->PUPDR |= (0x1 << (8 * 2)); // CENTER button
    LFT_UP_CNTR_PORT->PUPDR &= ~(0x3 << (7 * 2));
    LFT_UP_CNTR_PORT->PUPDR |= (0x1 << (7 * 2)); // UP button
    RIGHT_DOWN_PORT->PUPDR &= ~(0x3 << (6 * 2));
    RIGHT_DOWN_PORT->PUPDR |= (0x1 << (6 * 2)); // RIGHT button
    RIGHT_DOWN_PORT->PUPDR &= ~(0x3 << (5 * 2));
    RIGHT_DOWN_PORT->PUPDR |= (0x1 << (5 * 2)); // DOWN button
 
 
    /* Connect PF9 to EXTI9 and PF8 to EXTI8 */
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    SYSCFG->EXTICR[2] &= ~(0xF << 4);
    SYSCFG->EXTICR[2] |= (0x5 << 4);
    SYSCFG->EXTICR[2] &= ~(0xF << 0);
    SYSCFG->EXTICR[2] |= (0x5 << 0);
 
 
    /* Connect PF7 to EXTI7 */
    SYSCFG->EXTICR[1] &= ~(0xF << 12);
    SYSCFG->EXTICR[1] |= (0x5 << 12);
 
 
    /* Connect PE6 to EXTI6 and PE5 to EXTI5 */
    SYSCFG->EXTICR[1] &= ~(0xF << 8);
    SYSCFG->EXTICR[1] |= (0x4 << 8);
    SYSCFG->EXTICR[1] &= ~(0xF << 4);
    SYSCFG->EXTICR[1] |= (0x4 << 4);
 
 
    /* Enable EXTI interrupts */
    EXTI->IMR |= (1 << 5);
    EXTI->IMR |= (1 << 6);
    EXTI->IMR |= (1 << 7);
    EXTI->IMR |= (1 << 8);
    EXTI->IMR |= (1 << 9);
 
 
    /* Set EXTI to trigger on falling edge */
    EXTI->FTSR |= (1 << 5);
    EXTI->FTSR |= (1 << 6);
    EXTI->FTSR |= (1 << 7);
    EXTI->FTSR |= (1 << 8);
    EXTI->FTSR |= (1 << 9);
 
 
    /* Enable EXTI9_5 interrupt */
    NVIC_SetPriority(EXTI9_5_IRQn, 3);
    NVIC_EnableIRQ(EXTI9_5_IRQn);
 
 
    /* Initialize SSD array and TIM5 multiplexing */
    SSD_Init();
 
 
    /* Begin in PAUSE state showing 0.00 */
    current_state = PAUSE;
    time_hundredths = 0;
    SSD_DisplayValue(time_hundredths);
 
 
    /* Configure SysTick for 10 ms (0.01 s) interrupts */
    SysTick_Config(SystemCoreClock / 100);
    NVIC_SetPriority(SysTick_IRQn, 1);
 
 
    while (1)
    {
        /* Update the SSD digits with the current time */
        SSD_DisplayValue(time_hundredths);
    }
 
    return 0;
}