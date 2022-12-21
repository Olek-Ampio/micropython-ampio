#include "py/runtime.h"
#include "py/mphal.h"

#include "ampio.h"
#include "ampio_hw.h"
#include "ampio_uart_can.h"

STATIC TIM_HandleTypeDef htim10;

//##############################################################################
STATIC void io_init(void);
STATIC void timer_init(void);

//##############################################################################
void ampio_hw_init(void)
{
    io_init();
   	timer_init();
   	ampio_uart_can_init();
}

//##############################################################################
void ampio_hw_led_on(uint8_t led_no)
{
    switch(led_no)
    {
        case 1:
            HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
            break;
        case 2:
            HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
            break;
        case 3:
            HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
            break;
    }
}

//##############################################################################
void ampio_hw_led_off(uint8_t led_no)
{
    switch(led_no)
    {
        case 1:
            HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
            break;
        case 2:
            HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
            break;
        case 3:
            HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);
            break;
    }
}

//##############################################################################
void ampio_hw_led_toggle(uint8_t led_no)
{
    switch(led_no)
    {
        case 1:
            HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
            break;
        case 2:
            HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin);
            break;
        case 3:
            HAL_GPIO_TogglePin(LED3_GPIO_Port, LED3_Pin);
            break;
    }
}

//##############################################################################
void ampio_hw_can_send(uint8_t *data, uint8_t len)
{
	__HAL_TIM_DISABLE_IT(&htim10, TIM_IT_UPDATE);
	ampio_uart_can_push_msg(data, len);
	__HAL_TIM_ENABLE_IT(&htim10, TIM_IT_UPDATE);
}

//##############################################################################
STATIC void io_init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOB, LED1_Pin|LED2_Pin|LED3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : LED1_Pin LED2_Pin LED3_Pin */
  GPIO_InitStruct.Pin = LED1_Pin|LED2_Pin|LED3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

//##############################################################################
STATIC void timer_init(void)
{
	__HAL_RCC_TIM10_CLK_ENABLE();
	NVIC_SetPriority(TIM1_UP_TIM10_IRQn, 3);
	HAL_NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);

	htim10.Instance = TIM10;
	htim10.Init.Prescaler = 83;
	htim10.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim10.Init.Period = 1000 - 1;
	htim10.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	HAL_TIM_Base_Init(&htim10);
	HAL_TIM_Base_Start_IT(&htim10);
}

//##############################################################################
void TIM1_UP_TIM10_IRQHandler(void)
{
	if ( (__HAL_TIM_GET_FLAG(&htim10, TIM_FLAG_UPDATE) != RESET) &&
		 (__HAL_TIM_GET_IT_SOURCE(&htim10, TIM_IT_UPDATE) != RESET) )
	{
		__HAL_TIM_CLEAR_IT(&htim10, TIM_IT_UPDATE);

		ampio_1ms();
		ampio_uart_can_1ms();	// => ampio_uart_can_ex_msg_cb
//		ampio_uart_repl_1ms();
	}
}
