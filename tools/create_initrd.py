import sys
import os

root_dir = sys.argv[1]
initrd_dir = sys.argv[2]

initrd = open(initrd_dir, 'wb')

initrd.write('INRD'.encode())  # Magic

dirs = []
files_raw = []


def pad_bytes(bytes_arr, padlen):
    padding = (0).to_bytes(padlen-len(bytes_arr), 'little')
    return bytes_arr+padding

# Pass 1


cur_ind = 4
folder_ind = {}

walk = []

for i in os.walk(root_dir):
    walk.append(i)

for root_folder, folders, files in walk:
    # Each dir:
    # uint32_t num_entites
    # For each entity:
    # unint8 flags
    # uint32 start_block (TBF)
    # uint32 size (0 for directories)
    # char name[128]

    # each file:
    # just data
    print(root_folder, folders, files)
    folder_ind[root_folder] = cur_ind
    cur_dir = []
    cur_dir.append((len(folders) + len(files)).to_bytes(4, 'little'))
    for folder in folders:
        cur_dir.append((0x02).to_bytes(1, 'little'))
        cur_dir.append((0).to_bytes(4, 'little'))
        cur_dir.append((0).to_bytes(4, 'little'))
        cur_dir.append(pad_bytes(folder.encode(), 128))

    for file in files:
        fdata = ""
        with open(os.path.join(root_folder, file), 'rb') as f:
            fdata = f.read()
            files_raw.append(fdata)

        cur_dir.append((0x01).to_bytes(1, 'little'))
        cur_dir.append((0).to_bytes(4, 'little'))
        cur_dir.append((len(fdata)).to_bytes(4, 'little'))
        cur_dir.append(pad_bytes(file.encode(), 128))

    for bt in cur_dir:
        cur_ind += len(bt)
    dirs.append(cur_dir)

cur_dir_ind = 0
cur_file_ind = 0

for root_folder, folders, files in walk:
    find = 0
    for folder in folders:
        dirs[cur_dir_ind][2 + find*4] = folder_ind[os.path.join(
            root_folder, folder)].to_bytes(4, 'little')
        find += 1
    for file in files:
        dirs[cur_dir_ind][2+find*4] = cur_ind.to_bytes(4, 'little')
        find += 1
        cur_ind += len(files_raw[cur_file_ind])
    cur_dir_ind += 1


for dir in dirs:
    for bt in dir:
        # print(bt)
        initrd.write(bt)


for file in files_raw:
    initrd.write(file)

initrd.close()
