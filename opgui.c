/**
 * \file opgui.c
 * main control program for the open programmer
 *
 * Copyright (C) 2009-2023 Alberto Maccioni
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
#include "common.h"
#include "I2CSPI.h"
#include "coff.h"
#include "icd.h"
#include "deviceRW.h"
#include "fileIO.h"
#include "progAVR.h"
#include <string.h>

#define MAXLINES 600
#define  CONFIG_FILE "opgui.ini"
#define  CONFIG_DIR ".opgui"
#define MinDly 0
void Connect(GtkWidget *widget,GtkWidget *window);
void I2cspiR();
void I2cspiS();
void ProgID();
void PrintMessageI2C(const char *msg);
void ShowContext();
int FindDevice(int vid,int pid);
void TestHw();
int CheckS1();
char** strings;	//!localized strings
int cmdline=0;
int saveLog=0,programID=0,load_osccal=0,load_BKosccal=0;
int use_osccal=1,use_BKosccal=0;
int load_calibword=0,max_err=200;
int AVRlock=0x100,AVRfuse=0x100,AVRfuse_h=0x100,AVRfuse_x=0x100;
int ICDenable=0,ICDaddr=0x1FF0;
int FWVersion=0,HwID=0;
FILE* logfile=0;
char LogFileName[512]="";
char loadfile[512]="",savefile[512]="";
char loadfileEE[512]="",savefileEE[512]="";
char CoffFileName[512]="";
int vid=0x1209,pid=0x5432;
int new_vid=0x1209,new_pid=0x5432;
int old_vid=0x04D8,old_pid=0x0100;
WORD *memCODE_W=0;
int size=0,sizeW=0,sizeEE=0,sizeCONFIG=0,sizeUSERID=0;
unsigned char *memCODE=0,*memEE=0,memID[64],memCONFIG[48],memUSERID[8];
double hvreg=0;
int DeviceDetected=0;
int IOTimer=0;
int skipV33check=0;
int waitS1=0,waitingS1=0;
int progress=0;
int RWstop=0;
int forceConfig=0;
#ifdef DEBUG
	int addrDebug=0;
	unsigned short dataDebug=0;
	unsigned short statusDebug=0x3FFF;
#endif
//List of gtk controls
GtkTextBuffer * dataBuf;
GtkWidget * data,*data_scroll;
GtkWidget * window;
GtkWidget * toolbar;
GtkWidget * button;
GtkWidget * b_open;
GtkWidget * b_save;
GtkWidget * b_read;
GtkWidget * b_write;
GtkWidget * notebook;
GtkWidget * label;
GtkWidget * status_bar;
GtkWidget * img;
GtkWidget * devTree;
GtkWidget * devTypeCombo;
GtkWidget * devFramePIC;
GtkWidget * ICD_check;
GtkWidget * ICD_addr_entry;
GtkWidget * EEPROM_RW;
GtkWidget * ReadReserved;
GtkWidget * Write_ID_BKCal;
GtkWidget * WriteCalib12;
GtkWidget * UseOSCCAL;
GtkWidget * UseBKOSCCAL;
GtkWidget * UseFileCal;
GtkWidget * devFrameAVR;
GtkWidget * AVR_FuseLow,* AVR_FuseLowWrite,* AVR_FuseHigh,* AVR_FuseHighWrite,* AVR_FuseExt;
GtkWidget * AVR_FuseExtWrite,* AVR_Lock,* AVR_LockWrite;
GtkWidget * b_WfuseLF;
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
GtkWidget * I2CSpeed;
GtkWidget * DCDC_ON;
GtkWidget * DCDC_voltage;
GtkWidget * VPP_ON;
GtkWidget * VDD_ON;
GtkWidget * b_io_active;
GtkWidget * commandSend;
GtkWidget * commandTransfer;
GtkWidget * b_V33check;
GtkWidget * Hex_entry;
GtkWidget * Address_entry;
GtkWidget * Data_entry;
GtkWidget * Hex_data;
GtkWidget * Hex_data2;
GtkWidget * CW1_entry;
GtkWidget * CW2_entry;
GtkWidget * CW3_entry;
GtkWidget * CW4_entry;
GtkWidget * CW5_entry;
GtkWidget * CW6_entry;
GtkWidget * CW7_entry;
GtkWidget * ConfigForce;
GtkWidget * b_WaitS1;
GtkWidget * devFrameConfigW;
GtkWidget * devFrameICD;
GtkWidget * devFrameOsc;
GtkWidget * devPIC_CW1;
GtkWidget * devPIC_CW2;
GtkWidget * devPIC_CW3;
GtkWidget * devPIC_CW4;
GtkWidget * devPIC_CW5;
GtkWidget * devPIC_CW6;
GtkWidget * devPIC_CW7;
GtkWidget * devPIC_CW8;
GtkWidget * devinfo;
GtkWidget* stopBtn;
GtkWidget* readBtn;
GtkWidget* writeBtn;
GtkListStore *devStore;
GtkWidget *devTree, *devFiltEntry, *devFrame;
GtkTreeSelection *devSel;
///array of radio buttons for IO manual control
struct io_btn {	char * name;
				int x;
				int y;
				GtkWidget * r_0;	//radio button 0
				GtkWidget * r_1;	//radio button 1
				GtkWidget * r_I;	//radio button I
				GtkWidget * e_I;	//entry
				} ioButtons[13];
int statusID;
int ee = 0;
int readRes=0;
char dev[64]="";
int devType=-1;
char str[4096]="";
char* cur_path=0;
char* cur_pathEE=0;
enum dev_column_t {
  DEVICE_ID_COLUMN = 0,
  DEVICE_NAME_COLUMN,
  DEVICE_GROUP_COLUMN,
  DEVICE_N_COLUMNS
};
enum sort_type_t {
  SORT_STRING_NAME = 0,
  SORT_STRING_GROUP
};
enum sort_data_type_t { SDT_STRING = 0 };
char *groupNames[NUM_GROUPS] = {
	"PIC10/12",
	"PIC16",
	"PIC18",
	"PIC24",
	"PIC30/33",
	"ATMEL AVR",
	"MEMORY"
};
char *GROUP_ALL="*";

#if !defined _WIN32 && !defined __CYGWIN__	//Linux
	int fd = -1;
#ifdef hiddevIO	
	struct hiddev_report_info rep_info_i,rep_info_u;
	struct hiddev_usage_ref_multi ref_multi_i,ref_multi_u;
#endif
	char path[512]="";
	unsigned char bufferU[128],bufferI[128];
#else	//Windows
	unsigned char bufferU0[128],bufferI0[128];
	unsigned char *bufferU,*bufferI;
	DWORD NumberOfBytesRead,BytesWritten;
	ULONG Result;
	HANDLE WriteHandle,ReadHandle;
	OVERLAPPED HIDOverlapped;
	HANDLE hEventObject;
#endif
///
///Exit
gint delete_event( GtkWidget *widget,GdkEvent *event,gpointer data )
{
gtk_main_quit ();
return FALSE;
}
///
///Show program info window
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
		//"authors","Alberto Maccioni",NULL,
		"comments", "A graphical interface for the Open Programmer",
		"copyright",
		"Copyright (C) Alberto Maccioni 2009-2022\n\n"
		"This program is free software; you can \n"
		"redistribute it and/or modify it under \n"
		"the terms of the GNU General Public License \n"
		"as published by the Free Software Foundation;\n"
		"either version 2 of the License, or \n"
		"(at your option) any later version.",
		//"documenters"              GStrv*                : Read / Write
		"license",license,
		"logo",gdk_pixbuf_new_from_resource("/res/sys.png", NULL),
		//  "logo-icon-name"           gchar*                : Read / Write
		"program-name", "OPGUI",
		//  "translator-credits"       gchar*                : Read / Write
		"version",VERSION,
		"website","www.openprog.altervista.org",
		//  "website-label"            gchar*                : Read / Write
  		"wrap-license",TRUE,
		"title","Info about OPGUI",
		NULL);
}
///
///Append a message on the data tab; shorten the length of the entry field to MAXLINES
void PrintMessage(const char *msg){
	if(cmdline) return;	//do not print anything if using command line mode
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
	gtk_text_buffer_get_end_iter(dataBuf,&iter);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(data),&iter,0.0,FALSE,0,0);
}
///
///Print a message on the I2C data field
void PrintMessageI2C(const char *msg){
	GtkTextIter iter;
	GtkTextBuffer * dataBuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(I2CDataReceive));
	gtk_text_buffer_set_text(dataBuf,msg,-1);
	gtk_text_buffer_get_end_iter(dataBuf,&iter);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(I2CDataReceive),&iter,0.0,FALSE,0,0);
	while (gtk_events_pending ()) gtk_main_iteration();
}
///
///Print a message on the "command" data field
void PrintMessageCMD(const char *msg){
	GtkTextIter iter;
	GtkTextBuffer * dataBuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(commandTransfer));
	gtk_text_buffer_set_text(dataBuf,msg,-1);
	gtk_text_buffer_get_end_iter(dataBuf,&iter);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(commandTransfer),&iter,0.0,FALSE,0,0);
}
///
///Update option variables according to actual control values
void getOptions()
{
	vid=htoi(gtk_entry_get_text(GTK_ENTRY(VID_entry)),4);
	pid=htoi(gtk_entry_get_text(GTK_ENTRY(PID_entry)),4);
	saveLog = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_log));
	ee = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(EEPROM_RW))?0xFFFF:0;
	programID = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Write_ID_BKCal));
	max_err=atoi(gtk_entry_get_text(GTK_ENTRY(Errors_entry)));
	load_calibword= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(WriteCalib12));
	load_osccal= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(UseOSCCAL));
	load_BKosccal= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(UseBKOSCCAL));
	ICDenable= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ICD_check));
	readRes= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ReadReserved));
	skipV33check=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_V33check));
	waitS1=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_WaitS1));
	int i=sscanf(gtk_entry_get_text(GTK_ENTRY(ICD_addr_entry)),"%x",&ICDaddr);
	if(i!=1||ICDaddr<0||ICDaddr>0xFFFF) ICDaddr=0x1FF0;
	char *str=0;//gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(devCombo));
	if(str) strncpy(dev,str,sizeof(dev)-1);
	g_free(str);
	AVRfuse=AVRfuse_h=AVRfuse_x=AVRlock=0x100;
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(AVR_FuseLowWrite))){
		i=sscanf(gtk_entry_get_text(GTK_ENTRY(AVR_FuseLow)),"%x",&AVRfuse);
		if(i!=1||AVRfuse<0||AVRfuse>0xFF) AVRfuse=0x100;
	}
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(AVR_FuseHighWrite))){
		i=sscanf(gtk_entry_get_text(GTK_ENTRY(AVR_FuseHigh)),"%x",&AVRfuse_h);
		if(i!=1||AVRfuse_h<0||AVRfuse_h>0xFF) AVRfuse_h=0x100;
	}
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(AVR_FuseExtWrite))){
		i=sscanf(gtk_entry_get_text(GTK_ENTRY(AVR_FuseExt)),"%x",&AVRfuse_x);
		if(i!=1||AVRfuse_x<0||AVRfuse_x>0xFF) AVRfuse_x=0x100;
	}
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(AVR_LockWrite))){
		i=sscanf(gtk_entry_get_text(GTK_ENTRY(AVR_Lock)),"%x",&AVRlock);
		if(i!=1||AVRlock<0||AVRlock>0xFF) AVRlock=0x100;
	}
	str=malloc(128);
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ConfigForce))){
		int cw1,cw2,cw3,cw4,cw5,cw6,cw7;
		cw1=cw2=cw3=cw4=cw5=cw6=cw7=0x10000;
		i=sscanf(gtk_entry_get_text(GTK_ENTRY(CW1_entry)),"%x",&cw1);
		i=sscanf(gtk_entry_get_text(GTK_ENTRY(CW2_entry)),"%x",&cw2);
		i=sscanf(gtk_entry_get_text(GTK_ENTRY(CW3_entry)),"%x",&cw3);
		i=sscanf(gtk_entry_get_text(GTK_ENTRY(CW4_entry)),"%x",&cw4);
		i=sscanf(gtk_entry_get_text(GTK_ENTRY(CW5_entry)),"%x",&cw5);
		i=sscanf(gtk_entry_get_text(GTK_ENTRY(CW6_entry)),"%x",&cw6);
		i=sscanf(gtk_entry_get_text(GTK_ENTRY(CW7_entry)),"%x",&cw7);
		if(devType==PIC16){
			if((!strncmp(dev,"16F1",4)||!strncmp(dev,"12F1",4))&&sizeW>0x8008){		//16F1xxx
				if(cw1<=0x3FFF){
					memCODE_W[0x8007]=cw1;
					PrintMessage3(strings[S_ForceConfigWx],1,0x8007,cw1); //"forcing config word%d [0x%04X]=0x%04X"
				}
				if(cw2<=0x3FFF){
					memCODE_W[0x8008]=cw2;
					PrintMessage3(strings[S_ForceConfigWx],2,0x8008,cw2); //"forcing config word%d [0x%04X]=0x%04X"
				}
			}
			else{	//16Fxxx
				if(cw1<=0x3FFF&&sizeW>0x2007){
					memCODE_W[0x2007]=cw1;
					PrintMessage3(strings[S_ForceConfigWx],1,0x2007,cw1); //"forcing config word%d [0x%04X]=0x%04X"
				}
				if(cw2<=0x3FFF&&sizeW>0x2008){
					memCODE_W[0x2008]=cw2;
					printf("2\n");
					PrintMessage3(strings[S_ForceConfigWx],2,0x2008,cw2); //"forcing config word%d [0x%04X]=0x%04X"
				}
			}
		}
		else if(devType==PIC12){	//12Fxxx
			if(cw1<=0xFFF&&sizeW>0xFFF){
				memCODE_W[0xFFF]=cw1;
				PrintMessage3(strings[S_ForceConfigWx],1,0xFFF,cw1); //"forcing config word%d [0x%04X]=0x%04X"
			}
		}
		else if(devType==PIC18){	//18Fxxx
			if(cw1<=0xFFFF){
				memCONFIG[0]=cw1&0xFF;
				memCONFIG[1]=(cw1>>8)&0xFF;
			}
			if(cw2<=0xFFFF){
				memCONFIG[2]=cw2&0xFF;
				memCONFIG[3]=(cw2>>8)&0xFF;
			}
			if(cw3<=0xFFFF){
				memCONFIG[4]=cw3&0xFF;
				memCONFIG[5]=(cw3>>8)&0xFF;
			}
			if(cw4<=0xFFFF){
				memCONFIG[6]=cw4&0xFF;
				memCONFIG[7]=(cw4>>8)&0xFF;
			}
			if(cw5<=0xFFFF){
				memCONFIG[8]=cw5&0xFF;
				memCONFIG[9]=(cw5>>8)&0xFF;
			}
			if(cw6<=0xFFFF){
				memCONFIG[10]=cw6&0xFF;
				memCONFIG[11]=(cw6>>8)&0xFF;
			}
			if(cw7<=0xFFFF){
				memCONFIG[12]=cw7&0xFF;
				memCONFIG[13]=(cw7>>8)&0xFF;
			}
			PrintMessage(strings[S_ForceConfigW]); //"forcing config words"
			for(i=0;i<7;i++){
				PrintMessage2(strings[S_ConfigWordH],i+1,memCONFIG[i*2+1]);	//"CONFIG%dH: 0x%02X\t"
				PrintMessage2(strings[S_ConfigWordL],i+1,memCONFIG[i*2]);	//"CONFIG%dL: 0x%02X\r\n"
			}
		}
	}
	free(str);
}
///
///Check GUI for selected device and put in variable 'dev'. Also enable/disable R/W buttons
void GetSelectedDevice() {
	GtkTreeModel *tmpModel;
	GtkTreeIter tmpIter;
	char *devName;
	if (!GTK_IS_TREE_SELECTION(devSel)) { // Not initialised yet
		return;
	}
	if (gtk_tree_selection_get_selected(devSel, &tmpModel, &tmpIter)) {
		gtk_tree_model_get(tmpModel, &tmpIter, DEVICE_NAME_COLUMN, &devName, -1);
		strcpy(dev,devName);
		gtk_widget_set_sensitive(GTK_WIDGET(readBtn), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(writeBtn), TRUE);
		g_free(devName);
	} else { // Shouldn't ever happen, but just in case
		dev[0] = '\0';
		gtk_widget_set_sensitive(GTK_WIDGET(readBtn), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(writeBtn), FALSE);
	}
}
///
///Choose a file to open and call Load()
void Fopen(GtkWidget *widget,GtkWidget *window)
{
	GetSelectedDevice();
	if(progress) return;
	progress=1;
	GtkFileChooser *dialog;
	dialog = (GtkFileChooser*) gtk_file_chooser_dialog_new (strings[I_Fopen], //"Open File"
					GTK_WINDOW(window),
					GTK_FILE_CHOOSER_ACTION_OPEN,
					strings[I_CANCEL], GTK_RESPONSE_CANCEL,
					strings[I_OPEN], GTK_RESPONSE_ACCEPT,
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
			GtkFileChooser *dialog2;
			dialog2 = (GtkFileChooser*) gtk_file_chooser_dialog_new (strings[S_openEEfile],
							GTK_WINDOW(window),
							GTK_FILE_CHOOSER_ACTION_OPEN,
							strings[I_CANCEL],GTK_RESPONSE_CANCEL,
							strings[I_OPEN],GTK_RESPONSE_ACCEPT,
							NULL);
			if(!cur_pathEE) cur_pathEE = gtk_file_chooser_get_current_folder(dialog);
			if(cur_pathEE) gtk_file_chooser_set_current_folder(dialog2,cur_pathEE);
			if (gtk_dialog_run (GTK_DIALOG (dialog2)) == GTK_RESPONSE_ACCEPT){
			    char *filename2;
			    filename2 = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog2));
				if(cur_pathEE) free(cur_pathEE);
				cur_pathEE = gtk_file_chooser_get_current_folder(dialog2);
				LoadEE(dev,filename2);
				g_free (filename2);
			}
			gtk_widget_destroy(GTK_WIDGET(dialog2));
		}
	}
	gtk_widget_destroy (GTK_WIDGET(dialog));
	progress=0;
}
///
///Choose a file to save and call Save()
void Fsave(GtkWidget *widget,GtkWidget *window)
{
	if(progress) return;
	progress=1;
	GtkFileChooser *dialog;
	dialog = (GtkFileChooser*) gtk_file_chooser_dialog_new (strings[I_Fsave], //"Save File",
						GTK_WINDOW(window),
						GTK_FILE_CHOOSER_ACTION_SAVE,
						strings[I_CANCEL], GTK_RESPONSE_CANCEL,
						strings[I_SAVE], GTK_RESPONSE_ACCEPT,
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
			GtkFileChooser *dialog2;
			dialog2 = (GtkFileChooser*) gtk_file_chooser_dialog_new (strings[S_saveEEfile],
								GTK_WINDOW(window),
								GTK_FILE_CHOOSER_ACTION_SAVE,
								strings[I_CANCEL], GTK_RESPONSE_CANCEL,
								strings[I_SAVE], GTK_RESPONSE_ACCEPT,
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
			gtk_widget_destroy(GTK_WIDGET(dialog2));
		}
	  }
	gtk_widget_destroy (GTK_WIDGET(dialog));
	progress=0;
}
///
///Select data tab
void selectDataTab() { gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0); }
///
///Call device write function
void DevWrite(GtkWidget *widget,GtkWidget *window)
{
#ifndef DEBUG
	if(DeviceDetected!=1) return;
#endif
	selectDataTab();
	gtk_statusbar_push(GTK_STATUSBAR(status_bar),statusID,"");
	RWstop=0;
	getOptions();
	if(!waitingS1&&waitS1){
		waitingS1=1;
		int i,S1=0;
		PrintMessage(strings[I_PRESSS1]); //"press S1 to start"
		for(i=0;!S1&&waitS1&&waitingS1;i++){
			S1=CheckS1();
			msDelay(50);
			PrintMessage(".");
			if(i%64==63) PrintMessage(strings[S_NL]); //"\n"
			while (gtk_events_pending ()) gtk_main_iteration(); //handle UI events, including write button
			msDelay(50);
		}
		PrintMessage(strings[S_NL]); //"\n"
		if(!progress&&S1){
			gtk_widget_set_sensitive(stopBtn,TRUE);
			progress=1;
			Write(dev,ee);	//choose the right function
			progress=0;
			gtk_widget_set_sensitive(stopBtn,FALSE);
		}
		waitingS1=0;
	}
	else if(waitingS1) waitingS1=0;
	else if(!progress){
		gtk_widget_set_sensitive(stopBtn,TRUE);
		progress=1;
		Write(dev,ee);	//choose the right function
		progress=0;
		gtk_widget_set_sensitive(stopBtn,FALSE);
	}
}
///
///Call device read function
void DevRead(GtkWidget *widget,GtkWidget *window)
{
#ifndef DEBUG
	if(DeviceDetected!=1) return;
#endif
	selectDataTab();
	gtk_statusbar_push(GTK_STATUSBAR(status_bar),statusID,"");
	getOptions();
	RWstop=0;
	if(!waitingS1&&waitS1){
		waitingS1=1;
		int i,S1=0;
		PrintMessage(strings[I_PRESSS1]); //"press S1 to start"
		for(i=0;!S1&&waitS1&&waitingS1;i++){
			S1=CheckS1();
			msDelay(50);
			PrintMessage(".");
			if(i%64==63) PrintMessage(strings[S_NL]); //"\n"
			while (gtk_events_pending ()) gtk_main_iteration(); //handle UI events, including write button
			msDelay(50);
		}
		PrintMessage(strings[S_NL]); //"\n"
		if(!progress&&S1){
			gtk_widget_set_sensitive(stopBtn,TRUE);
			progress=1;
			Read(dev,ee,readRes);	//choose the right function
			progress=0;
			gtk_widget_set_sensitive(stopBtn,FALSE);
		}
		waitingS1=0;
	}
	else if(waitingS1) waitingS1=0;
	else if(!progress){
		gtk_widget_set_sensitive(stopBtn,TRUE);
		progress=1;
		Read(dev,ee,readRes);	//choose the right function
		progress=0;
		gtk_widget_set_sensitive(stopBtn,FALSE);
	}
}
///
/// Write fuse low byte at low frequency
void WriteATfuseLowLF(GtkWidget *widget,GtkWidget *window){
#ifndef DEBUG
	if(DeviceDetected!=1) return;
#endif
	if(progress) return;
	getOptions();
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(AVR_FuseLowWrite))){
		progress=1;
		if(AVRfuse<0x100) WriteATfuseSlow(AVRfuse);
		progress=0;
	}
}
///
///Callback function to set available options for each device type
void onDevSel_Changed(GtkWidget *widget,GtkWidget *window)
{
	struct DevInfo info;
	char str2[256],str3[64],strF[32];
	double x;
	GetSelectedDevice();
	if (strlen(dev) == 0) return; // None selected
	info=GetDevInfo(dev);
	sprintf(str, "<b>%s: %s</b>", strings[I_Dev], dev);
	gtk_label_set_markup(GTK_LABEL(devFrame), str);
	devType=info.family;
	gtk_label_set_text(GTK_LABEL(devinfo),info.features);
	if(devType==PIC12||devType==PIC16||devType==PIC18||devType==PIC24){
		gtk_widget_show_all(GTK_WIDGET(devFramePIC));
		gtk_widget_hide(GTK_WIDGET(devFrameAVR));
		gtk_widget_show_all(GTK_WIDGET(EEPROM_RW));
	}
	else if(devType==AVR){	//ATMEL
		gtk_widget_hide(GTK_WIDGET(devFramePIC));
		gtk_widget_show_all(GTK_WIDGET(devFrameAVR));
		gtk_widget_show_all(GTK_WIDGET(EEPROM_RW));
	}
	else{
		gtk_widget_hide(GTK_WIDGET(devFramePIC));
		gtk_widget_hide(GTK_WIDGET(devFrameAVR));
		gtk_widget_hide(GTK_WIDGET(EEPROM_RW));
	}
	if(devType==PIC16)		//ICD
		gtk_widget_show_all(GTK_WIDGET(devFrameICD));
	else gtk_widget_hide(GTK_WIDGET(devFrameICD));
	if(devType==PIC12||devType==PIC16)	//Osc options
		gtk_widget_show_all(GTK_WIDGET(devFrameOsc));
	else gtk_widget_hide(GTK_WIDGET(devFrameOsc));
	if(devType==PIC12||devType==PIC16||devType==PIC18)	//program ID
		gtk_widget_show_all(GTK_WIDGET(Write_ID_BKCal));
	else gtk_widget_hide(GTK_WIDGET(Write_ID_BKCal));
	if(devType==PIC16)	//Program Calib
		gtk_widget_show_all(GTK_WIDGET(WriteCalib12));
	else gtk_widget_hide(GTK_WIDGET(WriteCalib12));
	if(devType==PIC12||devType==PIC16||devType==PIC18){	//Force config
		gtk_widget_show_all(GTK_WIDGET(devFrameConfigW));
		gtk_widget_hide(GTK_WIDGET(devPIC_CW2));
		gtk_widget_hide(GTK_WIDGET(devPIC_CW3));
		gtk_widget_hide(GTK_WIDGET(devPIC_CW4));
		gtk_widget_hide(GTK_WIDGET(devPIC_CW5));
		gtk_widget_hide(GTK_WIDGET(devPIC_CW6));
		gtk_widget_hide(GTK_WIDGET(devPIC_CW7));
		gtk_widget_hide(GTK_WIDGET(devPIC_CW8));
		if(devType==PIC16){
			gtk_widget_show_all(GTK_WIDGET(devPIC_CW2));
		}
		else if(devType==PIC18){
			gtk_widget_show_all(GTK_WIDGET(devPIC_CW2));
			gtk_widget_show_all(GTK_WIDGET(devPIC_CW3));
			gtk_widget_show_all(GTK_WIDGET(devPIC_CW4));
			gtk_widget_show_all(GTK_WIDGET(devPIC_CW5));
			gtk_widget_show_all(GTK_WIDGET(devPIC_CW6));
			gtk_widget_show_all(GTK_WIDGET(devPIC_CW7));
			gtk_widget_show_all(GTK_WIDGET(devPIC_CW8));
		}
	}
	else{
		gtk_widget_hide(GTK_WIDGET(devFrameConfigW));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ConfigForce),FALSE);
	}
	gtk_statusbar_push(GTK_STATUSBAR(status_bar),statusID,dev);
}
/// Walk the TreeModel until we find an entry with the passed device name
/// Select that entry and then stop walking
gboolean selectDev_ForeachFunc(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, char *devNameToSelect) {
	char *thisEntryDevName;
	gtk_tree_model_get(model, iter, DEVICE_NAME_COLUMN, &thisEntryDevName, -1);
	int matched = (strcmp(thisEntryDevName, devNameToSelect) == 0);
	if (matched) gtk_tree_selection_select_iter(devSel, iter);
	g_free(thisEntryDevName);
	return matched;
}
///
/// Comparison function used when sorting the device tree
int sortIterCompareFunc(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer userdata) {
	enum sort_type_t sortcol = GPOINTER_TO_INT(userdata);
	int ret = 0;
	enum sort_data_type_t sortDataType;
	int dataCol;
	switch (sortcol) {
		case SORT_STRING_NAME:
			sortDataType = SDT_STRING;
			dataCol = DEVICE_NAME_COLUMN;
			break;
		case SORT_STRING_GROUP:
			sortDataType = SDT_STRING;
			dataCol = DEVICE_GROUP_COLUMN;
			break;
		default:
			return 0;
	}
	switch (sortDataType) {
		case SDT_STRING:
			{
			char *nameA, *nameB;
			gtk_tree_model_get(model, a, dataCol, &nameA, -1);
			gtk_tree_model_get(model, b, dataCol, &nameB, -1);
			if (nameA == NULL || nameB == NULL) {
				if (nameA == NULL && nameB == NULL) break; // Both null. return 0 (no need to free)
				ret = (nameA == NULL) ? -1 : 1;
			}
			else {
				ret = g_utf8_collate(nameA,nameB);
			}
			g_free(nameA);
			g_free(nameB);
			break;
			}
		default:
			g_return_val_if_reached(0);
	}
	return ret;
}
///
///Add devices to the device ListStore (which may not have been created)
///groupFilter: add devices in this group (-1 for all)
///textFilter: only add devices containing this string (NULL for all)
void AddDevices(enum group_t groupFilter, const char *textFilter) {
	if (GTK_IS_TREE_SELECTION(devSel))
		g_signal_handlers_disconnect_by_func(G_OBJECT(devSel),G_CALLBACK(onDevSel_Changed),NULL);
	if (!GTK_IS_LIST_STORE(devStore)) {
		devStore = gtk_list_store_new (DEVICE_N_COLUMNS,
	  						  G_TYPE_UINT,
                              G_TYPE_STRING,
                              G_TYPE_STRING);
		gtk_tree_view_set_model(GTK_TREE_VIEW(devTree), GTK_TREE_MODEL(devStore));
		gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(devStore), SORT_STRING_NAME,
			sortIterCompareFunc, GINT_TO_POINTER(SORT_STRING_NAME), NULL);
		gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(devStore), SORT_STRING_GROUP,
			sortIterCompareFunc, GINT_TO_POINTER(SORT_STRING_GROUP), NULL);
	 	gtk_tree_view_column_set_sort_column_id(
			gtk_tree_view_get_column(GTK_TREE_VIEW(devTree), 0), SORT_STRING_NAME);
		gtk_tree_view_column_set_sort_column_id(
			gtk_tree_view_get_column(GTK_TREE_VIEW(devTree), 1), SORT_STRING_GROUP);
		g_object_unref (G_OBJECT(devStore));
	}
	else gtk_list_store_clear(devStore);
	int i,j=0;
	char *devices=0,*tok;
	for(i=0;i<NDEVLIST;i++) {
		if(devices) free(devices);
		devices=malloc(strlen(DEVLIST[i].device)+1);
		strcpy(devices,DEVLIST[i].device);
		struct DevInfo info;
		populateDevInfo(&info, &(DEVLIST[i]));
		for(tok=strtok(devices,", \t");tok;tok=strtok(NULL,", \t")) {
			//info.device=malloc(strlen(tok)+1);
			//strcpy(info.device,tok);
			info.group=nameToGroup(tok);
			if(info.group!=-1&&(!textFilter || strlen(textFilter) == 0 || strstr(tok, textFilter)) &&
				(groupFilter == -1 || info.group == groupFilter)) {
					gtk_list_store_insert_with_values(devStore, NULL, -1,
						DEVICE_ID_COLUMN, j++,
						DEVICE_NAME_COLUMN, tok,
						DEVICE_GROUP_COLUMN, groupNames[info.group], -1);
			}
		}
	}
	free(devices);
	if(GTK_IS_TREE_SELECTION(devSel)) {
		gtk_tree_selection_unselect_all(devSel);
	}
	else {
		devSel = gtk_tree_view_get_selection(GTK_TREE_VIEW(devTree));
		gtk_tree_selection_set_mode(devSel, GTK_SELECTION_SINGLE);
	}	
	g_signal_connect(G_OBJECT(devSel),"changed",G_CALLBACK(onDevSel_Changed),NULL);
	gtk_tree_model_foreach(GTK_TREE_MODEL(devStore),
		(GtkTreeModelForeachFunc)selectDev_ForeachFunc,
	 	dev);
}
///
///Filter device list (in gtk_tree) according to type selected
void FilterDevType(GtkWidget *widget,GtkWidget *window)
{
	char *selGroupName = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(devTypeCombo));
	const char *filtText = gtk_entry_get_text(GTK_ENTRY(devFiltEntry));
	enum group_t selGroup = -1;
	for (int i=0; i<NUM_GROUPS; i++) {
		if (strcmp(selGroupName, groupNames[i]) == 0) {
			selGroup = i;
			i = NUM_GROUPS;
		}
	}
	// If no specific group selected, ALL should be selected
	if (selGroup == -1 && strcmp(GROUP_ALL, selGroupName)) {
		PrintMessage1("ERR: group name '%s' invalid", selGroupName);
		return;
	}
	AddDevices(selGroup, filtText);
	g_free(selGroupName);
	onDevSel_Changed(NULL, NULL);
}
///
/// Check or set IO signals according to IO tab controls
void IOchanged(GtkWidget *widget,GtkWidget *window)
{
	if(progress) return;
#ifndef DEBUG
	if(DeviceDetected!=1) return;
#endif
	if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_io_active))) return;
	int i,j=0;
	int trisa=1,trisb=0,trisc=0x30,latac=0,latb=0;
	int port=0,z;
	//char str[128]="IO:";
	char s2[64];
	str[0]=0;
	for(i=0;i<sizeof(ioButtons)/sizeof(ioButtons[0]);i++){
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ioButtons[i].r_0))){
			//strcat(str,"0");
		}
		else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ioButtons[i].r_1))){
			//strcat(str,"1");
			if(i<8) latb|=1<<i;
			else if(i==8) latac|=0x80; //RC7
			else if(i==9) latac|=0x40; //RC6
			else if(i==10) latac|=0x20; //RA5
			else if(i==11) latac|=0x10; //RA4
			else if(i==12) latac|=0x08; //RA3
		}
		else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ioButtons[i].r_I))){
			//strcat(str,"I");
			if(i<8)	trisb|=1<<i;
			else if(i==8) trisc|=0x80; //RC7
			else if(i==9) trisc|=0x40; //RC6
			else if(i==10) trisa|=0x20; //RA5
			else if(i==11) trisa|=0x10; //RA4
			else if(i==12) trisa|=0x8; //RA3
		}
	}
	//sprintf(s2," trisb=%02X latb=%02X trisc=%02X trisa=%02X latac=%02X",trisb,latb,trisc,trisa,latac);
	//strcat(str,s2);
	//gtk_statusbar_push(GTK_STATUSBAR(status_bar),statusID,str);
	bufferU[j++]=READ_RAM;
	bufferU[j++]=0x0F;
	bufferU[j++]=0x80;	//PORTA
	bufferU[j++]=READ_RAM;
	bufferU[j++]=0x0F;
	bufferU[j++]=0x81;	//PORTB
	bufferU[j++]=READ_RAM;
	bufferU[j++]=0x0F;
	bufferU[j++]=0x82;	//PORTC
	bufferU[j++]=WRITE_RAM;
	bufferU[j++]=0x0F;
	bufferU[j++]=0x92;	//TRISA
	bufferU[j++]=trisa;
	bufferU[j++]=WRITE_RAM;
	bufferU[j++]=0x0F;
	bufferU[j++]=0x93;	//TRISB
	bufferU[j++]=trisb;
	bufferU[j++]=WRITE_RAM;
	bufferU[j++]=0x0F;
	bufferU[j++]=0x94;	//TRISC
	bufferU[j++]=trisc;
	bufferU[j++]=EXT_PORT;
	bufferU[j++]=latb;
	bufferU[j++]=latac;
	bufferU[j++]=READ_ADC;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	PacketIO(2);
	for(z=0;z<DIMBUF-3&&bufferI[z]!=READ_RAM;z++);
	port=bufferI[z+3];	//PORTA
	//sprintf(s2," porta=%02X",port);
	//strcat(str,s2);
	//gtk_statusbar_push(GTK_STATUSBAR(status_bar),statusID,str);
	gtk_label_set_text(GTK_LABEL(ioButtons[10].e_I),(port&0x20)?"1":"0");
	gtk_label_set_text(GTK_LABEL(ioButtons[11].e_I),(port&0x10)?"1":"0");
	gtk_label_set_text(GTK_LABEL(ioButtons[12].e_I),(port&0x8)?"1":"0");
	for(z+=4;z<DIMBUF-3&&bufferI[z]!=READ_RAM;z++);
	port=bufferI[z+3];	//PORTB
	for(i=0;i<8;i++) gtk_label_set_text(GTK_LABEL(ioButtons[i].e_I),(port&(1<<i))?"1":"0");
	for(z+=4;z<DIMBUF-3&&bufferI[z]!=READ_RAM;z++);
	port=bufferI[z+3];	//PORTC
	gtk_label_set_text(GTK_LABEL(ioButtons[8].e_I),(port&0x80)?"1":"0");
	gtk_label_set_text(GTK_LABEL(ioButtons[9].e_I),(port&0x40)?"1":"0");
	for(z+=4;z<DIMBUF-2&&bufferI[z]!=READ_ADC;z++);
	double vpp=((bufferI[z+1]<<8)+bufferI[z+2])/1024.0*5*34/12;	//VPP
	sprintf(str,"VPP=%.2fV",vpp);
	gtk_statusbar_push(GTK_STATUSBAR(status_bar),statusID,str);
	return;
}
///
/// Start/stop timer to check for IO status
void IOactive(GtkWidget *widget,GtkWidget *window)
{
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_io_active))&&!icdTimer){
		IOTimer=g_timeout_add(100,(GSourceFunc)IOchanged,NULL);
	}
	else if(IOTimer){
		g_source_remove(IOTimer);
	}
}
///
/// Enable/disable VPP and VCC from IO tab
void VPPVDDactive(GtkWidget *widget,GtkWidget *window)
{
#ifndef DEBUG
	if(DeviceDetected!=1) return;
#endif
	int j=0,vdd_vpp=0;
	char str[16]="";
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(VPP_ON))){
		vdd_vpp+=4;
		strcat(str,"VPP ");
	}
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(VDD_ON))){
		vdd_vpp+=1;
		strcat(str,"VDD ");
	}
	gtk_statusbar_push(GTK_STATUSBAR(status_bar),statusID,str);
	bufferU[j++]=EN_VPP_VCC;		//VDD+VPP
	bufferU[j++]=vdd_vpp;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	PacketIO(2);
}
///
/// Enable/disable DCDC from IO tab or update DCDC voltage
void DCDCactive(GtkWidget *widget,GtkWidget *window)
{
#ifndef DEBUG
	if(DeviceDetected!=1) return;
#endif
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(DCDC_ON))){
		int j=0,vreg=0;
		char str[16];
		double voltage=gtk_range_get_value(GTK_RANGE(DCDC_voltage));
		vreg=voltage*10.0;
		sprintf(str,"DCDC %.1fV",voltage);
		gtk_statusbar_push(GTK_STATUSBAR(status_bar),statusID,str);
		bufferU[j++]=VREG_EN;			//enable HV regulator
		bufferU[j++]=SET_VPP;
		bufferU[j++]=vreg;
		bufferU[j++]=FLUSH;
		for(;j<DIMBUF;j++) bufferU[j]=0x0;
		PacketIO(2);
	}
	else{
		int j=0;
		bufferU[j++]=VREG_DIS;			//disable HV regulator
		bufferU[j++]=FLUSH;
		for(;j<DIMBUF;j++) bufferU[j]=0x0;
		PacketIO(2);
	}
}
///
/// convert hex line
void HexConvert(GtkWidget *widget,GtkWidget *window)
{
	char hex[256],str[256],s2[32];
	int i,address,length,sum=0;
	strncpy(hex,(const char *)gtk_entry_get_text(GTK_ENTRY(Hex_entry)),sizeof(hex));
	if(strlen(hex)>0){
		if(hex[0]==':'&&strlen(hex)>8){
			length=htoi(hex+1,2);
			address=htoi(hex+3,4);
			if(strlen(hex)<11+length*2) gtk_entry_set_text(GTK_ENTRY(Hex_data),"__line too short");
			else{
				for (i=1;i<=length*2+9;i+=2) sum += htoi(hex+i,2);
				if ((sum & 0xff)!=0){
					sprintf(str,"__checksum error, expected 0x%02X",(-sum+htoi(hex+9+length*2,2))&0xFF);
					gtk_entry_set_text(GTK_ENTRY(Hex_data),str);
				}
				else{
					switch(htoi(hex+7,2)){
						case 0:		//Data record
							sprintf(str,"address: 0x%04X ",address);
							if(i&&length) strcat(str,"data: 0x");
							for (i=0;i<length;i++){
								sprintf(s2,"%02X",htoi(hex+9+i*2,2));
								strcat(str,s2);
							}
							gtk_entry_set_text(GTK_ENTRY(Hex_data),str);
							break;
						case 4:		//extended linear address record
							if(strlen(hex)>14){
								sprintf(str,"extended linear address = %04X",htoi(hex+9,4));
								gtk_entry_set_text(GTK_ENTRY(Hex_data),str);
							}
							break;
						default:
							gtk_entry_set_text(GTK_ENTRY(Hex_data),"__unknown record type");
							break;
					}
				}
			}
		}
		else gtk_entry_set_text(GTK_ENTRY(Hex_data),"__invalid line");
	}
	else gtk_entry_set_text(GTK_ENTRY(Hex_entry),"");
}
///
/// convert address & data to hex line
void DataToHexConvert(GtkWidget *widget,GtkWidget *window)
{
	char hex[256],str[256],s2[32];
	int i,address,length,sum=0,x;
	i=sscanf(gtk_entry_get_text(GTK_ENTRY(Address_entry)),"%x",&address);
	if(i!=1) address=0;
	strncpy(hex,(const char *)gtk_entry_get_text(GTK_ENTRY(Data_entry)),sizeof(hex));
	length=strlen(hex);
	length&=0xFF;
	if(length>0){
		sprintf(str,":--%04X00",address&0xFFFF);
		for(i=0;i+1<length;i+=2){
			x=htoi(hex+i,2);
			//x&=0xFF;
			sum+=x;
			sprintf(s2,"%02X",x);
			strcat(str,s2);
		}
		sprintf(s2,"%02X",i/2);
		str[1]=s2[0];
		str[2]=s2[1];
		x=sum;
		sum+=i/2+(address&0xff)+((address>>8)&0xff);
		sprintf(s2,"%02X",(-sum)&0xFF);
		strcat(str,s2);
		gtk_entry_set_text(GTK_ENTRY(Hex_data2),str);
	}
}
///
///Choose a file to save a hex line
void HexSave(GtkWidget *widget,GtkWidget *window)
{
	GtkFileChooser *dialog;
	if(strlen((const char *)gtk_entry_get_text(GTK_ENTRY(Hex_data2)))<11) return;
	dialog = (GtkFileChooser*) gtk_file_chooser_dialog_new (strings[I_Fsave], //"Save File",
						GTK_WINDOW(window),
						GTK_FILE_CHOOSER_ACTION_SAVE,
						strings[I_CANCEL], GTK_RESPONSE_CANCEL,
						strings[I_SAVE], GTK_RESPONSE_ACCEPT,
						NULL);
	if(cur_path) gtk_file_chooser_set_current_folder(dialog,cur_path);
	gtk_file_chooser_set_do_overwrite_confirmation(dialog,TRUE);
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
	  {
	    char *filename;
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		if(cur_path) free(cur_path);
		cur_path = gtk_file_chooser_get_current_folder(dialog);
		FILE* f=fopen(filename,"w");
		if(f){
			fprintf(f,(const char *)gtk_entry_get_text(GTK_ENTRY(Hex_data2)));
			fclose(f);
		}
	    g_free (filename);
	  }
	gtk_widget_destroy (GTK_WIDGET(dialog));
}
///
/// Stop read or write
void Stop(GtkWidget *widget,GtkWidget *window)
{
#ifndef DEBUG
	if(DeviceDetected!=1) return;
#endif
	if(progress==1&&RWstop==0){
		RWstop=1;
		PrintMessage(strings[I_STOP]);
	}
}
///
///Close program
void Xclose(){
//	char *str=gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(devCombo));
//	if(str) strncpy(dev,str,sizeof(dev)-1);
	gtk_main_quit();
}
///
/// Show a message box
void MsgBox(const char* msg)
{
	GtkWidget * dialog = gtk_message_dialog_new (GTK_WINDOW(window),
                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                 GTK_MESSAGE_INFO,
                                 GTK_BUTTONS_OK,
                                 msg);
    gtk_window_set_title(GTK_WINDOW(dialog)," ");
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}
///
/// Find the programmer and setup communication
void Connect(GtkWidget *widget,GtkWidget *window){
	vid=htoi(gtk_entry_get_text(GTK_ENTRY(VID_entry)),4);
	pid=htoi(gtk_entry_get_text(GTK_ENTRY(PID_entry)),4);
	DeviceDetected=FindDevice(vid,pid);	//connect to USB programmer
	if(!DeviceDetected){
		DeviceDetected=FindDevice(new_vid,new_pid);	//try default
		if(DeviceDetected){
			vid=new_vid;
			pid=new_pid;
		}
	}
	if(!DeviceDetected) DeviceDetected=FindDevice(old_vid,old_pid); //try old one
	hvreg=0;
	ProgID();
}
///
/// I2C/SPI receive
void I2cspiR()
{
	//if(DeviceDetected!=1) return;
	gtk_statusbar_push(GTK_STATUSBAR(status_bar),statusID,"");
	saveLog = (int) gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_log));
	int nbyte=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(I2CNbyte));
	if(nbyte<0) nbyte=0;
	if(nbyte>60) nbyte=60;
	int mode=0;
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(I2C16bit))) mode=1;	//I2C mode
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(SPI00))) mode=2;	//SPI mode 00
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(SPI01))) mode=3;	//SPI mode 01
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(SPI10))) mode=4;	//SPI mode 10
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(SPI11))) mode=5;	//SPI mode 11
	char* tok;
    char tokbuf[512];
	BYTE tmpbuf[128];
	int i=0,x;
    strncpy(tokbuf, (const char *)gtk_entry_get_text(GTK_ENTRY(I2CDataSend)), sizeof(tokbuf));
	for(tok=strtok(tokbuf," ");tok&&i<128;tok=strtok(NULL," ")){
		if(sscanf(tok,"%x",&x)){
			tmpbuf[i] = (BYTE)x;
			i++;
		}
	}
	for(;i<128;i++) tmpbuf[i]=0;
	I2CReceive(mode,gtk_combo_box_get_active(GTK_COMBO_BOX(I2CSpeed)),nbyte,tmpbuf);
}
///
/// I2C/SPI send
void I2cspiS()
{
	//if(DeviceDetected!=1) return;
	gtk_statusbar_push(GTK_STATUSBAR(status_bar),statusID,"");
	saveLog = (int) gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_log));
	int nbyte=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(I2CNbyte));
	if(nbyte<0) nbyte=0;
	if(nbyte>57) nbyte=57;
	int mode=0;
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(I2C16bit))) mode=1;	//I2C mode
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(SPI00))) mode=2;	//SPI mode 00
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(SPI01))) mode=3;	//SPI mode 01
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(SPI10))) mode=4;	//SPI mode 10
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(SPI11))) mode=5;	//SPI mode 11
	char* tok;
    char tokbuf[512];
	BYTE tmpbuf[128];
	int i=0,x;
    strncpy(tokbuf, (const char *)gtk_entry_get_text(GTK_ENTRY(I2CDataSend)), sizeof(tokbuf));
	for(tok=strtok(tokbuf," ");tok&&i<128;tok=strtok(NULL," ")){
		if(sscanf(tok,"%x",&x)){
			tmpbuf[i] = (BYTE)x;
			i++;
		}
	}
	for(;i<128;i++) tmpbuf[i]=0;
	I2CSend(mode,gtk_combo_box_get_active(GTK_COMBO_BOX(I2CSpeed)),nbyte,tmpbuf);
}
///
/// send manual command
void CommandIO()
{
	if(DeviceDetected!=1) return;
	gtk_statusbar_push(GTK_STATUSBAR(status_bar),statusID,"");
	saveLog = (int) gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_log));
	char* tok;
    char tokbuf[512],str[16];
	int i=0,x;
    strncpy(tokbuf, (const char *)gtk_entry_get_text(GTK_ENTRY(commandSend)), sizeof(tokbuf));
	for(tok=strtok(tokbuf," ");tok&&i<DIMBUF;tok=strtok(NULL," ")){
		if(sscanf(tok,"%x",&x)){
			bufferU[i] = (BYTE)x;
			i++;
		}
	}
	for(;i<DIMBUF;i++) bufferU[i]=0x0;
	PacketIO(150);
	sprintf(tokbuf,">[");
	for(i=0;i<DIMBUF;i++){
		sprintf(str," %02X",bufferU[i]);
		strcat(tokbuf,str);
		if(i%32==31&&i!=DIMBUF-1)strcat(tokbuf,"\n    ");
	}
	strcat(tokbuf," ]\n<[");
	for(i=0;i<DIMBUF;i++){
		sprintf(str," %02X",bufferI[i]);
		strcat(tokbuf,str);
		if(i%32==31&&i!=DIMBUF-1)strcat(tokbuf,"\n     ");
	}
	strcat(tokbuf," ]");
	PrintMessageCMD(tokbuf);
}
///
///Display contents of EEprom memory
void DisplayEE(){
	char s[256],t[256],v[256],*aux,*g;
	int valid=0,empty=1,lines=0;
	int i,j,max;
	s[0]=0;
	v[0]=0;
	aux=malloc((sizeEE/COL+1)*(16+COL*5));
	aux[0]=0;
	PrintMessage(strings[S_EEMem]);	//"\r\nEEPROM memory:\r\n"
	max=sizeEE>7000?7000:sizeEE;
	for(i=0;i<max;i+=COL){
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
			lines++;
			if(lines>500){	//limit number of lines printed
				strcat(aux,"(...)\r\n");
				i=max-COL*2;
				lines=490;
			}
		}
		s[0]=0;
		v[0]=0;
	}
	if(empty) PrintMessage(strings[S_Empty]);	//empty
	else{
		PrintMessage(aux);
		if(sizeEE>max) PrintMessage("(...)\r\n");
	}
	free(aux);
}
///
///Start HV regulator
int StartHVReg(double V){
	int j=0,z;
	int vreg=(int)(V*10.0);
	if(saveLog&&logfile) fprintf(logfile,"StartHVReg(%.2f)\n",V);
	DWORD t0,t;
	if(V==-1){
		bufferU[j++]=VREG_DIS;			//disable HV regulator
		bufferU[j++]=FLUSH;
		PacketIO(5);
		msDelay(40);
		return -1;
	}
	t=t0=GetTickCount();
	bufferU[j++]=VREG_EN;			//enable HV regulator
	bufferU[j++]=SET_VPP;
	bufferU[j++]=vreg;
	bufferU[j++]=SET_PARAMETER;
	bufferU[j++]=SET_T3;
	bufferU[j++]=2000>>8;
	bufferU[j++]=2000&0xff;
	bufferU[j++]=WAIT_T3;
	bufferU[j++]=READ_ADC;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	PacketIO(5);
	msDelay(20);
	for(z=0;z<DIMBUF-2&&bufferI[z]!=READ_ADC;z++);
	int v=(bufferI[z+1]<<8)+bufferI[z+2];
//	PrintMessage2("v=%d=%fV\n",v,v/G);
	if(v==0){
		PrintMessage(strings[S_lowUsbV]);	//"Tensione USB troppo bassa (VUSB<4.5V)\r\n"
		return 0;
	}
		j=0;
		bufferU[j++]=WAIT_T3;
		bufferU[j++]=READ_ADC;
		bufferU[j++]=FLUSH;
		for(;j<DIMBUF;j++) bufferU[j]=0x0;
	for(;(v<(vreg/10.0-1)*G||v>(vreg/10.0+1)*G)&&t<t0+1500;t=GetTickCount()){
		PacketIO(5);
		msDelay(20);
		for(z=0;z<DIMBUF-2&&bufferI[z]!=READ_ADC;z++);
		v=(bufferI[z+1]<<8)+bufferI[z+2];
		if(HwID==3) v>>=2;		//if 12 bit ADC
//		PrintMessage2("v=%d=%fV\n",v,v/G);
	}
	if(v>(vreg/10.0+1)*G){
		PrintMessage(strings[S_HiVPP]);	//"Attenzione: tensione regolatore troppo alta\r\n\r\n"
		return 0;
	}
	else if(v<(vreg/10.0-1)*G){
		PrintMessage(strings[S_LowVPP]);	//"Attenzione: tensione regolatore troppo bassa\r\n\r\n"
		return 0;
	}
	else if(v==0){
		PrintMessage(strings[S_lowUsbV]);	//"Tensione USB troppo bassa (VUSB<4.5V)\r\n"
		return 0;
	}
	else{
		PrintMessage2(strings[S_reg],t-t0,v/G);	//"Regolatore avviato e funzionante dopo T=%d ms VPP=%.1f\r\n\r\n"
		if(saveLog&&logfile) fprintf(logfile,strings[S_reg],t-t0,v/G);
		return vreg;
	}
}
///
///Read programmer ID
void ProgID()
{
	if(DeviceDetected!=1) return;
	int j=0;
	bufferU[j++]=PROG_RST;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	PacketIO(2);
	for(j=0;j<DIMBUF-7&&bufferI[j]!=PROG_RST;j++);
	PrintMessage3(strings[S_progver],bufferI[j+1],bufferI[j+2],bufferI[j+3]); //"FW versione %d.%d.%d\r\n"
	FWVersion=(bufferI[j+1]<<16)+(bufferI[j+2]<<8)+bufferI[j+3];
	PrintMessage3(strings[S_progid],bufferI[j+4],bufferI[j+5],bufferI[j+6]);	//"ID Hw: %d.%d.%d"
	HwID=bufferI[j+6];
	if(HwID==1) PrintMessage(" (18F2550)\r\n\r\n");
	else if(HwID==2) PrintMessage(" (18F2450)\r\n\r\n");
	else if(HwID==3) PrintMessage(" (18F2458/2553)\r\n\r\n");
	else if(HwID==4) PrintMessage(" (18F25K50)\r\n\r\n");
	else PrintMessage(" (?)\r\n\r\n");
}
///
///Check if a 3.3V regulator is present
int CheckV33Regulator()
{
	int i,j=0;
	if(skipV33check) return 1;
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
	PacketIO(5);
	for(j=0;j<DIMBUF-3&&bufferI[j]!=READ_RAM;j++);
	i=bufferI[j+3]&0x2;		//B1 should be high
	for(j+=3;j<DIMBUF-3&&bufferI[j]!=READ_RAM;j++);
	return (i+(bufferI[j+3]&0x2))==2?1:0;
}
///
///Check if S1 is pressed
int CheckS1()
{
	int i,j=0;
	bufferU[j++]=READ_RAM;
	bufferU[j++]=0x0F;
	bufferU[j++]=0x84;	//READ PORTE
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	PacketIO(5);
	for(j=0;j<DIMBUF-3&&bufferI[j]!=READ_RAM;j++);
	i=bufferI[j+3]&0x8;		//i=E3
	return i?0:1;			//S1 open -> E3=1
}
///
///Execute hardware test
void TestHw(GtkWidget *widget,GtkWindow* parent)
{
#ifndef DEBUG
	if(DeviceDetected!=1) return;
#endif
	char str[256];
	StartHVReg(13);
	int j=0;
	MsgBox(strings[I_TestHW]);		//"Test hardware ..."
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x0;
	bufferU[j++]=EN_VPP_VCC;		//VDD+VPP
	bufferU[j++]=0x5;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	PacketIO(5);
	strcpy(str,strings[I_TestMSG]);
	strcat(str,"\n VDDU=5V\n VPPU=13V\n PGD(RB5)=0V\n PGC(RB6)=0V\n PGM(RB7)=0V");
	MsgBox(str);
	j=0;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x15;
	bufferU[j++]=EN_VPP_VCC;
	bufferU[j++]=0x1;			//VDD
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	PacketIO(5);
	strcpy(str,strings[I_TestMSG]);
	strcat(str,"\n VDDU=5V\n VPPU=0V\n PGD(RB5)=5V\n PGC(RB6)=5V\n PGM(RB7)=5V");
	MsgBox(str);
	j=0;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x1;
	bufferU[j++]=EN_VPP_VCC;
	bufferU[j++]=0x4;			//VPP
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	PacketIO(5);
	strcpy(str,strings[I_TestMSG]);
	strcat(str,"\n VDDU=0V\n VPPU=13V\n PGD(RB5)=5V\n PGC(RB6)=0V\n PGM(RB7)=0V");
	MsgBox(str);
	j=0;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x4;
	bufferU[j++]=EN_VPP_VCC;
	bufferU[j++]=0x0;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	PacketIO(5);
	strcpy(str,strings[I_TestMSG]);
	strcat(str,"\n VDDU=0V\n VPPU=0V\n PGD(RB5)=0V\n PGC(RB6)=5V\n PGM(RB7)=0V");
	MsgBox(str);
	j=0;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x0;
	bufferU[j++]=EN_VPP_VCC;
	bufferU[j++]=0x0;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	PacketIO(5);
	if(FWVersion>=0x900){	//IO test
		int j=0,i,x,r;
		strcpy(str,"0000000000000");
		PrintMessage("IO test\nRC|RA|--RB--|\n");
		for(i=0;i<13;i++){
			x=1<<i;
			j=0;
			bufferU[j++]=EN_VPP_VCC;
			bufferU[j++]=0x0;
			bufferU[j++]=SET_PORT_DIR;
			bufferU[j++]=0x0;	//TRISB
			bufferU[j++]=0x0;	//TRISA-C  (RC7:RC6:RA5:RA4:RA3:X:X:X)
			bufferU[j++]=EXT_PORT;
			bufferU[j++]=x&0xFF;	//PORTB
			bufferU[j++]=(x>>5)&0xFF;	//PORTA-C
			bufferU[j++]=READ_B;
			bufferU[j++]=READ_AC;
			bufferU[j++]=FLUSH;
			for(;j<DIMBUF;j++) bufferU[j]=0x0;
			PacketIO(5);
			for(j=0;j<DIMBUF-1&&bufferI[j]!=READ_B;j++);
			r=bufferI[j+1];
			for(j+=2;j<DIMBUF-1&&bufferI[j]!=READ_AC;j++);
			r+=(bufferI[j+1]&0xF8)<<5;
			for(j=0;j<13;j++) str[12-j]=x&(1<<j)?'1':'0';
			PrintMessage(str);
			PrintMessage1(" (%s)\n",r==x?"OK":strings[S_ErrSing]);
		}
	}
}
///
///Wait for X milliseconds
void msDelay(double delay)
{
#if !defined _WIN32 && !defined __CYGWIN__
	long x=(int)(delay*1000.0);
	usleep(x>MinDly?x:MinDly);
#else
//	Sleep((long)ceil(delay)>MinDly?(long)ceil(delay):MinDly);
	__int64 stop,freq,timeout;
	QueryPerformanceCounter((LARGE_INTEGER *)&stop);
	QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
	timeout=stop+delay*freq/1000.0;
	while(stop<timeout) QueryPerformanceCounter((LARGE_INTEGER *)&stop);
#endif
}
#if !defined _WIN32 && !defined __CYGWIN__	//Linux
///
/// Get system time
DWORD GetTickCount(){
	struct timeb now;
	ftime(&now);
	return now.time*1000+now.millitm;
}
#endif

///
///Write data packet, wait for X milliseconds, read response
///real waiting happens only if the desired delay is greater than 40ms; 
///in case of smaller delays the function simply waits for a response (up to 50ms) 
///
void PacketIO(double delay){
	#define TIMEOUT 50
	if(saveLog&&logfile) fprintf(logfile,"PacketIO(%.2f)\n",delay);
	int delay0=delay;
#if !defined _WIN32 && !defined __CYGWIN__	//Linux
	struct timespec ts;
	uint64_t start,stop;
	fd_set set;
	struct timeval timeout;
	int rv,i;
	FD_ZERO(&set); /* clear the set */
	FD_SET(fd, &set); /* add our file descriptor to the set */
	timeout.tv_sec = 0;
	timeout.tv_usec = TIMEOUT*1000;
	clock_gettime( CLOCK_REALTIME, &ts );
	start=ts.tv_nsec/1000;
	delay-=TIMEOUT-10;	//shorter delays are covered by 50ms timeout
	if(delay<MinDly) delay=MinDly;
	#ifndef hiddevIO	//use raw USB device
	//wait before writing
/*	rv = select(fd + 1, NULL, &set, NULL, &timeout); //wait for write event
	if(rv == -1){
		PrintMessage(strings[S_ErrSing]);	//error
		if(saveLog&&logfile) fprintf(logfile,strings[S_ErrSing]);
		return;
	}
	else if(rv == 0){
		PrintMessage(strings[S_comTimeout]);	//"comm timeout\r\n"
		if(saveLog&&logfile) fprintf(logfile,strings[S_comTimeout]);
		return;
	}*/
	//write
	int res = write(fd,bufferU,DIMBUF);
	if (res < 0) {
		printf("Error: %d\n", errno);
		perror("write");
	}
	usleep((int)(delay*1000.0));
	//wait before reading
	rv = select(fd + 1, &set, NULL, NULL, &timeout); //wait for event
	if(rv == -1){
		PrintMessage(strings[S_ErrSing]);	/*error*/
		if(saveLog&&logfile) fprintf(logfile,strings[S_ErrSing]);
		return;
	}
	else if(rv == 0){
		PrintMessage(strings[S_comTimeout]);	/*"comm timeout\r\n"*/
		if(saveLog&&logfile) fprintf(logfile,strings[S_comTimeout]);
		return;
	}
	//read
	res = read(fd, bufferI, DIMBUF);
	if (res < 0) {
		perror("read");
	}
	#else		//use hiddev device (old method)
	struct hiddev_event ev[80];
	int n=DIMBUF;
	for(i=0;i<DIMBUF;i++) ref_multi_u.values[i]=bufferU[i];
	//write
	ioctl(fd, HIDIOCSUSAGES, &ref_multi_u);
	ioctl(fd,HIDIOCSREPORT, &rep_info_u);
	usleep((int)(delay*1000.0));
	//read
	rv = select(fd + 1, &set, NULL, NULL, &timeout); //wait for event
	if(rv == -1){
		PrintMessage(strings[S_ErrSing]);	/*error*/
		if(saveLog&&logfile) fprintf(logfile,strings[S_ErrSing]);
	}
	else if(rv == 0){
		PrintMessage(strings[S_comTimeout]);	/*"comm timeout\r\n"*/
		if(saveLog&&logfile) fprintf(logfile,strings[S_comTimeout]);
	}
	else{
	//		ioctl(fd, HIDIOCGUSAGES, &ref_multi_i);
	//		ioctl(fd,HIDIOCGREPORT, &rep_info_i);
	#undef read()
		rv=read(fd, ev,sizeof(struct hiddev_event) *n);
		for(i=0;(ev[0].value!=bufferU[0])&&i<40;i++){		//read too early; try again after 5ms
			msDelay(5);
			rv=read(fd, ev,sizeof(struct hiddev_event) *n);
			if(saveLog&&logfile) fprintf(logfile,"Packet not ready, wait extra time\n");
		}
		if(i==40) fprintf(logfile,"Cannot read correct packet!!\n");
		for(i=0;i<n;i++) bufferI[i]=ev[i].value&0xFF;
	}
	#endif
	clock_gettime( CLOCK_REALTIME, &ts );
	stop  = ts.tv_nsec / 1000;
	if(saveLog&&logfile){
		WriteLogIO();
		fprintf(logfile,"T=%.2f ms (%+.2f ms)\n",(stop-start)/1000.0,(stop-start)/1000.0-delay0);
		if(bufferU[0]!=bufferI[0]) fprintf(logfile,"Cannot read correct packet!!\n");
	}
#else	//Windows
	__int64 start,stop,freq,timeout;
	QueryPerformanceCounter((LARGE_INTEGER *)&start);
	QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
	delay-=TIMEOUT-10;	//shorter delays are covered by 50ms timeout
	if(delay<MinDly) delay=MinDly;
	//write
	Result = WriteFile(WriteHandle,bufferU0,DIMBUF+1,&BytesWritten,NULL);
	QueryPerformanceCounter((LARGE_INTEGER *)&stop);
	timeout=stop+delay*freq/1000.0;
	while(stop<timeout) QueryPerformanceCounter((LARGE_INTEGER *)&stop);
	//read
	Result = ReadFile(ReadHandle,bufferI0,DIMBUF+1,&NumberOfBytesRead,(LPOVERLAPPED) &HIDOverlapped);
	Result = WaitForSingleObject(hEventObject,TIMEOUT);
	if(saveLog&&logfile) WriteLogIO();
	ResetEvent(hEventObject);
	if(Result!=WAIT_OBJECT_0){
		PrintMessage(strings[S_comTimeout]);	/*"comm timeout\r\n"*/
		if(saveLog&&logfile) fprintf(logfile,strings[S_comTimeout]);
	}
	QueryPerformanceCounter((LARGE_INTEGER *)&stop);
	if(saveLog&&logfile) fprintf(logfile,"T=%.2f ms (%+.2f ms)\n",(stop-start)*1000.0/freq,(stop-start)*1000.0/freq-delay0);
#endif
}

///-----------------------------------
///Main function
///-----------------------------------
int main( int argc, char *argv[])
{
	//int langID=GetUserDefaultLangID();
	DWORD t0=GetTickCount();
	FILE *f;
	gchar *homedir,*config_dir,*fname=0;
	char lang[32]="";
	int langfile=0;
	homedir = (gchar *) g_get_home_dir ();
	if(homedir){
		config_dir=g_build_path(G_DIR_SEPARATOR_S,homedir,CONFIG_DIR, NULL);
		if(!g_file_test(config_dir,G_FILE_TEST_IS_DIR))
	#if defined _WIN32
		mkdir(config_dir);
	#else
		mkdir(config_dir,0755);
	#endif
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
	vid_ini=vid;
	pid_ini=pid;
	max_err_ini=max_err;
#if defined _WIN32 || defined __CYGWIN__	//Windows
	bufferI=bufferI0+1;
	bufferU=bufferU0+1;
	bufferI0[0]=0;
	bufferU0[0]=0;
#endif
	gtk_init(&argc, &argv);
	unsigned int tmpbuf[128];
	opterr = 0;
	int option_index = 0;
	int help=0,command=0,i,j;
	char c;
	struct option long_options[] =
	{
		{"?",             no_argument,           &help, 1},
		{"h",             no_argument,           &help, 1},
		{"help",          no_argument,           &help, 1},
		{"c",             no_argument,         &command, 1},
		{"command",       no_argument,         &command, 1},
		{"lang",          required_argument,       0, 'l'},
		{"langfile",      no_argument,       &langfile, 1},
		{0, 0, 0, 0}
	};
	while ((j = getopt_long_only (argc, argv, "",long_options,&option_index)) != -1){
		if(j=='l'){ //language
			strncpy(lang,optarg,sizeof(lang)-1);
		}
	}
	for(j=0,i = optind; i < argc&&i<128; i++,j++) sscanf(argv[i], "%x", &tmpbuf[j]);
	for(;j<128;j++) tmpbuf[j]=0;
	strinit();
	char* langid=0;
	i=0;
	if(lang[0]){	//explicit language selection
		if(lang[0]=='i'&&langid[1]=='t'){  //built-in
			strings=strings_it;
			i=1;
		}
		else if(lang[0]=='e'&&lang[1]=='n'){  //built-in
			strings=strings_en;
			i=1;
		}
		else i=strfind(lang,"languages.rc"); //file look-up
	}
	if(i==0){
		#if defined _WIN32
		langid=malloc(19);
		int n=GetLocaleInfo(LOCALE_USER_DEFAULT,LOCALE_SISO639LANGNAME,langid,9);
		langid[n-1] = '-';
		GetLocaleInfo(LOCALE_USER_DEFAULT,LOCALE_SISO3166CTRYNAME,langid+n, 9);
		//printf("%d >%s<\n",n,langid);
		#else
		langid=getenv("LANG");
		#endif
		if(langid){
			if(langid[0]=='i'&&langid[1]=='t') strings=strings_it;
			else if(langid[0]=='e'&&langid[1]=='n') strings=strings_en;
			else if(strfind(langid,"languages.rc")); //first try full code
			else {	//then only first language code
				char* p=strchr(langid,'-');
				if(p) *p=0;
				if(!strfind(langid,"languages.rc")) strings=strings_en;
			}
		}
		else strings=strings_en;
	}
	if(langfile) GenerateLangFile(langid,"languages.rc");
	if(help){
		printf(strings[I_GUI_CMD_HELP]);
		exit(0);
	}
	if(command){
		cmdline=1;
		DeviceDetected=FindDevice(vid,pid);	//connect to USB programmer
		if(!DeviceDetected){
			DeviceDetected=FindDevice(new_vid,new_pid);	//try default
			if(DeviceDetected){
				vid=new_vid;
				pid=new_pid;
			}
		}
		if(!DeviceDetected) DeviceDetected=FindDevice(old_vid,old_pid); //try old one
		if(DeviceDetected){
			bufferU[0]=0;
			for(i=1;i<DIMBUF;i++) bufferU[i]=(char) tmpbuf[i-1];
			PacketIO(100);
			printf("> ");
			for(i=1;i<DIMBUF;i++) printf("%02X ",bufferU[i]);
			printf("\n< ");
			for(i=1;i<DIMBUF;i++) printf("%02X ",bufferI[i]);
			printf("\n");
		}
		else printf(strings[S_noprog]);
		exit(0);
	}

	GtkBuilder *builder = NULL;
	builder=gtk_builder_new_from_resource ("/res/opgui.glade");
	window  = GTK_WIDGET(gtk_builder_get_object (builder,"window"));
	g_signal_connect (window, "destroy", G_CALLBACK (Xclose), NULL);
	gtk_window_set_icon(GTK_WINDOW(window),gdk_pixbuf_new_from_resource("/res/sys.png", NULL));
	GtkCssProvider *cssProvider = gtk_css_provider_new();
	gtk_css_provider_load_from_resource(cssProvider, "/res/style.css");
	gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),GTK_STYLE_PROVIDER(cssProvider),GTK_STYLE_PROVIDER_PRIORITY_USER);
	GtkWidget* w=NULL;
	sprintf(str,"opgui v%s",VERSION);
	gtk_window_set_title(GTK_WINDOW(window),str);
	notebook = GTK_WIDGET(gtk_builder_get_object(builder,"NOTEBOOK"));
//------toolbar-------------
	w=GTK_WIDGET(gtk_builder_get_object(builder,"OPEN_T"));
	gtk_widget_set_tooltip_text(w,strings[I_Fopen]);//"Open File"
	g_signal_connect(w,"clicked",G_CALLBACK(Fopen), NULL);
	w=GTK_WIDGET(gtk_builder_get_object(builder,"SAVE_T"));
	gtk_widget_set_tooltip_text(w,strings[I_Fsave]);//"Save File"
	g_signal_connect(w,"clicked",G_CALLBACK(Fsave), NULL);
	readBtn=GTK_WIDGET(gtk_builder_get_object(builder,"READ_T"));
	gtk_widget_set_tooltip_text(readBtn,strings[I_DevR]);//"Read device"
	g_signal_connect(readBtn,"clicked",G_CALLBACK(DevRead), NULL);
	writeBtn=GTK_WIDGET(gtk_builder_get_object(builder,"WRITE_T"));
	gtk_widget_set_tooltip_text(writeBtn,strings[I_DevW]);//"Write device"
	g_signal_connect(writeBtn,"clicked",G_CALLBACK(DevWrite), NULL);
	stopBtn=GTK_WIDGET(gtk_builder_get_object(builder,"STOP_T"));
	gtk_widget_set_tooltip_text(stopBtn,strings[I_ICD_STOP]);//"Stop"
	g_signal_connect(stopBtn,"clicked",G_CALLBACK(Stop), NULL);
	w=GTK_WIDGET(gtk_builder_get_object(builder,"CONNECT_T"));
	gtk_widget_set_tooltip_text(w,strings[I_CONN]);//"Reconnect"
	g_signal_connect(w,"clicked",G_CALLBACK(Connect), NULL);
	w=GTK_WIDGET(gtk_builder_get_object(builder,"INFO_T"));
	gtk_widget_set_tooltip_text(w,strings[I_Info]);//"Info"
	g_signal_connect(w,"clicked",G_CALLBACK(info), NULL);
//------data tab-------------
	gtk_label_set_text(GTK_LABEL(GTK_WIDGET(gtk_builder_get_object(builder,"DATA_T_L"))),strings[I_Data]);	//"Data"
	data=GTK_WIDGET(gtk_builder_get_object(builder,"DATA"));
	dataBuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data));
//------device tab-------------
	gtk_label_set_text(GTK_LABEL(GTK_WIDGET(gtk_builder_get_object(builder,"DEVICE_L"))),strings[I_Dev]);	//"Device"
	gtk_label_set_text(GTK_LABEL(GTK_WIDGET(gtk_builder_get_object(builder,"TYPE_L"))),strings[I_Type]);	//"Type"
	devTypeCombo=GTK_WIDGET(gtk_builder_get_object(builder,"TYPE_C"));
	g_signal_connect(G_OBJECT(devTypeCombo),"changed",G_CALLBACK(FilterDevType),NULL);
	devFiltEntry=GTK_WIDGET(gtk_builder_get_object(builder,"DEV_SRC_E"));
 	g_signal_connect(G_OBJECT(devFiltEntry),"changed",G_CALLBACK(FilterDevType),NULL);
 	devTree = GTK_WIDGET(gtk_builder_get_object(builder,"DEV_TREE"));
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(devTree),
		-1, strings[I_Dev], gtk_cell_renderer_text_new(), "text", DEVICE_NAME_COLUMN, NULL);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(devTree),
		-1, strings[I_Type], gtk_cell_renderer_text_new(), "text", DEVICE_GROUP_COLUMN, NULL);
	// AddDevices() gets called when an entry in devTypeCombo is selected during init
	devFrame = GTK_WIDGET(gtk_builder_get_object(builder,"DEVICE_NAME"));
	devinfo = GTK_WIDGET(gtk_builder_get_object(builder,"DEV_INFO"));
	EEPROM_RW = GTK_WIDGET(gtk_builder_get_object(builder,"EE_RW"));
	gtk_button_set_label(GTK_BUTTON(EEPROM_RW),strings[I_EE]);	//"Read and write EEPROM"
	ReadReserved = GTK_WIDGET(gtk_builder_get_object(builder,"RES_READ"));
	gtk_button_set_label(GTK_BUTTON(ReadReserved),strings[I_ReadRes]);	//"Read reserved area"
	Write_ID_BKCal = GTK_WIDGET(gtk_builder_get_object(builder,"PROG_ID"));
	gtk_button_set_label(GTK_BUTTON(Write_ID_BKCal),strings[I_ID_BKo_W]);	//"Write ID and BKOscCal"
	WriteCalib12 = GTK_WIDGET(gtk_builder_get_object(builder,"PROG_CAL12"));
	gtk_button_set_label(GTK_BUTTON(WriteCalib12),strings[I_CalW]);	//"Write Calib 1 and 2"
	gtk_label_set_text(GTK_LABEL(GTK_WIDGET(gtk_builder_get_object(builder,"OSCCAL_L"))),strings[I_OSCW]);	//"Write OscCal"
	UseOSCCAL = GTK_WIDGET(gtk_builder_get_object(builder,"OSCCAL"));
	gtk_button_set_label(GTK_BUTTON(UseOSCCAL),strings[I_OSC]);	//"OSCCal"
	UseBKOSCCAL = GTK_WIDGET(gtk_builder_get_object(builder,"BKOSCCAL"));
	gtk_button_set_label(GTK_BUTTON(UseBKOSCCAL),strings[I_BKOSC]);	//"Backup OSCCal"
	UseFileCal = GTK_WIDGET(gtk_builder_get_object(builder,"FILECAL"));
	gtk_button_set_label(GTK_BUTTON(UseFileCal),strings[I_OSCF]);	//"From file"
	ICD_check = GTK_WIDGET(gtk_builder_get_object(builder,"ICD"));
	gtk_button_set_label(GTK_BUTTON(ICD_check),strings[I_ICD_ENABLE]);	//"Enable ICD"
	gtk_label_set_text(GTK_LABEL(GTK_WIDGET(gtk_builder_get_object(builder,"ICD_ADDR_L"))),strings[I_ICD_ADDRESS]);	//"ICD routine address"
	ICD_addr_entry = GTK_WIDGET(gtk_builder_get_object(builder,"ICD_ADDR"));
	ConfigForce = GTK_WIDGET(gtk_builder_get_object(builder,"FORCE_CW"));
	gtk_button_set_label(GTK_BUTTON(ConfigForce),strings[I_PIC_FORCECW]); //"force config word"
	CW1_entry = GTK_WIDGET(gtk_builder_get_object(builder,"CW1"));
	CW2_entry = GTK_WIDGET(gtk_builder_get_object(builder,"CW2"));
	CW3_entry = GTK_WIDGET(gtk_builder_get_object(builder,"CW3"));
	CW4_entry = GTK_WIDGET(gtk_builder_get_object(builder,"CW4"));
	CW5_entry = GTK_WIDGET(gtk_builder_get_object(builder,"CW5"));
	CW6_entry = GTK_WIDGET(gtk_builder_get_object(builder,"CW6"));
	CW7_entry = GTK_WIDGET(gtk_builder_get_object(builder,"CW7"));
	AVR_FuseLow = GTK_WIDGET(gtk_builder_get_object(builder,"FUSEL"));
	AVR_FuseLowWrite = GTK_WIDGET(gtk_builder_get_object(builder,"FUSEL_C"));
	gtk_button_set_label(GTK_BUTTON(AVR_FuseLowWrite),strings[I_AT_FUSE]);	//"Write Fuse Low"
	AVR_FuseHigh = GTK_WIDGET(gtk_builder_get_object(builder,"FUSEH"));
	AVR_FuseHighWrite = GTK_WIDGET(gtk_builder_get_object(builder,"FUSEH_C"));
	gtk_button_set_label(GTK_BUTTON(AVR_FuseHighWrite),strings[I_AT_FUSEH]);	//"Write Fuse High"
	AVR_FuseExt = GTK_WIDGET(gtk_builder_get_object(builder,"FUSEX"));
	AVR_FuseExtWrite = GTK_WIDGET(gtk_builder_get_object(builder,"FUSEX_C"));
	gtk_button_set_label(GTK_BUTTON(AVR_FuseExtWrite),strings[I_AT_FUSEX]);	//"Write Extended Fuse"
	AVR_Lock = GTK_WIDGET(gtk_builder_get_object(builder,"FUSELCK"));
	AVR_LockWrite = GTK_WIDGET(gtk_builder_get_object(builder,"FUSELCK_C"));
	gtk_button_set_label(GTK_BUTTON(AVR_LockWrite),strings[I_AT_LOCK]);	//"Write Lock"
	b_WfuseLF = GTK_WIDGET(gtk_builder_get_object(builder,"FUSEL_W3K"));
	gtk_button_set_label(GTK_BUTTON(b_WfuseLF),strings[I_AT_FUSELF]);		//"Write Fuse Low @3kHz"
	g_signal_connect(G_OBJECT(b_WfuseLF),"clicked",G_CALLBACK(WriteATfuseLowLF),window);
	devFramePIC = GTK_WIDGET(gtk_builder_get_object(builder,"PIC_OPT"));
	devFrameAVR = GTK_WIDGET(gtk_builder_get_object(builder,"AVR_OPT"));
	devFrameConfigW = GTK_WIDGET(gtk_builder_get_object(builder,"CW_OPT"));
	devFrameICD = GTK_WIDGET(gtk_builder_get_object(builder,"ICD_OPT"));
	devFrameOsc = GTK_WIDGET(gtk_builder_get_object(builder,"OSC_OPT"));
	devPIC_CW1 = GTK_WIDGET(gtk_builder_get_object(builder,"CW1_OPT"));
	devPIC_CW2 = GTK_WIDGET(gtk_builder_get_object(builder,"CW2_OPT"));
	devPIC_CW3 = GTK_WIDGET(gtk_builder_get_object(builder,"CW3_OPT"));
	devPIC_CW4 = GTK_WIDGET(gtk_builder_get_object(builder,"CW4_OPT"));
	devPIC_CW5 = GTK_WIDGET(gtk_builder_get_object(builder,"CW5_OPT"));
	devPIC_CW6 = GTK_WIDGET(gtk_builder_get_object(builder,"CW6_OPT"));
	devPIC_CW7 = GTK_WIDGET(gtk_builder_get_object(builder,"CW7_OPT"));
	devPIC_CW8 = GTK_WIDGET(gtk_builder_get_object(builder,"CW8_OPT"));
	gtk_widget_hide(GTK_WIDGET(devFrameAVR));
	gtk_widget_hide(GTK_WIDGET(devFramePIC));
//------options tab-------------
	gtk_label_set_text(GTK_LABEL(GTK_WIDGET(gtk_builder_get_object(builder,"OPTIONS_L"))),strings[I_Opt]);	//"Options"
	VID_entry=GTK_WIDGET(gtk_builder_get_object(builder,"VID"));
	PID_entry=GTK_WIDGET(gtk_builder_get_object(builder,"PID"));
	Errors_entry=GTK_WIDGET(gtk_builder_get_object(builder,"MAXERR"));
	b_connect = GTK_WIDGET(gtk_builder_get_object(builder,"CONNECT"));
	gtk_button_set_label(GTK_BUTTON(b_connect),strings[I_CONN]);	//"Reconnect"
	g_signal_connect(G_OBJECT(b_connect),"clicked",G_CALLBACK(Connect),window);
	b_testhw = GTK_WIDGET(gtk_builder_get_object(builder,"TEST"));
	gtk_button_set_label(GTK_BUTTON(b_testhw),strings[I_TestHWB]);	//"Hardware test"
	g_signal_connect(G_OBJECT(b_testhw),"clicked",G_CALLBACK(TestHw),window);
	b_log = GTK_WIDGET(gtk_builder_get_object(builder,"LOG"));
	gtk_button_set_label(GTK_BUTTON(b_log),strings[I_LOG]);	//"Log activity"
	b_V33check = GTK_WIDGET(gtk_builder_get_object(builder,"3VCHECK"));
	gtk_button_set_label(GTK_BUTTON(b_V33check),strings[I_CK_V33]);	//"Don't check for 3.3V regulator"
	b_WaitS1 = GTK_WIDGET(gtk_builder_get_object(builder,"S1"));
	gtk_button_set_label(GTK_BUTTON(b_WaitS1),strings[I_WAITS1]);	//"Wait for S1 before read/write"
	gtk_label_set_text(GTK_LABEL(GTK_WIDGET(gtk_builder_get_object(builder,"MAXERR_L"))),strings[I_MAXERR]);	//"Max errors"
//------I2C tab-------------
	gtk_label_set_text(GTK_LABEL(GTK_WIDGET(gtk_builder_get_object(builder,"I2CMODE"))),strings[I_I2CMode]);	//"Mode"
	I2C8bit = GTK_WIDGET(gtk_builder_get_object(builder,"I2C8BIT"));
	I2C16bit = GTK_WIDGET(gtk_builder_get_object(builder,"I2C16BIT"));
	SPI00 = GTK_WIDGET(gtk_builder_get_object(builder,"SPI00"));
	SPI01 = GTK_WIDGET(gtk_builder_get_object(builder,"SPI01"));
	SPI10 = GTK_WIDGET(gtk_builder_get_object(builder,"SPI10"));
	SPI11 = GTK_WIDGET(gtk_builder_get_object(builder,"SPI11"));
	gtk_label_set_text(GTK_LABEL(GTK_WIDGET(gtk_builder_get_object(builder,"DATASEND_L"))),strings[I_I2CDATAOUT]);	//"Data to send"
	I2CDataSend = GTK_WIDGET(gtk_builder_get_object(builder,"DATASEND"));
	gtk_label_set_text(GTK_LABEL(GTK_WIDGET(gtk_builder_get_object(builder,"DATATR_L"))),strings[I_I2CDATATR]);	//"Data transferred"
	I2CDataReceive = GTK_WIDGET(gtk_builder_get_object(builder,"DATATR"));
	gtk_label_set_text(GTK_LABEL(GTK_WIDGET(gtk_builder_get_object(builder,"NBYTE_L"))),strings[I_I2C_NB]);	//"Byes to read/write"
	I2CNbyte = GTK_WIDGET(gtk_builder_get_object(builder,"NBYTE_S"));
	gtk_label_set_text(GTK_LABEL(GTK_WIDGET(gtk_builder_get_object(builder,"SPEED_L"))),strings[I_Speed]);	//"Speed"
	I2CSpeed = GTK_WIDGET(gtk_builder_get_object(builder,"SPEED_C"));
	I2CSendBtn = GTK_WIDGET(gtk_builder_get_object(builder,"SEND_B"));
	gtk_button_set_label(GTK_BUTTON(I2CSendBtn),strings[I_I2CSend]);	//"Send"
	g_signal_connect(G_OBJECT(I2CSendBtn),"clicked",G_CALLBACK(I2cspiS),window);
	I2CReceiveBtn = GTK_WIDGET(gtk_builder_get_object(builder,"RECEIVE_B"));
	gtk_button_set_label(GTK_BUTTON(I2CReceiveBtn),strings[I_I2CReceive]);	//"Receive"
	g_signal_connect(G_OBJECT(I2CReceiveBtn),"clicked",G_CALLBACK(I2cspiR),window);
//------ICD tab-------------
	GtkWidget* loadCoffBtn = GTK_WIDGET(gtk_builder_get_object(builder,"LOADCOFF_B"));
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(loadCoffBtn),strings[I_LOAD_COFF]); //"load COFF file..."
	g_signal_connect(G_OBJECT(loadCoffBtn),"clicked",G_CALLBACK(loadCoff),window);
	g_signal_connect(GTK_WIDGET(gtk_builder_get_object(builder,"ICD_TAB")),"key_press_event",G_CALLBACK(icd_key_event),NULL);
	//menu
	gtk_menu_item_set_label(GTK_MENU_ITEM(gtk_builder_get_object(builder,"OPTIONS_M")),strings[I_Opt]); //"Options";
	icdMenuPC = GTK_WIDGET(gtk_builder_get_object(builder,"PCOUNT_M"));
	gtk_menu_item_set_label(GTK_MENU_ITEM(icdMenuPC),strings[I_SHOW_PC]); //"show Program Counter"
	icdMenuSTAT = GTK_WIDGET(gtk_builder_get_object(builder,"STATUS_M"));
	gtk_menu_item_set_label(GTK_MENU_ITEM(icdMenuSTAT),strings[I_SHOW_STATUS]); //"show status registers"
	icdMenuBank0 = GTK_WIDGET(gtk_builder_get_object(builder,"BANK0_M"));
	gtk_menu_item_set_label(GTK_MENU_ITEM(icdMenuBank0),strings[I_SHOW_BANK0]); //"show memory bank 0"
	icdMenuBank1 = GTK_WIDGET(gtk_builder_get_object(builder,"BANK1_M"));
	gtk_menu_item_set_label(GTK_MENU_ITEM(icdMenuBank1),strings[I_SHOW_BANK1]); //"show memory bank 1"
	icdMenuBank2 = GTK_WIDGET(gtk_builder_get_object(builder,"BANK2_M"));
	gtk_menu_item_set_label(GTK_MENU_ITEM(icdMenuBank2),strings[I_SHOW_BANK2]); //"show memory bank 2"
	icdMenuBank3 = GTK_WIDGET(gtk_builder_get_object(builder,"BANK3_M"));
	gtk_menu_item_set_label(GTK_MENU_ITEM(icdMenuBank3),strings[I_SHOW_BANK3]); //"show memory bank 3"
	icdMenuEE = GTK_WIDGET(gtk_builder_get_object(builder,"EE_M"));
	gtk_menu_item_set_label(GTK_MENU_ITEM(icdMenuEE),strings[I_SHOW_EE]); //"show EEPROM"
	//toolbar
	w=GTK_WIDGET(gtk_builder_get_object(builder,"ICD_RUN"));
	gtk_widget_set_tooltip_text(w,strings[I_ICD_RUN]);
	g_signal_connect(w,"clicked",G_CALLBACK(icdRun), NULL);
	w=GTK_WIDGET(gtk_builder_get_object(builder,"ICD_HALT"));
	gtk_widget_set_tooltip_text(w,strings[I_ICD_HALT]);
	g_signal_connect(w,"clicked",G_CALLBACK(icdHalt), NULL);
	w=GTK_WIDGET(gtk_builder_get_object(builder,"ICD_STEP"));
	gtk_widget_set_tooltip_text(w,strings[I_ICD_STEP]);
	g_signal_connect(w,"clicked",G_CALLBACK(icdStep), NULL);
	w=GTK_WIDGET(gtk_builder_get_object(builder,"ICD_STEPOVER"));
	gtk_widget_set_tooltip_text(w,strings[I_ICD_STEPOVER]);
	g_signal_connect(w,"clicked",G_CALLBACK(icdStepOver), NULL);
	w=GTK_WIDGET(gtk_builder_get_object(builder,"ICD_STOP"));
	gtk_widget_set_tooltip_text(w,strings[I_ICD_STOP]);
	g_signal_connect(w,"clicked",G_CALLBACK(icdStop), NULL);
	w=GTK_WIDGET(gtk_builder_get_object(builder,"ICD_REFRESH"));
	gtk_widget_set_tooltip_text(w,strings[I_ICD_REFRESH]);//"refresh"
	g_signal_connect(w,"clicked",G_CALLBACK(icdRefresh), NULL);
	icdCommand = GTK_WIDGET(gtk_builder_get_object(builder,"ICD_CMD_E"));
	gtk_widget_set_tooltip_text(icdCommand,strings[I_ICD_CMD]);//"command-line"
	g_signal_connect(G_OBJECT(icdCommand),"key_press_event",G_CALLBACK(icdCommand_key_event),NULL);
	w=GTK_WIDGET(gtk_builder_get_object(builder,"ICD_HELP"));
	gtk_widget_set_tooltip_text(w,strings[I_ICD_HELP]);//"help"
	g_signal_connect(w,"clicked",G_CALLBACK(ICDHelp), NULL);
	//source
	gtk_label_set_text(GTK_LABEL(GTK_WIDGET(gtk_builder_get_object(builder,"ICD_SOURCE_L"))),strings[I_ICD_SOURCE]);	//"Source"
	sourceTxt = GTK_WIDGET(gtk_builder_get_object(builder,"ICD_SOURCE"));
	sourceBuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(sourceTxt));
	g_signal_connect(G_OBJECT(sourceTxt),"button_press_event",G_CALLBACK(source_mouse_event),NULL);
	//status
	gtk_label_set_text(GTK_LABEL(GTK_WIDGET(gtk_builder_get_object(builder,"ICD_STAT_L"))),strings[I_ICD_STATUS]);	//"Status"
	statusTxt = GTK_WIDGET(gtk_builder_get_object(builder,"ICD_STATUS"));
	statusBuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(statusTxt));
	g_signal_connect(G_OBJECT(statusTxt),"button_press_event",G_CALLBACK(icdStatus_mouse_event),NULL);
//------IO tab-------------
	b_io_active = GTK_WIDGET(gtk_builder_get_object(builder,"IOEN"));
	gtk_button_set_label(GTK_BUTTON(b_io_active),strings[I_IO_Enable]);	//"Enable IO"
	g_signal_connect(G_OBJECT(b_io_active),"toggled",G_CALLBACK(IOactive),NULL);
	ioButtons[0].r_0 = GTK_WIDGET(gtk_builder_get_object(builder,"RB0_0"));
	ioButtons[0].r_1 = GTK_WIDGET(gtk_builder_get_object(builder,"RB0_1"));
	ioButtons[0].r_I = GTK_WIDGET(gtk_builder_get_object(builder,"RB0_I"));
	ioButtons[0].e_I = GTK_WIDGET(gtk_builder_get_object(builder,"RB0_L"));
	ioButtons[1].r_0 = GTK_WIDGET(gtk_builder_get_object(builder,"RB1_0"));
	ioButtons[1].r_1 = GTK_WIDGET(gtk_builder_get_object(builder,"RB1_1"));
	ioButtons[1].r_I = GTK_WIDGET(gtk_builder_get_object(builder,"RB1_I"));
	ioButtons[1].e_I = GTK_WIDGET(gtk_builder_get_object(builder,"RB1_L"));
	ioButtons[2].r_0 = GTK_WIDGET(gtk_builder_get_object(builder,"RB2_0"));
	ioButtons[2].r_1 = GTK_WIDGET(gtk_builder_get_object(builder,"RB2_1"));
	ioButtons[2].r_I = GTK_WIDGET(gtk_builder_get_object(builder,"RB2_I"));
	ioButtons[2].e_I = GTK_WIDGET(gtk_builder_get_object(builder,"RB2_L"));
	ioButtons[3].r_0 = GTK_WIDGET(gtk_builder_get_object(builder,"RB3_0"));
	ioButtons[3].r_1 = GTK_WIDGET(gtk_builder_get_object(builder,"RB3_1"));
	ioButtons[3].r_I = GTK_WIDGET(gtk_builder_get_object(builder,"RB3_I"));
	ioButtons[3].e_I = GTK_WIDGET(gtk_builder_get_object(builder,"RB3_L"));
	ioButtons[4].r_0 = GTK_WIDGET(gtk_builder_get_object(builder,"RB4_0"));
	ioButtons[4].r_1 = GTK_WIDGET(gtk_builder_get_object(builder,"RB4_1"));
	ioButtons[4].r_I = GTK_WIDGET(gtk_builder_get_object(builder,"RB4_I"));
	ioButtons[4].e_I = GTK_WIDGET(gtk_builder_get_object(builder,"RB4_L"));
	ioButtons[5].r_0 = GTK_WIDGET(gtk_builder_get_object(builder,"RB5_0"));
	ioButtons[5].r_1 = GTK_WIDGET(gtk_builder_get_object(builder,"RB5_1"));
	ioButtons[5].r_I = GTK_WIDGET(gtk_builder_get_object(builder,"RB5_I"));
	ioButtons[5].e_I = GTK_WIDGET(gtk_builder_get_object(builder,"RB5_L"));
	ioButtons[6].r_0 = GTK_WIDGET(gtk_builder_get_object(builder,"RB6_0"));
	ioButtons[6].r_1 = GTK_WIDGET(gtk_builder_get_object(builder,"RB6_1"));
	ioButtons[6].r_I = GTK_WIDGET(gtk_builder_get_object(builder,"RB6_I"));
	ioButtons[6].e_I = GTK_WIDGET(gtk_builder_get_object(builder,"RB6_L"));
	ioButtons[7].r_0 = GTK_WIDGET(gtk_builder_get_object(builder,"RB7_0"));
	ioButtons[7].r_1 = GTK_WIDGET(gtk_builder_get_object(builder,"RB7_1"));
	ioButtons[7].r_I = GTK_WIDGET(gtk_builder_get_object(builder,"RB7_I"));
	ioButtons[7].e_I = GTK_WIDGET(gtk_builder_get_object(builder,"RB7_L"));
	ioButtons[8].r_0 = GTK_WIDGET(gtk_builder_get_object(builder,"RC7_0"));
	ioButtons[8].r_1 = GTK_WIDGET(gtk_builder_get_object(builder,"RC7_1"));
	ioButtons[8].r_I = GTK_WIDGET(gtk_builder_get_object(builder,"RC7_I"));
	ioButtons[8].e_I = GTK_WIDGET(gtk_builder_get_object(builder,"RC7_L"));
	ioButtons[9].r_0 = GTK_WIDGET(gtk_builder_get_object(builder,"RC6_0"));
	ioButtons[9].r_1 = GTK_WIDGET(gtk_builder_get_object(builder,"RC6_1"));
	ioButtons[9].r_I = GTK_WIDGET(gtk_builder_get_object(builder,"RC6_I"));
	ioButtons[9].e_I = GTK_WIDGET(gtk_builder_get_object(builder,"RC6_L"));
	ioButtons[10].r_0 = GTK_WIDGET(gtk_builder_get_object(builder,"RA5_0"));
	ioButtons[10].r_1 = GTK_WIDGET(gtk_builder_get_object(builder,"RA5_1"));
	ioButtons[10].r_I = GTK_WIDGET(gtk_builder_get_object(builder,"RA5_I"));
	ioButtons[10].e_I = GTK_WIDGET(gtk_builder_get_object(builder,"RA5_L"));
	ioButtons[11].r_0 = GTK_WIDGET(gtk_builder_get_object(builder,"RA4_0"));
	ioButtons[11].r_1 = GTK_WIDGET(gtk_builder_get_object(builder,"RA4_1"));
	ioButtons[11].r_I = GTK_WIDGET(gtk_builder_get_object(builder,"RA4_I"));
	ioButtons[11].e_I = GTK_WIDGET(gtk_builder_get_object(builder,"RA4_L"));
	ioButtons[12].r_0 = GTK_WIDGET(gtk_builder_get_object(builder,"RA3_0"));
	ioButtons[12].r_1 = GTK_WIDGET(gtk_builder_get_object(builder,"RA3_1"));
	ioButtons[12].r_I = GTK_WIDGET(gtk_builder_get_object(builder,"RA3_I"));
	ioButtons[12].e_I = GTK_WIDGET(gtk_builder_get_object(builder,"RA3_L"));
	for(int ii=0;ii<sizeof(ioButtons)/sizeof(ioButtons[0]);ii++){
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ioButtons[ii].r_I),TRUE);
		g_signal_connect(G_OBJECT(ioButtons[ii].r_0),"toggled",G_CALLBACK(IOchanged),NULL);
		g_signal_connect(G_OBJECT(ioButtons[ii].r_1),"toggled",G_CALLBACK(IOchanged),NULL);
		g_signal_connect(G_OBJECT(ioButtons[ii].r_I),"toggled",G_CALLBACK(IOchanged),NULL);
	}
	VDD_ON = GTK_WIDGET(gtk_builder_get_object(builder,"VDDUEN"));
	VPP_ON = GTK_WIDGET(gtk_builder_get_object(builder,"VPPUEN"));
	DCDC_ON = GTK_WIDGET(gtk_builder_get_object(builder,"DCDCEN"));
	DCDC_voltage = GTK_WIDGET(gtk_builder_get_object(builder,"DCDC_S"));
	gtk_label_set_text(GTK_LABEL(GTK_WIDGET(gtk_builder_get_object(builder,"CMD_L"))),strings[I_IO_Commands]);	//"Manual commands"
	gtk_label_set_text(GTK_LABEL(GTK_WIDGET(gtk_builder_get_object(builder,"CMDSEND_L"))),strings[I_I2CDATAOUT]);	//"Data to send"
	commandSend = GTK_WIDGET(gtk_builder_get_object(builder,"CMDSEND"));
	gtk_label_set_text(GTK_LABEL(GTK_WIDGET(gtk_builder_get_object(builder,"CMDTR_L"))),strings[I_I2CDATATR]);	//"Data transferred"
	commandTransfer = GTK_WIDGET(gtk_builder_get_object(builder,"CMDTR"));
	w = GTK_WIDGET(gtk_builder_get_object(builder,"CMDTR_B"));
	gtk_button_set_label(GTK_BUTTON(w),strings[I_I2CSend]);	//"Send"
	g_signal_connect(w,"clicked",G_CALLBACK(CommandIO),window);
	g_signal_connect(G_OBJECT(VDD_ON),"toggled",G_CALLBACK(VPPVDDactive),NULL);
	g_signal_connect(G_OBJECT(VPP_ON),"toggled",G_CALLBACK(VPPVDDactive),NULL);
	g_signal_connect(G_OBJECT(DCDC_ON),"toggled",G_CALLBACK(DCDCactive),NULL);
	g_signal_connect(G_OBJECT(DCDC_voltage),"value_changed",G_CALLBACK(DCDCactive),NULL);
//------Utility tab-------------
	Hex_entry = GTK_WIDGET(gtk_builder_get_object(builder,"HEXIN"));
	Hex_data = GTK_WIDGET(gtk_builder_get_object(builder,"DATAOUT"));
	Address_entry = GTK_WIDGET(gtk_builder_get_object(builder,"ADDRIN"));
	Data_entry = GTK_WIDGET(gtk_builder_get_object(builder,"DATAIN"));
	Hex_data2 = GTK_WIDGET(gtk_builder_get_object(builder,"HEXOUT"));
	w = GTK_WIDGET(gtk_builder_get_object(builder,"HEXSAVE"));
	gtk_button_set_label(GTK_BUTTON(w),strings[I_Fsave]);
	g_signal_connect(w,"clicked",G_CALLBACK(HexSave),window);
	g_signal_connect(G_OBJECT(Hex_entry),"changed",G_CALLBACK(HexConvert),NULL);
	g_signal_connect(G_OBJECT(Address_entry),"changed",G_CALLBACK(DataToHexConvert),NULL);
	g_signal_connect(G_OBJECT(Data_entry),"changed",G_CALLBACK(DataToHexConvert),NULL);
//------status bar-------------
	status_bar = GTK_WIDGET(gtk_builder_get_object(builder,"STATUS_B"));
	statusID=gtk_statusbar_get_context_id(GTK_STATUSBAR(status_bar),"ID");
	//printf("load time: %dms\n",GetTickCount()-t0);fflush(stdout);
	gtk_widget_show_all(window);
	
//********Init*************
	char text[16];
	sprintf(text,"%04X",vid);
	gtk_entry_set_text(GTK_ENTRY(VID_entry),text);
	sprintf(text,"%04X",pid);
	gtk_entry_set_text(GTK_ENTRY(PID_entry),text);
	sprintf(text,"%d",max_err);
	gtk_entry_set_text(GTK_ENTRY(Errors_entry),text);
	sizeW=0x8400;
	memCODE_W=malloc(sizeW*sizeof(WORD));
	initVar();
	for(i=0;i<0x8400;i++) memCODE_W[i]=0x3fff;
	strncpy(LogFileName,strings[S_LogFile],sizeof(LogFileName));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(devTypeCombo), GROUP_ALL, GROUP_ALL);
	for (int i=0;i<NUM_GROUPS;i++)
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(devTypeCombo), groupNames[i], groupNames[i]);
	// These will trigger AddDevices to populate the device tree
	if (strlen(dev)>0) {
		struct DevInfo info = GetDevInfo(dev);
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(devTypeCombo), groupNames[info.group]);
	}
	else {
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(devTypeCombo), GROUP_ALL);
	}
	DeviceDetected=FindDevice(vid,pid);	//connect to USB programmer
	if(!DeviceDetected){
		DeviceDetected=FindDevice(new_vid,new_pid);	//try default
		if(DeviceDetected){
			vid=new_vid;
			pid=new_pid;
		}
	}
	if(!DeviceDetected) DeviceDetected=FindDevice(old_vid,old_pid); //try old one
	ProgID();		//get firmware version and reset
	gtk_main();
//	printf(ListDevices());
//******Save ini file******
// only if parameters are changed
	if(strcmp(dev_ini,dev)||vid_ini!=vid||pid_ini!=pid||max_err_ini!=max_err){
		if(homedir){
			f=fopen(fname,"w");
			if(f){
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

///
///Find the USB peripheral with proper vid&pid code
/// return 0 if not found
int FindDevice(int vid,int pid){
	int MyDeviceDetected = FALSE;
#if !defined _WIN32 && !defined __CYGWIN__	//Linux
	#ifndef hiddevIO	//use raw USB device
	struct hidraw_devinfo device_info;
	int i=-1;
	if(path[0]==0){	//search all devices
		if((fd = open("/dev/openprogrammer", O_RDWR|O_NONBLOCK))>0){ //try with this first
			sprintf(path,"/dev/openprogrammer");
			ioctl(fd, HIDIOCGRAWINFO, &device_info);
			if(device_info.vendor==vid&&device_info.product==pid) i=0;
			else{
				close(fd);
				i=-1;
			}
		}
		if(i){
			for(i=0;i<16;i++){
				sprintf(path,"/dev/hidraw%d",i);
				if((fd = open(path, O_RDWR|O_NONBLOCK))>0){
					ioctl(fd, HIDIOCGRAWINFO, &device_info);
					if(device_info.vendor==vid&&device_info.product==pid) break;
					else close(fd);
				}
			}
		}
		if(i==16){
			PrintMessage(strings[S_noprog]);
			path[0]=0;
			return 0;
		}
	}
	else{	//user supplied path
		if((fd = open(path, O_RDWR|O_NONBLOCK)) < 0) {
			PrintMessage1(strings[S_DevPermission],path); //"cannot open %s, make sure you have read permission on it",path);
			return 0;
		}
		ioctl(fd, HIDIOCGRAWINFO, &device_info);
		if(device_info.vendor!=vid||device_info.product!=pid){
			PrintMessage(strings[S_noprog]);
			return 0;
		}
	}
	printf(strings[S_progDev],path);
	return 1;
	#else		//use hiddev device (old method)
	struct hiddev_devinfo device_info;
	int i=-1;
	if(path[0]==0){	//search all devices
		if((fd = open("/dev/openprogrammer", O_RDONLY ))>0){ //try with this first
			ioctl(fd, HIDIOCGDEVINFO, &device_info);
			if(device_info.vendor==vid&&device_info.product==pid) i=0;
			else{
				close(fd);
				i=-1;
			}
		}
		if(i){
		for(i=0;i<16;i++){
			sprintf(path,"/dev/usb/hiddev%d",i);
			if((fd = open(path, O_RDONLY ))>0){
				ioctl(fd, HIDIOCGDEVINFO, &device_info);
				if(device_info.vendor==vid&&device_info.product==pid) break;
				else close(fd);
				}
			}
		}
		if(i==16){
			PrintMessage(strings[S_noprog]);
			path[0]=0;
			return 0;
		}
	}
	else{	//user supplied path
		if ((fd = open(path, O_RDONLY )) < 0) {
			PrintMessage1(strings[S_DevPermission],path); //"cannot open %s, make sure you have read permission on it",path);
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
	#endif
#else		//Windows
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
		//gtk_statusbar_push(status_bar,statusID,strings[S_noprog]);
	}
	else{
		PrintMessage(strings[S_prog]);	//"Programmer detected\r\n");
		PrintMessage2("VID=0x%04X PID=0x%04X\r\n",vid,pid);
		//gtk_statusbar_push(status_bar,statusID,strings[S_prog]);
	}
	return MyDeviceDetected;
}