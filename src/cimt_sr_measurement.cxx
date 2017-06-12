#include "vtkDICOMMetaData.h"
#include "vtkDICOMParser.h"
#include "vtkDICOMDictionary.h"
#include "vtkDICOMTagPath.h"
#include "vtkDICOMSequence.h"
#include "vtkDICOMItem.h"
#include "vtkDICOMUtilities.h"

#include <vtkSmartPointer.h>
#include <vtkStringArray.h>

#include <sstream>

#include <string.h>
#include <stdlib.h>

// const dicom tag paths used for pretty printing
const vtkDICOMTagPath titlePath( DC::ConceptNameCodeSequence, 0, DC::CodeMeaning );
const vtkDICOMTagPath valuePath( DC::ConceptCodeSequence, 0, DC::CodeMeaning );
const vtkDICOMTagPath measuredValuePath( DC::MeasuredValueSequence, 0, DC::NumericValue );
const vtkDICOMTagPath unitsPath( DC::MeasuredValueSequence, 0,
                             DC::MeasurementUnitsCodeSequence, 0, DC::CodeMeaning );

void PrintSequence( const vtkDICOMSequence& );
void PrintSequenceItem( const vtkDICOMItem& );

// fill an SR sequence with items
void FillSequence( vtkDICOMSequence* seq, 
                   const std::string& codeValue,
                   const std::string& codingSchemeDescriptor,
                   const std::string& codeMeaning )
{
  vtkDICOMItem seqItem;
  seqItem.SetAttributeValue( DC::CodeValue,
                             vtkDICOMValue( vtkDICOMVR::SH, codeValue ) );
  seqItem.SetAttributeValue( DC::CodingSchemeDesignator,
                             vtkDICOMValue( vtkDICOMVR::SH, codingSchemeDescriptor ) );
  seqItem.SetAttributeValue( DC::CodeMeaning,
                             vtkDICOMValue( vtkDICOMVR::LO, codeMeaning ) );
  seq->Clear();
  seq->AddItem( seqItem );
}

// print an SR sequence
void PrintSequence( const vtkDICOMSequence& seq )
{
  for( size_t i = 0; i < seq.GetNumberOfItems(); ++i )
    {
    vtkDICOMItem item = seq.GetItem( i );
    if( !item.IsEmpty() )
      PrintSequenceItem( item );
    }
}

// print an SR sequence item recursively
void PrintSequenceItem( const vtkDICOMItem& item )
{
  for( vtkDICOMDataElementIterator it = item.Begin(); it != item.End(); ++it )
    {
    vtkDICOMDataElement el = *it;
    std::cout << el.GetTag() << ","<< el.GetVR() << "," << el.GetValue().AsString() << std::endl;
    if( el.GetVR() == vtkDICOMVR::SQ )
      PrintSequenceItem( el.GetValue().AsItem()  );
    }
}

// find a matching SR sequence within a content sequence
void RecurseSequence( const vtkDICOMSequence& containerSeq, 
  const vtkDICOMSequence& desiredSeq, 
  vtkDICOMSequence* foundSeq )
{
  for( size_t i = 0; i < containerSeq.GetNumberOfItems(); ++i )
    {
    vtkDICOMItem item = containerSeq.GetItem( i );
    if( !item.IsEmpty() )
      {
      if( item.GetAttributeValue( DC::ConceptNameCodeSequence ).Matches( desiredSeq ) )
        {
        foundSeq->AddItem( item );
        }
      else if( item.GetAttributeValue( DC::ValueType ).Matches( "CONTAINER" ) )
        for( vtkDICOMDataElementIterator it = item.Begin(); it != item.End(); ++it )
          {
          vtkDICOMDataElement el = *it;
          if( el.GetVR() == vtkDICOMVR::SQ )
            {
            vtkDICOMSequence seq = el.GetValue();
            RecurseSequence( seq, desiredSeq, foundSeq );
            }
          } 
      }
    }
}

// print SR values nicely
void PrettyPrint( const vtkDICOMSequence& seq, 
  const vtkDICOMTagPath& titlePath,
  const vtkDICOMTagPath& valuePath,
  const vtkDICOMTagPath& unitsPath = vtkDICOMTagPath())
{
  std::string titleStr;
  std::string valueStr;
  std::string unitsStr;

  vtkDICOMValue v = seq.GetAttributeValue( 0, titlePath );
  titleStr = v.AsString();
  v = seq.GetAttributeValue( 0, valuePath );
  valueStr = v.AsString();

  std::cout << titleStr << ": " << valueStr;
  if( unitsPath.GetHead() == vtkDICOMTag() ) // empty, no units
    {
    std::cout << std::endl;
    }
  else
    {
    v = seq.GetAttributeValue( 0, unitsPath );
    unitsStr = v.AsString();
    std::cout << " (" << unitsStr << ")" << std::endl;
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

  vtkDICOMSequence contentSeq = meta->GetAttributeValue(DC::ContentSequence);
  vtkDICOMSequence foundSeq;
  vtkDICOMSequence desiredSeq;

  std::cout << "LATERALITY--------------------" << std::endl;
  FillSequence( &desiredSeq, "G-C171", "SRT", "Laterality" );
  RecurseSequence( contentSeq, desiredSeq, &foundSeq );
  //PrintSequence( foundSeq );
  PrettyPrint( foundSeq, titlePath, valuePath );

  std::cout << "AVE--------------------" << std::endl;
  FillSequence( &desiredSeq, "GEU-1005-26", "99GEMS", "IMT Posterior Average" );
  foundSeq.Clear();
  RecurseSequence( contentSeq, desiredSeq, &foundSeq );
  //PrintSequence( foundSeq );
  PrettyPrint( foundSeq, titlePath, measuredValuePath, unitsPath );

  std::cout << "MAX--------------------" << std::endl;
  FillSequence( &desiredSeq, "GEU-1005-27", "99GEMS", "IMT Posterior Max" );
  foundSeq.Clear();
  RecurseSequence( contentSeq, desiredSeq, &foundSeq );
  //PrintSequence( foundSeq );
  PrettyPrint( foundSeq, titlePath, measuredValuePath, unitsPath );

  std::cout << "MIN--------------------" << std::endl;
  FillSequence( &desiredSeq, "GEU-1005-28", "99GEMS", "IMT Posterior Min" );
  foundSeq.Clear();
  RecurseSequence( contentSeq, desiredSeq, &foundSeq );
  //PrintSequence( foundSeq );
  PrettyPrint( foundSeq, titlePath, measuredValuePath, unitsPath );

  std::cout << "STDDEV--------------------" << std::endl;
  FillSequence( &desiredSeq, "GEU-1005-29", "99GEMS", "IMT Posterior SD" );
  foundSeq.Clear();
  RecurseSequence( contentSeq, desiredSeq, &foundSeq );
  //PrintSequence( foundSeq );
  PrettyPrint( foundSeq, titlePath, measuredValuePath, unitsPath );

  std::cout << "nMEAS--------------------" << std::endl;
  FillSequence( &desiredSeq, "GEU-1005-30", "99GEMS", "IMT Posterior nMeas" );
  foundSeq.Clear();
  RecurseSequence( contentSeq, desiredSeq, &foundSeq );
  //PrintSequence( foundSeq );
  PrettyPrint( foundSeq, titlePath, measuredValuePath, unitsPath );

  return rval;
}
