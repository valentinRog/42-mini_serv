import subprocess
import socket
import time

def test_wrong_number_of_arguments():
    result = subprocess.run(['./mini_serv'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    assert result.returncode == 1
    assert result.stderr.decode().strip() == 'Wrong number of arguments'

def test_client_message_relay():
    PORT = 80
    ADDRESS = '127.0.0.1'
    server = subprocess.Popen(['./mini_serv', str(PORT)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.1)

    c1 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    c1.connect((ADDRESS, PORT))
    c2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    c2.connect((ADDRESS, PORT))

    assert c1.recv(1024).decode() == 'server: client 1 just arrived\n'

    c1.close()

    assert c2.recv(1024).decode() == 'server: client 0 just left\n'

    server.kill()
