#include "py/runtime.h"
#include "py/mphal.h"
#include "ampio_uart_can.h"

#include "ampio.h"

STATIC UART_HandleTypeDef huart2;

//#############################################################################
#define	RX_FIFO_SIZE 		10
#define	RX_BUFF_SIZE		150
#define	RX_TIMEOUT_MS		100

#define	TX_BUFF_SIZE		600

//############### TX ###############
STATIC uint8_t tx_buff[TX_BUFF_SIZE];
STATIC uint16_t tx_buff_push_ptr = 0;
STATIC uint16_t tx_buff_pop_ptr = 0;
STATIC uint8_t tx_sum;
STATIC uint16_t tx_ovr_cnt;

//############### RX ###############
STATIC uint8_t rx_timeout = RX_TIMEOUT_MS;
STATIC uint8_t rx_buff[RX_FIFO_SIZE][RX_BUFF_SIZE];
STATIC uint8_t rx_fifo_push_ptr = (RX_FIFO_SIZE - 1);
STATIC uint8_t rx_fifo_pop_ptr;
STATIC uint8_t rx_msg_cnt;
STATIC uint8_t frame_byte_last, frame_byte_sum, frame_byte_cnt;
STATIC uint8_t *rx_buff_ptr;
STATIC uint16_t rx_err_cnt;

//##############################################################################
STATIC void process_rx_byte(uint8_t rx_byte);
STATIC void process_tx_byte(void);
STATIC void push_byte(uint8_t data);

//##############################################################################
void ampio_uart_can_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_USART2_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART2 interrupt Init */
    NVIC_SetPriority(USART2_IRQn, 1);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
	__HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);

    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

//##############################################################################
void ampio_uart_can_1ms(void)
{
	uint8_t *msg;
	uint8_t len = 0;

	if (rx_msg_cnt > 0)
	{
		__HAL_UART_DISABLE_IT(&huart2, UART_IT_RXNE);

		rx_msg_cnt--;
		msg = (uint8_t *)&rx_buff[rx_fifo_pop_ptr][1];
		len = rx_buff[rx_fifo_pop_ptr][0];
		if (++rx_fifo_pop_ptr >= RX_FIFO_SIZE) {
			rx_fifo_pop_ptr = 0;
		}
		__HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);

	}
	if (len > 0)
	{
		ampio_uart_can_ex_msg_cb(msg, len);
	}

}

//##############################################################################
void ampio_uart_can_push_msg(uint8_t *msg, uint8_t len)
{
	if (len > 0)
	{
		tx_sum = 0x00;
		push_byte(0x2D);
		push_byte(0xD4);
		push_byte((uint8_t)(len + 1));

		while (len--) {
			push_byte(*msg++);
		}
		push_byte(tx_sum);
	}
}

//##############################################################################
STATIC void push_byte(uint8_t data)
{
	__HAL_UART_DISABLE_IT(&huart2, UART_IT_TXE);

	tx_sum += data;
	if ((++tx_buff_push_ptr) >= TX_BUFF_SIZE)
	{
		tx_buff_push_ptr = 0;
	}
	if (tx_buff_push_ptr == tx_buff_pop_ptr)
	{
		if (++tx_ovr_cnt == 0) {
			tx_ovr_cnt--;
		}
	}
	tx_buff[tx_buff_push_ptr] = data;

	__HAL_UART_ENABLE_IT(&huart2, UART_IT_TXE);
}

//##############################################################################
void USART2_IRQHandler(void)
{
	if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE)) {
		__HAL_UART_CLEAR_OREFLAG(&huart2);
	}


	if (__HAL_UART_GET_IT_SOURCE(&huart2, UART_IT_RXNE) && __HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE)) {
        process_rx_byte(huart2.Instance->DR);
	}

	if (__HAL_UART_GET_IT_SOURCE(&huart2, UART_IT_TXE) && __HAL_UART_GET_FLAG(&huart2, UART_FLAG_TXE)) {
        process_tx_byte();
	}
}

//##############################################################################
STATIC void process_tx_byte(void)
{
    if (tx_buff_pop_ptr == tx_buff_push_ptr)
    {
        __HAL_UART_DISABLE_IT(&huart2, UART_IT_TXE);
    }
    else
    {
        if ((++tx_buff_pop_ptr) >= TX_BUFF_SIZE)
            tx_buff_pop_ptr = 0;
        huart2.Instance->DR = tx_buff[tx_buff_pop_ptr];
    }
}

//##############################################################################
STATIC void rx_err(void)
{
	if (++rx_err_cnt == 0)
		rx_err_cnt--;
}

//##############################################################################
STATIC void process_rx_byte(uint8_t rx_byte)
{
	switch(frame_byte_cnt)
	{
		case 0:
			if (rx_byte != 0x2D) {
				frame_byte_cnt = 255;
				rx_err();
			}
			break;
		case 1:
			if (rx_byte != 0xD4) {
				frame_byte_cnt = 255;
				rx_err();
			}
			break;
		case 2:
			frame_byte_last = rx_byte + 2;
			frame_byte_sum  = rx_byte + 0x01;
			if (++rx_fifo_push_ptr >= RX_FIFO_SIZE)
				rx_fifo_push_ptr = 0;
			rx_buff_ptr = (uint8_t *)&rx_buff[rx_fifo_push_ptr][0];
			rx_buff_ptr[0] = frame_byte_last - 3;
			if (rx_fifo_push_ptr == rx_fifo_pop_ptr)
			{
				if (rx_msg_cnt > 0)
				{
					if (++rx_fifo_pop_ptr >= RX_FIFO_SIZE)
						rx_fifo_pop_ptr = 0;
					rx_err();
				}
			}
		break;		//długość

		default:
			if (frame_byte_cnt == frame_byte_last)
			{
				rx_timeout = 0;
				frame_byte_cnt = 255;
				if (rx_byte == frame_byte_sum)
				{
					if (++rx_msg_cnt > RX_FIFO_SIZE)
					{
						rx_msg_cnt = RX_FIFO_SIZE;
						rx_err();
					}
				}
				else
				{
					rx_err();
					if (rx_fifo_push_ptr == 0)
						rx_fifo_push_ptr = RX_FIFO_SIZE - 1;
					else
						rx_fifo_push_ptr--;
				}
			}
			else
			{
				if (frame_byte_cnt < (RX_BUFF_SIZE + 3))
					rx_buff_ptr[frame_byte_cnt - 2] = rx_byte;
				frame_byte_sum += rx_byte;
			}
		break;
	}
	frame_byte_cnt++;
}
