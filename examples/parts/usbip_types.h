//ref https://github.com/torvalds/linux/blob/master/tools/usb/usbip
#define USBIP_SYSFS_PATH_MAX 256
#define USBIP_SYSFS_BUS_ID_SIZE	32

struct usbip_usb_interface {
	uint8_t bInterfaceClass;
	uint8_t bInterfaceSubClass;
	uint8_t bInterfaceProtocol;
	uint8_t padding;	/* alignment */
} __attribute__((packed));

struct usbip_usb_device {
	char path[USBIP_SYSFS_PATH_MAX];
	char busid[USBIP_SYSFS_BUS_ID_SIZE];

	uint32_t busnum;
	uint32_t devnum;
	uint32_t speed;

	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;

	uint8_t bDeviceClass;
	uint8_t bDeviceSubClass;
	uint8_t bDeviceProtocol;
	uint8_t bConfigurationValue;
	uint8_t bNumConfigurations;
	uint8_t bNumInterfaces;
} __attribute__((packed));


#define USBIP_PROTO_VERSION ((1<<8) | 6)
struct usbip_op_common {
	uint16_t version;

#define USBIP_OP_REQUEST (0x80 << 8)
#define USBIP_OP_REPLY (0x00 << 8)
	uint16_t code;

#define USBIP_ST_OK 0x00
#define USBIP_ST_NA 0x01
	uint32_t status;

} __attribute__((packed));

#define USBIP_OP_DEVLIST 0x05

struct usbip_op_devlist_request {
} __attribute__((packed));

struct usbip_op_devlist_reply {
	uint32_t ndev;
	/* followed by reply_extra[] */
} __attribute__((packed));

struct usbip_op_devlist_reply_extra {
	struct usbip_usb_device    udev;
	struct usbip_usb_interface uinf[];
} __attribute__((packed));


#define USBIP_OP_IMPORT 0x03
struct usbip_op_import_request {
    char busid[USBIP_SYSFS_BUS_ID_SIZE];
} __attribute__((packed));

struct usbip_op_import_reply {
    struct usbip_usb_device udev;
//	struct usbip_usb_interface uinf[];
} __attribute__((packed));


