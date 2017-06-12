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
  if( node == NULL ) return values;
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


        string s_modality = "SR";
        map< cIMTMeasureKey, cIMTMeasureVector > measure_map;
        int max_values = 6;
        for( int i = 0; i < dcmFiles->GetNumberOfValues(); ++i )
          {
          string filename = dcmFiles->GetValue(i);
          if( verbose )
            {
            cout << "file " << i
                      << " : "  << filename
                      << endl;
            }

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

          if(!readSuccess) continue;
          
          DcmInputBufferStream bufferStream;
          offile_off_t length = fileSize;

          bufferStream.setBuffer((void*)memoryBuffer, length );
          bufferStream.setEos();
          
          DcmFileFormat fileFormat;
          OFCondition status = fileFormat.read( bufferStream, EXS_LittleEndianExplicit );

          if ( status.bad() ) continue;

          DSRDocument srDocument;
          status = srDocument.read( *fileFormat.getDataset() );
          if ( status.bad() ) continue;


          // is this an SR modality?
          OFString v;
          srDocument.getModality(v);

          if( verbose && !v.empty() )
            cout << "Modality: " << v.c_str() << endl;

          if( !v.empty() && s_modality == v.c_str() )
            {
            // get the IMT meausrement values from SR files
            if( verbose )
              {
              cout << "SR file found: " << filename << endl;
              }

            vector< vtksys::String > path =
              vtksys::SystemTools::SplitString( filename.c_str(), '/', true );
            string s_uid = path.at( path.size()-2 ).c_str();
            string s_lat;
            string s_left = "left";
            string lower_file = vtksys::SystemTools::GetFilenameName(filename.c_str());
            transform(lower_file.begin(), lower_file.end(), lower_file.begin(), ::tolower);
            string s_expected_lat = string::npos != lower_file.find(s_left) ? "left" : "right";
            string s_datetime;
            vector<cIMTMeasure> v_meas;
            cIMTMeasure m;
            cIMTMeasure m_empty;

            srDocument.getPatientID(v); 
            m.SetLabel("PatientID");
            if( !v.empty() )
              {
              m.SetValue(vtkVariant(v.c_str()));
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
              valid_key = false;
              }
            v_meas.push_back(m);

            DSRDocumentTree& tree = srDocument.getTree();
            DSRCodedEntryValue codedEV( "G-C171","SRT","Laterality");
            tree.gotoNamedNode( codedEV );
            DSRContentItem& item1 = tree.getCurrentContentItem();
            m.SetLabel("Laterality");
            if( !item1.isValid() )
              {
              cout << "Error: invalid item" << endl;
              }
            else
              {
              s_lat = item1.getCodeValue().getCodeMeaning().c_str();
              transform(s_lat.begin(), s_lat.end(), s_lat.begin(), ::tolower);
              m.SetValue(vtkVariant(s_lat.c_str()));
              }
            v_meas.push_back(m);
            
            m.SetLabel("Expected Laterality");
            m.SetValue(vtkVariant(s_expected_lat.c_str()));
            v_meas.push_back(m);

            // now get the measurements
            DSRDocumentTree& srTree = srDocument.getTree();
            srTree.gotoRoot();

            map<string,vector<pair<double,string>>> dataSets;
            string s_zero = "0";
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
                  if(s_zero != value)
                    {
                    dataSets[name].push_back(pair<double,string>(atof(value.c_str()),units));
                    }
                  }
                }
              while (srTree.iterate());
              }
            bool has_multiple = false;
            int multiple_size = 1;
            cout << "number of cimt data values: " << dataSets.size() << endl;
            for(auto it = dataSets.begin(); it != dataSets.end(); it++)
              {
              string name = it->first;
              vector<pair<double,string>> valueSet = it->second;
              if(!has_multiple && 1<valueSet.size())
                {
                has_multiple = true;
                multiple_size = valueSet.size();
                }
              pair<double,string> data = valueSet.front();
              m = m_empty;
              m.SetLabel(name);
              m.SetValue(vtkVariant(data.first));
              m.SetUnits(data.second);            
              v_meas.push_back(m);
              }
            m = m_empty;
            m.SetLabel("Multiple");
            m.SetValue(vtkVariant(multiple_size));
            v_meas.push_back(m);

            bool save = true;
            if( s_lat.empty() )
              {
              if( verbose )
                cout << s_uid << " missing laterality" << endl;
              save = false;
              }
            if( v_meas.empty() )
              {
              if( verbose )
                cout << s_uid << " missing all values" << endl;
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
