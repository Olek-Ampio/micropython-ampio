#include "py/mphal.h"

#include "ampio_can.h"
#include "ampio_hw.h"

void ampio_can_set_out(uint32_t mac, uint8_t no, uint8_t val)
{
    uint8_t tab[10];

	if (no > 0) {
		no--;
	}
    tab[0] = MSG_TYPE_SET_CAN_OUT;
    tab[1] = (uint8_t)(mac >> 24);
    tab[2] = (uint8_t)(mac >> 16);
    tab[3] = (uint8_t)(mac >> 8);
    tab[4] = (uint8_t)(mac >> 0);
    tab[5] = no;
    tab[6] = val;
    ampio_hw_can_send(tab, 7);
}

void ampio_can_send_broadcast(uint8_t mac0, uint8_t data[8], uint8_t len)
{
    uint8_t tab[10];

    if ( (len < 2) || (len > 8) ) {
        return;
    }
    tab[0] = MSG_TYPE_SEND_BROADCAST;
    tab[1] = mac0;
    memcpy(&tab[2], &data[0], len);
    ampio_hw_can_send(tab, len + 2);
}

void ampio_can_send_raw(uint32_t mac, uint8_t *data, uint8_t len)
{
    uint8_t tab[256];

    if ( (len < 1) || (len > 250) ) {
        return;
    }
    tab[0] = MSG_TYPE_SEND_PACK_MSG;
    tab[1] = (uint8_t)(mac >> 24);
    tab[2] = (uint8_t)(mac >> 16);
    tab[3] = (uint8_t)(mac >> 8);
    tab[4] = (uint8_t)(mac >> 0);
    memcpy(&tab[5], &data[0], len);
    ampio_hw_can_send(tab, len + 5);
}
