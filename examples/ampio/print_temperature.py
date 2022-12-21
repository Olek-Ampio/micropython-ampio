import ampio
import binascii

def can_msg_cb(mac, data):
    if data[1] == 0x06:
        temperature = data[2] | data[3] << 8
        if temperature > 0:
            temperature = (temperature - 1000) / 10
            print('mac=', hex(mac), 'temperatura=', temperature)

ampio.set_callback(can_msg_cb)