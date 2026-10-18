import asyncio
from bleak import BleakClient, BleakScanner

TEMP = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
RELAY = "cba1d466-344c-4be3-ab3f-189f80dd7518"

async def main():
    dev = await BleakScanner.find_device_by_name("TermoSense", timeout=15)
    async with BleakClient(dev) as c:
        await c.start_notify(TEMP, lambda _, d: print("notify:", d.decode()))
        await asyncio.sleep(3)
        await c.write_gatt_char(RELAY, b"1")   # relay on
        await asyncio.sleep(3)
        await c.write_gatt_char(RELAY, b"0")
        await asyncio.sleep(3)

asyncio.run(main())