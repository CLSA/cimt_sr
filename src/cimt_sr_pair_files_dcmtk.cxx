#include <vtkDirectory.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtksys/Glob.hxx>
#include <vtksys/SystemTools.hxx>

#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmsr/dsrdoc.h"
#include "dcmtk/ofstd/ofstream.h"
#include "dcmtk/dcmdata/dcistrmb.h"

#include "options.h"

#include <sstream>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <map>
#include <list>
#include <vector>
#include <string>

using namespace std;

vector<string> extractCIMT( DSRDocumentTree& tree )
{
  vector<string> values;
  DSRDocumentTreeNode* node =
    OFstatic_cast( DSRDocumentTreeNode*, tree.getNode() );
  if( NULL == node ) return values;
  DSRContentItem& item = tree.getCurrentContentItem();
  if( !item.isValid() ) return values;
  if( node->getRelationshipType() == DSRTypes::RT_contains &&
      node->getValueType() == DSRTypes::VT_Num )
  {
    DSRNumericMeasurementValue* nmv = item.getNumericValuePtr();
    if(nmv)
    {
      string name = node->getConceptName().getCodeMeaning().c_str();
      string value = nmv->getNumericValue().c_str();
      string units = nmv->getMeasurementUnit().getCodeMeaning().c_str();
      values.push_back(name);
      values.push_back(value);
      values.push_back(units);
    }
  }
  return values;
}

class cIMTMeasure
{
  public:
    cIMTMeasure( const vtkVariant& v, const string& s, const string& u ) :
      value( v ), label( s ), units( u ) {}

    cIMTMeasure() : value( vtkVariant() ) { this->label.clear(); this->units.clear(); }

    vtkVariant GetValue() const { return this->value; }
    void SetValue( const vtkVariant& v ) { this->value = v; }
    string GetLabel() const { return this->label; }
    void SetLabel( const string& l ) { this->label = l; }
    string GetUnits() const { return this->units; }
    void SetUnits( const string& u ) { this->units = u; }

    bool HasUnits() const { return !this->units.empty(); }

    cIMTMeasure& operator=(const cIMTMeasure& o ) {
      this->label = o.label;
      this->value = o.value;
      this->units = o.units;
      return *this; }
    bool operator==(const cIMTMeasure& o) const {
      return ( this->value == o.value &&
               this->label == o.label &&
               this->units == o.units ); }
    bool operator!=(const cIMTMeasure& o) const { return !(*this == o); }

  private:
    vtkVariant value;
    string label;
    string units;
};

ostream& operator<<(ostream& o, const cIMTMeasure& m)
{
  string s = m.GetLabel();
  s += string( " : " );
  s += m.GetValue().ToString();
  if( m.HasUnits() )
    {
    s += string( " (" );
    s += m.GetUnits();
    s += string(  ")" );
    }
  o << s;
  return o;
}

typedef vector<cIMTMeasure> cIMTMeasureVector;

class cIMTMeasureKey
{
  public:
    cIMTMeasureKey( const string& uid, const string& datetime )
      {
      this->uid=uid;
      this->datetime=datetime;
      }
    string uid;
    string datetime;
    bool operator<(const cIMTMeasureKey& k) const
      {
      int s_cmp = this->uid.compare(k.uid);
      if(0 == s_cmp)
        {
          return this->datetime < k.datetime;
        }
      return s_cmp < 0;
      }
};


void RecurseDirectorySearch( vtkStringArray* files, const string& folder )
{
  vtkSmartPointer<vtkDirectory> dir =
    vtkSmartPointer<vtkDirectory>::New();

  string s_dcm = ".dcm";
  string s_cwd = folder;
  string s_dot = ".";
  string s_dotdot = "..";

  unsigned last = s_cwd.find_last_of("/");
  if( last != (s_cwd.length()-1) )
    s_cwd += "/";

  dir->Open( s_cwd.c_str() );
  if( 0 == dir->GetNumberOfFiles() ) return;

  for( int i = 0; i < dir->GetNumberOfFiles(); ++i )
    {
    string s = dir->GetFile(i);
    if( s == s_dot || s == s_dotdot )
      continue;
    s = s_cwd + s;
    if( dir->FileIsDirectory( s.c_str() ) )
      {
      RecurseDirectorySearch( files, s );
      }
    else
      {
      string ext = vtksys::SystemTools::GetFilenameLastExtension(s);
      if( ext == s_dcm )
        {
        files->InsertNextValue(s);
        }
      }
    }
}

int main(const int argc, const char** argv)
{
  int rval = EXIT_SUCCESS;

  options opts( argv[0] );

  opts.add_input( "input" );
  opts.add_flag(  'v', "verbose", "Be verbose when generating SR data" );
  opts.add_option( "output", "", "Output SR measurements to a CSV file" );
  try
    {
    opts.set_arguments( argc, argv );
    if( opts.process() )
      {
      // now either show the help or run the application
      if( opts.get_flag( "help" ) )
        {
        opts.print_usage();
        }
      else
        {
        string foldername = opts.get_input( "input" );
        if( !vtksys::SystemTools::FileIsDirectory( foldername.c_str() ) )
          {
          stringstream stream;
          stream << "The input folder \"" << foldername <<"\" does not exist";
          throw runtime_error( stream.str() );
          }
        string output_file = opts.get_option( "output" );
        bool verbose = opts.get_flag( "verbose" );

        vtkSmartPointer<vtkStringArray> dcmFiles =
          vtkSmartPointer<vtkStringArray>::New();

        string s_cwd = foldername;
        RecurseDirectorySearch( dcmFiles, s_cwd );

        if( 0 == dcmFiles->GetNumberOfValues() )
          {
          vtksys::Glob glob;
          glob.RecurseOn();
          glob.SetRelative( s_cwd.c_str() );
          if( glob.FindFiles( "*.dcm" ) )
            {
            vector<string> files = glob.GetFiles();
            if( !files.empty() )
              {
              for(auto it = files.begin(); it != files.end(); ++it )
                {
                cout << *it << endl;
                }
              }
            }
          }
        if( 0 == dcmFiles->GetNumberOfValues() )
          {
          stringstream stream;
          stream << "No dicom files found in " << s_cwd;
          throw runtime_error( stream.str() );
          }

        DSRCodedEntryValue codedEV1( "G-C171","SRT","Laterality");
        DSRCodedEntryValue codedEV2( "GEU-1005-26","99GEMS","IMT Posterior Average");
        DSRCodedEntryValue codedEV3( "GEU-1005-27","99GEMS","IMT Posterior Max");
        DSRCodedEntryValue codedEV4( "GEU-1005-28","99GEMS","IMT Posterior Min");
        DSRCodedEntryValue codedEV5( "GEU-1005-29","99GEMS","IMT Posterior SD");
        DSRCodedEntryValue codedEV6( "GEU-1005-30","99GEMS","IMT Posterior nMeas");
        vector<DSRCodedEntryValue> required_codes;
        required_codes.push_back(codedEV1);
        required_codes.push_back(codedEV2);
        required_codes.push_back(codedEV3);
        required_codes.push_back(codedEV4);
        required_codes.push_back(codedEV5);
        required_codes.push_back(codedEV6);

        vector<string> cIMTLabels;
        cIMTLabels.push_back("IMT Posterior Average");
        cIMTLabels.push_back("IMT Posterior Max");
        cIMTLabels.push_back("IMT Posterior Min");
        cIMTLabels.push_back("IMT Posterior SD");
        cIMTLabels.push_back("IMT Posterior nMeas");
        string s_modality = "SR";
        string s_left = "left";
        cIMTMeasure m_empty;
        map< cIMTMeasureKey, cIMTMeasureVector > measure_map;
        for( int i = 0; i < dcmFiles->GetNumberOfValues(); ++i )
          {
          string filename = dcmFiles->GetValue(i);
          if( verbose )
            {
            cout << "file " << i
                      << " : "  << filename
                      << endl;
            }
          vector< vtksys::String > path =
            vtksys::SystemTools::SplitString( filename.c_str(), '/', true );
          string s_uid = path.at( path.size()-2 ).c_str();
          string lower_file = vtksys::SystemTools::GetFilenameName(filename.c_str());
          transform(lower_file.begin(), lower_file.end(), lower_file.begin(), ::tolower);
          string s_expected_lat = string::npos != lower_file.find(s_left) ? "left" : "right";

          ifstream fileStream(filename.c_str(),
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

          if(!readSuccess)
            {
            if( verbose )
              cout << s_uid << ": error failed to read file stream" << endl;
            continue;
            }

          DcmInputBufferStream bufferStream;
          offile_off_t length = fileSize;

          bufferStream.setBuffer((void*)memoryBuffer, length );
          bufferStream.setEos();

          DcmFileFormat fileFormat;
          OFCondition status = fileFormat.read( bufferStream, EXS_LittleEndianExplicit );

          if ( status.bad() )
            {
            if( verbose )
              cout << s_uid << ": error failed to read dicom file" << endl;
            continue;
            }

          DSRDocument srDocument;
          status = srDocument.read( *fileFormat.getDataset() );
          if ( status.bad() )
            {
            if( verbose )
              cout << s_uid << ": error failed to read sr document" << endl;
            continue;
            }

          // is this an SR modality?
          OFString v;
          srDocument.getModality(v);

          if( !v.empty() && s_modality == v.c_str() )
            {
            // get the IMT meausrement values from SR files
            if( verbose )
              {
              cout << "SR file found: " << filename << endl;
              }

            if(!srDocument.isValid())
              {
              if( verbose )
                cout << s_uid << ": error invalid sr document" << endl;
              continue;
              }
            DSRDocumentTree& srTree = srDocument.getTree();
            if(!srTree.isValid())
              {
              if( verbose )
                cout << s_uid << ": error invalid sr document tree state" << endl;
              continue;
              }
            size_t nodeId = 0;
            bool missing_codes = false;
            for(auto it = required_codes.begin(); it != required_codes.end(); it++)
              {
              if(0 == srTree.gotoNamedNode( *it ))
                {
                missing_codes = true;
                break;
                }
              }
            if(missing_codes)
              {
              if( verbose )
                cout << s_uid << ": error invalid sr document tree missing codes" << endl;
              continue;
              }

            string s_lat;
            string s_datetime;
            vector<cIMTMeasure> v_meas;
            cIMTMeasure m;

            srTree.gotoRoot();
            srDocument.getPatientID(v);
            m.SetLabel("PatientID");
            if( !v.empty() )
              {
              m.SetValue(vtkVariant(v.c_str()));
              }
            else
              {
              m.SetValue(vtkVariant("NA"));
              }
            v_meas.push_back(m);

            srDocument.getStudyDate(v);
            m.SetLabel("StudyDate");
            bool valid_key = true;
            if( !v.empty() )
              {
              m.SetValue(vtkVariant(v.c_str()));
              s_datetime = v.c_str();
              }
            else
              {
              m.SetValue(vtkVariant("NA"));
              valid_key = false;
              }
            v_meas.push_back(m);

            srDocument.getStudyTime(v);
            m.SetLabel("StudyTime");
            if( !v.empty() )
              {
              m.SetValue(vtkVariant(v.c_str()));
              s_datetime += v.c_str();
              }
            else
              {
              m.SetValue(vtkVariant("NA"));
              valid_key = false;
              }
            v_meas.push_back(m);

            m.SetLabel("Expected Laterality");
            m.SetValue(vtkVariant(s_expected_lat.c_str()));
            v_meas.push_back(m);

            bool done = false;
            bool valid = false;
            srTree.gotoRoot();
            srTree.gotoNamedNode( codedEV1 );
            map<string,int> side_map;
            do
              {
                DSRContentItem& item1 = srTree.getCurrentContentItem();
                if(item1.isValid())
                  {
                  s_lat = item1.getCodeValue().getCodeMeaning().c_str();
                  transform(s_lat.begin(), s_lat.end(), s_lat.begin(), ::tolower);
                  if(!s_lat.empty()) side_map[s_lat] = 1;
                  if(s_lat == s_expected_lat)
                    {
                    done = true;
                    valid = true;
                    if( verbose )
                      cout << s_uid << ": found expected side " << s_lat << " = " << s_expected_lat << endl;
                    }
                  else
                    {
                    if( verbose )
                      cout << s_uid << ": found unexpected side " << s_lat << " < > " << s_expected_lat << endl;
                    nodeId = srTree.gotoNextNamedNode( codedEV1 );
                    if(0 == nodeId)
                      {
                      if( verbose )
                        cout << s_uid << ": returning to first node" << endl;
                      done = true;
                      if(!valid)
                        {
                        srTree.gotoNamedNode( codedEV1 );
                        DSRContentItem& itemNext = srTree.getCurrentContentItem();
                        s_lat = itemNext.getCodeValue().getCodeMeaning().c_str();
                        transform(s_lat.begin(), s_lat.end(), s_lat.begin(), ::tolower);
                        }
                      }
                    }
                  }
              }
            while(!done);

            // make sure that we have at least one set of IMT values

            m.SetLabel("Laterality");
            if( !valid )
              {
              if( verbose )
                cout << s_uid << ": invalid laterality item " << s_lat << endl;
              }
            m.SetValue(vtkVariant(s_lat.c_str()));
            v_meas.push_back(m);

            map<string,vector<pair<double,string>>> dataSets;
            if( srTree.isValid() )
              {
              do
                {
                vector<string> values = extractCIMT( srTree );
                if(!values.empty())
                  {
                  string units = values.back();
                  values.pop_back();
                  string value = values.back();
                  values.pop_back();
                  string name = values.back();
                  values.pop_back();
                  // reject measurements that are not part of the expected set
                  if (std::find(cIMTLabels.begin(), cIMTLabels.end(), name) != cIMTLabels.end())
                    dataSets[name].push_back(pair<double,string>(atof(value.c_str()),units));
                  }
                }
              while (srTree.iterate());
              }
            int multiple_size = 0;
            vector<cIMTMeasure> v_meas_pending;
            map<int,int> item_lengths;
            for(auto it = dataSets.begin(); it != dataSets.end(); it++)
              {
              string name = it->first;
              vector<pair<double,string>> valueSet = it->second;
              item_lengths[valueSet.size()] = 1;
              if(valueSet.size() > multiple_size)
                {
                multiple_size = valueSet.size();
                }
              pair<double,string> data = valueSet.front();
              m = m_empty;
              m.SetLabel(name);
              m.SetValue(vtkVariant(data.first));
              m.SetUnits(data.second);
              v_meas_pending.push_back(m);
              }
            if(1 != item_lengths.size())
              {
              if( verbose )
                cout << s_uid << ": error inconsistency in number of data elements" << endl;
              continue;
              }

            if(1 < multiple_size)
              {
              if( verbose )
                cout << s_uid << ": processing for incorrect number of data " <<  multiple_size << endl;
              // for each variable, extract the first complete non-zero set such that
              // min < avg < max, SD > 0 and nMeas > 0
              vector<double> ave;
              vector<double> min;
              vector<double> max;
              vector<double> nmeas;
              vector<double> std;
              int idx = -1;
              cIMTMeasure m_ave;
              cIMTMeasure m_max;
              cIMTMeasure m_min;
              cIMTMeasure m_std;
              cIMTMeasure m_nmeas;
              for(auto it = dataSets.begin(); it != dataSets.end(); it++)
                {
                string name = it->first;
                vector<pair<double,string>> valueSet = it->second;
                for(auto vit = valueSet.begin(); vit!= valueSet.end(); vit++)
                  {
                  if(string::npos != name.find("Average"))
                    {
                    ave.push_back(vit->first);
                    m_ave.SetLabel(name);
                    m_ave.SetUnits(vit->second);
                    }
                  else if(string::npos != name.find("Min"))
                    {
                    min.push_back(vit->first);
                    m_min.SetLabel(name);
                    m_min.SetUnits(vit->second);
                    }
                  else if(string::npos != name.find("Max"))
                    {
                    max.push_back(vit->first);
                    m_max.SetLabel(name);
                    m_max.SetUnits(vit->second);
                    }
                  else if(string::npos != name.find("SD"))
                    {
                    std.push_back(vit->first);
                    m_std.SetLabel(name);
                    m_std.SetUnits(vit->second);
                    }
                  else if(string::npos != name.find("nMeas"))
                    {
                    nmeas.push_back(vit->first);
                    m_nmeas.SetLabel(name);
                    m_nmeas.SetUnits(vit->second);
                    }
                  }
                }
              int size = ave.size();
              if(size==min.size() &&
                 size==max.size() &&
                 size==std.size() &&
                 size==nmeas.size())
                {
                // select the set with the maximum nmeas
                double max_nmeas = 0.;
                for(int i=0; i<size; i++)
                  {
                  if(min[i] <= ave[i] && ave[i] <= max[i] && 0. < nmeas[i] && 0. < std[i])
                    {
                    if(nmeas[i]>max_nmeas)
                      {
                      max_nmeas = nmeas[i];
                      idx = i;
                      }
                    // for first available data strategy break
                    // break;
                    }
                  }
                }
              if(-1 != idx)
                {
                m_ave.SetValue(vtkVariant(ave[idx]));
                m_min.SetValue(vtkVariant(min[idx]));
                m_max.SetValue(vtkVariant(max[idx]));
                m_std.SetValue(vtkVariant(std[idx]));
                m_nmeas.SetValue(vtkVariant(nmeas[idx]));
                v_meas.push_back(m_ave);
                v_meas.push_back(m_max);
                v_meas.push_back(m_min);
                v_meas.push_back(m_std);
                v_meas.push_back(m_nmeas);
                if( verbose )
                  cout << "["<<min[idx] <<" < " << ave[idx] << " < " << max[idx] << "] +- " << std[idx] << " : " << nmeas[idx] << endl;
                }
              else
                {
                if( verbose )
                  cout << s_uid << ": no self-consistent set found in " << size << " sets" << endl;
                continue;
                }
              }
            else
              {
              for(auto it = v_meas_pending.begin(); it != v_meas_pending.end(); it++)
                {
                v_meas.push_back(*it);
                }
              }

            m = m_empty;
            m.SetLabel("NMultiple");
            m.SetValue(vtkVariant(multiple_size));
            v_meas.push_back(m);
            m = m_empty;
            m.SetLabel("NSides");
            m.SetValue(vtkVariant(side_map.size()));
            v_meas.push_back(m);
            m = m_empty;
            m.SetLabel("ExpectedFound");
            m.SetValue(vtkVariant((valid ? 1 : 0)));
            v_meas.push_back(m);

            bool save = true;
            if( s_lat.empty() )
              {
              if( verbose )
                cout << s_uid << ": error missing laterality" << endl;
              save = false;
              }
            if( v_meas.empty() )
              {
              if( verbose )
                cout << s_uid << ": error missing all values" << endl;
              save = false;
              }

            if( save )
              {
              cIMTMeasureKey key( s_uid, s_datetime );
              pair<cIMTMeasureKey, cIMTMeasureVector> p = make_pair( key, v_meas );
              measure_map.insert(p);
              }
            } // end if SR modality
          }  // end loop on dcm file names

        bool write_output = !output_file.empty();
        if( verbose || write_output )
          {
          ofstream ofs;
          string delim = "\",\"";
          if( write_output )
            ofs.open( output_file.c_str(), ofstream::out | ofstream::trunc );
          bool first = true;
          for( auto it=measure_map.cbegin(); it!=measure_map.cend(); ++it )
            {
            cIMTMeasureKey key = it->first;
            string s_uid = key.uid;
            string s_datetime = key.datetime;

            cIMTMeasureVector v_meas = it->second;
            stringstream h_str;
            h_str << "\"UID";
            stringstream r_str;
            r_str << "\"" << s_uid;
            if( verbose )
              {
              cout << s_uid << "(" << s_datetime << ")" << endl;
              }
            for( auto v_it=v_meas.cbegin(); v_it!=v_meas.cend(); ++v_it)
              {
              cIMTMeasure m = *v_it;
              if( verbose )
                {
                cout << m << endl;
                }
              if( write_output )
                {
                if( first )
                  {
                  h_str << delim
                        << m.GetLabel()
                        << delim
                        << "UNITS";
                  }
                string v_str =  m.GetValue().ToString();
                if( m.HasUnits() )
                  vtksys::SystemTools::ReplaceString( v_str, "\"", "" );
                r_str << delim
                      << v_str
                      << delim
                      << m.GetUnits();
                }
              } // end measure value loop

            if( first )
              {
              h_str << "\"";
              first = false;
              if( write_output )
                {
                ofs << h_str.rdbuf() << endl;
                }
              }
            if( write_output )
              {
              r_str << "\"";
              ofs << r_str.rdbuf() << endl;
              }
            } // end measure_map loop
          }
        }
      }
    }
  catch( exception &e )
    {
    cerr << "Uncaught exception: " << e.what() << endl;
    }

  return rval;
}
