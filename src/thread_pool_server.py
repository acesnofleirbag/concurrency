#!/usr/bin/env python3.8

from concurrent.futures import ThreadPoolExecutor
import argparse
import enum
import socket
import sys

class ProcessState(enum.Enum):
    WAITTING = 0
    PROCESSING = 1

def start_new_connection(sockobj, client_addr):
    print('{} connected'.format(client_addr))

    sockobj.sendall(b'*')

    state = ProcessState.WAITTING

    while True:
        try:
            buf = sockobj.recv(1024)

            if not buf:
                break
        except IOError as e:
            break

        for b in buf:
            if state == ProcessState.WAITTING:
                if b == ord(b'^'):
                    state = ProcessState.PROCESSING
            elif state == ProcessState.PROCESSING:
                if b == ord(b'$'):
                    state = ProcessState.WAITTING
                else:
                    sockobj.send(bytes([b + 1]))
            else:
                assert False

    print('{} done'.format(client_addr))

    sys.stdout.flush()
    sobkobj.close()


def main():
    argparser = argparse.ArgumentParser('Thread Pool Server')

    argparser.add_argument('host', help='server host name')
    argparser.add_argument('port', type=int, help='server port')
    argparser.add_argument('-n', type=int, default=64, help='number of threads in pool')

    args = argparser.parse_args()

    pool = ThreadPoolExecutor(args.n)

    sockobj = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sockobj.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sockobj.bind(('127.0.0.1', args.port))
    sockobj.listen(15)

    try:
        while True:
            client_sock, client_addr = sockobj.accept()
            pool.submit(start_new_connection, client_sock, client_addr)
    except KeyboardInterrupt as e:
        print('err: {}'.format(e))
        sockobj.close()


if __name__ == '__main__':
    main()
