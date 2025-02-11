import serial
from struct import unpack, pack
from typing import List, Tuple, Optional
from datetime import datetime


class ModbusRtuFramer:
    def __init__(self):
        self.buffer = bytearray()

    def calculate_crc(self, data: bytes) -> int:
        """Calculate Modbus RTU CRC16 without lookup table"""
        crc = 0xFFFF
        for byte in data:
            crc ^= byte
            for _ in range(8):
                if crc & 0x0001:
                    crc = (crc >> 1) ^ 0xA001
                else:
                    crc >>= 1
        return crc

    def validate_crc(self, data: bytes) -> bool:
        """Validate the CRC of received message"""
        message = data[:-2]  # Message without CRC
        received_crc = unpack("<H", data[-2:])[0]  # Little-endian CRC
        calculated_crc = self.calculate_crc(message)
        if received_crc != calculated_crc:
            print(
                f"Data: {' '.join(f'{byte:02x}' for byte in data)}: Received CRC: {received_crc:#04x}, Calculated CRC: {calculated_crc:#04x}"
            )
        return received_crc == calculated_crc

    def parse_request(self, data: bytes) -> Optional[dict]:
        """Parse a complete Modbus RTU request"""
        if len(data) < 4:  # Unit ID + Function Code + CRC (2 bytes)
            return None

        try:
            if not self.validate_crc(data):
                print("CRC validation failed")
                return None

            unit_id = data[0]
            function_code = data[1]

            request = {
                "unit_id": unit_id,
                "function_code": function_code,
                "data": data[2:-2],  # Remove unit ID, function code, and CRC
            }

            # Parse different function codes
            # [ID][FC][ADDR][NUM][CRC]
            if function_code == 0x01:  # Read Coils
                start_addr, quantity = unpack(">HH", request["data"][:4])
                request["start_address"] = start_addr
                request["quantity"] = quantity

            return request

        except Exception as e:
            print(f"Error parsing request: {e}")
            return None

    def build_read_coils_response(self, unit_id: int, coil_values: List[bool]) -> bytes:
        """Build response for Read Coils (0x01) request"""
        # Calculate number of bytes needed for coil values
        num_bytes = (len(coil_values) + 7) // 8

        # Pack coils into bytes
        coil_bytes = bytearray(num_bytes)
        for i, value in enumerate(coil_values):
            if value:
                coil_bytes[i // 8] |= 1 << (i % 8)

        # Build response PDU
        function_code = 0x01
        response = bytes([unit_id, function_code, num_bytes]) + bytes(coil_bytes)

        # Calculate and append CRC
        crc = self.calculate_crc(response)
        response += pack("<H", crc)  # Append CRC in little-endian

        return response


class ModbusRtuServer:
    def __init__(self, port: str, baudrate: int = 9600):
        self.port = port
        self.baudrate = baudrate
        self.serial = None
        self.framer = ModbusRtuFramer()
        # Simulated coil values for demonstration
        self.coils = [False] * (4 + 16)

    def connect(self) -> bool:
        """Open serial port connection"""
        try:
            self.serial = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=1,
            )
            return True
        except Exception as e:
            print(f"Connection failed: {e}")
            return False

    def handle_read_coils_request(self, request: dict) -> bytes:
        """Handle Read Coils request and generate response"""
        start_address = request["start_address"]
        quantity = request["quantity"]

        # Validate request
        if quantity < 1 or quantity > 2000:
            return self.build_error_response(
                request["unit_id"],
                request["function_code"],
                0x03,  # Illegal data value
            )

        if start_address + quantity > len(self.coils):
            return self.build_error_response(
                request["unit_id"],
                request["function_code"],
                0x02,  # Illegal data address
            )

        # Get requested coil values
        coil_values = self.coils[start_address : start_address + quantity]

        # Build response
        return self.framer.build_read_coils_response(request["unit_id"], coil_values)

    def build_error_response(
        self, unit_id: int, function_code: int, exception_code: int
    ) -> bytes:
        """Build Modbus error response"""
        response = bytes([unit_id, function_code | 0x80, exception_code])
        crc = self.framer.calculate_crc(response)
        return response + pack("<H", crc)

    def read_serial(self) -> Optional[dict]:
        """Read data from serial port and parse Modbus request"""
        if not self.serial:
            print("Not connected")
            return None

        try:
            # Wait for first byte (unit ID)
            first_byte = self.serial.read(1)
            if not first_byte:
                return None

            self.framer.buffer = bytearray(first_byte)

            # Read function code
            function_code = self.serial.read(1)
            if not function_code:
                return None

            self.framer.buffer.extend(function_code)

            # Read based on function code
            if function_code[0] == 0x01:  # Read Coils
                data = self.serial.read(4)  # Start address (2) + Quantity (2)
                if not data or len(data) != 4:
                    return None
                self.framer.buffer.extend(data)

            # Read CRC (2 bytes)
            crc = self.serial.read(2)
            if not crc or len(crc) != 2:
                return None

            self.framer.buffer.extend(crc)

            # Parse the complete message
            request = self.framer.parse_request(bytes(self.framer.buffer))
            self.framer.buffer.clear()
            return request

        except Exception as e:
            print(f"Error reading serial: {e}")
            return None

    def set_coil(self, address: int, value: bool):
        """Set coil value at specified address"""
        if 0 <= address < len(self.coils):
            self.coils[address] = value

    def close(self):
        """Close the serial connection"""
        if self.serial:
            self.serial.close()
            self.serial = None


# Keep the same last_values and change_coil functions
last_values = {
    1: [False, True] * 8,
    2: [True, False] * 8,
    3: [False, True] * 8,
    4: [True, False] * 8,
}


def change_coil(unit_id, server):
    vals = last_values[unit_id]
    for i in range(4, 20):
        server.set_coil(i, vals[i - 4])
    last_values[unit_id] = vals[::-1]


def main():
    server = ModbusRtuServer("/dev/tty.usbserial-11210", 115200)

    last_call = {
        1: datetime.now(),
        2: datetime.now(),
        3: datetime.now(),
        4: datetime.now(),
    }

    if server.connect():
        print("Connected!")
        try:
            while True:
                request = server.read_serial()
                if request:
                    print("Received Modbus request:")
                    print(
                        f"Unit ID: {request['unit_id']}",
                        f"Function: {request['function_code']}",
                        end="",
                    )
                    if "start_address" in request:
                        print(
                            f" Address: {request['start_address']}",
                            f"Qty: {request['quantity']}",
                        )
                    else:
                        print()

                    if request["function_code"] == 0x01:  # Read Coils
                        print(
                            "Last call: ",
                            round(
                                (
                                    datetime.now() - last_call[request["unit_id"]]
                                ).total_seconds()
                                * 1000,
                                1,
                            ),
                            "ms",
                        )
                        last_call[request["unit_id"]] = datetime.now()
                        response = server.handle_read_coils_request(request)
                        server.serial.write(response)
                        print("Sent response for Read Coils request")
                        change_coil(request["unit_id"], server)
                    print("---")
        except KeyboardInterrupt:
            print("\nClosing connection...")
        finally:
            server.close()


if __name__ == "__main__":
    main()
