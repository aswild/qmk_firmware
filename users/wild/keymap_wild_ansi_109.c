/*
 * Common keymap for keychron ansi_109 layouts.
 * This file isn't compiled directly, but instead should be #included from
 * a keyboards/.../keymaps/wild/keymap.c file.
 *
 * Copyright 2026 Allen Wild <allenwild93@gmail.com>
 *
 * Copyright 2024 ~ 2026 @ Keychron (https://www.keychron.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include QMK_KEYBOARD_H
#include "bootloader.h"
#include "bootmagic.h"
#include "keychron_common.h"
#include "keychron_rgb_type.h"
#include "rgb_matrix.h"

#if !(defined(LK_WIRELESS_ENABLE) || defined(KC_BLUETOOTH_ENABLE))
#define BT_HST1 KC_TRANSPARENT
#define BT_HST2 KC_TRANSPARENT
#define BT_HST3 KC_TRANSPARENT
#define P2P4G KC_TRANSPARENT
#define BAT_LVL KC_TRANSPARENT
#endif

enum wild_keycodes {
    W_RGBCTL = SAFE_RANGE,
    W_ENCFN,
    W_ENCDN,
    W_ENCUP,
    W_ENCDNFN,
    W_ENCUPFN,
};

enum layers {
    MAC_BASE,
    MAC_FN,
    WIN_BASE,
    WIN_FN,
};

#define FN_MAC MO(MAC_FN)
#define FN_WIN MO(WIN_FN)

// Modifier flattening - get a version of the current modifiers, ignoring left/right variations.
// This lets you do (mods == M_CTRL) to see if *only* left and/or right control modifier is set,
// but not shift or something else.
#define M_CTRL  (1 << 0)
#define M_ALT   (1 << 1)
#define M_SHIFT (1 << 2)
#define M_WIN   (1 << 3)

static uint8_t get_mods_flat(void) {
    uint8_t mods = get_mods();
    unsigned int m = 0;
    if (mods & MOD_MASK_CTRL) {
        m |= M_CTRL;
    }
    if (mods & MOD_MASK_ALT) {
        m |= M_ALT;
    }
    if (mods & MOD_MASK_SHIFT) {
        m |= M_SHIFT;
    }
    if (mods & MOD_MASK_GUI) {
        m |= M_WIN;
    }
    return m;
}

static uint8_t u8_sat_add(uint8_t a, int b) {
    int x = (int)a + b;
    if (x < 0)
        return 0;
    if (x > UINT8_MAX)
        return UINT8_MAX;
    return x;
}

// Fn+Knob changes indicator (numlock/capslock) color
// Ctrl: hue
// Alt: saturation
// Shift: brightness
//
// dir is -1 (decrease) or 1 (increase)
static void indicator_knob(int dir) {
    switch (get_mods_flat())
    {
        case M_CTRL:
            os_ind_cfg.hsv.h += RGB_MATRIX_HUE_STEP * dir; // wraps around
            break;

        case M_ALT:
            os_ind_cfg.hsv.s = u8_sat_add(os_ind_cfg.hsv.s, RGB_MATRIX_SAT_STEP * dir);
            break;

        case M_SHIFT:
            os_ind_cfg.hsv.v = u8_sat_add(os_ind_cfg.hsv.v, RGB_MATRIX_VAL_STEP * dir);
            break;

        default:
            return;
    }

    kc_rgb_update_indicators();
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        switch (keycode)
        {
            case W_ENCFN:
            {
                uint8_t mods = get_mods();
                if (mods == MOD_BIT(KC_LALT)) {
#ifdef RGB_MATRIX_ENABLE
                    // Fn+LAlt+Knob -> jump to bootloader.
                    // Go red (low-level, bypass RGB task)
                    rgb_matrix_driver.set_color_all(255, 0, 0);
                    rgb_matrix_driver.flush();
#endif // RGB_MATRIX_ENABLE

                    // un-press alt and wait for the update to propagate. Idk how to
                    // definitely wait for "the host polled our key state again" so just sleep some.
                    unregister_code(KC_LALT);
                    chThdSleepMilliseconds(500);
                    // clear eeprom (bootmagic does this)
                    eeconfig_disable();
                    // Jump to bootloader. This calls NVIC_SystemReset and never returns.
                    bootloader_jump();
                }
                return false;
            }

            // RGB Control button.
            //
            // No modifiers: toggle on/off
            // Ctrl: next mode
            // Shift or Ctrl+Shift: prev mode
            // Alt: Pleasing Rainbow Mode 1
            // Ctrl+Alt: Pleasing Rainbow Mode 2
            // Alt+Shift: solid color
            // Ctrl+Alt+Shift: solid color and default
#ifdef RGB_MATRIX_ENABLE
            case W_RGBCTL:
            {
                switch (get_mods_flat())
                {
                    case 0:
                        rgb_matrix_toggle();
                        break;
                    case M_CTRL:
                        rgb_matrix_step();
                        break;
                    case M_SHIFT:
                    case M_CTRL | M_SHIFT:
                        rgb_matrix_step_reverse();
                        break;
                    case M_ALT:
                        rgb_matrix_mode(RGB_MATRIX_RAINBOW_BEACON);
                        rgb_matrix_set_speed(10);
                        break;
                    case M_CTRL | M_ALT:
                        rgb_matrix_mode(RGB_MATRIX_JELLYBEAN_RAINDROPS);
                        rgb_matrix_set_speed(10);
                        break;
                    case M_ALT | M_SHIFT:
                        rgb_matrix_mode(RGB_MATRIX_SOLID_COLOR);
                        break;
                    case M_CTRL | M_ALT | M_SHIFT:
                        eeconfig_update_rgb_matrix_default();
                        kc_rgb_reset_indicators();
                        break;
                }
                return false;
            }
#endif // RGB_MATRIX_ENABLE

            // Encoder knob (layer 2, without fn key)
            // No modifiers: volume
            // shift: RGB brightness (value)
            // ctrl: RGB hue
            // alt: RGB saturation
            // alt+shift: RGB speed
            case W_ENCDN:
            {
                switch (get_mods_flat())
                {
                    case 0:
                        SEND_STRING(SS_TAP(X_VOLD));
                        break;
#ifdef RGB_MATRIX_ENABLE
                    case M_SHIFT:
                        rgb_matrix_decrease_val();
                        break;
                    case M_CTRL:
                        rgb_matrix_decrease_hue();
                        break;
                    case M_ALT:
                        rgb_matrix_decrease_sat();
                        break;
                    case M_ALT | M_SHIFT:
                        rgb_matrix_decrease_speed();
                        break;
#endif // RGB_MATRIX_ENABLE
                }
                return false;
            }

            case W_ENCUP:
            {
                switch (get_mods_flat())
                {
                    case 0:
                        SEND_STRING(SS_TAP(X_VOLU));
                        break;
#ifdef RGB_MATRIX_ENABLE
                    case M_SHIFT:
                        rgb_matrix_increase_val();
                        break;
                    case M_CTRL:
                        rgb_matrix_increase_hue();
                        break;
                    case M_ALT:
                        rgb_matrix_increase_sat();
                        break;
                    case M_ALT | M_SHIFT:
                        rgb_matrix_increase_speed();
                        break;
#endif // RGB_MATRIX_ENABLE
                }
                return false;
            }

            case W_ENCDNFN:
                indicator_knob(-1);
                return false;

            case W_ENCUPFN:
                indicator_knob(1);
                return false;
        }
    }
    return true;
}

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [MAC_BASE] = LAYOUT_ansi_109(
        KC_ESC,   KC_BRID,  KC_BRIU,  KC_MCTRL, KC_LNPAD, UG_VALD,  UG_VALU,  KC_MPRV,  KC_MPLY,  KC_MNXT,  KC_MUTE,  KC_VOLD,  KC_VOLU,  KC_MUTE,  KC_SNAP,  KC_SIRI,  UG_NEXT,  KC_F13,   KC_F14,   KC_F15,   KC_F16,
        KC_GRV,   KC_1,     KC_2,     KC_3,     KC_4,     KC_5,     KC_6,     KC_7,     KC_8,     KC_9,     KC_0,     KC_MINS,  KC_EQL,   KC_BSPC,  KC_INS,   KC_HOME,  KC_PGUP,  KC_NUM,   KC_PSLS,  KC_PAST,  KC_PMNS,
        KC_TAB,   KC_Q,     KC_W,     KC_E,     KC_R,     KC_T,     KC_Y,     KC_U,     KC_I,     KC_O,     KC_P,     KC_LBRC,  KC_RBRC,  KC_BSLS,  KC_DEL,   KC_END,   KC_PGDN,  KC_P7,    KC_P8,    KC_P9,
        KC_CAPS,  KC_A,     KC_S,     KC_D,     KC_F,     KC_G,     KC_H,     KC_J,     KC_K,     KC_L,     KC_SCLN,  KC_QUOT,            KC_ENT,                                 KC_P4,    KC_P5,    KC_P6,    KC_PPLS,
        KC_LSFT,            KC_Z,     KC_X,     KC_C,     KC_V,     KC_B,     KC_N,     KC_M,     KC_COMM,  KC_DOT,   KC_SLSH,            KC_RSFT,            KC_UP,              KC_P1,    KC_P2,    KC_P3,
        KC_LCTL,  KC_LOPTN, KC_LCMMD,                               KC_SPC,                                 KC_RCMMD, KC_ROPTN, FN_MAC,   KC_RCTL,  KC_LEFT,  KC_DOWN,  KC_RGHT,  KC_P0,              KC_PDOT,  KC_PENT),

    [MAC_FN] = LAYOUT_ansi_109(
        _______,  KC_F1,    KC_F2,    KC_F3,    KC_F4,    KC_F5,    KC_F6,    KC_F7,    KC_F8,    KC_F9,    KC_F10,   KC_F11,   KC_F12,   UG_TOGG,  _______,  _______,  UG_TOGG,  _______,  _______,  _______,  _______,
        _______,  BT_HST1,  BT_HST2,  BT_HST3,  P2P4G,    _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,
        UG_TOGG,  UG_NEXT,  UG_VALU,  UG_HUEU,  UG_SATU,  UG_SPDU,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,
        _______,  UG_PREV,  UG_VALD,  UG_HUED,  UG_SATD,  UG_SPDD,  _______,  _______,  _______,  _______,  _______,  _______,            _______,                                _______,  _______,  _______,  _______,
        _______,            _______,  _______,  _______,  _______,  BAT_LVL,  _______,  _______,  _______,  _______,  _______,            _______,            _______,            _______,  _______,  _______,
        _______,  _______,  _______,                                _______,                                _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______),

    [WIN_BASE] = LAYOUT_ansi_109(
        KC_ESC,   KC_F1,    KC_F2,    KC_F3,    KC_F4,    KC_F5,    KC_F6,    KC_F7,    KC_F8,    KC_F9,    KC_F10,   KC_F11,   KC_F12,   LSG(KC_Z),KC_PSCR,  G(KC_L),  W_RGBCTL, KC_MPLY,  KC_MAIL,  KC_CALC,  G(KC_F13),
        KC_GRV,   KC_1,     KC_2,     KC_3,     KC_4,     KC_5,     KC_6,     KC_7,     KC_8,     KC_9,     KC_0,     KC_MINS,  KC_EQL,   KC_BSPC,  KC_INS,   KC_HOME,  KC_PGUP,  KC_NUM,   KC_PSLS,  KC_PAST,  KC_PMNS,
        KC_TAB,   KC_Q,     KC_W,     KC_E,     KC_R,     KC_T,     KC_Y,     KC_U,     KC_I,     KC_O,     KC_P,     KC_LBRC,  KC_RBRC,  KC_BSLS,  KC_DEL,   KC_END,   KC_PGDN,  KC_P7,    KC_P8,    KC_P9,
        KC_LCTL,  KC_A,     KC_S,     KC_D,     KC_F,     KC_G,     KC_H,     KC_J,     KC_K,     KC_L,     KC_SCLN,  KC_QUOT,            KC_ENT,                                 KC_P4,    KC_P5,    KC_P6,    KC_PPLS,
        KC_LSFT,            KC_Z,     KC_X,     KC_C,     KC_V,     KC_B,     KC_N,     KC_M,     KC_COMM,  KC_DOT,   KC_SLSH,            KC_RSFT,            KC_UP,              KC_P1,    KC_P2,    KC_P3,
        KC_LCTL,  KC_LWIN,  KC_LALT,                                KC_SPC,                                 KC_RALT,  KC_RWIN,  FN_WIN,   KC_RCTL,  KC_LEFT,  KC_DOWN,  KC_RGHT,  KC_P0,              KC_PDOT,  KC_PENT),

    [WIN_FN] = LAYOUT_ansi_109(
        _______,  KC_BRID,  KC_BRIU,  KC_TASK,  KC_FILE,  UG_VALD,  UG_VALU,  KC_MPRV,  KC_MPLY,  KC_MNXT,  KC_MUTE,  KC_VOLD,  KC_VOLU,  W_ENCFN,  _______,  KC_SLEP,  _______,  _______,  _______,  _______,  _______,
        _______,  BT_HST1,  BT_HST2,  BT_HST3,  P2P4G,    _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,
        UG_TOGG,  UG_NEXT,  UG_VALU,  UG_HUEU,  UG_SATU,  UG_SPDU,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,
        KC_CAPS,  UG_PREV,  UG_VALD,  UG_HUED,  UG_SATD,  UG_SPDD,  _______,  _______,  _______,  _______,  _______,  _______,            _______,                                _______,  _______,  _______,  _______,
        _______,            _______,  _______,  _______,  _______,  BAT_LVL,  _______,  _______,  _______,  _______,  _______,            _______,            _______,            _______,  _______,  _______,
        _______,  _______,  _______,                                _______,                                _______,  _______,  _______,  _______,  KC_WBAK,  _______,  KC_WFWD,  _______,            _______,  _______)
};
// clang-format on

#if defined(ENCODER_MAP_ENABLE)
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [MAC_BASE] = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
    [MAC_FN]   = {ENCODER_CCW_CW(UG_VALD, UG_VALU)},
    [WIN_BASE] = {ENCODER_CCW_CW(W_ENCDN, W_ENCUP)},
    [WIN_FN]   = {ENCODER_CCW_CW(W_ENCDNFN, W_ENCUPFN)},
};
#endif // ENCODER_MAP_ENABLE
