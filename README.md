# movidius_windows
How to use the Movidius Neural Compute Stick on Windows

Yes, it is possible to use the Movidius Neural Compute Stick (https://developer.movidius.com/) on windows.
The standard SDK for it is for Ubuntu or Raspian but with a little fiddling you can get it to work, here's how.


## Step 1:
The first step is to download one of the standard windows apps I have already compiled. I recommend starting with the Alexnet app.
That app will allow you to configure and test your stick and make sure the right drivers are installed. 

First plug your stick into a USB port on your computer, it will go through an install, if that worked right you should 
see this ![USB Device properties](images/installed2.jpg) and when you go to computer->manage->device manager you should
see an item in there like this ![USB Device properties](images/devices1.jpg)

Right click on the Winusb device and display properties, the hardware ID;s should be these 
![USB Device properties](images/properties1.jpg)

If all that worked OK, then download the test alexnet0 test app, that app uses a pre-compiled graph file
which contains the "smarts" of the app. You can compile graph files yourself if you go to the caffe sub-folder of this git.

## Step 2:
Now that you have the Movidius device discovered by Windows, a word about how this little puppy works. It is really two USB devices
The first device is the boot device, the sole purpose of which is to upload the firmware (MvNCAPI.mvcmd) up into the Myriad2
(https://www.movidius.com/myriad2) chip inside of it. The Myriad2 is the real brains of the thing. So when you first plug
in the device the thing shown above (VID=03E7 PID=2150) that is the booter upper. To get it to become a neural net computer,
you have to upload that firmware file. To do that, fire up the alexnet0.exe program and go to the "Output" tab and click Open.

When you click open whilst the Movidius is plugged in, it will upload that firmware (MvNCAPI.mvcmd) file into the Myriad2 chip,
when it does that, it stops being the booter upper (VID=03E7 PID=2150) and turns into a new USB device, as if you unplugged the
booter upper and plugged in a new thing, you will here that "doo dink" sound that Windows makes when you plug and unplug 
a USB device.  You may wonder, why didn't they just make a compound USB device?  Beats the heck out of me, ask the Movidius guys,
I am sure there is a good explanation, probably has something to do with Linux or something.


