I'm trying to learn character devices, it's not the ideal code.

---

Run the character device:
```
sudo rmmod cdev.ko; make && sudo insmod cdev.ko
```

Run the user application inside `user-app` folder:
```
cd user-app
gcc -g user-app.c -o user-app.out 
sudo ./user-app.out write hello-world!
sudo ./user-app.out read
```