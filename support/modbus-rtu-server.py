import serial
import random
import argparse
from datetime import datetime


class ModbusRTUServer:
    def __init__(self, port, baudrate, start_addr, memory_size):
        self.port = port
        self.baudrate = baudrate
        self.start_addr = start_addr
        self.memory_size = memory_size
        self.coils = [False] * memory_size
        self.holding_registers = [0] * memory_size
        self.last_read_times = {i: None for i in range(1, 5)}
        self.last_coil_changed = 0
        self.last_register_changed = 0

    def calculate_crc(self, data):
        crc = 0xFFFF
        for byte in data:
            crc ^= byte
            for _ in range(8):
                if crc & 0x0001:
                    crc = (crc >> 1) ^ 0xA001
                else:
                    crc >>= 1
        return crc.to_bytes(2, "little")

    def verify_crc(self, data):
        received_crc = int.from_bytes(data[-2:], "little")
        calculated_crc = int.from_bytes(self.calculate_crc(data[:-2]), "little")
        return received_crc == calculated_crc

    def handle_read_coils(self, slave_id, data):
        start_addr = int.from_bytes(data[2:4], "big")
        quantity = int.from_bytes(data[4:6], "big")

        if not (
            0 <= start_addr - self.start_addr < self.memory_size
            and 0 < quantity <= 2000
            and start_addr - self.start_addr + quantity <= self.memory_size
        ):
            return None

        # Change one random coil
        change_idx = self.last_coil_changed % self.memory_size
        self.coils[change_idx] = not self.coils[change_idx]
        self.last_coil_changed = change_idx + 1

        # Prepare response
        byte_count = (quantity + 7) // 8
        response = bytearray([slave_id, 0x01, byte_count])

        for i in range(byte_count):
            byte = 0
            for bit in range(min(8, quantity - i * 8)):
                if self.coils[start_addr - self.start_addr + i * 8 + bit]:
                    byte |= 1 << bit
            response.append(byte)

        print(
            f"Read Coils: Slave {slave_id}, Start Addr {start_addr}, Quantity {quantity}, Response: {response.hex().upper()}",
            end="",
        )

        return response

    def handle_read_holding_registers(self, slave_id, data):
        start_addr = int.from_bytes(data[2:4], "big")
        quantity = int.from_bytes(data[4:6], "big")

        if not (
            0 <= start_addr - self.start_addr < self.memory_size
            and 0 < quantity <= 125
            and start_addr - self.start_addr + quantity <= self.memory_size
        ):
            return None

        # Change one random register
        change_idx = self.last_register_changed % self.memory_size
        self.holding_registers[change_idx] = random.randint(0, 65535)
        self.last_register_changed = change_idx + 1

        # Prepare response
        response = bytearray([slave_id, 0x03, quantity * 2])

        for i in range(quantity):
            value = self.holding_registers[start_addr - self.start_addr + i]
            response.extend(value.to_bytes(2, "big"))

        print(
            f"Read Holding Reg: Slave {slave_id}, Start Addr {start_addr}, Quantity {quantity}, Response: {response.hex().upper()}",
            end="",
        )

        return response

    def run(self, time):
        with serial.Serial(self.port, self.baudrate, timeout=1) as ser:
            while True:
                if ser.in_waiting >= 8:  # Minimum Modbus RTU frame size
                    request = ser.read(8)

                    if not self.verify_crc(request):
                        print("CRC failed!")
                        continue

                    slave_id = request[0]
                    if not 1 <= slave_id <= 4:
                        continue

                    function_code = request[1]
                    current_time = datetime.now()

                    if not time:
                        print()
                    else:
                        if self.last_read_times[slave_id]:
                            time_diff = (
                                current_time - self.last_read_times[slave_id]
                            ).total_seconds()
                            print(
                                f" - Last read for slave {slave_id}: {time_diff * 1000:.0f} ms"
                            )

                    self.last_read_times[slave_id] = current_time

                    response = None
                    if function_code == 0x01:  # Read Coils
                        response = self.handle_read_coils(slave_id, request)
                    elif function_code == 0x03:  # Read Holding Registers
                        response = self.handle_read_holding_registers(slave_id, request)

                    if response:
                        response.extend(self.calculate_crc(response))
                        ser.write(response)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Modbus RTU Server")
    parser.add_argument("port", help="Serial port")
    parser.add_argument("--baud", type=int, default=115200, help="Baudrate")
    parser.add_argument("--start", type=int, default=0, help="Starting address")
    parser.add_argument("--size", type=int, default=100, help="Memory size")
    parser.add_argument("--time", action="store_true", help="Print time between reads")

    args = parser.parse_args()
    server = ModbusRTUServer(args.port, args.baud, args.start, args.size)
    server.run(args.time)
