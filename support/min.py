from struct import pack
from binascii import crc32


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


def create_packet():
    frame = MINFrame(
        0x01,
        bytes(
            [
                0x00,
                0xC2,
                0x1,
                0x00,  # Baud(32)
                0x28,
                0x00,  # Periodic Interval(16)
                0x4,  # Bus(8)
                0x1,  # pr_size(8)
                0x33,  # Slave Address(8)
                0x1,  # Function, read coils(8)
                0x4,
                0x0,  # Address(16)
                0x10,
                0x00,  # Length(16)
            ]
        ),
    )

    return on_wire_bytes(frame)


def main():
    print(bytes_to_hexstr(create_packet()))


if __name__ == "__main__":
    main()
