#define STM32F411xE
#include "stm32f4xx.h"
#include "stm32f411xe.h"

#define BTN_BIT     (1UL << 0)    /* PA0 / EXTI line 0 */
#define LED_PIN     13U           /* PC13 */
#define LED_OFF()   (GPIOC->BSRR = (1UL << LED_PIN))   /* active low: set = OFF */

static void gpio_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN;

    GPIOC->MODER   &= ~(0x3UL << (LED_PIN * 2));
    GPIOC->MODER   |=  (0x1UL << (LED_PIN * 2));
    GPIOC->OTYPER  &= ~(1UL << LED_PIN);
    GPIOC->OSPEEDR &= ~(0x3UL << (LED_PIN * 2));
    GPIOC->PUPDR   &= ~(0x3UL << (LED_PIN * 2));
    LED_OFF();

    GPIOA->MODER &= ~(0x3UL << (0 * 2));      /* PA0 input */
    GPIOA->PUPDR &= ~(0x3UL << (0 * 2));
    GPIOA->PUPDR |=  (0x1UL << (0 * 2));      /* pull-up */
}

static void exti_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC->APB2ENR;

    SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI0;  /* EXTI0 <- PA0 */

    EXTI->IMR  |=  BTN_BIT;
    EXTI->EMR  &= ~BTN_BIT;
    EXTI->FTSR |=  BTN_BIT;       /* falling edge = ditekan */
    EXTI->RTSR &= ~BTN_BIT;
    EXTI->PR    =  BTN_BIT;

    NVIC_SetPriority(EXTI0_IRQn, 1);
    NVIC_EnableIRQ(EXTI0_IRQn);
}

static void timers_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN | RCC_APB1ENR_TIM5EN;
    (void)RCC->APB1ENR;

    /* TIM5: debounce 30 ms, one-pulse */
    TIM5->PSC  = 15999;
    TIM5->ARR  = 29;
    TIM5->CR1 |= TIM_CR1_OPM;
    TIM5->EGR  = TIM_EGR_UG;
    TIM5->SR   = 0;
    TIM5->DIER |= TIM_DIER_UIE;
    NVIC_SetPriority(TIM5_IRQn, 2);
    NVIC_EnableIRQ(TIM5_IRQn);

    /* TIM2: laju toggle LED 250 ms (belum dijalankan) */
    TIM2->PSC  = 15999;
    TIM2->ARR  = 249;
    TIM2->EGR  = TIM_EGR_UG;
    TIM2->SR   = 0;
    TIM2->DIER |= TIM_DIER_UIE;
    NVIC_SetPriority(TIM2_IRQn, 3);
    NVIC_EnableIRQ(TIM2_IRQn);
}

int main(void)
{
    gpio_init();
    timers_init();
    exti_init();

    while (1) {
        /* semua logika ada di ISR; main loop bebas untuk pekerjaan lain */
    }
}

/* Tombol ditekan: mulai jendela debounce */
void EXTI0_IRQHandler(void)
{
    if (EXTI->PR & BTN_BIT) {
        EXTI->PR   = BTN_BIT;
        EXTI->IMR &= ~BTN_BIT;       /* abaikan bouncing */

        TIM5->CNT  = 0;
        TIM5->SR   = 0;
        TIM5->CR1 |= TIM_CR1_CEN;
    }
}

/* Debounce selesai: validasi tekanan, lalu mulai blink */
void TIM5_IRQHandler(void)
{
    if (TIM5->SR & TIM_SR_UIF) {
        TIM5->SR &= ~TIM_SR_UIF;

        if (!(GPIOA->IDR & BTN_BIT)) {         /* masih ditekan */
            GPIOC->ODR ^= (1UL << LED_PIN);    /* toggle pertama saat ditekan */
            TIM2->CNT  = 0;
            TIM2->SR   = 0;
            TIM2->CR1 |= TIM_CR1_CEN;          /* mulai blink terus-menerus */
        }

        EXTI->PR   = BTN_BIT;
        EXTI->IMR |= BTN_BIT;
    }
}

/* Setiap 250 ms: toggle selama tombol masih ditekan */
void TIM2_IRQHandler(void)
{
    if (TIM2->SR & TIM_SR_UIF) {
        TIM2->SR &= ~TIM_SR_UIF;

        if (GPIOA->IDR & BTN_BIT) {            /* tombol dilepas */
            TIM2->CR1 &= ~TIM_CR1_CEN;         /* stop blink */
            LED_OFF();                         /* kembali normal */
        } else {
            GPIOC->ODR ^= (1UL << LED_PIN);
        }
    }
}