#ifndef AMPIO_UART_CAN_H_
#define AMPIO_UART_CAN_H_

#define MSG_TYPE_CAN_BROADCAST	0x01
#define MSG_TYPE_SET_CAN_OUT	0x11

void ampio_uart_can_init(void);
void ampio_uart_can_1ms(void);
void ampio_uart_can_push_msg(uint8_t *msg, uint8_t len);

void ampio_uart_can_ex_msg_cb(uint8_t *msg, uint8_t len);
#endif /* AMPIO_UART_CAN_H_ */
