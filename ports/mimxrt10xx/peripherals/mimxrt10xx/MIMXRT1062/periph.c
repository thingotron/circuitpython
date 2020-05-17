 /*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Lucian Copeland for Adafruit Industries
 * Copyright (c) 2019 Artur Pacholec
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
 * AUTHORS  R COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "py/obj.h"
#include "py/mphal.h"
#include "mimxrt10xx/periph.h"

LPI2C_Type *mcu_i2c_banks[] = { LPI2C1, LPI2C2, LPI2C3, LPI2C4 };

const mcu_periph_obj_t mcu_i2c_sda_list[9] = {
    PERIPH_PIN(1, 2, kIOMUXC_LPI2C1_SDA_SELECT_INPUT, 0, &pin_GPIO_SD_B1_05),
    PERIPH_PIN(1, 3, kIOMUXC_LPI2C1_SDA_SELECT_INPUT, 1, &pin_GPIO_AD_B1_01),

    PERIPH_PIN(2, 3, kIOMUXC_LPI2C2_SDA_SELECT_INPUT, 0, &pin_GPIO_SD_B1_10),
    PERIPH_PIN(2, 2, kIOMUXC_LPI2C2_SDA_SELECT_INPUT, 1, &pin_GPIO_B0_05),

    PERIPH_PIN(3, 2, kIOMUXC_LPI2C3_SDA_SELECT_INPUT, 0, &pin_GPIO_EMC_21),
    PERIPH_PIN(3, 2, kIOMUXC_LPI2C3_SDA_SELECT_INPUT, 1, &pin_GPIO_SD_B0_01),
    PERIPH_PIN(3, 1, kIOMUXC_LPI2C3_SDA_SELECT_INPUT, 2, &pin_GPIO_AD_B1_06),

    PERIPH_PIN(4, 2, kIOMUXC_LPI2C4_SDA_SELECT_INPUT, 0, &pin_GPIO_EMC_11),
    PERIPH_PIN(4, 0, kIOMUXC_LPI2C4_SDA_SELECT_INPUT, 1, &pin_GPIO_AD_B0_13),
};

const mcu_periph_obj_t mcu_i2c_scl_list[9] = {
    PERIPH_PIN(1, 2, kIOMUXC_LPI2C1_SCL_SELECT_INPUT, 0, &pin_GPIO_SD_B1_04),
    PERIPH_PIN(1, 3, kIOMUXC_LPI2C1_SCL_SELECT_INPUT, 1, &pin_GPIO_AD_B1_00),

    PERIPH_PIN(2, 3, kIOMUXC_LPI2C2_SCL_SELECT_INPUT, 0, &pin_GPIO_SD_B1_11),
    PERIPH_PIN(2, 2, kIOMUXC_LPI2C2_SCL_SELECT_INPUT, 1, &pin_GPIO_B0_04),

    PERIPH_PIN(3, 2, kIOMUXC_LPI2C3_SCL_SELECT_INPUT, 0, &pin_GPIO_EMC_22),
    PERIPH_PIN(3, 2, kIOMUXC_LPI2C3_SCL_SELECT_INPUT, 1, &pin_GPIO_SD_B0_00),
    PERIPH_PIN(3, 1, kIOMUXC_LPI2C3_SCL_SELECT_INPUT, 2, &pin_GPIO_AD_B1_07),

    PERIPH_PIN(4, 2, kIOMUXC_LPI2C4_SCL_SELECT_INPUT, 0, &pin_GPIO_EMC_12),
    PERIPH_PIN(4, 0, kIOMUXC_LPI2C4_SCL_SELECT_INPUT, 1, &pin_GPIO_AD_B0_12),
};

LPSPI_Type *mcu_spi_banks[] = { LPSPI1, LPSPI2, LPSPI3, LPSPI4 };

const mcu_periph_obj_t mcu_spi_sck_list[8] = {
    PERIPH_PIN(1, 3, kIOMUXC_LPSPI1_SCK_SELECT_INPUT, 0, &pin_GPIO_EMC_27),
    PERIPH_PIN(1, 4, kIOMUXC_LPSPI1_SCK_SELECT_INPUT, 1, &pin_GPIO_SD_B0_00),

    PERIPH_PIN(2, 4, kIOMUXC_LPSPI2_SCK_SELECT_INPUT, 0, &pin_GPIO_SD_B1_07),
    PERIPH_PIN(2, 2, kIOMUXC_LPSPI2_SCK_SELECT_INPUT, 1, &pin_GPIO_EMC_00),

    PERIPH_PIN(3, 7, kIOMUXC_LPSPI3_SCK_SELECT_INPUT, 0, &pin_GPIO_AD_B0_00),
    PERIPH_PIN(3, 2, kIOMUXC_LPSPI3_SCK_SELECT_INPUT, 1, &pin_GPIO_AD_B1_15),

    PERIPH_PIN(4, 3, kIOMUXC_LPSPI4_SCK_SELECT_INPUT, 0, &pin_GPIO_B0_03),
    PERIPH_PIN(4, 1, kIOMUXC_LPSPI4_SCK_SELECT_INPUT, 1, &pin_GPIO_B1_07),
};

const mcu_periph_obj_t mcu_spi_mosi_list[8] = {
    PERIPH_PIN(1, 3, kIOMUXC_LPSPI1_SDO_SELECT_INPUT, 0, &pin_GPIO_EMC_28),
    PERIPH_PIN(1, 4, kIOMUXC_LPSPI1_SDO_SELECT_INPUT, 1, &pin_GPIO_SD_B0_02),

    PERIPH_PIN(2, 4, kIOMUXC_LPSPI2_SDO_SELECT_INPUT, 0, &pin_GPIO_SD_B1_08),
    PERIPH_PIN(2, 2, kIOMUXC_LPSPI2_SDO_SELECT_INPUT, 1, &pin_GPIO_EMC_02),

    PERIPH_PIN(3, 7, kIOMUXC_LPSPI3_SDO_SELECT_INPUT, 0, &pin_GPIO_AD_B0_01),
    PERIPH_PIN(3, 2, kIOMUXC_LPSPI3_SDO_SELECT_INPUT, 1, &pin_GPIO_AD_B1_14),

    PERIPH_PIN(4, 3, kIOMUXC_LPSPI4_SDO_SELECT_INPUT, 0, &pin_GPIO_B0_02),
    PERIPH_PIN(4, 1, kIOMUXC_LPSPI4_SDO_SELECT_INPUT, 1, &pin_GPIO_B1_06),
};

const mcu_periph_obj_t mcu_spi_miso_list[8] = {
    PERIPH_PIN(1, 3, kIOMUXC_LPSPI1_SDI_SELECT_INPUT, 0, &pin_GPIO_EMC_29),
    PERIPH_PIN(1, 4, kIOMUXC_LPSPI1_SDI_SELECT_INPUT, 1, &pin_GPIO_SD_B0_03),

    PERIPH_PIN(2, 4, kIOMUXC_LPSPI2_SDI_SELECT_INPUT, 0, &pin_GPIO_SD_B1_09),
    PERIPH_PIN(2, 2, kIOMUXC_LPSPI2_SDI_SELECT_INPUT, 1, &pin_GPIO_EMC_03),

    PERIPH_PIN(3, 7, kIOMUXC_LPSPI3_SDI_SELECT_INPUT, 0, &pin_GPIO_AD_B0_02),
    PERIPH_PIN(3, 2, kIOMUXC_LPSPI3_SDI_SELECT_INPUT, 1, &pin_GPIO_AD_B1_13),

    PERIPH_PIN(4, 3, kIOMUXC_LPSPI4_SDI_SELECT_INPUT, 0, &pin_GPIO_B0_01),
    PERIPH_PIN(4, 1, kIOMUXC_LPSPI4_SDI_SELECT_INPUT, 1, &pin_GPIO_B1_05),
};

LPUART_Type *mcu_uart_banks[] = { LPUART1, LPUART2, LPUART3, LPUART4, LPUART5, LPUART6, LPUART7, LPUART8 };

const mcu_periph_obj_t mcu_uart_rx_list[18] = {
    PERIPH_PIN(1, 2, 0, 0, &pin_GPIO_AD_B0_13),

    PERIPH_PIN(2, 2, kIOMUXC_LPUART2_RX_SELECT_INPUT, 0, &pin_GPIO_SD_B1_10),
    PERIPH_PIN(2, 2, kIOMUXC_LPUART2_RX_SELECT_INPUT, 1, &pin_GPIO_AD_B1_03),

    PERIPH_PIN(3, 2, kIOMUXC_LPUART3_RX_SELECT_INPUT, 0, &pin_GPIO_AD_B1_07),
    PERIPH_PIN(3, 2, kIOMUXC_LPUART3_RX_SELECT_INPUT, 1, &pin_GPIO_EMC_14),
    PERIPH_PIN(3, 3, kIOMUXC_LPUART3_RX_SELECT_INPUT, 2, &pin_GPIO_B0_09),

    PERIPH_PIN(4, 4, kIOMUXC_LPUART4_RX_SELECT_INPUT, 0, &pin_GPIO_SD_B1_01),
    PERIPH_PIN(4, 2, kIOMUXC_LPUART4_RX_SELECT_INPUT, 1, &pin_GPIO_EMC_20),
    PERIPH_PIN(4, 2, kIOMUXC_LPUART4_RX_SELECT_INPUT, 2, &pin_GPIO_B1_01),

    PERIPH_PIN(5, 2, kIOMUXC_LPUART5_RX_SELECT_INPUT, 0, &pin_GPIO_EMC_24),
    PERIPH_PIN(5, 1, kIOMUXC_LPUART5_RX_SELECT_INPUT, 1, &pin_GPIO_B1_13),

    PERIPH_PIN(6, 2, kIOMUXC_LPUART6_RX_SELECT_INPUT, 0, &pin_GPIO_EMC_26),
    PERIPH_PIN(6, 2, kIOMUXC_LPUART6_RX_SELECT_INPUT, 1, &pin_GPIO_AD_B0_03),

    PERIPH_PIN(7, 2, kIOMUXC_LPUART7_RX_SELECT_INPUT, 0, &pin_GPIO_SD_B1_09),
    PERIPH_PIN(7, 2, kIOMUXC_LPUART7_RX_SELECT_INPUT, 1, &pin_GPIO_EMC_32),

    PERIPH_PIN(8, 2, kIOMUXC_LPUART8_RX_SELECT_INPUT, 0, &pin_GPIO_SD_B0_05),
    PERIPH_PIN(8, 2, kIOMUXC_LPUART8_RX_SELECT_INPUT, 1, &pin_GPIO_AD_B1_11),
    PERIPH_PIN(8, 2, kIOMUXC_LPUART8_RX_SELECT_INPUT, 2, &pin_GPIO_EMC_39),
};

const mcu_periph_obj_t mcu_uart_tx_list[18] = {
    PERIPH_PIN(1, 2, 0, 0, &pin_GPIO_AD_B0_12),

    PERIPH_PIN(2, 2, kIOMUXC_LPUART2_TX_SELECT_INPUT, 0, &pin_GPIO_SD_B1_11),
    PERIPH_PIN(2, 2, kIOMUXC_LPUART2_TX_SELECT_INPUT, 1, &pin_GPIO_AD_B1_02),

    PERIPH_PIN(3, 2, kIOMUXC_LPUART3_TX_SELECT_INPUT, 0, &pin_GPIO_AD_B1_06),
    PERIPH_PIN(3, 2, kIOMUXC_LPUART3_TX_SELECT_INPUT, 1, &pin_GPIO_EMC_13),
    PERIPH_PIN(3, 3, kIOMUXC_LPUART3_TX_SELECT_INPUT, 2, &pin_GPIO_B0_08),

    PERIPH_PIN(4, 4, kIOMUXC_LPUART4_TX_SELECT_INPUT, 0, &pin_GPIO_SD_B1_00),
    PERIPH_PIN(4, 2, kIOMUXC_LPUART4_TX_SELECT_INPUT, 1, &pin_GPIO_EMC_19),
    PERIPH_PIN(4, 2, kIOMUXC_LPUART4_TX_SELECT_INPUT, 2, &pin_GPIO_B1_00),

    PERIPH_PIN(5, 2, kIOMUXC_LPUART5_TX_SELECT_INPUT, 0, &pin_GPIO_EMC_23),
    PERIPH_PIN(5, 1, kIOMUXC_LPUART5_TX_SELECT_INPUT, 1, &pin_GPIO_B1_12),

    PERIPH_PIN(6, 2, kIOMUXC_LPUART6_TX_SELECT_INPUT, 0, &pin_GPIO_EMC_25),
    PERIPH_PIN(6, 2, kIOMUXC_LPUART6_TX_SELECT_INPUT, 1, &pin_GPIO_AD_B0_02),

    PERIPH_PIN(7, 2, kIOMUXC_LPUART7_TX_SELECT_INPUT, 0, &pin_GPIO_SD_B1_08),
    PERIPH_PIN(7, 2, kIOMUXC_LPUART7_TX_SELECT_INPUT, 1, &pin_GPIO_EMC_31),

    PERIPH_PIN(8, 2, kIOMUXC_LPUART8_TX_SELECT_INPUT, 0, &pin_GPIO_SD_B0_04),
    PERIPH_PIN(8, 2, kIOMUXC_LPUART8_TX_SELECT_INPUT, 1, &pin_GPIO_AD_B1_10),
    PERIPH_PIN(8, 2, kIOMUXC_LPUART8_TX_SELECT_INPUT, 2, &pin_GPIO_EMC_38),
};

const mcu_periph_obj_t mcu_uart_rts_list[9] = {
    PERIPH_PIN(1, 2, 0, 0, &pin_GPIO_AD_B0_15),
   
    PERIPH_PIN(2, 2, 0, 0, &pin_GPIO_AD_B1_01),

    PERIPH_PIN(3, 2, 0, 0, &pin_GPIO_AD_B1_05),
    PERIPH_PIN(3, 2, 0, 0, &pin_GPIO_EMC_16),

    PERIPH_PIN(4, 2, 0, 0, &pin_GPIO_EMC_18),

    PERIPH_PIN(5, 2, 0, 0, &pin_GPIO_EMC_27),

    PERIPH_PIN(6, 2, 0, 0, &pin_GPIO_EMC_29),

    PERIPH_PIN(7, 2, 0, 0, &pin_GPIO_SD_B1_07),

    PERIPH_PIN(8, 2, 0, 0, &pin_GPIO_SD_B0_03),
};

const mcu_periph_obj_t mcu_uart_cts_list[9] = {
    PERIPH_PIN(1, 2, 0, 0, &pin_GPIO_AD_B0_14),

    PERIPH_PIN(2, 2, 0, 0, &pin_GPIO_AD_B1_00),

    PERIPH_PIN(3, 2, kIOMUXC_LPUART3_CTS_B_SELECT_INPUT, 0, &pin_GPIO_EMC_15),
    PERIPH_PIN(3, 2, kIOMUXC_LPUART3_CTS_B_SELECT_INPUT, 1, &pin_GPIO_AD_B1_04),    

    PERIPH_PIN(4, 2, 0, 0, &pin_GPIO_EMC_17),

    PERIPH_PIN(5, 2, 0, 0, &pin_GPIO_EMC_28),

    PERIPH_PIN(6, 2, 0, 0, &pin_GPIO_EMC_30),

    PERIPH_PIN(7, 2, 0, 0, &pin_GPIO_SD_B1_06),

    PERIPH_PIN(8, 2, 0, 0, &pin_GPIO_SD_B0_02),
};

const mcu_pwm_obj_t mcu_pwm_list[67] = {
    PWM_PIN(PWM1, kPWM_Module_0, kPWM_PwmA, 1, &pin_GPIO_EMC_23),
    PWM_PIN(PWM1, kPWM_Module_0, kPWM_PwmA, 1, &pin_GPIO_SD_B0_00),

    PWM_PIN(PWM1, kPWM_Module_0, kPWM_PwmB, 1, &pin_GPIO_EMC_24),
    PWM_PIN(PWM1, kPWM_Module_0, kPWM_PwmB, 1, &pin_GPIO_SD_B0_01),

    PWM_PIN(PWM1, kPWM_Module_0, kPWM_PwmX, 4, &pin_GPIO_AD_B0_02),

    PWM_PIN(PWM1, kPWM_Module_1, kPWM_PwmA, 1, &pin_GPIO_EMC_25),
    PWM_PIN(PWM1, kPWM_Module_1, kPWM_PwmA, 1, &pin_GPIO_SD_B0_02),

    PWM_PIN(PWM1, kPWM_Module_1, kPWM_PwmB, 1, &pin_GPIO_EMC_26),
    PWM_PIN(PWM1, kPWM_Module_1, kPWM_PwmB, 1, &pin_GPIO_SD_B0_03),

    PWM_PIN(PWM1, kPWM_Module_1, kPWM_PwmX, 4, &pin_GPIO_AD_B0_03),

    PWM_PIN(PWM1, kPWM_Module_2, kPWM_PwmA, 1, &pin_GPIO_EMC_27),
    PWM_PIN(PWM1, kPWM_Module_2, kPWM_PwmA, 1, &pin_GPIO_SD_B0_04),

    PWM_PIN(PWM1, kPWM_Module_2, kPWM_PwmB, 1, &pin_GPIO_EMC_28),
    PWM_PIN(PWM1, kPWM_Module_2, kPWM_PwmB, 1, &pin_GPIO_SD_B0_05),

    PWM_PIN(PWM1, kPWM_Module_2, kPWM_PwmX, 4, &pin_GPIO_AD_B0_12),

    PWM_PIN(PWM1, kPWM_Module_3, kPWM_PwmA, 1, &pin_GPIO_AD_B0_10),
    PWM_PIN(PWM1, kPWM_Module_3, kPWM_PwmA, 1, &pin_GPIO_EMC_38),
    PWM_PIN(PWM1, kPWM_Module_3, kPWM_PwmA, 2, &pin_GPIO_SD_B1_00),
    PWM_PIN(PWM1, kPWM_Module_3, kPWM_PwmA, 4, &pin_GPIO_EMC_12),
    PWM_PIN(PWM1, kPWM_Module_3, kPWM_PwmA, 6, &pin_GPIO_B1_00),

    PWM_PIN(PWM1, kPWM_Module_3, kPWM_PwmB, 1, &pin_GPIO_AD_B0_11),
    PWM_PIN(PWM1, kPWM_Module_3, kPWM_PwmB, 1, &pin_GPIO_EMC_39),
    PWM_PIN(PWM1, kPWM_Module_3, kPWM_PwmB, 2, &pin_GPIO_SD_B1_01),
    PWM_PIN(PWM1, kPWM_Module_3, kPWM_PwmB, 4, &pin_GPIO_EMC_13),
    PWM_PIN(PWM1, kPWM_Module_3, kPWM_PwmB, 6, &pin_GPIO_B1_01),

    PWM_PIN(PWM1, kPWM_Module_3, kPWM_PwmX, 4, &pin_GPIO_AD_B0_13),

    PWM_PIN(PWM2, kPWM_Module_0, kPWM_PwmA, 1, &pin_GPIO_EMC_06),
    PWM_PIN(PWM2, kPWM_Module_0, kPWM_PwmA, 2, &pin_GPIO_B0_06),

    PWM_PIN(PWM2, kPWM_Module_0, kPWM_PwmB, 1, &pin_GPIO_EMC_07),
    PWM_PIN(PWM2, kPWM_Module_0, kPWM_PwmB, 2, &pin_GPIO_B0_07),

    PWM_PIN(PWM2, kPWM_Module_1, kPWM_PwmA, 1, &pin_GPIO_EMC_08),
    PWM_PIN(PWM2, kPWM_Module_1, kPWM_PwmA, 2, &pin_GPIO_B0_08),

    PWM_PIN(PWM2, kPWM_Module_1, kPWM_PwmB, 1, &pin_GPIO_EMC_09),
    PWM_PIN(PWM2, kPWM_Module_1, kPWM_PwmB, 2, &pin_GPIO_B0_09),

    PWM_PIN(PWM2, kPWM_Module_2, kPWM_PwmA, 1, &pin_GPIO_EMC_10),
    PWM_PIN(PWM2, kPWM_Module_2, kPWM_PwmA, 2, &pin_GPIO_B0_10),

    PWM_PIN(PWM2, kPWM_Module_2, kPWM_PwmB, 1, &pin_GPIO_EMC_11),
    PWM_PIN(PWM2, kPWM_Module_2, kPWM_PwmB, 2, &pin_GPIO_B0_11),

    PWM_PIN(PWM2, kPWM_Module_3, kPWM_PwmA, 0, &pin_GPIO_AD_B0_00),
    PWM_PIN(PWM2, kPWM_Module_3, kPWM_PwmA, 1, &pin_GPIO_AD_B0_09),
    PWM_PIN(PWM2, kPWM_Module_3, kPWM_PwmA, 1, &pin_GPIO_EMC_19),
    PWM_PIN(PWM2, kPWM_Module_3, kPWM_PwmA, 2, &pin_GPIO_SD_B1_02),
    PWM_PIN(PWM2, kPWM_Module_3, kPWM_PwmA, 6, &pin_GPIO_B1_02),

    PWM_PIN(PWM2, kPWM_Module_3, kPWM_PwmB, 0, &pin_GPIO_AD_B0_01),
    PWM_PIN(PWM2, kPWM_Module_3, kPWM_PwmB, 1, &pin_GPIO_EMC_20),
    PWM_PIN(PWM2, kPWM_Module_3, kPWM_PwmB, 2, &pin_GPIO_SD_B1_03),
    PWM_PIN(PWM2, kPWM_Module_3, kPWM_PwmB, 6, &pin_GPIO_B1_03),

    PWM_PIN(PWM3, kPWM_Module_0, kPWM_PwmA, 1, &pin_GPIO_EMC_29),

    PWM_PIN(PWM3, kPWM_Module_0, kPWM_PwmB, 1, &pin_GPIO_EMC_30),

    PWM_PIN(PWM3, kPWM_Module_1, kPWM_PwmA, 1, &pin_GPIO_EMC_31),

    PWM_PIN(PWM3, kPWM_Module_1, kPWM_PwmB, 1, &pin_GPIO_EMC_32),

    PWM_PIN(PWM3, kPWM_Module_2, kPWM_PwmA, 1, &pin_GPIO_EMC_33),

    PWM_PIN(PWM3, kPWM_Module_2, kPWM_PwmB, 1, &pin_GPIO_EMC_34),

    PWM_PIN(PWM3, kPWM_Module_3, kPWM_PwmA, 1, &pin_GPIO_EMC_21),

    PWM_PIN(PWM3, kPWM_Module_3, kPWM_PwmB, 1, &pin_GPIO_EMC_22),

    PWM_PIN(PWM4, kPWM_Module_0, kPWM_PwmA, 1, &pin_GPIO_AD_B1_08),
    PWM_PIN(PWM4, kPWM_Module_0, kPWM_PwmA, 1, &pin_GPIO_EMC_00),

    PWM_PIN(PWM4, kPWM_Module_0, kPWM_PwmB, 1, &pin_GPIO_EMC_01),

    PWM_PIN(PWM4, kPWM_Module_1, kPWM_PwmA, 1, &pin_GPIO_AD_B1_09),
    PWM_PIN(PWM4, kPWM_Module_1, kPWM_PwmA, 1, &pin_GPIO_EMC_02),

    PWM_PIN(PWM4, kPWM_Module_1, kPWM_PwmB, 1, &pin_GPIO_EMC_03),

    PWM_PIN(PWM4, kPWM_Module_2, kPWM_PwmA, 1, &pin_GPIO_B1_14),
    PWM_PIN(PWM4, kPWM_Module_2, kPWM_PwmA, 1, &pin_GPIO_EMC_04),

    PWM_PIN(PWM4, kPWM_Module_2, kPWM_PwmB, 1, &pin_GPIO_EMC_05),

    PWM_PIN(PWM4, kPWM_Module_3, kPWM_PwmA, 1, &pin_GPIO_B1_15),
    PWM_PIN(PWM4, kPWM_Module_3, kPWM_PwmA, 1, &pin_GPIO_EMC_17),

    PWM_PIN(PWM4, kPWM_Module_3, kPWM_PwmB, 1, &pin_GPIO_EMC_18),

};
