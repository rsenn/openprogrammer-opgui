/*
 * progAVR.c - algorithms to program the Atmel AVR family of microcontrollers
 * Copyright (C) 2009-2010 Alberto Maccioni
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

//This cannot be executed conditionally on MSVC
//#include "stdafx.h"


//configure for GUI or command-line
#ifdef _MSC_VER
	#define _GUI
	#include "msvc_common.h"
#else
	#define _CMD
#include "common.h"
#endif

#define  LOCK	1
#define  FUSE	2
#define  FUSE_H 4
#define  FUSE_X	8
#define  CAL	16
#define  SLOW	256
#define  RST 0x40

struct AVRID{
	int id;
	char *device;
} AVRLIST[]={
	//1K
	{0x9001,"AT90S1200"},
	{0x9004,"ATtiny11"},
	{0x9004,"ATtiny12"},
	{0x9007,"ATtiny13"},
	//2K
	{0x9101,"AT90S2313"},
	{0x9109,"ATtiny26"},
	{0x910A,"ATtiny2313"},
	{0x910B,"ATtiny24"},
	{0x910C,"ATtiny261"},
	//4K
	{0x9205,"ATmega48"},
	{0x920A,"ATmega48PA"},
	{0x9207,"ATtiny44"},
	{0x9208,"ATtiny461"},
	{0x9209,"ATtiny48"},
	{0x920D,"ATtiny4313"},
	//8K
	{0x9301,"AT90S8515"},
	{0x9303,"AT90S8535"},
	{0x9306,"ATmega8515"},
	{0x9307,"ATmega8"},
	{0x9308,"ATmega8535"},
	{0x930A,"ATmega88"},
	{0x930C,"ATtiny84"},
	{0x930D,"ATtiny861"},
	{0x930F,"ATmega88PA"},
	{0x9311,"ATtiny88"},
	//16K
	{0x9403,"ATmega16"},
	{0x9406,"ATmega168"},
	{0x940A,"ATmega164PA"},
	{0x940B,"ATmega168PA"},
	{0x940F,"ATmega164A"},
	//32K
	{0x950F,"ATmega328P"},
	{0x9511,"ATmega324PA"},
	{0x9514,"ATmega328"},
	{0x9515,"ATmega324A"},
	//64K
	{0x9602,"ATmega64"},
	{0x9609,"ATmega644A"},
	{0x960A,"ATmega644PA"},
	//128K
	{0x9705,"ATmega1284P"},
	{0x9706,"ATmega1284"},
};

#ifdef _MSC_VER
void COpenProgDlg::AtmelID(BYTE id[])
#else
void AtmelID(BYTE id[])
#endif
{
	char str[128]="";
	int i,idw=(id[1]<<8)+id[2];
	if(id[0]==0&&id[1]==1&&id[2]==2){
		PrintMessage(strings[S_Protected]);		//"Device protected"
		return;
	}
	if(id[0]==0x1E) strcat(str,"Atmel ");
	for(i=0;i<sizeof(AVRLIST)/sizeof(AVRLIST[0]);i++){
		if(idw==AVRLIST[i].id){
			strcat(str,AVRLIST[i].device);
			i=1000;
		}
	}
	switch(id[1]){
		case 0x90:
			strcat(str," 1KB Flash");
			break;
		case 0x91:
			strcat(str," 2KB Flash");
			break;
		case 0x92:
			strcat(str," 4KB Flash");
			break;
		case 0x93:
			strcat(str," 8KB Flash");
			break;
		case 0x94:
			strcat(str," 16KB Flash");
			break;
		case 0x95:
			strcat(str," 32KB Flash");
			break;
		case 0x96:
			strcat(str," 64KB Flash");
			break;
		case 0x97:
			strcat(str," 128KB Flash");
			break;
	}
	if(i!=1001) strcat(str,strings[S_nodev]); //"Unknown device\r\n");
	else strcat(str,"\r\n");
	PrintMessage(str);
}

#ifdef _MSC_VER
void COpenProgDlg::DisplayCODEAVR(int dim){
#else
void DisplayCODEAVR(int dim){
#endif
// display AVR CODE memory
	char s[256]="",t[256]="";
	int valid=0,empty=1,i,j;
	char* aux=(char*)malloc((dim/COL+1)*(16+COL*6));
	aux[0]=0;
	s[0]=0;
	for(i=0;i<dim;i+=COL*2){
		valid=0;
		for(j=i;j<i+COL*2&&j<dim;j++){
			sprintf(t,"%02X ",memCODE[j]);
			strcat(s,t);
			if(memCODE[j]<0xff) valid=1;
		}
		if(valid){
			sprintf(t,"%04X: %s\r\n",i,s);
			strcat(aux,t);
			empty=0;
		}
		s[0]=0;
	}
	if(empty) PrintMessage(strings[S_Empty]);	//empty
	else PrintMessage(aux);
	free(aux);
}

#ifdef _MSC_VER
void COpenProgDlg::ReadAT(int dim, int dim2, int options)
#else
void ReadAT(int dim, int dim2, int options)
#endif
// read ATMEL AVR
// dim=FLASH size in bytes, dim2=EEPROM size
// options: LOCK,FUSE,FUSE_H,FUSE_X,CAL
//			SLOW = slow communication
{
	int k=0,k2=0,z=0,i,j;
	BYTE signature[]={0,0,0};
	if(dim>0x20000||dim<0){
		PrintMessage(strings[S_CodeLim]);	//"Code size out of limits\r\n"
		return;
	}
	if(dim2>0x800||dim2<0){
		PrintMessage(strings[S_EELim]);	//"EEPROM size out of limits\r\n"
		return;
	}
	if(saveLog){
		OpenLogFile();	//"Log.txt"
		fprintf(logfile,"ReadAT(0x%X,0x%X,0x%X)\n",dim,dim2,options);
	}
	size=dim;
	sizeEE=dim2;
	if(memCODE) free(memCODE);
	memCODE=(unsigned char*)malloc(dim);		//CODE
	if(memEE) free(memEE);
	memEE=(unsigned char*)malloc(dim2);			//EEPROM
	for(j=0;j<size;j++) memCODE[j]=0xFF;
	for(j=0;j<sizeEE;j++) memEE[j]=0xFF;
	unsigned int start=GetTickCount();
	bufferU[0]=0;
	j=1;
	bufferU[j++]=SET_PARAMETER;
	bufferU[j++]=SET_T3;
	bufferU[j++]=20000>>8;
	bufferU[j++]=20000&0xff;
	bufferU[j++]=VREG_DIS;		//Disable HV reg
	bufferU[j++]=EN_VPP_VCC;	//VDD
	bufferU[j++]=0x0;
	bufferU[j++]=SPI_INIT;
	bufferU[j++]=options&SLOW?0:1;				//0=100k, 1=200k
	bufferU[j++]=CLOCK_GEN;
	bufferU[j++]=3;				//0=100k,200k,500k,1M,2M
	bufferU[j++]=CLOCK_GEN;
	bufferU[j++]=0xFF;
	bufferU[j++]=EXT_PORT;
	bufferU[j++]=0;
	bufferU[j++]=0;
	bufferU[j++]=WAIT_T3;		//20ms
	bufferU[j++]=EN_VPP_VCC;	//VDD
	bufferU[j++]=0x1;
	bufferU[j++]=WAIT_T3;		//20ms
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(50);
	read();
	if(saveLog)WriteLogIO();
	for(i=0;i<32;i++){
		j=1;
		bufferU[j++]=CLOCK_GEN;
		bufferU[j++]=0xFF;
		bufferU[j++]=EXT_PORT;
		bufferU[j++]=0;
		bufferU[j++]=RST;
		bufferU[j++]=EXT_PORT;
		bufferU[j++]=0;
		bufferU[j++]=0;
		bufferU[j++]=CLOCK_GEN;
		bufferU[j++]=4;
		bufferU[j++]=WAIT_T3;		//20ms
		bufferU[j++]=SPI_WRITE;		//Programming enable
		bufferU[j++]=2;
		bufferU[j++]=0xAC;
		bufferU[j++]=0x53;
		bufferU[j++]=SPI_READ;
		bufferU[j++]=2;
		bufferU[j++]=FLUSH;
		for(;j<DIMBUF;j++) bufferU[j]=0x0;
		write();
		msDelay(25);
		read();
		if(saveLog)WriteLogIO();
		for(z=1;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
		if(bufferI[z+2]==0x53) i=32;
	}
	if(i<33){
		j=1;
		bufferU[j++]=EN_VPP_VCC;	//VDD
		bufferU[j++]=0x0;
		bufferU[j++]=SPI_INIT;
		bufferU[j++]=0xFF;
		bufferU[j++]=CLOCK_GEN;
		bufferU[j++]=0xFF;
		bufferU[j++]=EXT_PORT;
		bufferU[j++]=0;
		bufferU[j++]=0;
		bufferU[j++]=FLUSH;
		for(;j<DIMBUF;j++) bufferU[j]=0x0;
		write();
		msDelay(3);
		read();
		if(saveLog)WriteLogIO();
		PrintMessage(strings[S_SyncErr]);	//"Synchronization error\r\n"
		if(saveLog) CloseLogFile();
		return;
	}
	j=1;
	bufferU[j++]=SPI_WRITE;		//Read signature bytes
	bufferU[j++]=3;
	bufferU[j++]=0x30;
	bufferU[j++]=0;
	bufferU[j++]=0;
	bufferU[j++]=SPI_READ;
	bufferU[j++]=1;
	bufferU[j++]=SPI_WRITE;		//Read signature bytes
	bufferU[j++]=3;
	bufferU[j++]=0x30;
	bufferU[j++]=0;
	bufferU[j++]=1;
	bufferU[j++]=SPI_READ;
	bufferU[j++]=1;
	bufferU[j++]=SPI_WRITE;		//Read signature bytes
	bufferU[j++]=3;
	bufferU[j++]=0x30;
	bufferU[j++]=0;
	bufferU[j++]=2;
	bufferU[j++]=SPI_READ;
	bufferU[j++]=1;
	if(options&LOCK){			//LOCK byte
		bufferU[j++]=SPI_WRITE;
		bufferU[j++]=3;
		bufferU[j++]=0x58;
		bufferU[j++]=0;
		bufferU[j++]=0;
		bufferU[j++]=SPI_READ;
		bufferU[j++]=1;
	}
	if(options&FUSE){			//FUSE byte
		bufferU[j++]=SPI_WRITE;
		bufferU[j++]=3;
		bufferU[j++]=0x50;
		bufferU[j++]=0;
		bufferU[j++]=0;
		bufferU[j++]=SPI_READ;
		bufferU[j++]=1;
	}
	if(options&FUSE_H){			//FUSE high byte
		bufferU[j++]=SPI_WRITE;
		bufferU[j++]=3;
		bufferU[j++]=0x58;
		bufferU[j++]=8;
		bufferU[j++]=0;
		bufferU[j++]=SPI_READ;
		bufferU[j++]=1;
	}
	if(options&FUSE_X){			//extended FUSE byte
		bufferU[j++]=SPI_WRITE;
		bufferU[j++]=3;
		bufferU[j++]=0x50;
		bufferU[j++]=8;
		bufferU[j++]=0;
		bufferU[j++]=SPI_READ;
		bufferU[j++]=1;
	}
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(8);
	read();
	if(saveLog)WriteLogIO();
	for(z=1;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
	signature[0]=bufferI[z+2];
	for(z+=3;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
	signature[1]=bufferI[z+2];
	for(z+=3;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
	signature[2]=bufferI[z+2];
	PrintMessage3("CHIP ID:%02X%02X%02X\r\n",signature[0],signature[1],signature[2]);
	AtmelID(signature);
	if(options&LOCK){			//LOCK byte
		for(z+=3;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
		PrintMessage1("LOCK bits:\t  0x%02X\r\n",bufferI[z+2]);
	}
	if(options&FUSE){			//FUSE byte
		for(z+=3;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
		PrintMessage1("FUSE bits:\t  0x%02X\r\n",bufferI[z+2]);
	}
	if(options&FUSE_H){			//FUSE high byte
		for(z+=3;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
		PrintMessage1("FUSE HIGH bits:\t  0x%02X\r\n",bufferI[z+2]);
	}
	if(options&FUSE_X){			//extended FUSE byte
		for(z+=3;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
		PrintMessage1("Extended FUSE bits: 0x%02X\r\n",bufferI[z+2]);
	}
	if(options&CAL){			//calibration byte
		j=1;
		bufferU[j++]=SPI_WRITE;
		bufferU[j++]=3;
		bufferU[j++]=0x38;
		bufferU[j++]=0;
		bufferU[j++]=0;
		bufferU[j++]=SPI_READ;
		bufferU[j++]=1;
		bufferU[j++]=SPI_WRITE;
		bufferU[j++]=3;
		bufferU[j++]=0x38;
		bufferU[j++]=0;
		bufferU[j++]=1;
		bufferU[j++]=SPI_READ;
		bufferU[j++]=1;
		bufferU[j++]=SPI_WRITE;
		bufferU[j++]=3;
		bufferU[j++]=0x38;
		bufferU[j++]=0;
		bufferU[j++]=2;
		bufferU[j++]=SPI_READ;
		bufferU[j++]=1;
		bufferU[j++]=SPI_WRITE;
		bufferU[j++]=3;
		bufferU[j++]=0x38;
		bufferU[j++]=0;
		bufferU[j++]=3;
		bufferU[j++]=SPI_READ;
		bufferU[j++]=1;
		bufferU[j++]=FLUSH;
		for(;j<DIMBUF;j++) bufferU[j]=0x0;
		write();
		msDelay(4);
		read();
		if(saveLog)WriteLogIO();
		for(z=1;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
		PrintMessage1("Calibration bits:\t  0x%02X",bufferI[z+2]);
		for(z+=3;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
		PrintMessage1(",0x%02X",bufferI[z+2]);
		for(z+=3;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
		PrintMessage1(",0x%02X",bufferI[z+2]);
		for(z+=3;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
		PrintMessage1(",0x%02X\r\n",bufferI[z+2]);
	}
//****************** read code ********************
	PrintMessage(strings[S_CodeReading1]);		//read code ...
	PrintStatusSetup();
	int c=(DIMBUF-5)/2;
	for(i=0,j=1;i<dim;i+=c*2){
		bufferU[j++]=AT_READ_DATA;
		bufferU[j++]=i<(dim-2*c)?c:(dim-i)/2;
		bufferU[j++]=i>>9;
		bufferU[j++]=i>>1;
		bufferU[j++]=FLUSH;
		for(;j<DIMBUF;j++) bufferU[j]=0x0;
		write();
		msDelay(options&SLOW?30:15);	//15
		read();
		if(bufferI[1]==AT_READ_DATA){
			for(z=3;z<bufferI[2]*2+3&&z<DIMBUF;z++) memCODE[k++]=bufferI[z];
		}
		PrintStatus(strings[S_CodeReading],i*100/(dim+dim2),i);	//"Reading: %3d%%, add. %03X"
		j=1;
		if(saveLog){
			fprintf(logfile,strings[S_Log7],i,i,k,k);	//"i=%d(0x%X), k=%d(0x%X)\n"
			WriteLogIO();
		}
	}
	PrintStatusEnd();
	if(k!=dim){
		PrintMessage("\r\n");
		PrintMessage2(strings[S_ReadCodeErr2],dim,k);	//"Errore in lettura area programma, richiesti %d byte, letti %d\r\n"
	}
	else PrintMessage(strings[S_Compl]);	//"completed\r\n"
//****************** read eeprom ********************
	if(dim2){
		PrintMessage(strings[S_ReadEE]);		//read EE ...
		PrintStatusSetup();
		for(k2=0,i=0,j=1;i<dim2;i++){
			bufferU[j++]=SPI_WRITE;		//Read eeprom memory
			bufferU[j++]=3;
			bufferU[j++]=0xA0;
			bufferU[j++]=i>>8;
			bufferU[j++]=i;
			bufferU[j++]=SPI_READ;
			bufferU[j++]=1;
			if(j>DIMBUF-9||i==dim-1){
				bufferU[j++]=FLUSH;
				for(;j<DIMBUF;j++) bufferU[j]=0x0;
				write();
				msDelay(4);
				read();
				for(z=1;z<DIMBUF-2;z++){
					if(bufferI[z]==SPI_READ&&bufferI[z+1]==1){
						memEE[k2++]=bufferI[z+2];
						z+=3;
					}
				}
				PrintStatus(strings[S_CodeReading],(i+dim)*100/(dim+dim2),i);	//"Reading: %3d%%, add. %03X"
				j=1;
				if(saveLog){
					fprintf(logfile,strings[S_Log7],i,i,k2,k2);	//"i=%d(0x%X), k=%d(0x%X)\n"
					WriteLogIO();
				}
			}
		}
		PrintStatusEnd();
		if(k2!=dim2){
			PrintMessage("\r\n");
			PrintMessage2(strings[S_ReadEEErr],dim2,k2);	//"Error reading EEPROM area, requested %d bytes, read %d\r\n"
		}
		else PrintMessage(strings[S_Compl]);	//"completed\r\n"
	}
//****************** exit program mode ********************
	bufferU[j++]=CLOCK_GEN;
	bufferU[j++]=0xFF;
	bufferU[j++]=SPI_WRITE;
	bufferU[j++]=1;
	bufferU[j++]=0;
	bufferU[j++]=EN_VPP_VCC;		//VDD
	bufferU[j++]=0;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(1);
	read();
	unsigned int stop=GetTickCount();
	PrintStatusClear();
//****************** visualize ********************
	PrintMessage(strings[S_CodeMem]);	//"\r\nProgram memory\r\n"
	DisplayCODEAVR(dim);
	if(dim2){
		DisplayEE();	//visualize EE
	}
	PrintMessage1(strings[S_End],(stop-start)/1000.0);	//"\r\nEnd (%.2f s)\r\n"
	if(saveLog) CloseLogFile();
}

#ifdef _MSC_VER
void COpenProgDlg::WriteAT(int dim, int dim2)
#else
void WriteAT(int dim, int dim2)
#endif
// write ATMEL micro
// dim=FLASH size in bytes, dim2=EEPROM size
{
	int k=0,z=0,i,j;
	int err=0,ritenta=0,maxtent=0;
	BYTE signature[]={0,0,0};
	if(dim>0x8000||dim<0){
		PrintMessage(strings[S_CodeLim]);	//"Code size out of limits\r\n"
		return;
	}
	if(dim2>0x800||dim2<0){
		PrintMessage(strings[S_EELim]);	//"EEPROM size out of limits\r\n"
		return;
	}
	if(saveLog){
		OpenLogFile();	//"Log.txt"
		fprintf(logfile,"WriteAT(0x%X,0x%X)\n",dim,dim2);
	}
	if(dim>size) dim=size;
	if(dim2>sizeEE) dim2=sizeEE;
	if(dim<1){
		PrintMessage(strings[S_NoCode]);	//"Data area is empty\r\n"
		return;
	}
	unsigned int start=GetTickCount();
	bufferU[0]=0;
	j=1;
	bufferU[j++]=SET_PARAMETER;
	bufferU[j++]=SET_T3;
	bufferU[j++]=20000>>8;
	bufferU[j++]=20000&0xff;
	bufferU[j++]=VREG_DIS;		//Disable HV reg
	bufferU[j++]=EN_VPP_VCC;	//VDD
	bufferU[j++]=0x0;
	bufferU[j++]=SPI_INIT;
	bufferU[j++]=1;
	bufferU[j++]=CLOCK_GEN;
	bufferU[j++]=3;
	bufferU[j++]=CLOCK_GEN;
	bufferU[j++]=0xFF;
	bufferU[j++]=EXT_PORT;
	bufferU[j++]=0;
	bufferU[j++]=0;
	bufferU[j++]=WAIT_T3;		//20ms
	bufferU[j++]=EN_VPP_VCC;	//VDD
	bufferU[j++]=0x1;
	bufferU[j++]=WAIT_T3;		//20ms
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(50);
	read();
	if(saveLog)WriteLogIO();
	for(i=0;i<32;i++){
		j=1;
		bufferU[j++]=CLOCK_GEN;
		bufferU[j++]=0xFF;
		bufferU[j++]=EXT_PORT;
		bufferU[j++]=0;
		bufferU[j++]=RST;
		bufferU[j++]=EXT_PORT;
		bufferU[j++]=0;
		bufferU[j++]=0;
		bufferU[j++]=CLOCK_GEN;
		bufferU[j++]=4;
		bufferU[j++]=WAIT_T3;		//20ms
		bufferU[j++]=SPI_WRITE;		//Programming enable
		bufferU[j++]=2;
		bufferU[j++]=0xAC;
		bufferU[j++]=0x53;
		bufferU[j++]=SPI_READ;
		bufferU[j++]=2;
		bufferU[j++]=FLUSH;
		for(;j<DIMBUF;j++) bufferU[j]=0x0;
		write();
		msDelay(25);
		read();
		if(saveLog)WriteLogIO();
		for(z=1;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
		//PrintMessage("i=%d z=%d   rx:%02X%02X\r\n",i,z,bufferI[z+2],bufferI[z+3]);
		if(bufferI[z+2]==0x53) i=32;
	}
	if(i<33){
		j=1;
		bufferU[j++]=EN_VPP_VCC;	//VDD
		bufferU[j++]=0x0;
		bufferU[j++]=SPI_INIT;
		bufferU[j++]=0xFF;
		bufferU[j++]=CLOCK_GEN;
		bufferU[j++]=0xFF;
		bufferU[j++]=EXT_PORT;
		bufferU[j++]=0;
		bufferU[j++]=0;
		bufferU[j++]=FLUSH;
		for(;j<DIMBUF;j++) bufferU[j]=0x0;
		write();
		msDelay(1);
		read();
		if(saveLog)WriteLogIO();
		PrintMessage(strings[S_SyncErr]);	//"Synchronization error\r\n"
		if(saveLog) CloseLogFile();
		return;
	}
	j=1;
	bufferU[j++]=SPI_WRITE;		//Read signature bytes
	bufferU[j++]=3;
	bufferU[j++]=0x30;
	bufferU[j++]=0;
	bufferU[j++]=0;
	bufferU[j++]=SPI_READ;
	bufferU[j++]=1;
	bufferU[j++]=SPI_WRITE;		//Read signature bytes
	bufferU[j++]=3;
	bufferU[j++]=0x30;
	bufferU[j++]=0;
	bufferU[j++]=1;
	bufferU[j++]=SPI_READ;
	bufferU[j++]=1;
	bufferU[j++]=SPI_WRITE;		//Read signature bytes
	bufferU[j++]=3;
	bufferU[j++]=0x30;
	bufferU[j++]=0;
	bufferU[j++]=2;
	bufferU[j++]=SPI_READ;
	bufferU[j++]=1;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(2);
	read();
	if(saveLog)WriteLogIO();
	for(z=1;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
	signature[0]=bufferI[z+2];
	for(z+=3;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
	signature[1]=bufferI[z+2];
	for(z+=3;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
	signature[2]=bufferI[z+2];
	PrintMessage3("CHIP ID:%02X%02X%02X\r\n",signature[0],signature[1],signature[2]);
	AtmelID(signature);
//****************** erase memory ********************
	PrintMessage(strings[S_StartErase]);	//"Erase ... "
	j=1;
	bufferU[j++]=SPI_WRITE;		//Chip erase
	bufferU[j++]=4;
	bufferU[j++]=0xAC;
	bufferU[j++]=0x80;
	bufferU[j++]=0;
	bufferU[j++]=0;
	bufferU[j++]=SET_PARAMETER;
	bufferU[j++]=SET_T3;
	bufferU[j++]=2000>>8;
	bufferU[j++]=2000&0xff;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(15);
	read();
	if(saveLog)WriteLogIO();
	PrintMessage(strings[S_Compl]);	//"completed\r\n"
//****************** write code ********************
	PrintMessage(strings[S_StartCodeProg]);	//"Write code ... "
	PrintStatusSetup();
	for(i=0,j=1;i<dim;i++){
		if(memCODE[i]!=0xFF){
			bufferU[j++]=SPI_WRITE;		//Write program memory
			bufferU[j++]=4;
			bufferU[j++]=0x40+(i&1?8:0);
			bufferU[j++]=i>>9;
			bufferU[j++]=i>>1;
			bufferU[j++]=memCODE[i];
			bufferU[j++]=WAIT_T3;		//6ms
			bufferU[j++]=WAIT_T3;
			bufferU[j++]=WAIT_T3;
			bufferU[j++]=SPI_WRITE;		//Read program memory
			bufferU[j++]=3;
			bufferU[j++]=0x20+(i&1?8:0);
			bufferU[j++]=i>>9;
			bufferU[j++]=i>>1;
			bufferU[j++]=SPI_READ;
			bufferU[j++]=1;
			bufferU[j++]=FLUSH;
			for(;j<DIMBUF;j++) bufferU[j]=0x0;
			j=1;
			write();
			msDelay(9);
			read();
			PrintStatus(strings[S_CodeWriting],i*100/dim,i);	//"Write: %d%%, addr. %03X"
			for(z=1;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
			if(z==DIMBUF-2||memCODE[i]!=bufferI[z+2]){
				if(ritenta<5){
					ritenta++;
					if (ritenta>maxtent) maxtent=ritenta;
					i--;
				}
				else{
					err++;
					ritenta=0;
				}
			}
			if(max_err&&err>max_err){
				PrintMessage1(strings[S_MaxErr],err);	//"Exceeded maximum number of errors (%d), write interrupted\r\n"
				PrintMessage(strings[S_IntW]);	//"Write interrupted"
				i=dim;
			}
			if(saveLog){
				fprintf(logfile,strings[S_Log8],i,i,k,k,err);	//"i=%d, k=%d, err=%d\n"
				WriteLogIO();
			}
		}
	}
	PrintStatusEnd();
	PrintMessage1(strings[S_ComplErr],err);	//"completed, %d errors\r\n"
//****************** write eeprom ********************
	if(dim2){
		PrintMessage(strings[S_EEAreaW]);	//"Write EEPROM ... "
		PrintStatusSetup();
		int errEE=0;
		for(i=0,j=1;i<dim2;i++){
			if(memEE[i]!=0xFF){
				bufferU[j++]=SPI_WRITE;		//Write EEPROM memory
				bufferU[j++]=4;
				bufferU[j++]=0xC0;
				bufferU[j++]=i>>8;
				bufferU[j++]=i;
				bufferU[j++]=memEE[i];
				bufferU[j++]=WAIT_T3;		//6ms
				bufferU[j++]=WAIT_T3;
				bufferU[j++]=WAIT_T3;
				bufferU[j++]=SPI_WRITE;		//Read EEPROM memory
				bufferU[j++]=3;
				bufferU[j++]=0xA0;
				bufferU[j++]=i>>8;
				bufferU[j++]=i;
				bufferU[j++]=SPI_READ;
				bufferU[j++]=1;
				bufferU[j++]=FLUSH;
				for(;j<DIMBUF;j++) bufferU[j]=0x0;
				j=1;
				write();
				msDelay(9);
				read();
				PrintStatus(strings[S_CodeWriting],i*100/dim2,i);	//"Write: %d%%, addr. %03X"
				for(z=1;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
				if(z==DIMBUF-2||memEE[i]!=bufferI[z+2]){
					if(ritenta<10){
						ritenta++;
						if (ritenta>maxtent) maxtent=ritenta;
						i--;
					}
					else{
						errEE++;
						ritenta=0;
					}
				}
				if(max_err&&err+errEE>max_err){
					PrintMessage1(strings[S_MaxErr],err+errEE);	//"Exceeded maximum number of errors (%d), write interrupted\r\n"
					PrintMessage(strings[S_IntW]);	//"Write interrupted"
					i=dim2;
				}
				if(saveLog){
					fprintf(logfile,strings[S_Log8],i,i,k,k,errEE);	//"i=%d, k=%d, err=%d\n"
					WriteLogIO();
				}
			}
		}
		PrintStatusEnd();
		PrintMessage1(strings[S_ComplErr],errEE);	//"completed, %d errors\r\n"
		err+=errEE;
	}
	if(maxtent) PrintMessage1(strings[S_MaxRetry],maxtent); 	//"Max retries in writing: %d\r\n"
//****************** write FUSE ********************
	if(AVRlock<0x100){
		PrintMessage(strings[S_FuseAreaW]);	//"Write Fuse ... "
		bufferU[j++]=SPI_WRITE;		//Write lock
		bufferU[j++]=4;
		bufferU[j++]=0xAC;
		bufferU[j++]=0xF9+(AVRlock&0x06);
		bufferU[j++]=0;
		bufferU[j++]=0;
		bufferU[j++]=WAIT_T3;		//9ms
		bufferU[j++]=FLUSH;
		for(;j<DIMBUF;j++) bufferU[j]=0x0;
		j=1;
		write();
		msDelay(9);
		read();
		if(saveLog)WriteLogIO();
		PrintMessage(strings[S_Compl]);	//"completed\r\n"
	}
//****************** exit program mode ********************
	bufferU[j++]=CLOCK_GEN;
	bufferU[j++]=0xFF;
	bufferU[j++]=EN_VPP_VCC;		//VDD
	bufferU[j++]=0;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(1);
	read();
	unsigned int stop=GetTickCount();
	PrintMessage3(strings[S_EndErr],(stop-start)/1000.0,err,err!=1?strings[S_ErrPlur]:strings[S_ErrSing]);	//"\r\nEnd (%.2f s) %d %s\r\n\r\n"
	if(saveLog)	CloseLogFile();
	PrintStatusClear();
}

#ifdef _MSC_VER
void COpenProgDlg::WriteATmega(int dim, int dim2, int page, int options)
#else
void WriteATmega(int dim, int dim2, int page, int options)
#endif
// write ATMEL micro
// dim=FLASH size in bytes, dim2=EEPROM, page=FLASH page size in bytes
// options: SLOW=slow communication
{
	int k=0,z=0,i,j;
	int err=0,ritenta=0,maxtent=0;
	BYTE signature[]={0,0,0};
	if(dim>0x10000||dim<0){
		PrintMessage(strings[S_CodeLim]);	//"Code size out of limits\r\n"
		return;
	}
	if(dim2>0x1000||dim2<0){
		PrintMessage(strings[S_EELim]);	//"EEPROM size out of limits\r\n"
		return;
	}
	if(saveLog){
		OpenLogFile();	//"Log.txt"
		fprintf(logfile,"WriteATmega(0x%X,0x%X,0x%X)\n",dim,dim2,page);
	}
	if(dim>size) dim=size;
	else{
		size=dim;
		memCODE=(unsigned char*)realloc(memCODE,dim);
	}
	if(size%(page*2)){	//grow to an integer number of pages
		j=size;
		dim=(j/(page*2)+1)*page*2;
		memCODE=(unsigned char*)realloc(memCODE,dim);
		for(;j<dim;j++) memCODE[j]=0xFF;
	}
	if(dim2>sizeEE) dim2=sizeEE;
	if(dim<1){
		PrintMessage(strings[S_NoCode]);	//"Data area is empty\r\n"
		return;
	}
	unsigned int start=GetTickCount();
	bufferU[0]=0;
	j=1;
	bufferU[j++]=SET_PARAMETER;
	bufferU[j++]=SET_T3;
	bufferU[j++]=20000>>8;
	bufferU[j++]=20000&0xff;
	bufferU[j++]=VREG_DIS;		//Disable HV reg
	bufferU[j++]=EN_VPP_VCC;	//VDD
	bufferU[j++]=0x0;
	bufferU[j++]=SPI_INIT;
	bufferU[j++]=options&SLOW?0:1;				//0=100k, 1=200k
	bufferU[j++]=CLOCK_GEN;
	bufferU[j++]=3;
	bufferU[j++]=CLOCK_GEN;
	bufferU[j++]=0xFF;
	bufferU[j++]=EXT_PORT;
	bufferU[j++]=0;
	bufferU[j++]=0;
	bufferU[j++]=WAIT_T3;		//20ms
	bufferU[j++]=EN_VPP_VCC;	//VDD
	bufferU[j++]=0x1;
	bufferU[j++]=WAIT_T3;		//20ms
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(50);
	read();
	if(saveLog)WriteLogIO();
	for(i=0;i<32;i++){
		j=1;
		bufferU[j++]=CLOCK_GEN;
		bufferU[j++]=0xFF;
		bufferU[j++]=EXT_PORT;
		bufferU[j++]=0;
		bufferU[j++]=RST;
		bufferU[j++]=EXT_PORT;
		bufferU[j++]=0;
		bufferU[j++]=0;
		bufferU[j++]=CLOCK_GEN;
		bufferU[j++]=4;
		bufferU[j++]=WAIT_T3;		//20ms
		bufferU[j++]=SPI_WRITE;		//Programming enable
		bufferU[j++]=2;
		bufferU[j++]=0xAC;
		bufferU[j++]=0x53;
		bufferU[j++]=SPI_READ;
		bufferU[j++]=2;
		bufferU[j++]=FLUSH;
		for(;j<DIMBUF;j++) bufferU[j]=0x0;
		write();
		msDelay(25);
		read();
		if(saveLog)WriteLogIO();
		for(z=1;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
		//PrintMessage("i=%d z=%d   rx:%02X%02X\r\n",i,z,bufferI[z+2],bufferI[z+3]);
		//PrintMessage(str);
		if(bufferI[z+2]==0x53) i=32;
	}
	if(i<33){
		j=1;
		bufferU[j++]=EN_VPP_VCC;	//VDD
		bufferU[j++]=0x0;
		bufferU[j++]=SPI_INIT;
		bufferU[j++]=0xFF;
		bufferU[j++]=CLOCK_GEN;
		bufferU[j++]=0xFF;
		bufferU[j++]=EXT_PORT;
		bufferU[j++]=0;
		bufferU[j++]=0;
		bufferU[j++]=FLUSH;
		for(;j<DIMBUF;j++) bufferU[j]=0x0;
		write();
		msDelay(2);
		read();
		if(saveLog)WriteLogIO();
		PrintMessage(strings[S_SyncErr]);	//"Synchronization error\r\n"
		if(saveLog) CloseLogFile();
		return;
	}
	j=1;
	bufferU[j++]=SPI_WRITE;		//Read signature bytes
	bufferU[j++]=3;
	bufferU[j++]=0x30;
	bufferU[j++]=0;
	bufferU[j++]=0;
	bufferU[j++]=SPI_READ;
	bufferU[j++]=1;
	bufferU[j++]=SPI_WRITE;		//Read signature bytes
	bufferU[j++]=3;
	bufferU[j++]=0x30;
	bufferU[j++]=0;
	bufferU[j++]=1;
	bufferU[j++]=SPI_READ;
	bufferU[j++]=1;
	bufferU[j++]=SPI_WRITE;		//Read signature bytes
	bufferU[j++]=3;
	bufferU[j++]=0x30;
	bufferU[j++]=0;
	bufferU[j++]=2;
	bufferU[j++]=SPI_READ;
	bufferU[j++]=1;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(2);
	read();
	if(saveLog)WriteLogIO();
	for(z=1;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
	signature[0]=bufferI[z+2];
	for(z+=3;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
	signature[1]=bufferI[z+2];
	for(z+=3;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
	signature[2]=bufferI[z+2];
	PrintMessage3("CHIP ID:%02X%02X%02X\r\n",signature[0],signature[1],signature[2]);
	AtmelID(signature);
//****************** erase memory ********************
	PrintMessage(strings[S_StartErase]);	//"Erase ... "
	j=1;
	bufferU[j++]=SPI_WRITE;		//Chip erase
	bufferU[j++]=4;
	bufferU[j++]=0xAC;
	bufferU[j++]=0x80;
	bufferU[j++]=0;
	bufferU[j++]=0;
	bufferU[j++]=SET_PARAMETER;
	bufferU[j++]=SET_T3;
	bufferU[j++]=5000>>8;
	bufferU[j++]=5000&0xff;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(15);
	read();
	if(saveLog)WriteLogIO();
	PrintMessage(strings[S_Compl]);	//"completed\r\n"
//****************** write code ********************
	PrintMessage(strings[S_StartCodeProg]);	//"Write code ... "
	PrintStatusSetup();
	int w=0,v,c,Rtry;
	for(i=0;i<dim;i+=page*2){
		for(z=i,v=0;z<i+page*2;z++) if(memCODE[z]<0xFF)v=1;
		if(v){
			for(k=0,j=1,v=0;k<page;k+=w){
				w=(page-k)<(DIMBUF-6)/2?(page-k):(DIMBUF-6)/2;
				bufferU[j++]=AT_LOAD_DATA;
				bufferU[j++]=w;
				bufferU[j++]=k>>8;
				bufferU[j++]=k;
				for(z=0;z<w*2;z++)	bufferU[j++]=memCODE[i+k*2+z];
				bufferU[j++]=FLUSH;
				for(;j<DIMBUF;j++) bufferU[j]=0x0;
				j=1;
				write();
				msDelay(options&SLOW?30:15);
				read();
				if(saveLog)WriteLogIO();
			}
			bufferU[j++]=SPI_WRITE;		//Write program memory page
			bufferU[j++]=4;
			bufferU[j++]=0x4C;
			bufferU[j++]=i>>9;
			bufferU[j++]=i>>1;
			bufferU[j++]=0;
			bufferU[j++]=WAIT_T3;		//5ms
			bufferU[j++]=FLUSH;
			for(;j<DIMBUF;j++) bufferU[j]=0x0;
			j=1;
			write();
			msDelay(10);
			read();
			PrintStatus(strings[S_CodeWriting],i*100/dim,i);	//"Write: %d%%, addr. %03X"
			if(saveLog)WriteLogIO();
			c=(DIMBUF-5)/2;
			for(k=0,j=1;k<page;k+=c){
				for(Rtry=0;Rtry<5;Rtry++){		//Try to read a few times
					bufferU[j++]=AT_READ_DATA;
					bufferU[j++]=k<(page-c)?c:page-k;
					bufferU[j++]=(i+k*2)>>9;
					bufferU[j++]=(i+k*2)>>1;
					bufferU[j++]=FLUSH;
					for(;j<DIMBUF;j++) bufferU[j]=0x0;
					write();
					msDelay(options&SLOW?30:15);	//25
					read();
					if(saveLog)WriteLogIO();
					if(bufferI[1]==AT_READ_DATA){
						for(w=0,z=3;z<bufferI[2]*2+3&&z<DIMBUF;z++){
							if(memCODE[i+k*2+w]!=bufferI[z]){
								if(Rtry<4)	z=DIMBUF;
								else err++;
							}
							w++;
						}
						if(z<DIMBUF) Rtry=100;
					}
					j=1;
				}
			}
			if(saveLog){
				fprintf(logfile,strings[S_Log8],i,i,w,w,err);	//"i=%d, k=%d, err=%d\n"
			}
			if(max_err&&err>max_err){
				PrintMessage1(strings[S_MaxErr],err);	//"Exceeded maximum number of errors (%d), write interrupted\r\n"
				PrintMessage(strings[S_IntW]);	//"Write interrupted"
				i=dim;
			}
		}
	}
	PrintStatusEnd();
	PrintMessage1(strings[S_ComplErr],err);	//"completed, %d errors\r\n"
//****************** write eeprom ********************
	if(dim2){
		PrintMessage(strings[S_EEAreaW]);	//"Write EEPROM ... "
		PrintStatusSetup();
		int errEE=0;
		for(i=0,j=1;i<dim2;i++){
			if(memEE[i]!=0xFF){
				bufferU[j++]=SPI_WRITE;		//Write EEPROM memory
				bufferU[j++]=4;
				bufferU[j++]=0xC0;
				bufferU[j++]=i>>8;
				bufferU[j++]=i;
				bufferU[j++]=memEE[i];
				bufferU[j++]=WAIT_T3;		//5ms
				bufferU[j++]=WAIT_T3;		//5ms
				bufferU[j++]=SPI_WRITE;		//Read EEPROM memory
				bufferU[j++]=3;
				bufferU[j++]=0xA0;
				bufferU[j++]=i>>8;
				bufferU[j++]=i;
				bufferU[j++]=SPI_READ;
				bufferU[j++]=1;
				bufferU[j++]=FLUSH;
				for(;j<DIMBUF;j++) bufferU[j]=0x0;
				j=1;
				write();
				msDelay(12);
				read();
				PrintStatus(strings[S_CodeWriting],i*100/dim2,i);	//"Write: %d%%, addr. %03X"
				for(z=1;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
				if(z==DIMBUF-2||memEE[i]!=bufferI[z+2]){
					if(ritenta<4){
						ritenta++;
						if (ritenta>maxtent) maxtent=ritenta;
						i--;
					}
					else{
						errEE++;
						ritenta=0;
					}
				}
				if(max_err&&err+errEE>max_err){
					PrintMessage1(strings[S_MaxErr],err+errEE);	//"Exceeded maximum number of errors (%d), write interrupted\r\n"
					PrintMessage(strings[S_IntW]);	//"Write interrupted"
					i=dim2;
				}
				if(saveLog){
					fprintf(logfile,strings[S_Log8],i,i,k,k,errEE);	//"i=%d, k=%d, err=%d\n"
					WriteLogIO();
				}
			}
		}
		PrintStatusEnd();
		PrintMessage1(strings[S_ComplErr],errEE);	//"completed, %d errors\r\n"
		err+=errEE;
	}
//****************** write FUSE ********************
	int err_f=0;
	if(AVRlock<0x100||AVRfuse<0x100||AVRfuse_h<0x100||AVRfuse_x<0x100)PrintMessage(strings[S_FuseAreaW]);	//"Write Fuse ... "
	if(AVRfuse<0x100){
		bufferU[j++]=SPI_WRITE;		//Write fuse
		bufferU[j++]=4;
		bufferU[j++]=0xAC;
		bufferU[j++]=0xA0;
		bufferU[j++]=0;
		bufferU[j++]=AVRfuse;
		bufferU[j++]=WAIT_T3;		//9ms
		bufferU[j++]=SPI_WRITE;
		bufferU[j++]=3;
		bufferU[j++]=0x50;
		bufferU[j++]=0;
		bufferU[j++]=0;
		bufferU[j++]=SPI_READ;
		bufferU[j++]=1;
		bufferU[j++]=FLUSH;
		for(;j<DIMBUF;j++) bufferU[j]=0x0;
		j=1;
		write();
		msDelay(9);
		read();
		if(saveLog)WriteLogIO();
		for(z=1;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
		if(z==DIMBUF-2||AVRfuse!=bufferI[z+2]) err_f++;
	}
	if(AVRfuse_h<0x100){
		bufferU[j++]=SPI_WRITE;		//Write fuse_h
		bufferU[j++]=4;
		bufferU[j++]=0xAC;
		bufferU[j++]=0xA8;
		bufferU[j++]=0;
		bufferU[j++]=AVRfuse_h;
		bufferU[j++]=WAIT_T3;		//9ms
		bufferU[j++]=SPI_WRITE;
		bufferU[j++]=3;
		bufferU[j++]=0x58;
		bufferU[j++]=8;
		bufferU[j++]=0;
		bufferU[j++]=SPI_READ;
		bufferU[j++]=1;
		bufferU[j++]=FLUSH;
		for(;j<DIMBUF;j++) bufferU[j]=0x0;
		j=1;
		write();
		msDelay(9);
		read();
		if(saveLog)WriteLogIO();
		for(z=1;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
		if(z==DIMBUF-2||AVRfuse_h!=bufferI[z+2]) err_f++;
	}
	if(AVRfuse_x<0x100){
		bufferU[j++]=SPI_WRITE;		//Write ext fuse
		bufferU[j++]=4;
		bufferU[j++]=0xAC;
		bufferU[j++]=0xA4;
		bufferU[j++]=0;
		bufferU[j++]=AVRfuse_x;
		bufferU[j++]=WAIT_T3;		//9ms
		bufferU[j++]=SPI_WRITE;
		bufferU[j++]=3;
		bufferU[j++]=0x50;
		bufferU[j++]=8;
		bufferU[j++]=0;
		bufferU[j++]=SPI_READ;
		bufferU[j++]=1;
		bufferU[j++]=FLUSH;
		for(;j<DIMBUF;j++) bufferU[j]=0x0;
		j=1;
		write();
		msDelay(9);
		read();
		if(saveLog)WriteLogIO();
		for(z=1;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
		if(z==DIMBUF-2||AVRfuse_x!=bufferI[z+2]) err_f++;
	}
	if(AVRlock<0x100){
		bufferU[j++]=SPI_WRITE;		//Write lock
		bufferU[j++]=4;
		bufferU[j++]=0xAC;
		bufferU[j++]=0xE0;
		bufferU[j++]=0;
		bufferU[j++]=AVRlock;
		bufferU[j++]=WAIT_T3;		//9ms
		bufferU[j++]=SPI_WRITE;
		bufferU[j++]=3;
		bufferU[j++]=0x58;
		bufferU[j++]=0;
		bufferU[j++]=0;
		bufferU[j++]=SPI_READ;
		bufferU[j++]=1;
		bufferU[j++]=FLUSH;
		for(;j<DIMBUF;j++) bufferU[j]=0x0;
		j=1;
		write();
		msDelay(9);
		read();
		if(saveLog)WriteLogIO();
		for(z=1;z<DIMBUF-2&&bufferI[z]!=SPI_READ;z++);
		if(z==DIMBUF-2||AVRlock!=bufferI[z+2]) err_f++;
	}
	err+=err_f;
	if(AVRlock<0x100||AVRfuse<0x100||AVRfuse_h<0x100||AVRfuse_x<0x100){
		PrintMessage1(strings[S_ComplErr],err_f);	//"completed, %d errors\r\n"
	}
//	if(maxtent) PrintMessage(strings[S_MaxRetry],maxtent); 	//"Max retries in writing: %d\r\n"
//****************** exit program mode ********************
	bufferU[j++]=CLOCK_GEN;
	bufferU[j++]=0xFF;
	bufferU[j++]=SPI_WRITE;
	bufferU[j++]=1;
	bufferU[j++]=0;
	bufferU[j++]=EN_VPP_VCC;		//VDD
	bufferU[j++]=0;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(1);
	read();
	unsigned int stop=GetTickCount();
	PrintMessage3(strings[S_EndErr],(stop-start)/1000.0,err,err!=1?strings[S_ErrPlur]:strings[S_ErrSing]);	//"\r\nEnd (%.2f s) %d %s\r\n\r\n"
	if(saveLog)CloseLogFile();
	PrintStatusClear();
}

