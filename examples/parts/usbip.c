/* vim: set sts=4:sw=4:ts=4:noexpandtab
	usbip.c

	Copyright 2017 Torbjorn Tyridal <ttyridal@gmail.com>

 	This file is part of simavr.

	simavr is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	simavr is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with simavr.  If not, see <http://www.gnu.org/licenses/>.


	This code is heavily inspired by
	https://github.com/lcgamboa/USBIP-Virtual-USB-Device
	Copyright (c) : 2016  Luis Claudio GambÃ´a Lopes
 */

/*
	this avrsim part will make your avr available as a usbip device.
	To connect it to the host system load modules usbip-core and vhci-hcd,
	then:
	~# usbip attach -r 127.0.0.1 -b 1-1
*/

/* TODO iso endpoint support */

#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define USBIP_PORT_NUM 3240

#include "usb_types.h"
#include "usbip_types.h"

#include "avr_usb.h"

struct usbip_t {
    struct avr_t * avr;
    bool attached;
    bool udev_valid;
	struct usbip_usb_device udev;
};

static int
control_read(
        const struct usbip_t * p,
        byte reqtype,
        byte req,
        word wValue,
        word wIndex,
        word wLength,
        byte * data)
{
    static const struct usb_endpoint control_ep = {0, 8};
	int ret;
	struct usb_setup_pkt buf = {
        reqtype | USB_REQTYPE_DIR_DEV_TO_HOST,
        req,
        wValue,
        wIndex,
        wLength };
	struct avr_io_usb pkt = { control_ep.epnum, sizeof buf, (uint8_t*) &buf };

	avr_ioctl(p->avr, AVR_IOCTL_USB_SETUP, &pkt);

	pkt.sz = control_ep.epsz;
	pkt.buf = data;
	while (wLength) {
		usleep(2000);
		switch (avr_ioctl(p->avr, AVR_IOCTL_USB_READ, &pkt)) {
			case AVR_IOCTL_USB_NAK:
				printf(" NAK\n");
				usleep(50000);
				continue;
			case AVR_IOCTL_USB_STALL:
				printf(" STALL\n");
				return AVR_IOCTL_USB_STALL;
            case 0: break;
			default:
				fprintf(stderr, "Unknown avr_ioctl return value\n");
				abort();
		}
		pkt.buf += pkt.sz;
		wLength -= pkt.sz;
	}
	wLength = pkt.buf - data;

	usleep(1000);
	pkt.sz = 0;
	while ((ret = avr_ioctl(p->avr, AVR_IOCTL_USB_WRITE, &pkt)) == AVR_IOCTL_USB_NAK)
		usleep(50000);
	assert(!ret);

	return wLength;
}

static bool
get_descriptor(
        const struct usbip_t * p,
        unsigned char descr_type,
        void * buf,
        size_t length)
{
    const unsigned char descr_index = 0;
    return control_read(p,
            USB_REQTYPE_STD + USB_REQTYPE_DEVICE,
            USB_REQUEST_GET_DESCRIPTOR,
            descr_type << 8 | descr_index,
            0,
            length,
            buf) == (int)length;
}

static bool
load_device_and_config_descriptor(
        struct usbip_t * p)
{
    if (p->udev_valid)
        return true;

    struct usb_device_descriptor dd;
    struct usb_configuration_descriptor cd;
    if (!get_descriptor(p, USB_DESCRIPTOR_DEVICE, &dd, sizeof dd)) {
        fprintf(stderr, "get device descriptor failed\n");
        p->udev_valid = false;
        return false;
    }

    if (!get_descriptor(p, USB_DESCRIPTOR_CONFIGURATION, &cd, sizeof cd)) {
        fprintf(stderr, "get configuration descriptor failed\n");
        p->udev_valid = false;
        return false;
    }

    strcpy(p->udev.path, "/sys/devices/pci0000:00/0000:00:01.2/usb1/1-1");
    strcpy(p->udev.busid, "1-1");
    p->udev.busnum = htonl(1);
    p->udev.devnum = htonl(2);
    p->udev.speed = htonl(2);

	p->udev.idVendor = htons(dd.idVendor);
	p->udev.idProduct = htons(dd.idProduct);
	p->udev.bcdDevice = htons(dd.bcdDevice);

	p->udev.bDeviceClass = dd.bDeviceClass;
	p->udev.bDeviceSubClass = dd.bDeviceSubClass;
	p->udev.bDeviceProtocol = dd.bDeviceProtocol;
	p->udev.bConfigurationValue = cd.bConfigurationValue;
	p->udev.bNumConfigurations = dd.bNumConfigurations;
	p->udev.bNumInterfaces = cd.bNumInterfaces;

    p->udev_valid = true;
    return true;
}


static void
vhci_usb_attach_hook(
        struct avr_irq_t * irq,
        uint32_t value,
        void * param)
{
    struct usbip_t * p = param;
	p->attached = !!value;
	printf("avr attached: %d\n", p->attached);

    avr_ioctl(p->avr, AVR_IOCTL_USB_VBUS, (void*) 1);

    (void)irq;
}
#if 0

static int
control_write(
        struct vhci_usb_t * p,
        struct _ep * ep,
        uint8_t reqtype,
        uint8_t req,
        uint16_t wValue,
        uint16_t wIndex,
        uint16_t wLength,
        uint8_t * data)
{
	assert((reqtype&0x80)==0);
	int ret;
	struct usbsetup buf =
		{ reqtype, req, wValue, wIndex, wLength };
	struct avr_io_usb pkt =
		{ ep->epnum, sizeof(struct usbsetup), (uint8_t*) &buf };

	avr_ioctl(p->avr, AVR_IOCTL_USB_SETUP, &pkt);
	usleep(10000);

	if (wLength > 0) {
		pkt.sz = (wLength > ep->epsz ? ep->epsz : wLength);
		pkt.buf = data;
		while ((ret = avr_ioctl(p->avr, AVR_IOCTL_USB_WRITE, &pkt)) != 0) {
			if (ret == AVR_IOCTL_USB_NAK) {
				usleep(50000);
				continue;
			}
			if (ret == AVR_IOCTL_USB_STALL) {
				printf(" STALL\n");
				return ret;
			}
			assert(ret==0);
			if (pkt.sz != ep->epsz)
				break;
			pkt.buf += pkt.sz;
			wLength -= pkt.sz;
			pkt.sz = (wLength > ep->epsz ? ep->epsz : wLength);
		}
	}

	pkt.sz = 0;
	while ((ret = avr_ioctl(p->avr, AVR_IOCTL_USB_READ, &pkt))
	        == AVR_IOCTL_USB_NAK) {
		usleep(50000);
	}
	return ret;
}

static void
handle_status_change(
        struct vhci_usb_t * p,
        struct usb_vhci_port_stat*prev,
        struct usb_vhci_port_stat*curr)
{
	if (~prev->status & USB_VHCI_PORT_STAT_POWER
	        && curr->status & USB_VHCI_PORT_STAT_POWER) {
		avr_ioctl(p->avr, AVR_IOCTL_USB_VBUS, (void*) 1);
		if (p->attached) {
			if (usb_vhci_port_connect(p->fd, 1, USB_VHCI_DATA_RATE_FULL) < 0) {
				perror("port_connect");
				abort();
			}
		}
	}
	if (prev->status & USB_VHCI_PORT_STAT_POWER
	        && ~curr->status & USB_VHCI_PORT_STAT_POWER)
		avr_ioctl(p->avr, AVR_IOCTL_USB_VBUS, 0);

	if (curr->change & USB_VHCI_PORT_STAT_C_RESET
	        && ~curr->status & USB_VHCI_PORT_STAT_RESET
	        && curr->status & USB_VHCI_PORT_STAT_ENABLE) {
//         printf("END OF RESET\n");
	}
	if (~prev->status & USB_VHCI_PORT_STAT_RESET
	        && curr->status & USB_VHCI_PORT_STAT_RESET) {
		avr_ioctl(p->avr, AVR_IOCTL_USB_RESET, NULL);
		usleep(50000);
		if (curr->status & USB_VHCI_PORT_STAT_CONNECTION) {
			if (usb_vhci_port_reset_done(p->fd, 1, 1) < 0) {
				perror("reset_done");
				abort();
			}
		}
	}
	if (~prev->flags & USB_VHCI_PORT_STAT_FLAG_RESUMING
	        && curr->flags & USB_VHCI_PORT_STAT_FLAG_RESUMING) {
		printf("port resuming\n");
		if (curr->status & USB_VHCI_PORT_STAT_CONNECTION) {
			printf("  completing\n");
			if (usb_vhci_port_resumed(p->fd, 1) < 0) {
				perror("resumed");
				abort();
			}
		}
	}
	if (~prev->status & USB_VHCI_PORT_STAT_SUSPEND
	        && curr->status & USB_VHCI_PORT_STAT_SUSPEND)
		printf("port suspedning\n");
	if (prev->status & USB_VHCI_PORT_STAT_ENABLE
	        && ~curr->status & USB_VHCI_PORT_STAT_ENABLE)
		printf("port disabled\n");

	*prev = *curr;
}

static void
handle_ep0_control(
        struct vhci_usb_t * p,
        struct _ep * ep0,
        struct usb_vhci_urb * urb)
{
	int res;
	if (urb->bmRequestType &0x80) {
		res = control_read(p,ep0,
				urb->bmRequestType,
				urb->bRequest,
				urb->wValue,
				urb->wIndex,
				urb->wLength,
				urb->buffer);
			if (res>=0) {
				urb->buffer_actual=res;
				res=0;
			}
	}
	else
		res = control_write(p,ep0,
			urb->bmRequestType,
			urb->bRequest,
			urb->wValue,
			urb->wIndex,
			urb->wLength,
			urb->buffer);

	if (res==AVR_IOCTL_USB_STALL)
		urb->status = USB_VHCI_STATUS_STALL;
	else
		urb->status = USB_VHCI_STATUS_SUCCESS;
}

static void *
vhci_usb_thread(
		void * param)
{
	struct vhci_usb_t * p = (struct vhci_usb_t*) param;
	struct _ep ep0 =
		{ 0, 0 };
	struct usb_vhci_port_stat port_status;
	int id, busnum;
	char*busid;
	p->fd = usb_vhci_open(1, &id, &busnum, &busid);

	if (p->fd < 0) {
		perror("open vhci failed");
		printf("driver loaded, and access bits ok?\n");
		abort();
	}
	printf("Created virtual usb host with 1 port at %s (bus# %d)\n", busid,
	        busnum);
	memset(&port_status, 0, sizeof port_status);

	bool avrattached = false;

	for (unsigned cycle = 0;; cycle++) {
		struct usb_vhci_work wrk;

		int res = usb_vhci_fetch_work(p->fd, &wrk);

		if (p->attached != avrattached) {
			if (p->attached && port_status.status & USB_VHCI_PORT_STAT_POWER) {
				if (usb_vhci_port_connect(p->fd, 1, USB_VHCI_DATA_RATE_FULL)
				        < 0) {
					perror("port_connect");
					abort();
				}
			}
			if (!p->attached) {
				ep0.epsz = 0;
				//disconnect
			}
			avrattached = p->attached;
		}

		if (res < 0) {
			if (errno == ETIMEDOUT || errno == EINTR || errno == ENODATA)
				continue;
			perror("fetch work failed");
			abort();
		}

		switch (wrk.type) {
			case USB_VHCI_WORK_TYPE_PORT_STAT:
				handle_status_change(p, &port_status, &wrk.work.port_stat);
				break;
			case USB_VHCI_WORK_TYPE_PROCESS_URB:
				if (!ep0.epsz)
					ep0.epsz = get_ep0_size(p);

				wrk.work.urb.buffer = 0;
				wrk.work.urb.iso_packets = 0;
				if (wrk.work.urb.buffer_length)
					wrk.work.urb.buffer = malloc(wrk.work.urb.buffer_length);
				if (wrk.work.urb.packet_count)
					wrk.work.urb.iso_packets = malloc(
					        wrk.work.urb.packet_count
					                * sizeof(struct usb_vhci_iso_packet));
				if (res) {
					if (usb_vhci_fetch_data(p->fd, &wrk.work.urb) < 0) {
						if (errno != ECANCELED)
							perror("fetch_data");
						free(wrk.work.urb.buffer);
						free(wrk.work.urb.iso_packets);
						usb_vhci_giveback(p->fd, &wrk.work.urb);
						break;
					}
				}

				if (usb_vhci_is_control(wrk.work.urb.type)
				        && !(wrk.work.urb.epadr & 0x7f)) {
					handle_ep0_control(p, &ep0, &wrk.work.urb);

				} else {
					struct avr_io_usb pkt =
						{ wrk.work.urb.epadr, wrk.work.urb.buffer_actual,
						        wrk.work.urb.buffer };
					if (usb_vhci_is_out(wrk.work.urb.epadr))
						res = avr_ioctl(p->avr, AVR_IOCTL_USB_WRITE, &pkt);
					else {
						pkt.sz = wrk.work.urb.buffer_length;
						res = avr_ioctl(p->avr, AVR_IOCTL_USB_READ, &pkt);
						wrk.work.urb.buffer_actual = pkt.sz;
					}
					if (res == AVR_IOCTL_USB_STALL)
						wrk.work.urb.status = USB_VHCI_STATUS_STALL;
					else if (res == AVR_IOCTL_USB_NAK)
						wrk.work.urb.status = USB_VHCI_STATUS_TIMEDOUT;
					else
						wrk.work.urb.status = USB_VHCI_STATUS_SUCCESS;
				}
				if (usb_vhci_giveback(p->fd, &wrk.work.urb) < 0)
					perror("giveback");
				free(wrk.work.urb.buffer);
				free(wrk.work.urb.iso_packets);
				break;
			case USB_VHCI_WORK_TYPE_CANCEL_URB:
				printf("cancel urb\n");
				break;
			default:
				printf("illegal work type\n");
				abort();
		}

	}
}
#endif

static int
open_usbip_socket(
        void)
{
	struct sockaddr_in serv;
	int listenfd;

	if ((listenfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket error");
		exit (1);
	};

	int reuse = 1;
	if (setsockopt(
				listenfd,
				SOL_SOCKET,
				SO_REUSEADDR,
				(const char*)&reuse,
				sizeof(reuse)) < 0)
		perror("setsockopt(SO_REUSEADDR) failed");

	memset(&serv, 0, sizeof serv);
	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = htonl(INADDR_ANY);
	serv.sin_port = htons(USBIP_PORT_NUM);

	if (bind(listenfd, (struct sockaddr *)&serv, sizeof serv) < 0) {
		perror("bind error");
		exit (1);
	}

	if (listen(listenfd, SOMAXCONN) < 0) {
		perror("listen error");
		exit (1);
	}

	return listenfd;
}


static void
handle_usbip_req_devlist(int sockfd, struct usbip_t * p) {
    struct usbip_op_common op_common = {
        htons(USBIP_PROTO_VERSION),
        htons(USBIP_OP_REPLY | USBIP_OP_DEVLIST),
        htonl(USBIP_ST_NA)
    };
    struct usbip_op_devlist_reply devlist = {htonl(1)};


    avr_ioctl(p->avr, AVR_IOCTL_USB_RESET, NULL);
    usleep(2500);

    load_device_and_config_descriptor(p);

    // get config AND interface descriptors
    struct {
        struct usb_configuration_descriptor config;
        struct usb_interface_descriptor interf[p->udev.bNumInterfaces];
    } cd;
    if (!get_descriptor(p, USB_DESCRIPTOR_CONFIGURATION, &cd, sizeof cd)) {
        fprintf(stderr, "get configuration descriptor failed\n");
        if (send(sockfd, &op_common, sizeof op_common, 0) != sizeof op_common)
            perror("sock send");
        return;
    }

    ssize_t devinfo_sz = sizeof (struct usbip_usb_device) +
                        devinfo->udev.bNumInterfaces * sizeof (struct usbip_usb_interface);
    struct usbip_op_devlist_reply_extra * devinfo = malloc(devinfo_sz);

    devinfo->udev = p->udev;

    for (byte i=0; i < devinfo->udev.bNumInterfaces; i++) {
        devinfo->uinf[i].bInterfaceClass = cd.interf[i].bInterfaceClass;
        devinfo->uinf[i].bInterfaceSubClass = cd.interf[i].bInterfaceSubClass;
        devinfo->uinf[i].bInterfaceProtocol = cd.interf[i].bInterfaceProtocol;
        devinfo->uinf[i].padding = 0;
    }

    op_common.status = htonl(USBIP_ST_OK);

#define sock_send(sockfd, dta, dta_sz) if(send(sockfd, dta, dta_sz, 0) != dta_sz) { perror("sock send"); break; }
    do {
        sock_send(sockfd, &op_common, sizeof op_common)
        sock_send(sockfd, &devlist, sizeof devlist)
        sock_send(sockfd, devinfo, devinfo_sz)
    } while (0);
#undef sock_send

    free(devinfo);
}


static void
handle_usbip_connection(int sockfd, struct usbip_t * p) {
	while (1) {
		struct usbip_op_common op_common;
		ssize_t nb = recv(sockfd, &op_common, sizeof op_common, 0);
		if (nb != sizeof op_common)
			return;
		unsigned version = ntohs(op_common.version);
		unsigned op_code = ntohs(op_common.code);
		if (version != USBIP_PROTO_VERSION) {
			fprintf(stderr, "Protocol version mismatch, request: %x, this: %x\n",
					version, USBIP_PROTO_VERSION);
			return;
		}
		switch (op_code) {
			case USBIP_OP_REQUEST | USBIP_OP_DEVLIST:
                handle_usbip_req_devlist(sockfd, p);
				break;
			default:
				fprintf(stderr, "Unknown usbip %s %x\n",
						op_code & USBIP_OP_REQUEST ? "request" : "reply",
						op_code & 0xff);
				return;
		}
	}
}

void *
usbip_main(
		struct usbip_t * p)
{
	int listenfd = open_usbip_socket();
	struct sockaddr_in cli;
	unsigned int clilen = sizeof(cli);
	int sockfd = accept(listenfd, (struct sockaddr *)&cli,  &clilen);

	if ( sockfd < 0) {
		printf ("accept error : %s \n", strerror (errno));
		exit (1);
	}
	fprintf(stderr, "Connection address:%s\n",inet_ntoa(cli.sin_addr));
	handle_usbip_connection(sockfd, p);
	close(sockfd);

(void)p;
	return NULL;
}


struct usbip_t *
usbip_create(
		struct avr_t * avr)
{
	struct usbip_t * p = malloc(sizeof *p);
    memset(p, 0, sizeof *p);
	p->avr = avr;


	avr_irq_t * t = avr_io_getirq(p->avr, AVR_IOCTL_USB_GETIRQ(), USB_IRQ_ATTACH);
	avr_irq_register_notify(t, vhci_usb_attach_hook, p);

	return p;
}

void
usbip_destroy(
	void * p)
{
	free(p);
}
