import socket
from datetime import datetime
import protocol
import threading
import initialize_password_window

password = ''  # the correct password that the user had set when initialized the server


class SingletonServer(type):
    """
    this class is made to make sure there is only one instance of the server object,
    given the fact that attempting to create multiple servers at the same address causes an error
    """

    _instances = {}

    def __call__(cls, *args, **kwargs):
        if cls not in cls._instances:
            instance = super().__call__(*args, **kwargs)
            cls._instances[cls] = instance

        return cls._instances[cls]


def init_password():
    """
    this function sets the correct password, via user input.
    """
    global password
    password = initialize_password_window.main()


def get_log(access_status: str) -> str:
    """
    this function creates the log message that will be saved to logging.txt file.
    :param access_status: a string variable, whether True\False.
    :return: the log message
    """

    print(f'access status : {access_status}')

    if access_status == "False":
        return f'[!] access to file system denied at {datetime.now()}'

    return f'[+] access to file system approved at {datetime.now()}'


def write_logging(access_status: str):
    """
    this function writes the log text to logging.txt file
    :param access_status: a string value, whether True\False.
    """
    with open('logging.txt', 'a') as logging_file:
        log = get_log(access_status)

        logging_file.write(f'{log}\n')


class Server(metaclass=SingletonServer):

    def __init__(self):
        """
        this function initializes the server socket and variables needed.
        """
        self.addr = ('0.0.0.0', 62100)
        self.sock = socket.socket()
        self.client = None

    def create_server(self):
        """
        this function binds the server to an address and starts on listening to upcoming clients.
        """
        self.sock.bind(self.addr)
        self.sock.listen()

    def accept_client(self):
        """
        this function accepts a new client.
        """
        self.client, _ = self.sock.accept()

    def get_access_status(self) -> str:
        """
        this function gets the access status (whether password entered by the user is correct or not) from the client.
        :return: the access status (a string containing True\False).
        """
        access = protocol.read_msg(self.client)
        self.client.close()
        return access

    def send_pass(self):
        """
        this function sends the correct password which had been set when first initialized the server to the client.
        """
        self.client.send(protocol.format_msg(password))


def handle_client(server: socket):
    """
    this function handles clients which is called using multi threading technique.
    :param server: a socket object which is the server.
    """
    server.send_pass()
    access_status = server.get_access_status()
    write_logging(access_status)


def main():
    server = Server()
    server.create_server()
    init_password()

    while True:
        server.accept_client()
        thread = threading.Thread(target=handle_client, args=(server,))
        thread.start()


if __name__ == '__main__':
    main()
