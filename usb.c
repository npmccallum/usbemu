#include <endian.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

enum usb_device_speed {
    USB_SPEED_UNKNOWN = 0,               /* enumerating */
    USB_SPEED_LOW, USB_SPEED_FULL,       /* usb 1.1 */
    USB_SPEED_HIGH,                      /* usb 2.0 */
    USB_SPEED_WIRELESS,                  /* wireless (usb 2.5) */
    USB_SPEED_SUPER,                     /* usb 3.0 */
    USB_SPEED_SUPER_PLUS,                /* usb 3.1 */
};

enum {
    USBIP_DIR_IN = 0,
    USBIP_DIR_OUT = 1,
};

enum {
    USBIP_CMD_SUBMIT = 1,
    USBIP_CMD_UNLINK = 2,
    USBIP_RET_SUBMIT = 3,
    USBIP_RET_UNLINK = 4
};

enum {
    USBIP_URB_NONE                = 0,
    USBIP_URB_SHORT_NOT_OK        = 1 << 0,
    USBIP_URB_ISO_ASAP            = 1 << 1,
    USBIP_URB_NO_TRANSFER_DMA_MAP = 1 << 2,
    USBIP_URB_NO_FSBR             = 1 << 5,
    USBIP_URB_ZERO_PACKET         = 1 << 6,
    USBIP_URB_NO_INTERRUPT        = 1 << 7,
    USBIP_URB_FREE_BUFFER         = 1 << 8,
    USBIP_URB_DIR_MASK            = 1 << 9,
};

struct usbip_submit_setup {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed));

struct usbip_submit_cmd {
    uint32_t transfer_flags;
    uint32_t transfer_buffer_length;
    uint32_t start_frame;
    uint32_t number_of_packets;
    uint32_t interval;
    struct usbip_submit_setup setup;
    uint8_t data[];
} __attribute__((packed));

struct usbip_submit_ret {
    uint32_t status;
    uint32_t actual_length;
    uint32_t start_frame;
    uint32_t number_of_packets;
    uint32_t error_count;
    struct usbip_submit_setup setup;
    uint8_t data[];
} __attribute__((packed));

struct usbip_unlink_cmd {
    uint32_t seqnum;
} __attribute__((packed));

struct usbip_unlink_ret {
    uint32_t status;
} __attribute__((packed));

struct usbip {
    uint32_t command;
    uint32_t seqnum;
    uint32_t devid;
    uint32_t direction;
    uint32_t endpoint;

    union {
        union {
            struct usbip_submit_cmd submit;
            struct usbip_unlink_cmd unlink;
        } cmd;

        union {
            struct usbip_submit_ret submit;
            struct usbip_unlink_ret unlink;
        } ret;
    };

    uint8_t padding[65488];
} __attribute__((packed));

#define SETUP_DIR(rt) ((rt) & 0b10000000 >> 7)
#define SETUP_TYP(rt) ((rt) & 0b01100000 >> 5)
#define SETUP_RCP(rt) ((rt) & 0b00011111 >> 0)

static const char *
usbip_setup_dir_str(uint8_t rt)
{
    return SETUP_DIR(rt) ? "D2H" : "H2D";
}

static const char *
usbip_setup_typ_str(uint8_t rt)
{
    switch (SETUP_TYP(rt)) {
    case 0:  return "standard";
    case 1:  return "class";
    case 2:  return "vendor";
    default: return "<reserved>";
    }
}

static const char *
usbip_setup_rcp_str(uint8_t rt)
{
    switch (SETUP_RCP(rt)) {
    case 0:  return "device";
    case 1:  return "interface";
    case 2:  return "endpoint";
    case 3:  return "other";
    default: return "<reserved>";
    }
}

static const char *
usbip_setup_req_str(uint8_t req)
{
    switch (req) {
    case 0:  return "GET_STATUS";
    case 1:  return "CLEAR_FEATURE";
    case 3:  return "SET_FEATURE";
    case 5:  return "SET_ADDRESS";
    case 6:  return "GET_DESCRIPTOR";
    case 7:  return "SET_DESCRIPTOR";
    case 8:  return "GET_CONFIGURATION";
    case 9:  return "SET_CONFIGURATION";
    case 10: return "GET_INTERFACE";
    case 11: return "SET_INTERFACE";
    case 12: return "SYNCH_FRAME";
    default: return "<reserved>";
    }
}

static void
usbip_dump(const struct usbip *u, FILE *f)
{
    fprintf(f, "{\n");
    fprintf(f, "  .command = %u\n",   u->command);
    fprintf(f, "  .seqnum = %u\n",    u->seqnum);
    fprintf(f, "  .devid = %u\n",     u->devid);
    fprintf(f, "  .direction = %u\n", u->direction);
    fprintf(f, "  .endpoint = %u\n",  u->endpoint);

    switch (u->command) {
    case USBIP_CMD_SUBMIT:
        fprintf(f, "  .cmd.submit.transfer_flags = 0x%08X\n",     u->cmd.submit.transfer_flags);
        fprintf(f, "  .cmd.submit.transfer_buffer_length = %u\n", u->cmd.submit.transfer_buffer_length);
        fprintf(f, "  .cmd.submit.start_frame = %u\n",            u->cmd.submit.start_frame);
        fprintf(f, "  .cmd.submit.number_of_packets = %u\n",      u->cmd.submit.number_of_packets);
        fprintf(f, "  .cmd.submit.interval = %u\n",               u->cmd.submit.interval);
        fprintf(f, "  .cmd.submit.setup.direction = %s\n",        usbip_setup_dir_str(u->cmd.submit.setup.bmRequestType));
        fprintf(f, "  .cmd.submit.setup.type = %s\n",             usbip_setup_typ_str(u->cmd.submit.setup.bmRequestType));
        fprintf(f, "  .cmd.submit.setup.recipient = %s\n",        usbip_setup_rcp_str(u->cmd.submit.setup.bmRequestType));
        fprintf(f, "  .cmd.submit.setup.bRequest = %s\n",         usbip_setup_req_str(u->cmd.submit.setup.bRequest));
        fprintf(f, "  .cmd.submit.setup.wValue = %hu\n",          u->cmd.submit.setup.wValue);
        fprintf(f, "  .cmd.submit.setup.wIndex = %hu\n",          u->cmd.submit.setup.wIndex);
        fprintf(f, "  .cmd.submit.setup.wLength = %hu\n",         u->cmd.submit.setup.wLength);

        fprintf(f, "  .cmd.submit.data[] = {");
        for (size_t i = 0; i < u->cmd.submit.transfer_buffer_length; i++)
            fprintf(f, "%s%02x", i % 32 == 0 ? "\n    " : "", u->cmd.submit.data[i]);
        fprintf(f, "\n  }\n");

        break;

    case USBIP_CMD_UNLINK:
        fprintf(f, "  .cmd.unlink.seqnum = %u\n",                 u->cmd.unlink.seqnum);
        break;

    case USBIP_RET_SUBMIT:
        fprintf(f, "  .ret.submit.status = %u\n",                 u->ret.submit.status);
        fprintf(f, "  .ret.submit.actual_length = %u\n",          u->ret.submit.actual_length);
        fprintf(f, "  .ret.submit.start_frame = %u\n",            u->ret.submit.start_frame);
        fprintf(f, "  .ret.submit.number_of_packets = %u\n",      u->ret.submit.number_of_packets);
        fprintf(f, "  .ret.submit.error_count = %u\n",            u->ret.submit.error_count);
        fprintf(f, "  .ret.submit.setup.bmRequestType = %hhu\n",  u->ret.submit.setup.bmRequestType);
        fprintf(f, "  .ret.submit.setup.bRequest = %hhu\n",       u->ret.submit.setup.bRequest);
        fprintf(f, "  .ret.submit.setup.wValue = %hu\n",          u->ret.submit.setup.wValue);
        fprintf(f, "  .ret.submit.setup.wIndex = %hu\n",          u->ret.submit.setup.wIndex);
        fprintf(f, "  .ret.submit.setup.wLength = %hu\n",         u->ret.submit.setup.wLength);

        fprintf(f, "  .ret.submit.data[] = {");
        for (size_t i = 0; i < u->ret.submit.actual_length; i++)
            fprintf(f, "%s%02x", i % 32 == 0 ? "\n    " : "", u->ret.submit.data[i]);
        fprintf(f, "\n  }\n");

        break;

    case USBIP_RET_UNLINK:
        fprintf(f, "  .ret.unlink.status = %u\n", u->ret.unlink.status);
        break;
    }

    fprintf(f, "}\n");
}

static int
usbip_ntoh(struct usbip *u)
{
    u->command = be32toh(u->command);
    u->seqnum = be32toh(u->seqnum);
    u->devid = be32toh(u->devid);
    u->direction = be32toh(u->direction);
    u->endpoint = be32toh(u->endpoint);

    switch (u->command) {
    case USBIP_CMD_SUBMIT:
        u->cmd.submit.transfer_flags = be32toh(u->cmd.submit.transfer_flags);
        u->cmd.submit.transfer_buffer_length = be32toh(u->cmd.submit.transfer_buffer_length);
        u->cmd.submit.start_frame = be32toh(u->cmd.submit.start_frame);
        u->cmd.submit.number_of_packets = be32toh(u->cmd.submit.number_of_packets);
        u->cmd.submit.interval = be32toh(u->cmd.submit.interval);    
        u->cmd.submit.setup.wValue = le16toh(u->cmd.submit.setup.wValue);
        u->cmd.submit.setup.wIndex = le16toh(u->cmd.submit.setup.wIndex);
        u->cmd.submit.setup.wLength = le16toh(u->cmd.submit.setup.wLength);
        return 0;

    case USBIP_CMD_UNLINK:
        u->cmd.unlink.seqnum = be32toh(u->cmd.unlink.seqnum);
        return 0;

    case USBIP_RET_SUBMIT:
        u->ret.submit.status = be32toh(u->ret.submit.status);
        u->ret.submit.actual_length = be32toh(u->ret.submit.actual_length);
        u->ret.submit.start_frame = be32toh(u->ret.submit.start_frame);
        u->ret.submit.number_of_packets = be32toh(u->ret.submit.number_of_packets);
        u->ret.submit.error_count = be32toh(u->ret.submit.error_count);    
        u->ret.submit.setup.wValue = le16toh(u->ret.submit.setup.wValue);
        u->ret.submit.setup.wIndex = le16toh(u->ret.submit.setup.wIndex);
        u->ret.submit.setup.wLength = le16toh(u->ret.submit.setup.wLength);
        return 0;

    case USBIP_RET_UNLINK:
        u->ret.unlink.status = be32toh(u->ret.unlink.status);
        return 0;

    default:
        return ENOTSUP;
    }
}

int
main()
{
    struct usbip usbip = {};
    int socks[2] = { -1, -1 };
    FILE *file = NULL;
    int r = 0;

    r = socketpair(AF_UNIX, SOCK_DGRAM, 0, socks);
    if (r != 0)
        return EXIT_FAILURE;

    file = fopen("/sys/devices/platform/vhci_hcd/attach", "w");
    if (!file)
        return EXIT_FAILURE;

    fprintf(file, "%u %d %u %u", 0, socks[0], 2, USB_SPEED_FULL);
    fclose(file);

    close(socks[0]);

    recv(socks[1], &usbip, sizeof(usbip), 0);
    usbip_ntoh(&usbip);
    usbip_dump(&usbip, stderr);

    close(socks[1]);
    return EXIT_SUCCESS;
}
