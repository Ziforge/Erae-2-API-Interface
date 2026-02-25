from functools import reduce


def choose_port(ports, port_type):
    while True:
        try:
            chosen_port = int(
                input(f"Select a {port_type} port (0-{len(ports)-1}): "))
            if 0 <= chosen_port < len(ports):
                return chosen_port
            else:
                print("Invalid selection, please choose a valid port number.")
        except ValueError:
            print("Invalid input, please enter a number.")


def list_ports(midi_in, midi_out):
    # Function to list available MIDI input and output ports

    available_in_ports = midi_in.get_ports()
    available_out_ports = midi_out.get_ports()

    print("\nAvailable MIDI Input Ports:")
    if available_in_ports:
        for i, port in enumerate(available_in_ports):
            print(f"{i}: {port}")
    else:
        print("No available MIDI input ports.")

    print("\nAvailable MIDI Output Ports:")
    if available_out_ports:
        for i, port in enumerate(available_out_ports):
            print(f"{i}: {port}")
    else:
        print("No available MIDI output ports.")

    return available_in_ports, available_out_ports


def clamp(n, a, b):
    """Clamps a number (n) between a range (a to b)."""
    return min(max(n, a), b)


def bitized7size(length: int) -> int:
    # Get size of the resulting 7 bits bytes array obtained when using the bitize7 function
    return length // 7 * 8 + ((1 + length % 7) if (length % 7 > 0) else 0)


def unbitized7size(length: int) -> int:
    # Get size of the resulting 7 bits bytes array obtained when using the bitize7 function
    return length // 8 * 7 + ((length % 8 - 1) if (length % 8 > 0) else 0)


def bitize7chksum(data: list[int], append_checksum: bool = True) -> list[int]:
    # 7-bitize an array of bytes and add the resulting checksum
    bitized7Arr = sum(([sum((el & 0x80) >> (j+1) for j, el in enumerate(data[i:min(i+7, len(data))]))] +
                      [el & 0x7F for el in data[i:min(i+7, len(data))]] for i in range(0, len(data), 7)), [])

    if append_checksum:
        return bitized7Arr + [reduce(lambda x, y: x ^ y, bitized7Arr)]
    else:
        return bitized7Arr


def checksum(data: list[int]) -> int:
    return reduce(lambda x, y: x ^ y, data)


def unbitize7chksum(bitized_data: list[int], checksum: int = None) -> list[int]:
    # Rebuild the 8-bit data from the 7-bit segments
    original_data = [0] * unbitized7size(len(bitized_data))

    i = 0
    outsize = 0
    inlen = len(bitized_data)
    while i < len(bitized_data):
        for j in range(7):
            if (j + 1 + i < inlen):
                original_data[outsize + j] = ((bitized_data[i]
                                               << (j + 1)) & 0x80) | bitized_data[i + j + 1]
        outsize = outsize+7
        i = i + 8

    # Validate checksum (should match the XOR of all the bytes in the bitized data)
    if checksum is not None:
        calculated_checksum = reduce(lambda x, y: x ^ y, bitized_data)
        if checksum != calculated_checksum:
            print("Warning: Checksum mismatch! {} {}".format(
                checksum, calculated_checksum))

    return original_data
