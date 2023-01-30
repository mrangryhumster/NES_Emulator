import sys

with open(sys.argv[1],"rb") as fp:
    data = fp.read()
    data_len = len(data)
    if data_len % 3 != 0:
        print("meh...")
        exit(1)
    index = 0
    for i in range(0,data_len,3):
        print("{{{0:#04x}, {1:#04x}, {2:#04x} }}, ".format(data[i],data[i+1],data[i+2]), end='')
        if (i+3) % 4 == 0:
            print("")
#        print("m_NESPalette[{}] = {{{}, {}, {} }};".format(index,data[i],data[i+1],data[i+2]))
#        index=index+1
        