## dbserver
* Run once
```
dbserver <port>
```
## dbclient
* Can run multiple
```
dbserver <hostname> <port>
```
___
### Build & Run
```
make
dbserver 3648
```
Another shell instance
```
dbclient 127.0.0.1 3648
```
