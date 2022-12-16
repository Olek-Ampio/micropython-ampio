#include "py/runtime.h"
#include "py/mphal.h"

#define VER_MA 0
#define VER_MI 0
#define VER_PA 3

//0.0.3 - fix: ampio_set_out
//0.0.2 - add: ampio_set_out

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

//#############################################################################
void ampio_init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Peripheral clock enable */
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
    HAL_NVIC_SetPriority(USART2_IRQn, 0, 1);
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
static void rx_err(void)
{
	if (++rx_err_cnt == 0)
		rx_err_cnt--;
}

//#############################################################################
STATIC void process_rx_byte(uint8_t rx_byte) {
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

//#############################################################################
STATIC void process_tx_byte(void) {
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

//#############################################################################
void USART2_IRQHandler(void) {

	if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE)) {
	}


	if (__HAL_UART_GET_IT_SOURCE(&huart2, UART_IT_RXNE) && __HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE)) {
        process_rx_byte(huart2.Instance->DR);
	}

	if (__HAL_UART_GET_IT_SOURCE(&huart2, UART_IT_TXE) && __HAL_UART_GET_FLAG(&huart2, UART_FLAG_TXE)) {
        process_tx_byte();
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
STATIC void push_msg(uint8_t *msg, uint8_t len)
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
//#############################################################################
STATIC mp_obj_t ampio_info(void) {
    mp_printf(&mp_plat_print, "ampio version %d.%d.%d\n", VER_MA, VER_MI, VER_PA);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ampio_info_obj, ampio_info);

//#############################################################################
STATIC mp_obj_t ampio_set_out(mp_obj_t mac_obj, mp_obj_t no_obj, mp_obj_t val_obj) {
    uint8_t tab[10];

    int mac = mp_obj_get_int(mac_obj);
    int no = mp_obj_get_int(no_obj);
    int val = mp_obj_get_int(val_obj);

	if (no > 0) {
		no--;
	}
    tab[0] = 0x01;
    tab[1] = (uint8_t)(mac >> 24);
    tab[2] = (uint8_t)(mac >> 16);
    tab[3] = (uint8_t)(mac >> 8);
    tab[4] = (uint8_t)(mac >> 0);
    tab[5] = no;
    tab[6] = val;
    push_msg(tab, 7);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(ampio_set_out_obj, ampio_set_out);


//#############################################################################
STATIC const mp_rom_map_elem_t ampio_module_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_ampio) },
    { MP_ROM_QSTR(MP_QSTR_info), MP_ROM_PTR(&ampio_info_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_out), MP_ROM_PTR(&ampio_set_out_obj) },
};
STATIC MP_DEFINE_CONST_DICT(ampio_module_globals, ampio_module_globals_table);

const mp_obj_module_t ampio_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&ampio_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_ampio, ampio_module);