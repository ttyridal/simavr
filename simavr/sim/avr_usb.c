/* vim: set sts=4:sw=4:ts=4:noexpandtab
	avr_usb.c

	Copyright 2012 Torbjorn Tyridal <ttyridal@gmail.com>

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
 */

/* TODO correct reset values */
/* TODO generate sofi every 1ms (when connected) */
/* TODO otg support? */
/* TODO drop bitfields? */
/* TODO thread safe ioctls */
/* TODO dual-bank endpoint buffers */
/* TODO actually pay attention to endpoint memory allocation ? buggy endpoint configuration doesn't matter in the simulator now. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include "avr_usb.h"
#include "avr_usb_int.h"

#define min(a,b) ((a)<(b)?(a):(b))

const uint8_t num_endpoints = 5;//sizeof (struct usb_internal_state.ep_state) / sizeof (struct usb_internal_state.ep_state[0]);

static uint8_t
current_ep_to_cpu(
		avr_usb_t * p)
{
	return p->io.avr->data[p->r_usbcon + uenum];
}

static struct _epstate *
get_epstate(
		avr_usb_t * p,
		uint8_t ep)
{
	assert(ep < num_endpoints);
	return &p->state->ep_state[ep];
}


enum epints {
	txini = 0,
	stalledi = 1,
	rxouti = 2,
	rxstpi = 3,
	nakouti = 4,
	nakini = 6,
	overfi = 10,
	underfi = 11,
};

static void
raise_ep_interrupt(
        struct avr_t * avr,
        avr_usb_t * p,
        uint8_t ep,
        enum epints irq)
{
	struct _epstate * epstate = get_epstate(p, ep);
	avr->data[p->r_usbcon + ueint] |= 1 << ep;
	switch (irq) {
		case txini:
		case stalledi:
		case rxouti:
		case nakouti:
		case nakini:
			epstate->ueintx.v |= 1 << irq;
			if (epstate->ueienx.v & (1 << irq))
				avr_raise_interrupt(avr, &p->state->com_vect);
			break;
		case rxstpi:
			epstate->ueintx.v |= 1 << irq;
			if (epstate->ueienx.v & (1 << irq))
				avr_raise_interrupt(avr, &p->state->com_vect);
			break;
		case overfi:
			epstate->uesta0x.overfi = 1;
			if (epstate->ueienx.flerre)
				avr_raise_interrupt(avr, &p->state->com_vect);
			break;
		case underfi:
			epstate->uesta0x.underfi = 1;
			if (epstate->ueienx.flerre)
				avr_raise_interrupt(avr, &p->state->com_vect);
			break;
		default:
			assert(0);
	}
}

static void
raise_usb_interrupt(
		avr_usb_t * p,
		enum usbints irq)
{
	uint8_t * Rudien = &p->io.avr->data[p->r_usbcon + udien];
	uint8_t * Rudint = &p->io.avr->data[p->r_usbcon + udint];

	switch (irq) {
		case uprsmi:
		case eorsmi:
		case wakeupi:
		case eorsti:
		case sofi:
		case suspi:
			*Rudint |= 1 << irq;
			if (*Rudien & (1 << irq))
				avr_raise_interrupt(p->io.avr, &p->state->gen_vect);
			break;
		default:
			assert(0);
	}

}

static void
reset_endpoints(
		struct avr_t * avr,
		avr_usb_t * p)
{
	memset(&p->state->ep_state[1], 0,
	        sizeof p->state->ep_state - sizeof p->state->ep_state[0]);
}

static int
ep_fifo_empty(
		struct _epstate * epstate)
{
	return epstate->bank[epstate->current_bank].tail == 0;
}

static int
ep_fifo_full(
		struct _epstate * epstate)
{
	return epstate->bank[epstate->current_bank].tail >=
					(8 << epstate->uecfg1x.epsize);
}

static uint8_t
ep_fifo_size(
		struct _epstate * epstate)
{
	assert(epstate->ueconx.epen);
	return (8 << epstate->uecfg1x.epsize);
}

static uint8_t
ep_fifo_count(
		struct _epstate * epstate)
{
	return epstate->bank[epstate->current_bank].tail;
}

static int
host_read_ep_fifo(
		struct _epstate * epstate,
		uint8_t * buf,
		size_t sz)
{
	if (!epstate->ueconx.epen) {
		printf("WARNING! Reading from non configured endpoint\n");
		return -1;
	}
	if (epstate->ueintx.txini) {
		return AVR_IOCTL_USB_NAK;
	}
	if (epstate->ueintx.fifocon && epstate->uecfg0x.eptype != 0) {
		return AVR_IOCTL_USB_NAK;
	}

	int ret = epstate->bank[epstate->current_bank].tail;
	printf("%s buflen:%zu, fifo:%d\n",__FUNCTION__,sz,ret);
	sz = min(epstate->bank[epstate->current_bank].tail, sz);
	memcpy(buf, epstate->bank[epstate->current_bank].bytes,
	        sz);
// 	int i,j;
// 	for (i=0, j=ret; j < epstate->bank[epstate->current_bank].tail; i++, j++)
// 		epstate->bank[epstate->current_bank].bytes[i] =
// 			epstate->bank[epstate->current_bank].bytes[j];

	epstate->bank[epstate->current_bank].tail = 0;
	return ret + (ep_fifo_size(epstate) << 8);
}

static int
host_write_ep_fifo(
        struct _epstate * epstate,
        uint8_t * buf,
        uint8_t len)
{
	if (!epstate->ueconx.epen) {
		printf("WARNING! Adding bytes to non configured endpoint\n");
		return -1;
	}

	if (epstate->ueintx.rxouti) {
		return AVR_IOCTL_USB_NAK;
	}
	if (epstate->ueintx.fifocon && epstate->uecfg0x.eptype != 0) {
		return AVR_IOCTL_USB_NAK;
	}

	if (len > ep_fifo_size(epstate)) {
		printf("EP OVERFI\n");
		len = sizeof epstate->bank[epstate->current_bank].bytes;
	}memcpy(epstate->bank[epstate->current_bank].bytes, buf, len);
	epstate->bank[epstate->current_bank].tail = len;

	return 0;
}

static uint8_t
avr_usb_ep_read_bytecount(
        struct avr_t * avr,
        avr_io_addr_t addr,
        void * param)
{
	avr_usb_t * p = (avr_usb_t *) param;
	if (pthread_mutex_lock(&p->state->mutex))
		abort();
	uint8_t ret = ep_fifo_count(get_epstate(p, current_ep_to_cpu(p)));
	pthread_mutex_unlock(&p->state->mutex);
	return ret;
}

static void
avr_usb_udaddr_write(
        struct avr_t * avr,
        avr_io_addr_t addr,
        uint8_t v,
        void * param)
{
	if (v & 0x80)
		AVR_LOG(avr, LOG_TRACE, "USB: Activate address %d\n", v & 0x7f);
	avr_core_watch_write(avr, addr, v);
}

static void
avr_usb_udcon_write(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_usb_t * p = (avr_usb_t *)param;

	if(avr->data[addr]&1 && !(v&1))
		avr_raise_irq(p->io.irq + USB_IRQ_ATTACH, !(v&1));
	avr_core_watch_write(avr, addr, v);
}

static void
avr_usb_uenum_write(
        struct avr_t * avr,
        avr_io_addr_t addr,
        uint8_t v,
        void * param)
{
	assert(v < num_endpoints);
	avr_core_watch_write(avr, addr, v);
}

static void
avr_debug_Write(
        struct avr_t * avr,
        avr_io_addr_t addr,
        uint8_t v,
        void * param)
{
    printf("%c", v);
}

static void
avr_usb_ep_write_ueintx(
        struct avr_t * avr,
        avr_io_addr_t addr,
        uint8_t v,
        void * param)
{
	avr_usb_t * p = (avr_usb_t *) param;
	uint8_t ep = current_ep_to_cpu(p);

	if(pthread_mutex_lock(&p->state->mutex))
		abort();

	union _ueintx * newstate = (union _ueintx*) &v;
	union _ueintx * curstate = &p->state->ep_state[ep].ueintx;

	if (curstate->rxouti & !newstate->rxouti) {
		curstate->rxouti = 0;
		printf("ep%d cpu released buffer, empty\n",ep);
	}
	if (curstate->txini & !newstate->txini) {
		curstate->txini = 0;
		printf("ep%d cpu released buffer, with data\n",ep);
	}
	if (curstate->rxstpi & !newstate->rxstpi) {
		curstate->txini = 1;
		curstate->rxouti = 0;
		curstate->rxstpi = 0;
	}
	if (curstate->fifocon & !newstate->fifocon) {
		curstate->fifocon = 0;
		printf("ep%d CPU released buffer\n", ep);
	}
	if (curstate->nakini & !newstate->nakini)
		curstate->nakini = 0;
	if (curstate->nakouti & !newstate->nakouti)
		curstate->nakouti = 0;
	if (curstate->stalledi & !newstate->stalledi)
		curstate->stalledi = 0;
	if (curstate->rwal & !newstate->rwal)
		AVR_LOG(avr, LOG_WARNING, "USB: Pointless change of ueintx.rwal\n");

	if ((curstate->v & 0xdf) == 0)
		avr->data[p->r_usbcon + ueint] &= 0xff ^ (1 << ep); // mark ep0 interrupt

	pthread_mutex_unlock(&p->state->mutex);
}

static uint8_t
avr_usb_ep_read(
        struct avr_t * avr,
        avr_io_addr_t addr,
        void * param)
{
	avr_usb_t * p = (avr_usb_t *) param;
	uint8_t laddr = addr - p->r_usbcon;
	uint8_t v;
	struct _epstate * epstate = get_epstate(p, current_ep_to_cpu(p));

	if (pthread_mutex_lock(&p->state->mutex))
		abort();
	switch(laddr) {
		case ueconx:  v = epstate->ueconx.v; break;
		case uecfg0x: v = epstate->uecfg0x.v; break;
		case uecfg1x: v = epstate->uecfg1x.v; break;
		case uesta0x: v = epstate->uesta0x.v; break;
		case uesta1x: v = epstate->uesta1x.v; break;
		case ueienx:  v = epstate->ueienx.v; break;
		case ueintx: v = epstate->ueintx.v; break;
		default:assert(0);
	}
	pthread_mutex_unlock(&p->state->mutex);
	return v;
}

static void
avr_usb_ep_write(
        struct avr_t * avr,
        avr_io_addr_t addr,
        uint8_t v,
        void * param)
{
	avr_usb_t * p = (avr_usb_t *) param;
	struct _epstate * epstate = get_epstate(p, current_ep_to_cpu(p));
	uint8_t laddr = addr - p->r_usbcon;

	if (pthread_mutex_lock(&p->state->mutex))
		abort();

	switch (laddr) {
		case ueconx:
			if (v & 1 << 4)
				epstate->ueconx.stallrq = 0;
			if (v & 1 << 5)
				epstate->ueconx.stallrq = 1;
			epstate->ueconx.epen = (v & 1) != 0;
			break;
		case uecfg0x:
			epstate->uecfg0x.v = v;
			epstate->uesta0x.cfgok = 0;
			break;
		case uecfg1x:
			epstate->uecfg1x.v = v;
			epstate->uesta0x.cfgok = epstate->uecfg1x.alloc;
			if (epstate->uecfg0x.eptype == 0)
				epstate->ueintx.txini = 1;
			else if (epstate->uecfg0x.epdir) {
				epstate->ueintx.txini = 1;
				epstate->ueintx.rwal = 1;
				epstate->ueintx.fifocon = 1;
			} else
				epstate->ueintx.rxouti = 0;
			avr_core_watch_write(avr, p->r_usbcon + uesta0x,
			        epstate->uesta0x.v);
			break;
		case uesta0x:
			v = (epstate->uesta0x.v & 0x9f) + (v & (0x60 & epstate->uesta0x.v));
			epstate->uesta0x.v = v;
			break;
		case ueienx:
			epstate->ueienx.v = v;
			break;
		default:
			assert(0);
	}
	pthread_mutex_unlock(&p->state->mutex);
}

static uint8_t
avr_read_ep_fifo(
        struct avr_t * avr,
        avr_io_addr_t addr,
        void * param)
{
	avr_usb_t * p = (avr_usb_t *) param;
	struct _epstate * epstate = get_epstate(p, current_ep_to_cpu(p));
	uint8_t ret = 0;

	if (!epstate->ueconx.epen) {
		printf("WARNING! Reading bytes from non configured endpoint\n");
		return 0;
	}

	if (ep_fifo_empty(epstate))
		raise_ep_interrupt(avr, p, current_ep_to_cpu(p), underfi);
	else {
		ret = epstate->bank[epstate->current_bank].bytes[0];

		int i,j;
		for (i = 0, j = ep_fifo_count(epstate) - 1; i < j; i++)
			epstate->bank[epstate->current_bank].bytes[i] =
					epstate->bank[epstate->current_bank].bytes[i + 1];
		epstate->bank[epstate->current_bank].tail--;
		epstate->ueintx.rwal = !ep_fifo_empty(epstate);
	}
	return ret;
}

static void
avr_write_ep_fifo(
        struct avr_t * avr,
        avr_io_addr_t addr,
        uint8_t v,
        void * param)
{
	avr_usb_t * p = (avr_usb_t *) param;
	struct _epstate * epstate = get_epstate(p, current_ep_to_cpu(p));

	if (!epstate->ueconx.epen) {
		printf("WARNING! Adding bytes to non configured endpoint\n");
		return;
	}

	if (pthread_mutex_lock(&p->state->mutex))
		abort();

	if (ep_fifo_full(epstate)) {
		pthread_mutex_unlock(&p->state->mutex);
		raise_ep_interrupt(avr, p, current_ep_to_cpu(p), overfi);
	}
	else {
		epstate->bank[epstate->current_bank].bytes[epstate->bank[epstate->current_bank].tail++] = v;
		epstate->ueintx.rwal = !ep_fifo_full(epstate);
		pthread_mutex_unlock(&p->state->mutex);
	}
}

static void
avr_usb_pll_write(
        struct avr_t * avr,
        avr_io_addr_t addr,
        uint8_t v,
        void * param)
{
	v |= (v >> 1) & 1;
	avr_core_watch_write(avr, addr, v);
}


avr_cycle_count_t
sof_generator(
        struct avr_t * avr,
        avr_cycle_count_t when,
        void * param)
{
	avr_usb_t * p = (avr_usb_t *) param;
	//stop sof generation if detached
	if (avr->data[p->r_usbcon + udcon] & 1)
		return 0;
	else {
		raise_usb_interrupt(p, sofi);
		return when;
	}
}

static int
avr_usb_ioctl(
		struct avr_io_t * io,
		uint32_t ctl,
		void * io_param)
{
	avr_usb_t * p = (avr_usb_t *) io;
	struct avr_io_usb * d = (struct avr_io_usb*) io_param;
	struct _epstate * epstate = 0;
	int ret;
	uint8_t ep;

	switch (ctl) {
		case AVR_IOCTL_USB_READ:
			ep = d->pipe & 0x7f;
			epstate = get_epstate(p, ep);

			if (epstate->ueconx.stallrq) {
				raise_ep_interrupt(io->avr, p, 0, stalledi);
				return AVR_IOCTL_USB_STALL;
			}
			if (ep && !epstate->uecfg0x.epdir)
				AVR_LOG(io->avr, LOG_WARNING, "USB: Host reading from OUT endpoint?\n");

			if (pthread_mutex_lock(&p->state->mutex))
				abort();
			ret = host_read_ep_fifo(epstate, d->buf, d->sz);
			pthread_mutex_unlock(&p->state->mutex);

			if (ret < 0) {
				// is this correct? It makes the cdc example work.
				// Linux stops polling the data ep if we send naks,but
				// according to usb spec nak'ing should be ok.
				if (epstate->uecfg0x.eptype == 2) {
					d->sz = 0;
					return 0;
				} else
					return ret;
			}

			if (pthread_mutex_lock(&p->state->mutex))
				abort();
			if (ep_fifo_empty(epstate)) {
				epstate->ueintx.fifocon = 1;
				pthread_mutex_unlock(&p->state->mutex);
				raise_ep_interrupt(io->avr, p, ep, txini);
				printf("ep%d Buffer with CPU\n", ep);
			} else
				pthread_mutex_unlock(&p->state->mutex);

			return ret;
		case AVR_IOCTL_USB_WRITE:
			ep = d->pipe & 0x7f;
			epstate = get_epstate(p, ep);

			if (ep && epstate->uecfg0x.epdir)
				AVR_LOG(io->avr, LOG_WARNING, "USB: Host writing to IN endpoint?\n");

			if (epstate->ueconx.stallrq) {
				raise_ep_interrupt(io->avr, p, 0, stalledi);
				return AVR_IOCTL_USB_STALL;
			}

			if (pthread_mutex_lock(&p->state->mutex))
				abort();
			ret = host_write_ep_fifo(epstate, d->buf, d->sz);
			if (ret < 0) {
				pthread_mutex_unlock(&p->state->mutex);
				return ret;
			}

			epstate->ueintx.fifocon = 1;
			pthread_mutex_unlock(&p->state->mutex);
			raise_ep_interrupt(io->avr, p, ep, rxouti);
			printf("ep%d Buffer with CPU\n", ep);
			return 0;
		case AVR_IOCTL_USB_SETUP:
			ep = d->pipe & 0x7f;
			epstate = get_epstate(p, ep);

			epstate->ueconx.stallrq = 0;
			// teensy actually depends on this (fails to ack rxouti on usb
			// control read status stage) even if the datasheet clearly states
			// that one should do so.
			epstate->ueintx.rxouti = 0;

			ret = host_write_ep_fifo(epstate, d->buf, d->sz);
			if (ret < 0)
				return ret;
			raise_ep_interrupt(io->avr, p, ep, rxstpi);

			return 0;
		case AVR_IOCTL_USB_RESET:
			AVR_LOG(io->avr, LOG_TRACE, "USB: __USB_RESET__\n");
			reset_endpoints(io->avr, p);
			raise_usb_interrupt(p, eorsti);
			if (0)
				avr_cycle_timer_register_usec(io->avr, 1000, sof_generator, p);
			return 0;
		default:
			return -1;
	}
}

void
avr_usb_reset(
		struct avr_io_t *io)
{
	avr_usb_t * p = (avr_usb_t *) io;
	uint8_t i;

	memset(p->state->ep_state, 0, sizeof p->state->ep_state);

	for (i = 0; i < otgtcon; i++)
		p->io.avr->data[p->r_usbcon + i] = 0;

	p->io.avr->data[p->r_usbcon] = 0x20;
	p->io.avr->data[p->r_usbcon + udcon] = 1;

	AVR_LOG(io->avr, LOG_TRACE, "USB: %s\n", __FUNCTION__);
}

static const char * irq_names[USB_IRQ_COUNT] = {
	[USB_IRQ_ATTACH] = ">attach",
};

static void
avr_usb_dealloc(
		struct avr_io_t * port)
{
	avr_usb_t * p = (avr_usb_t *) port;
	free(p->state);
}

static	avr_io_t	_io = {
	.kind = "usb",
	.reset = avr_usb_reset,
	.irq_names = irq_names,
	.ioctl = avr_usb_ioctl,
	.dealloc = avr_usb_dealloc,
};

static void
register_io_ep_readwrite(
		avr_t * avr,
		avr_usb_t * p,
		uint8_t laddr)
{
	avr_register_io_write(avr, p->r_usbcon + laddr, avr_usb_ep_write, p);
	avr_register_io_read(avr, p->r_usbcon + laddr, avr_usb_ep_read, p);
}

static void
register_vectors(
		avr_t * avr,
		avr_usb_t * p)
{
	// usb interrupts are multiplexed into just two vectors.
	// we therefore need fake bits for enable & raise

	// use usbe as fake enable bit
	p->state->com_vect.enable = (avr_regbit_t)AVR_IO_REGBIT(p->r_usbcon, 7);
	p->state->gen_vect.enable = (avr_regbit_t)AVR_IO_REGBIT(p->r_usbcon, 7);

//	// use reserved/unused bits in usbsta as fake raised bits
//	p->state->com_vect.raised = (avr_regbit_t)AVR_IO_REGBIT(p->r_usbcon+1,7);
//	p->state->gen_vect.raised = (avr_regbit_t)AVR_IO_REGBIT(p->r_usbcon+1,6);

	p->state->com_vect.vector = p->usb_com_vect;
	p->state->gen_vect.vector = p->usb_gen_vect;

	avr_register_vector(avr, &p->state->com_vect);
	avr_register_vector(avr, &p->state->gen_vect);
}

void avr_usb_init(avr_t * avr, avr_usb_t * p)
{
	p->io = _io;

	p->state = calloc(1, sizeof *p->state);

	pthread_mutexattr_t mutexattr;
	pthread_mutexattr_init(&mutexattr);
	pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_ERRORCHECK);

    if (pthread_mutex_init(&p->state->mutex, &mutexattr)) {
        printf("FATAL: avr_usb mutex init failed\n");
        abort();
    }
	pthread_mutexattr_destroy(&mutexattr);

	avr_register_io(avr, &p->io);
	register_vectors(avr, p);
	// allocate this module's IRQ
	avr_io_setirqs(&p->io, AVR_IOCTL_USB_GETIRQ(), USB_IRQ_COUNT, NULL);

	avr_register_io_write(avr, p->r_usbcon + udaddr, avr_usb_udaddr_write, p);
	avr_register_io_write(avr, p->r_usbcon + udcon, avr_usb_udcon_write, p);
	avr_register_io_write(avr, p->r_usbcon + uenum, avr_usb_uenum_write, p);

	avr_register_io_write(avr, 0x51, avr_debug_Write, p);

	avr_register_io_read(avr, p->r_usbcon + uedatx, avr_read_ep_fifo, p);
	avr_register_io_write(avr, p->r_usbcon + uedatx, avr_write_ep_fifo, p);
	avr_register_io_read(avr, p->r_usbcon + uebclx, avr_usb_ep_read_bytecount, p); //ro

	avr_register_io_read(avr, p->r_usbcon + ueintx, avr_usb_ep_read, p);
	avr_register_io_write(avr, p->r_usbcon + ueintx, avr_usb_ep_write_ueintx, p);

	register_io_ep_readwrite(avr, p, ueconx);
	register_io_ep_readwrite(avr, p, uecfg0x);
	register_io_ep_readwrite(avr, p, uecfg1x);
	register_io_ep_readwrite(avr, p, uesta0x);
	register_io_ep_readwrite(avr, p, uesta1x);
	register_io_ep_readwrite(avr, p, ueienx);

	avr_register_io_write(avr, p->r_pllcsr, avr_usb_pll_write, p);
}

