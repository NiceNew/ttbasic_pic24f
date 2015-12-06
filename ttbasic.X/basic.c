/*
	Title: TOYOSHIKI TinyBASIC
	Filename: ttbasic/basic.c
	(C)2012 Tetsuya Suzuki, All rights reserved.
*/

//�R���p�C���Ɉˑ�����w�b�_

//�n�[�h�E�F�A(OS��API)�Ɉˑ�����w�b�_
#include <stdlib.h> // rnd
#include "led.h"
#include "sw.h"
#include "sci.h"//c_putch,c_getch,c_kbhit
#include "flash.h"

// TinyBASIC symbols
#define SIZE_LINE 80 //1�s���͕������ƏI�[�L���̃T�C�Y
#define SIZE_IBUF 80//���ԃR�[�h�o�b�t�@�̃T�C�Y
#define SIZE_LIST 1023//���X�g�ۑ��̈�̃T�C�Y
#define SIZE_ARAY 128//�z��̈�̃T�C�Y
#define SIZE_GSTK 6//GOSUB�p�X�^�b�N�T�C�Y(2�P��)
#define SIZE_LSTK 15//FOR�p�X�^�b�N�T�C�Y(5�P��)

// Depending on device functions
// TO-DO Rewrite these functions to fit your machine
#define STR_EDITION "PIC24F"

// Terminal control
#define c_putch(c) putch2(c)
#define c_getch() getch2()
#define c_kbhit() kbhit2()

#define KEY_ENTER 13
void newline(void){c_putch(13);c_putch(10);}

//������Ԃ�
//1����w��l�܂�
short getrnd(short value){
	//�n�[�h�E�F�A�Ɉˑ�
	return (rand() % value) + 1;
    //return 1;
}

char lbuf[SIZE_LINE];//�R�}���h���C���̃o�b�t�@
unsigned char ibuf[SIZE_IBUF];//���ԃR�[�h�o�b�t�@
short var[26];// �ϐ��̈�
short ary[SIZE_ARAY];//�z��̈�
unsigned char listbuf[SIZE_LIST + 1];//���X�g�ۑ��̈�+�u�[�g�t���O
unsigned char* clp;//�J�����g�s�̐擪���w���|�C���^
unsigned char *cip;//�p�[�X�̒��ԃR�[�h�̃|�C���^

unsigned char* gstk[SIZE_GSTK];//�X�^�b�N�̈�
unsigned char gstki;//�X�^�b�N�C���f�N�X
unsigned char* lstk[SIZE_LSTK];//�X�^�b�N�̈�
unsigned char lstki;//�X�^�b�N�C���f�N�X

//PROTOTYPES
short iexp(void);

//intermediate code table
//__attribute__((section(".romdata"), space(prog), aligned(64)))
const char* kwtbl[] = {
	"IF",
	"GOTO",
	"GOSUB",
	"RETURN",
	"FOR",
	"TO",
	"STEP",
	"NEXT",
	"PRINT",
	"INPUT",
	"LED",//extend
	"REM",
	"LET",
	"STOP",
    "ON",
    "OFF",
	";",
	"-",
	"+",
	"*",
	"/",
	">=",
	"<>",
	">",
	"=",
	"<=",
	"<",
	"(",
	")",
	",",
	"#",
	"@",
	"RND",
	"ABS",
	"SW",//extend
	"SIZE",
	"LIST",
	"RUN",
	"NEW",
	"SAVE",//extend
	"BOOT",//extend
	"LOAD",//extend
};

enum{
	I_IF,
	I_GOTO,
	I_GOSUB,
	I_RETURN,
	I_FOR,
	I_TO,
	I_STEP,
	I_NEXT,
	I_PRINT,
	I_INPUT,
	I_LED,//extend
	I_REM,
	I_LET,
	I_STOP,
	I_ON,
	I_OFF,
	I_SEMI,//���X�g�\���ŋ󔒂����邪���l�͌��ɋ󔒂����Ȃ�
	I_MINUS,//���Z�q�̐擪
	I_PLUS,
	I_MUL,
	I_DIV,
	I_GTE,
	I_NEQ,
	I_GT,
	I_EQ,
	I_LTE,
	I_LT,
	I_OPEN,
	I_CLOSE,//���Z�q�̖���
	I_COMMA,//�����܂ł͐��l�ƕϐ������ɋ󔒂����Ȃ�
	I_SHARP,
	I_ARRAY,
	I_RND,
	I_ABS,
	I_SW,//extend
	I_SIZE,
	I_LIST,
	I_RUN,
	I_NEW,
	I_SAVE,//extend
	I_BOOT,//extend
	I_LOAD,//extend//�����܂ł̓L�[���[�h(�z��̃��[�v�Ō���)
	I_NUM,//�L�[���[�h�ł͂Ȃ��̂ŋ󔒂͌ʏ��������
	I_VAR,
	I_STR,
	I_EOL//���ԃR�[�h�̍s��
};

#define SIZE_KWTBL (sizeof(kwtbl) / sizeof(char*))
#define IS_OP(p) (p >= I_MINUS && p <= I_CLOSE)
#define IS_SEP(p) (p == I_COMMA || p == I_SEMI)
#define IS_TOKSP(p) (/*p >= I_IF && */p <= I_LET && p != I_RETURN)

//goto�̌��̃G���[�̓`�F�b�N����Ȃ�
//run�̓v���O���������݂���Ό��̃G���[�̓`�F�b�N����Ȃ�
unsigned char err;//�G���[�ԍ�(�G���[���b�Z�[�W�̃C���f�N�X)
//__attribute__((section(".romdata"), space(prog), aligned(64)))
const char* errmsg[] ={
	"OK",
	"Devision by zero",
	"Overflow",
	"Subscript out of range",
	"Icode buffer full",
	"List full",
	"GOSUB too many nested",
	"RETURN stack underflow",
	"FOR too many nested",
	"NEXT without FOR",
	"NEXT without counter",
	"NEXT mismatch FOR",
	"FOR without variable",
	"FOR without TO",
	"LET without variable",
	"IF without condition",
	"Undefined line number",
	"\'(\' or \')\' expected",
	"\'=\' expected",
	"Illegal command",
	"Syntax error",
	"Internal error",
	"Abort by [ESC]"
};

enum{
	ERR_OK,
	ERR_DIVBY0,
	ERR_VOF,
	ERR_SOL,
	ERR_IBUFOF,
	ERR_LBUFOF,
	ERR_GSTKOF,
	ERR_GSTKUF,
	ERR_LSTKOF,
	ERR_LSTKUF,
	ERR_NEXTWOV,
	ERR_NEXTUM,
	ERR_FORWOV,
	ERR_FORWOTO,
	ERR_LETWOV,
	ERR_IFWOC,
	ERR_ULN,
	ERR_PAREN,
	ERR_VWOEQ,
	ERR_COM,
	ERR_SYNTAX,
	ERR_SYS,
	ERR_ESC
};

//Standard C libraly�Œ჌�x���̊֐�
char c_toupper(char c) {return(c <= 'z' && c >= 'a' ? c - 32 : c);}
char c_isprint(char c) {return(c >= 32  && c <= 126);}
char c_isspace(char c) {return(c == ' ' || (c <= 13 && c >= 9));}
char c_isdigit(char c) {return(c <= '9' && c >= '0');}
char c_isalpha(char c) {return ((c <= 'z' && c >= 'a') || (c <= 'Z' && c >= 'A'));}

void c_puts(const char *s) {
	while (*s) c_putch(*s++); //�I�[�łȂ���Ε\�����ČJ��Ԃ�
}
void c_gets() {
	char c; //����
	unsigned char len; //������

	len = 0; //���������N���A
	while ((c = c_getch()) != KEY_ENTER) { //���s�łȂ���ΌJ��Ԃ�
		if (c == 9) c = ' '; //�mTab�n�L�[�͋󔒂ɒu��������
		//�mBackSpace�n�L�[�������ꂽ�ꍇ�̏����i�s���ł͂Ȃ����Ɓj
		if (((c == 8) || (c == 127)) && (len > 0)) {
			len--; //��������1���炷
			c_putch(8); c_putch(' '); c_putch(8); //����������
		}	else
		//�\���\�ȕ��������͂��ꂽ�ꍇ�̏����i�o�b�t�@�̃T�C�Y�𒴂��Ȃ����Ɓj
		if (c_isprint(c) && (len < (SIZE_LINE - 1))) {
			lbuf[len++] = c; //�o�b�t�@�֓���ĕ�������1���₷
			c_putch(c); //�\��
		}
	}
	newline(); //���s
	lbuf[len] = 0; //�I�[��u��

	if (len > 0) { //�����o�b�t�@����łȂ����
		while (c_isspace(lbuf[--len])); //�����̋󔒂�߂�
		lbuf[++len] = 0; //�I�[��u��
	}
}

//�w��̒l���w��̌����ŕ\��
//�w��̒l���w��̌����ɖ����Ȃ������ꍇ�̂݋󔒂Ő��߂�
void putnum(short value, short d) {
	unsigned char dig; //���ʒu
	unsigned char sign; //�����̗L���i�l���Βl�ɕϊ�������j

	if (value < 0) { //�����l��0�����Ȃ�
		sign = 1; //��������
		value = -value; //�l���Βl�ɕϊ�
	}
	else {
		sign = 0; //�����Ȃ�
	}

	lbuf[6] = 0; //�I�[��u��
	dig = 6; //���ʒu�̏����l�𖖔��ɐݒ�
	do { //���̏���������Ă݂�
		lbuf[--dig] = (value % 10) + '0'; //1�̈ʂ𕶎��ɕϊ����ĕۑ�
		value /= 10; //1�����Ƃ�
	} while (value > 0); //�l��0�łȂ���ΌJ��Ԃ�

	if (sign) //������������Ȃ�
		lbuf[--dig] = '-'; //������ۑ�

	while (6 - dig < d) { //�w��̌�����������Ă���ΌJ��Ԃ�
		c_putch(' '); //���̕s�����󔒂Ŗ��߂�
		d--; //�w��̌�����1���炷
	}
	c_puts(&lbuf[dig]); //���ʒu����o�b�t�@�̕������\��
}

//���l�̂ݓ��͉\�A�߂�l�͐��l
//INPUT�ł̂ݎg���Ă���
short getnum(){
	short value, tmp;
	char c;
	unsigned char len;
	unsigned char sign;

	len = 0;
	while((c = c_getch()) != 13){
		if((c == 8) && (len > 0)){//�o�b�N�X�y�[�X�̏���
			len--;
			c_putch(8); c_putch(' '); c_putch(8);
		} else
		if(	(len == 0 && (c == '+' || c == '-')) ||
			(len < 6 && c_isdigit(c))){//�����ƕ����̏���
			lbuf[len++] = c;
			c_putch(c);
		}
	}
	newline();
	lbuf[len] = 0;//������̏I�[�L����u���ē��͂��I��

	switch(lbuf[0]){
	case '-':
		sign = 1;
		len = 1;
		break;
	case '+':
		sign = 0;
		len = 1;
		break;
	default:
		sign = 0;
		len = 0;
		break;
	}

	value = 0;//���l��������
	tmp = 0;//�ߓn�I���l��������
	while(lbuf[len]){
		tmp = 10 * value + lbuf[len++] - '0';
		if(value > tmp){
			err = ERR_VOF;
		}
		value = tmp;
	}
	if(sign)
		return -value;
	return value;
}

//GOSUB-RETURN�p�̃X�^�b�N����
void gpush(unsigned char* pd){
	if(gstki < SIZE_GSTK){
		gstk[gstki++] = pd;
		return;
	}
	err = ERR_GSTKOF;
}

unsigned char* gpop(){
	if(gstki > 0){
		return gstk[--gstki];
	}
	err = ERR_GSTKUF;
	return NULL;
}

//FOR-NEXT�p�̃X�^�b�N����
void lpush(unsigned char* pd){
	if(lstki < SIZE_LSTK){
		lstk[lstki++] = pd;
		return;
	}
	err = ERR_LSTKOF;
}

unsigned char* lpop(){
	if(lstki > 0){
		return lstk[--lstki];
	}
	err = ERR_LSTKUF;
	return NULL;
}

//ip���w��BNN�̃o�C�g�񂩂�NN���\��short�^�̒l��Ԃ�
//B��0�̏ꍇ(���X�g�̏I�[)��NN�����݂��Ȃ��̂œ��ʂȒl32767��Ԃ�
//���ԃR�[�h�|�C���^�͐i�߂Ȃ��A�K�v�Ȃ�3�𑫂�
short getvalue(unsigned char* ip){
	if(*ip == 0)
		return 32767;//�s�����̑΍�Ƃ��čő�s�ԍ���Ԃ�
	return((short)*(ip + 1) + ((short)*(ip + 2) << 8));
}

//���ʂł�����ꂽ����l�ɕϊ����Ē��ԃR�[�h�|�C���^��i�߂�
short getparam(){
	short value;

	if(*cip != I_OPEN){
		err = ERR_PAREN;
		return 0;
	}
	cip++;
	value = iexp();
	if(err) return 0;

	if(*cip != I_CLOSE){
		err = ERR_PAREN;
		return 0;
	}
	cip++;

	return value;
}

//�s�ԍ����w�肵�čs�ԍ������������傫���s�̐擪�̃|�C���^�𓾂�
//�O���[�o���ȕϐ���ύX���Ȃ�
unsigned char* getlp(short lineno){
	unsigned char *lp;

	lp = listbuf;
	while(*lp){
		if(getvalue(lp) >= lineno)
			break;
		lp += *lp;
	}
	return lp;
}

//�s�o�b�t�@�̃g�[�N���𒆊ԃR�[�h�ɕϊ����Ē��ԃR�[�h�o�b�t�@�ɕۑ�
//���̒���putnum���g���Ă͂����Ȃ�
//���ԃR�[�h�̓C���f�b�N�Xlen�ňړ����邩�璆�ԃR�[�h�|�C���^���g��Ȃ�
//�󔒂��폜
//������I_EOL��u��
//�ϊ��ł����璷����Ԃ��A�ϊ��ł��Ȃ�������0��Ԃ�
//�G���[�t���O�𑀍삵�Ȃ�
//���@�`�F�b�N�͕ϐ��̘A�����͂�������
unsigned char toktoi(){
	unsigned char i;//�J��Ԃ��̃J�E���^(�ꎞ�I�ɒ��ԃR�[�h���Ӗ�����)
	unsigned char len;//���ԃR�[�h�̑��o�C�g��
	short value;//�����𐔒l�ɕϊ������Ƃ��g���ϐ�
	short tmp;//���l�ւ̕ϊ��ߒ��ŃI�[�o�[�t���[�łȂ����Ƃ��m�F����ϐ�
	char* pkw;//�L�[���[�h���w���|�C���^
	char* ptok;//�L�[���[�h�Ɣ�r���镶����̃g�[�N�����w���|�C���^
	char c;//������̈͂ݕ���("�܂���')��ێ�����
	char* s;//���C���o�b�t�@�̕�������w���|�C���^

	s = lbuf;
	len = 0;//���ԃR�[�h�̑��o�C�g�������Z�b�g
	while(*s){//�\�[�X�o�b�t�@�̖����܂ő�����
		while(c_isspace(*s)) s++;//�󔒂��X�L�b�v

		//�L�[���[�h�𒆊ԃR�[�h�ɕϊ��ł��邩�ǂ������ׂ�
		for(i = 0; i < SIZE_KWTBL; i++){
			pkw = (char *)kwtbl[i];//�L�[���[�h���w��
			ptok = s;//�s�o�b�t�@�̐擪���w��
			//������̔�r
			while((*pkw != 0) && (*pkw == c_toupper(*ptok))){
				pkw++;
				ptok++;
			}
			if(*pkw == 0){//���ԃR�[�h�ɕϊ��ł����ꍇ
				//i�����ԃR�[�h
				if(len >= SIZE_IBUF - 1){//�o�b�t�@�ɋ󂫂��Ȃ��ꍇ
					err = ERR_IBUFOF;
					return 0;
				}
				//�o�b�t�@�ɋ󂫂�����ꍇ
				ibuf[len++] = i;
				s = ptok;//�\�[�X�o�b�t�@�̎��̈ʒu
				break;
			}
		}

		//�X�e�[�g�����g�����l�A������A�ϐ��ȊO�̈�����v������ꍇ�̏���
		switch(i){
		case I_REM://REM�̏ꍇ�͈ȍ~�����̂܂ܕۑ����ďI��
			while(c_isspace(*s)) s++;//�󔒂��X�L�b�v
			ptok = s;
			for(i = 0; *ptok++; i++);//�s���܂ł̒������͂���
			if(len >= SIZE_IBUF - 2 - i){//�o�b�t�@�ɋ󂫂��Ȃ��ꍇ
				err = ERR_IBUFOF;
				return 0;
			}

			//�o�b�t�@�ɋ󂫂�����ꍇ
			ibuf[len++] = i;//������̒���
			while(i--){//��������R�s�[
				ibuf[len++] = *s++;
			}
			return len;
		default:
			break;
		}

		if(*pkw != 0){//�L�[���[�h�ł͂Ȃ������ꍇ
			ptok = s;//�|�C���^��擪�ɍĐݒ�

			if(c_isdigit(*ptok)){//�����̕ϊ������݂�
				value = 0;//���l��������
				tmp = 0;//�ߓn�I���l��������
				do{
					tmp = 10 * value + *ptok++ - '0';
					if(value > tmp){
						err = ERR_VOF;
						return 0;
					}
					value = tmp;
				} while(c_isdigit(*ptok));

				if(len >= SIZE_IBUF - 3){//�o�b�t�@�ɋ󂫂��Ȃ��ꍇ
					err = ERR_IBUFOF;
					return 0;
				}
				//�o�b�t�@�ɋ󂫂�����ꍇ
				s = ptok;
				ibuf[len++] = I_NUM;//���l��\�����ԃR�[�h
				ibuf[len++] = value & 255;
				ibuf[len++] = value >> 8;
			} else  //�����ł͂Ȃ�������

			//�ϐ��̕ϊ������݂�
			if(c_isalpha(*ptok)){//�ϐ���������
				if(len >= SIZE_IBUF - 2)
				{//�o�b�t�@�ɋ󂫂��Ȃ��ꍇ
					err = ERR_IBUFOF;
					return 0;
				}
				//�o�b�t�@�ɋ󂫂�����ꍇ
				if(len >= 2 && ibuf[len -2] == I_VAR){//���O���ϐ��Ȃ�
					 err = ERR_SYNTAX;
					 return 0;
				}
				//���O���ϐ��łȂ����
				ibuf[len++] = I_VAR;//�ϐ���\�����ԃR�[�h
				ibuf[len++] = c_toupper(*ptok) - 'A';//�ϐ��̃C���f�N�X
				s++;
			} else //�ϐ��ł͂Ȃ�������

			//������̕ϊ������݂�
			if(*s == '\"' || *s == '\''){//������̊J�n��������
				c = *s;
				s++;//"�̎���
				ptok = s;
				for(i = 0; (*ptok != c) && c_isprint(*ptok); i++)//������̒������͂���
					ptok++;
				if(len >= SIZE_IBUF - 1 - i){//�o�b�t�@�ɋ󂫂��Ȃ��ꍇ
					err = ERR_IBUFOF;
					return 0;
				}
				//�o�b�t�@�ɋ󂫂�����ꍇ
				ibuf[len++] = I_STR;//�������\�����ԃR�[�h
				ibuf[len++] = i;//������̒���
				while(i--){//��������R�s�[
					ibuf[len++] = *s++;
				}
				if(*s == c) s++;//"�̎���(NULL�I�[�Œ�~���Ă���\��������)

			//���@�Ⴂ�̏���
			} else { //�ǂ�ɂ��Y�����Ȃ����
				err = ERR_SYNTAX;
				return 0;//�G���[�M����Ԃ��Ă��̊֐���ł��؂�
			}
		}
	}
	ibuf[len++] = I_EOL;//�s�̏I�[�L����ۑ�
	return len;//�s�̏I�[�L�����܂߂��o�C�g��(���ꎩ�g��1�o�C�g���܂�)��Ԃ�
}

//���ԃR�[�h��1�s�������X�g�\��
//�����͍s�̒��̐擪�̒��ԃR�[�h�ւ̃|�C���^
void putlist(unsigned char* ip){
	unsigned char i;//�J��Ԃ��̃J�E���^(�ꎞ�I�ɒ��ԃR�[�h���Ӗ�����)
	short value;

	while(*ip != I_EOL){//icode�o�b�t�@�̖����܂ő�����
		if(*ip < SIZE_KWTBL){//�L�[���[�h�̏ꍇ
			c_puts(kwtbl[*ip]);
			if(IS_TOKSP(*ip) || *ip == I_SEMI)c_putch(' ');
			if(*ip == I_REM){
				ip++;
				i = *ip++;
				while(i--){
					c_putch(*ip++);
				}
				return;
			}
			ip++;
		} else
		if(*ip == I_NUM){//���l�̏ꍇ
			putnum(getvalue(ip), 0);
			ip += 3;
			if(!IS_OP(*ip) && !IS_SEP(*ip)) c_putch(' ');
		} else
		if(*ip == I_VAR){//�ϐ��̏ꍇ
			ip++;
			c_putch(*ip++ + 'A');
			if(!IS_OP(*ip) && !IS_SEP(*ip)) c_putch(' ');
		} else
		if(*ip == I_STR){//������̏ꍇ
			ip++;

			value = 0;//���̃t���O
			i = *ip;//���������擾
			while(i--){
				if(*(ip + i + 1) == '\"')
					value = 1;
			}
			if(value) c_putch('\''); else c_putch('\"');
			i = *ip++;
			while(i--){
				c_putch(*ip++);
			}
			if(value) c_putch('\''); else c_putch('\"');
		} else {//�ǂ�ɂ��Y�����Ȃ����
			err = ERR_SYS;//����͂Ȃ��Ǝv��
			return;//�G���[�M����Ԃ��Ă��̊֐���ł��؂�
		}
	}
}

//�O���[�o���ȕϐ���ύX���Ȃ�
short getsize(){
	short value;
	unsigned char* lp;

	lp = listbuf;
	while(*lp){//�����܂Ői�߂�
		lp += *lp;
	}

	value = listbuf + SIZE_LIST - lp - 1;
	return value;
}

//���ԃR�[�h�o�b�t�@�̓��e�����X�g�ɕۑ�
//�G���[���G���[�t���O�Œm�点��
void inslist(){
	unsigned char len;
	unsigned char *lp1, *lp2;

	cip = ibuf;//ip�͒��ԃR�[�h�o�b�t�@�̒��ԃR�[�h���w��(���X�g�̒��ԃR�[�h�ł͂Ȃ�)
	clp = getlp(getvalue(cip));//�s�ԍ���n���đ}���s�̈ʒu���擾

	//BUG ����s�ԍ��������ăo�b�t�@�I�[�o�[�t���[�̂Ƃ��폜�����
	//����s�ԍ������݂���ꍇ�͍폜�܂��͒u�������A�Ƃ肠�����폜
	lp1 = clp;
	if(getvalue(lp1) == getvalue(cip)){
		//�o�O�΍�̂��߂̉��}���u
		if((getsize() - *lp1) < *cip){//�����s���폜���Ă��V�����s��}���ł��Ȃ��Ȃ�
			err = ERR_LBUFOF;
			return;
		}

		lp2 = lp1 + *lp1;//�]�������v�Z
		while((len = *lp2) != 0){//�����܂ő�����
			while(len--){
				*lp1++ = *lp2++;
			}
		}
		*lp1 = 0;
	}

	//�s�ԍ������ŃX�e�[�g�����g���Ȃ���΂����������Ȃ�
	if(*cip == 4)
			return;

	//�}���\���ǂ����𒲂ׂ�
	while(*lp1){//�����܂Ői�߂�
		lp1 += *lp1;
	}

	if(*cip > (listbuf + SIZE_LIST - lp1 - 1)){//�}���s�\�Ȃ�
		err = ERR_LBUFOF;
		return;
	}
	//�󂫂����
	len = lp1 - clp + 1;
	lp2 = lp1 + *cip;
	while(len--){
		*lp2-- = *lp1--;
	}

	//�}������
	len = *cip;//�}�����钷��
	while(len--){
		*clp++ = *cip++;
	}
}

short getabs(){
	short value;

	value = getparam();
	if(err) return 0;

	if(value < 0) value *= -1;
	return value;
}

short getarray()
{
	short index;

	index = getparam();
	if(err) return 0;

	if(index < SIZE_ARAY){
		return ary[index];
	} else {
		err = ERR_SOL;
		return 0;
	}
}

short ivalue(){
	short value;

	switch(*cip){
	 case I_PLUS:
		 cip++;
		 value = ivalue();
		 break;
	 case I_MINUS:
		 cip++;
		 value = 0 - ivalue();
		 break;
	 case I_VAR:
		 cip++;
		 value = var[*cip++];
		 break;
	 case I_NUM:
		 value = getvalue(cip);
		 cip += 3;
		 break;
	 case I_ARRAY:
		 cip++;
		value = getarray();
		break;
	 case I_OPEN:
		 value = getparam();
		break;
	 case I_RND:
		cip++;
		value = getparam();
		if (err)
			return -1;
		value = getrnd(value);
		break;
	 case I_ABS:
		cip++;
		value = getabs();
		break;
	 case I_SIZE:
		cip++;
		if(*cip == I_OPEN){
			cip++;
			if(*cip == I_CLOSE)
				cip++;
			else{
				err = ERR_PAREN;
			}
		}
		value = getsize();
		break;
	case I_SW://extend
		cip++;
		if(*cip == I_OPEN){
			cip++;
			if(*cip == I_CLOSE)
				cip++;
			else{
				err = ERR_PAREN;
			}
		}
		value = (short)swstat;
		break;

	 default:
		value = 0;
		err = ERR_SYNTAX;
		break;
	}
	return value;
}

short icalc()
{
	short value1, value2;

	value1 = ivalue();
	//if(err) return value1;//������

	while(1){
		if(*cip == I_DIV){
			cip++;
			value2 = ivalue();
			//if(err)	break;////������
			if(value2 == 0){
				err = ERR_DIVBY0;
				break;
			}
			value1 /= value2;
		} else
		if(*cip == I_MUL){
			cip++;
			value2 = ivalue();
			//if(err)	break;//������
			value1 *= value2;
		} else {
			break;
		}
	}
	return value1;
}

short iexp()
{
	short value1, value2;

	value1 = icalc();
	//if(err)	return value1;

	while(*cip == I_PLUS || *cip == I_MINUS){
		value2 = icalc();
		//if(err)	break;
		value1 += value2;
	}
	return value1;
}

void iprint(){
	short value;
	short len;
	unsigned char i;

	len = 0;
	while(1){
		switch(*cip){
		case I_SEMI:
		case I_EOL:
			break;
		case I_STR:
			cip++;
			i = *cip++;
			while(i--){
				c_putch(*cip++);
			}
			break;
		case I_SHARP:
			cip++;
			len = iexp();
			if(err) return;
			break;
		default:
			value = iexp();
			if(err) return;
			putnum(value, len);
			break;
		}

		if(*cip == I_COMMA){
			cip++;
		}else{
			break;
		}
	};
	newline();
}

void iinput(){
	unsigned char i;
	short value;
	short index;

	while(1){
		switch(*cip){
		case I_STR://������Ȃ�
			cip++;
			i = *cip++;//���������擾
			while(i--){//������\��
				c_putch(*cip++);
			}
			if(*cip == I_VAR){//������̂�����낪�ϐ��Ȃ�
				cip++;
				value = getnum();
				//if(err) return;//������
				var[*cip++] = value;
			} else
			if(*cip == I_ARRAY){//������̂�����낪�z��Ȃ�
				cip++;
				index = getparam();//�C���f�b�N�X���擾
				if(err) return;
				if(index >= SIZE_ARAY){
					err = ERR_SOL;
					return;
				}
				value = getnum();
				//if(err) return;//������

				ary[index] = value;
			}
			break;
		case I_VAR:
			cip++;
			c_putch(*cip + 'A');
			c_putch(':');
			value = getnum();
			//if(err) return;//������
			var[*cip++] = value;
			break;
		case I_ARRAY:
			cip++;
			index = getparam();//�C���f�b�N�X���擾
			if(err){
				return;
			}
			if(index >= SIZE_ARAY){
				err = ERR_SOL;
				return;
			}
			c_putch('@');c_putch('(');
			putnum(index,0);
			c_putch(')');c_putch(':');
			value = getnum();
			//if(err) return;//������
			ary[index] = value;
			break;
		//default://�ǂ�ł��Ȃ���΃G���[
		//	err = ERR_SYNTAX;
		//	return;
		}

		switch(*cip){
		case I_COMMA://�R���}�Ȃ�p��
			cip++;
			break;
		case I_SEMI://�Z�~�R�����Ȃ�J��Ԃ����I��
		case I_EOL://�s���Ȃ�J��Ԃ����I��
			return;
		default://�ǂ�ł��Ȃ���΍s���G���[
			err = ERR_SYNTAX;
			return;
		}
	}
}

char iif(){
	short value1, value2;
	unsigned char i;
	
	value1 = iexp();
	if(err) return -1;

	i = *cip++;//���Z�q���擾

	value2 = iexp();
	if(err) return -1;

	switch(i){
	case I_EQ:
		return value1 == value2;
	case I_NEQ:
		return value1 != value2;
	case I_LT:
		return value1 <  value2;
	case I_LTE:
		return value1 <= value2;
	case I_GT:
		return value1 >  value2;
	case I_GTE:
		return value1 >= value2;
	default:
		 err = ERR_IFWOC;
		 return -1;
	}
}

void ivar(){
	short value;
	short index;

	index = *cip++;//�ϐ������擾
	if(*cip == I_EQ){
		cip++;
		value = iexp();
		if(err) return;
	} else {
		err = ERR_VWOEQ;
		return;
	}

	if(index < 26){
		var[index] = value;
	} else {
		err = ERR_SOL;
	}
}

void iarray(){
	short value;
	short index;

	index = getparam();//�C���f�b�N�X���擾
	if(err) return;
	if(*cip == I_EQ){
		cip++;
		value = iexp();
		if(err) return;
	} else {
		err = ERR_VWOEQ;
		return;
	}

	if(index < SIZE_ARAY){
		ary[index] = value;
	} else {
		err = ERR_SOL;
	}
}

void ilet(){
	switch(*cip){
	 case I_VAR://�ϐ��Ȃ�
		cip++;//�ϐ����֐i�߂�
		ivar();
		break;
	case I_ARRAY://�z��Ȃ�
		cip++;
		iarray();
		break;
	 default:
		err = ERR_LETWOV;
		break;
	}
}

void iled(){
    short no;
    unsigned char sw;
    
    no = iexp();
	if(err) return;
	if(*cip == I_ON){
        cip++;
        sw = 0;
    } else
    if(*cip == I_OFF){
        cip++;
        sw = 1;
    } else {
        err = ERR_SYNTAX;
        return;
    }
    led_write(no, sw);
}

//���X�g��\������
//���ԃR�[�h�|�C���^�������܂Ői��
void ilist(){
	short lineno;

	//�����𒲂ׂ�
	if(*cip == I_NUM){//�����������
		lineno = getvalue(cip);//�s�ԍ����擾
		cip += 3;
	} else {
		lineno = 0;//�Ȃ���΍s�ԍ���0
	}

	//�s�ԍ��̎w�肪�������ꍇ�͂��ꖢ���𖳎�
	for(	clp = listbuf;//���X�g�̐擪����͂��߂�
			*clp &&//���X�g�̏I�[(�擪��0)�łȂ���
			(getvalue(clp) < lineno);//�s�ԍ����w�薢���Ȃ�
			clp += *clp);//���̍s��

	while(*clp){//���X�g�̏I�[(�擪��0)�łȂ���ΌJ��Ԃ�
		putnum(getvalue(clp), 0);//�s�ԍ���\��
		c_putch(' ');
		putlist(clp + 3);
		if(err){
			break;
		}
		newline();
		clp += *clp;
	}
}

void inew(void){
	unsigned char i;

	for(i = 0; i < 26; i++)//�ϐ���������
		var[i] = 0;
	gstki = 0;//�X�^�b�N��������
	lstki = 0;//�X�^�b�N��������
	*listbuf = 0;
	clp = listbuf;//�s�|�C���^��������
}

//clp���w������1�s�̃X�e�[�g�����g�����s
//�X�e�[�g�����g�ɕ��򂪂������畡���s�����s
//���Ɏ��s����ׂ��s�̐擪��Ԃ�
//�G���[�𐶂����Ƃ���NULL��Ԃ�
unsigned char* iexe(){
	short lineno;
	unsigned char cd;
	unsigned char* lp;
	short vto, vstep;
	short index;//����ȑ傫���͂���Ȃ����G���[��h������

	while(1){
		if(c_kbhit()){
			if(c_getch() == 27){
				while(*clp){
					clp += *clp;
				}
				err = ERR_ESC;
				return clp;
			}
		}

		switch(*cip){
		case I_GOTO:
			cip++;
			lineno = iexp();
			clp = getlp(lineno);
			if(lineno != getvalue(clp)){
				err = ERR_ULN;
				return NULL;
			}
			cip = clp + 3;
			continue;
		case I_GOSUB:
			cip++;
			lineno = iexp();//�s�ԍ����擾
			if(err) return NULL;
			lp = getlp(lineno);
			if(lineno != getvalue(lp)){
				err = ERR_ULN;
				return NULL;
			}
			gpush(clp);
			gpush(cip);
			if(err) return NULL;
			clp = lp;
			cip = clp + 3;
			continue;
		case I_RETURN:
			cip = gpop();
			lp = gpop();//�X�^�b�N�̃G���[�ōs�|�C���^������Ȃ��悤��
			if(err) return NULL;
			clp = lp;
			break;
		case I_FOR:
			cip++;
			if(*cip++ != I_VAR){//�����ϐ��łȂ���Αł��؂�
				err = ERR_FORWOV;
				return NULL;
			}
			index = *cip;//�C���f�N�X���擾
			ivar();//�ϐ��ɒl�������ă|�C���^��i�߂�
			if(err) return NULL;

			if(*cip == I_TO){
				cip++;
				vto = iexp();//�I���l���擾���ă|�C���^��i�߂�
			} else {
				err = ERR_FORWOTO;
				return NULL;
			}
			if(*cip == I_STEP){//����STEP�������
				cip++;
				vstep = iexp();//�X�e�b�v�����擾���ă|�C���^��i�߂�
			} else {//STEP���Ȃ����
				vstep = 1;//�X�e�b�v��������l�ɐݒ�
			}

			lpush(clp);//�s�|�C���^��ޔ�
			lpush(cip);//���ԃR�[�h�|�C���^��ޔ�
			lpush((unsigned char*)vto);//�I���l���|�C���^�Ƃ��܂����đޔ�
			lpush((unsigned char*)vstep);//�������|�C���^�Ƃ��܂����đޔ�
			lpush((unsigned char*)index);//�ϐ���(�ϐ��ԍ�)���|�C���^�Ƃ��܂����đޔ�
			if(err) return NULL;
			break;
		case I_NEXT:
			cip++;
			if(*cip++ !=I_VAR){//�����ϐ��łȂ���Αł��؂�
				err = ERR_NEXTWOV;
				return NULL;
			}

			if(lstki < 5){
				err = ERR_LSTKUF;
				return NULL;
			}
			//�܂��X�^�b�N�̏�Ԃ�ύX���Ȃ�
			index = (short)lstk[lstki - 1];//�C���f�N�X���擾
			if(index != *cip){//�C���f�N�X���Ή����Ȃ����
				err = ERR_NEXTUM;
				return NULL;
			}
			cip++;
			vstep = (short)lstk[lstki - 2];
			var[index] += vstep;

			vto = (short)lstk[lstki - 3];
			if(	((vstep < 0) && (var[index] < vto)) ||
				((vstep > 0) && (var[index] > vto))){//���̏���������������J��Ԃ����I��
				lstki -= 5;//�X�^�b�N���̂Ă�
				break;
			}
			cip = lstk[lstki - 4];
			clp = lstk[lstki - 5];
			continue;//break;

		case I_IF:
			cip++;
			cd = iif();
			if(err){
				err = ERR_IFWOC;//�G���[�ԍ��̏�������
				return NULL;
			}
			if(cd)
				continue;
			//�����s�����̏�����REM�֗����
		case I_REM:
			//�|�C���^���s��(I_EOL�A38)�܂Ői�߂�
			//38���{���ɍs�����ǂ����͂킩��Ȃ���(������38��������Ȃ�)�A���ʂ͓���
			while(*cip != I_EOL) cip++;
			break;
		case I_STOP:
			while(*clp){
				clp += *clp;
			}
			return clp;
		case I_INPUT:
			cip++;
			iinput();
			break;
		case I_PRINT:
			cip++;
			iprint();
			break;
		case I_LET:
			cip++;
			ilet();
			break;
        case I_LED:
			cip++;
			iled();
            break;
		case I_VAR:
			cip++;
			ivar();
			break;
		case I_ARRAY:
			cip++;
			iarray();
			break;
		case I_LIST:
		case I_NEW:
		case I_RUN:
		case I_SAVE:
		case I_LOAD:
			err = ERR_COM;
			return NULL;
		}

		switch(*cip){
		case I_SEMI://�Z�~�R�����Ȃ�p��
			cip++;
			break;
		case I_EOL://�s���Ȃ�J��Ԃ����I��
			return clp + *clp;
		default://�ǂ�ł��Ȃ���΍s���G���[
			//c_puts("iexe:");putnum(*cip,0);newline();//DEBUG
			err = ERR_SYNTAX;
			return NULL;
		}
	}
}

void irun(){
	unsigned char* lp;//���Ɏ��s����s���ꎞ�I�ɕێ�

	gstki = 0;//�X�^�b�N��������
	lstki = 0;//�X�^�b�N��������
	clp = listbuf;
	
	while(*clp){//�����ɓ��B����܂ŌJ��Ԃ�
		cip = clp + 3;//�X�e�[�g�����g�̐擪
		lp = iexe();//�J�����g�s�����s���Ď��̍s�𓾂�
		if(err){//������
		//if(lp == NULL){
			return;
		}
		clp = lp;
	}
}

void icom(){
	cip = ibuf;
	switch(*cip){//�����̃R�}���h�̓}���`�X�e�[�g�����g�ŋL�q�ł��܂���
	case I_LIST:
		cip++;
		if(*cip == I_EOL || *(cip + 3) == I_EOL)
			ilist();
		else
			err = ERR_COM;
		break;
	case I_NEW:
		cip++;
		if(*cip == I_EOL)
			inew();
		else
			err = ERR_COM;
		break;
	case I_RUN:
		cip++;
		irun();//run�͂�����ɃS�~�������Ă�����
		break;
	case I_SAVE://extend
		cip++;
		if(*cip == I_BOOT){
			cip++;
			listbuf[SIZE_LIST] = I_BOOT;
		} else {
			listbuf[SIZE_LIST] = 0;
		}
		if(*cip == I_EOL)
			flash_write(listbuf);
		else
			err = ERR_COM;
		break;
	case I_LOAD://extend
        cip++;
		if(*cip == I_EOL)
			flash_read(listbuf);
		else
			err = ERR_COM;
		break;
	default:
		iexe();//��L�̂ǂ�����s����Ȃ������ꍇ
		break;
	}

	if(err && err != ERR_ESC){
		if(cip >= listbuf && cip < listbuf + SIZE_LIST && *clp)
		{
			newline(); c_puts("ERR LINE:");
			putnum(getvalue(clp), 0);//�s�ԍ���\��
			c_putch(' ');
			putlist(clp + 3);
		}
		else 
		{
			newline(); c_puts("YOU TYPE: ");
			putlist(ibuf);
		}
	}
	
}

void error(){
	newline();
	c_puts(errmsg[err]);
	newline();
	err = 0;
}

void basic(void){
	unsigned char len;

    // �����̃^�l�̃e�X�g
    //#include <xc.h>
    //c_puts("RCOUNT:");putnum(RCOUNT, 0);newline();
    //c_puts("DISICNT:");putnum(DISICNT, 0);newline();
    //c_puts("SPLIM:");putnum(SPLIM, 0);newline();
    //c_puts("ADC1BUF0:");putnum(ADC1BUF0, 0);newline();
    //c_puts("ADC1BUFF:");putnum(ADC1BUFF, 0);newline();
    //c_puts("U2TXREG:");putnum(U2TXREG, 0);newline();
    //c_puts("RTCVAL:");putnum(RTCVAL, 0);newline();
    //c_puts("ALRMVAL:");putnum(ALRMVAL, 0);newline();
    //c_puts("TMR2:");putnum(TMR2, 0);newline();

	inew();//��Ԃ�������
	c_puts("TOYOSHIKI TINY BASIC"); newline();// �N�����b�Z�[�W��\��
	c_puts(STR_EDITION);
	c_puts(" EDITION"); newline();
	if(bootflag() == I_BOOT){
		c_puts("Power on run"); newline();
		flash_read(listbuf);
		irun();
	}
	error();//�G���[���Ȃ����OK��\���A�����ŃG���[�t���O���N���A

	//1�s���͂Ə����̌J��Ԃ�
	while(1){
		c_putch('>'); // �v�����v�g
		c_gets(); // 1�s����
		len = toktoi();//���ԃR�[�h�o�b�t�@�֕ۑ�
		if(err){//�����ϊ��ł��Ȃ����
			newline(); c_puts("YOU TYPE:");
			c_puts(lbuf);
			error();
			continue;//�������Ȃ�
		}

		if(*ibuf == I_NUM){//�擪�ɐ��l������΍s�ԍ��Ƃ݂Ȃ�
			*ibuf = len;//���l�̒��ԃR�[�h���s�̒����ɒu������
			inslist();//���X�g�ɕۑ�
			if(err){
				error();//List buffer overflow
			}
		} else {
			icom();//�擪�ɐ��l���Ȃ���΃R�}���h�Ƃ��Ď��s
			error();//�G���[���Ȃ����OK��\��
		}
	}
}
