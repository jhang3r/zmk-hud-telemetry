#include "zmk_hud/telem_format.h"
#include <string.h>
#include <stdio.h>

static int fails;

#define OK(call, expect) do {                                  \
    char buf[128];                                             \
    int r = (call);                                            \
    if (r < 0 || strcmp(buf, expect) != 0) {                   \
        printf("FAIL: %s\n  got: [%s]\n  exp: [%s]\n",         \
               #call, r < 0 ? "<overflow>" : buf, expect);     \
        fails++;                                               \
    }                                                          \
} while (0)

int main(void)
{
    fails = 0;
    OK(telem_fmt_hello(buf, sizeof buf, "handwired_split"),
       "{\"t\":\"hello\",\"pv\":1,\"kb\":\"handwired_split\"}\n");
    { uint8_t L[2] = {0, 1};
      OK(telem_fmt_layers(buf, sizeof buf, L, 2), "{\"t\":\"lyr\",\"lyr\":[0,1]}\n"); }
    { uint8_t L[1] = {0};
      OK(telem_fmt_snapshot(buf, sizeof buf, L, 1, 82, 79, "ble", 1, true),
        "{\"t\":\"snapshot\",\"lyr\":[0],\"bat\":{\"c\":82,\"p\":79},\"ep\":{\"k\":\"ble\",\"pf\":1,\"on\":true}}\n"); }
    OK(telem_fmt_pos(buf, sizeof buf, 30, true), "{\"t\":\"pos\",\"p\":30,\"d\":true}\n");
    OK(telem_fmt_code(buf, sizeof buf, 30, 7, 80, false),
       "{\"t\":\"code\",\"p\":30,\"pg\":7,\"c\":80,\"d\":false}\n");
    OK(telem_fmt_batt(buf, sizeof buf, 'p', 78), "{\"t\":\"bat\",\"s\":\"p\",\"v\":78}\n");
    OK(telem_fmt_ep(buf, sizeof buf, "usb", -1, true), "{\"t\":\"ep\",\"k\":\"usb\",\"pf\":-1,\"on\":true}\n");

    /* overflow returns -1 */
    { char small[8]; int r = telem_fmt_hello(small, sizeof small, "handwired_split");
      if (r != -1) { printf("FAIL: overflow not detected (r=%d)\n", r); fails++; } }

    if (fails == 0) printf("ALL PASS\n");
    return fails ? 1 : 0;
}
