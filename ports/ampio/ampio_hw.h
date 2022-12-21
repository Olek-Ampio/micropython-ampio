#ifndef AMPIO_HW_H_
#define AMPIO_HW_H_

#define LED1_Pin GPIO_PIN_12
#define LED1_GPIO_Port GPIOB
#define LED2_Pin GPIO_PIN_13
#define LED2_GPIO_Port GPIOB
#define LED3_Pin GPIO_PIN_14
#define LED3_GPIO_Port GPIOB

void ampio_hw_init(void);
void ampio_hw_can_send(uint8_t *data, uint8_t len);
void ampio_hw_led_on(uint8_t led_no);
void ampio_hw_led_off(uint8_t led_no);
void ampio_hw_led_toggle(uint8_t led_no);

#endif /* AMPIO_HW_H_ */
