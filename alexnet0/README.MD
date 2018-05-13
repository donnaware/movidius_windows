# Introduction
This program can be used to test out the Movidius Neural Compute Stick on a Windows computer. It is based on the
[AlexNet](https://github.com/BVLC/caffe/tree/master/models/bvlc_alexnet) network can be used for image classification.

If you have not already done so, first get your Movidius stick set up on your Windows computer by following the 
instructions in the top level of this git [GitHub](http://github.com/movidius_windows).

## How to Run the program
If you want to just run the program without compiling it, just download these files into a folder and run it,
make sure all these files are in the same folder:
 alexnet0.exe
 gdiplus.dll
 libusb-1.0.dll
 graph
 MvNCAPI.mvcmd
 categories.csv


# How Make the program
You can compile this program using Borland BCB6. If you do not have that compiler you can try porting 
it over to something else, it is pretty basic and it should run on just about any Windows compiler. The
source code is in  [GitHub](http://github.com/movidius_windows/alexnet0/src)

## How to make caffemodel
This demo is based on a pre-compiled caffe model. If you want to make the model, go to the caffe sub-folder in this git


