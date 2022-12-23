import uasyncio

async def blink(out_no, period_ms):
    while True:
        ampio.set_out(0xa08b, out_no, 255)
        await uasyncio.sleep_ms(50)
        ampio.set_out(0xa08b, out_no, 0)
        await uasyncio.sleep_ms(period_ms)

async def main():
    uasyncio.create_task(blink(1, 700))
    uasyncio.create_task(blink(2, 400))
    uasyncio.create_task(blink(3, 300))
    await uasyncio.sleep_ms(10_000)

uasyncio.run(main())
