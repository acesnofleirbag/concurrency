#!/usr/bin/env python3.8

import argparse
import socket
import threading
import logging
import time

class ReadThread(threading.Thread):
    def __init__(self, name, sock):
        super().__init__()

        self.sock = sock
        self.name = name
        self.bufsize = 8 * 1024

    def run(self):
        fullbuf = b''

        while 1:
            buf = self.sock.recv(self.bufsize)
            fullbuf += buf

            logging.info('{} received {}'.format(self.name, buf))

            if b'1111' in fullbuf:
                break


def start_new_connection(name: str, host: str, port: int):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))

    if sock.recv(1) != b'*':
        logging.error('something is wrong, did not receive \'*\'')

    logging.info('{} connected'.format(name))

    rthread = ReadThread(name, sock)
    rthread.start()

    msg = b'^abc$de^abte$f'

    logging.info('{} sending: {}'.format(name, msg))
    sock.send(msg)
    time.sleep(1.0)

    msg = b'xyz^123'

    logging.info('{} sending: {}'.format(name, msg))
    sock.send(msg)
    time.sleep(1.0)

    msg = b'25$^ab0000$abab'

    logging.info('{} sending: {}'.format(name, msg))
    sock.send(msg)
    time.sleep(0.2)

    rthread.join()
    sock.close()

    logging.info('{} diconnecting'.format(name))


def main():
    logging.basicConfig(
        level=logging.DEBUG,
        format='%(levelname)s:%(asctime)s:%(message)s'
    )

    argparser = argparse.ArgumentParser('Simple TCP Client')

    argparser.add_argument('host', help='server host name')
    argparser.add_argument('port', type=int, help='server port')
    argparser.add_argument('-n', '--num_concurrent', type=int, default=1, help='number of concurrent connections')

    args = argparser.parse_args()

    connections = []

    for i in range(args.num_concurrent):
        name = 'conn{}'.format(i)

        tconn = threading.Thread(target=start_new_connection, args=(name, args.host, args.port))

        tconn.start()
        connections.append(tconn)

    for conn in connections:
        conn.join()


if __name__ == '__main__':
    main()
