//General routines to communicate via ICD with a target
#include "common.h"

extern int saveLog;
extern double Tcom;
extern int running;



//The following commands are implemented in the debugger monitor
//routine which is written in the last memory page on the target chip.
#define VER 	1	//;version
#define STEP 	2	//;step
#define GO 		3	//;go
#define RREG 	4	//;read register
#define WREG 	5	//;write register
#define EEADR 0x10D
#define EEADRH 0x10F
#define EEDATA 0x10C
#define EEDATH 0x10E
#define EECON1 0x18C
#define EECON2 0x18D
#define w_temp 0x6B
#define status_temp 0x6C
#define pclath_temp 0x6D
#define fsr_temp 0x6E

struct var{	char* name;	int display;} variables[0x200];

//Prepare ICD interface by resetting the target with a power-up sequence.
//MCLR is low so the target is reset even if power is not supplied by the programmer.
//Set communication speed at 1/(2*Tck us)
void startICD(int Tck){
	bufferU[0]=0;
	int j=1;
	bufferU[j++]=PROG_RST;
	bufferU[j++]=SET_PARAMETER;
	bufferU[j++]=SET_T1T2;
	bufferU[j++]=Tck;				//T1=XXu
	bufferU[j++]=100;				//T2=100u
	bufferU[j++]=SET_PARAMETER;
	bufferU[j++]=SET_T3;			//2ms
	bufferU[j++]=2000>>8;
	bufferU[j++]=2000&0xff;
	bufferU[j++]=VREG_DIS;			//disable HV regulator
	bufferU[j++]=EN_VPP_VCC;		// reset target
	bufferU[j++]=0x0;
	bufferU[j++]=WAIT_T3;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x2;				//set D as input
	bufferU[j++]=EN_VPP_VCC;		// power-up
	bufferU[j++]=0x1;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(3);
	read();
	if(saveLog)WriteLogIO();
}

//Check whether the target is running or is executing the debug routine.
//This is signaled by RB7 (Data): D=1 -> debugger monitor running
int isRunning(){
	int z,j=1;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x0;		//D=0
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x2;		//set D as input
	bufferU[j++]=READ_PINS;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(2);
	read();
	if(saveLog)WriteLogIO();
	for(z=0;z<DIMBUF-1&&bufferI[z]!=READ_PINS;z++);
	if(bufferI[z+1]&1) return 0;
	else return 1;
}

//Set the next breakpoint address, the freeze bit, 
//and continue execution.
//This is necessary because at every break 
//the ICD register is loaded with the last address.
void cont(int break_addr, int freeze){
	int j=1;
	//set breakpoint and freeze
	break_addr&=0x1FFF;
	bufferU[j++]=TX16;
	bufferU[j++]=0x2;
	bufferU[j++]=WREG;		//write register
	bufferU[j++]=(break_addr>>8)+(freeze?0x40:0);
	bufferU[j++]=0x1;
	bufferU[j++]=0x8E;
	bufferU[j++]=TX16;
	bufferU[j++]=0x2;
	bufferU[j++]=WREG;		//write register
	bufferU[j++]=break_addr&0xFF;
	bufferU[j++]=0x1;
	bufferU[j++]=0x8F;
	bufferU[j++]=TX16;
	bufferU[j++]=0x1;
	bufferU[j++]=GO;		//GO
	bufferU[j++]=0;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x2;		//set D as input
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(1+5*Tcom);
	read();
	if(saveLog)WriteLogIO();
	running=1;
}

//Execute a single step
void step(){
	int j=1;
	bufferU[j++]=TX16;
	bufferU[j++]=0x1;
	bufferU[j++]=STEP;		//single step
	bufferU[j++]=0;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x2;		//set D as input
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(1+Tcom);
	read();
	if(saveLog)WriteLogIO();
}

//Remove reset so that the target can start executing its code.
void run(){
	int j=1;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x2;
	bufferU[j++]=EN_VPP_VCC;		//MCLR=H
	bufferU[j++]=0x5;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(1);
	read();
	if(saveLog)WriteLogIO();
	running=1;
}

//Get the debugger monitor version
int version(){
	int j=1,z;
	bufferU[j++]=TX16;
	bufferU[j++]=0x1;
	bufferU[j++]=VER;		//version
	bufferU[j++]=0;
	bufferU[j++]=RX16;
	bufferU[j++]=0x1;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(1+2*Tcom);
	read();
	if(saveLog)WriteLogIO();
	for(z=0;z<DIMBUF-2&&bufferI[z]!=RX16;z++);
	return bufferI[z+3];
}

//Halt execution by setting RB6 (Clock) low
void Halt(){
	int j=1;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x6;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x2;		//set D as input
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(2);
	read();
	if(saveLog)WriteLogIO();
	running=0;
	//printf("halted\n");
}

//Read register at address addr
int ReadRegister(int addr){
	int j=1,z;
	bufferU[j++]=TX16;
	bufferU[j++]=0x2;
	bufferU[j++]=RREG;		//Read register
	bufferU[j++]=0x1;		//1 byte
	bufferU[j++]=(addr>>8)&0xFF;
	bufferU[j++]=addr&0xFF;
	bufferU[j++]=RX16;
	bufferU[j++]=0x1;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x2;		//set D as input
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(1+3*Tcom);
	read();
	if(saveLog)WriteLogIO();
	for(z=0;z<DIMBUF-2&&bufferI[z]!=RX16;z++);
	return bufferI[z+3];	
}

//Read n registers starting at address addr
int ReadRegisterN(int addr,int n,int* buf){
	int i,j=1,z,w;
	for(i=0;i<n;i+=w){
		w=i+(DIMBUF-9)/2<n?(DIMBUF-9)/2:n-i;
		bufferU[j++]=TX16;
		bufferU[j++]=0x2;
		bufferU[j++]=RREG;				//Read register
		bufferU[j++]=w;
		bufferU[j++]=(addr+i)>>8;
		bufferU[j++]=(addr+i)&0xFF;
		bufferU[j++]=RX16;
		bufferU[j++]=w;
		bufferU[j++]=FLUSH;
		for(;j<DIMBUF;j++) bufferU[j]=0x0;
		write();
		msDelay(1+(w+2)*Tcom);
		read();
		if(saveLog)WriteLogIO();
		for(z=0;z<DIMBUF-2&&bufferI[z]!=RX16;z++);
		for(j=0;j<w;j++) buf[i+j]=bufferI[z+3+j*2];
		j=1;
	}
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x2;		//set D as input
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(1);
	read();
	return i==n?0:-1;
}

//Write data at address addr
void WriteRegister(int addr,int data){
	int j=1;
	bufferU[j++]=TX16;
	bufferU[j++]=0x2;
	bufferU[j++]=WREG;		//write register
	bufferU[j++]=data&0xFF;
	bufferU[j++]=(addr>>8)&0xFF;
	bufferU[j++]=addr&0xFF;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x2;		//set D as input
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	write();
	msDelay(1+2*Tcom);
	read();	
	if(saveLog)WriteLogIO();
}

//Read program memory at address addr
int ReadProgMem(int addr){
	int addr_temp, data_temp, eecon_temp,data;
	addr_temp=(ReadRegister(EEADRH)<<8)+ReadRegister(EEADR);
	data_temp=(ReadRegister(EEDATH)<<8)+ReadRegister(EEDATA);
	eecon_temp=ReadRegister(EECON1);
	WriteRegister(EEADRH,addr>>8);
	WriteRegister(EEADR,addr&0xFF);
	WriteRegister(EECON1,eecon_temp|0x80);	//EEPGD=1
	WriteRegister(EECON1,eecon_temp|0x81);	//EEPGD=1 + RD=1
	data=(ReadRegister(EEDATH)<<8)+ReadRegister(EEDATA);
	WriteRegister(EEADRH,addr_temp<<8);
	WriteRegister(EEADR,addr_temp&0xFF);
	WriteRegister(EEDATH,data_temp<<8);
	WriteRegister(EEDATA,data_temp&0xFF);
	WriteRegister(EECON1,eecon_temp);
	return data;
}

//Read data memory at address addr
int ReadDataMem(int addr){
	int addr_temp, data_temp, eecon_temp,data;
	addr_temp=ReadRegister(EEADR);
	data_temp=ReadRegister(EEDATA);
	eecon_temp=ReadRegister(EECON1);
	WriteRegister(EEADR,addr);
	WriteRegister(EECON1,eecon_temp&0x7F);			//EEPGD=0
	WriteRegister(EECON1,(eecon_temp&0x7F)|0x1);	//EEPGD=0 + RD=1
	data=ReadRegister(EEDATA);
	WriteRegister(EEADR,addr_temp);
	WriteRegister(EEDATA,data_temp);
	WriteRegister(EECON1,eecon_temp);
	return data;	
}

// get register name from list
char* getVar(int addr,char *var){
	addr&=0x1FF;
	if(variables[addr].name) sprintf(var,variables[addr].name);
	else sprintf(var,"0x%03X",addr);
	return var;
}

//Disassemble a command and return string
char* decodeCmd(int cmd,char *str, int addrH){
	char ins[32],reg[32];
	if((cmd&0x3F9F)==0) sprintf(str,"nop");
	else if(cmd==0x0008) sprintf(str,"return");
	else if(cmd==0x0009) sprintf(str,"retfie");
	else if(cmd==0x0063) sprintf(str,"sleep");
	else if(cmd==0x0064) sprintf(str,"clrwdt");
	else if((cmd>>12)==0){	//byte oriented instructions
		if((cmd>>8)==0&&cmd&0x80) sprintf(str,"movwf %s",getVar(addrH+(cmd&0x7F),reg));
		else if((cmd>>8)==1){
			if(cmd&0x80) sprintf(str,"clrf %s",getVar(addrH+(cmd&0x7F),reg));
			else sprintf(str,"clrf w");
		}
		else{
			switch(cmd>>8){
				case 2:
					sprintf(ins,"subwf");
				break;
				case 3:
					sprintf(ins,"decf");
				break;
				case 4:
					sprintf(ins,"iorwf");
				break;
				case 5:
					sprintf(ins,"andwf");
				break;
				case 6:
					sprintf(ins,"xorwf");
				break;
				case 7:
					sprintf(ins,"addwf");
				break;
				case 8:
					sprintf(ins,"movf");
				break;
				case 9:
					sprintf(ins,"comf");
				break;
				case 10:
					sprintf(ins,"incf");
				break;
				case 11:
					sprintf(ins,"decfsz");
				break;
				case 12:
					sprintf(ins,"rrf");
				break;
				case 13:
					sprintf(ins,"rlf");
				break;
				case 14:
					sprintf(ins,"swapf");
				break;
				case 15:
					sprintf(ins,"incfsz");
				break;
			}
			sprintf(str,"%s %s,%c",ins,getVar(addrH+(cmd&0x7F),reg),cmd&0x80?'f':'w');
		}
	}
	else if((cmd>>12)==1){	//bit oriented instructions
		switch(cmd>>10){
			case 4:
				sprintf(ins,"bcf");
			break;
			case 5:
				sprintf(ins,"bsf");
			break;
			case 6:
				sprintf(ins,"btfsc");
			break;
			case 7:
				sprintf(ins,"btfss");
			break;
			default:
				sprintf(ins,"??");	//(not possible)
		}
		sprintf(str,"%s %s,%d",ins,getVar(addrH+(cmd&0x7F),reg),(cmd&0x380)>>7);
	}
	else if((cmd>>12)==2) sprintf(str,"%s 0x%X",cmd&0x800?"goto":"call",cmd&0x7FF);
	else if((cmd>>10)==0xC) sprintf(str,"movlw 0x%X",cmd&0xFF);
	else if((cmd>>10)==0xD) sprintf(str,"retlw 0x%X",cmd&0xFF);
	else if((cmd>>9)==0x1E) sprintf(str,"sublw 0x%X",cmd&0xFF);
	else if((cmd>>9)==0x1F) sprintf(str,"addlw 0x%X",cmd&0xFF);
	else if((cmd>>8)==0x38) sprintf(str,"iorlw 0x%X",cmd&0xFF);
	else if((cmd>>8)==0x39) sprintf(str,"andlw 0x%X",cmd&0xFF);
	else if((cmd>>8)==0x3A) sprintf(str,"xorlw 0x%X",cmd&0xFF);
	else sprintf(str,"unknown command");
	return str;
}
