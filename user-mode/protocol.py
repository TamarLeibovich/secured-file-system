import socket

MSG_LENGTH = 4  # up to 9999 bytes
ENCRYPTION_KEY = b"SecuredFileSystemProject"


def xor_data(data: bytes, key: bytes) -> bytes:
    """
    this function encrypts/decrypts the data by attempting XOR with the key.
    :param data: the data which needs to be encrypted/decrypted
    :param key: the encryption key used to decrypt/encrypt the data
    :return: the encrypted/decrypted data (in bytes)
    """
    expanded_key = (key * (len(data) // len(key) + 1))[:len(data)]  # making sure the key is in the right length

    # perform XOR operation between each pair of bytes
    output = bytes(d ^ k for d, k in zip(data, expanded_key))

    return output


def format_msg(msg: str) -> bytes:
    """
    this function formats the message: {msg_length}{encrypted_msg}
    :param msg: a string containing the message needed to be sent
    :return: the msg formatted in bytes
    """
    msg_len = str(len(msg)).zfill(MSG_LENGTH)
    msg_encrypted = xor_data(msg.encode(), ENCRYPTION_KEY)
    return msg_len.encode() + msg_encrypted


def read_msg(sock: socket) -> str:
    """
    this function reads the message from the socket, decrypts the data received and returns it as a string.
    :param sock: socket that contains a message which needs to be read
    :return: the message as a string
    """
    msg_len = int(sock.recv(MSG_LENGTH).decode())
    msg = xor_data(sock.recv(msg_len), ENCRYPTION_KEY)
    return msg.decode()

