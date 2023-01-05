#include "py/runtime.h"
#include "py/mphal.h"

#include "ampio_ver.h"
#include "ampio.h"
#include "ampio_hw.h"
#include "ampio_can.h"

STATIC uint16_t led_time;

STATIC mp_obj_t can_msg_cb = mp_const_none;

//#############################################################################
void ampio_init(void)
{
	ampio_hw_init();
}

//#############################################################################
void ampio_deinit(void)
{
	can_msg_cb = mp_const_none;
}

//##############################################################################
void ampio_1ms(void)
{
	static uint8_t time = 200;

	if (--time == 0) {
	 	ampio_hw_led_toggle(3);
	}

	if ( (led_time > 0) && (--led_time == 0)) {
		ampio_hw_led_off(2);
	}
}

//##############################################################################
void ampio_uart_can_ex_msg_cb(uint8_t *msg, uint8_t len)
{
	ampio_hw_led_on(2);
	led_time = 10;

	if (msg[0] == MSG_TYPE_CAN_BROADCAST) {
		if (len >= 5) {
			uint32_t can_id = (uint32_t)msg[4] << 24 | (uint32_t)msg[3] << 16 |
							  (uint32_t)msg[2] << 8 | msg[1];
			if (can_msg_cb != mp_const_none) {
				mp_obj_t can_id_obj = mp_obj_new_int_from_uint(can_id);
				mp_obj_t can_data_obj = mp_obj_new_bytes(&msg[5], len - 5);
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
    int mac = mp_obj_get_int(mac_obj);
    int no = mp_obj_get_int(no_obj);
    int val = mp_obj_get_int(val_obj);

	ampio_can_set_out(mac, no, val);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(ampio_set_out_obj, ampio_set_out);

//#############################################################################
STATIC mp_obj_t ampio_send_raw(mp_obj_t mac_obj, mp_obj_t data_obj) {

	uint32_t mac = mp_obj_get_int(mac_obj);
	mp_buffer_info_t data;
	mp_get_buffer_raise(data_obj, &data, MP_BUFFER_READ);

	if (data.len == 0) {
		mp_printf(&mp_plat_print, "len is 0\n");
		return mp_const_none;
	}
	if (data.len > 200) {
		mp_printf(&mp_plat_print, "len is > 200\n");
		return mp_const_none;
	}
	
	ampio_can_send_raw(mac, data.buf, data.len);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(ampio_send_raw_obj, ampio_send_raw);

//#############################################################################
STATIC mp_obj_t ampio_send_broadcast_temperature(mp_obj_t mac0_obj, mp_obj_t temperature_obj) {
  uint8_t mac0 = mp_obj_get_int(mac0_obj);
  float temperature_f = mp_obj_get_float(temperature_obj);
	uint16_t temperature = (int16_t)(temperature_f * 10) + 1000;

	uint8_t tab[4];
	tab[0] = 0xFE;
	tab[1] = 0x06;
	tab[2] = (uint8_t)(temperature >> 0);
	tab[3] = (uint8_t)(temperature >> 8);

	ampio_can_send_broadcast(mac0, tab, 4);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(ampio_send_broadcast_temperature_obj, ampio_send_broadcast_temperature);


//#############################################################################
STATIC const mp_rom_map_elem_t ampio_module_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_ampio) },
    { MP_ROM_QSTR(MP_QSTR_info), MP_ROM_PTR(&ampio_info_obj) },
	{ MP_ROM_QSTR(MP_QSTR_set_callback), MP_ROM_PTR(&ampio_set_callback_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_out), MP_ROM_PTR(&ampio_set_out_obj) },
    { MP_ROM_QSTR(MP_QSTR_send_broadcast_temperature), MP_ROM_PTR(&ampio_send_broadcast_temperature_obj) },
		{ MP_ROM_QSTR(MP_QSTR_send_raw), MP_ROM_PTR(&ampio_send_raw_obj) },
};
STATIC MP_DEFINE_CONST_DICT(ampio_module_globals, ampio_module_globals_table);

const mp_obj_module_t ampio_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&ampio_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_ampio, ampio_module);
