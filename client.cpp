#include <asio.hpp>
#include <iostream>
#include <sstream>
#include <memory>
#include <thread>
#include <mutex>

using asio::ip::tcp;

class Client {
public:
    enum ClientState { NeedExit, Offline, Online, Logined, InRoom };
    ClientState state;
    std::string activeRoomId;

private:
    asio::io_context io_context;
    std::unique_ptr<tcp::socket> socket;
    std::unique_ptr<tcp::socket> socket_receiver;
    std::unique_ptr<tcp::resolver> resolver;
    asio::error_code ec;
    std::string userId, userPassword;
    std::mutex mtxActiveRoomId;
    std::mutex mtxReceiverSocket;

public:

    explicit Client() {
        state = Offline;
        this->socket = std::make_unique<tcp::socket>(io_context);
        this->socket_receiver = std::make_unique<tcp::socket>(io_context);
        this->resolver = std::make_unique<tcp::resolver>(io_context);
    }
    ~Client() {
        exit();
    }

    void connectServer(int argc, char* argv[])
    {
        if (argc != 3) { 
            std::cout << "Usage : client.exe <server ip> <server port>" << std::endl; 
            return; 
        }
        asio::connect(*socket, resolver->resolve(argv[1], argv[2]));
        asio::connect(*socket_receiver, resolver->resolve(argv[1], argv[2]));
        std::thread messageReceiver([this]() {
            std::cout << "\treceiver thread " << std::this_thread::get_id() <<" starting..." << std::endl;
            while(this->state != NeedExit)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                auto reply = receiveMessage();
                if (reply.empty()) continue;
                if (strcmp(reply.data(), "fail receive") == 0) continue;
                std::cout << "receiveMessage(): " << reply << std::endl;
                if (this->state != InRoom) continue;
                size_t resultIdx = reply.find(' ', 0);
                auto result = reply.substr(0, resultIdx);
                if (strcmp(result.data(), "success") != 0) continue;
                size_t commandClassIdx = reply.find(' ', resultIdx+1);
                auto commandClass = reply.substr(resultIdx+1, commandClassIdx);
                size_t roomIdIdx = reply.find(' ', commandClassIdx+1);
                auto roomId = reply.substr(commandClassIdx+1, roomIdIdx-commandClassIdx-1);
                {
                    std::lock_guard<std::mutex> lg(this->mtxActiveRoomId);
                    if (roomId != this->activeRoomId) continue;
                }
                size_t classificationIdx = reply.find(' ', roomIdIdx+1);
                auto classification = reply.substr(roomIdIdx+1, classificationIdx-roomIdIdx-1);
                size_t clientIdIdx = reply.find(' ', classificationIdx+1);
                auto clientId = reply.substr(classificationIdx+1, clientIdIdx-classificationIdx-1);
                auto text = reply.substr(clientIdIdx+1);
                std::cout << "\t" + clientId + ": " + text << std::endl;
            }
            std::cout << "\t...receiver thread " << std::this_thread::get_id() <<" terminated" << std::endl;
        });
        messageReceiver.detach();
        state = Online;
    }

    void exit()
    {
        state = NeedExit;
        socket->shutdown(tcp::socket::shutdown_both, ec);
        socket->close();
        socket_receiver->shutdown(tcp::socket::shutdown_both, ec);
        socket_receiver->close();
    }

    void sendRequest(std::string& request)
    {
        unsigned long long request_length = request.length();
        request = std::string(sizeof(request_length), '\0') + request;
        strcpy_s(request.data(), sizeof(request_length), reinterpret_cast<char*>(&request_length));
        asio::write(*socket, asio::buffer(request, request.length()));
    }
    void receiveReply(std::string& reply)
    {
        unsigned long long reply_length = 0;
        asio::read(*socket, asio::buffer(reinterpret_cast<char*>(&reply_length), sizeof(unsigned long long)));
        std::cout << "reply_length: " << reply_length << std::endl;
        reply.resize(reply_length);
        asio::read(*socket, asio::buffer(reply, reply_length), ec);
    }


    void sendReceiverRequest(std::string& request)
    {
        unsigned long long request_length = request.length();
        request = std::string(sizeof(request_length), '\0') + request;
        strcpy_s(request.data(), sizeof(request_length), reinterpret_cast<char*>(&request_length));
        asio::write(*socket_receiver, asio::buffer(request, request.length()));
    }
    void receiveReceiverReply(std::string& reply)
    {
        unsigned long long reply_length = 0;
        asio::read(*socket_receiver, asio::buffer(reinterpret_cast<char*>(&reply_length), sizeof(unsigned long long)));
        reply.resize(reply_length);
        asio::read(*socket_receiver, asio::buffer(reply, reply_length), ec);
    }

    std::string login(std::string userId, std::string userPassword)
    {
        // write
        this->userId = userId;
        this->userPassword = userPassword;
        std::string request = std::string("login") + " " + userId + " " + userPassword;
        sendRequest(request);
        
        // read
        std::string reply;
        receiveReply(reply);
        std::cout << "reply readed" << std::endl;
        if (strcmp(reply.data(), "success login") == 0) state = Logined;
        return reply;
    }

    std::string logout()
    {
        // write
        this->userId = "";
        this->userPassword = "";
        std::string request = "logout";
        sendRequest(request);
        
        // read
        std::string reply;
        receiveReply(reply);
        if (strcmp(reply.data(), "success logout") == 0) state = Online;
        return reply;
    }

    std::vector<std::string> getRoomList()
    {
        // write
        std::string request = "list";
        sendRequest(request);

        // read
        std::string reply;
        receiveReply(reply);

        // parsing
        std::stringstream sstream(reply);
        std::vector<std::string> tokens;
        while (!sstream.fail() && !sstream.eof())
        {
            std::string token;
            std::getline(sstream, token, ' ');
            tokens.push_back(token);
        }
        return tokens;
    }

    std::string createRoom(std::string roomId)
    {
        // write
        std::string request = std::string("create") + " " + roomId;
        sendRequest(request);

        // read
        std::string reply;
        receiveReply(reply);
        return reply;
    }

    std::string deleteRoom(std::string roomId)
    {
        // write
        std::string request = std::string("delete") + " " + roomId;
        sendRequest(request);

        // read
        std::string reply;
        receiveReply(reply);
        return reply;
    }
    
    std::string enterRoom(std::string roomId)
    {
        // write
        std::string request = std::string("enter") + " " + roomId;
        sendRequest(request);

        // read
        std::string reply;
        receiveReply(reply);
        if (strcmp(reply.data(), "success enter") == 0) { 
            std::lock_guard<std::mutex> lg(mtxActiveRoomId); 
            state = InRoom; 
            this->activeRoomId = roomId;
            std::cout << "\tentered room:" + activeRoomId << std::endl;
        }
        return reply;
    }

    std::string leaveRoom()
    {
        // // write
        // std::string request = "leave";
        // socket->write_some(asio::buffer(request, request.length()));
        // // read
        // std::string reply(MAX_LENGTH, '\0');
        // size_t reply_length = socket->read_some(asio::buffer(reply));
        // if (strcmp(reply.data(), "success leave") == 0) 
        { std::lock_guard<std::mutex> lg(mtxActiveRoomId); state = Logined; activeRoomId = ""; }
        // return reply;        
        return "leaved to lobby";
    }

    std::string inviteUser(std::string targetUserId)
    {
        // write
        std::string request = std::string("invite") + " " + activeRoomId + " " + targetUserId;
        std::cout << "inviteUser request: " << request << std::endl;
        sendRequest(request);

        // read
        std::string reply;
        receiveReply(reply);
        return reply;
    }

    std::string sendMessage(std::string roomId, std::string message)
    {
        // write
        std::string request = std::string("send") + " " + roomId + " " + message;
        sendRequest(request);

        // read
        std::string reply;
        receiveReply(reply);
        return reply;
    }

    std::string receiveMessage()
    {
        // std::lock_guard<std::mutex> lg(this->mtxReceiverSocket);
        // write
        if (userId.empty()) return "fail receive";
        if (activeRoomId.empty()) return "fail receive";
        std::string request = std::string("receive") + " " + userId;
        sendReceiverRequest(request);
        // read
        std::string reply;
        receiveReceiverReply(reply);
        return reply;
    }
};


void printCommandText(Client::ClientState state)
{
    switch(state)
    {
    case Client::ClientState::Online:
        std::cout << "Enter command(exit, login <user id> <user password>): \n";
        break;
    case Client::ClientState::Logined:
        std::cout << "Enter command(exit, logout, create room, delete <room id>, enter <room id>, list): \n";
        break;
    case Client::ClientState::InRoom:
        std::cout << "Enter command(exit, logout, leave, invite <user id>, send <message>): \n";
        break;
    default :
        std::cout << "Offline or Exiting ... \n" << std::endl;
        break;
    }
}

std::vector<std::string> parseCommand(const std::string& command, char delimiter)
{
    std::stringstream sstream(command);
    std::vector<std::string> tokens;
    for (;;)
    {
        std::string token;
        std::getline(sstream, token, delimiter);
        std::cout << '\t' << sstream.fail() << " " << sstream.eof() << " token string length: " << token.length() << std::endl;
        if (sstream.fail()) break;
        tokens.push_back(token);
        if (sstream.eof()) break;
    }
    return tokens;
}

bool processCommand(Client& client, const std::string& commands)
{
    enum returnValue : bool { Other, Exit };
    
    size_t idx = commands.find(' ');
    auto mainCommand = commands.substr(0, idx);
    std::string reply;

    if (strcmp(mainCommand.data(), "send") == 0) {
        if (client.state < Client::ClientState::Logined) return Other;
        if (idx == std::string::npos) return Other;
        auto message = commands.substr(idx+1, std::string::npos);
        reply = client.sendMessage(client.activeRoomId, message);
        std::cout << "reply: " << reply << std::endl;
        return Other;
    }

    auto tokens = parseCommand(commands, ' ');
    std::cout << "commands: " + commands << ", tokens size: " << tokens.size() << std::endl;
    if (tokens.empty()) return Other;
    

    if (strcmp(mainCommand.data(), "list") == 0) {
        if (client.state != Client::ClientState::Logined) return Other;
        if (tokens.size() != 1) return Other;
        auto roomList = client.getRoomList();
        for (size_t idx = 0; idx < roomList.size(); ++ idx) { reply += roomList[idx] + " "; }
    }
    else if (strcmp(mainCommand.data(), "enter") == 0) {
        if (client.state != Client::ClientState::Logined) return Other;
        if (tokens.size() != 2) return Other;
        auto roomId = tokens[1];
        reply = client.enterRoom(roomId);
    }
    else if (strcmp(mainCommand.data(), "leave") == 0) {
        if (client.state != Client::ClientState::InRoom) return Other;
        reply = client.leaveRoom();
    }
    else if (strcmp(mainCommand.data(), "invite") == 0) {
        if (client.state != Client::ClientState::InRoom) return Other;
        if (tokens.size() != 2) return Other;
        auto userId = tokens[1];
        reply = client.inviteUser(userId);
    }
    else if (strcmp(mainCommand.data(), "create") == 0) {
        if (client.state != Client::ClientState::Logined) return Other;
        if (tokens.size() != 2) return Other;
        auto roomId = tokens[1];
        reply = client.createRoom(roomId);
    }
    else if (strcmp(mainCommand.data(), "delete") == 0) {
        if (client.state != Client::ClientState::Logined) return Other;
        if (tokens.size() != 2) return Other;
        auto roomId = tokens[1];
        reply = client.deleteRoom(roomId);
    }
    else if (strcmp(mainCommand.data(), "login") == 0) {
        if (client.state != Client::ClientState::Online) return Other;
        if (tokens.size() != 3) return Other;
        auto userId = tokens[1];
        auto userPassword = tokens[2];
        reply = client.login(userId, userPassword);
    }
    else if (strcmp(mainCommand.data(), "logout") == 0) {
        if (client.state < Client::ClientState::Logined) return Other;
        reply = client.logout();
    }
    else if (strcmp(mainCommand.data(), "exit") == 0) {
        return Exit;
    }
    std::cout << "reply: " << reply << std::endl;
    return Other;
}

int main(int argc, char* argv[])
{
    try 
    {
        std::cout << "\tmain thread " << std::this_thread::get_id() <<" starting..." << std::endl;
        // state: Offline
        Client client;
        client.connectServer(argc, argv);
        if (client.state != Client::ClientState::Online) return 1;
        // state: Online
        for (;;) // login
        {
            printCommandText(client.state);
            std::string commands;
            std::cin.clear();
            std::getline(std::cin, commands, '\n');
            bool isExit = processCommand(client, commands);
            if (isExit) break;
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception : " << e.what() << "\n";
    }
    std::cout << "\t...main thread " << std::this_thread::get_id() <<" terminated" << std::endl;
    return 0;
}