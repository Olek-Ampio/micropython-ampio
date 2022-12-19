#include "py/runtime.h"
#include "py/mphal.h"

#include "ampio.h"
#include "ampio_uart_can.h"

#define VER_MA 0
#define VER_MI 0
#define VER_PA 3

//0.0.3 - fix: ampio_set_out
//0.0.2 - add: ampio_set_out

STATIC TIM_HandleTypeDef htim10;
STATIC uint16_t led_time;

STATIC mp_obj_t can_msg_cb = mp_const_none;

//#############################################################################
STATIC void io_init(void);
STATIC void timer_init(void);
STATIC void ampio_can_send(uint8_t *data, uint8_t len);

//#############################################################################
void ampio_init(void)
{
	io_init();
	timer_init();
	ampio_uart_can_init();
}

//##############################################################################
STATIC void ampio_1ms(void)
{
	static uint8_t time = 200;

	if (--time == 0) {
	 	HAL_GPIO_TogglePin(LED3_GPIO_Port, LED3_Pin);
	}

	if ( (led_time > 0) && (--led_time == 0)) {
		HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
	}
}

//##############################################################################
void ampio_uart_can_ex_msg_cb(uint8_t *msg, uint8_t len)
{
	HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
	led_time = 10;

	if (msg[0] == MSG_TYPE_CAN_BROADCAST) {
		if (len >= 5) {
			uint32_t can_id = (uint32_t)msg[4] << 24 | (uint32_t)msg[3] << 16 |
							  (uint32_t)msg[2] << 8 | msg[1];
			if (can_msg_cb != mp_const_none) {
				mp_obj_t can_id_obj = mp_obj_new_int_from_uint(can_id);
				mp_obj_t can_data_obj = mp_obj_new_bytearray(len - 5, &msg[5]);
				mp_call_function_2(can_msg_cb, can_id_obj, can_data_obj);
			}
		}
	}
}

//#############################################################################
STATIC mp_obj_t ampio_info(void) {
    mp_printf(&mp_plat_print, "ampio version %d.%d.%d\n", VER_MA, VER_MI, VER_PA);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ampio_info_obj, ampio_info);

//#############################################################################
STATIC mp_obj_t ampio_set_callback(mp_obj_t callback) {
	can_msg_cb = callback;
	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(ampio_set_callback_obj, ampio_set_callback);

//#############################################################################
STATIC mp_obj_t ampio_set_out(mp_obj_t mac_obj, mp_obj_t no_obj, mp_obj_t val_obj) {
    uint8_t tab[10];

    int mac = mp_obj_get_int(mac_obj);
    int no = mp_obj_get_int(no_obj);
    int val = mp_obj_get_int(val_obj);

	if (no > 0) {
		no--;
	}
    tab[0] = 0x11;
    tab[1] = (uint8_t)(mac >> 24);
    tab[2] = (uint8_t)(mac >> 16);
    tab[3] = (uint8_t)(mac >> 8);
    tab[4] = (uint8_t)(mac >> 0);
    tab[5] = no;
    tab[6] = val;
    ampio_can_send(tab, 7);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(ampio_set_out_obj, ampio_set_out);


//#############################################################################
STATIC const mp_rom_map_elem_t ampio_module_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_ampio) },
    { MP_ROM_QSTR(MP_QSTR_info), MP_ROM_PTR(&ampio_info_obj) },
	{ MP_ROM_QSTR(MP_QSTR_set_callback), MP_ROM_PTR(&ampio_set_callback_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_out), MP_ROM_PTR(&ampio_set_out_obj) },
};
STATIC MP_DEFINE_CONST_DICT(ampio_module_globals, ampio_module_globals_table);

const mp_obj_module_t ampio_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&ampio_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_ampio, ampio_module);


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
	HAL_NVIC_SetPriority(TIM1_UP_TIM10_IRQn, 0, 3);
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

//##############################################################################
STATIC void ampio_can_send(uint8_t *data, uint8_t len)
{
	__HAL_TIM_DISABLE_IT(&htim10, TIM_IT_UPDATE);
	ampio_uart_can_push_msg(data, len);
	__HAL_TIM_ENABLE_IT(&htim10, TIM_IT_UPDATE);
}
