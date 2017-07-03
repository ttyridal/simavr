#!/usr/bin/env python3
from collections import namedtuple
from contextlib import closing
import socket
import struct
import binascii
import sys

USBIP_ST_OK = 0
USBIP_ST_NAK = 1

USBIP_CMD_SUBMIT = 0x0001
USBIP_CMD_UNLINK = 0x0002
USBIP_RET_SUBMIT = 0x0003
USBIP_RET_UNLINK = 0x0004

CmdHdr_s = struct.Struct("!IIIII")
CmdHdr = namedtuple("CmdHdr", "command seqnum devid direction ep")

CmdSubmit_s = struct.Struct("!Iiiii8s")
CmdSubmit = namedtuple("CmdSubmit", "flags tbl sof numpackets interval setup")

MsgSize = CmdHdr_s.size + CmdSubmit_s.size
USBIP_DIR_OUT = 0x00
URB_DIR_MASK = 0x00000200

CmdUnlink_s = struct.Struct("!I")
CmdUnlink = namedtuple("CmdUnlink", "unlink_seqnum")


CmdSubmitRet_s = struct.Struct("!IIIII8x") # status actual_length, sof, num_pkts, err_count

class NotHandled(Exception): pass

def do_setup(s, ret, setup, data):

    if setup.reqtype == 0x80 and setup.req == 6 and setup.val == 0x100:
        print("ACK get device descriptor")
        data = b'\x12\x01\x00\x02\x02\x00\x00\x10\xc0\x16\x7a\x04\x00\x01\x01\x02\x03\x01'
        l = min(len(data), setup.len)
        CmdSubmitRet_s.pack_into(ret, CmdHdr_s.size, USBIP_ST_OK, l, 0, 0, 0)
        s.send(ret + data[:l])
    elif setup.reqtype == 0x80 and setup.req == 6 and setup.val == 0x200:
        data = b'\x09\x02\x43\x00\x02\x01\x00\xc0\x32\x09\x04\x00\x00\x01\x02\x02\x01\x00\x05\x24\x00\x10\x01\x05\x24\x01\x01\x01\x04\x24\x02\x06\x05\x24\x06\x00\x01\x07\x05\x82\x03\x10\x00\x40\x09\x04\x01\x00\x02\x0a\x00\x00\x00\x07\x05\x03\x02\x20\x00\x00\x07\x05\x84\x02 \x00\x00'
        l = min(len(data), setup.len)
        CmdSubmitRet_s.pack_into(ret, CmdHdr_s.size, USBIP_ST_OK, l, 0, 0, 0)
        s.send(ret + data[:l])
    elif setup.reqtype == 0x80 and setup.req == 6 and setup.val == 0x300:
        data = b'\x04\x03\t\x04'
        l = min(len(data), setup.len)
        CmdSubmitRet_s.pack_into(ret, CmdHdr_s.size, USBIP_ST_OK, l, 0, 0, 0)
        s.send(ret + data[:l])
    elif setup.reqtype == 0x80 and setup.req == 6 and setup.val == 0x302 and setup.idx == 0x409:
        data = b'\x16\x03U\x00S\x00B\x00 \x00S\x00e\x00r\x00i\x00a\x00l\x00'
        l = min(len(data), setup.len)
        CmdSubmitRet_s.pack_into(ret, CmdHdr_s.size, USBIP_ST_OK, l, 0, 0, 0)
        s.send(ret + data[:l])
    elif setup.reqtype == 0x80 and setup.req == 6 and setup.val == 0x301 and setup.idx == 0x409:
        data = b'\x14\x03Y\x00o\x00u\x00r\x00 \x00N\x00a\x00m\x00e\x00'
        l = min(len(data), setup.len)
        CmdSubmitRet_s.pack_into(ret, CmdHdr_s.size, USBIP_ST_OK, l, 0, 0, 0)
        s.send(ret + data[:l])
    elif setup.reqtype == 0x80 and setup.req == 6 and setup.val == 0x303 and setup.idx == 0x409:
        data = b'\x0c\x031\x002\x003\x004\x005\x00'
        l = min(len(data), setup.len)
        CmdSubmitRet_s.pack_into(ret, CmdHdr_s.size, USBIP_ST_OK, l, 0, 0, 0)
        s.send(ret + data[:l])
    elif setup.reqtype == 0x0 and setup.req == 9 and setup.val == 1 and setup.idx == 0:
        print("Set config ACK")
        CmdSubmitRet_s.pack_into(ret, CmdHdr_s.size, USBIP_ST_OK, 0, 0, 0, 0)
        s.send(ret)

    elif setup.reqtype == 0x21 and setup.req == 0x20 and setup.val == 0 and setup.idx == 0 and len(data) == 7:
        print("ACK line coding")
        CmdSubmitRet_s.pack_into(ret, CmdHdr_s.size, USBIP_ST_OK, 0, 0, 0, 0)
        s.send(ret)
    elif setup.reqtype == 0x21 and setup.req == 0x22 and setup.idx == 0 and data is None:
        print("ACK line coding 2 value:",setup.val)
        CmdSubmitRet_s.pack_into(ret, CmdHdr_s.size, USBIP_ST_OK, 0, 0, 0, 0)
        s.send(ret)
    else:
        raise NotHandled()


def go_import(s):
    sendok=0
    while 1:
        b = s.recv(MsgSize)
        if b == b'':
            print("*disconnect*")
            break
        cmd = CmdHdr(*CmdHdr_s.unpack_from(b))
        if cmd.command == USBIP_CMD_SUBMIT:
            cmds = CmdSubmit(*CmdSubmit_s.unpack_from(b, CmdHdr_s.size))

            if cmd.direction == USBIP_DIR_OUT and cmds.tbl:
                data = s.recv(cmds.tbl)
                if data == b'':
                    print("*disconnected* midway")
                    break
            else:
                data = None

            ret = bytearray(MsgSize)
            CmdHdr_s.pack_into(ret, 0,
                USBIP_RET_SUBMIT,
                cmd.seqnum,
                cmd.devid,
                cmd.direction,
                cmd.ep)

            if cmd.ep == 0:
                Setup = namedtuple("Setup", "reqtype req val idx len")
                setup = Setup(*struct.unpack_from("BBHHH", cmds.setup))
                print("SETUP {dir} {tbl} type:{0:x} req:{1:x} val:{2:x} idx:{3:x} len:{4}".format(*setup, dir="IN" if cmd.direction else "OUT", tbl=cmds.tbl))
                if data: print("data:", binascii.hexlify(data))
                try:
                    do_setup(s, ret, setup, data)
                except NotHandled:
                    print("NAKing ep %d %s" % (cmd.ep, "IN" if cmd.direction else "OUT"))
                    CmdSubmitRet_s.pack_into(ret, CmdHdr_s.size, USBIP_ST_NAK, 0, 0, 0, 0)
                    s.send(ret)
            elif cmd.ep == 2: #interrupt endpoint
                CmdSubmitRet_s.pack_into(ret, CmdHdr_s.size, USBIP_ST_NAK, 0, 0, 0, 0)
                s.send(ret)
            elif cmd.ep == 3:
                print(repr(data))
                if data[0] == 13:
                    print("will send ok next")
                    sendok+=1
                CmdSubmitRet_s.pack_into(ret, CmdHdr_s.size, USBIP_ST_OK, 0, 0, 0, 0)
                s.send(ret)
            elif cmd.ep == 4:
                if sendok:
                    print("sending ok")
                    sendok -= 1
                    CmdSubmitRet_s.pack_into(ret, CmdHdr_s.size, USBIP_ST_OK, 4, 0, 0, 0)
                    s.send(ret + b'OK\r\n')
                else:
                    CmdSubmitRet_s.pack_into(ret, CmdHdr_s.size, USBIP_ST_OK, 0, 0, 0, 0)
                    s.send(ret)
            else:
                print("NAKing ep %d %s" % (cmd.ep, "IN" if cmd.direction else "OUT"))
                CmdSubmitRet_s.pack_into(ret, CmdHdr_s.size, USBIP_ST_NAK, 0, 0, 0, 0)
                try:
                    s.send(ret)
                except BrokenPipeError:
                    print("disconnected while trying to send reply")
                    break
        elif cmd.command == USBIP_CMD_UNLINK:
            ret = bytearray(MsgSize)
            urb_seqnum = struct.unpack_from("!I", b, CmdHdr_s.size)[0]
            print("unlink",urb_seqnum)
            CmdHdr_s.pack_into(ret, 0,
                USBIP_RET_UNLINK,
                cmd.seqnum,
                cmd.devid,
                cmd.direction,
                cmd.ep)
            struct.pack_into("!I", ret, CmdHdr_s.size, USBIP_ST_OK)
            s.send(ret)
        else:
            raise Exception("Unknown command :"+str(len(b))+' ' + binascii.hexlify(b).decode())




srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

with closing(srv):
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", 3240))
    srv.listen(1)

    Common_s = struct.Struct("!HHI")
    Common = namedtuple("Common", "version code status")
    USBIP_OP_IMPORT = 0x03
    USBIP_OP_DEVLIST = 0x05
    USBIP_SYSFS_BUS_ID_SIZE = 32
    USBIP_SYSFS_PATH_MAX = 256

    Dev_s = struct.Struct("!%ds%dsIIIHHHBBBBBB"%(USBIP_SYSFS_PATH_MAX, USBIP_SYSFS_BUS_ID_SIZE))
    dev = Dev_s.pack(
        b"/sys/devices/pci0000:00/0000:00:01.2/usb1/1-1",
        b"1-1",
        1,2,2,

        0x16C0,
        0x047A,
        0x0001,

        2,0,0, # device class, subclass, proto
        1,1,2)  # bConfigurationValue bNumConfigurations bNumInterfaces

    If_s = struct.Struct("!BBBx")
    if1 = If_s.pack(2,2,1)
    if2 = If_s.pack(0x0a,0,0)

    s, addr = srv.accept()
    with closing(s):
        op_common = Common(*Common_s.unpack(s.recv(Common_s.size)))
        if op_common.code == USBIP_OP_DEVLIST + 0x8000:
            s.send(Common_s.pack(op_common.version, op_common.code & 0xff, USBIP_ST_OK) + struct.pack("!I", 1) + dev + if1 + if2)
        elif op_common.code == USBIP_OP_IMPORT + 0x8000:
            busid = s.recv(USBIP_SYSFS_BUS_ID_SIZE)
            print("Attach, bus: %s" % busid)
            s.send(Common_s.pack(op_common.version, op_common.code & 0xff, USBIP_ST_OK) + dev)
            go_import(s)
        else:
            raise Exception("Unimplemented op")
