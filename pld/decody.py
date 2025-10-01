def to_bin(h, width):
    return format(h, width)

def walk_map(m, width):
    for (start_offset, identifier) in sorted(m, key=lambda p: p[0]):
        print(to_bin(start_offset, width) + ' ' + identifier)

        # identify set bits
        set_bits = []

        address = 0
        while address <= 20:
            if(start_offset) & 1 != 0:
                set_bits.append(address)
            address += 1 # increment address line
            start_offset = start_offset >> 1 # shift out the lowest bit

        formatted_addresses = map(lambda a: f'A{a}', set_bits)
        print(' bits set:', ', '.join(formatted_addresses))

memory_map = [
    ( 0xB8000, 'CGA RAM 0xB8000' ),
    ( 0xB9000, 'CGA RAM 0xB9000' ),
    ( 0xBA000, 'CGA RAM 0xBA000' ),
    ( 0xBB000, 'CGA RAM 0xBB000' ),
]
"""
memory_map = [ # PV7
    (0x8000, 'slot 0-2, first 8k'),
    (0xa000, 'slot 0-2, second 8k'),
    (0xc000, 'slot 0-3, first 8k'),
    (0xe000, 'slot 0-2, second 8k'),
    (0xffff, 'memory top')
]
"""

io_map = [
    ( 0x3D4, 'CRTC 3D4h' ),
    ( 0x3D5, 'CRTC 3D5h' ),
    ( 0x3D8, 'CGA 3D8h' ),
    ( 0x3D9, 'CGA 3D9h' ),
    ( 0x3DA, 'CGA 3DAh' ),

]

print('Memory Map 20b:')
walk_map(memory_map, '020b')

print('I/O Map 10b:')
walk_map(io_map, '010b')
