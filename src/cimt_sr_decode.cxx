#include "vtkDICOMMetaData.h"
#include "vtkDICOMParser.h"
#include "vtkDICOMDictionary.h"
#include "vtkDICOMItem.h"
#include "vtkDICOMUtilities.h"

#include <vtkSmartPointer.h>
#include <vtkStringArray.h>

#include <sstream>

#include <string.h>
#include <stdlib.h>


#define MAX_INDENT 24
#define INDENT_SIZE 2
#define MAX_LENGTH 120

// macro for performing tests
#define TestAssert(t) \
if (!(t)) \
{ \
  cout << exename << ": Assertion Failed: " << #t << "\n"; \
  cout << __FILE__ << ":" << __LINE__ << "\n"; \
  cout.flush(); \
  rval |= 1; \
}

void printElement(
 vtkDICOMMetaData *meta, const vtkDICOMItem *item,
 const vtkDICOMDataElementIterator &iter, int depth )
{
  vtkDICOMTag tag = iter->GetTag();
  int g = tag.GetGroup();
  int e = tag.GetElement();
  vtkDICOMVR vr = iter->GetVR();
  const char *name = "";
  vtkDICOMDictEntry d;
  if (item)
    {
    d = item->FindDictEntry(tag);
    }
  else if (meta)
    {
    d = meta->FindDictEntry(tag);
    }
  if (d.IsValid())
    {
    name = d.GetName();
    if (d.GetVR() != vr &&
        !(d.GetVR() == vtkDICOMVR::XS &&
          (vr == vtkDICOMVR::SS || vr == vtkDICOMVR::US)) &&
        !(d.GetVR() == vtkDICOMVR::OX &&
          (vr == vtkDICOMVR::OB || vr == vtkDICOMVR::OW)))
      {
      printf("VR mismatch! %s != %s %s\n",
             vr.GetText(), d.GetVR().GetText(), name);
      }
    }
  else if ((tag.GetGroup() & 0xFFFE) != 0 && tag.GetElement() == 0)
    {
    // group is even, element is zero
    name = "GroupLength";
    }
  else if ((tag.GetGroup() & 0x0001) != 0 &&
           (tag.GetElement() & 0xFF00) == 0)
    {
    // group is odd, element is a creator element
    name = "PrivateCreator";
    }
  // allow multiple values (i.e. for each image in series)
  vtkDICOMValue v = iter->GetValue();
  unsigned int vn = v.GetNumberOfValues();
  const vtkDICOMValue *vp = v.GetMultiplexData();
  if (vp == 0)
    {
    vp = &v;
    vn = 1;
    }

  // make an indentation string
  if (INDENT_SIZE*depth > MAX_INDENT)
    {
    depth = MAX_INDENT/INDENT_SIZE;
    }
  static const char spaces[MAX_INDENT+1] = "                        ";
  const char *indent = spaces + (MAX_INDENT - INDENT_SIZE*depth);

  for (unsigned int vi = 0; vi < vn; vi++)
    {
    v = vp[vi];
    unsigned int vl = v.GetVL();
    std::string s;
    if (vr == vtkDICOMVR::UN ||
        vr == vtkDICOMVR::SQ)
      {
      // sequences are printed later
      s = (vl > 0 ? "..." : "");
      }
    else if (vr == vtkDICOMVR::LT ||
             vr == vtkDICOMVR::ST ||
             vr == vtkDICOMVR::UT)
      {
      const char *cp = v.GetCharData();
      unsigned int j = 0;
      while (j < vl && cp[j] != '\0')
        {
        unsigned int k = j;
        unsigned int m = j;
        for (; j < vl && cp[j] != '\0'; j++)
          {
          m = j;
          if (cp[j] == '\r' || cp[j] == '\n' || cp[j] == '\f')
            {
            do { j++; }
            while (j < vl && (cp[j] == '\r' || cp[j] == '\n' || cp[j] == '\f'));
            break;
            }
          m++;
          }
        if (j == vl)
          {
          while (m > 0 && cp[m-1] == ' ') { m--; }
          }
        if (k != 0)
          {
          s.append("\\\\");
          }
        s.append(&cp[k], m-k);
        if (s.size() > MAX_LENGTH)
          {
          s.resize(MAX_LENGTH-4);
          s.append("...");
          break;
          }
        }
      }
    else
      {
      // print any other VR via conversion to string
      unsigned int n = v.GetNumberOfValues();
      size_t pos = 0;
      for (unsigned int i = 0; i < n; i++)
        {
        v.AppendValueToUTF8String(s, i);
        if (i < n - 1)
          {
          s.append("\\");
          }
        if (s.size() > MAX_LENGTH-4)
          {
          s.resize(pos);
          s.append("...");
          break;
          }
        pos = s.size();
        }
      }

    if (meta && vi == 0)
      {
      printf("%s(%04X,%04X) %s \"%s\" :", indent, g, e, vr.GetText(), name);
      }
    if (meta && vn > 1)
      {
      printf("%s%s  %4.4u",
        (vi == 0 ? " (multiple values)\n" : ""), indent, vi + 1);
      }
    if (vr == vtkDICOMVR::SQ)
      {
      size_t m = v.GetNumberOfValues();
      const vtkDICOMItem *items = v.GetSequenceData();
      printf(" (%u %s%s)\n",
        static_cast<unsigned int>(m), (m == 1 ? "item" : "items"),
        (vl == 0xffffffffu ? ", delimited" : ""));
      for (size_t j = 0; j < m; j++)
        {
        printf("%s%s---- SQ Item %04u at offset %u ----\n",
          indent, spaces+(MAX_INDENT - INDENT_SIZE),
          static_cast<unsigned int>(j+1),
          items[j].GetByteOffset());
        vtkDICOMDataElementIterator siter = items[j].Begin();
        vtkDICOMDataElementIterator siterEnd = items[j].End();

        for (; siter != siterEnd; ++siter)
          {
          printElement(meta, &items[j], siter, depth+1);
          }
        }
      }
    else if (vl == 0xffffffffu)
      {
      if (tag == DC::PixelData ||
          tag == DC::FloatPixelData ||
          tag == DC::DoubleFloatPixelData)
        {
        printf(" [...] (compressed)\n");
        }
      else
        {
        printf(" [...] (delimited)\n");
        }
      }
    else
      {
      const char *uidName = "";
      if (vr == vtkDICOMVR::UI)
        {
        uidName = vtkDICOMUtilities::GetUIDName(s.c_str());
        }
      if (uidName[0] != '\0')
        {
        printf(" [%s] {%s} (%u bytes)\n", s.c_str(), uidName, vl);
        }
      else if (vr == vtkDICOMVR::OB ||
               vr == vtkDICOMVR::OW ||
               vr == vtkDICOMVR::OF ||
               vr == vtkDICOMVR::OD)
        {
        printf(" [%s] (%u bytes)\n", (vl == 0 ? "" : "..."), vl);
        }
      else
        {
        printf(" [%s] (%u bytes)\n", s.c_str(), vl);
        }
      }
    }
}

int main(int argc, char *argv[])
{
  int rval = 0;

  const char *exename = argv[0];

  // remove path portion of exename
  const char *cp = exename + strlen(exename);
  while (cp != exename && cp[-1] != '\\' && cp[-1] != '/') { --cp; }
  exename = cp;

  if( 2 != argc )
  {
    std::cout << "usage: " << exename << " dicom_file_name" << std::endl;
    return rval;
  }

  const char *filename = argv[1];

  vtkSmartPointer<vtkDICOMParser> parser =
    vtkSmartPointer<vtkDICOMParser>::New();

  vtkSmartPointer<vtkDICOMMetaData> meta =
    vtkSmartPointer<vtkDICOMMetaData>::New();

  parser->SetFileName( filename );
  parser->SetMetaData( meta );  
  parser->Update();

  std::string seq_code = "SQ";

  for( vtkDICOMDataElementIterator iter = meta->Begin(); iter != meta->End(); ++iter )
  {

   printElement( meta, 0, iter, 0 );
/*
    vtkDICOMTag tag = iter->GetTag();
    vtkDICOMVR vr = iter->GetVR();
    vtkDICOMValue value = iter->GetValue();
    vtkDICOMDictEntry dict;

    if( !value.IsValid() )
      std::cout << "tag: " << tag << ", INVALID VALUE" << std::endl;
    else
    {
      dict = meta->FindDictEntry( tag );
     const char* name = dict.IsValid() ? dict.GetName() : "Private";

      std::cout << tag << ": " << vr << ", " << name << ": " << value << std::endl;
      if( vr == vtkDICOMVR::SQ )
      {
        size_t m = value.GetNumberOfValues();
      }  
    }  
  */  
  }

  return rval;
}
