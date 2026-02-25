from enum import Enum
import rtmidi
import struct
import time

from . import utils
from abc import ABC, abstractmethod

# SysEx Message Constants
SYSEX_START = 0xF0  # SysEx message start byte
SYSEX_END = 0xF7    # SysEx message end byte

# API identifiers
EMBODME_MANFACTURER_ID = [0x00, 0x21, 0x50]
ERAE_HARDWARE_FAMILY_CODE = [0x00, 0x01]
ERAE_TOUCH_FAMILY_MEMBER_CODE = [0x00, 0x01]
ERAE_2_FAMILY_MEMBER_CODE = [0x00, 0x02]

# Specific components of the Erae identifier:
ERAE_MIDI_NETWORK_ID = [0x01]
ERAE_SERVICE = [0x01]

# Erae API (common for service messages)
ERAE_API = [0x04]

# Command identifiers:
API_VERSIONREQUEST_COMMAND = 0x7F
API_MODE_ENABLE = 0x01
API_MODE_DISABLE = 0x02
FINGERSTREAM_COMMAND = 0x10
ZONE_BOUNDARY_REQUEST_COMMAND = 0x10
CLEAR_ZONE_COMMAND = 0x20
DRAW_PIXEL_COMMAND = 0x21
DRAW_RECTANGLE_COMMAND = 0x22
DRAW_IMAGE_COMMAND = 0x23

# Reply identifiers:
NON_FINGER = 0x7F

ZONE_BOUNDARY_REPLY = 0x01
API_VERSION_REPLY = 0x02


class EraeReplyHandler(ABC):
    """Abstract class to have custom hanlder of the API reply"""

    @abstractmethod
    def zone_boundary_reply(self, zone_id: int, width: int, height: int):
        pass

    @abstractmethod
    def finger_detection(self, finger_index: int, zone_id: int, action: int, x: float, y: float, z: float):
        pass

    @abstractmethod
    def api_version(self, api_version: int):
        pass


class EraeReplyPrint(EraeReplyHandler):
    """Erae reply handler that just prints out the messages"""

    def zone_boundary_reply(self, zone_id: int, width: int, height: int):
        print("Zone Boundary Reply Message Received:")
        print(f"Zone ID: {zone_id}")
        if width < 0x7F and height < 0x7F:
            print(f"Boundary: width={width}, height={height}")
        else:
            print("Boundary: Zone ID not valid")

    def finger_detection(self, finger_index: int, zone_id: int, action: int, x: float, y: float, z: float):
        print("Fingerstream Message Received:")
        print(f"Finger ID: {finger_index}")
        print(f"Zone ID: {zone_id}")
        print(f"Action: {action} (0=click, 1=slide, 2=release)")
        print(f"Position: X={x}, Y={y}, Z={z}")

    def api_version(self, version: int):
        print("API Version Message Received:")
        print(f"Version: {version}")


class EraeProduct(Enum):
    ERAE_TOUCH = 0
    ERAE_2 = 1


class EraeAPISysex:
    def __init__(self, erae_product: EraeProduct, midi_in_port: int = 0, midi_out_port: int = 0) -> None:
        """Initialize the EraeSysexAPI object and open the MIDI output port."""
        self.midi_out = rtmidi.MidiOut()
        self.midi_in = rtmidi.MidiIn()

        # List available MIDI output ports
        available_out_ports = self.midi_out.get_ports()
        if available_out_ports:
            self.midi_out.open_port(midi_out_port)  # Open specified MIDI port
            print(f"Opened MIDI port: {available_out_ports[midi_out_port]}")
        else:
            print("No MIDI output ports found.")
            exit()

        # Open MIDI input port to receive data
        available_in_ports = self.midi_out.get_ports()
        if self.midi_in.get_ports():
            # Open the same port for receiving
            self.midi_in.open_port(midi_in_port)
            print(
                f"Listening on MIDI port: {available_in_ports[midi_in_port]}")
        else:
            print("No MIDI input ports found.")
            exit()

        if erae_product == EraeProduct.ERAE_TOUCH:
            self.family_member_code = ERAE_TOUCH_FAMILY_MEMBER_CODE
        elif erae_product == EraeProduct.ERAE_2:
            self.family_member_code = ERAE_2_FAMILY_MEMBER_CODE
        else:
            print("Erae Product undedefined")
            exit()

        # Set the callback to process received messages
        self.midi_in.ignore_types(sysex=False)

    class CallbackDataMidi(object):
        def __init__(self, receiver_prefix: list[int], erae_reply_handler: EraeReplyHandler) -> None:
            self.receiver_prefix = receiver_prefix
            self.erae_reply_handler = erae_reply_handler

    @classmethod
    def create_sysex_message(cls, message_bytes: list[int]):
        """Helper function to wrap bytes with SysEx start(F0) and end(F7)."""

        # Raise exception so we can trace the erroneous message
        for msg in message_bytes:
            if msg < 0 or msg > 127:
                raise Exception(
                    "Sysex msg contains values outside [0x00; 0x7F]: {}".format(
                        ', '.join(hex(x) for x in message_bytes)))

        return [SYSEX_START] + message_bytes + [SYSEX_END]

    @classmethod
    def receive_midi_message(cls, message, callback_data: CallbackDataMidi):
        """Callback function to handle incoming MIDI messages."""
        receiver_prefix = callback_data.receiver_prefix
        erae_handler = callback_data.erae_reply_handler

        msg, timestamp = message

        # print("Received SysEx message: {}".format(
        #     ', '.join(hex(x) for x in msg)))

        # Check if the message is a SysEx message
        if msg[0] == SYSEX_START and msg[-1] == SYSEX_END:
            # Extract the SysEx message (excluding F0 and F7)
            sysex_message = msg[1:-1]

            if len(sysex_message) > len(receiver_prefix) and sysex_message[0: len(receiver_prefix)] == receiver_prefix:
                offset = len(receiver_prefix)

                # Non Finger Stream
                if sysex_message[offset] == NON_FINGER:
                    offset += 1

                    if sysex_message[offset] == ZONE_BOUNDARY_REPLY:
                        offset += 1

                        zone_id = sysex_message[offset]
                        offset += 1

                        width = sysex_message[offset]
                        offset += 1

                        height = sysex_message[offset]

                        erae_handler.zone_boundary_reply(
                            zone_id, width, height)

                    if sysex_message[offset] == API_VERSION_REPLY:
                        offset += 1

                        api_version = sysex_message[offset]
                        offset += 1

                        erae_handler.api_version(api_version)

                # Finger Stream
                else:
                    action_finger_byte = sysex_message[offset]
                    offset += 1

                    zone_id = sysex_message[offset]
                    offset += 1

                    finger_id_data_length = utils.bitized7size(8)
                    finger_id_data = sysex_message[offset:offset +
                                                   finger_id_data_length]
                    offset += finger_id_data_length

                    xyz_data_length = utils.bitized7size(3*4)
                    xyz_data = sysex_message[offset:offset+xyz_data_length]
                    offset += xyz_data_length

                    xyz_data_checksum = sysex_message[offset]

                    # Unibit
                    finger_id_unbitized = utils.unbitize7chksum(
                        finger_id_data)
                    finger_id = struct.unpack(
                        '<Q', bytearray(finger_id_unbitized))[0]

                    xyz_data_unbitized = utils.unbitize7chksum(
                        xyz_data, xyz_data_checksum)
                    x = struct.unpack('<f', bytearray(
                        xyz_data_unbitized[0:4]))[0]
                    y = struct.unpack('<f', bytearray(
                        xyz_data_unbitized[4:8]))[0]
                    z = struct.unpack('<f', bytearray(
                        xyz_data_unbitized[8:12]))[0]

                    action = (action_finger_byte) & 0x07

                    erae_handler.finger_detection(
                        finger_id, zone_id, action, x, y, z)

    def send_sysex_message(self, message_bytes: list[int], verbose: bool = False) -> None:
        """Send a SysEx message."""
        sysex_message = EraeAPISysex.create_sysex_message(message_bytes)
        self.midi_out.send_message(sysex_message)
        if verbose:
            print("Sent SysEx message: {}".format(
                ', '.join(hex(x) for x in sysex_message)))

    def enable_api_mode(self, receiver_prefix: list[int], erae_handler: EraeReplyHandler) -> None:
        """Enable API mode on the Erae device."""
        message = EMBODME_MANFACTURER_ID + ERAE_HARDWARE_FAMILY_CODE + self.family_member_code + ERAE_MIDI_NETWORK_ID + ERAE_SERVICE + ERAE_API + [
            API_MODE_ENABLE
        ] + receiver_prefix

        self.midi_in.set_callback(
            EraeAPISysex.receive_midi_message, EraeAPISysex.CallbackDataMidi(receiver_prefix, erae_handler))

        self.send_sysex_message(message)

    def disable_api_mode(self) -> None:
        """Disable API mode on the Erae device."""
        message = EMBODME_MANFACTURER_ID + ERAE_HARDWARE_FAMILY_CODE + self.family_member_code + ERAE_MIDI_NETWORK_ID + ERAE_SERVICE + ERAE_API + [
            API_MODE_DISABLE
        ]
        self.send_sysex_message(message)

    def send_api_version_request(self, receiver_prefix: list[int], erae_handler: EraeReplyHandler) -> None:
        """Send a Zone Boundary Request message."""
        message = EMBODME_MANFACTURER_ID + ERAE_HARDWARE_FAMILY_CODE + self.family_member_code + ERAE_MIDI_NETWORK_ID + ERAE_SERVICE + ERAE_API + [
            API_VERSIONREQUEST_COMMAND
        ] + receiver_prefix

        self.midi_in.set_callback(
            EraeAPISysex.receive_midi_message, EraeAPISysex.CallbackDataMidi(receiver_prefix, erae_handler))

        self.send_sysex_message(message)

    def send_zone_boundary_request(self, zone_id: int) -> None:
        """Send a Zone Boundary Request message."""
        message = EMBODME_MANFACTURER_ID + ERAE_HARDWARE_FAMILY_CODE + self.family_member_code + ERAE_MIDI_NETWORK_ID + ERAE_SERVICE + ERAE_API + [
            ZONE_BOUNDARY_REQUEST_COMMAND
        ]
        message.append(zone_id)
        self.send_sysex_message(message)

    def send_clear_zone_display(self, zone_id: int) -> None:
        """Send a Clear Zone Display message."""
        message = EMBODME_MANFACTURER_ID + ERAE_HARDWARE_FAMILY_CODE + self.family_member_code + ERAE_MIDI_NETWORK_ID + ERAE_SERVICE + ERAE_API + [
            CLEAR_ZONE_COMMAND
        ]
        message.append(zone_id)
        self.send_sysex_message(message)

    def send_draw_pixel(self, zone_id: int, xpos: int, ypos: int, red: int, green: int, blue: int) -> None:
        """Send a Draw Pixel message."""
        message = EMBODME_MANFACTURER_ID + ERAE_HARDWARE_FAMILY_CODE + self.family_member_code + \
            ERAE_MIDI_NETWORK_ID + ERAE_SERVICE + \
            ERAE_API + [DRAW_PIXEL_COMMAND]
        message.append(zone_id)
        message.append(xpos)
        message.append(ypos)
        message.extend([red, green, blue])  # RGB values
        self.send_sysex_message(message)

    def send_draw_rectangle(self, zone_id: int, xpos: int, ypos: int, width: int, height: int, red: int, green: int, blue: int) -> None:
        """Send a Draw Rectangle message."""
        message = EMBODME_MANFACTURER_ID + ERAE_HARDWARE_FAMILY_CODE + self.family_member_code + ERAE_MIDI_NETWORK_ID + ERAE_SERVICE + ERAE_API + [
            DRAW_RECTANGLE_COMMAND
        ]
        message.append(zone_id)
        message.append(xpos)
        message.append(ypos)
        message.append(width)
        message.append(height)
        message.extend([red, green, blue])  # RGB values
        self.send_sysex_message(message)

    def send_draw_image(self, zone_id: int, xpos: int, ypos: int, width: int, height: int, rgb_data: list[int]) -> None:
        """Send a Draw Image message."""
        message = EMBODME_MANFACTURER_ID + ERAE_HARDWARE_FAMILY_CODE + self.family_member_code + ERAE_MIDI_NETWORK_ID + ERAE_SERVICE + ERAE_API + [
            DRAW_IMAGE_COMMAND
        ]
        message.append(zone_id)
        message.append(xpos)
        message.append(ypos)
        message.append(width)
        message.append(height)
        data_bitized_checksum = utils.bitize7chksum(rgb_data)
        message.extend(data_bitized_checksum)
        self.send_sysex_message(message)

    def close(self) -> None:
        """Close the MIDI output port."""
        self.midi_out.close_port()
        print("MIDI port closed.")
