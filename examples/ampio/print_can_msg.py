import ampio
import binascii

def can_msg_cb(mac, data):
    print(hex(mac), binascii.hexlify(data))

ampio.set_callback(can_msg_cb)