from struct import pack
from binascii import crc32
import argparse


def int32_to_bytes(value: int) -> bytes:
    return pack(">I", value)


def bytes_to_hexstr(bytesequence: bytes) -> str:
    return "".join(f"{b:02X}" for b in bytesequence)


class MINFrame:
    def __init__(
        self,
        min_id: int,
        payload: bytes,
        ack_or_reset=False,
    ):
        if ack_or_reset:
            self.min_id = min_id
        else:
            self.min_id = min_id & 0x3F
        self.payload = payload


HEADER_BYTE = 0xAA
STUFF_BYTE = 0x55
EOF_BYTE = 0x55


def on_wire_bytes(frame: MINFrame) -> bytes:
    """
    Get the on-wire byte sequence for the frame,

    including stuff bytes after every 0xaa 0xaa pair
    """
    prolog = bytes([frame.min_id, len(frame.payload)]) + frame.payload

    crc = crc32(prolog, 0)
    raw = prolog + int32_to_bytes(crc)

    stuffed = bytearray([HEADER_BYTE, HEADER_BYTE, HEADER_BYTE])

    count = 0

    for i in raw:
        stuffed.append(i)
        if i == HEADER_BYTE:
            count += 1
            if count == 2:
                stuffed.append(STUFF_BYTE)
                count = 0
        else:
            count = 0

    stuffed.append(EOF_BYTE)

    return bytes(stuffed)


def create_packet(baudrate, interval, bus, slave, function, address, length):
    frame = MINFrame(
        0x01,
        bytes(
            [
                baudrate & 0xFF,  # Baud rate
                (baudrate >> 8) & 0xFF,
                (baudrate >> 16) & 0xFF,
                (baudrate >> 24) & 0xFF,
                interval & 0xFF,  # Periodic Interval(16)
                (interval >> 8) & 0xFF,
                bus,  # Bus(8)
                0x1,  # array length(8)
                slave,  # Slave Address(8)
                function,  # Function, 1= read coils(8), 3= read hr
                address & 0xFF,  # Address(16)
                (address >> 8) & 0xFF,
                length & 0xFF,  # Length(16)
                (length >> 8) & 0xFF,
            ]
        ),
    )

    return on_wire_bytes(frame)


def main():
    """
    Main function to generate a MIN frame packet.

    This function parses command line arguments to generate a MIN frame packet
    with the specified parameters.

    Command line arguments:
    --baudrate : int : Baud rate (32-bit integer)
    --interval : int : Periodic interval (16-bit integer)
    --bus      : int : Bus (8-bit integer)
    --slave    : int : Slave address (8-bit integer)
    --function : int : Function code (8-bit integer)
    --address  : int : Address (16-bit integer)
    --length   : int : Length (16-bit integer)

    Example:
    python min.py --baudrate 115200 --interval 1000 --bus 1 --slave 1 --function 3 --address 0 --length 2
    """
    parser = argparse.ArgumentParser(description="Generate MIN frame packet.")
    parser.add_argument(
        "--baudrate", type=int, required=True, help="Baud rate (32-bit integer)"
    )
    parser.add_argument(
        "--interval", type=int, required=True, help="Periodic interval (16-bit integer)"
    )
    parser.add_argument("--bus", type=int, required=True, help="Bus (8-bit integer)")
    parser.add_argument(
        "--slave", type=int, required=True, help="Slave address (8-bit integer)"
    )
    parser.add_argument(
        "--function", type=int, required=True, help="Function code (8-bit integer)"
    )
    parser.add_argument(
        "--address", type=int, required=True, help="Address (16-bit integer)"
    )
    parser.add_argument(
        "--length", type=int, required=True, help="Length (16-bit integer)"
    )

    args = parser.parse_args()

    packet = create_packet(
        args.baudrate,
        args.interval,
        args.bus,
        args.slave,
        args.function,
        args.address,
        args.length,
    )

    print(bytes_to_hexstr(packet))


if __name__ == "__main__":
    main()
