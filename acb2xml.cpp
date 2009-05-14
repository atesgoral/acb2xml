#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <stdio.h>
#include <tchar.h>
#include "stdlib.h"
#include "math.h"

#define COLOR_BOOK_HEADER "8BCB"

/* Color space IDs */

#define COLOR_SPACE_RGB       0
#define COLOR_SPACE_HSB       1
#define COLOR_SPACE_CMYK      2
#define COLOR_SPACE_Pantone   3
#define COLOR_SPACE_Focoltone 4
#define COLOR_SPACE_Trumatch  5
#define COLOR_SPACE_Toyo      6
#define COLOR_SPACE_Lab       7
#define COLOR_SPACE_Grayscale 8
#define COLOR_SPACE_HKS       10

#define OUTPUT_XML
//#define OUTPUT_SQL

#ifdef OUTPUT_XML
  #define xmlprint printf
#else
  #define xmlprint noop
#endif

void noop(...)
{
}

typedef struct _XMLSCOPE {
  _XMLSCOPE *pParent;
  const char *tag;
  int indent;
} XS, *PXS;

/* Color space component quantizers */

int Quant_CMYK(unsigned char component)
{
  return floor((1 - component / 255.0) * 100 + 0.5);
}

int Quant_Lab_L(unsigned char component)
{
  return floor(component / 255.0 * 100 + 0.5);
}

int Quant_Lab_ab(unsigned char component)
{
  return component - 128;
}

/* Read utils */

short ReadShort(FILE *pFile)
{
  short i;
  
  if (fread(&i, 1, 2, pFile) != 2)
    throw "Cannot read short integer.";

  return _byteswap_ushort(i);
}

long ReadLong(FILE *pFile)
{
  long i;
  
  if (fread(&i, 1, 4, pFile) != 4)
    throw "Cannot read long integer.";

  return _byteswap_ulong(i);
}

wchar_t *ReadString(FILE *pFile)
{
  wchar_t *s = NULL;

  try
  {
    int length = ReadLong(pFile);

    int bufsize = (length + 1) << 1;
    s = (wchar_t *)malloc(bufsize);

    for (int i = 0; i < length; i++)
      s[i] = ReadShort(pFile);

    s[length] = 0;

    return s;
  }
  catch (...)
  {
    if (s)
      free(s);

    throw;
  }
}

PXS CreateXML(const char *version, const char *encoding)
{
  xmlprint("<?xml version=\"%s\" encoding=\"%s\"?>", version, encoding);
  PXS pXS = new XS;
  pXS->indent = 0;
  pXS->pParent = NULL;
  pXS->tag = NULL;

  return pXS;
}

// DestroyXML

void Indent(PXS pXS) // Quick and dirty. Fix later.
{
  for (int i = 0; i < pXS->indent; i++)
    xmlprint(" ");
}

void OpenTag(PXS *ppXS, const char *tag)
{
  PXS pNewXS = new XS;
  pNewXS->indent = (*ppXS)->indent + 4;
  pNewXS->pParent = *ppXS;
  pNewXS->tag = strdup(tag);

  xmlprint("\n");

  Indent(*ppXS);

  // Hack!!!
  if (strcmp(tag, "color-book") == 0)
  {
	  xmlprint("<color-book version=\"1.0\" xmlns=\"http://magnetiq.com/ns/2007/05/colorbook\">");
  }
  else
  {
	  xmlprint("<%s>", pNewXS->tag);
  }

  *ppXS = pNewXS;
}

void CloseTag(PXS *ppXS, bool bIndent = false)
{
  if (bIndent)
  {
    xmlprint("\n");
	(*ppXS)->indent -= 4; // Hacky
    Indent(*ppXS);
  }

  xmlprint("</%s>", (*ppXS)->tag);

  //free(const_cast<void *>((*ppXS)->tag));

  PXS pParentXS = (*ppXS)->pParent;

  delete *ppXS;

  *ppXS = pParentXS;
}

bool ConvertACB(const char *filename)
{
  bool ret = true;

  FILE *pFile = NULL;

  try
  {
    pFile = fopen(filename, "rb");

    if (!pFile)
      throw "Cannot open file.";
    
    char header[4];

    if (fread(header, 1, 4, pFile) != 4)
      throw "Cannot read header.";

    if (strncmp(header, COLOR_BOOK_HEADER, 4))
      throw "Invalid header. Probably not a valid Photoshop Color Book file.";

    int version = ReadShort(pFile);

    if (version != 1)
      throw "Invalid color book version.";

    PXS pXS = CreateXML("1.0", "UTF-8");
    
    OpenTag(&pXS, "color-book");

    OpenTag(&pXS, "version");
    xmlprint("%d", version);
    CloseTag(&pXS);

    short id = ReadShort(pFile);
    
    OpenTag(&pXS, "id");
    xmlprint("%.4x", id);
    CloseTag(&pXS);

    wchar_t *s;
    
    OpenTag(&pXS, "title");
    xmlprint("<![CDATA[%S]]>", ReadString(pFile));
    CloseTag(&pXS);

    OpenTag(&pXS, "prefix");
    xmlprint("<![CDATA[%S]]>", ReadString(pFile));
    CloseTag(&pXS);

    OpenTag(&pXS, "postfix");
    xmlprint("<![CDATA[%S]]>", ReadString(pFile));
    CloseTag(&pXS);

    OpenTag(&pXS, "description");
    xmlprint("<![CDATA[%S]]>", ReadString(pFile));
    CloseTag(&pXS);

    int colors = ReadShort(pFile);

    OpenTag(&pXS, "colors");
    xmlprint("%d", colors);
    CloseTag(&pXS);

	fprintf(stderr, "Found %d colors\n", colors);

    OpenTag(&pXS, "page-size");
    xmlprint("%d", ReadShort(pFile));
    CloseTag(&pXS);

    OpenTag(&pXS, "page-offset");
    xmlprint("%d", ReadShort(pFile));
    CloseTag(&pXS);

    int colorspace = ReadShort(pFile);
    
    OpenTag(&pXS, "color-space");
    
    switch (colorspace)
    {
      case COLOR_SPACE_RGB:
        xmlprint("RGB");
        break;

      case COLOR_SPACE_HSB:
        xmlprint("HSB");
        break;

      case COLOR_SPACE_CMYK:
        xmlprint("CMYK");
        break;

      case COLOR_SPACE_Lab:
        xmlprint("Lab");
        break;

      case COLOR_SPACE_Grayscale:
        xmlprint("Grayscale");
        break;

      default:
        xmlprint("Unknown");
    }

    CloseTag(&pXS);

    char code[7];
    code[6] = 0;

    //colors = 2;

    for (int i = 0; i < colors; i++)
    {
      if (!(s = ReadString(pFile)))
        throw "Cannot read color name.";

      OpenTag(&pXS, "color");

      OpenTag(&pXS, "name");
      xmlprint("%S", s);
      CloseTag(&pXS);
     
      if (fread(code, 1, 6, pFile) != 6)
        throw "Cannot read code";

      OpenTag(&pXS, "alias");
      xmlprint("<![CDATA[%s]]>", code);
      CloseTag(&pXS);

      unsigned char components[4];
#ifdef OUTPUT_SQL
      int sqlcomps[4];
#endif

      switch (colorspace)
      {
        case COLOR_SPACE_RGB:
          if (fread(components, 1, 3, pFile) != 3)
            throw "Cannot read RGB components.";

          OpenTag(&pXS, "red");
          xmlprint("%d", components[0]);
          CloseTag(&pXS);

          OpenTag(&pXS, "green");
          xmlprint("%d", components[1]);
          CloseTag(&pXS);

          OpenTag(&pXS, "blue");
          xmlprint("%d", components[2]);
          CloseTag(&pXS);

#ifdef OUTPUT_SQL
          sqlcomps[0] = components[0];
          sqlcomps[1] = components[1];
          sqlcomps[2] = components[2];
          sqlcomps[3] = 0;
#endif
          break;

        case COLOR_SPACE_CMYK:
          if (fread(components, 1, 4, pFile) != 4)
            throw "Cannot read CMYK components.";

          OpenTag(&pXS, "cyan");
          xmlprint("%d", Quant_CMYK(components[0]));
          CloseTag(&pXS);

          OpenTag(&pXS, "magenta");
          xmlprint("%d", Quant_CMYK(components[1]));
          CloseTag(&pXS);

          OpenTag(&pXS, "yellow");
          xmlprint("%d", Quant_CMYK(components[2]));
          CloseTag(&pXS);

          OpenTag(&pXS, "black");
          xmlprint("%d", Quant_CMYK(components[3]));
          CloseTag(&pXS);
          
#ifdef OUTPUT_SQL
          sqlcomps[0] = Quant_CMYK(components[0]);
          sqlcomps[1] = Quant_CMYK(components[1]);
          sqlcomps[2] = Quant_CMYK(components[2]);
          sqlcomps[3] = Quant_CMYK(components[3]);
#endif
          break;

        case COLOR_SPACE_Lab:
          if (fread(components, 1, 3, pFile) != 3)
            throw "Cannot read Lab components.";

          OpenTag(&pXS, "lightness");
          xmlprint("%d", Quant_Lab_L(components[0]));
          CloseTag(&pXS);

          OpenTag(&pXS, "a-chrominance");
          xmlprint("%d", Quant_Lab_ab(components[1]));
          CloseTag(&pXS);

          OpenTag(&pXS, "b-chrominance");
          xmlprint("%d", Quant_Lab_ab(components[2]));
          CloseTag(&pXS);
          
#ifdef OUTPUT_SQL
          sqlcomps[0] = Quant_Lab_L(components[0]);
          sqlcomps[1] = Quant_Lab_ab(components[1]);
          sqlcomps[2] = Quant_Lab_ab(components[2]);
          sqlcomps[3] = 0;
#endif
          break;

        default:
          throw "Unsupported color space.";
      }

#ifdef OUTPUT_SQL
      printf("UPDATE Colors SET Component1 = %d, Component2 = %d, Component3 = %d, Component4 = %d WHERE ColorCatalogID = $ccid AND Color = '%S';\n", sqlcomps[0], sqlcomps[1], sqlcomps[2], sqlcomps[3], s);
#endif

      CloseTag(&pXS, true);
    }

    CloseTag(&pXS, true);

	fprintf(stderr, "Done.\n");
  }
  catch (char *errstr)
  {
    fprintf(stderr, "ERROR: %s\n", errstr);
    ret = false;
  }

  if (pFile)
    fclose(pFile);

  return ret;
}

int _tmain(int argc, _TCHAR* argv[])
{
  fprintf(stderr, "ACB2XML v2.0 beta, 2007-05-08\n");
  fprintf(stderr, "by Ates Goral\n");
  fprintf(stderr, "http://magnetiq.com/\n\n");

  if (argc > 1)
  {
    if (!ConvertACB(argv[1]))
      fprintf(stderr, "Conversion error.\n");
  }
  else
  {
    fprintf(stderr, "Usage:\n  acb2xml <acb filename>\n\n");
  }

  return 0;
}
