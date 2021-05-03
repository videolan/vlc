
# Use this script with DEBUG_CAPTION enabled in cflags

captions = []

with open("/tmp/caption_dump.hex", "rb") as f:
    byte = f.read(4)
    while byte != b"":
        caption = b"GA94"
        while byte != b"":
            pos = f.tell()
            marker = f.read(4)
            if marker == b"GA94":
                break
            f.seek(pos)
            byte = f.read(1)
            caption += byte
        print(" - GA94 detected, size = {}", len(caption))
        captions.append(caption)

print("There are {} captions".format(len(captions)))


for caption in captions:
    print( '-:' + ''.join('%02X'%(c) for c in caption))
