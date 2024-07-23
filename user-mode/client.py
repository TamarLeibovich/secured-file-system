import socket
import password_prompt
import protocol

password = ''  # the correct password given by the server is stored here


def retrieve_access_status(client: socket) -> bool:
    """
    this function prompts the user for password whenever clicked on a file in the file explorer. Receives the password
    entered from password_prompt's main function, compares it to the actual password, sends to the server True\False,
    a boolean value, and returns that value.
    :param client: client's socket
    :return: whether password is correct or not: True\False
    """
    global password

    password_input = password_prompt.main()
    value = (password_input == password)
    client.sock.send(protocol.format_msg(str(value)))
    client.close_socket()
    return value


class Client:
    def __init__(self):
        """
        this function initializes a socket object which is meant to connect on port 62123
        """
        self.sock = socket.socket()
        self.addr = ('127.0.0.1', 62100)

    def connect_client(self):
        """
        this function connects the socket to the server
        """
        self.sock.connect(self.addr)

    def get_pass(self):
        """
        this function gets the correct password from the server and stores it in the password variable for later use
        """
        global password
        password = protocol.read_msg(self.sock)

    def close_socket(self):
        """
        this function closes the socket created in the initialization function
        """
        self.sock.close()


def main():
    client = Client()
    client.connect_client()
    client.get_pass()
    return retrieve_access_status(client)


if __name__ == '__main__':
    main()
