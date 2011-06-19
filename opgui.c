/*
 * op.c - control program for the open programmer
 * Copyright (C) 2009-2010 Alberto Maccioni
 * for detailed info see:
 * http://openprog.altervista.org/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 * or see <http://www.gnu.org/licenses/>
 */


#if !defined _WIN32 && !defined __CYGWIN__
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <asm/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/hiddev.h>
#include <linux/input.h>
#else
#include <windows.h>
#include <setupapi.h>
#include <ddk/hidusage.h>
#include <ddk/hidpi.h>
#endif

#include <gtk/gtk.h>
#include <sys/timeb.h>
#include <wchar.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <getopt.h>
#include <string.h>
#include "strings.h"
#include "instructions.h"

#define MAXLINES 500
#define COL 16
#define VERSION "0.7.9"
#define G (12.0/34*1024/5)		//=72,2823529412
#define  LOCK	1
#define  FUSE	2
#define  FUSE_H 4
#define  FUSE_X	8
#define  CAL	16
#define  SLOW	256
#define  CONFIG_FILE "opgui.ini"
#define  CONFIG_DIR ".opgui"

#if !defined _WIN32 && !defined __CYGWIN__
    #define write() ioctl(fd, HIDIOCSUSAGES, &ref_multi_u); ioctl(fd,HIDIOCSREPORT, &rep_info_u);
    #define read() ioctl(fd, HIDIOCGUSAGES, &ref_multi_i); ioctl(fd,HIDIOCGREPORT, &rep_info_i);
    #define bufferU ref_multi_u.values
    #define bufferI ref_multi_i.values

#else
	#define write()	Result=WriteFile(WriteHandle,bufferU,DIMBUF,&BytesWritten,NULL);
	#define read()	Result = ReadFile(ReadHandle,bufferI,DIMBUF,&NumberOfBytesRead,(LPOVERLAPPED) &HIDOverlapped);\
					Result = WaitForSingleObject(hEventObject,10);\
					ResetEvent(hEventObject);\
					if(Result!=WAIT_OBJECT_0){\
						PrintMessage(strings[S_comTimeout]);	/*"comm timeout\r\n"*/\
					}
#endif

#define PrintMessage1(s,p) {sprintf(str,s,p); PrintMessage(str);}
#define PrintMessage2(s,p1,p2) {sprintf(str,s,p1,p2); PrintMessage(str);}
#define PrintMessage3(s,p1,p2,p3) {sprintf(str,s,p1,p2,p3); PrintMessage(str);}

typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;
#define CloseLogFile() if(logfile)fclose(logfile);

#if !defined _WIN32 && !defined __CYGWIN__
DWORD GetTickCount();
#endif
void msDelay(double delay);
void Save(char* dev,char* savefile);
int Load(char* dev,char* loadfile);
void SaveEE(char* dev,char* savefile);
void LoadEE(char* dev,char* loadfile);
unsigned int htoi(const char *hex, int length);
void Write(char* dev,int ee);
void Read(char* dev,int ee,int r);
void AddDevices();
void TestHw();
int StartHVReg(double V);
void ProgID();
void DisplayEE();
int FindDevice();
void OpenLogFile();
void MsgBox(const char* msg);
void Connect();
void I2cspiR();
void I2cspiS();

char** strings;
int saveLog=0,programID=0,MinDly=1,load_osccal=0,load_BKosccal=0;
int use_osccal=1,use_BKosccal=0;
int load_calibword=0,max_err=200;
int AVRlock=0x100,AVRfuse=0x100,AVRfuse_h=0x100,AVRfuse_x=0x100;
int ICDenable=0,ICDaddr=0x1FF0;
int FWVersion=0,HwID=0;
FILE* logfile=0;
char LogFileName[512]="";
char loadfile[512]="",savefile[512]="";
char loadfileEE[512]="",savefileEE[512]="";
int vid=0x04D8,pid=0x0100;
WORD *dati_hex=0;
int size=0,sizeW=0,sizeEE=0,sizeCONFIG=0;
unsigned char *memCODE=0,*memEE=0,memID[8],memCONFIG[48];
double hvreg=0;
int DeviceDetected=0;

GtkTextBuffer *dataBuf=0;
GtkWidget * data;
GtkWidget * data_scroll;
GtkWidget * window;
GtkWidget * toolbar;
GtkWidget * button;
GtkWidget * b_open;
GtkWidget * b_save;
GtkWidget * b_read;
GtkWidget * b_write;
GtkWidget * notebook;
GtkWidget * label;
GtkWidget * status;
GtkWidget * img;
GtkWidget * devCombo;
GtkWidget * ICD_check;
GtkWidget * ICD_addr_entry;
GtkWidget * EEPROM_RW;
GtkWidget * ReadReserved;
GtkWidget * Write_ID_BKCal;
GtkWidget * WriteCalib12;
GtkWidget * UseOSCCAL;
GtkWidget * UseBKOSCCAL;
GtkWidget * UseFileCal;
GtkWidget * AVR_FuseLow;
GtkWidget * AVR_FuseLowWrite;
GtkWidget * AVR_FuseHigh;
GtkWidget * AVR_FuseHighWrite;
GtkWidget * AVR_FuseExt;
GtkWidget * AVR_FuseExtWrite;
GtkWidget * AVR_Lock;
GtkWidget * AVR_LockWrite;
GtkWidget * b_connect;
GtkWidget * b_testhw;
GtkWidget * b_log;
GtkWidget * VID_entry;
GtkWidget * PID_entry;
GtkWidget * Errors_entry;
GtkWidget * I2C8bit;
GtkWidget * I2C16bit;
GtkWidget * SPI00;
GtkWidget * SPI01;
GtkWidget * SPI10;
GtkWidget * SPI11;
GtkWidget * I2CDataSend;
GtkWidget * I2CDataReceive;
GtkWidget * I2CSendBtn;
GtkWidget * I2CReceiveBtn;
GtkWidget * I2CNbyte;

int statusID;
int ee = 0;
int readRes=0;
char dev[64]="";
char str[4096];
char* cur_path=0;
char* cur_pathEE=0;

#if !defined _WIN32 && !defined __CYGWIN__
int fd = -1;
struct hiddev_report_info rep_info_i,rep_info_u;
struct hiddev_usage_ref_multi ref_multi_i,ref_multi_u;
int DIMBUF=64;
char path[256]="";
#else
unsigned char bufferU[128],bufferI[128];
DWORD NumberOfBytesRead,BytesWritten;
ULONG Result;
HANDLE WriteHandle,ReadHandle;
OVERLAPPED HIDOverlapped;
HANDLE hEventObject;
int DIMBUF=65;
#endif



gint delete_event( GtkWidget *widget,GdkEvent *event,gpointer data )
{
gtk_main_quit ();
return FALSE;
}

void PrintMessage(const char *msg){
	GtkTextIter iter,iter2;
	gtk_text_buffer_get_end_iter(dataBuf,&iter);
	gtk_text_buffer_insert(dataBuf,&iter,msg,-1);
	gtk_text_buffer_get_start_iter(dataBuf,&iter2);
	gtk_text_buffer_get_end_iter(dataBuf,&iter);
	int l=gtk_text_buffer_get_line_count(dataBuf);
	if(l>MAXLINES+10){ 	//MAXLINES
		gtk_text_iter_set_line(&iter,l-MAXLINES);
		gtk_text_buffer_delete(dataBuf,&iter2,&iter);
	}
	while (gtk_events_pending ()) gtk_main_iteration();
	gtk_text_view_scroll_to_iter(data,&iter,0.0,FALSE,0,0);
}

void PrintMessageI2C(const char *msg){
	GtkTextIter iter,iter2;
	GtkWidget * dataBuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(I2CDataReceive));
	gtk_text_buffer_set_text(dataBuf,msg,-1);
	gtk_text_buffer_get_end_iter(dataBuf,&iter);
	gtk_text_view_scroll_to_iter(I2CDataReceive,&iter,0.0,FALSE,0,0);
	while (gtk_events_pending ()) gtk_main_iteration();
}

void getOptions()
{
	vid=htoi(gtk_entry_get_text(VID_entry),4);
	pid=htoi(gtk_entry_get_text(PID_entry),4);
	saveLog = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_log));
	ee = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(EEPROM_RW))?0xFFFF:0;
	programID = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Write_ID_BKCal));
	max_err=atoi(gtk_entry_get_text(GTK_ENTRY(Errors_entry)));
	load_calibword= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(WriteCalib12));
	load_osccal= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(UseOSCCAL));
	load_BKosccal= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(UseBKOSCCAL));
	ICDenable= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ICD_check));
	readRes= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ReadReserved));
	int i=sscanf(gtk_entry_get_text(GTK_ENTRY(ICD_addr_entry)),"%x",&ICDaddr);
	if(i!=1||ICDaddr<0||ICDaddr>0xFFFF) ICDaddr=0x1FF0;
	char *str=gtk_combo_box_get_active_text(GTK_COMBO_BOX(devCombo));
	if(str) strncpy(dev,str,sizeof(dev)-1);
	g_free(str);
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(AVR_FuseLowWrite))){
		sscanf(gtk_entry_get_text(GTK_ENTRY(AVR_FuseLow)),"%x",&AVRfuse);
		if(i!=1||AVRfuse<0||AVRfuse>0xFF) AVRfuse=0x100;
	}
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(AVR_FuseHighWrite))){
		sscanf(gtk_entry_get_text(GTK_ENTRY(AVR_FuseHigh)),"%x",&AVRfuse_h);
		if(i!=1||AVRfuse_h<0||AVRfuse_h>0xFF) AVRfuse_h=0x100;
	}
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(AVR_FuseExtWrite))){
		sscanf(gtk_entry_get_text(GTK_ENTRY(AVR_FuseExt)),"%x",&AVRfuse_x);
		if(i!=1||AVRfuse_x<0||AVRfuse_x>0xFF) AVRfuse_x=0x100;
	}
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(AVR_LockWrite))){
		sscanf(gtk_entry_get_text(GTK_ENTRY(AVR_Lock)),"%x",&AVRlock);
		if(i!=1||AVRlock<0||AVRlock>0xFF) AVRlock=0x100;
	}
}

void Fopen(GtkWidget *widget,GtkWidget *window)
{
	char *str=gtk_combo_box_get_active_text(GTK_COMBO_BOX(devCombo));
	if(str) strncpy(dev,str,sizeof(dev)-1);
	g_free(str);
	GtkWidget *dialog;
	dialog = gtk_file_chooser_dialog_new ("Open File",
				      GTK_WINDOW(window),
				      GTK_FILE_CHOOSER_ACTION_OPEN,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      NULL);
	if(cur_path) gtk_file_chooser_set_current_folder(dialog,cur_path);
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT){
	    char *filename;
	    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		if(cur_path) free(cur_path);
		cur_path = gtk_file_chooser_get_current_folder(dialog);
	    Load(dev,filename);
	    g_free (filename);
		if(!strncmp(dev,"AT",2)){	//load EEPROM from separate file for ATMEL chips
			GtkWidget *dialog2;
			dialog2 = gtk_file_chooser_dialog_new (strings[S_openEEfile],
						      GTK_WINDOW(window),
						      GTK_FILE_CHOOSER_ACTION_OPEN,
						      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
						      NULL);
			if(!cur_pathEE) cur_pathEE = gtk_file_chooser_get_current_folder(dialog);
			if(cur_pathEE) gtk_file_chooser_set_current_folder(dialog2,cur_pathEE);
			if (gtk_dialog_run (GTK_DIALOG (dialog2)) == GTK_RESPONSE_ACCEPT){
			    char *filename2;
			    filename2 = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog2));
				if(cur_pathEE) free(cur_pathEE);
				cur_pathEE = gtk_file_chooser_get_current_folder(dialog2);
			//CFileDialog dlgA(TRUE,"hex",NULL,OFN_HIDEREADONLY,strings[S_fileEEP]);	//"File Hex8 (*.hex;.eep ..."
				LoadEE(dev,filename2);
				g_free (filename2);
			}
			gtk_widget_destroy (dialog2);
		}
	}
	gtk_widget_destroy (dialog);
}

void Fsave(GtkWidget *widget,GtkWidget *window)
{
	GtkWidget *dialog;
	dialog = gtk_file_chooser_dialog_new ("Save File",
				      GTK_WINDOW(window),
				      GTK_FILE_CHOOSER_ACTION_SAVE,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
				      NULL);
	if(cur_path) gtk_file_chooser_set_current_folder(dialog,cur_path);
	gtk_file_chooser_set_do_overwrite_confirmation(dialog,TRUE);
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
	  {
	    char *filename;
	    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		if(cur_path) free(cur_path);
		cur_path = gtk_file_chooser_get_current_folder(dialog);
   		Save(dev,filename);
		PrintMessage1(strings[S_FileSaved],filename);
	    g_free (filename);
		if(!strncmp(dev,"AT",2)&&sizeEE){	//save EEPROM on separate file for ATMEL chips
			GtkWidget *dialog2;
			dialog2 = gtk_file_chooser_dialog_new (strings[S_saveEEfile],
						      GTK_WINDOW(window),
						      GTK_FILE_CHOOSER_ACTION_SAVE,
						      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						      GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
						      NULL);
			if(!cur_pathEE) cur_pathEE = gtk_file_chooser_get_current_folder(dialog);
			if(cur_pathEE) gtk_file_chooser_set_current_folder(dialog2,cur_pathEE);
			gtk_file_chooser_set_do_overwrite_confirmation(dialog2,TRUE);
			if (gtk_dialog_run (GTK_DIALOG (dialog2)) == GTK_RESPONSE_ACCEPT){
			    char *filename2;
			    filename2 = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog2));
				if(cur_pathEE) free(cur_pathEE);
				cur_pathEE = gtk_file_chooser_get_current_folder(dialog2);
				SaveEE(dev,filename2);
				PrintMessage1(strings[S_FileSaved],filename2);
				g_free (filename2);
			}
			gtk_widget_destroy (dialog2);
		}
	  }
	gtk_widget_destroy (dialog);
}

void DevWrite(GtkWidget *widget,GtkWidget *window)
{
	if(DeviceDetected!=1) return;
	gtk_statusbar_push(status,statusID,"");
	getOptions();
	Write(dev,ee);	//choose the right function
}

void DevRead(GtkWidget *widget,GtkWidget *window)
{
	if(DeviceDetected!=1) return;
	gtk_statusbar_push(status,statusID,"");
	getOptions();
	Read(dev,ee,readRes);	//choose the right function
}

void info(GtkWidget *widget,GtkWidget *window)
{
  const gchar *license =
    "This program is free software; you can redistribute it and/or\n"
    "modify it under the terms of the GNU Library General Public License as\n"
    "published by the Free Software Foundation; either version 2 of the\n"
    "License, or (at your option) any later version.\n"
    "\n"
    "This program is distributed in the hope that it will be useful,\n"
    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n"
    "Library General Public License for more details.\n"
    "\n"
    "You should have received a copy of the GNU Library General Public\n"
    "License along with the Gnome Library; see the file COPYING.LIB.  If not,\n"
    "write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,\n"
    "Boston, MA 02111-1307, USA.\n";

	gtk_show_about_dialog (NULL,
		//"artists"                  GStrv*                : Read / Write
		//"authors"                  GStrv*                : Read / Write
		"comments", "A graphical interface for the Open Programmer",
		"copyright",
"Copyright (C) Alberto Maccioni 2009-2011\n\n\
This program is free software; you can \n\
redistribute it and/or modify it under \n\
the terms of the GNU General Public License \n\
as published by the Free Software Foundation;\n\
either version 2 of the License, or \n\
(at your option) any later version.",
		//"documenters"              GStrv*                : Read / Write
		"license",license,
		//  "logo"                     GdkPixbuf*            : Read / Write
		//  "logo-icon-name"           gchar*                : Read / Write
		"program-name", "OPGUI",
		//  "translator-credits"       gchar*                : Read / Write
		"version","0.7.9",
		"website","www.openprog.altervista.org",
		//  "website-label"            gchar*                : Read / Write
  		"wrap-license",TRUE,
		"title","Info about OPGUI");

}

void Xclose(){
	getOptions();
	gtk_main_quit();
}

int main( int argc, char *argv[])
{
	#include "strings.c"
	#if defined _WIN32
	int langID=GetUserDefaultLangID();
	if((langID&0xFF)==0x10)strings=strings_it;
	#else
	if(getenv("LANG")&&strstr(getenv("LANG"),"it")!=0) strings=strings_it;
	#endif
	else strings=strings_en;
	FILE *f;
	gchar *homedir,*config_dir,*fname;
	homedir = (gchar *) g_get_home_dir ();
	if(homedir){
		config_dir=g_build_path(G_DIR_SEPARATOR_S,homedir,CONFIG_DIR, NULL);
		if(!g_file_test(config_dir,G_FILE_TEST_IS_DIR)) mkdir(config_dir, 0755);
		fname = g_build_path(G_DIR_SEPARATOR_S,config_dir,CONFIG_FILE, NULL);
		f=fopen(fname,"r");
		if(f){
			char temp[256],line[256];
			int X;
			for(;fgets(line,256,f);){
				if(sscanf(line,"device %s",temp)>0) strcpy(dev,temp);
				else if(sscanf(line,"vid %X",&X)>0) vid=X;
				else if(sscanf(line,"pid %X",&X)>0) pid=X;
				else sscanf(line,"maxerr %d",&max_err);
			}
			fclose(f);
		}
	}
	char dev_ini[64];
	strncpy(dev_ini,dev,sizeof(dev_ini));
	int vid_ini=vid,pid_ini=pid,max_err_ini=max_err;

	gtk_init(&argc, &argv);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	sprintf(str,"opgui v%s",VERSION);
	gtk_window_set_title(GTK_WINDOW(window),str);
	gtk_window_set_default_size(GTK_WINDOW(window), 750, 250);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	g_signal_connect(window, "destroy",G_CALLBACK(Xclose),NULL);

	GtkWidget * vbox = gtk_vbox_new(FALSE,0);
	gtk_container_add(GTK_CONTAINER(window),vbox);
	PangoFontDescription    *font_desc;

//------toolbar-------------
	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar),GTK_TOOLBAR_ICONS);
	gtk_box_pack_start(GTK_BOX(vbox),toolbar,FALSE,FALSE,0);
	gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar),GTK_STOCK_OPEN,strings[I_Fopen],"",G_CALLBACK(Fopen),0,-1);//"Open File"
	gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar),GTK_STOCK_SAVE,strings[I_Fsave],"",G_CALLBACK(Fsave),0,-1);//"Save File"
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
	gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar),GTK_STOCK_GOTO_TOP,strings[I_DevR],"",G_CALLBACK(DevRead),0,-1);//"Read device"
	gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar),GTK_STOCK_GOTO_BOTTOM,strings[I_DevW],"",G_CALLBACK(DevWrite),0,-1);//"Write device"
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
	gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar),GTK_STOCK_INFO,strings[I_Info],"",G_CALLBACK(info),0,-1);//"Info"

//------tab widget-------------
	notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(vbox),notebook,TRUE,TRUE,0);

//------data tab-------------
	label = gtk_label_new(strings[I_Data]);	//"Data"
	data_scroll = gtk_scrolled_window_new(NULL,NULL);
	data = gtk_text_view_new();
	dataBuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data));
	gtk_text_view_set_editable(GTK_TEXT_VIEW(data),FALSE);
	gtk_container_add(data_scroll,data);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),data_scroll,label);
	font_desc = pango_font_description_from_string ("monospace 8");
	gtk_widget_modify_font (data, font_desc);
	pango_font_description_free (font_desc);

//------device tab-------------
	label = gtk_label_new(strings[I_Dev]);	//"Device"
	GtkWidget * table_dev = gtk_table_new(3,3,FALSE);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),table_dev,label);

	GtkWidget * devHbox1 = gtk_hbox_new(FALSE,10);
	gtk_table_attach(GTK_TABLE(table_dev),devHbox1,0,1,0,1,GTK_FILL,0,5,5);
	label = gtk_label_new(strings[I_Dev]);	//"Device"
	gtk_box_pack_start(GTK_BOX(devHbox1),label,FALSE,TRUE,0);
	devCombo = gtk_combo_box_new_text();
	gtk_box_pack_start(GTK_BOX(devHbox1),devCombo,FALSE,TRUE,0);

	EEPROM_RW = gtk_check_button_new_with_label(strings[I_EE]);	//"Read and write EEPROM"
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(EEPROM_RW),TRUE);
	gtk_table_attach(GTK_TABLE(table_dev),EEPROM_RW,0,1,1,2,GTK_FILL,0,5,5);

	ReadReserved = gtk_check_button_new_with_label(strings[I_ReadRes]);	//"Read reserved area"
	gtk_table_attach(GTK_TABLE(table_dev),ReadReserved,0,1,2,3,GTK_FILL,0,5,0);

	Write_ID_BKCal = gtk_check_button_new_with_label(strings[I_ID_BKo_W]);	//"Write ID and BKOscCal"
	gtk_table_attach(GTK_TABLE(table_dev),Write_ID_BKCal,0,1,3,4,GTK_FILL,0,5,0);

	WriteCalib12 = gtk_check_button_new_with_label(strings[I_CalW]);	//"Write Calib 1 and 2"
	gtk_table_attach(GTK_TABLE(table_dev),WriteCalib12,0,1,4,5,GTK_FILL,0,5,0);

	GtkWidget * devFrameOsc = gtk_frame_new(strings[I_OSCW]);	//"Write OscCal"
	gtk_table_attach(GTK_TABLE(table_dev),devFrameOsc,0,1,5,6,GTK_FILL,0,5,0);
	GtkWidget * devVboxOsc = gtk_vbox_new(FALSE,10);
	gtk_container_add(GTK_CONTAINER(devFrameOsc),devVboxOsc);
	UseOSCCAL = gtk_radio_button_new_with_label(NULL,strings[I_OSC]);	//"OSCCal"
	UseBKOSCCAL = gtk_radio_button_new_with_label(\
		gtk_radio_button_get_group(GTK_RADIO_BUTTON(UseOSCCAL)),strings[I_BKOSC]);	//"Backup OSCCal"
	UseFileCal = gtk_radio_button_new_with_label(\
		gtk_radio_button_get_group(GTK_RADIO_BUTTON(UseOSCCAL)),strings[I_OSCF]);	//"From file"
	gtk_container_add(GTK_CONTAINER(devVboxOsc),UseOSCCAL);
	gtk_container_add(GTK_CONTAINER(devVboxOsc),UseBKOSCCAL);
	gtk_container_add(GTK_CONTAINER(devVboxOsc),UseFileCal);

	GtkWidget * devHboxICD = gtk_hbox_new(FALSE,5);
	gtk_table_attach(GTK_TABLE(table_dev),devHboxICD,0,1,6,7,GTK_FILL,0,5,5);
	ICD_check = gtk_check_button_new_with_label(strings[I_ICD_ENABLE]);	//"Enable ICD"
	gtk_container_add(GTK_CONTAINER(devHboxICD),ICD_check);
	ICD_addr_entry = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(ICD_addr_entry),4);
	gtk_container_add(GTK_CONTAINER(devHboxICD),ICD_addr_entry);
	label = gtk_label_new(strings[I_ICD_ADDRESS]);	//"ICD routine address"
	gtk_container_add(GTK_CONTAINER(devHboxICD),label);

	GtkWidget * devFrameAVR = gtk_frame_new(strings[I_AT_CONFIG]);	//"Atmel configuration"
	gtk_table_attach(GTK_TABLE(table_dev),devFrameAVR,1,2,0,4,0,0,5,0);
	GtkWidget * devTableAVR = gtk_table_new(3,2,FALSE);
	gtk_container_add(GTK_CONTAINER(devFrameAVR),devTableAVR);
	AVR_FuseLow = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(AVR_FuseLow),4);
	gtk_table_attach(GTK_TABLE(devTableAVR),AVR_FuseLow,0,1,0,1,0,0,5,0);
	AVR_FuseLowWrite = gtk_check_button_new_with_label(strings[I_AT_FUSE]);	//"Write Fuse Low"
	gtk_table_attach(GTK_TABLE(devTableAVR),AVR_FuseLowWrite,1,2,0,1,GTK_FILL,0,5,0);
	AVR_FuseHigh = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(AVR_FuseHigh),4);
	gtk_table_attach(GTK_TABLE(devTableAVR),AVR_FuseHigh,0,1,1,2,0,0,5,0);
	AVR_FuseHighWrite = gtk_check_button_new_with_label(strings[I_AT_FUSEH]);	//"Write Fuse High"
	gtk_table_attach(GTK_TABLE(devTableAVR),AVR_FuseHighWrite,1,2,1,2,GTK_FILL,0,5,0);
	AVR_FuseExt = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(AVR_FuseExt),4);
	gtk_table_attach(GTK_TABLE(devTableAVR),AVR_FuseExt,0,1,2,3,0,0,5,0);
	AVR_FuseExtWrite = gtk_check_button_new_with_label(strings[I_AT_FUSEX]);	//"Write Extended Fuse"
	gtk_table_attach(GTK_TABLE(devTableAVR),AVR_FuseExtWrite,1,2,2,3,GTK_FILL,0,5,0);
	AVR_Lock = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(AVR_Lock),4);
	gtk_table_attach(GTK_TABLE(devTableAVR),AVR_Lock,0,1,3,4,0,0,5,0);
	AVR_LockWrite = gtk_check_button_new_with_label(strings[I_AT_LOCK]);	//"Write Lock"
	gtk_table_attach(GTK_TABLE(devTableAVR),AVR_LockWrite,1,2,3,4,GTK_FILL,0,5,0);


//------options tab-------------
	label = gtk_label_new(strings[I_Opt]);	//"Options"
	GtkWidget * optTable = gtk_table_new(6,2,FALSE);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),optTable,label);

	b_connect = gtk_button_new_with_label(strings[I_CONN]);	//"Reconnect"
	gtk_table_attach(GTK_TABLE(optTable),b_connect,0,1,0,1,GTK_FILL,0,5,5);

	b_testhw = gtk_button_new_with_label(strings[I_TestHWB]);	//"Hardware test"
	gtk_table_attach(GTK_TABLE(optTable),b_testhw,0,1,1,2,GTK_FILL,0,5,5);

	GtkWidget * optHboxVid = gtk_hbox_new(FALSE,5);
	gtk_table_attach(GTK_TABLE(optTable),optHboxVid,0,1,2,3,GTK_FILL,0,5,5);
	label = gtk_label_new("Vid");
	gtk_box_pack_start(GTK_BOX(optHboxVid),label,FALSE,TRUE,0);
	VID_entry = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(VID_entry),4);
	gtk_box_pack_start(GTK_BOX(optHboxVid),VID_entry,FALSE,TRUE,0);

	GtkWidget * optHboxPid = gtk_hbox_new(FALSE,5);
	gtk_table_attach(GTK_TABLE(optTable),optHboxPid,0,1,4,5,GTK_FILL,0,5,5);
	label = gtk_label_new("Pid");
	gtk_box_pack_start(GTK_BOX(optHboxPid),label,FALSE,TRUE,0);
	PID_entry = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(PID_entry),4);
	gtk_box_pack_start(GTK_BOX(optHboxPid),PID_entry,FALSE,TRUE,0);

	b_log = gtk_check_button_new_with_label(strings[I_LOG]);	//"Log activity"
	gtk_table_attach(GTK_TABLE(optTable),b_log,0,1,6,7,GTK_FILL,0,5,5);

	GtkWidget * optHboxErr = gtk_hbox_new(FALSE,5);
	gtk_table_attach(GTK_TABLE(optTable),optHboxErr,0,1,8,9,GTK_FILL,0,5,5);
	label = gtk_label_new(strings[I_MAXERR]);	//"Max errors"
	gtk_container_add(GTK_CONTAINER(optHboxErr),label);
	Errors_entry = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(Errors_entry),6);
	gtk_container_add(GTK_CONTAINER(optHboxErr),Errors_entry);

//------I2C tab-------------
	label = gtk_label_new("I2C/SPI");
	GtkWidget * i2cTable = gtk_table_new(4,2,FALSE);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),i2cTable,label);

	GtkWidget * i2cModeFrame = gtk_frame_new(strings[I_I2CMode]);	//"Mode"
	gtk_table_attach(GTK_TABLE(i2cTable),i2cModeFrame,0,1,0,4,GTK_FILL,0,5,0);
	GtkWidget * i2cVboxMode = gtk_vbox_new(FALSE,10);
	gtk_container_add(GTK_CONTAINER(i2cModeFrame),i2cVboxMode);
	I2C8bit = gtk_radio_button_new_with_label(NULL,"I2C 8 Bit");
	I2C16bit = gtk_radio_button_new_with_label(\
		gtk_radio_button_get_group(GTK_RADIO_BUTTON(I2C8bit)),"I2C 16 Bit");
	SPI00 = gtk_radio_button_new_with_label(\
		gtk_radio_button_get_group(GTK_RADIO_BUTTON(I2C8bit)),"SPI(00)");
	SPI01 = gtk_radio_button_new_with_label(\
		gtk_radio_button_get_group(GTK_RADIO_BUTTON(I2C8bit)),"SPI(01)");
	SPI10 = gtk_radio_button_new_with_label(\
		gtk_radio_button_get_group(GTK_RADIO_BUTTON(I2C8bit)),"SPI(10)");
	SPI11 = gtk_radio_button_new_with_label(\
		gtk_radio_button_get_group(GTK_RADIO_BUTTON(I2C8bit)),"SPI(11)");
	gtk_container_add(GTK_CONTAINER(i2cVboxMode),I2C8bit);
	gtk_container_add(GTK_CONTAINER(i2cVboxMode),I2C16bit);
	gtk_container_add(GTK_CONTAINER(i2cVboxMode),gtk_hseparator_new());
	gtk_container_add(GTK_CONTAINER(i2cVboxMode),SPI00);
	gtk_container_add(GTK_CONTAINER(i2cVboxMode),SPI01);
	gtk_container_add(GTK_CONTAINER(i2cVboxMode),SPI10);
	gtk_container_add(GTK_CONTAINER(i2cVboxMode),SPI11);

	GtkWidget * i2cVboxTX = gtk_vbox_new(FALSE,2);
	gtk_table_attach(GTK_TABLE(i2cTable),i2cVboxTX,0,2,4,5,GTK_FILL,0,5,0);
	label = gtk_label_new(strings[I_I2CDATAOUT]);	//"Data to send"
	gtk_container_add(GTK_CONTAINER(i2cVboxTX),label);
	I2CDataSend = gtk_entry_new();
	gtk_container_add(GTK_CONTAINER(i2cVboxTX),I2CDataSend);

	GtkWidget * i2cVboxRX = gtk_vbox_new(FALSE,2);
	gtk_table_attach(GTK_TABLE(i2cTable),i2cVboxRX,0,2,5,6,GTK_FILL,0,5,0);
	label = gtk_label_new(strings[I_I2CDATATR]);	//"Data transferred"
	gtk_container_add(GTK_CONTAINER(i2cVboxRX),label);
	I2CDataReceive = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(I2CDataReceive),FALSE);
	gtk_container_add(GTK_CONTAINER(i2cVboxRX),I2CDataReceive);
	gtk_widget_set_size_request(GTK_WIDGET(I2CDataReceive),100,60);

	GtkWidget * i2cHboxNB = gtk_hbox_new(FALSE,5);
	gtk_table_attach(GTK_TABLE(i2cTable),i2cHboxNB,1,2,0,1,GTK_FILL,0,5,0);
	label = gtk_label_new(strings[I_I2C_NB]);	//"Byes to read/write"
	gtk_container_add(GTK_CONTAINER(i2cHboxNB),label);
	I2CNbyte = 	gtk_spin_button_new_with_range(0,64,1);
	gtk_container_add(GTK_CONTAINER(i2cHboxNB),I2CNbyte);

	I2CSendBtn = gtk_button_new_with_label(strings[I_I2CSend]);	//"Send"
	gtk_table_attach(GTK_TABLE(i2cTable),I2CSendBtn,1,2,1,2,GTK_FILL,0,5,0);

	I2CReceiveBtn = gtk_button_new_with_label(strings[I_I2CReceive]);	//"Receive"
	gtk_table_attach(GTK_TABLE(i2cTable),I2CReceiveBtn,1,2,2,3,GTK_FILL,0,5,0);

//------status bar-------------
	status= gtk_statusbar_new();
	gtk_box_pack_start(GTK_BOX(vbox),status,FALSE,TRUE,0);
	statusID=gtk_statusbar_get_context_id(status,"ID");

	g_signal_connect(G_OBJECT(b_testhw),"clicked",G_CALLBACK(TestHw),window);
	g_signal_connect(G_OBJECT(b_connect),"clicked",G_CALLBACK(Connect),window);
	g_signal_connect(G_OBJECT(I2CReceiveBtn),"clicked",G_CALLBACK(I2cspiR),window);
	g_signal_connect(G_OBJECT(I2CSendBtn),"clicked",G_CALLBACK(I2cspiS),window);
	gtk_widget_show_all(window);

//********Init*************
	char text[16];
	sprintf(text,"%04X",vid);
	gtk_entry_set_text(VID_entry,text);
	sprintf(text,"%04X",pid);
	gtk_entry_set_text(PID_entry,text);
	sprintf(text,"%d",max_err);
	gtk_entry_set_text(Errors_entry,text);
	sizeW=0x2400;
	dati_hex=malloc(sizeW*sizeof(WORD));
	int i;
	for(i=0;i<0x2400;i++) dati_hex[i]=0x3fff;
	strncpy(LogFileName,strings[S_LogFile],sizeof(LogFileName));
	AddDevices();	//populate device list
	gtk_combo_box_set_active(devCombo,0);
	gtk_combo_box_set_wrap_width(devCombo,6);
	gchar* str;
	for(i=0;(str=gtk_combo_box_get_active_text(devCombo))&&strcmp(str,dev)&&i<1000;i++){
		gtk_combo_box_set_active(devCombo,i);
	}
	if(i==1000||!str)gtk_combo_box_set_active(devCombo,0);
	g_free(str);
	DeviceDetected=FindDevice();	//connect to USB programmer
	ProgID();		//get firmware version and reset
	gtk_main();
//******Save ini file******
// only if parameters are changed
	if(strcmp(dev_ini,dev)||vid_ini!=vid||pid_ini!=pid||max_err_ini!=max_err){
		if(homedir){
			f=fopen(fname,"w");
			if(f){
				char temp[256],line[256];
				fprintf(f,"device %s\n",dev);
				fprintf(f,"maxerr %d\n",max_err);
				fprintf(f,"vid %X\n",vid);
				fprintf(f,"pid %X\n",pid);
			}
			fclose(f);
		}
	}
	return 0;
}

void MsgBox(const char* msg)
{
	GtkWidget * dialog = gtk_message_dialog_new (GTK_WINDOW(window),
                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                 GTK_MESSAGE_INFO,
                                 GTK_BUTTONS_OK,
                                 msg);
    gtk_window_set_title(dialog," ");
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

void Connect(){
	vid=htoi(gtk_entry_get_text(VID_entry),4);
	pid=htoi(gtk_entry_get_text(PID_entry),4);
	DeviceDetected=FindDevice();
	hvreg=0;
	ProgID();
}

DWORD GetTickCount(){
	struct timeb now;
	ftime(&now);
	return now.time*1000+now.millitm;
}

void DisplayEE(){
	char s[256],t[256],v[256],*aux,*g;
	int valid=0,empty=1;
	int i,j;
	s[0]=0;
	v[0]=0;
	aux=malloc((sizeEE/COL+1)*(16+COL*5));
	aux[0]=0;
	PrintMessage(strings[S_EEMem]);	//"\r\nmemoria EEPROM:\r\n"
	for(i=0;i<sizeEE;i+=COL){
		valid=0;
		for(j=i;j<i+COL&&j<sizeEE;j++){
			sprintf(t,"%02X ",memEE[j]);
			strcat(s,t);
			sprintf(t,"%c",isprint(memEE[j])&&(memEE[j]<0xFF)?memEE[j]:'.');
			g=g_locale_to_utf8(t,-1,NULL,NULL,NULL);
			if(g) strcat(v,g);
			g_free(g);
			if(memEE[j]<0xff) valid=1;
		}
		if(valid){
			sprintf(t,"%04X: %s %s\r\n",i,s,v);
			strcat(aux,t);
			empty=0;
		}
		s[0]=0;
		v[0]=0;
	}
	if(empty) PrintMessage(strings[S_Empty]);	//empty
	else PrintMessage(aux);
	free(aux);
}

void I2cspiR()		// I2C/SPI receive
{
	//if(DeviceDetected!=1) return;
	gtk_statusbar_push(status,statusID,"");
	saveLog = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_log));
	int nbyte=gtk_spin_button_get_value_as_int(I2CNbyte);
	if(nbyte<0) nbyte=0;
	if(nbyte>60) nbyte=60;
	int mode=0;
	if(gtk_toggle_button_get_active(I2C16bit)) mode=1;	//I2C mode
	if(gtk_toggle_button_get_active(SPI00)) mode=2;	//SPI mode 00
	if(gtk_toggle_button_get_active(SPI01)) mode=3;	//SPI mode 01
	if(gtk_toggle_button_get_active(SPI10)) mode=4;	//SPI mode 10
	if(gtk_toggle_button_get_active(SPI11)) mode=5;	//SPI mode 11
	char* tok;
	BYTE tmpbuf[128];
	int i=0;
	for(tok=strtok(gtk_entry_get_text(I2CDataSend)," ");tok&&i<128;tok=strtok(NULL," ")){
		if(sscanf(tok,"%x",&tmpbuf[i])) i++;
	}
	for(;i<128;i++) tmpbuf[i]=0;
	I2CReceive(mode,nbyte,tmpbuf);
}

void I2cspiS() // I2C/SPI send
{
	//if(DeviceDetected!=1) return;
	gtk_statusbar_push(status,statusID,"");
	saveLog = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_log));
	int j=1;
	int nbyte=gtk_spin_button_get_value_as_int(I2CNbyte);
	if(nbyte<0) nbyte=0;
	if(nbyte>57) nbyte=57;
	int mode=0;
	if(gtk_toggle_button_get_active(I2C16bit)) mode=1;	//I2C mode
	if(gtk_toggle_button_get_active(SPI00)) mode=2;	//SPI mode 00
	if(gtk_toggle_button_get_active(SPI01)) mode=3;	//SPI mode 01
	if(gtk_toggle_button_get_active(SPI10)) mode=4;	//SPI mode 10
	if(gtk_toggle_button_get_active(SPI11)) mode=5;	//SPI mode 11
	char* tok;
	BYTE tmpbuf[128];
	int i=0;
	for(tok=strtok(gtk_entry_get_text(I2CDataSend)," ");tok&&i<128;tok=strtok(NULL," ")){
		if(sscanf(tok,"%x",&tmpbuf[i])) i++;
	}
	for(;i<128;i++) tmpbuf[i]=0;
	I2CSend(mode,nbyte,tmpbuf);
}

int StartHVReg(double V){
	int j=1,z;
	int vreg=(int)(V*10.0);
	bufferU[0]=0;
	DWORD t0,t;
	if(V==-1){
		bufferU[j++]=VREG_DIS;			//disable HV regulator
		bufferU[j++]=FLUSH;
		write();
		msDelay(40);
		read();
		return -1;
	}
	t=t0=GetTickCount();
	bufferU[j++]=VREG_EN;			//enable HV regulator
	bufferU[j++]=SET_VPP;
	bufferU[j++]=vreg<80?vreg-8:vreg;		//set VPP, compensate for offset at low voltage
	bufferU[j++]=SET_PARAMETER;
	bufferU[j++]=SET_T3;
	bufferU[j++]=2000>>8;
	bufferU[j++]=2000&0xff;
	bufferU[j++]=WAIT_T3;
	bufferU[j++]=READ_ADC;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(20);
	read();
	for(z=1;z<DIMBUF-2&&bufferI[z]!=READ_ADC;z++);
	int v=(bufferI[z+1]<<8)+bufferI[z+2];
//	printf("v=%d=%fV\n",v,v/G);
	if(v==0){
		PrintMessage(strings[S_lowUsbV]);	//"Tensione USB troppo bassa (VUSB<4.5V)\r\n"
		return 0;
	}
	for(;(v<(vreg/10.0-0.5)*G||v>(vreg/10.0+0.5)*G)&&t<t0+1500;t=GetTickCount()){
		j=1;
		bufferU[j++]=WAIT_T3;
		bufferU[j++]=READ_ADC;
		bufferU[j++]=FLUSH;
		for(;j<DIMBUF;j++) bufferU[j]=0x0;
		write();
		msDelay(20);
		read();
		for(z=1;z<DIMBUF-2&&bufferI[z]!=READ_ADC;z++);
		v=(bufferI[z+1]<<8)+bufferI[z+2];
		if(HwID==3) v>>=2;		//if 12 bit ADC
//		printf("v=%d=%fV\n",v,v/G);
	}
	if(v>(vreg/10.0+0.7)*G){
		PrintMessage(strings[S_HiVPP]);	//"Attenzione: tensione regolatore troppo alta\r\n\r\n"
		return 0;
	}
	else if(v<(vreg/10.0-0.7)*G){
		PrintMessage(strings[S_LowVPP]);	//"Attenzione: tensione regolatore troppo bassa\r\n\r\n"
		return 0;
	}
	else if(v==0){
		PrintMessage(strings[S_lowUsbV]);	//"Tensione USB troppo bassa (VUSB<4.5V)\r\n"
		return 0;
	}
	else{
		PrintMessage2(strings[S_reg],t-t0,v/G);	//"Regolatore avviato e funzionante dopo T=%d ms VPP=%.1f\r\n\r\n"
		return vreg;
	}
}

void ProgID()
{
	if(DeviceDetected!=1) return;
	int j=1;
	bufferU[0]=0;
	bufferU[j++]=PROG_RST;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(2);
	read();
	PrintMessage3(strings[S_progver],bufferI[2],bufferI[3],bufferI[4]); //"FW versione %d.%d.%d\r\n"
	FWVersion=(bufferI[2]<<16)+(bufferI[3]<<8)+bufferI[4];
	PrintMessage3(strings[S_progid],bufferI[5],bufferI[6],bufferI[7]);	//"ID Hw: %d.%d.%d"
	HwID=bufferI[7];
	if(HwID==1) PrintMessage(" (18F2550)\r\n\r\n");
	else if(HwID==2) PrintMessage(" (18F2450)\r\n\r\n");
	else if(HwID==3) PrintMessage(" (18F2458/2553)\r\n\r\n");
	else printf(" (?)\r\n\r\n");
}

int CheckV33Regulator()
{
	int i,j=1;
	bufferU[j++]=WRITE_RAM;
	bufferU[j++]=0x0F;
	bufferU[j++]=0x93;
	bufferU[j++]=0xFE;	//B0 = output
	bufferU[j++]=EXT_PORT;
	bufferU[j++]=0x01;	//B0=1
	bufferU[j++]=0;
	bufferU[j++]=READ_RAM;
	bufferU[j++]=0x0F;
	bufferU[j++]=0x81;	//Check if B1=1
	bufferU[j++]=EXT_PORT;
	bufferU[j++]=0x00;	//B0=0
	bufferU[j++]=0;
	bufferU[j++]=READ_RAM;
	bufferU[j++]=0x0F;
	bufferU[j++]=0x81;	//Check if B1=0
	bufferU[j++]=WRITE_RAM;
	bufferU[j++]=0x0F;
	bufferU[j++]=0x93;
	bufferU[j++]=0xFF;	//BX = input
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(2);
	read();
	for(j=1;j<DIMBUF-3&&bufferI[j]!=READ_RAM;j++);
	i=bufferI[j+3]&0x2;		//B1 should be high
	for(j+=3;j<DIMBUF-3&&bufferI[j]!=READ_RAM;j++);
	return (i+bufferI[j+3]&0x2)==2?1:0;
}

void TestHw(GtkWidget *widget,GtkWindow* parent){
	if(DeviceDetected!=1) return;
	int j=1;
	bufferU[0]=0;
	MsgBox(strings[I_TestHW]);		//"Test hardware ..."
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x0;
	bufferU[j++]=EN_VPP_VCC;		//VDD+VPP
	bufferU[j++]=0x5;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(20);
	read();
	MsgBox("VDD=5V, VPP=13V, D=0V, CK=0V, PGM=0V");
	j=1;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x15;
	bufferU[j++]=EN_VPP_VCC;
	bufferU[j++]=0x1;			//VDD
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(20);
	read();
	MsgBox("VDD=5V, VPP=0V, D=5V, CK=5V, PGM=5V");
	j=1;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x1;
	bufferU[j++]=EN_VPP_VCC;
	bufferU[j++]=0x4;			//VPP
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(20);
	read();
	MsgBox("VDD=0V, VPP=13V, D=5V, CK=0V, PGM=0V");
	j=1;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x4;
	bufferU[j++]=EN_VPP_VCC;
	bufferU[j++]=0x0;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(20);
	read();
	MsgBox("VDD=0V, VPP=0V, D=0V, CK=5V, PGM=0V");
	j=1;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x0;
	bufferU[j++]=EN_VPP_VCC;
	bufferU[j++]=0x0;			//VPP
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(20);
	read();
}

void msDelay(double delay)
{
#if !defined _WIN32 && !defined __CYGWIN__
	long x=(int)delay*1000.0;
	usleep(x>MinDly?x:MinDly);
#else
	Sleep((long)ceil(delay)>MinDly?(long)ceil(delay):MinDly);
#endif
}

int FindDevice(){
	int MyDeviceDetected = FALSE;
#if !defined _WIN32 && !defined __CYGWIN__
	struct hiddev_devinfo device_info;
	int i;
	if(path[0]==0){	//search all devices
		for(i=0;i<16;i++){
			sprintf(path,"/dev/usb/hiddev%d",i);
			if((fd = open(path, O_RDONLY ))>0){
				ioctl(fd, HIDIOCGDEVINFO, &device_info);
				if(device_info.vendor==vid&&device_info.product==pid) break;
				else close(fd);
			}
		}
		if(i==16){
			PrintMessage(strings[S_noprog]);
			return 0;
		}
	}
	else{	//user supplied path
		if ((fd = open(path, O_RDONLY )) < 0) {
			PrintMessage1("cannot open %s, make sure you have read permission on it",path);
			return 0;
		}
		ioctl(fd, HIDIOCGDEVINFO, &device_info);
		if(device_info.vendor!=vid||device_info.product!=pid){
			PrintMessage(strings[S_noprog]);
			return 0;
		}
	}
	MyDeviceDetected = TRUE;
	rep_info_u.report_type=HID_REPORT_TYPE_OUTPUT;
	rep_info_i.report_type=HID_REPORT_TYPE_INPUT;
	rep_info_u.report_id=rep_info_i.report_id=HID_REPORT_ID_FIRST;
	rep_info_u.num_fields=rep_info_i.num_fields=1;
	ref_multi_u.uref.report_type=HID_REPORT_TYPE_OUTPUT;
	ref_multi_i.uref.report_type=HID_REPORT_TYPE_INPUT;
	ref_multi_u.uref.report_id=ref_multi_i.uref.report_id=HID_REPORT_ID_FIRST;
	ref_multi_u.uref.field_index=ref_multi_i.uref.field_index=0;
	ref_multi_u.uref.usage_index=ref_multi_i.uref.usage_index=0;
	ref_multi_u.num_values=ref_multi_i.num_values=DIMBUF;

#else
	char string[256];
	PSP_DEVICE_INTERFACE_DETAIL_DATA detailData;
	HANDLE DeviceHandle;
	HANDLE hDevInfo;
	GUID HidGuid;
	char MyDevicePathName[1024];
	ULONG Length;
	ULONG Required;
	typedef struct _HIDD_ATTRIBUTES {
	    ULONG   Size;
	    USHORT  VendorID;
	    USHORT  ProductID;
	    USHORT  VersionNumber;
	} HIDD_ATTRIBUTES, *PHIDD_ATTRIBUTES;

	typedef void (__stdcall*GETHIDGUID) (OUT LPGUID HidGuid);
	typedef BOOLEAN (__stdcall*GETATTRIBUTES)(IN HANDLE HidDeviceObject,OUT PHIDD_ATTRIBUTES Attributes);
	typedef BOOLEAN (__stdcall*SETNUMINPUTBUFFERS)(IN  HANDLE HidDeviceObject,OUT ULONG  NumberBuffers);
	typedef BOOLEAN (__stdcall*GETNUMINPUTBUFFERS)(IN  HANDLE HidDeviceObject,OUT PULONG  NumberBuffers);
	typedef BOOLEAN (__stdcall*GETFEATURE) (IN  HANDLE HidDeviceObject, OUT PVOID ReportBuffer, IN ULONG ReportBufferLength);
	typedef BOOLEAN (__stdcall*SETFEATURE) (IN  HANDLE HidDeviceObject, IN PVOID ReportBuffer, IN ULONG ReportBufferLength);
	typedef BOOLEAN (__stdcall*GETREPORT) (IN  HANDLE HidDeviceObject, OUT PVOID ReportBuffer, IN ULONG ReportBufferLength);
	typedef BOOLEAN (__stdcall*SETREPORT) (IN  HANDLE HidDeviceObject, IN PVOID ReportBuffer, IN ULONG ReportBufferLength);
	typedef BOOLEAN (__stdcall*GETMANUFACTURERSTRING) (IN  HANDLE HidDeviceObject, OUT PVOID ReportBuffer, IN ULONG ReportBufferLength);
	typedef BOOLEAN (__stdcall*GETPRODUCTSTRING) (IN  HANDLE HidDeviceObject, OUT PVOID ReportBuffer, IN ULONG ReportBufferLength);
	typedef BOOLEAN (__stdcall*GETINDEXEDSTRING) (IN  HANDLE HidDeviceObject, IN ULONG  StringIndex, OUT PVOID ReportBuffer, IN ULONG ReportBufferLength);
	HIDD_ATTRIBUTES Attributes;
	SP_DEVICE_INTERFACE_DATA devInfoData;
	int LastDevice = FALSE;
	int MemberIndex = 0;
	LONG Result;
	char UsageDescription[256];

	Length=0;
	detailData=NULL;
	DeviceHandle=NULL;

	HMODULE hHID=0;
	GETHIDGUID HidD_GetHidGuid=0;
	GETATTRIBUTES HidD_GetAttributes=0;
	SETNUMINPUTBUFFERS HidD_SetNumInputBuffers=0;
	GETNUMINPUTBUFFERS HidD_GetNumInputBuffers=0;
	GETFEATURE HidD_GetFeature=0;
	SETFEATURE HidD_SetFeature=0;
	GETREPORT HidD_GetInputReport=0;
	SETREPORT HidD_SetOutputReport=0;
	GETMANUFACTURERSTRING HidD_GetManufacturerString=0;
	GETPRODUCTSTRING HidD_GetProductString=0;
	hHID = LoadLibrary("hid.dll");
	if(!hHID){
		PrintMessage("Can't find hid.dll");
		return 0;
	}
	HidD_GetHidGuid=(GETHIDGUID)GetProcAddress(hHID,"HidD_GetHidGuid");
	HidD_GetAttributes=(GETATTRIBUTES)GetProcAddress(hHID,"HidD_GetAttributes");
	HidD_SetNumInputBuffers=(SETNUMINPUTBUFFERS)GetProcAddress(hHID,"HidD_SetNumInputBuffers");
	HidD_GetNumInputBuffers=(GETNUMINPUTBUFFERS)GetProcAddress(hHID,"HidD_GetNumInputBuffers");
	HidD_GetFeature=(GETFEATURE)GetProcAddress(hHID,"HidD_GetFeature");
	HidD_SetFeature=(SETFEATURE)GetProcAddress(hHID,"HidD_SetFeature");
	HidD_GetInputReport=(GETREPORT)GetProcAddress(hHID,"HidD_GetInputReport");
	HidD_SetOutputReport=(SETREPORT)GetProcAddress(hHID,"HidD_SetOutputReport");
	HidD_GetManufacturerString=(GETMANUFACTURERSTRING)GetProcAddress(hHID,"HidD_GetManufacturerString");
	HidD_GetProductString=(GETPRODUCTSTRING)GetProcAddress(hHID,"HidD_GetProductString");
	if(HidD_GetHidGuid==NULL\
		||HidD_GetAttributes==NULL\
		||HidD_GetFeature==NULL\
		||HidD_SetFeature==NULL\
		||HidD_GetInputReport==NULL\
		||HidD_SetOutputReport==NULL\
		||HidD_GetManufacturerString==NULL\
		||HidD_GetProductString==NULL\
		||HidD_SetNumInputBuffers==NULL\
		||HidD_GetNumInputBuffers==NULL) return 0;


	HMODULE hSAPI=0;
	hSAPI = LoadLibrary("setupapi.dll");
	if(!hSAPI){
		PrintMessage("Can't find setupapi.dll");
		return 0;
	}
	typedef HDEVINFO (WINAPI* SETUPDIGETCLASSDEVS) (CONST GUID*,PCSTR,HWND,DWORD);
	typedef BOOL (WINAPI* SETUPDIENUMDEVICEINTERFACES) (HDEVINFO,PSP_DEVINFO_DATA,CONST GUID*,DWORD,PSP_DEVICE_INTERFACE_DATA);
	typedef BOOL (WINAPI* SETUPDIGETDEVICEINTERFACEDETAIL) (HDEVINFO,PSP_DEVICE_INTERFACE_DATA,PSP_DEVICE_INTERFACE_DETAIL_DATA_A,DWORD,PDWORD,PSP_DEVINFO_DATA);
	typedef BOOL (WINAPI* SETUPDIDESTROYDEVICEINFOLIST) (HDEVINFO);
	SETUPDIGETCLASSDEVS SetupDiGetClassDevsA=0;
	SETUPDIENUMDEVICEINTERFACES SetupDiEnumDeviceInterfaces=0;
	SETUPDIGETDEVICEINTERFACEDETAIL SetupDiGetDeviceInterfaceDetailA=0;
	SETUPDIDESTROYDEVICEINFOLIST SetupDiDestroyDeviceInfoList=0;
	SetupDiGetClassDevsA=(SETUPDIGETCLASSDEVS) GetProcAddress(hSAPI,"SetupDiGetClassDevsA");
	SetupDiEnumDeviceInterfaces=(SETUPDIENUMDEVICEINTERFACES) GetProcAddress(hSAPI,"SetupDiEnumDeviceInterfaces");
	SetupDiGetDeviceInterfaceDetailA=(SETUPDIGETDEVICEINTERFACEDETAIL) GetProcAddress(hSAPI,"SetupDiGetDeviceInterfaceDetailA");
	SetupDiDestroyDeviceInfoList=(SETUPDIDESTROYDEVICEINFOLIST) GetProcAddress(hSAPI,"SetupDiDestroyDeviceInfoList");
	if(SetupDiGetClassDevsA==NULL\
		||SetupDiEnumDeviceInterfaces==NULL\
		||SetupDiDestroyDeviceInfoList==NULL\
		||SetupDiGetDeviceInterfaceDetailA==NULL) return 0;


	/*
	The following code is adapted from Usbhidio_vc6 application example by Jan Axelson
	for more information see see http://www.lvr.com/hidpage.htm
	*/

	/*
	API function: HidD_GetHidGuid
	Get the GUID for all system HIDs.
	Returns: the GUID in HidGuid.
	*/
	HidD_GetHidGuid(&HidGuid);

	/*
	API function: SetupDiGetClassDevs
	Returns: a handle to a device information set for all installed devices.
	Requires: the GUID returned by GetHidGuid.
	*/
	hDevInfo=SetupDiGetClassDevs(&HidGuid,NULL,NULL,DIGCF_PRESENT|DIGCF_INTERFACEDEVICE);
	devInfoData.cbSize = sizeof(devInfoData);

	//Step through the available devices looking for the one we want.
	//Quit on detecting the desired device or checking all available devices without success.
	MemberIndex = 0;
	LastDevice = FALSE;
	do
	{
		/*
		API function: SetupDiEnumDeviceInterfaces
		On return, MyDeviceInterfaceData contains the handle to a
		SP_DEVICE_INTERFACE_DATA structure for a detected device.
		Requires:
		The DeviceInfoSet returned in SetupDiGetClassDevs.
		The HidGuid returned in GetHidGuid.
		An index to specify a device.
		*/
		Result=SetupDiEnumDeviceInterfaces (hDevInfo, 0, &HidGuid, MemberIndex, &devInfoData);
		if (Result != 0)
		{
			//A device has been detected, so get more information about it.
			/*
			API function: SetupDiGetDeviceInterfaceDetail
			Returns: an SP_DEVICE_INTERFACE_DETAIL_DATA structure
			containing information about a device.
			To retrieve the information, call this function twice.
			The first time returns the size of the structure in Length.
			The second time returns a pointer to the data in DeviceInfoSet.
			Requires:
			A DeviceInfoSet returned by SetupDiGetClassDevs
			The SP_DEVICE_INTERFACE_DATA structure returned by SetupDiEnumDeviceInterfaces.

			The final parameter is an optional pointer to an SP_DEV_INFO_DATA structure.
			This application doesn't retrieve or use the structure.
			If retrieving the structure, set
			MyDeviceInfoData.cbSize = length of MyDeviceInfoData.
			and pass the structure's address.
			*/
			//Get the Length value.
			//The call will return with a "buffer too small" error which can be ignored.
			Result = SetupDiGetDeviceInterfaceDetail(hDevInfo, &devInfoData, NULL, 0, &Length, NULL);

			//Allocate memory for the hDevInfo structure, using the returned Length.
			detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(Length);

			//Set cbSize in the detailData structure.
			detailData -> cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

			//Call the function again, this time passing it the returned buffer size.
			Result = SetupDiGetDeviceInterfaceDetail(hDevInfo, &devInfoData, detailData, Length,&Required, NULL);

			// Open a handle to the device.
			// To enable retrieving information about a system mouse or keyboard,
			// don't request Read or Write access for this handle.
			/*
			API function: CreateFile
			Returns: a handle that enables reading and writing to the device.
			Requires:
			The DevicePath in the detailData structure
			returned by SetupDiGetDeviceInterfaceDetail.
			*/
			DeviceHandle=CreateFile(detailData->DevicePath,
				0, FILE_SHARE_READ|FILE_SHARE_WRITE,
				(LPSECURITY_ATTRIBUTES)NULL,OPEN_EXISTING, 0, NULL);

			/*
			API function: HidD_GetAttributes
			Requests information from the device.
			Requires: the handle returned by CreateFile.
			Returns: a HIDD_ATTRIBUTES structure containing
			the Vendor ID, Product ID, and Product Version Number.
			Use this information to decide if the detected device is
			the one we're looking for.
			*/

			//Set the Size to the number of bytes in the structure.
			Attributes.Size = sizeof(Attributes);
			Result = HidD_GetAttributes(DeviceHandle,&Attributes);

			//Is it the desired device?
			MyDeviceDetected = FALSE;
			char a[256];
			if (Attributes.VendorID == vid)
			{
				if (Attributes.ProductID == pid)
				{
					//Both the Vendor ID and Product ID match.
					MyDeviceDetected = TRUE;
					strcpy(MyDevicePathName,detailData->DevicePath);

					// Get a handle for writing Output reports.
					WriteHandle=CreateFile(detailData->DevicePath,
						GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
						(LPSECURITY_ATTRIBUTES)NULL,OPEN_EXISTING,0,NULL);

					//Get a handle to the device for the overlapped ReadFiles.
					ReadHandle=CreateFile(detailData->DevicePath,
						GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,(LPSECURITY_ATTRIBUTES)NULL,
						OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);

					if (hEventObject) CloseHandle(hEventObject);
					hEventObject = CreateEvent(NULL,TRUE,TRUE,"");

					//Set the members of the overlapped structure.
					HIDOverlapped.hEvent = hEventObject;
					HIDOverlapped.Offset = 0;
					HIDOverlapped.OffsetHigh = 0;
					Result=HidD_SetNumInputBuffers(DeviceHandle,64);
				}
				else
					//The Product ID doesn't match.
					CloseHandle(DeviceHandle);
			}
			else
				//The Vendor ID doesn't match.
				CloseHandle(DeviceHandle);
		//Free the memory used by the detailData structure (no longer needed).
		free(detailData);
		}
		else
			//SetupDiEnumDeviceInterfaces returned 0, so there are no more devices to check.
			LastDevice=TRUE;
		//If we haven't found the device yet, and haven't tried every available device,
		//try the next one.
		MemberIndex = MemberIndex + 1;
	} //do
	while ((LastDevice == FALSE) && (MyDeviceDetected == FALSE));

	//Free the memory reserved for hDevInfo by SetupDiClassDevs.
	SetupDiDestroyDeviceInfoList(hDevInfo);

/*	if(info){
		PrintMessage3("Device detected: vid=0x%04X pid=0x%04X\nPath: %s\n",vid,pid,MyDevicePathName);
		if(HidD_GetManufacturerString(DeviceHandle,string,sizeof(string))==TRUE) wprintf(L"Manufacturer string: %s\n",string);
		if(HidD_GetProductString(DeviceHandle,string,sizeof(string))==TRUE) wprintf(L"Product string: %s\n",string);
	}*/
#endif
	if (MyDeviceDetected == FALSE){
		PrintMessage(strings[S_noprog]);	//"Programmer not detected\r\n"
		//gtk_statusbar_push(status,statusID,strings[S_noprog]);
	}
	else{
		PrintMessage(strings[S_prog]);	//"Programmer detected\r\n");
		//gtk_statusbar_push(status,statusID,strings[S_prog]);
	}
	return MyDeviceDetected;
}
