from pymodbus.server import StartSerialServer

from pymodbus.device import ModbusDeviceIdentification
from pymodbus.datastore import ModbusSequentialDataBlock
from pymodbus.datastore import ModbusSlaveContext, ModbusServerContext

from pymodbus.transaction import ModbusRtuFramer

import logging
import argparse


formatter = logging.Formatter(
    "%(asctime)s.%(msecs)03d %(levelname)s: %(message)s", datefmt="%Y-%m-%d %H:%M:%S"
)
logging.basicConfig(
    format=formatter._style._fmt, datefmt=formatter.datefmt, level=logging.INFO
)
log = logging.getLogger()
log.setLevel(logging.DEBUG)


class CallbackDataBlock(ModbusSequentialDataBlock):
    """A datablock that stores the new value in memory,.

    and passes the operation to a message queue for further processing.
    """

    def __init__(self, addr, values):
        """Initialize."""
        super().__init__(addr, values)

    def setValues(self, address, values):
        """Set the requested values of the datastore."""
        super().setValues(address, values)
        txt = f"Callback from setValues with address {address}, value {values}"
        log.debug(txt)

    def getValues(self, address, count=1):
        """Return the requested values from the datastore."""
        result = super().getValues(address, count=count)
        txt = f"Callback from getValues with address {address}, count {count}, data {result}"
        log.info(txt)
        return result

    def validate(self, address, count=1):
        """Check to see if the request is in range."""
        result = super().validate(address, count=count)
        txt = f"Callback from validate with address {address}, count {count},data {result}"
        log.debug(txt)
        return result


store = ModbusSlaveContext(
    co=CallbackDataBlock(0x0000, [0xFF, 0x00] * 8),
    hr=CallbackDataBlock(0x0000, [0xFF, 0x00] * 8),
)
context = ModbusServerContext(slaves=store, single=True)

identity = ModbusDeviceIdentification()
identity.VendorName = "Pymodbus"
identity.ProductCode = "PM"
identity.VendorUrl = "http://github.com/riptideio/pymodbus/"
identity.ProductName = "Pymodbus Server"
identity.ModelName = "Pymodbus Server"
identity.MajorMinorRevision = "1.0"

parser = argparse.ArgumentParser(description="Modbus Serial Server")
parser.add_argument(
    "--port",
    type=str,
    required=True,
    help="Serial port to use for the Modbus server (e.g., /dev/ttyUSB0)",
)
args = parser.parse_args()
serial_port = args.port

StartSerialServer(
    context=context,  # Data storage
    identity=identity,  # server identify
    timeout=1,  # waiting time for request to complete
    port=serial_port,  # serial port
    framer=ModbusRtuFramer,  # The framer strategy to use
    stopbits=1,  # The number of stop bits to use
    bytesize=8,  # The bytesize of the serial messages
    # parity="E",  # Which kind of parity to use
    baudrate=115200,  # The baud rate to use for the serial device
    # handle_local_echo=False,  # Handle local echo of the USB-to-RS485 adaptor
    # ignore_missing_slaves=True,  # ignore request to a missing slave
    # broadcast_enable=False,  # treat slave_id 0 as broadcast address,
    # strict=True,  # use strict timing, t1.5 for Modbus RTU
)
