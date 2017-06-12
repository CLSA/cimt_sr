#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmsr/dsrdoc.h"
#include "dcmtk/ofstd/ofstream.h"
#include "dcmtk/dcmdata/dcistrmb.h"

#include <iostream>
#include <fstream>
#include <map>
#include <list>
#include <vector>
#include <string>
using namespace std;

void printSRBasic( DSRDocument& );
void printTreeCursor( DSRTreeNodeCursor& );
void printNode( DSRDocumentTree& );
void printNumericEntry( DSRDocumentTree& );
void printFindings( DSRDocumentTree& );
vector<string> printCIMT( DSRDocumentTree& );
void printCLSAtest( DSRDocument& );

int main(const int argc, const char** argv)
{
/*
  DcmFileFormat fileformat;
  OFCondition status = fileformat.loadFile("/home/dean/files/data/Measure.SR-B547411.dcm");
  if ( status.good() )
  {
    DSRDocument document;
    status = document.read( *fileformat.getDataset() );
    if ( status.good() )
    {
      status = document.renderHTML( std::cout );
      if ( status.bad() )
       std::cerr << "Error: cannot render SR document (" << status.text() 
            << ")" << std::endl;
    } 
    else
    {
      std::cerr << "Error: cannot read SR document (" << status.text() 
           << ")" << std::endl;
    }
  }  
  else
  {
    std::cerr << "Error: cannot load DICOM file (" << status.text() 
         << ")" << std::endl;
  }       
*/
// now try loading the file into memory buffer and
// use a stream to read it
  if(argc<2) return EXIT_FAILURE;

  
  ifstream fileStream(argv[1],
    ios::in|ios::binary|ios::ate);

  ifstream::pos_type fileSize = 0;
  char* memoryBuffer;
  bool readSuccess = false;
  if( fileStream.is_open() )
  {
    fileSize = fileStream.tellg();
    memoryBuffer = new char[fileSize];
    fileStream.seekg( 0, ios::beg );
    fileStream.read( memoryBuffer, fileSize );
    fileStream.close();
    readSuccess = true;
  }

  if(!readSuccess) return EXIT_FAILURE;
  
  
  DcmInputBufferStream bufferStream;
  offile_off_t length = fileSize;

  bufferStream.setBuffer((void*)memoryBuffer, length );
  bufferStream.setEos();
  
  DcmFileFormat fileFormat;
  OFCondition status = fileFormat.read( bufferStream, EXS_LittleEndianExplicit );
  
  if ( status.good() )
  {
    DSRDocument srDocument;
    status = srDocument.read( *fileFormat.getDataset() );
    if ( status.good() )
    {
      if ( status.bad() )
       cerr << "Error: cannot render SR document (" << status.text() 
            << ")" << endl;
      
      // get the tree and find what we want
      DSRDocumentTree& srTree = srDocument.getTree();
      cout << (srTree.isValid() ? "valid tree" : "invalid tree" ) << endl; 
/*
      srTree.gotoRoot();
      DSRContentItem& ciref = srTree.getCurrentContentItem();
      if(ciref.isValid())
      {
         cout << "root content item: " << ciref.getStringValue() << endl;
         cout << "vtype: " << ciref.getValueType() << ", rtype: " << 
               ciref.getRelationshipType() << endl;
      }

      DSRCodedEntryValue val;
      val.setCode( OFString("G-C171"),OFString("SRT"),OFString("Finding Site"));
      size_t loc = srTree.gotoNamedNode(val);
      cout << "tried to go to : " << loc << endl;

      //ciref = srTree.getCurrentContentItem();
      if(srTree.getCurrentContentItem().isValid())
      {
         cout << "content item: " << srTree.getCurrentContentItem().getStringValue() << endl;
         cout << "vtype: " << srTree.getCurrentContentItem().getValueType() << ", rtype: " << 
               srTree.getCurrentContentItem().getRelationshipType() << endl;
      }

*/
     
     srTree.gotoRoot();
     if( srTree.isValid() )
     {
       do {
         printNode( srTree );
       }while (srTree.iterate());
     }

     cout << "finding only cIMT relevent entries: " << endl;
     srTree.gotoRoot();
     map<string,vector<pair<double,string>>> dataSets;
     if( srTree.isValid() )
     {
       do {
         vector<string> values = printCIMT( srTree );
         if(!values.empty())
         {
           string units = values.back();
           values.pop_back();
           string value = values.back();
           values.pop_back();
           string name = values.back();
           values.pop_back();
           dataSets[name].push_back(pair<double,string>(atof(value.c_str()),units));
         }
       }while (srTree.iterate());
     }

     
     // process the map to extract each set of values
     // retain sets with min < avg < max, nmeas > 0, SD > 0
     // retain the last validated set?
     vector<double> ave;
     vector<double> min;
     vector<double> max;
     vector<double> nmeas;
     vector<double> std;
     int idx = -1;
     if(!dataSets.empty())
     {
       for(auto it = dataSets.begin(); it != dataSets.end(); it++)
       {
          string name = it->first;
          vector<pair<double,string>> valueSet = it->second;
          for(auto vit = valueSet.begin(); vit!= valueSet.end(); vit++)
          {
            if(string::npos != name.find("Average"))
              ave.push_back(vit->first);
            else if(string::npos != name.find("Min"))
              min.push_back(vit->first);
            else if(string::npos != name.find("Max"))
              max.push_back(vit->first);
            else if(string::npos != name.find("SD"))
              std.push_back(vit->first);
            else if(string::npos != name.find("nMeas"))
              nmeas.push_back(vit->first);
          }  
       }
       
       int size = ave.size();
       if(size==min.size() &&
          size==max.size() &&
          size==std.size() &&
          size==nmeas.size())
       {
         for(int i=0; i<size; i++)
         {
           if(min[i]<ave[i] && ave[i]<max[i] && 0<nmeas[i])
           {
             idx = i;
           }
         }
         if(-1 != idx)
         {
           if(1<size)
             cout << "multiple values (" << size << ") using set " << idx+1 << endl;
           else
             cout << "using only available value set" << endl;
         }
         else
         {
           cout << "WARNING: no self-consistent set found in " << size << " sets" << endl;
           idx = size-1;
         }
       }
       else
       {
         if(1<size)
         {
           cout << "WARNING: inconsistent number of elements among sets, using last complete set" << endl;
           // get the min size
           list<int> sz;
           sz.push_back(size);
           sz.push_back(max.size());
           sz.push_back(min.size());
           sz.push_back(std.size());
           sz.push_back(nmeas.size());
           sz.sort();           
           idx = 0 < sz.front() ? sz.front()-1 : -1;
         }
       }
       
     }
     
     if(-1 != idx)
     {
       cout << "IMT Posterior Average: " << ave[idx] << " (millimeter)" << endl;
       cout << "IMT Posterior Max    : " << max[idx] << " (millimeter)" << endl;
       cout << "IMT Posterior Min    : " << min[idx] << " (millimeter)" << endl;
       cout << "IMT Posterior SD     : " << std[idx] << " (millimeter)" << endl;
       cout << "IMT Posterior nMeas  : " << static_cast<int>(nmeas[idx]) << " (no units)" << endl;
     }

     printCLSAtest( srDocument );

    } 
    else
    {
      cerr << "Error: cannot read SR document (" << status.text() 
           << ")" << endl;
    }
  }  
  else
  {
    cerr << "Error: cannot load DICOM data (" << status.text() 
         << ")" << endl;
  }
  
  bufferStream.releaseBuffer();

  delete [] memoryBuffer;

  return EXIT_SUCCESS;
}

//----------------------------------------------------------------
void printTreeCursor( DSRTreeNodeCursor& cursor )
{
  if(cursor.isValid())
  {
  OFString pos;
  cursor.getPosition(pos);
  cout << "cursor at node id: " 
       << cursor.getNodeID() 
       << ", pos: " 
       << pos << endl;
  }
  else cout << "cursor is invalid, cant print" << endl;
}

//----------------------------------------------------------------
void printNode( DSRDocumentTree& tree)
{
  DSRDocumentTreeNode* node = 
    OFstatic_cast( DSRDocumentTreeNode*, tree.getNode() );
  if( node == NULL ) return;
  const DSRCodedEntryValue conceptName = node->getConceptName();
  if( conceptName.isValid() )
  {
    conceptName.print( cout );
    cout << ", ";
  }
  else cout << "invalid conceptName, ";
  cout << "ID: " << node->getNodeID()
       << ", rtype: " << 
       DSRTypes::relationshipTypeToReadableName(
         node->getRelationshipType() )
       << ", vtype: " << 
       DSRTypes::valueTypeToReadableName(
         node->getValueType() )
       << endl;
 
 if( node->hasChildNodes() ) cout << "node has children" << endl;
 if( node->hasSiblingNodes() ) cout << "node has siblings" << endl;
 if( !node->hasChildNodes() && !node->hasSiblingNodes() )
   cout << "node is terminal" << endl;

  DSRContentItem& item = tree.getCurrentContentItem();
  if( item.isValid() )
  {
     DSRNumericMeasurementValue* nmv = item.getNumericValuePtr();

     if(nmv)
     {
       cout << nmv->getNumericValue() <<" ("
            << nmv->getMeasurementUnit().getCodeMeaning() << ")" << endl; 
     }
  }
 cout << "---------------------------------------" << endl;
}

//----------------------------------------------------------------
void printFindings( DSRDocumentTree& tree )
{
  DSRDocumentTreeNode* node = 
    OFstatic_cast( DSRDocumentTreeNode*, tree.getNode() );
  if( node == NULL ) return;
  if( node->getRelationshipType() == DSRTypes::RT_hasConceptMod &&
      node->getValueType() == DSRTypes::VT_Code )
  {
    DSRContentItem& item = tree.getCurrentContentItem();
    if( item.isValid() )
    {
      if( item.getConceptName().getCodeMeaning() != "Derivation" )
      {
        cout << item.getConceptName().getCodeMeaning() << ", "
             << item.getCodeValue().getCodeMeaning() << endl;
      } 
    }
  } 
}

//----------------------------------------------------------------
void printNumericEntry( DSRDocumentTree& tree )
{
  DSRDocumentTreeNode* node = 
    OFstatic_cast( DSRDocumentTreeNode*, tree.getNode() );
  if( node == NULL ) return;
  if( node->getRelationshipType() == DSRTypes::RT_contains &&
      node->getValueType() == DSRTypes::VT_Num )
  {
    DSRContentItem& item = tree.getCurrentContentItem();
    if( item.isValid() )
    {
      DSRNumericMeasurementValue* nmv = item.getNumericValuePtr();
      if(nmv)
      {
        cout << node->getConceptName().getCodeMeaning() << ": " 
             << nmv->getNumericValue() <<" ("
             << nmv->getMeasurementUnit().getCodeMeaning() << ")" << endl; 
      }
    }
  } 
}

//----------------------------------------------------------------
vector<string> printCIMT( DSRDocumentTree& tree )
{
  vector<string> values;
  DSRDocumentTreeNode* node = 
    OFstatic_cast( DSRDocumentTreeNode*, tree.getNode() );
  if( node == NULL ) return values;
  DSRContentItem& item = tree.getCurrentContentItem();
  if( !item.isValid() ) return values;
  if( node->getRelationshipType() == DSRTypes::RT_hasConceptMod &&
      node->getValueType() == DSRTypes::VT_Code )
  {
    if( item.getConceptName().getCodeMeaning() != "Derivation" )
    {
      cout << item.getConceptName().getCodeMeaning() << ", "
           << item.getCodeValue().getCodeMeaning() << endl;
    } 
  }
  else if( node->getRelationshipType() == DSRTypes::RT_contains &&
      node->getValueType() == DSRTypes::VT_Num )
  {
    DSRNumericMeasurementValue* nmv = item.getNumericValuePtr();
    if(nmv)
    {
      string name = node->getConceptName().getCodeMeaning().c_str();
      string value = nmv->getNumericValue().c_str();
      string units = nmv->getMeasurementUnit().getCodeMeaning().c_str();
      cout << name << ": " << value << " (" << units << ")" << endl;
      values.push_back(name);
      values.push_back(value);
      values.push_back(units);
    }
  }
  return values;
}


void printCLSAtest( DSRDocument& srDocument )
{
  OFString ofstr;
  cout << endl;
  srDocument.getPatientID(ofstr);
  cout << "CLSA DCS Visit ID (PatientID) :" << ofstr << endl;

  DSRDocumentTree& tree = srDocument.getTree();
  if( !tree.isValid() )
  {
    cout << "Error: invalid tree" << endl;
    return;
  }

  DSRCodedEntryValue codedEV( "G-C171","SRT","Laterality"); 
  tree.gotoNamedNode( codedEV );
  DSRContentItem& item1 = tree.getCurrentContentItem();
  if( !item1.isValid() )
  {
    cout << "Error: invalid item" << endl;
    return;
  }
  cout << item1.getConceptName().getCodeMeaning() << ", "
       << item1.getCodeValue().getCodeMeaning() << endl;

  codedEV.clear();
  codedEV.setCode("GEU-1005-26","99GEMS","IMT Posterior Average");
  tree.gotoNamedNode( codedEV );
  DSRContentItem& item2 = tree.getCurrentContentItem();
  if( !item2.isValid() )
  {
    cout << "Error: invalid item" << endl;
    return;
  }
  DSRNumericMeasurementValue* nmv = item2.getNumericValuePtr();
  cout << item2.getConceptName().getCodeMeaning() << ": " 
       << nmv->getNumericValue() <<" ("
       << nmv->getMeasurementUnit().getCodeMeaning() << ")" << endl; 

  codedEV.clear();
  codedEV.setCode("GEU-1005-27","99GEMS","IMT Posterior Max");
  tree.gotoNamedNode( codedEV );
  DSRContentItem& item3 = tree.getCurrentContentItem();
  if( !item3.isValid() )
  {
    cout << "Error: invalid item" << endl;
    return;
  }
  nmv = item3.getNumericValuePtr();
  cout << item3.getConceptName().getCodeMeaning() << ": " 
       << nmv->getNumericValue() <<" ("
       << nmv->getMeasurementUnit().getCodeMeaning() << ")" << endl; 

  codedEV.clear();
  codedEV.setCode("GEU-1005-28","99GEMS","IMT Posterior Min");
  tree.gotoNamedNode( codedEV );
  DSRContentItem& item4 = tree.getCurrentContentItem();
  if( !item4.isValid() )
  {
    cout << "Error: invalid item" << endl;
    return;
  }
  nmv = item4.getNumericValuePtr();
  cout << item4.getConceptName().getCodeMeaning() << ": " 
       << nmv->getNumericValue() <<" ("
       << nmv->getMeasurementUnit().getCodeMeaning() << ")" << endl; 

  codedEV.clear();
  codedEV.setCode("GEU-1005-29","99GEMS","IMT Posterior SD");
  tree.gotoNamedNode( codedEV );
  DSRContentItem& item5 = tree.getCurrentContentItem();
  if( !item5.isValid() )
  {
    cout << "Error: invalid item" << endl;
    return;
  }
  nmv = item5.getNumericValuePtr();
  cout << item5.getConceptName().getCodeMeaning() << ": " 
       << nmv->getNumericValue() <<" ("
       << nmv->getMeasurementUnit().getCodeMeaning() << ")" << endl; 

  codedEV.clear();
  codedEV.setCode("GEU-1005-30","99GEMS","IMT Posterior nMeas");
  tree.gotoNamedNode( codedEV );
  DSRContentItem& item6 = tree.getCurrentContentItem();
  if( !item6.isValid() )
  {
    cout << "Error: invalid item" << endl;
    return;
  }
  nmv = item6.getNumericValuePtr();
  cout << item6.getConceptName().getCodeMeaning() << ": " 
       << nmv->getNumericValue() <<" ("
       << nmv->getMeasurementUnit().getCodeMeaning() << ")" << endl; 

}

//----------------------------------------------------------------
void printSRBasic( DSRDocument& srDocument )
{
  // excercise some direct basic querying
  cout << endl;
  cout << "BASIC dumping of SR document" << endl;
  OFString ofstr;
  srDocument.getModality(ofstr);
  cout << "modality :" << ofstr << endl;
  srDocument.getSOPClassUID(ofstr);
  cout << "SOPClassUID :" << ofstr << endl;
  srDocument.getStudyInstanceUID(ofstr);
  cout << "StudyInstanceUID :" << ofstr << endl;
  srDocument.getSeriesInstanceUID(ofstr);
  cout << "SeriesInstanceUID :" << ofstr << endl;
  srDocument.getSOPInstanceUID(ofstr);
  cout << "SOPInstanceUID :" << ofstr << endl;
  srDocument.getInstanceCreatorUID(ofstr);
  cout << "InstanceCreatorUID :" << ofstr << endl;
  srDocument.getStudyDate(ofstr);
  cout << "StudyDate :" << ofstr << endl;
  srDocument.getStudyTime(ofstr);
  cout << "StudyTime :" << ofstr << endl;
  srDocument.getInstanceCreationDate(ofstr);
  cout << "InstanceCreationDate :" << ofstr << endl;
  srDocument.getInstanceCreationTime(ofstr);
  cout << "InstanceCreationTime :" << ofstr << endl;
  srDocument.getContentDate(ofstr);
  cout << "ContentDate :" << ofstr << endl;
  srDocument.getContentTime(ofstr);
  cout << "ContentTime :" << ofstr << endl;
  srDocument.getStudyID(ofstr);
  cout << "StudyID :" << ofstr << endl;
  srDocument.getPatientID(ofstr);
  cout << "PatientID :" << ofstr << endl;
  srDocument.getSeriesNumber(ofstr);
  cout << "SeriesNumber :" << ofstr << endl;
  srDocument.getInstanceNumber(ofstr);
  cout << "InstanceNumber :" << ofstr << endl;
}
