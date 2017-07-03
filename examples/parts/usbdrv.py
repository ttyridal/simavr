#!/usr/bin/env python3
from collections import namedtuple
from contextlib import closing
import socket
import struct
import binascii
import unittest
import time

Common_s = struct.Struct("!HHI")
Common = namedtuple("Common", "version code status")
USBIP_OP_IMPORT = 0x03
USBIP_VER = 0x0106
USBIP_SYSFS_BUS_ID_SIZE = 32
USBIP_SYSFS_PATH_MAX = 256
USBIP_CMD_SUBMIT = 0x0001

class TestStuff(unittest.TestCase):
    def usbip_attach(self, s):
        s.send(struct.pack("!HHxxxx32s", USBIP_VER, USBIP_OP_IMPORT | 0x8000, b"1-1"))
        X = struct.Struct("!HHI")
        x = X.unpack(s.recv(X.size))
        self.assertEqual(x[0], USBIP_VER)
        self.assertEqual(x[1], USBIP_OP_IMPORT)
        self.assertEqual(x[2], 0, "status ok import-op")


        X = struct.Struct("!%ds%dsIIIHHHBBBBBB"%(USBIP_SYSFS_PATH_MAX, USBIP_SYSFS_BUS_ID_SIZE))
        x = X.unpack(s.recv(X.size))
        x = (x[0].strip(b'\0'),  x[1].strip(b'\0'), x[2], x[3], x[4], x[5], x[6], x[7], x[8], x[9], x[10],x[11],x[12],x[13],)
        print("attached {} {}\n {} {} {}\n {:x} {:x} {:x}\n {} {} {}\n {} {} {}".format(*x))


    def get_device_desc(self, s):
        print("get device desc")
        s.send(struct.pack("!IIIIIIiiii",
            USBIP_CMD_SUBMIT, 1, 0x10002, 1, 0,
            0x200, 64, 0, 0, 0) +
            struct.pack("BBHHH", 0x80, 6, 0x100, 0, 64)
        )

        X = struct.Struct("!IIIIIIiiiixxxxxxxx")
        x = X.unpack(s.recv(X.size))
        self.assertEqual(x[0], 3)
        self.assertEqual(x[1], 1)
        self.assertEqual(x[2], 0x10002)
        self.assertEqual(x[3], 1, "direction")
        self.assertEqual(x[4], 0, "ep")

        self.assertEqual(x[5], 0) #status
        self.assertEqual(x[6], 18)
        self.assertEqual(s.recv(x[6]), b'\x12\x01\x00\x02\x02\x00\x00\x10\xc0\x16\x7a\x04\x00\x01\x01\x02\x03\x01')

    def get_device_qualifier(self, s):
        print("get something")
        s.send(struct.pack("!IIIIIIiiii",
            USBIP_CMD_SUBMIT, 1, 0x10002, 1, 0,
            0x200, 64, 0, 0, 0) +
            struct.pack("BBHHH", 0x80, 6, 0x600, 0, 10)
        )

        X = struct.Struct("!IIIIIIiiiixxxxxxxx")
        x = X.unpack(s.recv(X.size))
        self.assertEqual(x[0], 3)
        self.assertEqual(x[1], 1)
        self.assertEqual(x[2], 0x10002)
        self.assertEqual(x[3], 1, "direction")
        self.assertEqual(x[4], 0, "ep")

        self.assertEqual(x[5], 1) #status


    def get_config_descriptor(self, s):
        print("get config1")
        s.send(struct.pack("!IIIIIIiiii",
            USBIP_CMD_SUBMIT, 1, 0x10002, 1, 0,
            0x200, 9, 0, 0, 0) +
            struct.pack("BBHHH", 0x80, 6, 0x200, 0, 9)
        )

        X = struct.Struct("!IIIIIIiiiixxxxxxxx")
        x = X.unpack(s.recv(X.size))
        self.assertEqual(x[0], 3)
        self.assertEqual(x[1], 1)
        self.assertEqual(x[2], 0x10002)
        self.assertEqual(x[3], 1, "direction")
        self.assertEqual(x[4], 0, "ep")

        self.assertEqual(x[5], 0) #status
        self.assertEqual(x[6], 9)
        self.assertEqual(s.recv(x[6]), b'\t\x02C\x00\x02\x01\x00\xc02')


        print("get config2")
        s.send(struct.pack("!IIIIIIiiii",
            USBIP_CMD_SUBMIT, 1, 0x10002, 1, 0,
            0x200, 67, 0, 0, 0) +
            struct.pack("BBHHH", 0x80, 6, 0x200, 0, 67)
        )

        X = struct.Struct("!IIIIIIiiiixxxxxxxx")
        x = X.unpack(s.recv(X.size))
        self.assertEqual(x[0], 3)
        self.assertEqual(x[1], 1)
        self.assertEqual(x[2], 0x10002)
        self.assertEqual(x[3], 1, "direction")
        self.assertEqual(x[4], 0, "ep")

        self.assertEqual(x[5], 0) #status
        self.assertEqual(x[6], 67)
        data = b'\x09\x02\x43\x00\x02\x01\x00\xc0\x32\x09\x04\x00\x00\x01\x02\x02\x01\x00\x05\x24\x00\x10\x01\x05\x24\x01\x01\x01\x04\x24\x02\x06\x05\x24\x06\x00\x01\x07\x05\x82\x03\x10\x00\x40\x09\x04\x01\x00\x02\x0a\x00\x00\x00\x07\x05\x03\x02\x20\x00\x00\x07\x05\x84\x02 \x00\x00'
        self.assertEqual(s.recv(x[6]), data)

    def get_strings(self, s):
        s.send(struct.pack("!IIIIIIiiii",
            USBIP_CMD_SUBMIT, 1, 0x10002, 1, 0,
            0x200, 255, 0, 0, 0) +
            struct.pack("BBHHH", 0x80, 6, 0x300, 0, 255)
        )

        X = struct.Struct("!IIIIIIiiiixxxxxxxx")
        x = X.unpack(s.recv(X.size))
        self.assertEqual(x[0], 3)
        self.assertEqual(x[1], 1)
        self.assertEqual(x[2], 0x10002)
        self.assertEqual(x[3], 1, "direction")
        self.assertEqual(x[4], 0, "ep")

        self.assertEqual(x[5], 0) #status
        self.assertEqual(x[6], 4)

        self.assertEqual(s.recv(x[6]), b'\x04\x03\t\x04')

        print("get name1")
        s.send(struct.pack("!IIIIIIiiii",
            USBIP_CMD_SUBMIT, 1, 0x10002, 1, 0,
            0x200, 255, 0, 0, 0) +
            struct.pack("BBHHH", 0x80, 6, 0x302, 0x409, 255)
        )

        X = struct.Struct("!IIIIIIiiiixxxxxxxx")
        x = X.unpack(s.recv(X.size))
        self.assertEqual(x[0], 3)
        self.assertEqual(x[1], 1)
        self.assertEqual(x[2], 0x10002)
        self.assertEqual(x[3], 1, "direction")
        self.assertEqual(x[4], 0, "ep")

        self.assertEqual(x[5], 0) #status
        self.assertEqual(x[6], 22)

        self.assertEqual(s.recv(x[6]), b'\x16\x03U\x00S\x00B\x00 \x00S\x00e\x00r\x00i\x00a\x00l\x00')


        print("get manuf")
        s.send(struct.pack("!IIIIIIiiii",
            USBIP_CMD_SUBMIT, 1, 0x10002, 1, 0,
            0x200, 255, 0, 0, 0) +
            struct.pack("BBHHH", 0x80, 6, 0x301, 0x409, 255)
        )

        X = struct.Struct("!IIIIIIiiiixxxxxxxx")
        x = X.unpack(s.recv(X.size))
        self.assertEqual(x[0], 3)
        self.assertEqual(x[1], 1)
        self.assertEqual(x[2], 0x10002)
        self.assertEqual(x[3], 1, "direction")
        self.assertEqual(x[4], 0, "ep")

        self.assertEqual(x[5], 0) #status
        self.assertEqual(x[6], 20)
        self.assertEqual(s.recv(x[6]), b'\x14\x03Y\x00o\x00u\x00r\x00 \x00N\x00a\x00m\x00e\x00')


        print("get serial")
        s.send(struct.pack("!IIIIIIiiii",
            USBIP_CMD_SUBMIT, 1, 0x10002, 1, 0,
            0x200, 255, 0, 0, 0) +
            struct.pack("BBHHH", 0x80, 6, 0x303, 0x409, 255)
        )

        X = struct.Struct("!IIIIIIiiiixxxxxxxx")
        x = X.unpack(s.recv(X.size))
        self.assertEqual(x[0], 3)
        self.assertEqual(x[1], 1)
        self.assertEqual(x[2], 0x10002)
        self.assertEqual(x[3], 1, "direction")
        self.assertEqual(x[4], 0, "ep")

        self.assertEqual(x[5], 0) #status
        self.assertEqual(x[6], 12)
        self.assertEqual(s.recv(x[6]), b'\x0c\x031\x002\x003\x004\x005\x00')

    def set_config(self, s):
        print("send set config")
        s.send(struct.pack("!IIIIIIiiii",
            USBIP_CMD_SUBMIT, 1, 0x10002, 0, 0,
            0x200, 0, 0, 0, 0) +
            struct.pack("BBHHH", 0x00, 9, 0x01, 0, 0)
        )

        X = struct.Struct("!IIIIIIiiiixxxxxxxx")
        x = X.unpack(s.recv(X.size))
        self.assertEqual(x[0], 3)
        self.assertEqual(x[1], 1)
        self.assertEqual(x[2], 0x10002)
        self.assertEqual(x[3], 0, "direction")
        self.assertEqual(x[4], 0, "ep")

        self.assertEqual(x[5], 0) #status
        self.assertEqual(x[6], 0)

    def set_line_coding(self, s):
        print("send cdc ctrl 1")
        s.send(struct.pack("!IIIIIIiiii",
            USBIP_CMD_SUBMIT, 1, 0x10002, 0, 0,
            0x200, 7, 0, 0, 0) +
            struct.pack("BBHHH", 0x21, 0x20, 0x0, 0, 7) + b'\x80\x25\x00\x00\x00\x00\x08'
        )

        X = struct.Struct("!IIIIIIiiiixxxxxxxx")
        x = X.unpack(s.recv(X.size))
        self.assertEqual(x[0], 3)
        self.assertEqual(x[1], 1)
        self.assertEqual(x[2], 0x10002)
        self.assertEqual(x[3], 0, "direction")
        self.assertEqual(x[4], 0, "ep")

        self.assertEqual(x[5], 0) #status
        self.assertEqual(x[6], 0)



        print("send cdc ctrl 2")
        s.send(struct.pack("!IIIIIIiiii",
            USBIP_CMD_SUBMIT, 1, 0x10002, 0, 0,
            0x200, 7, 0, 0, 0) +
            struct.pack("BBHHH", 0x21, 0x20, 0x0, 0, 7) + b'\x00\xe1\x00\x00\x00\x00\x08'
        )

        X = struct.Struct("!IIIIIIiiiixxxxxxxx")
        x = X.unpack(s.recv(X.size))
        self.assertEqual(x[0], 3)
        self.assertEqual(x[1], 1)
        self.assertEqual(x[2], 0x10002)
        self.assertEqual(x[3], 0, "direction")
        self.assertEqual(x[4], 0, "ep")

        self.assertEqual(x[5], 0) #status
        self.assertEqual(x[6], 0)


        # sleep some
        print("send cdc ctrl 3")
        s.send(struct.pack("!IIIIIIiiii",
            USBIP_CMD_SUBMIT, 1, 0x10002, 0, 0,
            0x200, 0, 0, 0, 0) +
            struct.pack("BBHHH", 0x21, 0x22, 0x3, 0, 0)
        )

        X = struct.Struct("!IIIIIIiiiixxxxxxxx")
        x = X.unpack(s.recv(X.size))
        self.assertEqual(x[0], 3)
        self.assertEqual(x[1], 1)
        self.assertEqual(x[2], 0x10002)
        self.assertEqual(x[3], 0, "direction")
        self.assertEqual(x[4], 0, "ep")

        self.assertEqual(x[5], 0) #status
        self.assertEqual(x[6], 0)

    def poll_int_ep(self, s):
        print("poll_int")
        s.send(struct.pack("!IIIIIIiiii",
            USBIP_CMD_SUBMIT, 1, 0x10002, 1, 2,
            0x200, 64, 0, 0, 0) +
            struct.pack("BBHHH", 0, 0, 0, 0, 0)
        )
        X = struct.Struct("!IIIIIIiiiixxxxxxxx")
        x = X.unpack(s.recv(X.size))

        self.assertEqual(x[0], 3)
        self.assertEqual(x[1], 1)
        self.assertEqual(x[2], 0x10002)
        self.assertEqual(x[3], 1, "direction")
        self.assertEqual(x[4], 2, "ep")

        self.assertEqual(x[5], 1) #status
        self.assertEqual(x[6], 0, 'length') #status

    def write_bulk(self, s, dta=b"H", ep=3):
        print("write_bulk")
        max_retry = 2
        for retry in range(max_retry + 1):
            direct = 0
            s.send(struct.pack("!IIIIIIiiii",
                USBIP_CMD_SUBMIT, 1, 0x10002, direct, ep,
                0x200, len(dta), 0, 0, 0) +
                struct.pack("BBHHH", 0, 0, 0, 0, 0) + dta
            )
            X = struct.Struct("!IIIIIIiiiixxxxxxxx")
            x = X.unpack(s.recv(X.size))

            if (x[5] == 1 and retry < max_retry): # write naked
                time.sleep(0.1)
                continue

            self.assertEqual(x[0], 3)
            self.assertEqual(x[1], 1, 'seqnum')
            self.assertEqual(x[2], 0x10002)
            self.assertEqual(x[3], direct, "direction")
            self.assertEqual(x[4], ep, "ep")

            self.assertEqual(x[5], 0, 'status')
            self.assertEqual(x[6], 0, 'length')
            break

    def read_bulk(self, s, ep=4):
        print("read_bulk")
        max_retry = 2
        for retry in range(max_retry + 1):
            direct = 1
            s.send(struct.pack("!IIIIIIiiii",
                USBIP_CMD_SUBMIT, 1, 0x10002, direct, ep,
                0x200, 64, 0, 0, 0) +
                struct.pack("BBHHH", 0, 0, 0, 0, 0)
            )
            X = struct.Struct("!IIIIIIiiiixxxxxxxx")
            x = X.unpack(s.recv(X.size))

            if (x[5] == 1 and retry < max_retry): # write naked
                time.sleep(0.1)
                continue

            self.assertEqual(x[0], 3)
            self.assertEqual(x[1], 1, 'seqnum')
            self.assertEqual(x[2], 0x10002)
            self.assertEqual(x[3], direct, "direction")
            self.assertEqual(x[4], ep, "ep")

            self.assertEqual(x[5], 0, 'status')
            self.assertEqual(x[6], 0, 'length')
            break

    def xtest1(self):
        with socket.create_connection(("localhost", 3240)) as s:

            self.usbip_attach(s)

            self.get_device_desc(s)

            self.get_device_qualifier(s)

            self.get_config_descriptor(s)

            self.get_strings(s)

            self.set_config(s)

#             self.set_line_coding(s)

            for x in range(10):
                self.poll_int_ep(s)

            for x in range(10):
                self.write_bulk(s)
                time.sleep(1)
                self.read_bulk(s)




    def test1(self):
#        for i in range(10):
#            print("="*20,"iteration",i,"="*20)
            self.xtest1()


if __name__ =="__main__":
    unittest.main()
