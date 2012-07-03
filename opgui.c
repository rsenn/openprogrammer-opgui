/*
 * opgui.c - control program for the open programmer
 * Copyright (C) 2009-2011 Alberto Maccioni
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
//#define DEBUG

#include "common.h"
#include "I2CSPI.h"
#include "coff.h"
#include "icd.h"
#include "deviceRW.h"
#include "fileIO.h"
#include "icons.h"

#define MAXLINES 500
#define  CONFIG_FILE "opgui.ini"
#define  CONFIG_DIR ".opgui"

void Connect();
void I2cspiR();
void I2cspiS();
void ProgID();
void PrintMessageI2C(const char *msg);
void ShowContext();
int FindDevice();
void TestHw();

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
char CoffFileName[512]="";
int vid=0x04D8,pid=0x0100;
WORD *memCODE_W=0;
int size=0,sizeW=0,sizeEE=0,sizeCONFIG=0;
unsigned char *memCODE=0,*memEE=0,memID[8],memCONFIG[48];
double hvreg=0;
int DeviceDetected=0;
int UseCoff=0;
struct src_i source_info[LMAX];
struct srcfile *s_files;
struct symbol *sym;
int nsym=0;
char* Slabel[LMAX],*Sulabel[ULMAX];
//struct var{	char* name;	int display;} variables[0x200];
struct symbol *watch;
int nwatch=0;
unsigned short coff_data[DATA_MAX];
int ver=0,reset=1,freeze=0,icdConnected=0,running=0;
int break_addr,print_addr;
#define Tck 30
double Tcom=0.001*Tck*18+0.03; //communication time for a 16 bit tranfer (ms)
int icdTimer=0;
int currentSource=-1;
int sourceHilight=0;
char lastCmd[64]="";
#ifdef DEBUG
	int addrDebug=0;
	unsigned short dataDebug=0;
	unsigned short statusDebug=0x3FFF;
#endif

GtkTextBuffer * dataBuf;
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
GtkWidget * status_bar;
GtkWidget * img;
GtkWidget * devCombo;
GtkWidget * devTypeCombo;
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
GtkWidget * statusTxt;
GtkTextBuffer * statusBuf;
GtkWidget * sourceTxt;
GtkTextBuffer * sourceBuf;
GtkWidget * icdVbox1;
GtkWidget * icdMenuPC;
GtkWidget * icdMenuSTAT;
GtkWidget * icdMenuBank0;
GtkWidget * icdMenuBank1;
GtkWidget * icdMenuBank2;
GtkWidget * icdMenuBank3;
GtkWidget * icdMenuEE;
GtkWidget * icdCommand;

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
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(data),&iter,0.0,FALSE,0,0);
}

void PrintMessageI2C(const char *msg){
	GtkTextIter iter;
	GtkTextBuffer * dataBuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(I2CDataReceive));
	gtk_text_buffer_set_text(dataBuf,msg,-1);
	gtk_text_buffer_get_end_iter(dataBuf,&iter);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(I2CDataReceive),&iter,0.0,FALSE,0,0);
	while (gtk_events_pending ()) gtk_main_iteration();
}

void PrintMessageICD(const char *msg){
	GtkTextIter iter;
	//GtkWidget * dataBuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(statusTxt));
	gtk_text_buffer_set_text(statusBuf,msg,-1);
	gtk_text_buffer_get_start_iter(statusBuf,&iter);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(statusTxt),&iter,0.0,FALSE,0,0);
	while (gtk_events_pending ()) gtk_main_iteration();
}

void AppendMessageICD(const char *msg){
	GtkTextIter iter;
	//GtkWidget * dataBuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(statusTxt));
	gtk_text_buffer_get_end_iter(statusBuf,&iter);
	gtk_text_buffer_insert(statusBuf,&iter,msg,-1);
	gtk_text_buffer_get_start_iter(statusBuf,&iter);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(statusTxt),&iter,0.0,FALSE,0,0);
	while (gtk_events_pending ()) gtk_main_iteration();
}

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
	int i=sscanf(gtk_entry_get_text(GTK_ENTRY(ICD_addr_entry)),"%x",&ICDaddr);
	if(i!=1||ICDaddr<0||ICDaddr>0xFFFF) ICDaddr=0x1FF0;
	char *str=gtk_combo_box_get_active_text(GTK_COMBO_BOX(devCombo));
	if(str) strncpy(dev,str,sizeof(dev)-1);
	g_free(str);
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
}

void Fopen(GtkWidget *widget,GtkWidget *window)
{
	char *str=gtk_combo_box_get_active_text(GTK_COMBO_BOX(devCombo));
	if(str) strncpy(dev,str,sizeof(dev)-1);
	g_free(str);
	GtkFileChooser *dialog;
	dialog = (GtkFileChooser*) gtk_file_chooser_dialog_new ("Open File",
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
			GtkFileChooser *dialog2;
			dialog2 = (GtkFileChooser*) gtk_file_chooser_dialog_new (strings[S_openEEfile],
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
			gtk_widget_destroy(GTK_WIDGET(dialog2));
		}
	}
	gtk_widget_destroy (GTK_WIDGET(dialog));
}

void Fsave(GtkWidget *widget,GtkWidget *window)
{
	GtkFileChooser *dialog;
	dialog = (GtkFileChooser*) gtk_file_chooser_dialog_new ("Save File",
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
			GtkFileChooser *dialog2;
			dialog2 = (GtkFileChooser*) gtk_file_chooser_dialog_new (strings[S_saveEEfile],
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
			gtk_widget_destroy(GTK_WIDGET(dialog2));
		}
	  }
	gtk_widget_destroy (GTK_WIDGET(dialog));
}

void DevWrite(GtkWidget *widget,GtkWidget *window)
{
	if(DeviceDetected!=1) return;
	gtk_statusbar_push(GTK_STATUSBAR(status_bar),statusID,"");
	getOptions();
	Write(dev,ee);	//choose the right function
}

void DevRead(GtkWidget *widget,GtkWidget *window)
{
	if(DeviceDetected!=1) return;
	gtk_statusbar_push(GTK_STATUSBAR(status_bar),statusID,"");
	getOptions();
	Read(dev,ee,readRes);	//choose the right function
}

void FilterDevType(GtkWidget *widget,GtkWidget *window)
{
	char *str=gtk_combo_box_get_active_text(GTK_COMBO_BOX(devTypeCombo));
	GtkTreeModel *store = gtk_combo_box_get_model( GTK_COMBO_BOX(devCombo) );
	gtk_list_store_clear( GTK_LIST_STORE( store ) );
	int i=gtk_combo_box_get_active(GTK_COMBO_BOX(devTypeCombo));
	switch(i){
		case 1:		//10F 12F
			for(i=0;i<Ndevices;i++) if(!strncmp(devices[i],"10F",3)||!strncmp(devices[i],"12F",3)) \
				gtk_combo_box_append_text(GTK_COMBO_BOX(devCombo),devices[i]);
		break;
		case 2:		//16F
			for(i=0;i<Ndevices;i++) if(!strncmp(devices[i],"16F",3)) gtk_combo_box_append_text(GTK_COMBO_BOX(devCombo),devices[i]);
		break;
		case 3: 	//18F
			for(i=0;i<Ndevices;i++) if(!strncmp(devices[i],"18F",3)) gtk_combo_box_append_text(GTK_COMBO_BOX(devCombo),devices[i]);
		break;
		case 4:		//24F
			for(i=0;i<Ndevices;i++) if(!strncmp(devices[i],"24F",3)) gtk_combo_box_append_text(GTK_COMBO_BOX(devCombo),devices[i]);
		break;
		case 5:		//30F 33F
			for(i=0;i<Ndevices;i++) if(!strncmp(devices[i],"30F",3)||!strncmp(devices[i],"33F",3)) \
				gtk_combo_box_append_text(GTK_COMBO_BOX(devCombo),devices[i]);
		break;
		case 6:		//ATMEL
			for(i=0;i<Ndevices;i++) if(!strncmp(devices[i],"AT",2)) gtk_combo_box_append_text(GTK_COMBO_BOX(devCombo),devices[i]);
		break;
		case 7:		//24 25 93 DS 11
			for(i=0;i<Ndevices;i++) if( (strncmp(devices[i],"24F",3)&&strncmp(devices[i],"24H",3))&&\
				(!strncmp(devices[i],"24",2)||!strncmp(devices[i],"25",2)||!strncmp(devices[i],"93",2)|| \
				 !strncmp(devices[i],"11",2)||!strncmp(devices[i],"DS",2))) \
				gtk_combo_box_append_text(GTK_COMBO_BOX(devCombo),devices[i]);
		break;
		default:	//*
			for(i=0;i<Ndevices;i++) gtk_combo_box_append_text(GTK_COMBO_BOX(devCombo),devices[i]);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(devCombo),0);
	for(i=0;(str=gtk_combo_box_get_active_text(GTK_COMBO_BOX(devCombo)))&&strcmp(str,dev)&&i<1000;i++){
		gtk_combo_box_set_active(GTK_COMBO_BOX(devCombo),i);
	}
	if(i==1000||!str)gtk_combo_box_set_active(GTK_COMBO_BOX(devCombo),0);
}

void scrollToLine(int line)
{
	GtkTextIter iter,iter2;
	gtk_text_buffer_get_end_iter(sourceBuf,&iter);
	if(line>0){
		gtk_text_iter_set_line(&iter,line-1);
		iter2=iter;
		gtk_text_iter_forward_char(&iter2);
		gtk_text_iter_forward_to_line_end(&iter2);
	}
	else{
		gtk_text_buffer_get_selection_bounds(sourceBuf,&iter,&iter2);
		iter2=iter;
	}
	gtk_text_buffer_select_range(sourceBuf,&iter,&iter2);
	while (gtk_events_pending ()) gtk_main_iteration();
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(sourceTxt),&iter,0.0,TRUE,0,0.5);
}

void SourceHilightLine(int line)
{
	GtkTextIter iter,iter2;
	GtkTextTag* tag;
	if(line>0){
		tag=gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(sourceBuf),"break_text");
		if(!tag) tag=gtk_text_buffer_create_tag(sourceBuf,"break_text","background","red", NULL);
		gtk_text_buffer_get_end_iter(sourceBuf,&iter);
		gtk_text_iter_set_line(&iter,line-1);
		iter2=iter;
		gtk_text_iter_forward_char(&iter2);
		gtk_text_iter_forward_to_line_end(&iter2);
		gtk_text_buffer_apply_tag (sourceBuf,tag,&iter,&iter2);
	}
	while (gtk_events_pending ()) gtk_main_iteration();
}

void SourceRemoveHilightLine(int line)
{
	GtkTextIter iter,iter2;
	if(line>0){
		gtk_text_buffer_get_end_iter(sourceBuf,&iter);
		gtk_text_iter_set_line(&iter,line-1);
		iter2=iter;
		gtk_text_iter_forward_char(&iter2);
		gtk_text_iter_forward_to_line_end(&iter2);
		gtk_text_buffer_remove_tag_by_name(sourceBuf,"break_text",&iter,&iter2);
	}
	while (gtk_events_pending ()) gtk_main_iteration();
}

//load source file into source pane
int loadSource(FILE *f){
	if(!f) return 0;
	fseek(f,0,SEEK_END);
	int size=ftell(f);
	fseek(f,0,SEEK_SET);
	char* tmp=(char*)malloc(size+1);
	size=fread(tmp,1,size,f);
	tmp[size]=0;
	char* g=g_locale_to_utf8(tmp,-1,NULL,NULL,NULL);
	gtk_text_buffer_set_text(sourceBuf,g,-1);
	free(tmp);
	g_free(g);
	return 1;
}

//load and analyze coff file
void loadCoff(GtkWidget *widget,GtkWidget *window)
{
	GtkFileChooser *dialog;
	dialog = (GtkFileChooser*) gtk_file_chooser_dialog_new ("Open Coff File",
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
		UseCoff=analyzeCOFF(filename,Slabel,Sulabel,source_info,&s_files,coff_data,&sym,&nsym);
		g_free (filename);
		//load source for address 0
		if(source_info[0].src_file!=-1){
			if(currentSource==source_info[0].src_file){
				scrollToLine(source_info[0].src_line);
			}
			else if(loadSource(s_files[source_info[0].src_file].ptr)){
				scrollToLine(source_info[0].src_line);
				currentSource=source_info[0].src_file;
			}
		}
	}
	gtk_widget_destroy (GTK_WIDGET(dialog));
}

// List of variables used when decoding an assembly word
void initVar(){
	int i;
	for(i=0;i<0x200;i++){//clear variable list
		variables[i].name=0;
		variables[i].display=0;
	}
	variables[0].name="INDF";
	variables[1].name="TMR0";
	variables[2].name="PCL";
	variables[3].name="STATUS";
	variables[4].name="FSR";
	variables[5].name="PORTA";
	variables[6].name="PORTB";
	variables[7].name="PORTC";
	variables[8].name="PORTD";
	variables[9].name="PORTE";
	variables[10].name="PCLATH";
	variables[11].name="INTCON";
	variables[12].name="PIR1";
	variables[13].name="PIR2";
	variables[14].name="TMR1L";
	variables[15].name="TMR1H";
	variables[16].name="T1CON";
	variables[17].name="TMR2";
	variables[18].name="T2CON";
	variables[19].name="SSPBUF";
	variables[20].name="SSPCON";
	variables[21].name="CCPR1L";
	variables[22].name="CCPR1H";
	variables[23].name="CCP1CON";
	variables[24].name="RCSTA";
	variables[25].name="TXREG";
	variables[26].name="RCREG";
	variables[27].name="CCPR2L";
	variables[28].name="CCPR2H";
	variables[29].name="CCP2CON";
	variables[30].name="ADRESH";
	variables[31].name="ADCON0";
	variables[0x6B].name="DEBUG_VAR1";
	variables[0x6C].name="DEBUG_VAR2";
	variables[0x6D].name="DEBUG_VAR3";
	variables[0x6E].name="DEBUG_VAR4";
	variables[0x6F].name="DEBUG_VAR5";
	variables[0x70].name="DEBUG_VAR6";
	variables[0x71].name="DEBUG_VAR7";
	variables[0x72].name="DEBUG_VAR8";
	variables[0x80].name="INDF";
	variables[0x81].name="OPTION_REG";
	variables[0x82].name="PCL";
	variables[0x83].name="STATUS";
	variables[0x84].name="FSR";
	variables[0x85].name="TRISA";
	variables[0x86].name="TRISB";
	variables[0x87].name="TRISC";
	variables[0x88].name="TRISD";
	variables[0x89].name="TRISE";
	variables[0x8A].name="PCLATH";
	variables[0x8B].name="INTCON";
	variables[0x8C].name="PIE1";
	variables[0x8D].name="PIE2";
	variables[0x8E].name="PCON";
	variables[0x91].name="SSPCON2";
	variables[0x92].name="PR2";
	variables[0x93].name="SSPADD";
	variables[0x94].name="SSPSTAT";
	variables[0x98].name="TXSTA";
	variables[0x99].name="SPBRG";
	variables[0x9E].name="ADRESL";
	variables[0x9F].name="ADCON1";
	variables[0x100].name="INDF";
	variables[0x101].name="TMR0";
	variables[0x102].name="PCL";
	variables[0x103].name="STATUS";
	variables[0x104].name="FSR";
	variables[0x106].name="PORTB";
	variables[0x10A].name="PCLATH";
	variables[0x10B].name="INTCON";
	variables[0x10C].name="EEDATA";
	variables[0x10D].name="EEADR";
	variables[0x10E].name="EEDATH";
	variables[0x10F].name="EEADRH";
	variables[0x180].name="INDF";
	variables[0x181].name="OPTION_REG";
	variables[0x182].name="PCL";
	variables[0x183].name="STATUS";
	variables[0x184].name="FSR";
	variables[0x186].name="TRISB";
	variables[0x18A].name="PCLATH";
	variables[0x18B].name="INTCON";
	variables[0x18C].name="EECON1";
	variables[0x18D].name="EECON2";
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
		//"authors","Alberto Maccioni",NULL,
		"comments", "A graphical interface for the Open Programmer",
		"copyright",
		"Copyright (C) Alberto Maccioni 2009-2011\n\n"
		"This program is free software; you can \n"
		"redistribute it and/or modify it under \n"
		"the terms of the GNU General Public License \n"
		"as published by the Free Software Foundation;\n"
		"either version 2 of the License, or \n"
		"(at your option) any later version.",
		//"documenters"              GStrv*                : Read / Write
		"license",license,
		//  "logo"                     GdkPixbuf*            : Read / Write
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

void ICDHelp(GtkWidget *widget,GtkWidget *window)
{
	GtkWidget* dialog = gtk_message_dialog_new (GTK_WINDOW(window),
                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                 GTK_MESSAGE_INFO,
                                 GTK_BUTTONS_CLOSE,
                                 strings[I_ICD_HELP_TXT]);
	/* Destroy the dialog when the user responds to it (e.g. clicks a button) */
	g_signal_connect_swapped (GTK_WINDOW(dialog), "response",G_CALLBACK (gtk_widget_destroy),dialog);
	gtk_window_set_title(GTK_WINDOW(dialog),strings[I_ICD_HELP]);
	gtk_widget_show_all (dialog);
}

void icdCheck(GtkWidget *widget,GtkWidget *window)
{
#ifndef DEBUG
	if(DeviceDetected!=1) return;
#endif
	if(!isRunning()){
		gtk_timeout_remove(icdTimer);
		ShowContext();
	}
}

void icdRun(GtkWidget *widget,GtkWidget *window)
{
#ifndef DEBUG
	if(DeviceDetected!=1) return;
#endif
	if(!icdConnected){
		saveLog = (int) gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_log));
		if(saveLog){
			OpenLogFile();	//"Log.txt"
			fprintf(logfile,"ICD start\n");
		}
		startICD(Tck);	//start ICD mode by supplying the target and forcing a reset
		run();			//remove reset
		icdConnected=1;
		icdTimer=gtk_timeout_add(20,(GtkFunction)icdCheck,NULL);
		PrintMessageICD("running");
	}
	else if(!running){
		cont(break_addr,freeze);	//continue execution
		icdTimer=gtk_timeout_add(20,(GtkFunction)icdCheck,NULL);
		PrintMessageICD("running");
	}
}

void icdHalt(GtkWidget *widget,GtkWidget *window)
{
#ifndef DEBUG
	if(DeviceDetected!=1) return;
#endif
	if(running){
		gtk_timeout_remove(icdTimer);
		Halt();
		ShowContext();
	}
}

void icdStep(GtkWidget *widget,GtkWidget *window)
{
#ifndef DEBUG
	if(DeviceDetected!=1) return;
#endif
	if(running){
		gtk_timeout_remove(icdTimer);
		Halt();
	}
	step();
#ifdef DEBUG
	addrDebug++;
#endif
	ShowContext();
}

void icdStepOver(GtkWidget *widget,GtkWidget *window)
{
#ifndef DEBUG
	if(DeviceDetected!=1) return;
#endif
	int addr,data;
	if(running){
		gtk_timeout_remove(icdTimer);
		Halt();
	}
	addr=((ReadRegister(0x18E)&0x1F)<<8)+ReadRegister(0x18F);
	data=ReadProgMem(addr);
	if((data>>11)==4){	//if call break at return address
		cont(addr+1,freeze);
		icdTimer=gtk_timeout_add(20,(GtkFunction)icdCheck,NULL);
	}
	else{		//normal step
		step();
		#ifdef DEBUG
		addrDebug++;
		#endif
		ShowContext();
	}
}

void icdStop(GtkWidget *widget,GtkWidget *window)
{
#ifndef DEBUG
	if(DeviceDetected!=1) return;
#endif
	if(running){
		gtk_timeout_remove(icdTimer);
		Halt();
	}
	bufferU[0]=0;
	int j=1;
	bufferU[j++]=EN_VPP_VCC;		// reset target
	bufferU[j++]=0x0;
	bufferU[j++]=WAIT_T3;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x2;				//set D as input
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(3);
	read();
	if(saveLog)WriteLogIO();
	icdConnected=0;
	PrintMessageICD("stopped");
	scrollToLine(source_info[0].src_line);
}

void icdRefresh(GtkWidget *widget,GtkWidget *window)
{
#ifndef DEBUG
	if(DeviceDetected!=1) return;
#endif
	if(!running){
		ShowContext();
	}
}

// Read and display an entire bank of memory
void ShowBank(int bank,char* status){
	if(bank>3) bank=3;
	if(bank<0) bank=0;
	int b[128];
	char temp[128];
	int i;
	sprintf(temp,"bank %d:",bank);
	strcat(status,temp);
	ReadRegisterN(bank*0x80,128,b);
	for(i=0;i<128;i++){
		if(i%16==0){
			sprintf(temp,"\n0x%03X:",i+bank*0x80);
			strcat(status,temp);
		}
		sprintf(temp,"%02X",b[i]);
		strcat(status,temp);
	}
	strcat(status,"\n");
}

// Main ICD show function:
// prints status info according to selected options
// and the value of variables in the watch list
void ShowContext(){
	int i,addr,data,s;
	char cmd[32]="";
	char status[4096]="",temp[128];
	addr=((ReadRegister(0x18E)&0x1F)<<8)+ReadRegister(0x18F);
	data=ReadProgMem(addr);
	s=ReadRegister(status_temp);
	s=(s>>4)+((s<<4)&0xF0);		//STATUS is swapped
//	printf("addr %X, status %X, data %X\n",addr,s,data);
#ifdef DEBUG
	addr=addrDebug;
	s=statusDebug;
	if(UseCoff) data=coff_data[addr];
#endif
	if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(icdMenuPC))){
		sprintf(temp,"%s: %s (0x%04X) \nPC=0x%04X\n",strings[S_NextIns],decodeCmd(data,cmd,(s&0x60)<<2),data,addr); //"Next instruction"
		strcat(status,temp);
	}
	if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(icdMenuSTAT))){
		sprintf(temp,"STATUS=0x%02X (",s);
		strcat(status,temp);
		sprintf(temp,"%s ",s&0x80?"IRP":"   ");
		strcat(status,temp);
		sprintf(temp,"%s ",s&0x40?"RP1":"   ");
		strcat(status,temp);
		sprintf(temp,"%s ",s&0x20?"RP0":"   ");
		strcat(status,temp);
		sprintf(temp,"%s ",s&0x10?"TO":"  ");
		strcat(status,temp);
		sprintf(temp,"%s ",s&0x8?"PD":"  ");
		strcat(status,temp);
		sprintf(temp,"%s ",s&0x4?"Z":" ");
		strcat(status,temp);
		sprintf(temp,"%s ",s&0x2?"DC":"  ");
		strcat(status,temp);
		sprintf(temp,"%s)\n",s&0x1?"C":" ");
		strcat(status,temp);
		sprintf(temp,"W=0x%02X PCLATH=0x%02X FSR=0x%02X\n",ReadRegister(w_temp),ReadRegister(pclath_temp),ReadRegister(fsr_temp));
		strcat(status,temp);
	}
	if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(icdMenuBank0))) ShowBank(0,status);
	if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(icdMenuBank1))) ShowBank(1,status);
	if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(icdMenuBank2))) ShowBank(2,status);
	if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(icdMenuBank3))) ShowBank(3,status);
	int rawsource=0;
	if(UseCoff){	//hilight corresponding source line
		if(data!=coff_data[addr]){
			sprintf(temp,"code at address 0x%04X (0x%04X) is different than what specified in coff file (0x%04X)\n",addr,data,coff_data[addr]);
			strcat(status,temp);
			rawsource=1;
		}
#ifndef DEBUG
		else{
#endif
//			printf("addr %d, file %d, line %d, current %d, ptr %X\n",addr,source_info[addr].src_file,source_info[addr].src_line,currentSource,s_files[source_info[addr].src_file].ptr);
			if(source_info[addr].src_file!=-1){
				if(currentSource==source_info[addr].src_file)	scrollToLine(source_info[addr].src_line);
				else if(loadSource(s_files[source_info[addr].src_file].ptr)){
					scrollToLine(source_info[addr].src_line);
					currentSource=source_info[addr].src_file;
				}
				else rawsource=1;
			}
			else rawsource=1;
#ifndef DEBUG
		}
#endif
	}
	if(!UseCoff || rawsource==1){	//show raw source if no source file is available
		#define LINES_BEFORE 5
		#define LINES_AFTER 7
		#define NLINES LINES_BEFORE + LINES_AFTER
		int addr0,addr1,line_pc=0;
		char tmp[64*NLINES],t2[64];
		tmp[0]=0;
		int progmem[NLINES];
		addr0=addr-LINES_BEFORE<0?0:addr-LINES_BEFORE;
		addr1=addr+LINES_AFTER>0x1FFF?0x1FFF:addr+LINES_AFTER;
		ReadProgMemN(addr0,addr1-addr0,progmem);
		for(i=addr0;i<addr1;i++){
			sprintf(t2,"0x%04X: %s (0x%04X)\n",i,decodeCmd(progmem[i-addr0],cmd,(s&0x60)<<2),progmem[i-addr0]);
			strcat(tmp,t2);
			if(i==addr) line_pc=i;
		}
		gtk_text_buffer_set_text(sourceBuf,tmp,-1);
		currentSource=-1;
		scrollToLine(line_pc-addr0+1);
	}
	for(i=0;i<nwatch;i++){
		sprintf(temp,"%s=0x%02X\n",watch[i].name,ReadRegister(watch[i].value));
		strcat(status,temp);
	}
	PrintMessageICD(status);
	if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(icdMenuEE))){
		unsigned char data[256];
		str[0]=0;
		char s[64],t[9],*g;
		t[8]=0;
		ReadDataMemN(0,256,data);
		strcat(str,"EEPROM:\n");
		for(i=0;i<0x100;i++){
			if(i%8==0){
				sprintf(s,"\n0x%02X: ",i);
				strcat(str,s);
			}
			sprintf(s,"%02X ",data[i]);
			strcat(str,s);
			t[i&0x7]=isprint(data[i])?data[i]:'.';
			if(i%8==7){
				g=g_locale_to_utf8(t,-1,NULL,NULL,NULL);
				if(g) strcat(str,g);
				g_free(g);
			}
		}
		AppendMessageICD(str);
	}

}

int addWatch(struct symbol s){
	int i;
	for(i=0;i<nwatch&&strcmp(watch[i].name,s.name);i++);
	if(i<nwatch){	//remove watch
		for(;i<nwatch-1;i++){
			watch[i].name=watch[i+1].name;
			watch[i].value=watch[i+1].value;
		}
		nwatch--;
		watch=realloc(watch,nwatch*sizeof(struct symbol));
	}
	else{			//add watch
		nwatch++;
		watch=realloc(watch,nwatch*sizeof(struct symbol));
		watch[nwatch-1].name=s.name;
		watch[nwatch-1].value=s.value;
		return 1;
	}
	return 0;
}

// ICD Command parser
int executeCommand(char *command){
//******************* break ********************************
	if(strstr(command,"break")){
		if(sscanf(command,"break %x",&break_addr)==1){
			if(running) Halt();
			break_addr&=0x1FFF;
			sprintf(str,"break at address 0x%04X\n",break_addr);
			AppendMessageICD(str);
			SourceRemoveHilightLine(sourceHilight);
			SourceHilightLine(source_info[break_addr].src_line);
			sourceHilight=source_info[break_addr].src_line;
		}
	}
//******************* clear ********************************
	if(strstr(command,"clear")){
		PrintMessageICD("");
	}
//******************* freeze ********************************
	else if(strstr(command,"freeze")){
		char option[32];
		if(sscanf(command,"freeze %s",option)==1){
			if(running) Halt();
			if(!strcmp(option,"on")) freeze=1;
			if(!strcmp(option,"off")) freeze=0;
			WriteRegister(0x18E,(break_addr>>8)+(freeze?0x40:0));
		}
		sprintf(str,"peripheral freeze is %s\n",freeze?"on":"off");
		AppendMessageICD(str);
	}
//******************* halt ********************************
	else if(!strcmp(command,"h")||!strcmp(command,"halt")){
		icdHalt(NULL,NULL);
	}
//******************* help ********************************
	else if(!strcmp(command,"help")){
		ICDHelp(NULL,NULL);
	}
//******************* list ********************************
	else if(strstr(command,"list ")){
		#define LISTLINES 10
		int addr,i;
		char tmp[32*LISTLINES],t2[32],cmd[32]="";
		tmp[0]=0;
		int progmem[LISTLINES];
		if(sscanf(command,"list %x",&addr)==1){
			addr&=0x1FFF;
			ReadProgMemN(addr,LISTLINES,progmem);
			for(i=0;i<LISTLINES;i++){
				sprintf(t2,"0x%04X: %s (0x%04X)\n",i+addr,decodeCmd(progmem[i],cmd,0),progmem[i]);
				strcat(tmp,t2);
			}
			//printf(tmp);
			AppendMessageICD(tmp);
		}
	}
//******************* print ********************************
	else if(strstr(command,"print ")||strstr(command,"p ")){
		int bank,i,addr,data;
		char var[128];
		if(strstr(command,"print p")||strstr(command,"p p")){	//program memory
			int addr;
			if(sscanf(command,"print p %x",&addr)==1||sscanf(command,"p p %x",&addr)==1){
				addr&=0x1FFF;
				if(running) Halt();
				data=ReadProgMem(addr);
				sprintf(str,"0x%04X: %s (0x%04X)\n",addr,decodeCmd(data,var,0x1000),data);
				AppendMessageICD(str);
			}
		}
		else if(!strcmp(command,"print ee")||!strcmp(command,"p ee")){	//eeprom
			unsigned char data[256];
			str[0]=0;
			char s[64],t[9],*g;
			t[8]=0;
			if(running) Halt();
			ReadDataMemN(0,256,data);
			sprintf(str,"EEPROM:\n");
			for(i=0;i<0x100;i++){
				if(i%8==0){
					sprintf(s,"\n0x%02X: ",i);
					strcat(str,s);
				}
				sprintf(s,"%02X ",data[i]);
				strcat(str,s);
				t[i&0x7]=isprint(data[i])?data[i]:'.';
				if(i%8==7){
					g=g_locale_to_utf8(t,-1,NULL,NULL,NULL);
					if(g) strcat(str,g);
					g_free(g);
				}
			}
			strcat(str,"\n");
			//printf("EEPROM:\n");fflush(stdout);
			AppendMessageICD(str);
		}
		else if(sscanf(command,"print ee %x",&addr)==1||sscanf(command,"p ee %x",&addr)==1){ //single EE address
			addr&=0xFF;
			if(running) Halt();
			data=ReadDataMem(addr);
			sprintf(str,"eeprom memory at 0x%02X=0x%02X (%c)\n",addr,data,isprint(data)?data:'.');
			AppendMessageICD(str);
		}
		else if(sscanf(command,"print bank %x",&bank)==1||sscanf(command,"p bank %x",&bank)==1){	//memory bank
			str[0]=0;
			bank&=0x1FF;
			if(bank>3) bank/=0x80;
			if(running) Halt();
			ShowBank(bank,str);
			AppendMessageICD(str);
		}
		else if(sscanf(command,"print 0x%x",&print_addr)==1||sscanf(command,"p 0x%x",&print_addr)==1){ //mem address
			print_addr&=0x1FF;
			if(running) Halt();
			sprintf(str,"[0x%03X]=0x%02X\n",print_addr,ReadRegister(print_addr));
			AppendMessageICD(str);
		}
		else if(sscanf(command,"print %s",var)==1||sscanf(command,"p %s",var)==1){ //var name
			str[0]=0;
			if(running) Halt();
			if(!strcmp("W",var)||!strcmp("w",var)) sprintf(str,"W = 0x%02X\n",ReadRegister(w_temp));
			else if(!strcmp("STATUS",var)) sprintf(str,"0x003: STATUS = 0x%02X\n",ReadRegister(status_temp));
			else if(!strcmp("FSR",var)) sprintf(str,"0x004: FSR = 0x%02X\n",ReadRegister(fsr_temp));
			else if(!strcmp("PCLATH",var)) sprintf(str,"0x00A: PCLATH = 0x%02X\n",ReadRegister(pclath_temp));
			else{
				for(i=0;i<nsym&&strcmp(var,sym[i].name);i++);
				if(i<nsym){
					sprintf(str,"0x%03X: %s = 0x%02X\n",sym[i].value,sym[i].name,ReadRegister(sym[i].value));
				}
				else{	//look in standard variables
					for(i=0;i<0x200;i++){
						if(variables[i].name&&!strcmp(var,variables[i].name)){
							sprintf(str,"0x%03X: %s = 0x%02X\n",i,variables[i].name,ReadRegister(i));
							i=0x200;
						}
					}
				}
			}
			AppendMessageICD(str);
		}
	}
//******************* run ********************************
	else if(!strcmp(command,"r")||!strcmp(command,"run")){
		icdRun(NULL,NULL);
	}
//******************* step ********************************
	else if(!strcmp(command,"s")||!strcmp(command,"step")||strstr(command,"step")||strstr(command,"s ")){
		int i,n=1;
		sscanf(command,"step %d",&n);
		sscanf(command,"s %d",&n);
#ifdef DEBUG
		addrDebug+=n;
#endif
		if(running) Halt();
		for(i=0;i<n;i++) step();
		if(n>1)sprintf(str,"step %d\n",n);
		else str[0]=0;
		ShowContext();
		AppendMessageICD(str);
	}
//******************* step over ********************************
	else if(!strcmp(command,"ss")||!strcmp(command,"step over")||strstr(command,"step over ")||strstr(command,"ss ")){
		int i,n=1;
		sscanf(command,"step over %d",&n);
		sscanf(command,"ss %d",&n);
#ifdef DEBUG
		addrDebug+=n;
#endif
		for(i=0;i<n;i++) icdStepOver(NULL,NULL);
		if(n>1)sprintf(str,"step over %d\n",n);
		AppendMessageICD(str);
	}
//******************* version ********************************
	else if(!strcmp(command,"ver")||!strcmp(command,"version")){
		if(running) Halt();
		sprintf(str,"debugger version: %.1f\n",version()/10.0);
		AppendMessageICD(str);
	}
//******************* watch ********************************
	else if(strstr(command,"watch ")||strstr(command,"w ")){
		int i,var_addr;
		char var[64];
		if(sscanf(command,"watch 0x%x",&var_addr)||sscanf(command,"w 0x%x",&var_addr)){
			struct symbol s;
			sprintf(var,"[0x%X]",var_addr);
			s.name=strdup(var);
			s.value=var_addr;
			addWatch(s);
			if(!running) ShowContext();
		}
		else if(sscanf(command,"watch %s",var)||sscanf(command,"w %s",var)){
			for(i=0;i<nsym&&strcmp(var,sym[i].name);i++);
			if(i<nsym){
				addWatch(sym[i]);
				if(!running) ShowContext();
			}
			else{	//look in standard variables
				for(i=0;i<0x200;i++){
					if(variables[i].name&&!strcmp(var,variables[i].name)){
						struct symbol s;
						s.name=variables[i].name;
						s.value=i;
						addWatch(s);
						if(!running) ShowContext();
						i=0x200;
					}
				}
			}
		}
	}
//******************* set variable ********************************
//to do: special addresses (PC, status ecc)
	else{
		char var[64],*p;
		int data,i,addr=-1;
		if((p=strchr(command,'='))){
			*p=0;
			if(sscanf(command,"[%x]",&addr)&&sscanf(p+1,"%x",&data)){
				if(running) Halt();
				WriteRegister(addr,data);
				ShowContext();
				sprintf(str,"[0x%x]=0x%02X\n",addr,data);
				AppendMessageICD(str);
			}
			else if(sscanf(command,"%s",var)&&sscanf(p+1,"%x",&data)){
				for(i=0;i<nsym&&strcmp(var,sym[i].name);i++);
				if(i<nsym&&sym[i].value<0x400){
					addr=sym[i].value;
				}
				else{	//look in standard variables
					for(i=0;i<0x200;i++){
						if(variables[i].name&&!strcmp(var,variables[i].name)){
							addr=i;
							i=0x200;
						}
					}
				}
				if(addr!=-1){
					if(running) Halt();
					WriteRegister(addr,data);
					ShowContext();
					sprintf(str,"%s=0x%02X\n",var,data);
					AppendMessageICD(str);
				}
			}
		}
		else return 0;
	}
	return 1;
}

int removeWatch(char* name){
	int i;
	for(i=0;i<nwatch&&strcmp(watch[i].name,name);i++);
	if(i<nwatch){	//remove watch
		for(;i<nwatch-1;i++){
			watch[i].name=watch[i+1].name;
			watch[i].value=watch[i+1].value;
		}
		nwatch--;
		watch=realloc(watch,nwatch*sizeof(struct symbol));
		return 1;
	}
	return 0;
}

gint source_mouse_event(GtkWidget *widget, GdkEventButton *event, gpointer func_data)
{
	if(GTK_IS_TEXT_VIEW(widget)&&event->type==GDK_2BUTTON_PRESS){
		gint x,y,i;
		GtkTextIter iter,iter2,itx;
		gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(widget),GTK_TEXT_WINDOW_WIDGET,event->x,event->y,&x,&y);
		gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(widget),&iter,x,y);
//		printf("x %d y %d\n",x,y);
		iter2=iter;
		char c;
		for(itx=iter2,c=gtk_text_iter_get_char(&itx);isalnum(c)||c=='_';iter2=itx){
			gtk_text_iter_forward_char(&itx);
			c=gtk_text_iter_get_char(&itx);
		}
		for(itx=iter,c=gtk_text_iter_get_char(&itx);isalnum(c)||c=='_';iter=itx){
			gtk_text_iter_backward_char(&itx);
			c=gtk_text_iter_get_char(&itx);
		}
		gtk_text_iter_forward_char(&iter);
		char* selection=gtk_text_buffer_get_text(sourceBuf,&iter,&iter2,FALSE);
		for(i=0;i<nsym&&strcmp(selection,sym[i].name);i++);
		if(i<nsym){
			addWatch(sym[i]);
			ShowContext();
		}
		else{	//set breakpoint
			int line=gtk_text_iter_get_line(&iter)+1;
			for(i=0;i<LMAX;i++) if(source_info[i].src_line==line){
				//if(UseCoff && i>0 && (coff_data[i-1]>>11)!=4) i--; //if not a call break at previous address;
				break_addr=i;
				sprintf(str,"break at address 0x%x\n",i);
				AppendMessageICD(str);
				SourceRemoveHilightLine(sourceHilight);
				SourceHilightLine(line);
				sourceHilight=line;
				break;
			}
		}
	}
	return FALSE;
}

gint icdStatus_mouse_event(GtkWidget *widget, GdkEventButton *event, gpointer func_data)
{
	if(GTK_IS_TEXT_VIEW(widget)&&event->type==GDK_2BUTTON_PRESS){
		gint x,y;
		GtkTextIter iter,iter2,itx;
		gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(widget),GTK_TEXT_WINDOW_WIDGET,event->x,event->y,&x,&y);
		gtk_text_view_get_iter_at_location  (GTK_TEXT_VIEW(widget),&iter,x,y);
		iter2=iter;
		char c;
		for(itx=iter2,c=gtk_text_iter_get_char(&itx);isalnum(c)||c=='_';iter2=itx){
			gtk_text_iter_forward_char(&itx);
			c=gtk_text_iter_get_char(&itx);
		}
		for(itx=iter,c=gtk_text_iter_get_char(&itx);isalnum(c)||c=='_';iter=itx){
			gtk_text_iter_backward_char(&itx);
			c=gtk_text_iter_get_char(&itx);
		}
		gtk_text_iter_forward_char(&iter);
		char* selection=gtk_text_buffer_get_text(statusBuf,&iter,&iter2,FALSE);
		if(removeWatch(selection)) ShowContext();
	}
	return FALSE;
}

gint icdCommand_key_event(GtkWidget *widget, GdkEventButton *event, gpointer func_data)
{
	if(event->type==GDK_KEY_PRESS&&((GdkEventKey*)event)->keyval==0xFF0D){	//enter
		char s[64];
		strncpy(s,gtk_entry_get_text(GTK_ENTRY(icdCommand)),63);
		if(!strlen(s)){
			strcpy(s,lastCmd);
			gtk_entry_set_text(GTK_ENTRY(icdCommand),s);	//briefly flash last command
			while (gtk_events_pending ()) gtk_main_iteration();
			msDelay(60);
		}
		else strcpy(lastCmd,s);
		if(executeCommand(s)) gtk_entry_set_text(GTK_ENTRY(icdCommand),"");
//		sprintf(s,"k=%X\n",((GdkEventKey*)event)->keyval);
//		AppendMessageICD(gtk_entry_get_text(icdCommand));
	}
	return FALSE;
}

gint icd_key_event(GtkWidget *widget, GdkEventButton *event, gpointer func_data)
{
	while (gtk_events_pending ()) gtk_main_iteration();	//wait completion of other tasks
	if(event->type==GDK_KEY_PRESS){
		switch(((GdkEventKey*)event)->keyval){
			case 0xFFBE:
				ICDHelp(NULL,NULL);	//F1 = help
				break;
			case 0xFFC2:
				icdHalt(NULL,NULL);	//F5 = halt
				break;
			case 0xFFC4:
				icdStep(NULL,NULL);	//F7 = step
				break;
			case 0xFFC5:
				icdStepOver(NULL,NULL);	//F8 = step over
				break;
			case 0xFFC6:
				icdRun(NULL,NULL);	//F9 = run
				break;
		}
//		char s[64];
//		sprintf(s,"k=%X\n",((GdkEventKey*)event)->keyval);
//		AppendMessageICD(s);
	}
	return FALSE;
}

void Xclose(){
	gtk_main_quit();
}

int main( int argc, char *argv[])
{
	strinit();
	#if defined _WIN32
	int langID=GetUserDefaultLangID();
	if((langID&0xFF)==0x10)strings=strings_it;
	#else
	if(getenv("LANG")&&strstr(getenv("LANG"),"it")!=0) strings=strings_it;
	#endif
	else strings=strings_en;
	FILE *f;
	gchar *homedir,*config_dir,*fname=0;
	homedir = (gchar *) g_get_home_dir ();
	if(homedir){
		config_dir=g_build_path(G_DIR_SEPARATOR_S,homedir,CONFIG_DIR, NULL);
		if(!g_file_test(config_dir,G_FILE_TEST_IS_DIR))
	#if defined _WIN32
		mkdir(config_dir);
	#else
		mkdir(config_dir, 0x0755);
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

	gtk_init(&argc, &argv);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	sprintf(str,"opgui v%s",VERSION);
	gtk_window_set_title(GTK_WINDOW(window),str);
	gtk_window_set_default_size(GTK_WINDOW(window), 750, 250);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	g_signal_connect(GTK_WINDOW(window), "destroy",G_CALLBACK(Xclose),NULL);
	GdkPixbuf* pixbuf = gdk_pixbuf_new_from_inline (-1, system_icon, FALSE, NULL);
	gtk_window_set_icon(GTK_WINDOW(window),pixbuf);

	GtkWidget * vbox = gtk_vbox_new(FALSE,0);
	gtk_container_add(GTK_CONTAINER(window),vbox);
	PangoFontDescription    *font_desc;

//------toolbar-------------
	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar),GTK_TOOLBAR_ICONS);
	gtk_box_pack_start(GTK_BOX(vbox),toolbar,FALSE,FALSE,0);

	pixbuf = gdk_pixbuf_new_from_inline (-1, read_icon, FALSE, NULL);
	GtkWidget* iconRead = gtk_image_new_from_pixbuf(pixbuf);
	pixbuf = gdk_pixbuf_new_from_inline (-1, write_icon, FALSE, NULL);
	GtkWidget* iconWrite = gtk_image_new_from_pixbuf(pixbuf);

	gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar),GTK_STOCK_OPEN,strings[I_Fopen],"",G_CALLBACK(Fopen),0,-1);//"Open File"
	gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar),GTK_STOCK_SAVE,strings[I_Fsave],"",G_CALLBACK(Fsave),0,-1);//"Save File"
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
	gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),"",strings[I_DevR],"",iconRead,G_CALLBACK(DevRead),NULL);//"Read device"
	gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),"",strings[I_DevW],"",iconWrite,G_CALLBACK(DevWrite),NULL);//"Write device"
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
	gtk_container_add(GTK_CONTAINER(data_scroll),data);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),data_scroll,label);
	font_desc = pango_font_description_from_string ("monospace 8");
	gtk_widget_modify_font (data, font_desc);
	pango_font_description_free (font_desc);

//------device tab-------------
	label = gtk_label_new(strings[I_Dev]);	//"Device"
	GtkWidget * table_dev = gtk_table_new(2,2,FALSE);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),table_dev,label);

	GtkWidget * devHbox1 = gtk_hbox_new(FALSE,10);
	gtk_table_attach(GTK_TABLE(table_dev),devHbox1,0,1,0,1,GTK_FILL,0,5,5);
	label = gtk_label_new(strings[I_Dev]);	//"Device"
	gtk_box_pack_start(GTK_BOX(devHbox1),label,FALSE,TRUE,0);
	devCombo = gtk_combo_box_new_text();
	gtk_box_pack_start(GTK_BOX(devHbox1),devCombo,FALSE,TRUE,0);

	GtkWidget * devHbox2 = gtk_hbox_new(FALSE,10);
	gtk_table_attach(GTK_TABLE(table_dev),devHbox2,1,2,0,1,GTK_FILL,0,5,5);
	label = gtk_label_new(strings[I_Type]);	//"Type"
	gtk_box_pack_start(GTK_BOX(devHbox2),label,FALSE,TRUE,0);
	devTypeCombo = gtk_combo_box_new_text();
	gtk_box_pack_start(GTK_BOX(devHbox2),devTypeCombo,FALSE,TRUE,0);

	GtkWidget * devFramePIC = gtk_frame_new(strings[I_PIC_CONFIG]);	//"PIC configuration"
	gtk_table_attach(GTK_TABLE(table_dev),devFramePIC,0,1,1,6,0,0,5,5);
	GtkWidget * devVboxPIC = gtk_vbox_new(FALSE,5);
	gtk_container_add(GTK_CONTAINER(devFramePIC),GTK_WIDGET(devVboxPIC));

	EEPROM_RW = gtk_check_button_new_with_label(strings[I_EE]);	//"Read and write EEPROM"
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(EEPROM_RW),TRUE);
//	gtk_table_attach(GTK_TABLE(table_dev),EEPROM_RW,0,1,1,2,GTK_FILL,0,5,5);
	gtk_container_add(GTK_CONTAINER(devVboxPIC),GTK_WIDGET(EEPROM_RW));

	ReadReserved = gtk_check_button_new_with_label(strings[I_ReadRes]);	//"Read reserved area"
//	gtk_table_attach(GTK_TABLE(table_dev),ReadReserved,0,1,2,3,GTK_FILL,0,5,0);
	gtk_container_add(GTK_CONTAINER(devVboxPIC),GTK_WIDGET(ReadReserved));

	Write_ID_BKCal = gtk_check_button_new_with_label(strings[I_ID_BKo_W]);	//"Write ID and BKOscCal"
//	gtk_table_attach(GTK_TABLE(table_dev),Write_ID_BKCal,0,1,3,4,GTK_FILL,0,5,0);
	gtk_container_add(GTK_CONTAINER(devVboxPIC),GTK_WIDGET(Write_ID_BKCal));

	WriteCalib12 = gtk_check_button_new_with_label(strings[I_CalW]);	//"Write Calib 1 and 2"
//	gtk_table_attach(GTK_TABLE(table_dev),WriteCalib12,0,1,4,5,GTK_FILL,0,5,0);
	gtk_container_add(GTK_CONTAINER(devVboxPIC),GTK_WIDGET(WriteCalib12));

	GtkWidget * devFrameOsc = gtk_frame_new(strings[I_OSCW]);	//"Write OscCal"
//	gtk_table_attach(GTK_TABLE(table_dev),devFrameOsc,0,1,5,6,GTK_FILL,0,5,0);
	gtk_container_add(GTK_CONTAINER(devVboxPIC),GTK_WIDGET(devFrameOsc));
	GtkWidget * devVboxOsc = gtk_vbox_new(FALSE,5);
	gtk_container_add(GTK_CONTAINER(devFrameOsc),GTK_WIDGET(devVboxOsc));
	UseOSCCAL = gtk_radio_button_new_with_label(NULL,strings[I_OSC]);	//"OSCCal"
	UseBKOSCCAL = gtk_radio_button_new_with_label(\
		gtk_radio_button_get_group(GTK_RADIO_BUTTON(UseOSCCAL)),strings[I_BKOSC]);	//"Backup OSCCal"
	UseFileCal = gtk_radio_button_new_with_label(\
		gtk_radio_button_get_group(GTK_RADIO_BUTTON(UseOSCCAL)),strings[I_OSCF]);	//"From file"
	gtk_container_add(GTK_CONTAINER(devVboxOsc),GTK_WIDGET(UseOSCCAL));
	gtk_container_add(GTK_CONTAINER(devVboxOsc),GTK_WIDGET(UseBKOSCCAL));
	gtk_container_add(GTK_CONTAINER(devVboxOsc),GTK_WIDGET(UseFileCal));

//	GtkWidget * devHboxICD = gtk_hbox_new(FALSE,5);
//	gtk_table_attach(GTK_TABLE(table_dev),devHboxICD,0,2,6,7,GTK_FILL,0,5,5);
//	gtk_container_add(GTK_CONTAINER(devVboxPIC),GTK_WIDGET(devHboxICD));
	ICD_check = gtk_check_button_new_with_label(strings[I_ICD_ENABLE]);	//"Enable ICD"
//	gtk_container_add(GTK_CONTAINER(devHboxICD),ICD_check);
	gtk_container_add(GTK_CONTAINER(devVboxPIC),ICD_check);
	GtkWidget * devHboxICD = gtk_hbox_new(FALSE,10);
	label = gtk_label_new(strings[I_ICD_ADDRESS]);	//"ICD routine address"
//	gtk_container_add(GTK_CONTAINER(devHboxICD),label);
	gtk_box_pack_start(GTK_BOX(devHboxICD),GTK_WIDGET(label),0,0,5);
	ICD_addr_entry = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(ICD_addr_entry),4);
//	gtk_container_add(GTK_CONTAINER(devHboxICD),GTK_WIDGET(ICD_addr_entry));
	gtk_box_pack_start(GTK_BOX(devHboxICD),GTK_WIDGET(ICD_addr_entry),0,0,5);
	gtk_container_add(GTK_CONTAINER(devVboxPIC),GTK_WIDGET(devHboxICD));

	GtkWidget * devFrameAVR = gtk_frame_new(strings[I_AT_CONFIG]);	//"Atmel configuration"
	gtk_table_attach(GTK_TABLE(table_dev),devFrameAVR,1,2,1,2,0,4,5,5);
	GtkWidget * devTableAVR = gtk_table_new(2,2,FALSE);
	gtk_container_add(GTK_CONTAINER(devFrameAVR),devTableAVR);
	AVR_FuseLow = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(AVR_FuseLow),4);
	gtk_table_attach(GTK_TABLE(devTableAVR),AVR_FuseLow,0,1,0,1,0,0,5,1);
	AVR_FuseLowWrite = gtk_check_button_new_with_label(strings[I_AT_FUSE]);	//"Write Fuse Low"
	gtk_table_attach(GTK_TABLE(devTableAVR),AVR_FuseLowWrite,1,2,0,1,GTK_FILL,0,5,0);
	AVR_FuseHigh = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(AVR_FuseHigh),4);
	gtk_table_attach(GTK_TABLE(devTableAVR),AVR_FuseHigh,0,1,1,2,0,0,5,1);
	AVR_FuseHighWrite = gtk_check_button_new_with_label(strings[I_AT_FUSEH]);	//"Write Fuse High"
	gtk_table_attach(GTK_TABLE(devTableAVR),AVR_FuseHighWrite,1,2,1,2,GTK_FILL,0,5,0);
	AVR_FuseExt = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(AVR_FuseExt),4);
	gtk_table_attach(GTK_TABLE(devTableAVR),AVR_FuseExt,0,1,2,3,0,0,5,1);
	AVR_FuseExtWrite = gtk_check_button_new_with_label(strings[I_AT_FUSEX]);	//"Write Extended Fuse"
	gtk_table_attach(GTK_TABLE(devTableAVR),AVR_FuseExtWrite,1,2,2,3,GTK_FILL,0,5,0);
	AVR_Lock = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(AVR_Lock),4);
	gtk_table_attach(GTK_TABLE(devTableAVR),AVR_Lock,0,1,3,4,0,0,5,1);
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
	gtk_container_add(GTK_CONTAINER(optHboxErr),GTK_WIDGET(Errors_entry));

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
	gtk_container_add(GTK_CONTAINER(i2cVboxMode),GTK_WIDGET(I2C8bit));
	gtk_container_add(GTK_CONTAINER(i2cVboxMode),GTK_WIDGET(I2C16bit));
	gtk_container_add(GTK_CONTAINER(i2cVboxMode),gtk_hseparator_new());
	gtk_container_add(GTK_CONTAINER(i2cVboxMode),GTK_WIDGET(SPI00));
	gtk_container_add(GTK_CONTAINER(i2cVboxMode),GTK_WIDGET(SPI01));
	gtk_container_add(GTK_CONTAINER(i2cVboxMode),GTK_WIDGET(SPI10));
	gtk_container_add(GTK_CONTAINER(i2cVboxMode),GTK_WIDGET(SPI11));

	GtkWidget * i2cVboxTX = gtk_vbox_new(FALSE,2);
	gtk_table_attach(GTK_TABLE(i2cTable),i2cVboxTX,0,2,4,5,GTK_FILL,0,5,0);
	label = gtk_label_new(strings[I_I2CDATAOUT]);	//"Data to send"
	gtk_container_add(GTK_CONTAINER(i2cVboxTX),label);
	I2CDataSend = gtk_entry_new();
	gtk_container_add(GTK_CONTAINER(i2cVboxTX),GTK_WIDGET(I2CDataSend));

	GtkWidget * i2cVboxRX = gtk_vbox_new(FALSE,2);
	gtk_table_attach(GTK_TABLE(i2cTable),i2cVboxRX,0,2,5,6,GTK_FILL,0,5,0);
	label = gtk_label_new(strings[I_I2CDATATR]);	//"Data transferred"
	gtk_container_add(GTK_CONTAINER(i2cVboxRX),label);
	I2CDataReceive = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(I2CDataReceive),FALSE);
	gtk_container_add(GTK_CONTAINER(i2cVboxRX),GTK_WIDGET(I2CDataReceive));
	gtk_widget_set_size_request(I2CDataReceive,100,60);

	GtkWidget * i2cHboxNB = gtk_hbox_new(FALSE,5);
	gtk_table_attach(GTK_TABLE(i2cTable),i2cHboxNB,1,2,0,1,GTK_FILL,0,5,0);
	label = gtk_label_new(strings[I_I2C_NB]);	//"Byes to read/write"
	gtk_container_add(GTK_CONTAINER(i2cHboxNB),label);
	I2CNbyte = 	gtk_spin_button_new_with_range(0,64,1);
	gtk_container_add(GTK_CONTAINER(i2cHboxNB),GTK_WIDGET(I2CNbyte));

	I2CSendBtn = gtk_button_new_with_label(strings[I_I2CSend]);	//"Send"
	gtk_table_attach(GTK_TABLE(i2cTable),I2CSendBtn,1,2,1,2,GTK_FILL,0,5,0);

	I2CReceiveBtn = gtk_button_new_with_label(strings[I_I2CReceive]);	//"Receive"
	gtk_table_attach(GTK_TABLE(i2cTable),I2CReceiveBtn,1,2,2,3,GTK_FILL,0,5,0);

//------ICD tab-------------
	label = gtk_label_new("ICD");
	icdVbox1 = gtk_vbox_new(FALSE,5);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),icdVbox1,label);
	GtkWidget * loadCoffBtn = gtk_button_new_with_label(strings[I_LOAD_COFF]); //"load COFF file..."
	//menu
	GtkWidget * icdMenuBar = gtk_menu_bar_new ();
	GtkWidget * icdRootMenu = gtk_menu_item_new_with_label(strings[I_Opt]); //"Options";
    gtk_menu_shell_append (GTK_MENU_SHELL (icdMenuBar), icdRootMenu);
	GtkWidget * icdMenu = gtk_menu_new();
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (icdRootMenu), icdMenu);
	icdMenuPC = gtk_check_menu_item_new_with_label(strings[I_SHOW_PC]); //"show Program Counter"
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(icdMenuPC),TRUE);
	icdMenuSTAT = gtk_check_menu_item_new_with_label(strings[I_SHOW_STATUS]); //"show status registers"
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(icdMenuSTAT),TRUE);
	icdMenuBank0 = gtk_check_menu_item_new_with_label(strings[I_SHOW_BANK0]); //"show memory bank 0"
	icdMenuBank1 = gtk_check_menu_item_new_with_label(strings[I_SHOW_BANK1]); //"show memory bank 1"
	icdMenuBank2 = gtk_check_menu_item_new_with_label(strings[I_SHOW_BANK2]); //"show memory bank 2"
	icdMenuBank3 = gtk_check_menu_item_new_with_label(strings[I_SHOW_BANK3]); //"show memory bank 3"
	icdMenuEE = gtk_check_menu_item_new_with_label(strings[I_SHOW_EE]); //"show EEPROM"
	gtk_menu_shell_append (GTK_MENU_SHELL (icdMenu), icdMenuPC);
	gtk_menu_shell_append (GTK_MENU_SHELL (icdMenu), icdMenuSTAT);
	gtk_menu_shell_append (GTK_MENU_SHELL (icdMenu), icdMenuBank0);
	gtk_menu_shell_append (GTK_MENU_SHELL (icdMenu), icdMenuBank1);
	gtk_menu_shell_append (GTK_MENU_SHELL (icdMenu), icdMenuBank2);
	gtk_menu_shell_append (GTK_MENU_SHELL (icdMenu), icdMenuBank3);
	gtk_menu_shell_append (GTK_MENU_SHELL (icdMenu), icdMenuEE);
	//toolbar
	pixbuf = gdk_pixbuf_new_from_inline (-1, go_icon, FALSE, NULL);
	GtkWidget* iconGO = gtk_image_new_from_pixbuf(pixbuf);
	pixbuf = gdk_pixbuf_new_from_inline (-1, halt_icon, FALSE, NULL);
	GtkWidget* iconHALT = gtk_image_new_from_pixbuf(pixbuf);
	pixbuf = gdk_pixbuf_new_from_inline (-1, step_icon, FALSE, NULL);
	GtkWidget* iconSTEP = gtk_image_new_from_pixbuf(pixbuf);
	pixbuf = gdk_pixbuf_new_from_inline (-1, stepover_icon, FALSE, NULL);
	GtkWidget* iconSTEPOVER = gtk_image_new_from_pixbuf(pixbuf);
	pixbuf = gdk_pixbuf_new_from_inline (-1, stop_icon, FALSE, NULL);
	GtkWidget* iconSTOP = gtk_image_new_from_pixbuf(pixbuf);

	GtkWidget * icdtoolbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(icdtoolbar),GTK_TOOLBAR_ICONS);
	gtk_box_pack_start(GTK_BOX(icdVbox1),icdtoolbar,FALSE,FALSE,0);
	gtk_toolbar_append_item(GTK_TOOLBAR(icdtoolbar),"",strings[I_ICD_RUN],"",iconGO,G_CALLBACK(icdRun),0);
	gtk_toolbar_append_item(GTK_TOOLBAR(icdtoolbar),"",strings[I_ICD_HALT],"",iconHALT,G_CALLBACK(icdHalt),0);
	gtk_toolbar_append_item(GTK_TOOLBAR(icdtoolbar),"",strings[I_ICD_STEP],"",iconSTEP,G_CALLBACK(icdStep),0);
	gtk_toolbar_append_item(GTK_TOOLBAR(icdtoolbar),"",strings[I_ICD_STEPOVER],"",iconSTEPOVER,G_CALLBACK(icdStepOver),0);
	gtk_toolbar_append_space(GTK_TOOLBAR(icdtoolbar));
	gtk_toolbar_append_item(GTK_TOOLBAR(icdtoolbar),"",strings[I_ICD_STOP],"",iconSTOP,G_CALLBACK(icdStop),0);
	gtk_toolbar_append_space(GTK_TOOLBAR(icdtoolbar));
	gtk_toolbar_insert_stock(GTK_TOOLBAR(icdtoolbar),GTK_STOCK_REFRESH,strings[I_ICD_REFRESH],"",G_CALLBACK(icdRefresh),0,-1);//"refresh"
	gtk_toolbar_append_space(GTK_TOOLBAR(icdtoolbar));
	gtk_toolbar_insert_widget(GTK_TOOLBAR(icdtoolbar),loadCoffBtn,strings[I_LOAD_COFF],"",-1);//"load COFF"
	gtk_toolbar_append_space(GTK_TOOLBAR(icdtoolbar));
	icdCommand = gtk_entry_new();
	gtk_toolbar_insert_widget(GTK_TOOLBAR(icdtoolbar),icdCommand,strings[I_ICD_CMD],"",-1);//"command-line"
	gtk_toolbar_append_space(GTK_TOOLBAR(icdtoolbar));
	gtk_toolbar_insert_stock(GTK_TOOLBAR(icdtoolbar),GTK_STOCK_HELP,strings[I_ICD_HELP],"",G_CALLBACK(ICDHelp),0,-1);//"help"

	GtkWidget *hpaned = gtk_hpaned_new ();
	gtk_box_pack_start(GTK_BOX(icdVbox1),hpaned,TRUE,TRUE,0);
	gint width,height;
	gtk_window_get_size(GTK_WINDOW(window),&width,&height);
	gtk_paned_set_position(GTK_PANED (hpaned),width/2);
	//source
	GtkWidget * icdVbox2 = gtk_vbox_new(FALSE,5);
	gtk_paned_pack1 (GTK_PANED (hpaned), icdVbox2, TRUE, FALSE);
	label = gtk_label_new(strings[I_ICD_SOURCE]);	//"Source"
	gtk_box_pack_start(GTK_BOX(icdVbox2),label,FALSE,FALSE,0);
	GtkWidget * sourceScroll = gtk_scrolled_window_new(NULL,NULL);
	sourceTxt = gtk_text_view_new();
	sourceBuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(sourceTxt));
	gtk_text_view_set_editable(GTK_TEXT_VIEW(sourceTxt),FALSE);
	gtk_container_add(GTK_CONTAINER(sourceScroll),sourceTxt);
	font_desc = pango_font_description_from_string ("monospace 8");
	gtk_widget_modify_font (sourceTxt, font_desc);
	pango_font_description_free (font_desc);
	gtk_box_pack_start(GTK_BOX(icdVbox2),sourceScroll,TRUE,TRUE,0);
	//status
	GtkWidget * icdVbox3 = gtk_vbox_new(FALSE,5);
	gtk_paned_pack2 (GTK_PANED (hpaned), icdVbox3, TRUE, FALSE);
	GtkWidget * icdHbox2 = gtk_hbox_new(TRUE,20);
	gtk_box_pack_start(GTK_BOX(icdVbox3),icdHbox2,FALSE,FALSE,0);
	label = gtk_label_new(strings[I_ICD_STATUS]);	//"Status"
	gtk_box_pack_start(GTK_BOX(icdHbox2),label,FALSE,FALSE,0);
	gtk_box_pack_start(GTK_BOX(icdHbox2),icdMenuBar,FALSE,FALSE,0);
	GtkWidget * statusScroll = gtk_scrolled_window_new(NULL,NULL);
	statusTxt = gtk_text_view_new();
	statusBuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(statusTxt));
	gtk_text_view_set_editable(GTK_TEXT_VIEW(statusTxt),FALSE);
	gtk_container_add(GTK_CONTAINER(statusScroll),statusTxt);
	font_desc = pango_font_description_from_string ("monospace 8");
	gtk_widget_modify_font (statusTxt, font_desc);
	pango_font_description_free (font_desc);
	gtk_box_pack_start(GTK_BOX(icdVbox3),statusScroll,TRUE,TRUE,0);

//------status bar-------------
	status_bar = gtk_statusbar_new();
	gtk_box_pack_start(GTK_BOX(vbox),status_bar,FALSE,TRUE,0);
	statusID=gtk_statusbar_get_context_id(GTK_STATUSBAR(status_bar),"ID");

	g_signal_connect(G_OBJECT(b_testhw),"clicked",G_CALLBACK(TestHw),window);
	g_signal_connect(G_OBJECT(b_connect),"clicked",G_CALLBACK(Connect),window);
	g_signal_connect(G_OBJECT(I2CReceiveBtn),"clicked",G_CALLBACK(I2cspiR),window);
	g_signal_connect(G_OBJECT(I2CSendBtn),"clicked",G_CALLBACK(I2cspiS),window);
	g_signal_connect(G_OBJECT(loadCoffBtn),"clicked",G_CALLBACK(loadCoff),window);
	g_signal_connect(G_OBJECT(sourceTxt),"button_press_event",G_CALLBACK(source_mouse_event),NULL);
	g_signal_connect(G_OBJECT(statusTxt),"button_press_event",G_CALLBACK(icdStatus_mouse_event),NULL);
	g_signal_connect(G_OBJECT(icdCommand),"key_press_event",G_CALLBACK(icdCommand_key_event),NULL);
	g_signal_connect(G_OBJECT(icdVbox1),"key_press_event",G_CALLBACK(icd_key_event),NULL);
	g_signal_connect(G_OBJECT(devTypeCombo),"changed",G_CALLBACK(FilterDevType),NULL);

	gtk_widget_show_all(window);

//********Init*************
	char text[16];
	sprintf(text,"%04X",vid);
	gtk_entry_set_text(GTK_ENTRY(VID_entry),text);
	sprintf(text,"%04X",pid);
	gtk_entry_set_text(GTK_ENTRY(PID_entry),text);
	sprintf(text,"%d",max_err);
	gtk_entry_set_text(GTK_ENTRY(Errors_entry),text);
	sizeW=0x2400;
	memCODE_W=malloc(sizeW*sizeof(WORD));
	initVar();
	int i;
	for(i=0;i<0x2400;i++) memCODE_W[i]=0x3fff;
	strncpy(LogFileName,strings[S_LogFile],sizeof(LogFileName));
	gtk_combo_box_append_text(GTK_COMBO_BOX(devTypeCombo),"*");
	gtk_combo_box_append_text(GTK_COMBO_BOX(devTypeCombo),"PIC10/12");
	gtk_combo_box_append_text(GTK_COMBO_BOX(devTypeCombo),"PIC16");
	gtk_combo_box_append_text(GTK_COMBO_BOX(devTypeCombo),"PIC18");
	gtk_combo_box_append_text(GTK_COMBO_BOX(devTypeCombo),"PIC24");
	gtk_combo_box_append_text(GTK_COMBO_BOX(devTypeCombo),"PIC30/33");
	gtk_combo_box_append_text(GTK_COMBO_BOX(devTypeCombo),"ATMEL AVR");
	gtk_combo_box_append_text(GTK_COMBO_BOX(devTypeCombo),"EEPROM/FLASH");
	gtk_combo_box_set_active(GTK_COMBO_BOX(devTypeCombo),0);
//	gtk_combo_box_set_active(GTK_COMBO_BOX(devCombo),0);
	gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(devCombo),6);
//	AddDevices();	//populate device list
	DeviceDetected=FindDevice();	//connect to USB programmer
	ProgID();		//get firmware version and reset
	gtk_main();
//******Save ini file******
// only if parameters are changed
	if(strcmp(dev_ini,dev)||vid_ini!=vid||pid_ini!=pid||max_err_ini!=max_err){
		printf(homedir);
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

void Connect(){
	vid=htoi(gtk_entry_get_text(GTK_ENTRY(VID_entry)),4);
	pid=htoi(gtk_entry_get_text(GTK_ENTRY(PID_entry)),4);
	DeviceDetected=FindDevice();
	hvreg=0;
	ProgID();
}

DWORD GetTickCount(){
	struct timeb now;
	ftime(&now);
	return now.time*1000+now.millitm;
}

void I2cspiR()		// I2C/SPI receive
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
	I2CReceive(mode,nbyte,tmpbuf);
}

void I2cspiS() // I2C/SPI send
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
	I2CSend(mode,nbyte,tmpbuf);
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
	else PrintMessage(" (?)\r\n\r\n");
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
	return (i+(bufferI[j+3]&0x2))==2?1:0;
}

void TestHw(GtkWidget *widget,GtkWindow* parent){
	if(DeviceDetected!=1) return;
	StartHVReg(13);
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
			path[0]=0;
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
//	char UsageDescription[256];

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
		//gtk_statusbar_push(status_bar,statusID,strings[S_prog]);
	}
	return MyDeviceDetected;
}
