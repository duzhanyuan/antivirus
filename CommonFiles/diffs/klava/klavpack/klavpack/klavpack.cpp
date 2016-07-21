#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#include <vector>
#include "../../klavpacklib/klavpack_encode.h"
#include "../../klavpacklib/klavpack_decode.h"

/* external data from asmldr.asm */
extern "C" const unsigned char* asm_loader_begin;
extern "C" const unsigned char* asm_loader_end;

/* ------------------------------------------------------------------------- */

struct Coder
{
	virtual bool is_packed (const unsigned char *data, size_t size) = 0;
	virtual bool pack (const unsigned char *data, size_t size, Diff_Buffer *buf) = 0;
	virtual bool unpack (const unsigned char *data, size_t size, Diff_Buffer *buf) = 0;
};

struct DLL_Coder : public Coder
{
	virtual ~DLL_Coder () {}

	virtual bool is_packed (const unsigned char *data, size_t size)
		{ return Diff_DLL_IsPacked (data, size); }
	virtual bool pack (const unsigned char *data, size_t size, Diff_Buffer *buf)
		{ return Diff_DLL_Pack (data, size, buf); }
	virtual bool unpack (const unsigned char *data, size_t size, Diff_Buffer *buf)
		{ return Diff_DLL_Unpack (data, size, buf); }
};

struct KFB_Coder : public Coder
{
	virtual ~KFB_Coder () {}

	virtual bool is_packed (const unsigned char *data, size_t size)
		{ return Diff_KFB_IsPacked (data, size); }
	virtual bool pack (const unsigned char *data, size_t size, Diff_Buffer *buf)
		{ return Diff_KFB_Pack (data, size, buf); }
	virtual bool unpack (const unsigned char *data, size_t size, Diff_Buffer *buf)
		{ return Diff_KFB_Unpack (data, size, buf); }
};

static int Encode(char* SrcName, char* DstName, Coder *coder)
{
  std::vector<unsigned char> Source;
  Diff_Buffer Destination;
  unsigned char* SrcBuff;
  const unsigned char* DstBuff;
  size_t SrcSize;
  size_t DstSize;
  FILE*  f;

  if ( NULL == (f = fopen(SrcName, "rb")) )
  {
    printf("Can't open %s\n", SrcName);
    return(0);
  }

  fseek(f, 0, SEEK_END);
  SrcSize = ftell(f);
  fseek(f, 0, SEEK_SET);

  Source.resize(SrcSize);
  SrcBuff = &Source[0];

  if ( SrcSize != fread(SrcBuff, 1, SrcSize, f) )
  {
    printf("Can't read 0x%X bytes from source stream\n", SrcSize);
    fclose(f);
    return(0);
  }

  fclose(f);

  if ( ! coder->pack (SrcBuff, SrcSize, & Destination) )
  {
    printf("Internal compression error\n");
    return(0);
  }

  DstBuff = Destination.m_data;
  DstSize = Destination.m_size;

  if ( NULL == (f = fopen(DstName, "wb")) )
  {
    printf("Can't create %s\n", DstName);
    return(0);
  }

  if ( DstSize != fwrite(DstBuff, 1, DstSize, f) )
  {
    printf("Can't write 0x%X bytes to destination stream\n", DstSize);
    fclose(f);
    return(0);
  }

  fclose(f);
  return(1);
}

/* ------------------------------------------------------------------------- */

static int Decode(char* SrcName, char* DstName, Coder *coder)
{
  std::vector<unsigned char> Source;
  Diff_Buffer Destination;
  unsigned char* SrcBuff;
  const unsigned char* DstBuff;
  size_t SrcSize;
  size_t DstSize;
  FILE*  f;

  if ( NULL == (f = fopen(SrcName, "rb")) )
  {
    printf("Can't open %s\n", SrcName);
    return(0);
  }

  fseek(f, 0, SEEK_END);
  SrcSize = ftell(f);
  fseek(f, 0, SEEK_SET);

  Source.resize(SrcSize);
  SrcBuff = &Source[0];

  if ( SrcSize != fread(SrcBuff, 1, SrcSize, f) )
  {
    printf("Can't read 0x%X bytes from source stream\n", SrcSize);
    fclose(f);
    return(0);
  }

  fclose(f);

  /* check for compression */
  if ( ! coder->is_packed (SrcBuff, SrcSize) )
  {
    printf("Not packed\n");
    return(0);
  }

  /* decompress image */
  if ( ! coder->unpack (SrcBuff, SrcSize, & Destination) )
  {
    printf("Internal decompression error\n");
    return(0);
  }

  DstBuff = Destination.m_data;
  DstSize = Destination.m_size;

  if ( NULL == (f = fopen(DstName, "wb")) )
  {
    printf("Can't create %s\n", DstName);
    return(0);
  }

  if ( DstSize != fwrite(DstBuff, 1, DstSize, f) )
  {
    printf("Can't write 0x%X bytes to destination stream\n", DstSize);
    fclose(f);
    return(0);
  }

  fclose(f);
  return(1);
}

/* ------------------------------------------------------------------------- */

static int GenLoader(char* FileName)
{
  const unsigned char* Loader;
  unsigned int   LoaderSize;
  signed int     Counter;
  FILE* f;

  if ( NULL == (f = fopen(FileName, "wt")) )
  {
    printf("Can't create %s\n", FileName);
    return(0);
  }

  fprintf(f, "/* autogenerated loader body */\n");
  fprintf(f, "static const unsigned char asm_loader_binary[] = {\n");

  Loader = asm_loader_begin;
  LoaderSize = (unsigned int)(asm_loader_end - asm_loader_begin);
  Counter = 10;

  while ( LoaderSize-- )
  {
    if ( Counter >= 10 )
    {
      Counter = 0;
      fprintf(f, "  ");
    }

    if ( LoaderSize )
      fprintf(f, "0x%2.2X, ", *Loader++);
    else
    {
      fprintf(f, "0x%2.2X\n", *Loader++);
      break;
    }

    if ( ++Counter >= 10 )
      fprintf(f, "\n");
  }

  fprintf(f, "};\n");
  fclose(f);
  return(1);
}

/* ------------------------------------------------------------------------- */

int main(int argc, char* argv[])
{
  int Result = 0;

  /* check for hidden option "g" */
  if ( argc == 3 && (argv[1][0]|0x20) == 'g' )
  {
    /* gen loader binary */
    if ( !GenLoader(argv[2]) )
    {
      printf("GenLoader() FAIL\n");
      return(-4);
    }

    /* done */
    return(0);
  }

  if ( argc != 4 )
  {
    printf("Usage: klavpack.exe <e|ek|d|dk> <source file> <destination file>\n");
    printf("  (use e for encoding DLLs, ek for encoding KFBs, same for decode)\n");
    return(-1);
  }

  DLL_Coder dll_coder;
  KFB_Coder kfb_coder;

  Coder * coder = & dll_coder;

  if ( (argv[1][1]|0x20) == 'k')
	coder = & kfb_coder;

  if ( (argv[1][0]|0x20) == 'e' )
  {
    /* encode mode */
    printf("Encoding %s -> %s\n", argv[2], argv[3]);
    Result = Encode(argv[2], argv[3], coder);
  }
  else if ( (argv[1][0]|0x20) == 'd' )
  {
    /* decode mode */
    printf("Decoding %s -> %s\n", argv[2], argv[3]);
    Result = Decode(argv[2], argv[3], coder);
  }
  else
  {
    printf("Unknown option: %s\n", argv[1]);
    return(-2);
  }

  if ( !Result )
  {
    printf("Some errors detected. Aborting...\n");
    return(-3);
  }

  return(0);
}

/* ------------------------------------------------------------------------- */