import time
import cv2
import rtmidi

from erae import erae_api_sysex
from erae import utils


def send_image(erae_device: erae_api_sysex.EraeAPISysex) -> None:
    image = cv2.imread('tiger.png')
    if image is None:
        print("Error: Image not found.")
    else:
        # Convert the image to a specific size (for example, 640x480)
        target_width = 24
        target_height = 24
        resized_image = cv2.resize(image, (target_width, target_height))
        resized_image = cv2.cvtColor(resized_image, cv2.COLOR_BGR2RGB)
        if resized_image.dtype != 'uint8' or len(resized_image.shape) != 3 or resized_image.shape[2] != 3:
            resized_image = cv2.convertScaleAbs(resized_image)

        # On Erae::
        # - Origin: bottom left
        # - X: positive right
        # - Y: positive up
        resized_imageinverted = resized_image[::-1, :, :]
        flat_data = resized_imageinverted.flatten()

        erae_device.send_draw_image(
            0, 0, 0, target_width, target_height, flat_data)


def send_draw_rectangle(erae_device: erae_api_sysex.EraeAPISysex) -> None:
    erae_device.send_clear_zone_display(0)
    erae_device.send_draw_rectangle(0, 5, 5, 6, 10, 100, 0, 0)

    # Check if the image was loaded correctly
    image = cv2.imread('tiger.png')
    if image is None:
        print("Error: Image not found.")
    else:
        # Convert the image to a specific size (for example, 640x480)
        target_width = 24
        target_height = 24
        resized_image = cv2.resize(image, (target_width, target_height))
        resized_image = cv2.cvtColor(resized_image, cv2.COLOR_BGR2RGB)

        if resized_image.dtype != 'uint8' or len(resized_image.shape) != 3 or resized_image.shape[2] != 3:
            resized_image = cv2.convertScaleAbs(resized_image)

        resized_imageinverted = resized_image[::-1, :, :]
        flat_data = resized_imageinverted.flatten()

        erae_device.send_clear_zone_display(0)
        erae_device.send_draw_image(
            0, 0, 0, target_width, target_height, flat_data)


if __name__ == "__main__":

    # List available ports and let the user choose which one to open
    midi_in = rtmidi.MidiIn()
    midi_out = rtmidi.MidiOut()
    available_in_ports, available_out_ports = utils.list_ports(
        midi_in, midi_out)

    # Choose input and output ports
    if not available_in_ports:
        print("No MIDI input ports available.")
        exit(1)
    if not available_out_ports:
        print("No MIDI output ports available.")
        exit(1)

    # Initialize the library with the default MIDI port
    input_port = utils.choose_port(available_in_ports, "input")
    output_port = utils.choose_port(available_in_ports, "output")

    erae_device = erae_api_sysex.EraeAPISysex(
        erae_api_sysex.EraeProduct.ERAE_2, input_port, output_port)

    # Enable API Mode
    erae_handler = erae_api_sysex.EraeReplyPrint()
    receiver_prefix = [0x01, 0x02, 0x03]
    erae_device.enable_api_mode(receiver_prefix, erae_handler)

    # Get zone boundary
    erae_device.send_zone_boundary_request(0)

    try:
        last_time = 0
        mode = 0
        while True:
            if (time.time() > last_time + 5):
                mode = (mode % 3)
                if mode == 0:
                    erae_device.send_clear_zone_display(0)
                    send_image(erae_device)

                elif mode == 1:
                    erae_device.send_clear_zone_display(0)
                    erae_device.send_draw_rectangle(0, 5, 5, 6, 10, 100, 0, 0)

                elif mode == 2:
                    erae_device.send_clear_zone_display(0)
                    erae_device.send_draw_pixel(0, 3, 3, 0, 0, 100)

                mode += 1
                last_time = time.time()

            time.sleep(0.05)

    except KeyboardInterrupt:
        print("Exiting...")

    # Close the MIDI port
    erae_device.close()
