import struct

def create_pe(filename):
    # DOS Header (64 bytes)
    # e_magic (MZ), ..., e_lfanew (Offset 60)
    dos_header = struct.pack('<2s 58x I', b'MZ', 0x40)
    
    # PE Header (24 bytes)
    # Signature (4)
    pe_sig = b'PE\0\0'
    # Machine=i386 (0x14c), Sections=1, Timestamp=0, Symbols=0, SymCount=0, OptHdrSize=0xE0, Char=0x102
    pe_header = struct.pack('<H H I I I H H', 0x014c, 1, 0, 0, 0, 0xE0, 0x0102)
    
    # Optional Header (PE32)
    # Magic=0x10b (2)
    # Linker(2), CodeSize(4), Data(4), Uninit(4) -> 14 bytes padding
    # EntryPoint (4) - Offset 16
    # CodeBase (4) - Offset 20
    # DataBase (4) - Offset 24
    # ImageBase (4) - Offset 28
    # SectionAlign (4) - Offset 32
    # FileAlign (4) - Offset 36
    opt_header = struct.pack('<H 14x I I I I I I 64x', 
                             0x010b,        # Magic
                             0x1000,        # EntryPoint
                             0x1000,        # CodeBase
                             0x2000,        # DataBase
                             0x00400000,    # ImageBase (0x400000)
                             0x1000,        # SectionAlignment
                             0x200          # FileAlignment
                            )
    
    # Section Header (.text) - 40 bytes
    # Name (8), VirtSize(4), VirtAddr(4), RawSize(4), RawPtr(4), ..., Char(4)
    sec_header = struct.pack('<8s I I I I 12x I', 
                             b'.text', 
                             0x1000,        # VirtualSize
                             0x1000,        # VirtualAddress
                             0x200,         # SizeOfRawData
                             0x200,         # PointerToRawData
                             0x60000020     # Characteristics (Read/Execute/Code)
                            )
    
    # Header Construction
    full_header = dos_header + pe_sig + pe_header + opt_header + sec_header
    # Pad header to 512 bytes
    full_header += b'\0' * (512 - len(full_header))
    
    # Code Section (at 512)
    # cli; hlt; (0xFA 0xF4)
    code = b'\xFA\xF4' + b'\0' * (512 - 2)
    
    with open(filename, 'wb') as f:
        f.write(full_header)
        f.write(code)

if __name__ == "__main__":
    create_pe("TEST.EXE")
