# Asio-TCP-IP-Socket-Chatting
server-client program using asio socket
## Build options (windows)
```
...,
"-I", "C:\\<enter path>\\asio-1.28.0\\include",
"-l", "ws2_32"
```

## Launch arguments
```
server.exe <server port>
ex) server.exe 3333
```
```
client.exe <server ip address> <server port>
ex) client.exe 127.0.0.1 3333
```

## Client commands
```
login <client id> <client password>
list
create <room id>
delete <room id>
enter <room id>
invite <client id>
send <message>
leave
logout
exit
```
## Default client/room info
```
client1 : password1 { room1 }
client2 : password2 { room1 }
room1 : { client1 client2 }
```
## Examples
```
<client1>
        main thread 1 starting...
Enter command(exit, login <user id> <user password>):
        receiver thread 2 starting...
login client1 password1
        0 0 token string length: 5
        0 0 token string length: 7
        0 1 token string length: 9
commands: login client1 password1, tokens size: 3
reply_length: 13
reply readed
reply: success login
Enter command(exit, logout, create room, delete <room id>, enter <room id>, list):
create room2
        0 0 token string length: 6
        0 1 token string length: 5
commands: create room2, tokens size: 2
reply_length: 14
reply: success create
Enter command(exit, logout, create room, delete <room id>, enter <room id>, list):
enter room2
        0 0 token string length: 5
        0 1 token string length: 5
commands: enter room2, tokens size: 2
reply_length: 13
        entered room:room2
reply: success enter
Enter command(exit, logout, leave, invite <user id>, send <message>):
invite client2
        0 0 token string length: 6
        0 1 token string length: 7
commands: invite client2, tokens size: 2
inviteUser request: invite room2 client2
reply_length: 14
reply: success invite
Enter command(exit, logout, leave, invite <user id>, send <message>):
receiveMessage(): success receive room2 announcement client1 invites client2
        client1: invites client2
receiveMessage(): success receive room2 message client2 hi2
        client2: hi2
send hi1
reply_length: 12
reply: success send
Enter command(exit, logout, leave, invite <user id>, send <message>):
receiveMessage(): success receive room2 message client1 hi1
        client1: hi1

```
```
<client2>
        main thread 1 starting...
Enter command(exit, login <user id> <user password>): 
        receiver thread 2 starting...
login client2 password2
        0 0 token string length: 5
        0 0 token string length: 7
        0 1 token string length: 9
commands: login client2 password2, tokens size: 3
reply_length: 13
reply readed
reply: success login
Enter command(exit, logout, create room, delete <room id>, enter <room id>, list): 
list
        0 1 token string length: 4
commands: list, tokens size: 1
reply_length: 24
reply: success list room1 room2
Enter command(exit, logout, create room, delete <room id>, enter <room id>, list):
enter room2
        0 0 token string length: 5
        0 1 token string length: 5
commands: enter room2, tokens size: 2
reply_length: 13
        entered room:room2
reply: success enter
Enter command(exit, logout, leave, invite <user id>, send <message>):
send hi2
reply_length: 12
reply: success send
Enter command(exit, logout, leave, invite <user id>, send <message>):
receiveMessage(): success receive room2 message client2 hi2
        client2: hi2
receiveMessage(): success receive room2 message client1 hi1
        client1: hi1
```
```
<server>
clientSession thread created
clientSession thread created
        0 0 token string length: 5
        0 0 token string length: 7
        0 1 token string length: 9
log(): commands: login client1 password1, tokens size: 3
log(): user:client1 trying to login with password:password1 .. success
reply: success login
clientSession thread created
clientSession thread created
        0 0 token string length: 5
        0 0 token string length: 7
        0 1 token string length: 9
log(): commands: login client2 password2, tokens size: 3
log(): user:client2 trying to login with password:password2 .. success
reply: success login
        0 0 token string length: 6
        0 1 token string length: 5
log(): commands: create room2, tokens size: 2
log(): room2 created by client1 ... roomId length:5
reply: success create
        0 0 token string length: 5
        0 1 token string length: 5
log(): commands: enter room2, tokens size: 2
reply: success enter
        0 0 token string length: 6
        0 0 token string length: 5
        0 1 token string length: 7
log(): commands: invite room2 client2, tokens size: 3
log(): room2 announcement client1 invites client2
reply: success invite
log(): clientId:client1 received success receive room2 announcement client1 invites client2
reply: success receive room2 announcement client1 invites client2  
        0 1 token string length: 4
log(): commands: list, tokens size: 1
log(): client:client2 requests room list
reply: success list room1 room2
        0 0 token string length: 5
        0 1 token string length: 5
log(): commands: enter room2, tokens size: 2
reply: success enter
send log(): room2 message client2 hi2
key count: 1
        room_key: room2, roomId length: 5
        room_key: room1, roomId length: 5
reply: success send
log(): clientId:client2 received success receive room2 message client2 hi2
reply: success receive room2 message client2 hi2
log(): clientId:client1 received success receive room2 message client2 hi2
reply: success receive room2 message client2 hi2
send log(): room2 message client1 hi1
key count: 1
        room_key: room2, roomId length: 5
        room_key: room1, roomId length: 5
reply: success send
log(): clientId:client2 received success receive room2 message client1 hi1
reply: success receive room2 message client1 hi1
log(): clientId:client1 received success receive room2 message client1 hi1
reply: success receive room2 message client1 hi1

```
