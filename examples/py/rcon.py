import socket

# Script built for python 3.7.3
# Version 0.1
# Author Hampus Hallkvist
# Date 08/04/2019

def Connect():
    print("Connection...")
    HOST = '127.0.0.1'
    PORT = 15001
    PASSWORD = 'test'
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Connect to server
    try:
        sock.connect((HOST, PORT))
        sock.send(bytearray(PASSWORD, 'utf-8'))
        data = sock.recv(1024)
        print(data)
        # Test accepted
        try:
            sock.send(b'ping')
            sock.recv(1024)
        except:
            return -1
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
        print("Write your command")
        result = input()
        if (len(result) < 1):
            print("To short command")
            continue
        print("Sending " + result)
        try:
            sock.send(bytearray(result, 'utf-8'))
            data = sock.recv(1024)
        except:
            print("Connection lost")
            break
        if (len(data) > 1):
            print("Received ", data)

    input("Press any key to continue")

main()