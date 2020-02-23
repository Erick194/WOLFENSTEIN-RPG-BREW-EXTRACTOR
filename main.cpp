
#include <windows.h>
#include <conio.h>
#include <stdio.h>

#include "funciones.h"
#include "zip.h"
#include "unzip.h"
#include <png.h>

using namespace std;

HZIP hz; DWORD writ;

char string[64] = { 0 };

int getNumTableBytes(int paramInt);
int getNumTableShorts(int paramInt);
int getNumTableInts(int paramInt);
void loadByteTable(byte *cache, int paramInt);
void loadShortTable(short *cache, int paramInt);
void loadIntTable(int *cache, int paramInt);

short *skyMapPalette;

typedef byte*  cache;
static cache writeData;
static unsigned int current = 0;

//**************************************************************
//**************************************************************
//  Png_WriteData
//
//  Work with data writing through memory
//**************************************************************
//**************************************************************

static void Png_WriteData(png_structp png_ptr, cache data, size_t length) {
    writeData = (byte*)realloc(writeData, current + length);
    memcpy(writeData + current, data, length);
    current += length;
}

//**************************************************************
//**************************************************************
//  Png_Create
//
//  Create a PNG image through memory
//**************************************************************
//**************************************************************

static int TablesOffsets[20] = {0};
cache Png_Create(cache data, int* size, int width, int height, int paloffset, int offsetx = 0, int offsety = 0, bool sky = false)
{
    int i, j;
    cache image;
    cache out;
    cache* row_pointers;
    png_structp png_ptr;
    png_infop info_ptr;
    png_colorp palette;
    
    int bit_depth = 4;
    int palsize = 16;
    
    if(sky)
    {
        bit_depth = 8;
        palsize = 256;
    }
    
    // setup png pointer
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    if(png_ptr == NULL) {
        error("Png_Create: Failed getting png_ptr");
        return NULL;
    }

    // setup info pointer
    info_ptr = png_create_info_struct(png_ptr);
    if(info_ptr == NULL) {
        png_destroy_write_struct(&png_ptr, NULL);
        error("Png_Create: Failed getting info_ptr");
        return NULL;
    }

    // what does this do again?
    if(setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        error("Png_Create: Failed on setjmp");
        return NULL;
    }

    // setup custom data writing procedure
    png_set_write_fn(png_ptr, NULL, Png_WriteData, NULL);

    // setup image
    png_set_IHDR(
        png_ptr,
        info_ptr,
        width,
        height,
        bit_depth,
        PNG_COLOR_TYPE_PALETTE,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT);

    // setup palette
    FILE *fpal = fopen ("Datos/newPalettes.bin","rb");
    if(!fpal)
    {
       error("No puede abrir newPalettes.bin");
    }

    int gettrans = -1;
    if(sky)
    { 
       palette = (png_colorp) malloc((palsize*3)*png_sizeof(png_color));
       
       for(int x = 0;x < 256;x++)
       {
          int Palbit = skyMapPalette[x];
          int B = (B = Palbit & 0x1F) << 3 | B >> 2;
          int G = (G = Palbit >> 5 & 0x3F) << 2 | G >> 4;
          int R = (R = Palbit >> 11 & 0x1F) << 3 | R >> 2;
          //printf("skyMapPalette[%d] %x\n",x, skyMapPalette[x]);
          //printf("R %x||G %x||B %x\n",R, G, B);
     
          palette[x].red = R;
          palette[x].green = G;
          palette[x].blue = B;
       }
    }
    else
    {
        fseek(fpal,(paloffset),SEEK_SET);
            
        palette = (png_colorp) malloc((palsize*3)*png_sizeof(png_color));

        for(int x = 0;x < palsize;x++)
        {
           int Palbit = ReadWord(fpal);
           int B = (B = Palbit & 0x1F) << 3 | B >> 2;
           int G = (G = Palbit >> 5 & 0x3F) << 2 | G >> 4;
           int R = (R = Palbit >> 11 & 0x1F) << 3 | R >> 2;
     
           palette[x].red = R;
           palette[x].green = G;
           palette[x].blue = B;
           
           if(gettrans == -1)
           {
              if(palette[x].red==255 && palette[x].green==0 && palette[x].blue==255)
                 gettrans = x;
              else if(palette[x].red==255 && palette[x].green==4 && palette[x].blue==255)
                 gettrans = x;
           }
        }
        fclose(fpal);
    }
    
    png_set_PLTE(png_ptr, info_ptr,palette,palsize);
    
    if(gettrans != -1)
    {
        png_byte trans[gettrans+1]; 
        for(int tr =0;tr < gettrans+1; tr++)
        {
          if(tr==gettrans){trans[tr]=0;}
          else {trans[tr]=255;}
        }
        png_set_tRNS(png_ptr, info_ptr,trans,gettrans+1,NULL);
    }

    // add png info to data
    png_write_info(png_ptr, info_ptr);

    if(offsetx !=0 || offsety !=0)
    {
       int offs[2];
    
       offs[0] = Swap32(offsetx);
       offs[1] = Swap32(offsety);
    
       png_write_chunk(png_ptr, (png_byte*)"grAb", (byte*)offs, 8);
    }

    // setup packing if needed
    png_set_packing(png_ptr);
    png_set_packswap(png_ptr);

    // copy data over
    byte inputdata;
    image = data;
    row_pointers = (cache*)malloc(sizeof(byte*) * height);
    for(i = 0; i < height; i++)
    {
        row_pointers[i] = (cache)malloc(width);
        for(j = 0; j < width; j++)
        {
            inputdata = *image;
            //if(inputdata == 0x7f){inputdata = (byte)gettrans;}
            row_pointers[i][j] = inputdata;
            image++;
        }
    }

    // cleanup
    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, info_ptr);
    free((void**)&palette);
    free((void**)row_pointers);
    palette = NULL;
    row_pointers = NULL;
    png_destroy_write_struct(&png_ptr, &info_ptr);

    // allocate output
    out = (cache)malloc(current);
    memcpy(out, writeData, current);
    *size = current;

    free(writeData);
    writeData = NULL;
    current = 0;

    return out;
}

//--------Flip--------//
static void flip(byte *input, byte *output, int width, int height)
{
    int i;
    int length = (width*height);
    //printf("sizeof(input) %d\n",length);
    
    byte pixel;
    int count = 0;
    int offset = 1;
    for(int i=0; i < height; i++)
    {
        for(int j=0; j < width; j++)
        {
            pixel = (byte)input[count];
            output[(length-width*offset)+j] = (byte)pixel;
            count++;
        }
        offset+=1;
    }
}

//--------Rotar 90º Y Flip--------//
/*static void rotate90andflip(byte *input, byte *output, int width, int height)
{
    int i;
    int length = (width*height);
    int height0 = height;//width
    int width0 = length / height;//height
    //printf("sizeof(input) %d\n",length);
    
    int bit1 = width0;
    int bit2 = 1;
    int bit3 = 0;
    
    int offset = length;
    
    byte pixel;
    for(i = 0; i < length; i++)
    {
    pixel = (byte)input[i];
    output[length - offset + bit3] = (byte)pixel;
    
    if(bit2 == height0){offset = length;}
    else{offset = length - bit1;}
    
    if(bit2 == height0){bit2 = 0; bit3 += 1;}
    bit2 +=1;
    bit1 = width0 * bit2;
    }
}*/

static int pngcount2 = 0;
void Create_Walls(int file, int startoff,int count,int paloffset, int x0, int x1, int y0, int y1)
{
    char string2[64] = { 0 };
    //printf("x0 %d x1 %d y0 %d y1 %d\n",x0,x1,y0,y1);
    
    int height = y1-y0;
    int height2 = 64;
    if(height <= 64){height2 = 64;}
    if(height <= 32){height2 = 32;}
    if(height <= 16){height2 = 16;}
    if(height <= 8){height2 = 8;}
    if(height <= 4){height2 = 4;}
    if(height <= 2){height2 = 2;}
    if(height <= 0){height2 = 64;}
    height += (height2-height);
    int width = (count/height)*2;

    int mode;

    //printf("width %d || height %d || count %d\n",width,height, count);
    
    sprintf(string2,"Datos/tex%2.2d.bin",file);
    
    FILE *in;
    in = fopen(string2,"rb");
    if(!in)
    {
       error("No puede abrir %s", string2);
    }

    cache input;
    cache output;
    cache pngout;
    
    input = (byte*)malloc(count*2);
    output = (byte*)malloc(count*2);
    
    for(int a = 0; a < (count*2); a++)
    {
       input[a] = 0x00;
    }
    //printf("startoff %d\n", startoff);
    fseek(in, startoff,SEEK_SET);

    byte pixel;
    int pix = 0;
    int pix2 = 0;

    for(int a = 0; a < count; a++)
    {
       pixel = ReadByte(in);
       input[pix] = (byte)(pixel & 0xF);//0xff;
       input[pix+1] = (byte)(pixel >> 4 & 0xF);//0xff;
       pix+=2;
    }
    
    flip(input, output, width, height);
    
    /*FILE *outraw;
    outraw = fopen("tmp.raw","wb");
    
    for(int a = 0; a < count*2; a++)
    {
       fputc(output[a], outraw);
    }
    fclose(outraw);
    getch();*/

    int pngsize = 0;
    pngout = Png_Create(output, &pngsize,width,height, paloffset);

    sprintf(string,"WTEX/WTEX%03d",pngcount2);
    ZipAdd(hz, string, pngout, pngsize);
    pngcount2++;

    fclose(in);
    free(input);
    free(output);
    free(pngout);
}

static int pngcount = 0;
void Create_Sprites(int file, int startoff,int count,int paloffset, int x0, int x1, int y0, int y1)
{
    //printf("x0 %d x1 %d y0 %d y1 %d\n",x0,x1,y0,y1);
    
    int width = x1-x0;
    int height = y1-y0;
    char string2[64] = { 0 };
    int endoffset, shapeoffset;
    
    //printf("width %d height %d\n",width,height);//getch();
    
    sprintf(string2,"Datos/tex%2.2d.bin",file);
    
    FILE *in;
    in = fopen(string2,"rb");
    if(!in)
    {
     error("No puede abrir %s", string2);
    }
    
    endoffset = (startoff+count)-2;
    fseek(in, endoffset,SEEK_SET);
    
    shapeoffset = endoffset - ReadWord(in);
    fseek(in, shapeoffset,SEEK_SET);
    
    //printf("shapeoffset %d\n",shapeoffset);
    //printf("endoffset %d\n",endoffset);
    
    
    int i = 0;
    int j = 0;
    int k = 0;
    int linecount = 0;
    int shapetype[256] = {0};
    byte shape;
    byte shapeBit;
    for(i = 0; i < height; i++)
    {
       if(!(i & 1))
          shape = ReadByte(in);
          
       if(!(i & 1))
          shapeBit = (byte)(shape & 0xF);
       else 
          shapeBit = (byte)(shape >> 4 & 0xF);
     
       shapetype[i] = shapeBit;
    }

    linecount = i;
    //printf("linecount %d\n",linecount);
    
    byte* input;
    byte* output;
    byte* pngout;
    
    //height = 64;//con y offset puesto
    input = (byte*)malloc(width*height);
    output = (byte*)malloc(width*height);
    memset(input, 0 , width*height); 
    memset(output, 0 , width*height); 

    //printf("pos %d\n",ftell(in));
    
    int pos = 0;
    int cnt = 0;

    for(i = 0; i < linecount; i++)
    {
       if(shapetype[i] != 0)
       {
           for(j = 0; j < shapetype[i]; j++)
           {
               //pos = ReadByte(in);//con x offset puesto
               pos = (ReadByte(in) - x0);
               cnt = ReadByte(in);
               for(k = 0; k < cnt; k++)
               {
           	      input[((i*width)+ pos)+k] = 0xff;
               }
           }
       }
    }
    
    byte pixel2;
    int pix = 0;
    int pix2 = 0;
    //printf("startoff %d\n",startoff);
    
    for(int a = 0; a < (width*height); a++)
    {
       if(input[a] == 0xff)
       {
          fseek(in,startoff+pix,SEEK_SET);
          pixel2 = ReadByte(in);
                    
          if(pix2)
          {
             input[a] = (byte)(pixel2 >> 4 & 0xF);//0xff;
          }
          else
          {
             input[a] = (byte)(pixel2 & 0xF);//0xff;
          }
          if(pix2 == 1)
          {
             pix+=1;
             pix2 = 0;
          }
          else
          {
             pix2+=1;
          }
       }  
    }

    //rotate90andflip(input, output, width, height);
    
    /*FILE *outraw;
    outraw = fopen("tmp.raw","wb");
    
    for(int a = 0; a < (width*height); a++)
    {
       fputc(input[a], outraw);
    }
    fclose(outraw);
    */
    
    //printf("paloffset %d\n",paloffset);
    //getch();

    int offsetx = (32-x0);
    int offsety = (64-y0)+1;
    int pngsize = 0;
    pngout = Png_Create(input, &pngsize,width,height, paloffset, offsetx, offsety);

    sprintf(string,"STEX/STEX%03d",pngcount);
    ZipAdd(hz, string, pngout, pngsize);
    pngcount++;
    
    fclose(in);
    free(input);
    free(output);
    free(pngout);
}

sword mediaMappings[512];
byte mediaDimensions[1024];
byte mediaBounds[4096];//x1x2y1y2
sword mediaPalColors[1024];
sword mediaTexelSizes[1024];

static void RegisterMedia(int paramInt)
{
    int pos, cnt;
    cnt = mediaMappings[paramInt];
    paramInt = mediaMappings[(paramInt + 1)];
    //printf("cnt %d paramInt %d\n",cnt,paramInt);
    while (cnt < paramInt)
    {
      pos = cnt;
      if (mediaPalColors[pos] & 0x8000)
      {
        pos = mediaPalColors[pos] & 0x3FF;
      }
      mediaPalColors[pos] |= 0x4000;
      //printf("pos %d Pal %d\n",pos ,mediaPalColors[pos]);
      
      pos = cnt;
      if (mediaTexelSizes[pos] & 0x8000)
      {
        pos = mediaTexelSizes[pos] & 0x3FF;
      }
      mediaTexelSizes[pos] |= 0x4000; 
      
      //printf("pos %d mediaTexelSizes %d\n",pos ,mediaTexelSizes[pos]);
      cnt++;
    }
}

static int Texels[4096] = {0};
static int SpriteOffset[4096][3] = {0,0,0};
static int TextureOffset[4096][3] = {0,0,0};
static int PalsOffset[4096] = {0};


static int Grapics[4096][4] = {0,0,0,0};

static void FinalizeMedia()
{
    int u = 0;
    int v = 0;
    int i5 = 0;
    int i6 = 1;
    int i4 = 0;
    int i8 = 0;
    int i9 = 0;
    int i10 = 0;
    int i11 = 0;
    int i12 = 0;
    int i7 = 0;
    for (i7 = 0; i7 < 1024; i7++)
    {
      i8 = (mediaTexelSizes[i7] & 0x4000) != 0 ? 1 : 0;
      i9 = (mediaPalColors[i7] & 0x4000) != 0 ? 1 : 0;
      
      //printf("i7 %d\n",i7);
      //printf("i8 %d\n",i8);//mediaTexelSizes
      //printf("i9 %d\n",i9);//mediaPalColors
      if (i8 != 0)
      {
        if ((((i8 = i10 = (mediaTexelSizes[i7] & 0x3FFF) + 1) - 1 & i8) == 0 ? 1 : 0) != 0)
        {
          //printf("i6 %d i10 %d\n",i6,i10+4);//mediaTexelSizes
         // printf("v8 %d\n",(mediaTexelSizes[i7] & 0x3FFF) + 1);//mediaTexelSizes
          //getch();
          //mediaTexelSizes[i7] |= 0x1000;
        }
        else
        {
          i4 += i10;
        }
      }
      if (i9 != 0)
      {
        if ((i10 = mediaPalColors[i7] & 0x3FFF) < 256) {
          i10 = 256;
        }
        //printf("i5 %d i10 %d\n",i6,i10);//mediaPalColors
        i5++;
      }
      //getch();
    }
    
    //printf("\n\n\n");
    i5 = 0;
    int paloffs = 0;
    for (i7 = 0; i7 < 1024; i7++)
    {
      i8 = (mediaPalColors[i7] & 0x4000) != 0 ? 1 : 0;
      i9 = (mediaPalColors[i7] & 0x8000) != 0 ? 1 : 0;
      i10 = mediaPalColors[i7] & 0x3FFF;
      
      //printf("i8 %d i9 %d i10 %d\n",i8,i9,i10);//mediaPalColors
      if ((i8 != 0) && (i9 == 0))
      {
        //printf("paloffs %d i5 %d\n",paloffs,i5);//mediaPalColors
        PalsOffset[i5] = paloffs;
        i11 = (short)(i7 | 0x8000);
        for (i12 = i7 + 1; i12 < 1024; i12++) {
          if (mediaPalColors[i12] == i11) {
            mediaPalColors[i12] = ((short)(0xC000 | i5));
          }
        }
        mediaPalColors[i7] = ((short)(0x4000 | i5));
        i5++;
        paloffs += (i10 << 1)/* + 4*/;
      }
      else if (i9 == 0)
      {
        //printf("i9 mediaPalColors i10 %d\n",i10);//mediaPalColors
       
      }
      //getch();
    }
    
    i6 = 0;
    i7 = -1;
    i8 = 0;
    i9 = 0;
    i10 = 0;
    i11 = 0;
    i12 = 0;
    i5 = 0;
    
    int i15 = 0;
    //printf("\n\n\n");
    int v29, v32, t1, t2;
    for (int i1 = 0; i1 < 1024; i1++)
    {
      int i13 = (mediaTexelSizes[i1] & 0x3FFF) + 1;
      if(!(mediaTexelSizes[i1] & 0x4000) || (mediaTexelSizes[i1] & 0x8000))
      {
         if (!(mediaTexelSizes[i1] & 0x8000))
            i9 += i13 + 0;
      }
      else
      {
          if (i10 != i7)
          {
             i7 = i10;
             i8 = 0;
             //printf("i7 %d\n",i7);//mediaTexelSizes
          }
          
          if (i8 != i9)
          {
             //printf("i9 - i8 %d\n",i9 - i8);//mediaTexelSizes
          }

          if ((((t1 = t2 = (mediaTexelSizes[i1] & 0x3FFF) + 1) - 1 & t1) == 0 ? 1 : 0) != 0)
          {
              //printf("i5 %d v8 %d fl %x\n",i5, (mediaTexelSizes[i1] & 0x3FFF) + 1, mediaTexelSizes[i1]);//mediaTexelSizes
              if(i5 > 210)
              {
                    mediaTexelSizes[i1] |= 0x2000;
              }
              //getch();
          }
          
          Grapics[i5][0] = mediaTexelSizes[i1] & (0x2000);
          Grapics[i5][1] = i7;
          Grapics[i5][2] = i8;
          Grapics[i5][3] = (i13);
          
          //printf("i5 %d Mode [%x] || File [%d] || Start [%d] || Size [%d]\n",i5, Grapics[i5][0],Grapics[i5][1],Grapics[i5][2],Grapics[i5][3]);
          //getch();
          //printf("i5 %d i13 %d\n",i5, i13);
          
          
          for (v32 = i1 + 1; v32 < 1024; v32++)
          {
              if (mediaTexelSizes[v32] == (short)(i1 | 0x8000))
              {
                mediaTexelSizes[v32] = ((short)(i5 | 0xC000));
              }
          }
          
          mediaTexelSizes[i1] = i5++ | 0x4000;
          
          i9 += i13 + 0;
          i8 = i9;
      }
      if ( i9 > 0x4000 )
      {
          i10++;
          i9 = 0;
      }
    }
}

static void extraer(int paramInt1)
{
    //system("CLS");
    int i1 = paramInt1;
    //printf("i1 %d\n",i1);

    int file = 0;
    int startoff = 0;
    int count = 0;
    int paramInt, x0, x1, y0, y1;

    bool sprite = false;

    paramInt1 = mediaTexelSizes[i1] & 0x1FFF;
    //printf("paramInt1 %d\n",paramInt1);
    
    //printf("Grapics[%d][0] %X\n",paramInt1, Grapics[paramInt1][0]);

    if(Grapics[paramInt1][0] & 0x2000)
    {
        file = Grapics[paramInt1][1];
        startoff = Grapics[paramInt1][2];
        count = Grapics[paramInt1][3];
        //printf("1File %d\n",file);
        //printf("1OffsetS %d\n",startoff);
        //printf("1OffsetE %d\n",count);
        sprite = false;
        //getch();
    }
    else
    {
        file = Grapics[paramInt1][1];
        startoff = Grapics[paramInt1][2];
        count = Grapics[paramInt1][3];
        //printf("2File %d\n",file);
        //printf("2OffsetS %d\n",startoff);
        //printf("2OffsetE %d\n",count);
        sprite = true;
    }
    
    
    paramInt = mediaPalColors[i1] & 0x3FF;
    //printf("pal %d\n",paramInt);//arrayOfInt1[(mediaPalColors[i1] & 0x3FF)]);
    //printf("PalsOffset %d\n",PalsOffset[paramInt]);
    x0  = (mediaBounds[(i1 << 2)] & 0xFF);
    x1  = (mediaBounds[((i1 << 2) + 1)] & 0xFF);
    y0  = (mediaBounds[((i1 << 2) + 2)] & 0xFF);
    y1  = (mediaBounds[((i1 << 2) + 3)] & 0xFF);
    //printf("x0 %d x1 %d y0 %d y1 %d\n",x0,x1,y0,y1);
  
    if (sprite)
    {
        Create_Sprites(file,startoff,count,PalsOffset[paramInt], x0, x1, y0, y1);
    }
    else
    {
        Create_Walls(file,startoff,count,PalsOffset[paramInt], x0, x1, y0, y1);
    }
}

// New like original source 
#define  MaxFiles 18
FILE *tables;
int tableOffsets[MaxFiles];
int prevOffset;

void initTableLoading()
{
     FILE *fp = fopen("Datos/tables.bin","rb");
     if(!fp) { error("No puede abrir tables.bin");}
    
     for (int i = 0; i < MaxFiles; i++)
     {
         tableOffsets[i] = ReadUint(fp);
         //printf("tableOffsets[%d] %d\n",i,tableOffsets[i]);
     }
     fclose(fp);
}

void beginTableLoading()
{
     if(tables) {fclose(tables); tables = 0;}
     tables = fopen("Datos/tables.bin","rb");
     
     prevOffset = 0;
     fseek(tables, MaxFiles*sizeof(int),SEEK_SET);
}

void seekTable(int paramInt)
{
     int i = 0;
     if (paramInt > 0)
     {
        i = tableOffsets[(paramInt - 1)];
     }
     if (i < prevOffset)
     {
        error("seekTable seeking backwards");
     }
     fseek(tables, i+(MaxFiles*sizeof(int)),SEEK_SET);
     //printf("newOffset %d\n",i+(MaxFiles*sizeof(int)));
     //bufSkip(prevIS, i - prevOffset, false);
     prevOffset = i;
     //printf("prevOffset %d\n",prevOffset);
}

void finishTableLoading()
{
     fclose(tables);
     tables = 0;
}

int getNumTableBytes(int paramInt)
{
    int i = tableOffsets[paramInt] - 4;
    if (paramInt == 0) {
      return i;
    }
    return i - tableOffsets[(paramInt - 1)];
}

int getNumTableShorts(int paramInt)
{
    int i = tableOffsets[paramInt] - 4;
    if (paramInt == 0) {
      return i / 2;
    }
    return (i - tableOffsets[(paramInt - 1)]) / 2;
}
  
int getNumTableInts(int paramInt)
{
    int i = tableOffsets[paramInt] - 4;
    if (paramInt == 0) {
      return i / 4;
    }
    return (i - tableOffsets[(paramInt - 1)]) / 4;
}

void loadByteTable(byte *cache, int paramInt)
{
    seekTable(paramInt);
    int i = ReadUint(tables);
    fread (cache, sizeof(byte), i, tables);
    prevOffset += 4 + i;
}

void loadShortTable(short *cache, int paramInt)
{
    seekTable(paramInt);
    int i = ReadUint(tables);
    i *= 2;
    int j = 0;
    int k = 0;
    while (j < i)
    {
        int m = j + 40960 > i ? i - j : 40960;
        for (int n = 0; n < m; n += 2)
        {
            cache[(k++)] = ReadSword(tables);
        }
        j += 40960;
    }
    prevOffset += 4 + i;
}

void loadIntTable(int *cache, int paramInt)
{
    seekTable(paramInt);
    int i = ReadUint(tables);
    i *= 4;
    int j = 0;
    int k = 0;
    while (j < i)
    {
        int m = j + 40960 > i ? i - j : 40960;
        for (int n = 0; n < m; n += 4)
        {
             cache[(k++)] = ReadUint(tables);
        }
        j += 40960;
    }
    prevOffset += 4 + i;
}

void ExtraerSKYS(int idx, int num)
{
    int sizepal = getNumTableShorts(idx-1);
    int sizetex = getNumTableBytes(idx);

    skyMapPalette = (short*)malloc(sizepal);
    byte skyMapTexels[sizetex];
    
    beginTableLoading();
    loadShortTable(skyMapPalette, idx-1);
    
    beginTableLoading();
    loadByteTable(skyMapTexels, idx);

    //DEBUG
    /*
    FILE *outraw;
    outraw = fopen("uot.raw","wb");
    for(int i = 0; i < sizepal; i++)
    {
        fwrite (&skyMapPalette[i], sizeof(short), 1, outraw);
    }
    fclose(outraw);
    
    outraw = fopen("tmp.raw","wb");
    for(int i = 0; i < sizetex; i++)
    {
        fwrite (&skyMapTexels[i], sizeof(byte), 1, outraw);
    }
    fclose(outraw);
    */
    
    
    int pngsize = 0;
    cache pngout = Png_Create(skyMapTexels, &pngsize, 128, 128, 0, 0, 0, true);
    
    //DEBUG
    /*
    sprintf(string,"SKY%02d.png",num);
    FILE *f2 = fopen(string,"wb");
    for(int i = 0; i < pngsize; i++)
    fputc(pngout[i], f2);
    fclose(f2);
    */

    sprintf(string,"SKYS/SKY%02d", num);
    ZipAdd(hz, string, pngout, pngsize);
    pngcount++;

    free(&skyMapTexels);
    free(pngout);
}

void ShowInfo()
{
    setcolor(0x07);printf("     #############");
    setcolor(0x0A);printf("(ERICK194)");
    setcolor(0x07);printf("##############\n"); 
    printf("     # WOLFENSTERIN RPG (BREW) EXTRACTOR #\n");
    printf("     #  CREADO POR ERICK VASQUEZ GARCIA  #\n");
    printf("     #   ES PARA WOLFENSTERIN RPG(BREW)  #\n");
    printf("     #           VERSION 1.0             #\n");
    printf("     #           MODO DE USO:            #\n");
    printf("     #    LEER EL LEEME.TXT/README.TXT   #\n");
    printf("     #####################################\n");
    printf("\n");
}

typedef struct fdata_s
{
	byte type;
	int  offset;
} fdata_t;

fdata_t *files;

int main(int argc, char *argv[])
{
    ShowInfo();

    FILE *f1, *f2, *f3;
    char fname[64];
    char name[256];
    bool gzip;
 
    f1 = fopen("Datos/wolfensteinrpg.idx","rb");
    if(!f1) { error("No puede abrir wolfensteinrpg.idx"); }
    
    f2 = fopen("Datos/wolfensteinrpg00.bin","rb");
    if(!f2) { error("No puede abrir wolfensteinrpg00.bin"); }
    
    
    int count_files = ReadWord(f1);
    //printf("count_files %d\n",count_files);
    
    files = (fdata_t *)malloc(sizeof(fdata_t)*count_files+1);
    
    for(int i = 0; i <= count_files; i++)
    {
        files[i].type = ReadByte(f1);
        files[i].offset = ReadUint(f1);
    }
    
    //Extraes Archivos Zip y Descomprimir
    for(int i = 0; i <= count_files; i++)
    {
        PrintfPorcentaje(i,count_files-1,true, 10,"Extrayendo Archivos......");
        
        //printf("files[%d].type %d\n",i,files[i].type);
        //printf("files[%d].offset %d\n",i,files[i].offset);
        
        if(files[i].type == 0)
        {
            gzip = false;

            if(i == 7)
                 sprintf(name,"Datos/flak.bmp");
            else if(i == 10)
                 sprintf(name,"Datos/Horz_Kicking.bmp");
            else if(i == 42)
                 sprintf(name,"Datos/newMappings.bin");
            else if(i == 43)
                 sprintf(name,"Datos/newPalettes.bin");
            else if(i == 49)
                 sprintf(name,"Datos/sounds.bin");
            else if(i == 52)
                 sprintf(name,"Datos/strings0.bin");
            else if(i == 53)
                 sprintf(name,"Datos/strings1.bin");
            else if(i == 74)
                 sprintf(name,"Datos/Vert_Kicking.bmp");
            else if(i == 75)
                 sprintf(name,"Datos/WarImages.bmp");
            else if(i == 76)
                 sprintf(name,"Datos/WarTable.bmp");
            else
            {
                sprintf(name,"Datos/tmp.gz", i);
                gzip = true;
            }
            
            fseek(f2, files[i].offset,SEEK_SET);
            int count = files[i+1].offset - files[i].offset;
            
            f3 = fopen(name,"wb");
            for(int j = 0; j < count; j++)
            {
                fputc(ReadByte(f2), f3);
            }
            fclose(f3);
            
            //Extrae Dato Del Archivo Gz
            if(gzip)
            {
                SHELLEXECUTEINFO ShExecInfo = {0};
                ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
                ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
                ShExecInfo.hwnd = NULL;
                ShExecInfo.lpVerb = NULL;
                ShExecInfo.lpFile = "gunzip.bat";		
                ShExecInfo.lpParameters = "";	
                ShExecInfo.lpDirectory = "Datos";
                ShExecInfo.nShow = SW_HIDE;
                ShExecInfo.hInstApp = NULL;	
                ShellExecuteEx(&ShExecInfo);
                WaitForSingleObject(ShExecInfo.hProcess,INFINITE);
            }
        }
    }

    free(files);
    fclose(f1);
    fclose(f2);

    f1 = fopen("Datos/newMappings.bin","rb");
    if(!f1)
    {
        error("No puede abrir newMappings.bin");
    }
    
    hz = CreateZip("WolfensteinRpgBrew.pk3",0);
 
    int i = 0;
    
    for(i = 0; i < 512; i++)
    {
        mediaMappings[i] = ReadSword(f1);
    }
    for(i = 0; i < 1024; i++)
    {
        mediaDimensions[i] = ReadByte(f1);
    }
    for(i = 0; i < 4096; i++)
    {
        mediaBounds[i] = ReadByte(f1);
    }
    for(i = 0; i < 1024; i++)
    {
        mediaPalColors[i] = ReadSword(f1);
    }
    for(i = 0; i < 1024; i++)
    {
        mediaTexelSizes[i] = ReadSword(f1);
    }
    
    fclose(f1);
    
    for(i = 0; i < 512; i++)
    {
        if(mediaMappings[i] != -1)
        {
            if(mediaMappings[i+1] == -1) continue;
            RegisterMedia(i);
        }
    }
    
    FinalizeMedia();

    for(i = 0; i < 512; i++)
    {
        PrintfPorcentaje(i,512-1,true, 11,"Extrayendo Sprites Y Texturas......");
        if(mediaMappings[i] != -1)
        {
            if(mediaMappings[i+1] == -1) continue;
    
            int i1 = mediaMappings[i];
            int paramInt = mediaMappings[(i + 1)];
    
            while (i1 < paramInt)
            {
                extraer(i1);
                i1++;
            }
        }
    }
    
    initTableLoading();
    PrintfPorcentaje(1,0,true, 12,"Extrayendo Sky Textura......");
    ExtraerSKYS(17, 0);
    finishTableLoading();

    //Extraer Midis
    FILE *fp = fopen("Datos/sounds.idx","rb");
    if(!fp) { error("No puede abrir sounds.idx");}
    
    f2 = fopen("Datos/sounds.bin","rb");
    if(!f2) { error("No puede abrir sounds.bin"); }
    
    count_files = ReadWord(f1);
    printf("count_files %d\n",count_files);
    
    files = (fdata_t *)malloc(sizeof(fdata_t)*count_files+1);
    
    for(int i = 0; i <= count_files; i++)
    {
        files[i].type = ReadByte(f1);
        files[i].offset = ReadUint(f1);
    }
    
    for(int i = 0; i <= count_files; i++)
    {
        PrintfPorcentaje(i,count_files-1,true, 13,"Extrayendo Midis......");
        
        if(files[i].type == 0)
        {
            /*sprintf(name,"Datos/%02d.mid", i);

            fseek(f2, files[i].offset,SEEK_SET);
            int count = files[i+1].offset - files[i].offset;
            
            f3 = fopen(name,"wb");
            for(int j = 0; j < count; j++)
            {
                fputc(ReadByte(f2), f3);
            }
            fclose(f3);*/
            
            fseek(f2, files[i].offset,SEEK_SET);
            int size = files[i+1].offset - files[i].offset;
            
            byte* input = (byte*)malloc(size);
            for(int j = 0; j < size; j++)
            {
                input[j] = ReadByte(f2);
            }
            
            sprintf(string,"SOUND/%02d.mid", i);
            ZipAdd(hz, string, input, size);
            
            free(input);
        }
    }
    
    free(files);
    fclose(f1);
    fclose(f2);

    CloseZip(hz);
    system("PAUSE");
    return EXIT_SUCCESS;
}
