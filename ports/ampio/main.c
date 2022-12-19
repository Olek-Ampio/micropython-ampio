/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2020 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>

#include "py/runtime.h"
#include "py/stackctrl.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "shared/readline/readline.h"
#include "shared/runtime/pyexec.h"
#include "shared/runtime/softtimer.h"
#include "lib/oofatfs/ff.h"
#include "lib/littlefs/lfs1.h"
#include "lib/littlefs/lfs1_util.h"
#include "lib/littlefs/lfs2.h"
#include "lib/littlefs/lfs2_util.h"
#include "extmod/modnetwork.h"
#include "extmod/vfs.h"
#include "extmod/vfs_fat.h"
#include "extmod/vfs_lfs.h"

#include "boardctrl.h"
#include "mpbthciport.h"
#include "mpu.h"
#include "systick.h"
#include "pendsv.h"
#include "powerctrl.h"
#include "gccollect.h"
#include "factoryreset.h"
#include "modmachine.h"
#include "led.h"
#include "pin.h"
#include "usb.h"
#include "storage.h"

#include "ampio.h"

void nlr_jump_fail(void *val) {
    printf("FATAL: uncaught exception %p\n", val);
    mp_obj_print_exception(&mp_plat_print, MP_OBJ_FROM_PTR(val));
    MICROPY_BOARD_FATAL_ERROR("");
}

void abort(void) {
    MICROPY_BOARD_FATAL_ERROR("abort");
}

#ifndef NDEBUG
void MP_WEAK __assert_func(const char *file, int line, const char *func, const char *expr) {
    (void)func;
    printf("Assertion '%s' failed, at file %s:%d\n", expr, file, line);
    MICROPY_BOARD_FATAL_ERROR("");
}
#endif

STATIC mp_obj_t pyb_main(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_opt, MP_ARG_INT, {.u_int = 0} }
    };

    if (mp_obj_is_str(pos_args[0])) {
        MP_STATE_PORT(pyb_config_main) = pos_args[0];

        // parse args
        mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
        mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
        #if MICROPY_ENABLE_COMPILER
        MP_STATE_VM(mp_optimise_value) = args[0].u_int;
        #endif
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(pyb_main_obj, 1, pyb_main);

#if MICROPY_HW_FLASH_MOUNT_AT_BOOT
// avoid inlining to avoid stack usage within main()
MP_NOINLINE STATIC bool init_flash_fs(uint reset_mode) {
    if (reset_mode == BOARDCTRL_RESET_MODE_FACTORY_FILESYSTEM) {
        // Asked by user to reset filesystem
        factory_reset_create_filesystem();
    }

    // Default block device to entire flash storage
    mp_obj_t bdev = MP_OBJ_FROM_PTR(&pyb_flash_obj);

    int ret;

    // Try to mount the flash on "/flash" and chdir to it for the boot-up directory.
    mp_obj_t mount_point = MP_OBJ_NEW_QSTR(MP_QSTR__slash_flash);
    ret = mp_vfs_mount_and_chdir_protected(bdev, mount_point);

    if (ret == -MP_ENODEV && bdev == MP_OBJ_FROM_PTR(&pyb_flash_obj)
        && reset_mode != BOARDCTRL_RESET_MODE_FACTORY_FILESYSTEM) {
        // No filesystem, bdev is still the default (so didn't detect a possibly corrupt littlefs),
        // and didn't already create a filesystem, so try to create a fresh one now.
        ret = factory_reset_create_filesystem();
        if (ret == 0) {
            ret = mp_vfs_mount_and_chdir_protected(bdev, mount_point);
        }
    }

    if (ret != 0) {
        printf("MPY: can't mount flash\n");
        return false;
    }

    return true;
}
#endif

void stm32_main(uint32_t reset_mode) {
    // Enable 8-byte stack alignment for IRQ handlers, in accord with EABI
    SCB->CCR |= SCB_CCR_STKALIGN_Msk;

    // Hook for a board to run code at start up, for example check if a
    // bootloader should be entered instead of the main application.
    MICROPY_BOARD_STARTUP();

    // Enable caches and prefetch buffers

    #if defined(STM32F4)

    #if INSTRUCTION_CACHE_ENABLE
    __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
    #endif
    #if DATA_CACHE_ENABLE
    __HAL_FLASH_DATA_CACHE_ENABLE();
    #endif
    #if PREFETCH_ENABLE
    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    #endif

    #elif defined(STM32F7) || defined(STM32H7)

    #if ART_ACCLERATOR_ENABLE
    __HAL_FLASH_ART_ENABLE();
    #endif

    SCB_EnableICache();
    SCB_EnableDCache();

    #elif defined(STM32L4)

    #if !INSTRUCTION_CACHE_ENABLE
    __HAL_FLASH_INSTRUCTION_CACHE_DISABLE();
    #endif
    #if !DATA_CACHE_ENABLE
    __HAL_FLASH_DATA_CACHE_DISABLE();
    #endif
    #if PREFETCH_ENABLE
    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    #endif

    #endif

    mpu_init();

    #if __CORTEX_M >= 0x03
    // Set the priority grouping
    NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
    #endif

    // SysTick is needed by HAL_RCC_ClockConfig (called in SystemClock_Config)
    HAL_InitTick(TICK_INT_PRIORITY);

    // set the system clock to be HSE
    SystemClock_Config();

    #if defined(STM32F4) || defined(STM32F7)
    #if defined(__HAL_RCC_DTCMRAMEN_CLK_ENABLE)
    // The STM32F746 doesn't really have CCM memory, but it does have DTCM,
    // which behaves more or less like normal SRAM.
    __HAL_RCC_DTCMRAMEN_CLK_ENABLE();
    #elif defined(CCMDATARAM_BASE)
    // enable the CCM RAM
    __HAL_RCC_CCMDATARAMEN_CLK_ENABLE();
    #endif
    #elif defined(STM32H7A3xx) || defined(STM32H7A3xxQ) || defined(STM32H7B3xx) || defined(STM32H7B3xxQ)
    // Enable SRAM clock.
    __HAL_RCC_SRDSRAM_CLK_ENABLE();
    #elif defined(STM32H7)
    // Enable D2 SRAM1/2/3 clocks.
    __HAL_RCC_D2SRAM1_CLK_ENABLE();
    __HAL_RCC_D2SRAM2_CLK_ENABLE();
    __HAL_RCC_D2SRAM3_CLK_ENABLE();
    #endif

    MICROPY_BOARD_EARLY_INIT();

    // basic sub-system init
    pendsv_init();
    led_init();
    machine_init();
    #if MICROPY_HW_ENABLE_STORAGE
    storage_init();
    #endif

    boardctrl_state_t state;
    state.reset_mode = reset_mode;
    state.log_soft_reset = false;

    MICROPY_BOARD_BEFORE_SOFT_RESET_LOOP(&state);

soft_reset:

    MICROPY_BOARD_TOP_SOFT_RESET_LOOP(&state);

    // Stack limit should be less than real stack size, so we have a chance
    // to recover from limit hit.  (Limit is measured in bytes.)
    // Note: stack control relies on main thread being initialised above
    mp_stack_set_top(&_estack);
    mp_stack_set_limit((char *)&_estack - (char *)&_sstack - 1024);

    // GC init
    gc_init(MICROPY_HEAP_START, MICROPY_HEAP_END);

    // MicroPython init
    mp_init();

    // Initialise low-level sub-systems.  Here we need to very basic things like
    // zeroing out memory and resetting any of the sub-systems.  Following this
    // we can run Python scripts (eg boot.py), but anything that is configurable
    // by boot.py must be set after boot.py is run.

    MP_STATE_PORT(pyb_stdio_uart) = NULL;

    readline_init0();
    pin_init0();
    // extint_init0();
    // timer_init0();

    #if MICROPY_HW_ENABLE_USB
    pyb_usb_init0();
    #endif

    // Initialise the local flash filesystem.
    // Create it if needed, mount in on /flash, and set it as current dir.
    bool mounted_flash = false;
    #if MICROPY_HW_FLASH_MOUNT_AT_BOOT
    mounted_flash = init_flash_fs(state.reset_mode);
    #endif

    #if MICROPY_HW_ENABLE_USB
    // if the SD card isn't used as the USB MSC medium then use the internal flash
    if (pyb_usb_storage_medium == PYB_USB_STORAGE_MEDIUM_NONE) {
        pyb_usb_storage_medium = PYB_USB_STORAGE_MEDIUM_FLASH;
    }
    #endif

    if (mounted_flash) {
        mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR__slash_flash));
        mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR__slash_flash_slash_lib));
    }

    // reset config variables; they should be set by boot.py
    MP_STATE_PORT(pyb_config_main) = MP_OBJ_NULL;

    // Run boot.py (or whatever else a board configures at this stage).
    if (MICROPY_BOARD_RUN_BOOT_PY(&state) == BOARDCTRL_GOTO_SOFT_RESET_EXIT) {
        goto soft_reset_exit;
    }

    // Now we initialise sub-systems that need configuration from boot.py,
    // or whose initialisation can be safely deferred until after running
    // boot.py.

    #if MICROPY_HW_ENABLE_USB
    // init USB device to default setting if it was not already configured
    if (!(pyb_usb_flags & PYB_USB_FLAG_USB_MODE_CALLED)) {
        #if MICROPY_HW_USB_MSC
        const uint16_t pid = MICROPY_HW_USB_PID_CDC_MSC;
        const uint8_t mode = USBD_MODE_CDC_MSC;
        #else
        const uint16_t pid = MICROPY_HW_USB_PID_CDC;
        const uint8_t mode = USBD_MODE_CDC;
        #endif
        pyb_usb_dev_init(pyb_usb_dev_detect(), MICROPY_HW_USB_VID, pid, mode, 0, NULL, NULL);
    }
    #endif

    ampio_init();

    // At this point everything is fully configured and initialised.

    // Run main.py (or whatever else a board configures at this stage).
    if (MICROPY_BOARD_RUN_MAIN_PY(&state) == BOARDCTRL_GOTO_SOFT_RESET_EXIT) {
        goto soft_reset_exit;
    }
    
    #if MICROPY_ENABLE_COMPILER
    // Main script is finished, so now go into REPL mode.
    // The REPL mode can change, or it can request a soft reset.
    for (;;) {
        if (pyexec_mode_kind == PYEXEC_MODE_RAW_REPL) {
            if (pyexec_raw_repl() != 0) {
                break;
            }
        } else {
            if (pyexec_friendly_repl() != 0) {
                break;
            }
        }
    }
    #endif

soft_reset_exit:
    // soft reset

    MICROPY_BOARD_START_SOFT_RESET(&state);

    #if MICROPY_HW_ENABLE_STORAGE
    if (state.log_soft_reset) {
        mp_printf(&mp_plat_print, "MPY: sync filesystems\n");
    }
    storage_flush();
    #endif

    if (state.log_soft_reset) {
        mp_printf(&mp_plat_print, "MPY: soft reboot\n");
    }

    soft_timer_deinit();
    machine_deinit();

    MICROPY_BOARD_END_SOFT_RESET(&state);

    gc_sweep_all();
    mp_deinit();

    goto soft_reset;
}

MP_REGISTER_ROOT_POINTER(mp_obj_t pyb_config_main);
