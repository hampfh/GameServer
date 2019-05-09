import socket

# Script built for python 3.7.3
# Version 0.1
# Author Hampus Hallkvist
# Date 08/04/2019

def Connect():
    HOST = '127.0.0.1'
    PORT = 15000
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Connect to server
    try:
        sock.connect((HOST, PORT))
        data = sock.recv(1024)
        print(repr(data))
        sock.send(b'Hello world')
    except:
        print("Connection refused")
        return -1
    return sock
    
def main():
    data = ""

    sock = Connect()
    if (sock == -1):
        return

    # Game loop
    while True:
        data = sock.recv(1024)
        if (len(data) > 1):
            print("Received", data)
        sock.send(b"output")

main()
