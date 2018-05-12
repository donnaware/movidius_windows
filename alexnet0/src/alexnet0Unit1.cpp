//---------------------------------------------------------------------------
// AlexNet network can be used for image classification.
//---------------------------------------------------------------------------
#include <vcl.h>
#include <stdio.h>
#include <stdlib.h>
#pragma hdrstop
#pragma link "gdiplus.lib"
#pragma link "libusb.lib"
#pragma link "mvncapi.lib"
#include "alexnet0Unit1.h"
#include "mvnc.h"
#include "fp16.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "AdvGlowButton"
#pragma link "AdvOfficePager"
#pragma link "AdvOfficePagerStylers"
#pragma link "AdvOfficeStatusBar"
#pragma link "AdvOfficeStatusBarStylers"
#pragma link "AdvPicture"
#pragma link "AdvMemo"
#pragma link "AdvGrid"
#pragma link "BaseGrid"
#pragma link "AdvmWS"
#pragma resource "*.dfm"
//---------------------------------------------------------------------------
TForm1 *Form1;
//---------------------------------------------------------------------------
__fastcall TForm1::TForm1(TComponent* Owner) : TForm(Owner)
{
    EXEPath = ExtractFilePath(Application->ExeName);
    usb_library_load();
}
//---------------------------------------------------------------------------
void __fastcall TForm1::FormClose(TObject *Sender, TCloseAction &Action)
{
    usb_library_unload();
}
//---------------------------------------------------------------------------
void __fastcall TForm1::FormShow(TObject *Sender)
{
    Memo1->Clear();
    Memo1->Lines->Add("ready");
    Memo2->Clear();
	if(FileExists("debug file")) {
		Memo2->Lines->LoadFromFile("debug.log");
	}
	else {
		TStringList *dbg = new TStringList();
		dbg->Add("debug file");
		dbg->SaveToFile("debug.log");
		delete dbg;
    }
	if(FileExists("categories.csv")) {
        StringGrid1->LoadFromCSV("categories.csv");
    }
    PageControl1->ActivePageIndex = 2;
    ResizeImage();
}
//---------------------------------------------------------------------------
void __fastcall TForm1::ExitButton1Click(TObject *Sender)
{
	if(graphFileBuf) free(graphFileBuf);
    if(OpenButton1->Caption == "close") OpenButton1->Click();
    Close();
}
//---------------------------------------------------------------------------
void __fastcall TForm1::AdvOfficePage2Enter(TObject *Sender)
{
    Memo2->Lines->LoadFromFile("debug.log");
}
//---------------------------------------------------------------------------
void __fastcall TForm1::OpenButton1Click(TObject *Sender)
{
    if(OpenButton1->Caption == "open") {
        char devName[NAME_SIZE];
        mvncStatus retCode = mvncGetDeviceName(0, devName, NAME_SIZE);
        if(retCode == MVNC_OK) {     // failed to get device name, maybe none plugged in.
            Memo1->Lines->Add("NCS device found: " + AnsiString(devName));
        }
        else {   // got device name
            Memo1->Lines->Add("Error - No NCS devices found.");
    	    Memo1->Lines->Add("    mvncStatus value: " + AnsiString(retCode));
            return;
        }
        retCode = mvncOpenDevice(devName, &deviceHandle);      // Try to open the NCS device via the device name
        if(retCode == MVNC_OK) {
            Memo1->Lines->Add("NCS device Opened.");
        }
        else {         // failed to open the device.
            Memo1->Lines->Add("Error - Could not open NCS device.");
    	    Memo1->Lines->Add("    mvncStatus value: " + AnsiString(retCode));
            return;
        }
        // deviceHandle is ready to use now.
        // Pass it to other NC API calls as needed and close it when finished.
        Memo1->Lines->Add("NCS Device opened normally.");
        Memo2->Lines->LoadFromFile("debug.log");
        OpenButton1->Caption = "close";
        StatusBar1->Panels->Items[0]->Text = "VCS device opened";
    }
    else {
        mvncStatus retCode = mvncCloseDevice(deviceHandle);
        deviceHandle = NULL;
        if(retCode == MVNC_OK) {
            Memo1->Lines->Add("NCS device Closed.");
        }
        else {
            Memo1->Lines->Add("Error - Could not close NCS device.\n");
        	Memo1->Lines->Add("    mvncStatus value: " + AnsiString(retCode));
            return;
        }
        Memo1->Lines->Add("NCS Device Closed normally.");
        Memo2->Lines->LoadFromFile("debug.log");
        OpenButton1->Caption = "open";
        StatusBar1->Panels->Items[0]->Text = "VCS device closed";
        StatusBar1->Panels->Items[1]->Text = "graph not loaded";
        StatusBar1->Panels->Items[2]->Text = AnsiString(status_strings[0]);
    }
}
//---------------------------------------------------------------------------
void __fastcall TForm1::statusButton1Click(TObject *Sender)
{
    int retCode = getDeviceStatus();
//  Memo1->Lines->Add("mvncStatus value: " + AnsiString(retCode));
    Memo1->Lines->Add("Status: " + AnsiString(status_strings[retCode&0x7]));
    StatusBar1->Panels->Items[2]->Text = AnsiString(status_strings[retCode&0x7]);
}
//---------------------------------------------------------------------------
void __fastcall TForm1::TabSheet2Enter(TObject *Sender)
{
    Memo2->Lines->LoadFromFile("debug.log");
}
//---------------------------------------------------------------------------
// Load a graph file,  caller must free the buffer returned.
//---------------------------------------------------------------------------
void __fastcall TForm1::LoadGraphButton1Click(TObject *Sender)
{
    AnsiString graphfile = EXEPath + AnsiString(GRAPH_FILE_NAME);

	FILE *fp = fopen(graphfile.c_str(), "rb");
	if(fp == NULL) {
       Memo1->Lines->Add("Error: Could not find graph file");
       return;
    }
	fseek(fp, 0, SEEK_END);
	graphFileLen = ftell(fp);
	rewind(fp);
    graphFileBuf = malloc(graphFileLen);
	if(!graphFileBuf) {
		fclose(fp);
        Memo1->Lines->Add("Error: Could not allocate memory");
		return;
	}
    unsigned int n = fread(graphFileBuf, 1, graphFileLen, fp);
	if(n != graphFileLen) {
		fclose(fp);
		free(graphFileBuf);
        Memo1->Lines->Add("Error: Could not read graph file");
		return;
	}
	fclose(fp);
    Memo1->Lines->Add("Graph file loaded.");
    StatusBar1->Panels->Items[1]->Text = "graph file loaded";
}
//---------------------------------------------------------------------------
// Load new prototxt file
//---------------------------------------------------------------------------
// GoogleNet image dimensions, network mean values for each channel in BGR order.
//---------------------------------------------------------------------------
const int networkDim = 227;
float networkMean[] = {0.40787054*255.0, 0.45752458*255.0, 0.48109378*255.0};
//---------------------------------------------------------------------------
void __fastcall TForm1::ResizeImage(void)
{
    Graphics::TBitmap *FormImage = GetFormImage();
    int xo = LoadImage1->Left + AdvOfficePage3->Left +  PageControl1->Left;
    int yo = LoadImage1->Top  + AdvOfficePage3->Top  +  PageControl1->Top ;
    int reqsize = networkDim;
    for(int y = 0; y < reqsize; y++) {
        for(int x = 0; x < reqsize ; x++) {
            Image1->Picture->Bitmap->Canvas->Pixels[x][y] = FormImage->Canvas->Pixels[x+xo][y+yo];
        }
    }
}
//---------------------------------------------------------------------------
void __fastcall TForm1::LoadImageButton1Click(TObject *Sender)
{
    PageControl1->ActivePageIndex = 2;
    if(OpenPictureDialog1->Execute()) {
        LoadImage1->Picture->Bitmap->LoadFromFile(OpenPictureDialog1->FileName);
        ResizeImage();
    }
}
//---------------------------------------------------------------------------
void __fastcall TForm1::pasteButton1Click(TObject *Sender)
{
    LoadImage1->Picture->Assign(Clipboard());
    ResizeImage();
}
//---------------------------------------------------------------------------
void __fastcall TForm1::LoadImage(void)
{
    int reqsize      = networkDim;
    float *mean      = networkMean;

	float *imgfp32 = (float*) malloc(sizeof(*imgfp32) * reqsize * reqsize * 3);
	if(!imgfp32) {
        Memo1->Lines->Add("Error: Could not allocate memory");
		return;
	}
    int i = 0;
	for(int y = 0; y < reqsize; y++) {
	    for(int x = 0; x < reqsize ; x++) {
    		imgfp32[i++] = GetRValue(Image1->Picture->Bitmap->Canvas->Pixels[x][y]);
	    	imgfp32[i++] = GetGValue(Image1->Picture->Bitmap->Canvas->Pixels[x][y]);
		    imgfp32[i++] = GetBValue(Image1->Picture->Bitmap->Canvas->Pixels[x][y]);
        }
    }
	half  *imgfp16 = (half*) malloc(sizeof(*imgfp16) * reqsize * reqsize * 3);
	if(!imgfp16) {
		free(imgfp32);
        Memo1->Lines->Add("Error: Could not allocate memory");
		return;
	}
	for(i = 0; i < reqsize*reqsize; i++) {
		float blue, green, red;
        blue  = imgfp32[3*i+2];
        green = imgfp32[3*i+1];
        red   = imgfp32[3*i+0];

        imgfp32[3*i+0] = blue - mean[0];
        imgfp32[3*i+1] = green- mean[1];
        imgfp32[3*i+2] = red  - mean[2];

        // uncomment to see what values are getting passed to mvncLoadTensor() before conversion to half float
        //printf("Blue: %f, Grean: %f,  Red: %f \n", imgfp32[3*i+0], imgfp32[3*i+1], imgfp32[3*i+2]);
	}
	floattofp16((unsigned char *)imgfp16, imgfp32, 3*reqsize*reqsize);
	free(imgfp32);
	imageBufFp16 = imgfp16;
}
//---------------------------------------------------------------------------
void __fastcall TForm1::RunButton1Click(TObject *Sender)
{
    void* graphHandle;

    StringGrid1->SortSettings->Direction = sdAscending;
    StringGrid1->SortByColumn(0);

    mvncStatus retCode = mvncAllocateGraph(deviceHandle, &graphHandle, graphFileBuf, graphFileLen);     // allocate the graph
    if(retCode != MVNC_OK)   {   // error allocating graph
        Memo1->Lines->Add("Could not allocate graph for file: " + AnsiString(GRAPH_FILE_NAME));
    	Memo1->Lines->Add("Error from mvncAllocateGraph is: " + AnsiString(retCode));
    }
    else {
        // successfully allocated graph.  Now graphHandle is ready to go.
        // use graphHandle for other API calls and call mvncDeallocateGraph when done with it.
        Memo1->Lines->Add("Successfully allocated graph for: " + AnsiString(GRAPH_FILE_NAME));

        // LoadImage will convert channels to floats, subtract network mean for each value in each channel.  Then, convert
        // floats to half precision floats and return pointer to the buffer half *imageBufFp16 of half precision floats (Fp16s)
        LoadImage();

        // calculate the length of the buffer that contains the half precision floats.
        // 3 channels * width * height * sizeof a 16bit float
        unsigned int lenBufFp16 = 3*networkDim*networkDim*sizeof(*imageBufFp16);

        // start the inference with mvncLoadTensor
        retCode = mvncLoadTensor(graphHandle, imageBufFp16, lenBufFp16, NULL);
        if(retCode != MVNC_OK) {   // error loading tensor
            Memo1->Lines->Add("Could not load tensor");
        	Memo1->Lines->Add("Error from mvncLoadTensor is: " + AnsiString(retCode));
        }
        else {
            // the inference has been started, now call mvncGetResult() for the
            // inference result 
            Memo1->Lines->Add("Successfully loaded the tensor for image");
            
            AnsiString tmp;
            void* resultData16;
            void* userParam;
            unsigned int lenResultData;
            retCode = mvncGetResult(graphHandle, &resultData16, &lenResultData, &userParam);
            if(retCode == MVNC_OK) {   // Successfully got the result.  The inference result is in the buffer pointed to by resultData
                Memo1->Lines->Add("Successfully got the inference result for image");
                tmp = tmp.sprintf("resultData is %d bytes which is %d 16-bit floats.\n", lenResultData, lenResultData/(int)sizeof(half));
                Memo1->Lines->Add(tmp);

                // convert half precision floats to full floats
                int numResults = lenResultData / sizeof(half);
                float* resultData32;
	            resultData32 = (float*)malloc(numResults * sizeof(*resultData32));
                fp16tofloat(resultData32, (unsigned char*)resultData16, numResults);

                float maxResult = 0.0;
                int maxIndex = -1;
                for(int index = 0; index < numResults; index++) {
                    StringGrid1->Cells[1][index+1] = resultData32[index];
                    if(resultData32[index] > maxResult) {
                        maxResult = resultData32[index];
                        maxIndex = index;
                    }
                }
                AnsiString maxstring;
                for(int c = 0; c < 10; c++) {
                    maxstring = maxstring + " " + StringGrid1->Cells[c][maxIndex+1];
                }

                tmp = tmp.sprintf("Index of top result is: %d, classification = %s", maxIndex, maxstring.c_str());
                Memo1->Lines->Add(tmp);
                StaticText1->Caption = "Result: " + tmp;
                tmp = tmp.sprintf("Probability of top result is: %f", resultData32[maxIndex]);
                Memo1->Lines->Add(tmp);
                StaticText2->Caption = "Probability: " + tmp;
            }
        }
        retCode = mvncDeallocateGraph(graphHandle);
    	graphHandle = NULL;
    }
}
//---------------------------------------------------------------------------




