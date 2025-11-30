// buttons.c
#include "buttons.h"

#include <gpiod.h>
#include <string.h>

/* Match your existing gpio16_17.c setup */
#define BTN_CHIP_PATH   "/dev/gpiochip2"
#define OFF_GPIO16      7   /* A button */
#define OFF_GPIO17      8   /* B button */
#define OFF_GPIO15      13  /* Start button */

/* active-low or active-high depends on wiring; we’ll interpret
 * "ACTIVE" from libgpiod as "pressed".
 */

static struct gpiod_chip *btn_chip = NULL;
static struct gpiod_line_request *btn_req = NULL;

bool buttons_init(void)
{
    if (btn_req)
        return true;  // already initialised

    struct gpiod_line_settings *ls_in = NULL;
    struct gpiod_line_config  *lcfg  = NULL;
    struct gpiod_request_config *req = NULL;
    unsigned int offsets[3] = { OFF_GPIO16, OFF_GPIO17, OFF_GPIO15 };

    btn_chip = gpiod_chip_open(BTN_CHIP_PATH);
    if (!btn_chip) {
        return false;
    }

    ls_in = gpiod_line_settings_new();
    if (!ls_in) goto fail;
    if (gpiod_line_settings_set_direction(ls_in, GPIOD_LINE_DIRECTION_INPUT) < 0)
        goto fail;
    /* Optionally:
     * gpiod_line_settings_set_bias(ls_in, GPIOD_LINE_BIAS_PULL_UP);
     */

    lcfg = gpiod_line_config_new();
    if (!lcfg) goto fail;
    if (gpiod_line_config_add_line_settings(lcfg, offsets, 3, ls_in) < 0)
        goto fail;

    req = gpiod_request_config_new();
    if (!req) goto fail;
    gpiod_request_config_set_consumer(req, "gbe_buttons");

    btn_req = gpiod_chip_request_lines(btn_chip, req, lcfg);
    if (!btn_req) goto fail;

    gpiod_line_settings_free(ls_in);
    gpiod_line_config_free(lcfg);
    gpiod_request_config_free(req);
    return true;

fail:
    if (ls_in) gpiod_line_settings_free(ls_in);
    if (lcfg)  gpiod_line_config_free(lcfg);
    if (req)   gpiod_request_config_free(req);
    if (btn_req) {
        gpiod_line_request_release(btn_req);
        btn_req = NULL;
    }
    if (btn_chip) {
        gpiod_chip_close(btn_chip);
        btn_chip = NULL;
    }
    return false;
}

void buttons_poll(buttons_state_t *state)
{
    if (!state) return;

    state->start = state->a = state->b = false;

    if (!btn_req)
        return;  // not initialised

    enum gpiod_line_value vals[3];
    if (gpiod_line_request_get_values(btn_req, vals) < 0) {
        return;  // treat as all released on error
    }

    /* Interpret ACTIVE as pressed (true). Adjust if inverted. */
    bool a_pressed     = (vals[0] == GPIOD_LINE_VALUE_ACTIVE);
    bool b_pressed     = (vals[1] == GPIOD_LINE_VALUE_ACTIVE);
    bool start_pressed = (vals[2] == GPIOD_LINE_VALUE_ACTIVE);

    state->a     = a_pressed;
    state->b     = b_pressed;
    state->start = start_pressed;
}

void buttons_shutdown(void)
{
    if (btn_req) {
        gpiod_line_request_release(btn_req);
        btn_req = NULL;
    }
    if (btn_chip) {
        gpiod_chip_close(btn_chip);
        btn_chip = NULL;
    }
}
