````
gcc client.c -o client && gcc server.c -o server
cd minifs
make
dd bs=4096 count=100 if=/dev/zero of=image
./mkfs ./image
sudo insmod MINIFS_fs.ko
mkdir test
sudo mount -o loop -t MINIFS_fs image ./test
sudo chmod 0777 ./test -R
cd test && ../../server 
````
run client in another terminal inside sc folder
./client ‘command’
available commands cd, ls, mkdir, cp, rm, mv, cat, echo, tee, touch
‘>’ could be used for remote fs, ‘-stdin’ for stdin
examples of commands
````
./client touch tmp1
./client -stdin cat ‘>’ tmp2
./client cat tmp2
````
