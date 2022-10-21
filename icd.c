//General routines to communicate via ICD with a target
#include "common.h"
#include "coff.h"

extern int saveLog;
extern double Tcom;
extern int running;
extern int DeviceDetected;
extern GtkWidget * b_log;
extern char* cur_path;

GtkWidget * statusTxt;
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
GtkTextBuffer * statusBuf;
int icdTimer=0;
int break_addr,print_addr;
int currentSource=-1;
int sourceHilight=0;
char lastCmd[64]="";
int UseCoff=0;
struct src_i source_info[LMAX];
struct srcfile *s_files;
struct symbol *sym;
int nsym=0;
char* Slabel[LMAX],*Sulabel[ULMAX];
struct symbol *watch;
int nwatch=0;
unsigned short coff_data[DATA_MAX];
int ver=0,reset=1,freeze=0,icdConnected=0,running=0;
#define Tck 30
double Tcom=0.001*Tck*18+0.03; //communication time for a 16 bit tranfer (ms)

void ShowContext();

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
void startICD(int tck){
	int j=0;
	bufferU[j++]=PROG_RST;
	bufferU[j++]=SET_PARAMETER;
	bufferU[j++]=SET_T1T2;
	bufferU[j++]=tck;				//T1=XXu
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
	PacketIO(3);
	if(saveLog){
		fprintf(logfile,"startICD()\n");
	}
}

//Check whether the target is running or is executing the debug routine.
//This is signaled by RB7 (Data): D=1 -> debugger monitor running
int isRunning(){
	int z,j=0;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x0;		//D=0
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x2;		//set D as input
	bufferU[j++]=READ_PINS;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	PacketIO(2);
	if(saveLog){
		fprintf(logfile,"isRunning()\n");
	}
	for(z=0;z<DIMBUF-1&&bufferI[z]!=READ_PINS;z++);
	if(bufferI[z+1]&1) running=0;
	else running=1;
	return running;
}

//Set the next breakpoint address, the freeze bit, 
//and continue execution.
//This is necessary because at every break 
//the ICD register is loaded with the last address.
void cont(int break_addr, int freeze){
	int j=0;
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
	PacketIO(1+5*Tcom);
	if(saveLog){
		fprintf(logfile,"continue()\n");
	}
	running=1;
}

//Execute a single step
void step(){
	int j=0;
	bufferU[j++]=TX16;
	bufferU[j++]=0x1;
	bufferU[j++]=STEP;		//single step
	bufferU[j++]=0;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x2;		//set D as input
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	PacketIO(1+Tcom);
	if(saveLog){
		fprintf(logfile,"step()\n");
	}
}

//Remove reset so that the target can start executing its code.
void run(){
	int j=0;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x2;
	bufferU[j++]=EN_VPP_VCC;		//MCLR=H
	bufferU[j++]=0x5;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	PacketIO(2);
	if(saveLog){
		fprintf(logfile,"run()\n");
	}
	running=1;
}

//Get the debugger monitor version
int version(){
	int j=0,z;
	bufferU[j++]=TX16;
	bufferU[j++]=0x1;
	bufferU[j++]=VER;		//version
	bufferU[j++]=0;
	bufferU[j++]=RX16;
	bufferU[j++]=0x1;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	PacketIO(1+2*Tcom);
	if(saveLog){
		fprintf(logfile,"version()\n");
	}
	for(z=0;z<DIMBUF-2&&bufferI[z]!=RX16;z++);
	return bufferI[z+3];
}

//Halt execution by setting RB6 (Clock) low
void Halt(){
	int j=0;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x6;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x2;		//set D as input
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	PacketIO(2);
	if(saveLog){
		fprintf(logfile,"halt()\n");
	}
	running=0;
	//printf("halted\n");
}

//Read register at address addr
int ReadRegister(int addr){
	int j=0,z;
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
	PacketIO(1+3*Tcom);
	if(saveLog){
		fprintf(logfile,"ReadRegister(0x%X)\n",addr);
	}
	for(z=0;z<DIMBUF-2&&bufferI[z]!=RX16;z++);
	return bufferI[z+3];	
}

//Read n registers starting at address addr
int ReadRegisterN(int addr,int n,int* buf){
	int i,j=0,z,w;
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
		PacketIO(1+(w+2)*Tcom);
		if(saveLog){
			fprintf(logfile,"ReadRegisterN(0x%X,%d)\n",addr,n);
		}
		for(z=0;z<DIMBUF-2&&bufferI[z]!=RX16;z++);
		for(j=0;j<w;j++) buf[i+j]=bufferI[z+3+j*2];
		j=0;
	}
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x2;		//set D as input
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	PacketIO(2);
	return i==n?0:-1;
}

//Write data at address addr
void WriteRegister(int addr,int data){
	int j=0;
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
	PacketIO(1+2*Tcom);
	if(saveLog){
		fprintf(logfile,"WriteRegister(0x%X,0x%X)\n",addr,data);
	}
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

//Read program memory (n locations) starting at address addr
int ReadProgMemN(int addr,int n,int* buf){
	int addr_temp, data_temp, eecon_temp;
	if(saveLog) fprintf(logfile,"ReadProgMemN(0x%X,%d)\n",addr,n);
	int i,j=0,z,w,k;
	bufferU[j++]=TX16;
	bufferU[j++]=2;
	bufferU[j++]=RREG;		//Read register
	bufferU[j++]=4;			//4 bytes: EEDATA,EEADR,EEDATH,EEADRH
	bufferU[j++]=(EEDATA>>8)&0xFF;
	bufferU[j++]=EEDATA&0xFF;
	bufferU[j++]=RX16;
	bufferU[j++]=4;
	bufferU[j++]=TX16;
	bufferU[j++]=2;
	bufferU[j++]=RREG;		//Read register
	bufferU[j++]=1;			//1 byte
	bufferU[j++]=(EECON1>>8)&0xFF;
	bufferU[j++]=EECON1&0xFF;
	bufferU[j++]=RX16;
	bufferU[j++]=1;
	bufferU[j++]=TX16;
	bufferU[j++]=2;
	bufferU[j++]=WREG;		//write register
	bufferU[j++]=0x80;		//EEPGD=1
	bufferU[j++]=(EECON1>>8)&0xFF;
	bufferU[j++]=EECON1&0xFF;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	PacketIO(1+13*Tcom);
	j=0;
	for(z=0;z<DIMBUF-5&&bufferI[z]!=RX16;z++);
	data_temp=bufferI[z+3]+(bufferI[z+7]<<8);
	addr_temp=bufferI[z+5]+(bufferI[z+9]<<8);
	for(z+=10;z<DIMBUF-3&&bufferI[z]!=RX16;z++);
	eecon_temp=bufferI[z+3];
	w=k=0;
	for(i=0;i<n;i++){
		bufferU[j++]=TX16;
		bufferU[j++]=8;
		bufferU[j++]=WREG;			//write register
		bufferU[j++]=(addr+i)&0xFF;
		bufferU[j++]=(EEADR>>8)&0xFF;
		bufferU[j++]=EEADR&0xFF;
		bufferU[j++]=WREG;			//write register
		bufferU[j++]=((addr+i)>>8)&0xFF;
		bufferU[j++]=(EEADRH>>8)&0xFF;
		bufferU[j++]=EEADRH&0xFF;
		bufferU[j++]=WREG;			//write register
		bufferU[j++]=0x81;			//RD=1
		bufferU[j++]=(EECON1>>8)&0xFF;
		bufferU[j++]=EECON1&0xFF;
		bufferU[j++]=RREG;		//Read register
		bufferU[j++]=3;			//3 bytes: EEDATA,EEADR,EEDATH
		bufferU[j++]=(EEDATA>>8)&0xFF;
		bufferU[j++]=EEDATA&0xFF;
		bufferU[j++]=RX16;
		bufferU[j++]=3;
		w++;
		if(j>DIMBUF-21||i==n-1){
			bufferU[j++]=FLUSH;
			for(;j<DIMBUF;j++) bufferU[j]=0x0;
			PacketIO(2+13*Tcom*w);
			j=0;
			w=0;
			for(z=0;z<DIMBUF-5;z++){
				if(bufferI[z]==RX16){
					buf[k++]=bufferI[z+3]+(bufferI[z+7]<<8);
					z+=8; //******controllare!!**********
				}
			}
		}
	}
	bufferU[j++]=TX16;
	bufferU[j++]=10;
	bufferU[j++]=WREG;			//write register
	bufferU[j++]=eecon_temp;	//EEPGD=1
	bufferU[j++]=(EECON1>>8)&0xFF;
	bufferU[j++]=EECON1&0xFF;
	bufferU[j++]=WREG;			//write register
	bufferU[j++]=data_temp&0xFF;
	bufferU[j++]=(EEDATA>>8)&0xFF;
	bufferU[j++]=EEDATA&0xFF;
	bufferU[j++]=WREG;			//write register
	bufferU[j++]=data_temp>>8;
	bufferU[j++]=(EEDATH>>8)&0xFF;
	bufferU[j++]=EEDATH&0xFF;
	bufferU[j++]=WREG;			//write register
	bufferU[j++]=addr_temp&0xFF;
	bufferU[j++]=(EEADR>>8)&0xFF;
	bufferU[j++]=EEADR&0xFF;
	bufferU[j++]=WREG;			//write register
	bufferU[j++]=addr_temp>>8;
	bufferU[j++]=(EEADRH>>8)&0xFF;
	bufferU[j++]=EEADRH&0xFF;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x2;		//set D as input
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	PacketIO(1+10*Tcom);
	return i;
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

//Read n bytes from data memory starting at address addr
int ReadDataMemN(int addr,int n,unsigned char* buf){
	int addr_temp, data_temp, eecon_temp;
	if(saveLog) fprintf(logfile,"ReadDataMemN(0x%X,%d)\n",addr,n);
	int i,j=0,z,w,k;
	bufferU[j++]=TX16;
	bufferU[j++]=2;
	bufferU[j++]=RREG;		//Read register
	bufferU[j++]=2;			//2 bytes: EEDATA,EEADR
	bufferU[j++]=(EEDATA>>8)&0xFF;
	bufferU[j++]=EEDATA&0xFF;
	bufferU[j++]=RX16;
	bufferU[j++]=2;
	bufferU[j++]=TX16;
	bufferU[j++]=2;
	bufferU[j++]=RREG;		//Read register
	bufferU[j++]=1;			//1 byte
	bufferU[j++]=(EECON1>>8)&0xFF;
	bufferU[j++]=EECON1&0xFF;
	bufferU[j++]=RX16;
	bufferU[j++]=1;
	bufferU[j++]=TX16;
	bufferU[j++]=2;
	bufferU[j++]=WREG;		//write register
	bufferU[j++]=0;			//EEPGD=0
	bufferU[j++]=(EECON1>>8)&0xFF;
	bufferU[j++]=EECON1&0xFF;
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	PacketIO(1+13*Tcom);
	j=0;
	for(z=0;z<DIMBUF-5&&bufferI[z]!=RX16;z++);
	data_temp=bufferI[z+3];
	addr_temp=bufferI[z+5];
	for(z+=6;z<DIMBUF-3&&bufferI[z]!=RX16;z++);
	eecon_temp=bufferI[z+3];
	w=k=0;
	for(i=0;i<n;i++){
		bufferU[j++]=TX16;
		bufferU[j++]=6;
		bufferU[j++]=WREG;			//write register
		bufferU[j++]=(addr+i)&0xFF;
		bufferU[j++]=(EEADR>>8)&0xFF;
		bufferU[j++]=EEADR&0xFF;
		bufferU[j++]=WREG;			//write register
		bufferU[j++]=0x1;			//RD=1
		bufferU[j++]=(EECON1>>8)&0xFF;
		bufferU[j++]=EECON1&0xFF;
		bufferU[j++]=RREG;		//Read register
		bufferU[j++]=1;			// EEDATA
		bufferU[j++]=(EEDATA>>8)&0xFF;
		bufferU[j++]=EEDATA&0xFF;
		bufferU[j++]=RX16;
		bufferU[j++]=1;
		w++;
		if(j>DIMBUF-17||i==n-1){
			bufferU[j++]=FLUSH;
			for(;j<DIMBUF;j++) bufferU[j]=0x0;
			PacketIO(2+10*Tcom*w);
			j=0;
			w=0;
			for(z=0;z<DIMBUF-5;z++){
				if(bufferI[z]==RX16){
					buf[k++]=bufferI[z+3];
					z+=4;
				}
			}
		}
	}
	bufferU[j++]=TX16;
	bufferU[j++]=6;
	bufferU[j++]=WREG;			//write register
	bufferU[j++]=eecon_temp;	//EEPGD=1
	bufferU[j++]=(EECON1>>8)&0xFF;
	bufferU[j++]=EECON1&0xFF;
	bufferU[j++]=WREG;			//write register
	bufferU[j++]=data_temp&0xFF;
	bufferU[j++]=(EEDATA>>8)&0xFF;
	bufferU[j++]=EEDATA&0xFF;
	bufferU[j++]=WREG;			//write register
	bufferU[j++]=addr_temp&0xFF;
	bufferU[j++]=(EEADR>>8)&0xFF;
	bufferU[j++]=EEADR&0xFF;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x2;		//set D as input
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	PacketIO(1+10*Tcom);
	return i;
}

// get register name from list
char* getVar(int addr,char *var){
	addr&=0x1FF;
	if(variables[addr].name) strcpy(var,variables[addr].name);
	else sprintf(var,"0x%03X",addr);
	return var;
}

//Disassemble a command and return string
//addrH is the higher (bank) address for memory (RP1-RP0)
char* decodeCmd(int cmd,char *str, int addrH){
	char ins[32],reg[32];
	if((cmd&0x3F9F)==0) sprintf(str,"nop");
	else if(cmd==0x0001) sprintf(str,"reset");
	else if(cmd==0x0008) sprintf(str,"return");
	else if(cmd==0x0009) sprintf(str,"retfie");
	else if(cmd==0x000A) sprintf(str,"callw");
	else if(cmd==0x000B) sprintf(str,"brw");
	else if(cmd==0x0062) sprintf(str,"option");
	else if(cmd==0x0063) sprintf(str,"sleep");
	else if(cmd==0x0064) sprintf(str,"clrwdt");
	else if(cmd==0x0065) sprintf(str,"trisa");
	else if(cmd==0x0066) sprintf(str,"trisb");
	else if(cmd==0x0067) sprintf(str,"trisc");
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
				default:
					sprintf(ins,"???");
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

//Functions tied to GUI:
///
///Print a message on the ICD data field
void PrintMessageICD(const char *msg){
	GtkTextIter iter;
	gtk_text_buffer_set_text(statusBuf,msg,-1);
	gtk_text_buffer_get_start_iter(statusBuf,&iter);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(statusTxt),&iter,0.0,FALSE,0,0);
	while (gtk_events_pending ()) gtk_main_iteration();
}
///
///Append a message on the ICD data field
void AppendMessageICD(const char *msg){
	GtkTextIter iter;
	gtk_text_buffer_get_end_iter(statusBuf,&iter);
	gtk_text_buffer_insert(statusBuf,&iter,msg,-1);
	gtk_text_buffer_get_start_iter(statusBuf,&iter);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(statusTxt),&iter,0.0,FALSE,0,0);
	while (gtk_events_pending ()) gtk_main_iteration();
}
///
///Scroll source file
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
///
///Hilight line in source code
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
///
///Remove hilight line in source code
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
///
///load source file into source pane
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
///
///load and analyze coff file
void loadCoff(GtkWidget *widget,GtkWidget *window)
{
	GtkFileChooser *dialog;
	dialog = (GtkFileChooser*) gtk_file_chooser_dialog_new (strings[I_LOAD_COFF], //"Open Coff File",
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
///
/// List of variables used when decoding an assembly word
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
///
///Show ICD help window
void ICDHelp(GtkWidget *widget,GtkWidget *window)
{
	GtkWidget* dialog = gtk_message_dialog_new (GTK_WINDOW(window),
                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                 GTK_MESSAGE_INFO,
                                 GTK_BUTTONS_CLOSE,
                                 strings[I_ICD_HELP_TXT]);

	g_signal_connect_swapped (GTK_WINDOW(dialog), "response",G_CALLBACK (gtk_widget_destroy),dialog);
	gtk_window_set_title(GTK_WINDOW(dialog),strings[I_ICD_HELP]);
	gtk_widget_show_all (dialog);
}
///
///ICD: check if program is running
void icdCheck(GtkWidget *widget,GtkWidget *window)
{
#ifndef DEBUG
	if(DeviceDetected!=1) return;
#endif
	if(!isRunning()){
		g_source_remove(icdTimer);
		ShowContext();
	}
}
///
///ICD: run program
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
		icdTimer=g_timeout_add(20,(GSourceFunc)icdCheck,NULL);
		PrintMessageICD("running");
	}
	else if(!running){
		cont(break_addr,freeze);	//continue execution
		icdTimer=g_timeout_add(20,(GSourceFunc)icdCheck,NULL);
		PrintMessageICD("running");
	}
}
///
///ICD: halt program
void icdHalt(GtkWidget *widget,GtkWidget *window)
{
#ifndef DEBUG
	if(DeviceDetected!=1) return;
#endif
	if(running){
		g_source_remove(icdTimer);
		Halt();
		ShowContext();
	}
}
///
///ICD: step program
void icdStep(GtkWidget *widget,GtkWidget *window)
{
#ifndef DEBUG
	if(DeviceDetected!=1) return;
#endif
	if(running){
		g_source_remove(icdTimer);
		Halt();
	}
	step();
#ifdef DEBUG
	addrDebug++;
#endif
	ShowContext();
}
///
///ICD: step program jumping over calls
void icdStepOver(GtkWidget *widget,GtkWidget *window)
{
#ifndef DEBUG
	if(DeviceDetected!=1) return;
#endif
	int addr,data;
	if(running){
		g_source_remove(icdTimer);
		Halt();
	}
	addr=((ReadRegister(0x18E)&0x1F)<<8)+ReadRegister(0x18F);
	data=ReadProgMem(addr);
	if((data>>11)==4){	//if call break at return address
		cont(addr+1,freeze);
		icdTimer=g_timeout_add(20,(GSourceFunc)icdCheck,NULL);
	}
	else{		//normal step
		step();
		#ifdef DEBUG
		addrDebug++;
		#endif
		ShowContext();
	}
}
///
///ICD: stop program
void icdStop(GtkWidget *widget,GtkWidget *window)
{
#ifndef DEBUG
	if(DeviceDetected!=1) return;
#endif
	if(running){
		g_source_remove(icdTimer);
		Halt();
	}
//	bufferU[0]=0;
	int j=0;
	bufferU[j++]=EN_VPP_VCC;		// reset target
	bufferU[j++]=0x0;
	bufferU[j++]=WAIT_T3;
	bufferU[j++]=SET_CK_D;
	bufferU[j++]=0x2;				//set D as input
	bufferU[j++]=FLUSH;
	for(;j<DIMBUF;j++) bufferU[j]=0x0;
	PacketIO(3);
	if(saveLog)WriteLogIO();
	icdConnected=0;
	PrintMessageICD("stopped");
	scrollToLine(source_info[0].src_line);
}
///
///ICD: refresh status
void icdRefresh(GtkWidget *widget,GtkWidget *window)
{
#ifndef DEBUG
	if(DeviceDetected!=1) return;
#endif
	if(!running){
		ShowContext();
	}
}
///
/// Read and display an entire bank of memory
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
///
/// Main ICD show function:
/// prints status info according to selected options
/// and the value of variables in the watch list
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
///
///Add symbol to the list of watched variables
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
///
/// ICD Command parser
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
///
///Remove variable from watch list
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
///
///Handle mouse events in source code window
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
///
///Handle mouse events in ICD status window
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
///
///Handle keyboard events in ICD command edit box
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
///
///Handle keyboard events in ICD tab
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

