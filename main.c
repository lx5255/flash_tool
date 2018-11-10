#include <stdio.h>
#include <stdlib.h>
#include "unistd.h"
#include "direct.h"
#include "string.h"
#include <io.h>


typedef unsigned char bool;
typedef unsigned char u8;
#define false 0
#define true  1


unsigned long ld_word(u8 *in)
{
    unsigned long word;
    word = in[3]; 
    word = (word<<8)|in[2]; 
    word = (word<<8)|in[1]; 
    word = (word<<8)|in[0]; 
    printf("word %d\n", word);
    return word;
}

void st_word(u8 *out, unsigned long word)
{
    out[3] = (word>>27)&0xff; 
    out[2] = (word>>16)&0xff; 
    out[1] = (word>>8)&0xff; 
    out[0] = word&0xff; 
    printf("word %d %d %d %d\n", out[0], out[1], out[2], out[3]);
}

typedef struct 
{
	unsigned long dpt_start;
	unsigned long npart;
    unsigned long flash_size;
}flash_inf;
#define FLASH_HEAD_TAG      "flashmg"
#define DPT_START           8 
#define PART_CNT            12 
#define FLASH_SIZE_OFFSET   16 
bool get_flash_head(flash_inf *head, u8 *mbr)
{
    if((mbr ==NULL)||(head == NULL)){
        return false;
    }
    if(strcmp(FLASH_HEAD_TAG, mbr)){ 
        printf("flash tag err\n");
        return false;
    }
    printf("55aa %8x %8x\n", mbr[510], mbr[511]);
    if((mbr[510] != 0x55)||(mbr[511] != 0xaa)){
        printf("55aa err\n");
        return false; 
    }
    /* memcpy(head->tag, mbr, 8); */
    head->dpt_start = ld_word(&mbr[DPT_START]);
    head->flash_size = ld_word(&mbr[FLASH_SIZE_OFFSET]);
    head->npart = ld_word(&mbr[PART_CNT]);
    return true;
}

bool st_flash_head(u8 *mbr, flash_inf *head)
{
    if((mbr ==NULL)||(head == NULL)){
        return false;
    }
    strcpy(mbr, FLASH_HEAD_TAG);   
    st_word(&mbr[DPT_START], head->dpt_start);
    st_word(&mbr[PART_CNT], head->npart);
    st_word(&mbr[FLASH_SIZE_OFFSET], head->flash_size);
    mbr[510] = 0x55;
    mbr[511] = 0xaa;
    return true;
}

u8 *get_empty_dpt(u8 *mbr)
{
    flash_inf head;
    u8 *dpt;
    if(get_flash_head(&head, mbr) == false){ 
        printf("no flash tag\n");
        return NULL;
    }
    if(head.dpt_start > (512 - 32)){
        printf("dpt st err %d\n", head.dpt_start);
        return NULL;
    }
    dpt = &mbr[head.dpt_start];   
    for(; dpt<&mbr[512 - 32]; dpt+=32){
        printf("dpt %x\n", dpt);
        if((dpt[0] != 'D')||(dpt[1] != 'P') ||(dpt[2] != 'T')){
            return dpt;
        }
    }
    return NULL;
}

typedef struct 
{
	char part_name[12];
	unsigned long part_addr;
	unsigned long part_size;
}dpt_inf;
#define PART_NAME_OFFSET        3 
#define PART_ADDR_OFFSET        15
#define PART_SIZE_OFFSET        19 
bool get_dpt_inf(dpt_inf *inf, u8 *dpt)
{
    if((inf == NULL)||(dpt == NULL)){
        return false;
    }
    if((dpt[0] != 'D')||(dpt[1] != 'P') ||(dpt[2] != 'T')){
       return false; 
    }
    memcpy(inf->part_name, &dpt[PART_NAME_OFFSET], 12);
    inf->part_addr = ld_word(&dpt[PART_ADDR_OFFSET]);    
    inf->part_size = ld_word(&dpt[PART_SIZE_OFFSET]);    
}

bool st_dpt_inf(u8 *dpt, dpt_inf *inf)
{
    if((inf == NULL)||(dpt == NULL)){
        return false;
    }
    dpt[0] = 'D';
    dpt[1] = 'P';
    dpt[2] = 'T';
    memcpy(&dpt[PART_NAME_OFFSET],inf->part_name, 12);
    st_word(&dpt[PART_ADDR_OFFSET], inf->part_addr);
    st_word(&dpt[PART_SIZE_OFFSET], inf->part_size);
}


bool get_outfile_name(u8 *out, int argc, u8 *argv[])
{
   int arg_cnt;
   for(arg_cnt = 0; arg_cnt<argc; arg_cnt++){
       if(strcmp("-outfile", argv[arg_cnt]) == 0){ 
           if((argc - arg_cnt)>=1){
               strcpy(out, argv[arg_cnt+1]);
               return true;
           }
       }
   }
   return false;
}

u8 flash_mbr[512];
int flash_size = 0;
int copy_file(char *outfilename, char *infilename)
{
	FILE *infile = NULL, *outfile = NULL;
    int size;
    flash_inf flash_head;
    dpt_inf part_inf;
    bool resethead = false;
    u8 *mbr = &flash_mbr;
    u8 *dpt;
    u8 buff[512];
    int res = 0;

    if(strlen(infilename)>12){
        printf("infile name too long:%s\n", infilename);
        return 3;
    }


   	outfile = fopen((void *)outfilename,"rb+");
	if(outfile == NULL) {
		outfile = fopen(outfilename,  "wb+");
		if(outfile == NULL) {
			printf("new file err\n");
			return 1;
		}
	}

   	infile =  fopen(infilename,  "rb");
	if(infile == NULL) {
	    fclose(outfile);
		printf("in open err\n");
		return 2;
	}

	res =  fread((void *)mbr,  1, 512, infile);

	fseek(outfile,0L,SEEK_END);   /*利用fseek函数将指针定位在文件结尾的位置*/
	size = ftell(outfile) ;
    if(size <512){ //没有写头
       resethead = true; 
    }else{
	    fseek(outfile,0L,SEEK_SET);   
		res =  fread((void *)mbr,  1, 512,outfile);
        if(res != 512){
           printf("read file err\n"); 
           res = 4;
           goto ERR_DEAL;
        }
        if(false == get_flash_head(&flash_head, mbr)){ //头校验不对需要从写
            resethead = true; 
        }
	    fseek(outfile,0L,SEEK_END);   /*利用fseek函数将指针定位在文件结尾的位置*/
    }

    memcpy(part_inf.part_name, infilename, strlen(infilename) == 12?12:strlen(infilename)+1);
    part_inf.part_size = 0;
    //从新写FALSH头信息
    if(resethead == true){
       memset(flash_mbr, 0x00, 512);
       flash_head.flash_size = flash_size; 
       flash_head.npart = 0;
       flash_head.dpt_start = 32;
       st_flash_head(mbr, &flash_head);
       printf("resethead\n");
	   fseek(outfile,512L,SEEK_SET); 
    } 

    part_inf.part_addr = ftell(outfile); 
    if(part_inf.part_addr %512){ //对齐
       part_inf.part_addr += 512 - part_inf.part_addr%512; 
	   fseek(outfile,part_inf.part_addr,SEEK_SET); 
    }
	fseek(infile,0L,SEEK_SET); 
	while(1){  //拷贝数据
		res =  fread((void *)buff,  1, 512,infile);
		if(res == 0) {
			break;
		}
		res =  fwrite((void *)buff, 1, res, outfile);
		if(res ==  0) {
			printf("write err  %d\n", res);
            res = 5;
            goto ERR_DEAL;
		}
        part_inf.part_size += res;
	}
    dpt = get_empty_dpt(mbr);  //获取一个可写的DPT
    if(dpt == NULL){
        res = 6;
        goto ERR_DEAL;
    }
    st_dpt_inf(dpt, &part_inf);

    flash_head.npart++;
    st_flash_head(mbr, &flash_head);

    //将MBR写入文件
	fseek(outfile,0L,SEEK_SET); 
    res =  fwrite((void *)mbr, 1, 512, outfile);
    if(res != 512){
        res = 6;
    }else{
        res = 0; 
    }

ERR_DEAL:
	fclose(infile);
	fclose(outfile);
	return res;

}


u8 outfile_name[512];
bool outfile_flag = 0; 
int main(int argc, char *argv[])
{
	int res = 0; 
    bool is_file = false;

    printf("Hello world!\n");

    outfile_flag = get_outfile_name(outfile_name, argc, argv);
    if(outfile_flag == false){
        strcpy(outfile_name, "flash_image.bin");         
    }

    printf("enter flash size(MB)：");
    scanf("%d",&flash_size);
    printf("\n");

    if(flash_size > 32){
        printf("flash size err\n");
        return 1;
    }
    flash_size = flash_size*1024*1024;
	res = _unlink(outfile_name);
	printf("del file res %d", res);

    u8 arg_cnt;
    for(arg_cnt = 0; arg_cnt<argc; arg_cnt++){
        printf("arg:%s\n", argv[arg_cnt]);
        if(strcmp("-infile", argv[arg_cnt]) == 0){ 
            is_file = true;
        }else if(strcmp("-outfile", argv[arg_cnt]) == 0){
            is_file = false;
        }else{
           if(is_file == true){
                res = copy_file(outfile_name, argv[arg_cnt]);            
                if(res){
                   printf("cope file %s err %d\n", argv[arg_cnt], res); 
	               res = _unlink(outfile_name);
                   return 1; 
                }
           }
        }
    }

    return 0;
}
