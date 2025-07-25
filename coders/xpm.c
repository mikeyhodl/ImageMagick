/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%                            X   X  PPPP   M   M                              %
%                             X X   P   P  MM MM                              %
%                              X    PPPP   M M M                              %
%                             X X   P      M   M                              %
%                            X   X  P      M   M                              %
%                                                                             %
%                                                                             %
%                  Read/Write X Windows system Pixmap Format                  %
%                                                                             %
%                              Software Design                                %
%                                   Cristy                                    %
%                                 July 1992                                   %
%                                                                             %
%                                                                             %
%  Copyright @ 1999 ImageMagick Studio LLC, a non-profit organization         %
%  dedicated to making software imaging solutions freely available.           %
%                                                                             %
%  You may not use this file except in compliance with the License.  You may  %
%  obtain a copy of the License at                                            %
%                                                                             %
%    https://imagemagick.org/script/license.php                               %
%                                                                             %
%  Unless required by applicable law or agreed to in writing, software        %
%  distributed under the License is distributed on an "AS IS" BASIS,          %
%  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   %
%  See the License for the specific language governing permissions and        %
%  limitations under the License.                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%
*/

/*
  Include declarations.
*/
#include "MagickCore/studio.h"
#include "MagickCore/attribute.h"
#include "MagickCore/blob.h"
#include "MagickCore/blob-private.h"
#include "MagickCore/cache.h"
#include "MagickCore/coder-private.h"
#include "MagickCore/color.h"
#include "MagickCore/color-private.h"
#include "MagickCore/colormap.h"
#include "MagickCore/colorspace.h"
#include "MagickCore/colorspace-private.h"
#include "MagickCore/exception.h"
#include "MagickCore/exception-private.h"
#include "MagickCore/geometry.h"
#include "MagickCore/image.h"
#include "MagickCore/image-private.h"
#include "MagickCore/list.h"
#include "MagickCore/magick.h"
#include "MagickCore/memory_.h"
#include "MagickCore/module.h"
#include "MagickCore/monitor.h"
#include "MagickCore/monitor-private.h"
#include "MagickCore/pixel-accessor.h"
#include "MagickCore/quantize.h"
#include "MagickCore/quantum-private.h"
#include "MagickCore/resize.h"
#include "MagickCore/resource_.h"
#include "MagickCore/splay-tree.h"
#include "MagickCore/static.h"
#include "MagickCore/string_.h"
#include "MagickCore/string-private.h"
#include "MagickCore/threshold.h"
#include "MagickCore/token.h"
#include "MagickCore/utility.h"
#include "coders/coders-private.h"

/*
  Global declarations.
*/
static SplayTreeInfo
  *xpm_symbolic = (SplayTreeInfo *) NULL;

/*
  Forward declarations.
*/
static MagickBooleanType
  WritePICONImage(const ImageInfo *,Image *,ExceptionInfo *),
  WriteXPMImage(const ImageInfo *,Image *,ExceptionInfo *);

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   I s X P M                                                                 %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  IsXPM() returns MagickTrue if the image format type, identified by the
%  magick string, is XPM.
%
%  The format of the IsXPM method is:
%
%      MagickBooleanType IsXPM(const unsigned char *magick,const size_t length)
%
%  A description of each parameter follows:
%
%    o magick: compare image format pattern against these bytes. or
%      blob.
%
%    o length: Specifies the length of the magick string.
%
*/
static MagickBooleanType IsXPM(const unsigned char *magick,const size_t length)
{
  if (length < 9)
    return(MagickFalse);
  if (LocaleNCompare((char *) magick+1,"* XPM *",7) == 0)
    return(MagickTrue);
  return(MagickFalse);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   R e a d X P M I m a g e                                                   %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  ReadXPMImage() reads an X11 pixmap image file and returns it.  It
%  allocates the memory necessary for the new Image structure and returns a
%  pointer to the new image.
%
%  The format of the ReadXPMImage method is:
%
%      Image *ReadXPMImage(const ImageInfo *image_info,ExceptionInfo *exception)
%
%  A description of each parameter follows:
%
%    o image_info: the image info.
%
%    o exception: return any errors or warnings in this structure.
%
*/

static int CompareXPMColor(const void *target,const void *source)
{
  const char
    *p,
    *q;

  p=(const char *) target;
  q=(const char *) source;
  return(strcmp(p,q));
}

static ssize_t CopyXPMColor(char *destination,const char *source,size_t length)
{
  const char
    *p;

  p=source;
  while (length-- && (*p != '\0'))
  {
    if (*p == '"')
      break;
    *destination++=(*p++);
  }
  if (length != 0)
    *destination='\0';
  return((ssize_t) (p-source));
}

static char *NextXPMLine(char *p)
{
  assert(p != (char *) NULL);
  p=strchr(p,'\n');
  if (p != (char *) NULL)
    p++;
  return(p);
}

static char *ParseXPMColor(char *,const MagickBooleanType)
  magick_attribute((__pure__));

static char *ParseXPMColor(char *color,const MagickBooleanType search_start)
{
#define NumberTargets  6

  char
    *p,
    *r;

  const char
    *q;

  ssize_t
    i;

  static const char
    *const targets[NumberTargets] = { "c ", "g ", "g4 ", "m ", "b ", "s " };

  if (*color == '\0')
    return((char *) NULL);
  if (search_start != MagickFalse)
    {
      for (i=0; i < NumberTargets; i++)
      {
        p=color;
        for (q=targets[i]; *p != '\0'; p++)
        {
          if (*p == '\n')
            break;
          if (*p != *q)
            continue;
          if (isspace((int) ((unsigned char) (*(p-1)))) == 0)
            continue;
          r=p;
          for ( ; ; )
          {
            if (*q == '\0')
              return(p);
            if (*r++ != *q++)
              break;
          }
          q=targets[i];
        }
      }
      return((char *) NULL);
    }
  for (p=color+1; *p != '\0'; p++)
  {
    if (*p == '\n')
      break;
    if (isspace((int) ((unsigned char) (*(p-1)))) == 0)
      continue;
    if (isspace((int) ((unsigned char) (*p))) != 0)
      continue;
    for (i=0; i < NumberTargets; i++)
    {
      if ((*p == *targets[i]) && (*(p+1) == *(targets[i]+1)))
        return(p);
    }
  }
  return(p);
}

static Image *ReadXPMImage(const ImageInfo *image_info,ExceptionInfo *exception)
{
  char
    *grey,
    key[MagickPathExtent],
    *next,
    *p,
    *q,
    target[MagickPathExtent],
    *xpm_buffer;

  Image
    *image;

  MagickBooleanType
    active,
    status;

  Quantum
    *r;

  ssize_t
    x;

  size_t
    length;

  SplayTreeInfo
    *xpm_colors;

  ssize_t
    count,
    j,
    y;

  unsigned long
    colors,
    columns,
    rows,
    width;

  /*
    Open image file.
  */
  assert(image_info != (const ImageInfo *) NULL);
  assert(image_info->signature == MagickCoreSignature);
  assert(exception != (ExceptionInfo *) NULL);
  assert(exception->signature == MagickCoreSignature);
  if (IsEventLogging() != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",
      image_info->filename);
  image=AcquireImage(image_info,exception);
  status=OpenBlob(image_info,image,ReadBinaryBlobMode,exception);
  if (status == MagickFalse)
    {
      image=DestroyImageList(image);
      return((Image *) NULL);
    }
  /*
    Read XPM file.
  */
  length=MagickPathExtent;
  xpm_buffer=(char *) AcquireQuantumMemory((size_t) length,sizeof(*xpm_buffer));
  if (xpm_buffer == (char *) NULL)
    ThrowReaderException(ResourceLimitError,"MemoryAllocationFailed");
  *xpm_buffer='\0';
  p=xpm_buffer;
  while (ReadBlobString(image,p) != (char *) NULL)
  {
    ssize_t
      offset;

    if ((*p == '#') && ((p == xpm_buffer) || (*(p-1) == '\n')))
      continue;
    if ((*p == '}') && (*(p+1) == ';'))
      break;
    p+=(ptrdiff_t) strlen(p);
    offset=p-xpm_buffer;
    if ((size_t) (offset+MagickPathExtent) < length)
      continue;
    length<<=1;
    xpm_buffer=(char *) ResizeQuantumMemory(xpm_buffer,length+MagickPathExtent,
      sizeof(*xpm_buffer));
    if (xpm_buffer == (char *) NULL)
      break;
    p=xpm_buffer+offset;
  }
  if (xpm_buffer == (char *) NULL)
    ThrowReaderException(ResourceLimitError,"MemoryAllocationFailed");
  /*
    Remove comments & read properties.
  */
  count=0;
  width=0;
  for (p=xpm_buffer; *p != '\0'; p++)
  {
    char
      properties[MagickPathExtent];

    if (*p != '"')
      continue;
    (void) GetNextToken(p,(const char **) NULL,MagickPathExtent,properties);
    count=(ssize_t) MagickSscanf(properties,"%lu %lu %lu %lu",&columns,&rows,
      &colors,&width);
    image->columns=columns;
    image->rows=rows;
    image->colors=colors;
    if (count == 4)
      break;
  }
  if ((count != 4) || (width == 0) || (width > 3) ||
      (image->columns == 0) || (image->rows == 0) ||
      (image->colors == 0) || (image->colors > MaxColormapSize))
    {
      xpm_buffer=DestroyString(xpm_buffer);
      ThrowReaderException(CorruptImageError,"ImproperImageHeader");
    }
  /*
    Remove unquoted characters.
  */
  active=MagickFalse;
  for (q=xpm_buffer; *p != '\0'; )
  {
    if (*p++ == '"')
      {
        if (active != MagickFalse)
          *q++='\n';
        active=active != MagickFalse ? MagickFalse : MagickTrue;
      }
    if (active != MagickFalse)
      *q++=(*p);
  }
  *q='\0';
  if (active != MagickFalse)
    {
      xpm_buffer=DestroyString(xpm_buffer);
      ThrowReaderException(CorruptImageError,"UnexpectedEndOfFile");
    }
  /*
    Initialize image structure.
  */
  xpm_colors=NewSplayTree(CompareXPMColor,RelinquishMagickMemory,
    (void *(*)(void *)) NULL);
  if (AcquireImageColormap(image,image->colors,exception) == MagickFalse)
    {
      xpm_colors=DestroySplayTree(xpm_colors);
      xpm_buffer=DestroyString(xpm_buffer);
      ThrowReaderException(ResourceLimitError,"MemoryAllocationFailed");
    }
  /*
    Read image colormap.
  */
  image->depth=1;
  next=NextXPMLine(xpm_buffer);
  for (j=0; (j < (ssize_t) image->colors) && (next != (char *) NULL); j++)
  {
    char
      symbolic[MagickPathExtent];

    MagickAddressType
      value = (MagickAddressType) j;

    p=next;
    next=NextXPMLine(p);
    if (next == (char *) NULL)
      break;
    length=MagickMin((size_t) width,MagickPathExtent-1);
    if (CopyXPMColor(key,p,length) != (ssize_t) length)
      break;
    status=AddValueToSplayTree(xpm_colors,ConstantString(key),(const void *)
      value);
    /*
      Parse color.
    */
    (void) memset(target,0,sizeof(target));
    (void) CopyMagickString(target,"gray",MagickPathExtent);
    q=(char *) NULL;
    if (strlen(p) > width)
      q=ParseXPMColor(p+width,MagickTrue);
    (void) memset(symbolic,0,sizeof(symbolic));
    *symbolic='\0';
    if (q != (char *) NULL)
      {
        while ((isspace((int) ((unsigned char) *q)) == 0) && (*q != '\0'))
          q++;
        if ((next-q) < 0)
          break;
        (void) CopyXPMColor(target,q,MagickMin((size_t) (next-q),
          MagickPathExtent-1));
        q=ParseXPMColor(target,MagickFalse);
        (void) CopyXPMColor(symbolic,q,MagickMin((size_t) (next-q),
          MagickPathExtent-1));
        if (q != (char *) NULL)
          *q='\0';
      }
    (void) StripMagickString(target);
    if (*symbolic != '\0')
      (void) AddValueToSplayTree(xpm_symbolic,ConstantString(target),
        ConstantString(symbolic));
    grey=strstr(target,"grey");
    if (grey != (char *) NULL)
      grey[2]='a';
    if (LocaleCompare(target,"none") == 0)
      {
        image->storage_class=DirectClass;
        image->alpha_trait=BlendPixelTrait;
      }
    status=QueryColorCompliance(target,XPMCompliance,&image->colormap[j],
      exception);
    if (status == MagickFalse)
      break;
    if (image->depth < image->colormap[j].depth)
      image->depth=image->colormap[j].depth;
  }
  if (j < (ssize_t) image->colors)
    {
      xpm_colors=DestroySplayTree(xpm_colors);
      xpm_buffer=DestroyString(xpm_buffer);
      ThrowReaderException(CorruptImageError,"CorruptImage");
    }
  j=0;
  if (image_info->ping == MagickFalse)
    {
      /*
        Read image pixels.
      */
      status=SetImageExtent(image,image->columns,image->rows,exception);
      if (status == MagickFalse)
        {
          xpm_colors=DestroySplayTree(xpm_colors);
          xpm_buffer=DestroyString(xpm_buffer);
          return(DestroyImageList(image));
        }
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        p=NextXPMLine(p);
        if (p == (char *) NULL)
          break;
        r=QueueAuthenticPixels(image,0,y,image->columns,1,exception);
        if (r == (Quantum *) NULL)
          break;
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          count=CopyXPMColor(key,p,MagickMin(width,MagickPathExtent-1));
          if (count != (ssize_t) width)
            break;
          j=(ssize_t) GetValueFromSplayTree(xpm_colors,key);
          if (image->storage_class == PseudoClass)
            SetPixelIndex(image,(Quantum) j,r);
          SetPixelViaPixelInfo(image,image->colormap+j,r);
          p+=(ptrdiff_t) count;
          r+=(ptrdiff_t) GetPixelChannels(image);
        }
        if (x < (ssize_t) image->columns)
          break;
        if (SyncAuthenticPixels(image,exception) == MagickFalse)
          break;
      }
      if (y < (ssize_t) image->rows)
        {
          xpm_colors=DestroySplayTree(xpm_colors);
          xpm_buffer=DestroyString(xpm_buffer);
          ThrowReaderException(CorruptImageError,"NotEnoughPixelData");
        }
    }
  /*
    Relinquish resources.
  */
  xpm_buffer=DestroyString(xpm_buffer);
  xpm_colors=DestroySplayTree(xpm_colors);
  if (CloseBlob(image) == MagickFalse)
    status=MagickFalse;
  if (status == MagickFalse)
    return(DestroyImageList(image));
  return(GetFirstImageInList(image));
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   R e g i s t e r X P M I m a g e                                           %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  RegisterXPMImage() adds attributes for the XPM image format to
%  the list of supported formats.  The attributes include the image format
%  tag, a method to read and/or write the format, whether the format
%  supports the saving of more than one frame to the same file or blob,
%  whether the format supports native in-memory I/O, and a brief
%  description of the format.
%
%  The format of the RegisterXPMImage method is:
%
%      size_t RegisterXPMImage(void)
%
*/
ModuleExport size_t RegisterXPMImage(void)
{
  MagickInfo
    *entry;

  if (xpm_symbolic == (SplayTreeInfo *) NULL)
    xpm_symbolic=NewSplayTree(CompareSplayTreeString,RelinquishMagickMemory,
      RelinquishMagickMemory);
  entry=AcquireMagickInfo("XPM","PICON","Personal Icon");
  entry->decoder=(DecodeImageHandler *) ReadXPMImage;
  entry->encoder=(EncodeImageHandler *) WritePICONImage;
  entry->flags^=CoderAdjoinFlag;
  (void) RegisterMagickInfo(entry);
  entry=AcquireMagickInfo("XPM","PM","X Windows system pixmap (color)");
  entry->decoder=(DecodeImageHandler *) ReadXPMImage;
  entry->encoder=(EncodeImageHandler *) WriteXPMImage;
  entry->flags^=CoderAdjoinFlag;
  entry->flags|=CoderStealthFlag;
  (void) RegisterMagickInfo(entry);
  entry=AcquireMagickInfo("XPM","XPM","X Windows system pixmap (color)");
  entry->decoder=(DecodeImageHandler *) ReadXPMImage;
  entry->encoder=(EncodeImageHandler *) WriteXPMImage;
  entry->magick=(IsImageFormatHandler *) IsXPM;
  entry->flags^=CoderAdjoinFlag;
  (void) RegisterMagickInfo(entry);
  return(MagickImageCoderSignature);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   U n r e g i s t e r X P M I m a g e                                       %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  UnregisterXPMImage() removes format registrations made by the
%  XPM module from the list of supported formats.
%
%  The format of the UnregisterXPMImage method is:
%
%      UnregisterXPMImage(void)
%
*/
ModuleExport void UnregisterXPMImage(void)
{
  (void) UnregisterMagickInfo("PICON");
  (void) UnregisterMagickInfo("PM");
  (void) UnregisterMagickInfo("XPM");
  if (xpm_symbolic != (SplayTreeInfo *) NULL)
    xpm_symbolic=DestroySplayTree(xpm_symbolic);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   W r i t e P I C O N I m a g e                                             %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  WritePICONImage() writes an image to a file in the Personal Icon format.
%
%  The format of the WritePICONImage method is:
%
%      MagickBooleanType WritePICONImage(const ImageInfo *image_info,
%        Image *image,ExceptionInfo *exception)
%
%  A description of each parameter follows.
%
%    o image_info: the image info.
%
%    o image:  The image.
%
%    o exception: return any errors or warnings in this structure.
%
*/
static MagickBooleanType WritePICONImage(const ImageInfo *image_info,
  Image *image,ExceptionInfo *exception)
{
#define ColormapExtent  155
#define GraymapExtent  95
#define PiconGeometry  "48x48>"

  static unsigned char
    Colormap[]=
    {
      0x47, 0x49, 0x46, 0x38, 0x37, 0x61, 0x06, 0x00, 0x05, 0x00, 0xf4, 0x05,
      0x00, 0x00, 0x00, 0x00, 0x2f, 0x4f, 0x4f, 0x70, 0x80, 0x90, 0x7e, 0x7e,
      0x7e, 0xdc, 0xdc, 0xdc, 0xff, 0xff, 0xff, 0x00, 0x00, 0x80, 0x00, 0x00,
      0xff, 0x1e, 0x90, 0xff, 0x87, 0xce, 0xeb, 0xe6, 0xe6, 0xfa, 0x00, 0xff,
      0xff, 0x80, 0x00, 0x80, 0xb2, 0x22, 0x22, 0x2e, 0x8b, 0x57, 0x32, 0xcd,
      0x32, 0x00, 0xff, 0x00, 0x98, 0xfb, 0x98, 0xff, 0x00, 0xff, 0xff, 0x00,
      0x00, 0xff, 0x63, 0x47, 0xff, 0xa5, 0x00, 0xff, 0xd7, 0x00, 0xff, 0xff,
      0x00, 0xee, 0x82, 0xee, 0xa0, 0x52, 0x2d, 0xcd, 0x85, 0x3f, 0xd2, 0xb4,
      0x8c, 0xf5, 0xde, 0xb3, 0xff, 0xfa, 0xcd, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x21, 0xf9, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00,
      0x00, 0x00, 0x06, 0x00, 0x05, 0x00, 0x00, 0x05, 0x18, 0x20, 0x10, 0x08,
      0x03, 0x51, 0x18, 0x07, 0x92, 0x28, 0x0b, 0xd3, 0x38, 0x0f, 0x14, 0x49,
      0x13, 0x55, 0x59, 0x17, 0x96, 0x69, 0x1b, 0xd7, 0x85, 0x00, 0x3b,
    },
    Graymap[]=
    {
      0x47, 0x49, 0x46, 0x38, 0x37, 0x61, 0x04, 0x00, 0x04, 0x00, 0xf3, 0x0f,
      0x00, 0x00, 0x00, 0x00, 0x12, 0x12, 0x12, 0x21, 0x21, 0x21, 0x33, 0x33,
      0x33, 0x45, 0x45, 0x45, 0x54, 0x54, 0x54, 0x66, 0x66, 0x66, 0x78, 0x78,
      0x78, 0x87, 0x87, 0x87, 0x99, 0x99, 0x99, 0xab, 0xab, 0xab, 0xba, 0xba,
      0xba, 0xcc, 0xcc, 0xcc, 0xde, 0xde, 0xde, 0xed, 0xed, 0xed, 0xff, 0xff,
      0xff, 0x21, 0xf9, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00,
      0x00, 0x00, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04, 0x0c, 0x10, 0x04, 0x31,
      0x48, 0x31, 0x07, 0x25, 0xb5, 0x58, 0x73, 0x4f, 0x04, 0x00, 0x3b,
    };

#define MaxCixels  92

  static const char
    Cixel[MaxCixels+1] = " .XoO+@#$%&*=-;:>,<1234567890qwertyuipasdfghjk"
                         "lzxcvbnmMNBVCZASDFGHJKLPIUYTREWQ!~^/()_`'][{}|";

  char
    buffer[MagickPathExtent],
    basename[MagickPathExtent],
    name[MagickPathExtent],
    symbol[MagickPathExtent];

  Image
    *affinity_image,
    *picon;

  ImageInfo
    *blob_info;

  MagickBooleanType
    status,
    transparent;

  PixelInfo
    pixel;

  QuantizeInfo
    *quantize_info;

  RectangleInfo
    geometry;

  const Quantum
    *p;

  ssize_t
    i,
    x;

  Quantum
    *q;

  size_t
    characters_per_pixel,
    colors;

  ssize_t
    j,
    k,
    y;

  /*
    Open output image file.
  */
  assert(image_info != (const ImageInfo *) NULL);
  assert(image_info->signature == MagickCoreSignature);
  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  assert(exception != (ExceptionInfo *) NULL);
  assert(exception->signature == MagickCoreSignature);
  if (IsEventLogging() != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  status=OpenBlob(image_info,image,WriteBinaryBlobMode,exception);
  if (status == MagickFalse)
    return(status);
  if (IssRGBCompatibleColorspace(image->colorspace) == MagickFalse)
    (void) TransformImageColorspace(image,sRGBColorspace,exception);
  SetGeometry(image,&geometry);
  (void) ParseMetaGeometry(PiconGeometry,&geometry.x,&geometry.y,
    &geometry.width,&geometry.height);
  picon=ResizeImage(image,geometry.width,geometry.height,TriangleFilter,
    exception);
  blob_info=CloneImageInfo(image_info);
  *blob_info->magick='\0';
  (void) AcquireUniqueFilename(blob_info->filename);
  if ((image_info->type != TrueColorType) &&
      (IdentifyImageCoderGray(image,exception) != MagickFalse))
    affinity_image=BlobToImage(blob_info,Graymap,GraymapExtent,exception);
  else
    affinity_image=BlobToImage(blob_info,Colormap,ColormapExtent,exception);
  (void) RelinquishUniqueFileResource(blob_info->filename);
  blob_info=DestroyImageInfo(blob_info);
  if ((picon == (Image *) NULL) || (affinity_image == (Image *) NULL))
    {
      if (affinity_image != (Image *) NULL)
        affinity_image=DestroyImage(affinity_image);
      if (picon != (Image *) NULL)
        picon=DestroyImage(picon);
      return(MagickFalse);
    }
  quantize_info=AcquireQuantizeInfo(image_info);
  status=RemapImage(quantize_info,picon,affinity_image,exception);
  quantize_info=DestroyQuantizeInfo(quantize_info);
  affinity_image=DestroyImage(affinity_image);
  transparent=MagickFalse;
  if (picon->storage_class == PseudoClass)
    {
      (void) CompressImageColormap(picon,exception);
      if (picon->alpha_trait != UndefinedPixelTrait)
        transparent=MagickTrue;
    }
  else
    {
      /*
        Convert DirectClass to PseudoClass picon.
      */
      if (picon->alpha_trait != UndefinedPixelTrait)
        {
          /*
            Map all the transparent pixels.
          */
          for (y=0; y < (ssize_t) picon->rows; y++)
          {
            q=GetAuthenticPixels(picon,0,y,picon->columns,1,exception);
            if (q == (Quantum *) NULL)
              break;
            for (x=0; x < (ssize_t) picon->columns; x++)
            {
              if (GetPixelAlpha(picon,q) == (Quantum) TransparentAlpha)
                transparent=MagickTrue;
              else
                SetPixelAlpha(picon,OpaqueAlpha,q);
              q+=(ptrdiff_t) GetPixelChannels(picon);
            }
            if (SyncAuthenticPixels(picon,exception) == MagickFalse)
              break;
          }
        }
      (void) SetImageType(picon,PaletteType,exception);
    }
  colors=picon->colors;
  if (transparent != MagickFalse)
    {
      colors++;
      picon->colormap=(PixelInfo *) ResizeQuantumMemory((void **)
        picon->colormap,(size_t) colors,sizeof(*picon->colormap));
      if (picon->colormap == (PixelInfo *) NULL)
        ThrowWriterException(ResourceLimitError,"MemoryAllocationError");
      picon->colormap[colors-1].red=0.0;
      picon->colormap[colors-1].green=0.0;
      picon->colormap[colors-1].blue=0.0;
      picon->colormap[colors-1].alpha=TransparentAlpha;
      for (y=0; y < (ssize_t) picon->rows; y++)
      {
        q=GetAuthenticPixels(picon,0,y,picon->columns,1,exception);
        if (q == (Quantum *) NULL)
          break;
        for (x=0; x < (ssize_t) picon->columns; x++)
        {
          if (GetPixelAlpha(picon,q) == (Quantum) TransparentAlpha)
            SetPixelIndex(picon,(Quantum) picon->colors,q);
          q+=(ptrdiff_t) GetPixelChannels(picon);
        }
        if (SyncAuthenticPixels(picon,exception) == MagickFalse)
          break;
      }
    }
  /*
    Compute the character per pixel.
  */
  characters_per_pixel=1;
  for (k=MaxCixels; (ssize_t) colors > k; k*=MaxCixels)
    characters_per_pixel++;
  /*
    XPM header.
  */
  (void) WriteBlobString(image,"/* XPM */\n");
  GetPathComponent(picon->filename,BasePath,basename);
  (void) FormatLocaleString(buffer,MagickPathExtent,
    "static const char *%.1024s[] = {\n",basename);
  (void) WriteBlobString(image,buffer);
  (void) WriteBlobString(image,"/* columns rows colors chars-per-pixel */\n");
  (void) FormatLocaleString(buffer,MagickPathExtent,
    "\"%.20g %.20g %.20g %.20g\",\n",(double) picon->columns,(double)
    picon->rows,(double) colors,(double) characters_per_pixel);
  (void) WriteBlobString(image,buffer);
  GetPixelInfo(image,&pixel);
  for (i=0; i < (ssize_t) colors; i++)
  {
    /*
      Define XPM color.
    */
    pixel=picon->colormap[i];
    pixel.colorspace=sRGBColorspace;
    pixel.depth=8;
    pixel.alpha=(double) OpaqueAlpha;
    (void) QueryColorname(image,&pixel,XPMCompliance,name,exception);
    if (transparent != MagickFalse)
      {
        if (i == (ssize_t) (colors-1))
          (void) CopyMagickString(name,"grey75",MagickPathExtent);
      }
    /*
      Write XPM color.
    */
    k=i % MaxCixels;
    symbol[0]=Cixel[k];
    for (j=1; j < (ssize_t) characters_per_pixel; j++)
    {
      k=((i-k)/MaxCixels) % MaxCixels;
      symbol[j]=Cixel[k];
    }
    symbol[j]='\0';
    (void) FormatLocaleString(buffer,MagickPathExtent,
      "\"%.1024s c %.1024s\",\n",symbol,name);
    (void) WriteBlobString(image,buffer);
  }
  /*
    Define XPM pixels.
  */
  (void) WriteBlobString(image,"/* pixels */\n");
  for (y=0; y < (ssize_t) picon->rows; y++)
  {
    p=GetVirtualPixels(picon,0,y,picon->columns,1,exception);
    if (p == (const Quantum *) NULL)
      break;
    (void) WriteBlobString(image,"\"");
    for (x=0; x < (ssize_t) picon->columns; x++)
    {
      k=((ssize_t) GetPixelIndex(picon,p) % MaxCixels);
      symbol[0]=Cixel[k];
      for (j=1; j < (ssize_t) characters_per_pixel; j++)
      {
        k=(((int) GetPixelIndex(picon,p)-k)/MaxCixels) % MaxCixels;
        symbol[j]=Cixel[k];
      }
      symbol[j]='\0';
      (void) CopyMagickString(buffer,symbol,MagickPathExtent);
      (void) WriteBlobString(image,buffer);
      p+=(ptrdiff_t) GetPixelChannels(picon);
    }
    (void) FormatLocaleString(buffer,MagickPathExtent,"\"%.1024s\n",
      y == (ssize_t) (picon->rows-1) ? "" : ",");
    (void) WriteBlobString(image,buffer);
    status=SetImageProgress(image,SaveImageTag,(MagickOffsetType) y,
      picon->rows);
    if (status == MagickFalse)
      break;
  }
  picon=DestroyImage(picon);
  (void) WriteBlobString(image,"};\n");
  if (CloseBlob(image) == MagickFalse)
    status=MagickFalse;
  return(status);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   W r i t e X P M I m a g e                                                 %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  WriteXPMImage() writes an image to a file in the X pixmap format.
%
%  The format of the WriteXPMImage method is:
%
%      MagickBooleanType WriteXPMImage(const ImageInfo *image_info,
%        Image *image,ExceptionInfo *exception)
%
%  A description of each parameter follows.
%
%    o image_info: the image info.
%
%    o image:  The image.
%
%    o exception: return any errors or warnings in this structure.
%
*/
static MagickBooleanType WriteXPMImage(const ImageInfo *image_info,Image *image,
  ExceptionInfo *exception)
{
#define MaxCixels  92

  char
    buffer[MagickPathExtent],
    basename[MagickPathExtent],
    name[MagickPathExtent],
    symbol[MagickPathExtent];

  const Quantum
    *p;

  MagickBooleanType
    status;

  PixelInfo
    pixel;

  size_t
    characters_per_pixel;

  ssize_t
    i,
    j,
    k,
    opacity,
    x,
    y;

  static const char
    Cixel[MaxCixels+1] = " .XoO+@#$%&*=-;:>,<1234567890qwertyuipasdfghjk"
                         "lzxcvbnmMNBVCZASDFGHJKLPIUYTREWQ!~^/()_`'][{}|";

  /*
    Open output image file.
  */
  assert(image_info != (const ImageInfo *) NULL);
  assert(image_info->signature == MagickCoreSignature);
  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  assert(exception != (ExceptionInfo *) NULL);
  assert(exception->signature == MagickCoreSignature);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  status=OpenBlob(image_info,image,WriteBinaryBlobMode,exception);
  if (status == MagickFalse)
    return(status);
  if (IssRGBCompatibleColorspace(image->colorspace) == MagickFalse)
    if (IssRGBCompatibleColorspace(image->colorspace) == MagickFalse)
      (void) TransformImageColorspace(image,sRGBColorspace,exception);
  opacity=(-1);
  if ((image->alpha_trait & BlendPixelTrait) == 0)
    {
      if ((image->storage_class == DirectClass) || (image->colors > 256))
        (void) SetImageType(image,PaletteType,exception);
    }
  else
    {
      double
        alpha,
        beta;

      /*
        Identify transparent colormap index.
      */
      if ((image->storage_class == DirectClass) || (image->colors > 256))
        (void) SetImageType(image,PaletteBilevelAlphaType,exception);
      for (i=0; i < (ssize_t) image->colors; i++)
        if (image->colormap[i].alpha != (double) OpaqueAlpha)
          {
            if (opacity < 0)
              {
                opacity=i;
                continue;
              }
            alpha=(double) TransparentAlpha-(double)
              image->colormap[i].alpha;
            beta=(double) TransparentAlpha-(double)
              image->colormap[opacity].alpha;
            if (alpha < beta)
              opacity=i;
          }
      if (opacity == -1)
        {
          (void) SetImageType(image,PaletteBilevelAlphaType,exception);
          for (i=0; i < (ssize_t) image->colors; i++)
            if (image->colormap[i].alpha != (double) OpaqueAlpha)
              {
                if (opacity < 0)
                  {
                    opacity=i;
                    continue;
                  }
                alpha=(double) TransparentAlpha-(double)
                  image->colormap[i].alpha;
                beta=(double) TransparentAlpha-(double)
                  image->colormap[opacity].alpha;
                if (alpha < beta)
                  opacity=i;
              }
        }
      if (opacity >= 0)
        {
          image->colormap[opacity].red=image->transparent_color.red;
          image->colormap[opacity].green=image->transparent_color.green;
          image->colormap[opacity].blue=image->transparent_color.blue;
        }
    }
  /*
    Compute the character per pixel.
  */
  characters_per_pixel=1;
  for (k=MaxCixels; (ssize_t) image->colors > k; k*=MaxCixels)
    characters_per_pixel++;
  /*
    XPM header.
  */
  (void) WriteBlobString(image,"/* XPM */\n");
  GetPathComponent(image->filename,BasePath,basename);
  if (isalnum((int) ((unsigned char) *basename)) == 0)
    {
      (void) FormatLocaleString(buffer,MagickPathExtent,"xpm_%.1024s",basename);
      (void) CopyMagickString(basename,buffer,MagickPathExtent);
    }
  if (isalpha((int) ((unsigned char) basename[0])) == 0)
    basename[0]='_';
  for (i=1; basename[i] != '\0'; i++)
    if (isalnum((int) ((unsigned char) basename[i])) == 0)
      basename[i]='_';
  (void) FormatLocaleString(buffer,MagickPathExtent,
    "static char *%.1024s[] = {\n",basename);
  (void) WriteBlobString(image,buffer);
  (void) WriteBlobString(image,"/* columns rows colors chars-per-pixel */\n");
  (void) FormatLocaleString(buffer,MagickPathExtent,
    "\"%.20g %.20g %.20g %.20g \",\n",(double) image->columns,(double)
    image->rows,(double) image->colors,(double) characters_per_pixel);
  (void) WriteBlobString(image,buffer);
  GetPixelInfo(image,&pixel);
  for (i=0; i < (ssize_t) image->colors; i++)
  {
    const char
      *symbolic;

    /*
      Define XPM color.
    */
    pixel=image->colormap[i];
    pixel.colorspace=sRGBColorspace;
    pixel.depth=8;
    pixel.alpha=(double) OpaqueAlpha;
    (void) QueryColorname(image,&pixel,XPMCompliance,name,exception);
    if (i == opacity)
      (void) CopyMagickString(name,"None",MagickPathExtent);
    /*
      Write XPM color.
    */
    k=i % MaxCixels;
    symbol[0]=Cixel[k];
    for (j=1; j < (ssize_t) characters_per_pixel; j++)
    {
      k=((i-k)/MaxCixels) % MaxCixels;
      symbol[j]=Cixel[k];
    }
    symbol[j]='\0';
    symbolic=(const char *) GetValueFromSplayTree(xpm_symbolic,name);
    if (symbolic == (const char *) NULL)
      (void) FormatLocaleString(buffer,MagickPathExtent,
        "\"%.1024s c %.1024s\",\n",symbol,name);
    else
      (void) FormatLocaleString(buffer,MagickPathExtent,
        "\"%.1024s c %.1024s %.1024s\",\n",symbol,name,symbolic);
    (void) WriteBlobString(image,buffer);
  }
  /*
    Define XPM pixels.
  */
  (void) WriteBlobString(image,"/* pixels */\n");
  for (y=0; y < (ssize_t) image->rows; y++)
  {
    p=GetVirtualPixels(image,0,y,image->columns,1,exception);
    if (p == (const Quantum *) NULL)
      break;
    (void) WriteBlobString(image,"\"");
    for (x=0; x < (ssize_t) image->columns; x++)
    {
      k=((ssize_t) GetPixelIndex(image,p) % MaxCixels);
      symbol[0]=Cixel[k];
      for (j=1; j < (ssize_t) characters_per_pixel; j++)
      {
        k=(((int) GetPixelIndex(image,p)-k)/MaxCixels) % MaxCixels;
        symbol[j]=Cixel[k];
      }
      symbol[j]='\0';
      (void) CopyMagickString(buffer,symbol,MagickPathExtent);
      (void) WriteBlobString(image,buffer);
      p+=(ptrdiff_t) GetPixelChannels(image);
    }
    (void) FormatLocaleString(buffer,MagickPathExtent,"\"%.1024s\n",
      (y == (ssize_t) (image->rows-1) ? "" : ","));
    (void) WriteBlobString(image,buffer);
    if (image->previous == (Image *) NULL)
      {
        status=SetImageProgress(image,SaveImageTag,(MagickOffsetType) y,
          image->rows);
        if (status == MagickFalse)
          break;
      }
  }
  (void) WriteBlobString(image,"};\n");
  (void) CloseBlob(image);
  return(MagickTrue);
}
