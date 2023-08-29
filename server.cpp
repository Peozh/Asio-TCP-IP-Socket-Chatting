#include <iostream>
#include <thread>
#include <asio.hpp>
#include <vector>
#include <queue>

using asio::ip::tcp;

// const int MAX_LENGTH = 1024;

struct ClientInfo
{
    std::string clientId;
    std::string clientPassword;
    std::vector<std::string> roomIds;
    std::queue<std::string> receivedMessages;
    std::unique_ptr<std::mutex> mtx;
    inline static std::mutex mtxClients;
};

struct RoomInfo
{
    std::string roomId;
    std::vector<std::string> clientIds;
    std::unique_ptr<std::mutex> mtx;
    inline static std::mutex mtxRooms;
};

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

std::string processCommand(std::string& clientId, const std::string& commands, size_t commands_length, std::unordered_map<std::string, ClientInfo>& clients, std::unordered_map<std::string, RoomInfo>& rooms)
{
    size_t commandIdx = commands.find(' ');
    auto mainCommand = commands.substr(0, commandIdx);

    if (strcmp(mainCommand.data(), "receive") == 0) { // locked
        if (mainCommand.size()+1 == commands.size()) { return "fail receive"; }
        clientId = commands.substr(commandIdx+1, commands_length-mainCommand.size()-1);
        std::string message;
        {
            std::lock_guard<std::mutex> lg(ClientInfo::mtxClients);
            if (clients.count(clientId) == 0) { return "fail receive"; }
            if (clients.at(clientId).receivedMessages.empty()) { return "fail receive"; }
        }
        {
            std::lock_guard<std::mutex> lg(*clients.at(clientId).mtx);
            message = clients.at(clientId).receivedMessages.front();
            clients.at(clientId).receivedMessages.pop();
        }
        std::string command = "success receive " + message;
        std::cout << "log(): " << "clientId:" << clientId << " received " << command << std::endl;
        return command;
    }

    if (strcmp(mainCommand.data(), "send") == 0) { // locked
        if (commandIdx == std::string::npos) return "fail send";
        size_t roomIdIdx = commands.find(' ', commandIdx+1);
        if (roomIdIdx == std::string::npos) return "fail send";
        auto roomId = commands.substr(commandIdx+1, roomIdIdx-commandIdx-1);
        auto message = commands.substr(roomIdIdx+1, commands_length-roomIdIdx-1);
        std::string command = roomId + " " + "message" + " " + clientId + " " + message;
        auto log = command;
        std::cout << "send log(): " << log << std::endl;
        std::cout << "key count: " << rooms.count(roomId) << std::endl;
        for (auto& room_pair : rooms) {
            std::cout << "\troom_key: " << room_pair.first << ", roomId length: " << room_pair.first.size() << std::endl;
        }
        {
            std::lock_guard<std::mutex> lg(RoomInfo::mtxRooms);
            if (rooms.count(roomId) == 0) return "fail send";
            for (auto targetClientId : rooms.at(roomId).clientIds)
            {
                std::lock_guard<std::mutex> lg(*clients.at(targetClientId).mtx);
                clients.at(targetClientId).receivedMessages.push(command);
                // todo: thread cv notify to work
            }
        }
        return "success send";
    }

    auto tokens = parseCommand(commands, ' ');
    std::cout << "log(): commands: " + commands << ", tokens size: " << tokens.size() << std::endl;
    if (tokens.empty()) return "Empty command";
    else if (strcmp(mainCommand.data(), "list") == 0) { // locked
        if (tokens.size() != 1) return "fail list";
        std::string log = "client:" + clientId + " requests room list";
        std::cout << "log(): " << log << std::endl;
        std::string command = "list";
        {
            std::lock_guard<std::mutex> lg(ClientInfo::mtxClients);
            for (const auto roomId : clients.at(clientId).roomIds) { 
                std::lock_guard<std::mutex> lg(*clients.at(clientId).mtx);
                command += " " + roomId; 
            }
        }
        return "success " + command;
    }
    else if (strcmp(mainCommand.data(), "enter") == 0) { // locked
        if (tokens.size() != 2) return "fail enter";
        auto roomId = tokens[1];
        {
            std::lock_guard<std::mutex> lg(ClientInfo::mtxClients);
            if (clients.count(clientId) == 0) return "fail enter";
            for (auto targetRoom : clients.at(clientId).roomIds) {
                std::lock_guard<std::mutex> lg(*clients.at(clientId).mtx);
                if (strcmp(targetRoom.data(), roomId.data()) == 0) return "success enter"; 
            }
        }
        return "fail enter";
    }
    else if (strcmp(mainCommand.data(), "invite") == 0) { // locked
        if (tokens.size() != 3) return "fail invite";
        auto roomId = tokens[1];
        auto userId = tokens[2];
        std::string command = roomId + " " + "announcement" + " " + clientId + " invites " + userId;
        auto log = command;
        std::cout << "log(): " << log << std::endl;
        {
            std::lock_guard<std::mutex> lg(RoomInfo::mtxRooms);
            if (rooms.count(roomId) == 0) return "fail invite";
            bool alreadyInvited = false;
            for (auto targetClientId : rooms.at(roomId).clientIds)
            { 
                std::lock_guard<std::mutex> lg(*clients.at(targetClientId).mtx);
                clients.at(targetClientId).receivedMessages.push(command);
                if (strcmp(targetClientId.data(), userId.data()) == 0) alreadyInvited = true;
            }
            if (!alreadyInvited) {
                std::lock_guard<std::mutex> lg(*clients.at(userId).mtx);
                clients.at(userId).roomIds.push_back(roomId);
            }
            if (!alreadyInvited) {
                std::lock_guard<std::mutex> lg(*rooms.at(roomId).mtx);
                rooms.at(roomId).clientIds.push_back(userId);
            }
        }
        return "success invite";
    }
    else if (strcmp(mainCommand.data(), "create") == 0) { // locked
        if (tokens.size() != 2) return "fail create";
        auto roomId = tokens[1];
        std::string log = roomId + " created by " + clientId;
        std::cout << "log(): " << log << " ... roomId length:" << roomId.length() << std::endl;
        {
            std::lock_guard<std::mutex> lg(RoomInfo::mtxRooms);
            if (rooms.count(roomId) != 0) return "fail create";
            rooms.insert({ roomId, RoomInfo{ roomId, { clientId }, std::make_unique<std::mutex>() } });
        }
        {
            std::lock_guard<std::mutex> lg(*clients.at(clientId).mtx);
            clients.at(clientId).roomIds.push_back(roomId);
        }
        return "success create";
    }
    else if (strcmp(mainCommand.data(), "delete") == 0) { // locked
        if (tokens.size() != 2) return "fail delete";
        auto roomId = tokens[1];
        std::string log = roomId + " deleted by " + clientId;
        std::cout << "log(): " << log << std::endl;
        {
            std::lock_guard<std::mutex> lg(RoomInfo::mtxRooms);
            if (rooms.count(roomId) != 0) return "fail delete";
        }
        {
            std::lock_guard<std::mutex> lg(*rooms.at(roomId).mtx);
            if (rooms.at(roomId).clientIds.size() == 1) rooms.erase(roomId); // todo: lock
        }
        {
            std::lock_guard<std::mutex> lg(ClientInfo::mtxClients);
            for (auto iter = clients.at(clientId).roomIds.begin(); iter != clients.at(clientId).roomIds.end(); ++iter) {
                if (*iter == roomId) {
                    std::lock_guard<std::mutex> lg(*clients.at(clientId).mtx);
                    clients.at(clientId).roomIds.erase(iter);
                }
            }
        }
        return "success delete";
    }
    else if (strcmp(mainCommand.data(), "login") == 0) { // locked
        if (tokens.size() != 3) return "fail login";
        auto userId = tokens[1];
        auto userPassword = tokens[2];
        auto log = "user:" + userId + " trying to login with password:" + userPassword;
        {
            std::lock_guard<std::mutex> lg(ClientInfo::mtxClients);
            if (clients.count(userId) == 0) { std::cout << "log(): " << log << " .. failed" << std::endl; return "fail login"; }
        }
        {
            std::lock_guard<std::mutex> lg(*clients.at(userId).mtx);
            if (strcmp(clients.at(userId).clientPassword.data(), userPassword.data()) == 0) { std::cout << "log(): " << log << " .. success" << std::endl; clientId = userId; }
        }
        return "success login";
    }
    else if (strcmp(mainCommand.data(), "logout") == 0) {
        auto log = "user:" + clientId + " logouted";
        clientId = "";
        // todo: thread cv notify to terminate
        std::cout << "log(): " << log << std::endl;
        return "success logout";
    }
    else if (strcmp(mainCommand.data(), "exit") == 0) {
        auto log = "user:" + clientId + " exited";
        clientId = "";
        // todo: thread cv notify to terminate
        std::cout << "log(): " << log << std::endl;
        return "success exit";
    }
    return "invalid command";
}

void clientSession(tcp::socket socket, std::unordered_map<std::string, ClientInfo>& clients, std::unordered_map<std::string, RoomInfo>& rooms)
{
    std::string clientId = "";
    try
    {
        std::error_code ec;
        for(;;)
        {
            // read
            unsigned long long request_length;
            asio::read(socket, asio::buffer(reinterpret_cast<char*>(&request_length), sizeof(unsigned long long)), ec);
            std::string data(request_length, '\0');
            asio::read(socket, asio::buffer(data, request_length), ec);
            if (ec == asio::stream_errc::eof) { break; }
            else if (ec) throw std::system_error(ec);
            // write
            std::string reply = processCommand(clientId, data, request_length, clients, rooms);
            if (strcmp(reply.data(), "fail receive") != 0) { std::cout << "reply: " << reply << std::endl; }
            if (strcmp(reply.data(), "success exit") == 0) { std::cout << "success exit" << reply << std::endl; break; }
            unsigned long long reply_length = reply.length();
            reply = std::string(sizeof(reply_length), '\0') + reply;
            strcpy_s(reply.data(), sizeof(reply_length), reinterpret_cast<char*>(&reply_length));
            asio::write(socket, asio::buffer(reply, reply.length()));
        }
        socket.shutdown(tcp::socket::shutdown_both, ec);
        socket.close();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    std::cout << "clientSession closed\n";
}

void server(asio::io_context& io_context, unsigned short port)
{
    std::unordered_map<std::string, ClientInfo> clients; 
    std::unordered_map<std::string, RoomInfo> rooms;

    clients.insert({ "client1", ClientInfo{ "client1", "password1", { "room1" }, {}, std::make_unique<std::mutex>() } });
    clients.insert({ "client2", ClientInfo{ "client2", "password2", { "room1" }, {}, std::make_unique<std::mutex>() } });
    rooms.insert({ "room1", RoomInfo{ "room1", { "client1", "client2" }, std::make_unique<std::mutex>() } });

    tcp::acceptor a(io_context, tcp::endpoint(tcp::v4(), port));
    for (;;)
    {
        tcp::socket socket(io_context);
        a.accept(socket);
        std::thread([sock = std::move(socket), &clients, &rooms]() mutable {
            clientSession(std::move(sock), clients, rooms);
        }).detach();
        std::cout << "clientSession thread created" << std::endl;
    }
}

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: server.exe <port>\n";
            return 1;
        }
        asio::io_context io_context;
        server(io_context, std::stoi(argv[1]));
    }
    catch(const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << '\n';
    }
    return 0;
}