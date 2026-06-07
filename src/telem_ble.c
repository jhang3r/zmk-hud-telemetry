#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/kernel.h>

#include "telem_transport.h"

/* Fixed 128-bit UUIDs for the HUD telemetry service + notify characteristic.
 * Service:  6b1d7e00-a1b2-4c3d-9e5f-0000feed0001
 * Notify:   6b1d7e00-a1b2-4c3d-9e5f-0000feed0002  */
#define HUD_SVC_UUID BT_UUID_128_ENCODE(0x6b1d7e00, 0xa1b2, 0x4c3d, 0x9e5f, 0x0000feed0001)
#define HUD_CHR_UUID BT_UUID_128_ENCODE(0x6b1d7e00, 0xa1b2, 0x4c3d, 0x9e5f, 0x0000feed0002)

static struct bt_uuid_128 hud_svc_uuid = BT_UUID_INIT_128(HUD_SVC_UUID);
static struct bt_uuid_128 hud_chr_uuid = BT_UUID_INIT_128(HUD_CHR_UUID);

static bool s_subscribed;

/* defined below; submitted from the CCC callback to send a snapshot off the
 * BLE callback context. */
void telem_ble_snapshot_work_submit(void);

static void hud_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    s_subscribed = (value == BT_GATT_CCC_NOTIFY);
    if (s_subscribed) {
        telem_ble_snapshot_work_submit();
    }
}

BT_GATT_SERVICE_DEFINE(hud_svc,
    BT_GATT_PRIMARY_SERVICE(&hud_svc_uuid),
    BT_GATT_CHARACTERISTIC(&hud_chr_uuid.uuid,
        BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_NONE,
        NULL, NULL, NULL),
    BT_GATT_CCC(hud_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* Zephyr accepts the characteristic declaration attr and notifies its value. */
#define HUD_VALUE_ATTR (&hud_svc.attrs[1])

static void snapshot_work_handler(struct k_work *w)
{
    ARG_UNUSED(w);
    telem_send_snapshot();
}
static K_WORK_DEFINE(snapshot_work, snapshot_work_handler);

void telem_ble_snapshot_work_submit(void)
{
    k_work_submit(&snapshot_work);
}

void telem_ble_init(void)
{
    /* service is registered statically via BT_GATT_SERVICE_DEFINE */
}

void telem_ble_write(const uint8_t *buf, size_t len)
{
    if (!s_subscribed) return;

    /* Chunk to a conservative 20-byte payload (default ATT MTU). The host
     * reassembles on '\n', so smaller chunks are correct, just chattier. */
    const size_t chunk = 20;
    size_t off = 0;
    while (off < len) {
        size_t n = (len - off > chunk) ? chunk : (len - off);
        int err = bt_gatt_notify(NULL, HUD_VALUE_ATTR, buf + off, n);
        if (err) break; /* not subscribed / buffers full: drop remainder */
        off += n;
    }
}
