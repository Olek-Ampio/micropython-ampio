#define MICROPY_HW_BOARD_NAME       "F411AMPIO"
#define MICROPY_HW_MCU_NAME         "STM32F411"

#define MICROPY_HW_HAS_SWITCH       (0)
#define MICROPY_HW_HAS_FLASH        (1)
#define MICROPY_HW_ENABLE_RTC       (0)
#define MICROPY_HW_ENABLE_USB       (1)

// HSE is 8MHz
#define MICROPY_HW_CLK_PLLM (5)
#define MICROPY_HW_CLK_PLLN (210)
#define MICROPY_HW_CLK_PLLP (RCC_PLLP_DIV4)
#define MICROPY_HW_CLK_PLLQ (7)

// does not have a 32kHz crystal
#define MICROPY_HW_RTC_USE_LSE      (0)

// LEDs
#define MICROPY_HW_LED1             (pin_B12)
#define MICROPY_HW_LED2             (pin_B13)
#define MICROPY_HW_LED3             (pin_B14)
#define MICROPY_HW_LED_ON(pin)      (mp_hal_pin_high(pin))
#define MICROPY_HW_LED_OFF(pin)     (mp_hal_pin_low(pin))

// USB config
#define MICROPY_HW_USB_FS              (1)
#define MICROPY_HW_USB_VBUS_DETECT_PIN (pin_A9)
#define MICROPY_HW_USB_OTG_ID_PIN      (pin_A10)
