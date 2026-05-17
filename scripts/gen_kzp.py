import struct
import os
import sys

# Khazar Package Format (.kzp)
# Magic: 0x4B5A504B ('KZPK')
# Header: Magic(4), Name(32), Version(16), FileCount(4)
# Entry: Filename(64), Size(4), Data(size)

MAGIC = 0x4B5A504B

def create_package(name, version, output_file, files):
    """
    files: list of tuples (local_path, target_path_in_os)
    """
    with open(output_file, 'wb') as f:
        # Write Header
        f.write(struct.pack('<I', MAGIC))
        f.write(name.ljust(32, '\0').encode('ascii')[:32])
        f.write(version.ljust(16, '\0').encode('ascii')[:16])
        f.write(struct.pack('<I', len(files)))
        
        for local_path, target_path in files:
            if not os.path.exists(local_path):
                print(f"Error: {local_path} not found")
                continue
                
            with open(local_path, 'rb') as source:
                data = source.read()
                
            # Write Entry
            f.write(target_path.ljust(64, '\0').encode('ascii')[:64])
            f.write(struct.pack('<I', len(data)))
            f.write(data)
            
    print(f"Package {output_file} created successfully with {len(files)} files.")

if __name__ == '__main__':
    if len(sys.argv) < 5:
        print("Usage: python gen_kzp.py <name> <version> <output.kzp> <local_file1:target_path1> ...")
        sys.exit(1)
        
    pkg_name = sys.argv[1]
    pkg_ver = sys.argv[2]
    pkg_out = sys.argv[3]
    pkg_files = []
    
    for arg in sys.argv[4:]:
        local, target = arg.split(':')
        pkg_files.append((local, target))
        
    create_package(pkg_name, pkg_ver, pkg_out, pkg_files)
