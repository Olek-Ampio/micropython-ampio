#ifndef AMPIO_CAN_H_
#define AMPIO_CAN_H_

#define MSG_TYPE_CAN_BROADCAST	    0x01
#define MSG_TYPE_SET_CAN_OUT	    0x11
#define MSG_TYPE_SEND_BROADCAST	    0x12
#define MSG_TYPE_SEND_PACK_MSG	    0x13
#define MSG_TYPE_SEND_DIRECT_MSG	0x14

void ampio_can_set_out(uint32_t mac, uint8_t no, uint8_t val);
void ampio_can_send_broadcast(uint8_t mac0, uint8_t data[8], uint8_t len);
void ampio_can_send_raw(uint32_t mac, uint8_t *data, uint8_t len);

#endif /* AMPIO_CAN_H_ */
