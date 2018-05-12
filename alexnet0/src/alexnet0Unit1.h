//---------------------------------------------------------------------------
#ifndef alexnet0Unit1H
#define alexnet0Unit1H
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ExtCtrls.hpp>
#include <ComCtrls.hpp>
#include <Graphics.hpp>
#include <Dialogs.hpp>
#include <ExtDlgs.hpp>
#include "AdvGlowButton.hpp"
#include "AdvOfficePager.hpp"
#include "AdvOfficePagerStylers.hpp"
#include "AdvOfficeStatusBar.hpp"
#include "AdvOfficeStatusBarStylers.hpp"
#include "AdvPicture.hpp"
#include "AdvMemo.hpp"
#include "AdvGrid.hpp"
#include "BaseGrid.hpp"
#include <Grids.hpp>
#include "AdvmWS.hpp"
//---------------------------------------------------------------------------
#define NAME_SIZE 100                              // arbitrary buffer size for the device name
#define GRAPH_FILE_NAME "graph"                    // graph file name

typedef unsigned short half; // 16 bits.  will use this to store half precision floats

char *status_strings[] = {
	"myriad not initialized",   // 0,
	"myriad initialized",       // 1,
	"myriad waiting",           // 2,
	"myriad running",           // 3,
	"myriad finished",          // 4,
	"myriad pending",           // 5,
};

//---------------------------------------------------------------------------
class TForm1 : public TForm
{
__published:	// IDE-managed Components
    TPanel *Panel1;
    TOpenPictureDialog *OpenPictureDialog1;
    TAdvGlowButton *OpenButton1;
    TAdvGlowButton *statusButton1;
    TAdvGlowButton *ExitButton1;
    TAdvGlowButton *LoadGraphButton1;
    TAdvGlowButton *RunButton1;
    TAdvOfficeStatusBar *StatusBar1;
    TAdvOfficeStatusBarOfficeStyler *AdvOfficeStatusBarOfficeStyler1;
    TAdvOfficePager *PageControl1;
    TAdvOfficePagerOfficeStyler *AdvOfficePagerOfficeStyler1;
    TAdvOfficePage *AdvOfficePage1;
    TAdvOfficePage *AdvOfficePage2;
    TAdvOfficePage *AdvOfficePage3;
    TMemo *Memo1;
    TMemo *Memo2;
    TAdvGlowButton *LoadImageButton1;
    TAdvOfficePage *AdvOfficePage4;
    TImage *Image1;
    TImage *LoadImage1;
    TAdvStringGrid *StringGrid1;
    TAdvGlowButton *pasteButton1;
    TLabel *Label1;
    TLabel *Label2;
    TStaticText *StaticText1;
    TStaticText *StaticText2;
    TAdvGlowButton *runButton2;
    TOpenDialog *OpenDialog1;
    void __fastcall OpenButton1Click(TObject *Sender);
    void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
    void __fastcall FormShow(TObject *Sender);
    void __fastcall ExitButton1Click(TObject *Sender);
    void __fastcall LoadGraphButton1Click(TObject *Sender);
    void __fastcall LoadImageButton1Click(TObject *Sender);
    void __fastcall RunButton1Click(TObject *Sender);
    void __fastcall statusButton1Click(TObject *Sender);
    void __fastcall TabSheet2Enter(TObject *Sender);
    void __fastcall pasteButton1Click(TObject *Sender);
    void __fastcall AdvOfficePage2Enter(TObject *Sender);

private:	// User declarations

    AnsiString EXEPath;
    void *deviceHandle;

public:		// User declarations

    void *graphFileBuf;
    unsigned int graphFileLen;

    void __fastcall ResizeImage(void);
    void __fastcall LoadImage(void);
    half *imageBufFp16; // image buffer

    __fastcall TForm1(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TForm1 *Form1;
//---------------------------------------------------------------------------
#endif
