I'm trying to learn character devices, it's not the ideal code.

---

Install `linux-api-headers` first, then:

Run the character device:
```
sudo rmmod cdev.ko; make && sudo insmod cdev.ko
```

Run the user application inside `user-app` folder:
```
cd user-app
gcc user-app.c -o user-app.out 
sudo ./user-app.out write hello-world!
sudo ./user-app.out read

sudo ./user-app.out ioctl-write 1337
sudo ./user-app.out ioctl-read
```
