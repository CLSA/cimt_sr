#include "vtkDICOMMetaData.h"
#include "vtkDICOMParser.h"
#include "vtkDICOMDictionary.h"
#include "vtkDICOMTagPath.h"
#include "vtkDICOMSequence.h"
#include "vtkDICOMFileSorter.h"
#include "vtkDICOMItem.h"
#include "vtkDICOMUtilities.h"

#include <vtkDirectory.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtksys/Glob.hxx>
#include <vtksys/SystemTools.hxx>

#include "options.h"

#include <sstream>
#include <string.h>
#include <stdlib.h>

// const dicom tag paths used for pretty printing
const vtkDICOMTagPath titlePath( DC::ConceptNameCodeSequence, 0, DC::CodeMeaning );
const vtkDICOMTagPath valuePath( DC::ConceptCodeSequence, 0, DC::CodeMeaning );
const vtkDICOMTagPath measuredValuePath( DC::MeasuredValueSequence, 0, DC::NumericValue );
const vtkDICOMTagPath unitsPath( DC::MeasuredValueSequence, 0,
                             DC::MeasurementUnitsCodeSequence, 0, DC::CodeMeaning );

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

const vtkDICOMTag emptyTag;

class cIMTMeasure
{
  public:
    cIMTMeasure( const vtkVariant& v, const std::string& s, const std::string& u ) :
      value( v ), label( s ), units( u ) {}

    cIMTMeasure() : value( vtkVariant() ) { this->label.clear(); this->units.clear(); }

    vtkVariant GetValue() const { return this->value; }
    void SetValue( const vtkVariant& v ) { this->value = v; }
    std::string GetLabel() const { return this->label; }
    void SetLabel( const std::string& l ) { this->label = l; }
    std::string GetUnits() const { return this->units; }
    void SetUnits( const std::string& u ) { this->units = u; }

    bool LoadFromSequence( const vtkDICOMSequence& seq,
      const vtkDICOMTagPath& titlePath,
      const vtkDICOMTagPath& valuePath,
      const vtkDICOMTagPath& unitsPath = vtkDICOMTagPath()) {

      if( 0 == seq.GetNumberOfItems() )
        return false;

      vtkDICOMValue v = seq.GetAttributeValue( 0, titlePath );
      this->label = v.AsString();
      v = seq.GetAttributeValue( 0, valuePath );

      if( emptyTag == unitsPath.GetHead() ) // empty, no units
        {
        this->value = vtkVariant( v.AsString() );
        this->units.clear();
        }
      else
        {
        this->value = vtkVariant( v.AsDouble() );
        v = seq.GetAttributeValue( 0, unitsPath );
        this->units = v.AsString();
        }
      return true;
    }

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
    std::string label;
    std::string units;
};

ostream& operator<<(ostream& o, const cIMTMeasure& m)
{
  std::string s = m.GetLabel();
  s += std::string( " : " );
  s += m.GetValue().ToString();
  if( m.HasUnits() )
    {
    s += std::string( " (" );
    s += m.GetUnits();
    s += std::string(  ")" );
    }
  o << s;
  return o;
}

typedef std::vector<cIMTMeasure> cIMTMeasureVector;

class cIMTMeasureKey
{
  public:
    cIMTMeasureKey( const std::string& uid, const std::string& side )
      {
      this->uid=uid;
      this->side=side;
      }
    std::string uid;
    std::string side;
    bool operator<(const cIMTMeasureKey& k) const
      {
      int s_cmp = this->uid.compare(k.uid);
      if(0 == s_cmp)
        {
          return this->side < k.side;
        }
      return s_cmp < 0;
      }
};


void RecurseDirectorySearch( vtkStringArray* files, const std::string& folder )
{
  vtkSmartPointer<vtkDirectory> dir =
    vtkSmartPointer<vtkDirectory>::New();

  std::string s_dcm = ".dcm";
  std::string s_cwd = folder;
  std::string s_dot = ".";
  std::string s_dotdot = "..";

  unsigned last = s_cwd.find_last_of("/");
  if( last != (s_cwd.length()-1) )
    s_cwd += "/";

  dir->Open( s_cwd.c_str() );
  if( 0 == dir->GetNumberOfFiles() ) return;

  for( int i = 0; i < dir->GetNumberOfFiles(); ++i )
    {
    std::string s = dir->GetFile(i);
    if( s == s_dot || s == s_dotdot )
      continue;
    s = s_cwd + s;
    if( dir->FileIsDirectory( s.c_str() ) )
      {
      RecurseDirectorySearch( files, s );
      }
    else
      {
      std::string ext = vtksys::SystemTools::GetFilenameLastExtension(s);
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
        std::string foldername = opts.get_input( "input" );
        if( !vtksys::SystemTools::FileIsDirectory( foldername.c_str() ) )
          {
          std::stringstream stream;
          stream << "The input folder \"" << foldername <<"\" does not exist";
          throw std::runtime_error( stream.str() );
          }
        std::string output_file = opts.get_option( "output" );
        bool verbose = opts.get_flag( "verbose" );

        vtkSmartPointer<vtkStringArray> dcmFiles =
          vtkSmartPointer<vtkStringArray>::New();

        std::string s_cwd = foldername;
        RecurseDirectorySearch( dcmFiles, s_cwd );

        if( 0 == dcmFiles->GetNumberOfValues() )
          {
          vtksys::Glob glob;
          glob.RecurseOn();
          glob.SetRelative( s_cwd.c_str() );
          if( glob.FindFiles( "*.dcm" ) )
            {
            std::vector<std::string> files = glob.GetFiles();
            if( !files.empty() )
              {
              for(auto it = files.begin(); it != files.end(); ++it )
                {
                std::cout << *it << std::endl;
                }
              }
            }
          }
        if( 0 == dcmFiles->GetNumberOfValues() )
          {
          std::stringstream stream;
          stream << "No dicom files found in " << s_cwd;
          throw std::runtime_error( stream.str() );
          }

        vtkSmartPointer<vtkDICOMFileSorter> sorter =
          vtkSmartPointer<vtkDICOMFileSorter>::New();
        sorter->SetInputFileNames( dcmFiles );
        sorter->RequirePixelDataOff();
        sorter->Update();

        if( verbose )
          {
          std::cout << "Number of studies: " << sorter->GetNumberOfStudies() << std::endl;
          std::cout << "Number of series: " << sorter->GetNumberOfSeries() << std::endl;
          }

        vtkSmartPointer<vtkDICOMParser> parser =
          vtkSmartPointer<vtkDICOMParser>::New();
        vtkSmartPointer<vtkDICOMMetaData> meta =
          vtkSmartPointer<vtkDICOMMetaData>::New();
        parser->SetMetaData( meta );

        vtkDICOMSequence contentSeq = meta->GetAttributeValue(DC::ContentSequence);
        vtkDICOMSequence foundSeq;
        vtkDICOMSequence desiredSeq;

        vtkstd::string s_modality = "SR";
        std::map< cIMTMeasureKey, cIMTMeasureVector > measure_map;
        int max_values = 6;
        for( int i = 0; i < sorter->GetNumberOfStudies(); ++i )
          {
          for( int j = sorter->GetFirstSeriesForStudy(i); j <= sorter->GetLastSeriesForStudy(i); ++j )
            {
            // find the parent cineloop of a given still image
            vtkStringArray* s_files = sorter->GetFileNamesForSeries(j);
            for( int k = 0; k < s_files->GetNumberOfValues(); ++k )
              {
              std::string filename = s_files->GetValue(k);
              if( verbose )
                {
                std::cout << "study " << i
                          << ", series " << j
                          << ", file " << k
                          << " : "  << filename
                          << std::endl;
                }

              parser->SetFileName( filename.c_str() );
              parser->Update();

              // is this an SR modality?
              vtkDICOMValue v = meta->GetAttributeValue( DC::Modality );
              if( verbose && v.IsValid() )
                std::cout << "Modality: " << v.AsString() << std::endl;

              if( v.IsValid() && s_modality == v.AsString() )
                {
                // get the IMT meausrement values from SR files
                if( verbose )
                  {
                  std::cout << "SR file found: " << filename << std::endl;
                  }

                std::vector< vtksys::String > path =
                  vtksys::SystemTools::SplitString( filename.c_str(), '/', true );
                std::string s_uid = path.at( path.size()-2 ).c_str();
                std::string s_lat;
                std::vector<cIMTMeasure> v_meas;
                cIMTMeasure m;

                v = meta->GetAttributeValue(DC::PatientID);
                m.SetLabel("PatientID");
                if( v.IsValid() )
                  {
                  m.SetValue(vtkVariant(v.AsString()));
                  }
                v_meas.push_back(m);

                v = meta->GetAttributeValue(DC::StudyDate);
                m.SetLabel("StudyDate");
                if( v.IsValid() )
                  {
                  m.SetValue(vtkVariant(v.AsString()));
                  }
                v_meas.push_back(m);

                v = meta->GetAttributeValue(DC::StudyTime);
                m.SetLabel("StudyTime");
                if( v.IsValid() )
                  {
                  m.SetValue(vtkVariant(v.AsString()));
                  }
                v_meas.push_back(m);

                vtkDICOMSequence contentSeq = meta->GetAttributeValue(DC::ContentSequence);

                foundSeq.Clear();
                FillSequence( &desiredSeq, "G-C171", "SRT", "Laterality" );
                RecurseSequence( contentSeq, desiredSeq, &foundSeq );
                int n_missing = 0;
                if( m.LoadFromSequence( foundSeq, titlePath, valuePath ) )
                  {
                  v_meas.push_back(m);
                  s_lat = m.GetValue().ToString();
                  }
                else
                  n_missing++;

                FillSequence( &desiredSeq, "GEU-1005-26", "99GEMS", "IMT Posterior Average" );
                foundSeq.Clear();
                RecurseSequence( contentSeq, desiredSeq, &foundSeq );
                if( m.LoadFromSequence( foundSeq, titlePath, measuredValuePath, unitsPath ) )
                  v_meas.push_back(m);
                else
                  n_missing++;

                FillSequence( &desiredSeq, "GEU-1005-27", "99GEMS", "IMT Posterior Max" );
                foundSeq.Clear();
                RecurseSequence( contentSeq, desiredSeq, &foundSeq );
                if( m.LoadFromSequence( foundSeq, titlePath, measuredValuePath, unitsPath ) )
                  v_meas.push_back(m);
                else
                  n_missing++;

                FillSequence( &desiredSeq, "GEU-1005-28", "99GEMS", "IMT Posterior Min" );
                foundSeq.Clear();
                RecurseSequence( contentSeq, desiredSeq, &foundSeq );
                if( m.LoadFromSequence( foundSeq, titlePath, measuredValuePath, unitsPath ) )
                  v_meas.push_back(m);
                else
                  n_missing++;

                FillSequence( &desiredSeq, "GEU-1005-29", "99GEMS", "IMT Posterior SD" );
                foundSeq.Clear();
                RecurseSequence( contentSeq, desiredSeq, &foundSeq );
                if( m.LoadFromSequence( foundSeq, titlePath, measuredValuePath, unitsPath ) )
                  v_meas.push_back(m);
                else
                  n_missing++;

                FillSequence( &desiredSeq, "GEU-1005-30", "99GEMS", "IMT Posterior nMeas" );
                foundSeq.Clear();
                RecurseSequence( contentSeq, desiredSeq, &foundSeq );
                if( m.LoadFromSequence( foundSeq, titlePath, measuredValuePath, unitsPath ) )
                  v_meas.push_back(m);
                else
                  n_missing++;

                bool save = true;
                if( s_lat.empty() )
                  {
                  if( verbose )
                    std::cout << s_uid << " missing laterality" << std::endl;
                  save = false;
                  }
               if( v_meas.empty() )
                  {
                  if( verbose )
                    std::cout << s_uid << " missing all values" << std::endl;
                  save = false;
                  }
                if( 0 < n_missing && !v_meas.empty() )
                  {
                  if( verbose )
                    std::cout << s_uid << " missing "
                              << n_missing << " of  " << max_values << " values" << std::endl;
                  }

                if( save )
                  {
                  cIMTMeasureKey key( s_uid, s_lat );
                  std::pair<cIMTMeasureKey, cIMTMeasureVector> p = std::make_pair( key, v_meas );
                  measure_map.insert(p);
                  }
                }
              }
            }
          }
        bool write_output = !output_file.empty();
        if( verbose || write_output )
          {
          std::ofstream ofs;
          std::string delim = "\",\"";
          if( write_output )
            ofs.open( output_file.c_str(), std::ofstream::out | std::ofstream::trunc );
          bool first = true;
          for( auto it=measure_map.cbegin(); it!=measure_map.cend(); ++it )
            {
            cIMTMeasureKey key = it->first;
            std::string s_uid = key.uid;
            std::string s_lat = key.side;

            cIMTMeasureVector v_meas = it->second;
            std::stringstream h_str;
            h_str << "\"UID";
            std::stringstream r_str;
            r_str << "\"" << s_uid;
            if( verbose )
              {
              std::cout << s_uid << "(" << s_lat << ")" << std::endl;
              }
            for( auto v_it=v_meas.cbegin(); v_it!=v_meas.cend(); ++v_it)
              {
              cIMTMeasure m = *v_it;
              if( verbose )
                {
                std::cout << m << std::endl;
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
                std::string v_str =  m.GetValue().ToString();
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
                ofs << h_str.rdbuf() << std::endl;
                }
              }
            if( write_output )
              {
              r_str << "\"";
              ofs << r_str.rdbuf() << std::endl;
              }
            } // end measure_map loop
          }
        }
      }
    }
  catch( std::exception &e )
    {
    std::cerr << "Uncaught exception: " << e.what() << std::endl;
    }

  return rval;
}
